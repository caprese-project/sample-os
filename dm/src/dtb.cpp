#include <algorithm>
#include <bit>
#include <dm/dtb.h>
#include <dm/fdt.h>
#include <iterator>
#include <sstream>

const std::set<std::string> device_tree::u32_types(std::begin(FDT_U32_TYPES), std::end(FDT_U32_TYPES));
const std::set<std::string> device_tree::u64_types(std::begin(FDT_U64_TYPES), std::end(FDT_U64_TYPES));
const std::set<std::string> device_tree::str_types(std::begin(FDT_STR_TYPES), std::end(FDT_STR_TYPES));
const std::set<std::string> device_tree::array_types(std::begin(FDT_ARRAY_TYPES), std::end(FDT_ARRAY_TYPES));
const std::set<std::string> device_tree::phandle_types(std::begin(FDT_PHANDLE_TYPES), std::end(FDT_PHANDLE_TYPES));
const std::set<std::string> device_tree::str_list_types(std::begin(FDT_STR_LIST_TYPES), std::end(FDT_STR_LIST_TYPES));

void device_tree::load(const char* begin, const char* end) {
  std::istringstream stream(std::string(begin, end - begin));

  fdt_header_t header;
  stream.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (header.magic != std::byteswap(FDT_HEADER_MAGIC)) [[unlikely]] {
    abort();
  }

  uint32_t totalsize      = std::byteswap(header.totalsize);
  uint32_t off_dt_struct  = std::byteswap(header.off_dt_struct);
  uint32_t off_dt_strings = std::byteswap(header.off_dt_strings);

  if (totalsize > end - begin) [[unlikely]] {
    abort();
  }

  stream.seekg(off_dt_struct);

  root = parse_node(stream, off_dt_struct, off_dt_strings, "");
}

device_tree_node device_tree::parse_node(std::istream& stream, uint32_t off_dt_struct, uint32_t off_dt_strings, const std::string& dir) {
  device_tree_node node;

  uint32_t tag = read_u32(stream);
  while (tag == FDT_NOP) {
    tag = read_u32(stream);
  }

  if (tag != FDT_BEGIN_NODE) [[unlikely]] {
    abort();
  }

  node.name     = read_str(stream);
  size_t at_pos = node.name.find('@');
  if (at_pos != std::string::npos) {
    node.address = std::stoull(node.name.substr(at_pos + 1), nullptr, 16);
    node.name    = node.name.substr(0, at_pos);
  } else {
    node.address = 0;
  }
  node.full_name = dir + "/" + node.name;

  align(stream);

  while (!stream.eof()) {
    tag = read_u32(stream);

    if (tag == FDT_PROP) {
      device_tree_property property;

      uint32_t len     = read_u32(stream);
      uint32_t nameoff = read_u32(stream);

      property.name = read_str(stream, off_dt_strings + nameoff);

      std::streampos pos = stream.tellg();

      if (u32_types.contains(property.name)) {
        property.value = read_u32(stream);
      } else if (u64_types.contains(property.name)) {
        property.value = read_u64(stream);
      } else if (str_types.contains(property.name)) {
        property.value = read_str(stream);
      } else if (array_types.contains(property.name)) {
        std::vector<char> data;
        data.resize(len);
        stream.read(data.data(), len);

        property.value = std::move(data);
      } else if (phandle_types.contains(property.name)) {
        property.value = read_u32(stream);
      } else if (str_list_types.contains(property.name)) {
        std::vector<std::string> strings;
        std::streampos           beg = stream.tellg();

        while (stream.tellg() - beg < len) {
          strings.emplace_back(read_str(stream));
        }

        property.value = std::move(strings);
      } else {
        std::vector<char> data;
        data.resize(len);
        stream.read(data.data(), len);

        property.value = std::move(data);
      }

      stream.seekg(pos + static_cast<std::streamoff>(len));

      align(stream);

      node.properties.emplace(property.name, std::move(property));
    } else if (tag == FDT_BEGIN_NODE) {
      back_u32(stream);
      device_tree_node child = parse_node(stream, off_dt_struct, off_dt_strings, node.full_name == "/" ? "" : node.full_name);
      node.children.emplace(child.name, std::move(child));
    } else if (tag == FDT_END_NODE) {
      break;
    } else if (tag == FDT_NOP) {
      continue;
    } else {
      abort();
    }
  }

  align(stream);

  return node;
}

void device_tree::back_u32(std::istream& stream) {
  stream.seekg(-static_cast<std::streamoff>(sizeof(uint32_t)), std::ios_base::cur);
}

uint32_t device_tree::read_u32(std::istream& stream) {
  uint32_t u32;
  stream.read(reinterpret_cast<char*>(&u32), sizeof(u32));
  return std::byteswap(u32);
}

uint64_t device_tree::read_u64(std::istream& stream) {
  uint64_t u64;
  stream.read(reinterpret_cast<char*>(&u64), sizeof(u64));
  return std::byteswap(u64);
}

std::string device_tree::read_str(std::istream& stream) {
  std::string str;
  char        ch;
  while (stream.get(ch) && ch != '\0') {
    str.push_back(ch);
  }
  return str;
}

std::string device_tree::read_str(std::istream& stream, uint32_t offset) {
  std::streampos pos = stream.tellg();
  stream.seekg(offset);
  std::string str = read_str(stream);
  stream.seekg(pos);
  return str;
}

void device_tree::align(std::istream& stream) {
  while (stream.tellg() % 4 != 0) {
    stream.get();
  }
}

device_tree::device_tree(const char* begin, const char* end) {
  load(begin, end);
}

bool device_tree::has_node(const std::string& full_path) const {
  if (!full_path.starts_with('/')) [[unlikely]] {
    return false;
  }

  const device_tree_node* node = &root;
  std::string             path = full_path;
  do {
    size_t slash_pos = path.find('/', 1);
    if (slash_pos == std::string::npos) {
      slash_pos = path.size();
    }

    std::string name = path.substr(1, slash_pos - 1);

    auto iter = node->children.find(name);
    if (iter == node->children.end()) {
      return false;
    }

    node = &iter->second;

    path = path.substr(slash_pos);
  } while (!path.empty());

  return true;
}

const device_tree_node& device_tree::get_node(const std::string& full_path) const {
  if (!has_node(full_path)) [[unlikely]] {
    abort();
  }

  const device_tree_node* node = &root;
  std::string             path = full_path;
  do {
    size_t slash_pos = path.find('/', 1);
    if (slash_pos == std::string::npos) {
      slash_pos = path.size();
    }

    std::string name = path.substr(1, slash_pos - 1);

    auto iter = node->children.find(name);
    if (iter == node->children.end()) {
      abort();
    }

    node = &iter->second;

    path = path.substr(slash_pos);
  } while (!path.empty());

  return *node;
}
