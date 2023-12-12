#ifndef APM_PROGRAM_LOADER_H_
#define APM_PROGRAM_LOADER_H_

#include <cstdint>
#include <functional>
#include <istream>
#include <libcaprese/cap.h>

class task;

class program_loader {
protected:
  std::reference_wrapper<task>         target_ref;
  std::reference_wrapper<std::istream> stream_ref;

  virtual bool map_page(uintptr_t va, int level, int flags, const void* data = nullptr);

public:
  program_loader(std::reference_wrapper<task> target_ref, std::reference_wrapper<std::istream> stream_ref);

  virtual ~program_loader();

  virtual bool load() = 0;
};

#endif // APM_PROGRAM_LOADER_H_
