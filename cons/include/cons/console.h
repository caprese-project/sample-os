#ifndef CONS_CONSOLE_H_
#define CONS_CONSOLE_H_

#include <cstddef>
#include <libcaprese/cap.h>
#include <string>
#include <string_view>

struct console_ops {
  void (*putc)(endpoint_cap_t ep_cap, char ch);
  char (*getc)(endpoint_cap_t ep_cap);
};

enum struct console_mode {
  raw,
  cooked,
};

class console {
  console_ops    ops;
  console_mode   mode;
  std::string    buffer;
  size_t         cursor_pos;
  endpoint_cap_t ep_cap;
  bool           esc;
  std::string    esc_buffer;

public:
  console(endpoint_cap_t ep_cap, console_ops ops, console_mode mode = console_mode::cooked);

  ssize_t read(char* dst, size_t max_size);
  ssize_t write(std::string_view str);

private:
  char    getc();
  void    putc(char ch);
  void    puts(std::string_view str);
  void    move_cursor(std::streamoff offset);
  ssize_t raw_read(char* dst, size_t max_size);
  ssize_t cooked_read(char* dst, size_t max_size);
};

#endif // CONS_CONSOLE_H_
