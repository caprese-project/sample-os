#include <cons/file.h>
#include <cons/support/uart.h>
#include <libcaprese/syscall.h>

file::file(std::string_view abs_path, file_type type, endpoint_cap_t ep_cap): abs_path(abs_path) {
  if (ep_cap == 0 || unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
    return;
  }

  console_ops ops;
  switch (type) {
    case file_type::uart:
      ops.putc = uart_putc;
      ops.getc = uart_getc;
      break;
    default:
      return;
  }

  this->console.emplace(ep_cap, ops);
}

const std::string& file::get_abs_path() const {
  return this->abs_path;
}

std::streamsize file::read(char* buffer, std::streamsize size) {
  if (!this->console.has_value()) [[unlikely]] {
    return -1;
  }

  return this->console->read(buffer, size);
}

std::streamsize file::write(std::string_view data) {
  if (!this->console.has_value()) [[unlikely]] {
    return -1;
  }

  return this->console->write(data);
}
