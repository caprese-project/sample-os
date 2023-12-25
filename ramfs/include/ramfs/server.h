#ifndef RAMFS_SERVER_H_
#define RAMFS_SERVER_H_

#include <cstdint>
#include <libcaprese/cap.h>

extern id_cap_t ramfs_id_cap;

[[noreturn]] void run(uintptr_t ramfs_va_base);

#endif // RAMFS_SERVER_H_
