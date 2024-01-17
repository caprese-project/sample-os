#include <ramfs/file_stream.h>

file_stream::file_stream(class file& file): file(file), pos(0) { }

std::streamsize file_stream::read(char* buffer, std::streamsize size) {
  std::streamsize act_size = file.get().read(pos, buffer, size);
  pos += act_size;
  return act_size;
}

std::streamsize file_stream::write(std::string_view data) {
  std::streamsize act_size = file.get().write(pos, data);
  pos += act_size;
  return act_size;
}

std::streampos file_stream::seek(std::streamoff off, std::ios_base::seekdir dir) {
  switch (dir) {
    case std::ios_base::beg:
      pos = off;
      break;
    case std::ios_base::cur:
      pos += off;
      break;
    case std::ios_base::end:
      pos = file.get().size() + off;
      break;
  }

  return pos;
}

std::streampos file_stream::tell() const {
  return pos;
}
