#ifndef RAMFS_FILE_H_
#define RAMFS_FILE_H_

#include <string>
#include <string_view>
#include <vector>

class file {
  std::string       abs_path;
  std::vector<char> data;

public:
  file(std::string_view abs_path, std::vector<char>&& data);

  file(const file&)            = delete;
  file& operator=(const file&) = delete;
  file(file&&)                 = default;
  file& operator=(file&&)      = default;

  [[nodiscard]] const std::string& get_abs_path() const;
  [[nodiscard]] std::string_view   get_name() const;
  [[nodiscard]] std::streamsize    size() const;

  [[nodiscard]] std::streamsize read(std::streampos pos, char* buffer, std::streamsize size);
  [[nodiscard]] std::streamsize write(std::streampos pos, std::string_view data);
};

#endif // RAMFS_FILE_H_
