#ifndef CONS_DIRECTORY_H_
#define CONS_DIRECTORY_H_

#include <cons/file.h>
#include <fs/ipc.h>
#include <functional>
#include <map>
#include <string>
#include <string_view>

enum struct directory_type {
  regular_directory = FS_FT_DIR,
  device_file       = FS_FT_REG,
};

class directory {
  std::string                                   abs_path;
  std::map<std::string, directory, std::less<>> subdirs;
  std::optional<class file>                     file;
  directory_type                                type;

private:
  directory(std::string_view abs_path, file_type type, endpoint_cap_t ep_cap);

public:
  directory(std::string_view abs_path);

  directory(const directory&)            = delete;
  directory& operator=(const directory&) = delete;
  directory(directory&&)                 = default;
  directory& operator=(directory&&)      = default;

  [[nodiscard]] const std::string& get_abs_path() const;
  [[nodiscard]] directory_type     get_type() const;
  [[nodiscard]] bool               contains(std::string_view name) const;
  [[nodiscard]] directory&         get_dir(std::string_view name);
  [[nodiscard]] const directory&   get_dir(std::string_view name) const;
  [[nodiscard]] class file&        get_file();
  [[nodiscard]] const class file&  get_file() const;
  [[nodiscard]] std::string_view   get_name() const;

  [[nodiscard]] bool create_directory(std::string_view name);
  [[nodiscard]] bool create_file(std::string_view name, file_type type, endpoint_cap_t ep_cap);
};

#endif // CONS_DIRECTORY_H_
