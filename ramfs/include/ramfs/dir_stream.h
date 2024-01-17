#ifndef RAMFS_DIR_STREAM_H_
#define RAMFS_DIR_STREAM_H_

#include <functional>
#include <ramfs/directory.h>
#include <fs/ipc.h>

class dir_stream {
  std::reference_wrapper<directory> dir;
  std::streampos                    pos;

public:
  dir_stream(directory& dir);

  dir_stream(const dir_stream&)            = delete;
  dir_stream& operator=(const dir_stream&) = delete;
  dir_stream(dir_stream&&)                 = default;
  dir_stream& operator=(dir_stream&&)      = default;

  [[nodiscard]] std::streamsize read(fs_file_info* buffer);
};

#endif // RAMFS_DIR_STREAM_H_
