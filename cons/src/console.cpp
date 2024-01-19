#include <cons/console.h>
#include <cstring>
#include <libcaprese/syscall.h>

console::console(endpoint_cap_t ep_cap, console_ops ops, console_mode mode): ops(ops), mode(mode), cursor_pos(0), ep_cap(ep_cap), esc(false) { }

ssize_t console::read(char* dst, size_t max_size) {
  switch (this->mode) {
    case console_mode::raw:
      return this->raw_read(dst, max_size);
    case console_mode::cooked:
      return this->cooked_read(dst, max_size);
  }
  return 0;
}

ssize_t console::write(std::string_view str) {
  switch (this->mode) {
    case console_mode::raw:
      this->puts(str);
      break;
    case console_mode::cooked:
      for (const auto& ch : str) {
        if (ch == '\n') {
          this->putc('\r');
          this->putc('\n');
        } else {
          this->putc(ch);
        }
      }
      break;
    default:
      return 0;
  }
  return str.size();
}

char console::getc() {
  char ch = this->ops.getc(this->ep_cap);

  while (ch == static_cast<char>(-1)) {
    sys_system_yield();
    ch = this->ops.getc(this->ep_cap);
  };

  return ch;
}

void console::putc(char ch) {
  this->ops.putc(this->ep_cap, ch);
}

void console::puts(std::string_view str) {
  for (const auto& ch : str) {
    this->putc(ch);
  }
}

void console::move_cursor(std::streamoff offset) {
  if (offset < 0) {
    size_t n = std::min<size_t>(-offset, this->cursor_pos);
    if (n == 0) {
      return;
    }

    this->puts("\e[");
    this->puts(std::to_string(n));
    this->putc('D');

    this->cursor_pos -= n;
  } else if (offset > 0) {
    size_t n = std::min<size_t>(this->cursor_pos + offset, this->buffer.size()) - this->cursor_pos;
    if (n == 0) {
      return;
    }

    this->puts("\e[");
    this->puts(std::to_string(n));
    this->putc('C');

    this->cursor_pos += n;
  }
}

ssize_t console::raw_read(char* dst, size_t max_size) {
  for (size_t i = 0; i < max_size; ++i) {
    char ch = this->getc();

    // CR/LF
    if (ch == '\r' || ch == '\n') {
      this->puts("\r\n");
    } else {
      this->putc(ch);
    }

    dst[i] = ch;
  }

  return max_size;
}

ssize_t console::cooked_read(char* dst, size_t max_size) {
  if (this->buffer.empty()) [[unlikely]] {
    char ch;
    do {
      ch = this->getc();

      if (!this->esc) {
        switch (ch) {
          // ESC
          case '\e':
            this->esc        = true;
            this->esc_buffer = "\e";
            break;
          // BS/DEL
          case '\b':
          case '\x7f':
            if (this->cursor_pos == 0) {
              // Do nothing.
            } else if (this->cursor_pos < this->buffer.size()) {
              this->putc('\b');
              this->puts(std::string_view(this->buffer.c_str() + this->cursor_pos, this->buffer.size() - this->cursor_pos));
              this->putc(' ');

              this->puts("\e[");
              this->puts(std::to_string(this->buffer.size() - this->cursor_pos + 1));
              this->putc('D');

              this->buffer.erase(this->cursor_pos - 1, 1);
              --this->cursor_pos;
            } else if (this->buffer.size() != 0) {
              this->puts("\b \b");

              this->buffer.pop_back();
              --this->cursor_pos;
            }
            break;
          // CR/LF
          case '\r':
          case '\n': {
            this->puts("\r\n");
            this->buffer += '\n';
            this->cursor_pos = 0;
            break;
          }
          default:
            if (this->cursor_pos == this->buffer.size()) {
              this->buffer += ch;
              this->putc(ch);
              ++this->cursor_pos;
            } else {
              this->putc(ch);
              this->puts(std::string_view(this->buffer.c_str() + this->cursor_pos, this->buffer.size() - this->cursor_pos));

              this->puts("\e[");
              this->puts(std::to_string(this->buffer.size() - this->cursor_pos));
              this->putc('D');

              this->buffer.insert(this->cursor_pos, 1, ch);
              ++this->cursor_pos;
            }
            break;
        }
      } else {
        this->esc_buffer += ch;

        if (!this->esc_buffer.starts_with("\e[")) [[unlikely]] {
          this->esc = false;
          this->esc_buffer.clear();
          continue;
        }

        if (this->esc_buffer == "\e[") [[unlikely]] {
          continue;
        }

        if (isdigit(ch)) {
          continue;
        }

        if (ch == 'C') {
          std::streamoff off;
          if (this->esc_buffer == "\e[C") {
            off = 1;
          } else {
            off = std::stoull(this->esc_buffer.substr(2, this->esc_buffer.size() - 3));
          }
          this->move_cursor(off);
        } else if (ch == 'D') {
          std::streamoff off;
          if (this->esc_buffer == "\e[D") {
            off = -1;
          } else {
            off = -std::stoll(this->esc_buffer.substr(2, this->esc_buffer.size() - 3));
          }
          this->move_cursor(off);
        }

        this->esc = false;
        this->esc_buffer.clear();
      }
    } while (ch != '\n' && ch != '\r');
  }

  ssize_t result = static_cast<ssize_t>(this->buffer.copy(dst, max_size));
  this->buffer.erase(0, result);

  return result;
}
