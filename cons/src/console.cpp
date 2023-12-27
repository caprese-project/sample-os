#include <cons/console.h>
#include <cstring>

console::console(console_ops ops, console_mode mode): ops(ops), mode(mode), cursor_pos(0), esc(false) { }

ssize_t console::read(endpoint_cap_t ep_cap, char* dst, size_t max_size) {
  switch (this->mode) {
    case console_mode::raw:
      return this->raw_read(ep_cap, dst, max_size);
    case console_mode::cooked:
      return this->cooked_read(ep_cap, dst, max_size);
  }
  return 0;
}

ssize_t console::write(endpoint_cap_t ep_cap, const char* str, size_t size) {
  this->puts(ep_cap, str, size);
  return size;
}

char console::getc(endpoint_cap_t ep_cap) {
  char ch;

  do {
    ch = this->ops.getc(ep_cap);
  } while (ch == static_cast<char>(-1));

  return ch;
}

void console::puts(endpoint_cap_t ep_cap, const char* str, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    this->ops.putc(ep_cap, str[i]);
  }
}

void console::puts(endpoint_cap_t ep_cap, const char* str) {
  this->puts(ep_cap, str, strlen(str));
}

void console::move_cursor(endpoint_cap_t ep_cap, std::streamoff offset) {
  if (offset < 0) {
    size_t n = std::min<size_t>(-offset, this->cursor_pos);
    if (n == 0) {
      return;
    }

    std::string cursor_move = "\e[";
    cursor_move += std::to_string(n);
    cursor_move += "D";
    this->puts(ep_cap, cursor_move.c_str(), cursor_move.size());

    this->cursor_pos -= n;
  } else if (offset > 0) {
    size_t n = std::min<size_t>(this->cursor_pos + offset, this->buffer.size()) - this->cursor_pos;
    if (n == 0) {
      return;
    }

    std::string cursor_move = "\e[";
    cursor_move += std::to_string(n);
    cursor_move += "C";
    this->puts(ep_cap, cursor_move.c_str(), cursor_move.size());

    this->cursor_pos += n;
  }
}

ssize_t console::raw_read(endpoint_cap_t ep_cap, char* dst, size_t max_size) {
  for (size_t i = 0; i < max_size; ++i) {
    char ch = this->getc(ep_cap);

    // CR/LF
    if (ch == '\r' || ch == '\n') {
      this->ops.putc(ep_cap, '\n');
    } else {
      this->ops.putc(ep_cap, ch);
    }

    dst[i] = ch;
  }

  return max_size;
}

ssize_t console::cooked_read(endpoint_cap_t ep_cap, char* dst, size_t max_size) {
  if (this->buffer.empty()) [[unlikely]] {
    char ch;
    do {
      ch = this->getc(ep_cap);

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
              this->ops.putc(ep_cap, '\b');
              this->puts(ep_cap, this->buffer.c_str() + this->cursor_pos, this->buffer.size() - this->cursor_pos);
              this->ops.putc(ep_cap, ' ');

              std::string cursor_move = "\e[";
              cursor_move += std::to_string(this->buffer.size() - this->cursor_pos + 1);
              cursor_move += "D";
              this->puts(ep_cap, cursor_move.c_str(), cursor_move.size());

              this->buffer.erase(this->cursor_pos - 1, 1);
              --this->cursor_pos;
            } else if (this->buffer.size() != 0) {
              this->puts(ep_cap, "\b \b", 3);

              this->buffer.pop_back();
              --this->cursor_pos;
            }
            break;
          case '\r':
          case '\n': {
            this->ops.putc(ep_cap, '\n');
            this->buffer += '\n';
            this->cursor_pos = 0;
            break;
          }
          default:
            if (this->cursor_pos == this->buffer.size()) {
              this->buffer += ch;
              this->ops.putc(ep_cap, ch);
              ++this->cursor_pos;
            } else {
              this->ops.putc(ep_cap, ch);
              this->puts(ep_cap, this->buffer.c_str() + this->cursor_pos, this->buffer.size() - this->cursor_pos);

              std::string cursor_move = "\e[";
              cursor_move += std::to_string(this->buffer.size() - this->cursor_pos);
              cursor_move += "D";
              this->puts(ep_cap, cursor_move.c_str(), cursor_move.size());

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
          this->move_cursor(ep_cap, off);
        } else if (ch == 'D') {
          std::streamoff off;
          if (this->esc_buffer == "\e[D") {
            off = -1;
          } else {
            off = -std::stoll(this->esc_buffer.substr(2, this->esc_buffer.size() - 3));
          }
          this->move_cursor(ep_cap, off);
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
