#ifndef CONS_CONSOLE_H_
#define CONS_CONSOLE_H_

#include <cstddef>
#include <libcaprese/cap.h>
#include <string>

struct console_ops {
  void (*putc)(endpoint_cap_t ep_cap, char ch);
  char (*getc)(endpoint_cap_t ep_cap);
};

enum struct console_mode {
  raw,
  cooked,
};

class console {
  console_ops  ops;
  console_mode mode;
  std::string  buffer;
  size_t       cursor_pos;
  bool         esc;
  std::string  esc_buffer;

public:
  console(console_ops ops, console_mode mode = console_mode::cooked);

  ssize_t read(endpoint_cap_t ep_cap, char* dst, size_t max_size);
  ssize_t write(endpoint_cap_t ep_cap, const char* str, size_t size);

private:
  char    getc(endpoint_cap_t ep_cap);
  void    puts(endpoint_cap_t ep_cap, const char* str, size_t size);
  void    puts(endpoint_cap_t ep_cap, const char* str);
  void    move_cursor(endpoint_cap_t ep_cap, std::streamoff offset);
  ssize_t raw_read(endpoint_cap_t ep_cap, char* dst, size_t max_size);
  ssize_t cooked_read(endpoint_cap_t ep_cap, char* dst, size_t max_size);
};

#endif // CONS_CONSOLE_H_
