#ifndef CONS_FILE_H_
#define CONS_FILE_H_

#include <cons/console.h>
#include <libcaprese/cap.h>
#include <optional>
#include <string>
#include <string_view>

enum struct file_type {
  uart = 0,
};

class file {
  std::string                  abs_path;
  std::optional<class console> console;

public:
  file(std::string_view abs_path, file_type type, endpoint_cap_t ep_cap);

  file(const file&)            = delete;
  file& operator=(const file&) = delete;
  file(file&&)                 = default;
  file& operator=(file&&)      = default;

  [[nodiscard]] const std::string& get_abs_path() const;

  [[nodiscard]] std::streamsize read(char* buffer, std::streamsize size);
  [[nodiscard]] std::streamsize write(std::string_view data);
};

#endif // CONS_FILE_H_
