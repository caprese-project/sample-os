#ifndef RAMFS_STREAM_H_
#define RAMFS_STREAM_H_

#include <functional>
#include <ios>
#include <ramfs/file.h>

class file_stream {
  std::reference_wrapper<class file> file;
  std::streampos                     pos;

public:
  file_stream(class file& file);

  file_stream(const file_stream&)            = delete;
  file_stream& operator=(const file_stream&) = delete;
  file_stream(file_stream&&)                 = default;
  file_stream& operator=(file_stream&&)      = default;

  [[nodiscard]] std::streamsize read(char* buffer, std::streamsize size);
  [[nodiscard]] std::streamsize write(std::string_view data);
  [[nodiscard]] std::streampos  seek(std::streamoff off, std::ios_base::seekdir dir);
  [[nodiscard]] std::streampos  tell() const;
};

#endif // RAMFS_STREAM_H_
