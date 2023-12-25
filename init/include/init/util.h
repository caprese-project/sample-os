#ifndef INIT_UTIL_H_
#define INIT_UTIL_H_

#include <init/cpio.h>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <stdint.h>

extern const char _ramfs_start[];
extern const char _ramfs_end[];

inline static void push_cap(message_buffer_t* msg_buf, cap_t cap) {
  msg_buf->data[msg_buf->cap_part_length++] = msg_buf_transfer(cap);
}

inline static uintptr_t round_up(uintptr_t value, uintptr_t align) {
  return (value + align - 1) / align * align;
}

inline static const char* ramfs_find(const char* name, size_t* size) {
  return cpio_find_file(_ramfs_start, _ramfs_end - _ramfs_start, name, size);
}

inline static cap_t copy_ep_cap_and_transfer(task_cap_t task_cap, endpoint_cap_t ep_cap) {
  cap_t cap_copy = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  return unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_copy));
}

#endif // INIT_UTIL_H_
