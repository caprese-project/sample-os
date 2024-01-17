#ifndef RAMFS_DIRECTORY_H_
#define RAMFS_DIRECTORY_H_

#include <fs/ipc.h>
#include <functional>
#include <map>
#include <optional>
#include <ramfs/file.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class directory {
  std::string                                   abs_path;
  std::map<std::string, directory, std::less<>> dirs;
  std::map<std::string, file, std::less<>>      files;

public:
  directory(std::string_view abs_path);

  directory(const directory&)            = delete;
  directory& operator=(const directory&) = delete;
  directory(directory&&)                 = default;
  directory& operator=(directory&&)      = default;

  [[nodiscard]] const std::string& get_abs_path() const;
  [[nodiscard]] std::string_view   get_name() const;

  [[nodiscard]] bool             has_dir(std::string_view name) const;
  [[nodiscard]] bool             has_file(std::string_view name) const;
  [[nodiscard]] directory&       get_dir(std::string_view name);
  [[nodiscard]] file&            get_file(std::string_view name);
  [[nodiscard]] const directory& get_dir(std::string_view name) const;
  [[nodiscard]] const file&      get_file(std::string_view name) const;

  [[nodiscard]] std::streamsize read(std::streampos pos, fs_file_info* buffer);

  [[nodiscard]] std::optional<std::reference_wrapper<directory>> find_directory(std::string_view path);
  [[nodiscard]] std::optional<std::reference_wrapper<file>>      find_file(std::string_view path);
  [[nodiscard]] std::optional<std::reference_wrapper<directory>> create_directories(std::string_view path);
  [[nodiscard]] std::optional<std::reference_wrapper<file>>      create_file(std::string_view path, std::vector<char>&& data = {});
  [[nodiscard]] bool                                             remove(std::string_view path);
};

#endif // RAMFS_DIRECTORY_H_
