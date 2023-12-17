#ifndef APM_RAMFS_H_
#define APM_RAMFS_H_

#include <cstddef>
#include <sstream>
#include <utility>

class ramfs {
  const char* _data;
  size_t      _size;

public:
  ramfs(const char* begin, const char* end);

  [[nodiscard]] bool               has_file(const char* name) const;
  [[nodiscard]] std::istringstream open_file(const char* name) const;

private:
  std::pair<const char*, size_t> find_file(const char* name) const;
};

#endif // APM_RAMFS_H_
