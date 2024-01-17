#include <ramfs/dir_stream.h>

dir_stream::dir_stream(directory& dir): dir(dir), pos(0) { }

std::streamsize dir_stream::read(fs_file_info* buffer) {
  if (dir.get().read(pos, buffer) == 0) {
    return 0;
  }

  pos += 1;

  return sizeof(fs_file_info);
}
