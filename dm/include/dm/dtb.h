#ifndef DM_DTB_H_
#define DM_DTB_H_

#include <functional>
#include <istream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct device_tree_property {
  std::string                                                                                                                           name;
  std::variant<uint32_t, uint64_t, std::string, std::vector<char>, std::vector<std::string>, std::vector<std::pair<uintptr_t, size_t>>> value;
};

struct device_tree_node {
  std::string                                              name;
  uintptr_t                                                address;
  std::string                                              full_name;
  std::map<std::string, device_tree_property, std::less<>> properties;
  std::map<std::string, device_tree_node, std::less<>>     children;
};

class device_tree {
  static const std::set<std::string> u32_types;
  static const std::set<std::string> u64_types;
  static const std::set<std::string> str_types;
  static const std::set<std::string> array_types;
  static const std::set<std::string> phandle_types;
  static const std::set<std::string> str_list_types;

  device_tree_node root;

private:
  device_tree_node parse_node(std::istream& stream, uint32_t off_dt_struct, uint32_t off_dt_strings, const std::string& dir, size_t address_cells, size_t size_cells);
  void             back_u32(std::istream& stream);
  uint32_t         read_u32(std::istream& stream);
  uint64_t         read_u64(std::istream& stream);
  std::string      read_str(std::istream& stream);
  std::string      read_str(std::istream& stream, uint32_t offset);
  void             align(std::istream& stream);

public:
  void load(const char* begin, const char* end);

  [[nodiscard]] bool                    has_node(std::string_view full_path) const;
  [[nodiscard]] const device_tree_node& get_node(std::string_view full_path) const;
};

#endif // DM_DTB_H_
