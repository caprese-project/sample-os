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

uint32_t device_tree_property::to_u32() const {
  return std::byteswap(*reinterpret_cast<const uint32_t*>(value.data()));
}

uint64_t device_tree_property::to_u64() const {
  return std::byteswap(*reinterpret_cast<const uint64_t*>(value.data()));
}

std::string device_tree_property::to_str() const {
  return std::string(value.data(), value.size());
}

std::vector<char> device_tree_property::to_array() const {
  return value;
}

std::vector<std::string> device_tree_property::to_str_list() const {
  std::vector<std::string> list;
  std::istringstream       stream(to_str());
  std::string              str;
  while (std::getline(stream, str, '\0')) {
    list.push_back(str);
  }
  return list;
}

std::vector<std::pair<uintptr_t, size_t>> device_tree_property::to_reg(uint32_t addr_cells, uint32_t size_cells) const {
  std::vector<std::pair<uintptr_t, size_t>> regions;
  const std::vector<char>&                  data   = this->value;
  const uint32_t*                           ptr    = reinterpret_cast<const uint32_t*>(data.data());
  size_t                                    length = data.size() / sizeof(uint32_t);

  for (size_t i = 0; i < length; i += (addr_cells + size_cells)) {
    uintptr_t addr = 0;
    for (size_t j = 0; j < addr_cells; ++j) {
      addr = (addr << 32) | std::byteswap(ptr[i + j]);
    }

    size_t size = 0;
    for (size_t j = 0; j < size_cells; ++j) {
      size = (size << 32) | std::byteswap(ptr[i + addr_cells + j]);
    }

    regions.emplace_back(addr, size);
  }

  return regions;
}

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

  root = parse_node(stream, off_dt_struct, off_dt_strings, "", FDT_DEFAULT_ADDRESS_CELLS, FDT_DEFAULT_SIZE_CELLS);
}

device_tree_node device_tree::parse_node(std::istream& stream, uint32_t off_dt_struct, uint32_t off_dt_strings, const std::string& dir, size_t address_cells, size_t size_cells) {
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

  node.address_cells = address_cells;
  node.size_cells    = size_cells;

  align(stream);

  while (!stream.eof()) {
    tag = read_u32(stream);

    if (tag == FDT_PROP) {
      device_tree_property property;

      uint32_t len     = read_u32(stream);
      uint32_t nameoff = read_u32(stream);

      property.name = read_str(stream, off_dt_strings + nameoff);

      std::streampos pos = stream.tellg();

      property.value.resize(len);
      stream.read(property.value.data(), len);

      stream.seekg(pos + static_cast<std::streamoff>(len));

      align(stream);

      node.properties.emplace(property.name, std::move(property));
    } else if (tag == FDT_BEGIN_NODE) {
      back_u32(stream);

      uint32_t child_address_cells = FDT_DEFAULT_ADDRESS_CELLS;
      uint32_t child_size_cells    = FDT_DEFAULT_SIZE_CELLS;
      if (node.properties.contains("#address-cells")) {
        child_address_cells = node.properties.at("#address-cells").to_u32();
      }
      if (node.properties.contains("#size-cells")) {
        child_size_cells = node.properties.at("#size-cells").to_u32();
      }

      device_tree_node child = parse_node(stream, off_dt_struct, off_dt_strings, node.full_name == "/" ? "" : node.full_name, child_address_cells, child_size_cells);
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

bool device_tree::has_node(std::string_view full_path) const {
  if (!full_path.starts_with('/')) [[unlikely]] {
    return false;
  }

  const device_tree_node* node = &root;
  std::string_view        path = full_path;
  do {
    size_t slash_pos = path.find('/', 1);
    if (slash_pos == std::string::npos) {
      slash_pos = path.size();
    }

    std::string_view name = path.substr(1, slash_pos - 1);

    auto iter = node->children.find(name);
    if (iter == node->children.end()) {
      return false;
    }

    node = &iter->second;

    path = path.substr(slash_pos);
  } while (!path.empty());

  return true;
}

const device_tree_node& device_tree::get_node(std::string_view full_path) const {
  if (!has_node(full_path)) [[unlikely]] {
    abort();
  }

  const device_tree_node* node = &root;
  std::string_view        path = full_path;
  do {
    size_t slash_pos = path.find('/', 1);
    if (slash_pos == std::string::npos) {
      slash_pos = path.size();
    }

    std::string_view name = path.substr(1, slash_pos - 1);

    auto iter = node->children.find(name);
    if (iter == node->children.end()) {
      abort();
    }

    node = &iter->second;

    path = path.substr(slash_pos);
  } while (!path.empty());

  return *node;
}
