#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <service/fs.h>
#include <string.h>

id_cap_t fs_mount(endpoint_cap_t ep_cap, const char* root_path) {
  assert(ep_cap != 0);
  assert(root_path != NULL);

  size_t root_path_len = strlen(root_path) + 1;

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2 + root_path_len);
  __if_unlikely (msg == NULL) {
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_MOUNT);
  set_ipc_cap(msg, 1, unwrap_sysret(sys_endpoint_cap_copy(ep_cap)), false);
  set_ipc_data_array(msg, 2, root_path, root_path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return 0;
  }

  int      result = get_ipc_data(msg, 0);
  id_cap_t id_cap = move_ipc_cap(msg, 1);

  delete_ipc_message(msg);

  __if_unlikely (result != FS_CODE_S_OK) {
    return 0;
  }

  return id_cap;
}

void fs_unmount(id_cap_t id_cap) {
  assert(id_cap != 0);

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2);

  set_ipc_data(msg, 0, FS_MSG_TYPE_UNMOUNT);
  set_ipc_cap(msg, 1, id_cap, false);

  sys_endpoint_cap_call(__fs_ep_cap, msg);
  delete_ipc_message(msg);
}

bool fs_mounted(const char* root_path) {
  assert(root_path != NULL);

  size_t root_path_len = strlen(root_path) + 1;

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 1 + root_path_len);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_MOUNTED);
  set_ipc_data_array(msg, 1, root_path, root_path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  int  result  = get_ipc_data(msg, 0);
  bool mounted = get_ipc_data(msg, 1);

  delete_ipc_message(msg);

  return result == FS_CODE_S_OK && mounted != 0;
}

bool fs_exists(const char* path) {
  assert(path != NULL);

  size_t path_len = strlen(path) + 1;

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 1 + path_len);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_EXISTS);
  set_ipc_data_array(msg, 1, path, path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  int  result = get_ipc_data(msg, 0);
  bool exists = get_ipc_data(msg, 1);

  delete_ipc_message(msg);

  return result == FS_CODE_S_OK && exists != 0;
}

id_cap_t fs_open(const char* path) {
  assert(path != NULL);

  size_t path_len = strlen(path) + 1;

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 1 + path_len);
  __if_unlikely (msg == NULL) {
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_OPEN);
  set_ipc_data_array(msg, 1, path, path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return 0;
  }

  int      result = get_ipc_data(msg, 0);
  id_cap_t fd     = move_ipc_cap(msg, 1);

  delete_ipc_message(msg);

  __if_unlikely (result != FS_CODE_S_OK) {
    return 0;
  }

  return fd;
}

void fs_close(id_cap_t fd) {
  assert(fd != 0);

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2);
  __if_unlikely (msg == NULL) {
    return;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_CLOSE);
  set_ipc_cap(msg, 1, fd, false);

  sys_endpoint_cap_call(__fs_ep_cap, msg);
  delete_ipc_message(msg);
}

ssize_t fs_read(id_cap_t fd, void* buf, size_t count) {
  assert(fd != 0);
  assert(buf != NULL);

  __if_unlikely (count == 0) {
    return 0;
  }

  message_t* msg = new_ipc_message(0x1000);
  __if_unlikely (msg == NULL) {
    return -1;
  }

  const size_t chunk_size = 0x1000 - sizeof(uintptr_t) * 4;

  ssize_t read_size = 0;
  while (read_size < (ssize_t)count) {
    size_t chunk_read_size = count - read_size;
    if (chunk_read_size > chunk_size) {
      chunk_read_size = chunk_size;
    }

    set_ipc_data(msg, 0, FS_MSG_TYPE_READ);
    set_ipc_cap(msg, 1, fd, true);
    set_ipc_data(msg, 2, chunk_read_size);

    sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);

    __if_unlikely (sysret_failed(sysret)) {
      set_ipc_data(msg, 1, 0);
      read_size = -1;
      break;
    }

    int result = get_ipc_data(msg, 0);

    __if_unlikely (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) {
      read_size = -1;
      break;
    }

    size_t act_size = get_ipc_data(msg, 1);
    memcpy((char*)buf + read_size, get_ipc_data_ptr(msg, 2), act_size);

    read_size += act_size;

    __if_unlikely (result == FS_CODE_E_EOF) {
      break;
    }

    destroy_ipc_message(msg);
  }

  delete_ipc_message(msg);

  return read_size;
}

ssize_t fs_write(id_cap_t fd, const void* buf, size_t count) {
  assert(fd != 0);
  assert(buf != NULL);

  __if_unlikely (count == 0) {
    return 0;
  }

  message_t* msg = new_ipc_message(0x1000);
  __if_unlikely (msg == NULL) {
    return -1;
  }

  const size_t chunk_size = 0x1000 - sizeof(uintptr_t) * 4;
  const char*  ptr        = (const char*)buf;

  ssize_t written_size = 0;
  while (written_size < (ssize_t)count) {
    size_t chunk_write_size = count - written_size;
    if (chunk_write_size > chunk_size) {
      chunk_write_size = chunk_size;
    }

    set_ipc_data(msg, 0, FS_MSG_TYPE_WRITE);
    set_ipc_cap(msg, 1, fd, true);
    set_ipc_data(msg, 2, chunk_write_size);
    set_ipc_data_array(msg, 3, ptr + written_size, chunk_write_size);

    sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);

    __if_unlikely (sysret_failed(sysret)) {
      written_size = -1;
      break;
    }

    int result = get_ipc_data(msg, 0);

    __if_unlikely (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) {
      written_size = -1;
      break;
    }

    size_t act_size = get_ipc_data(msg, 1);
    written_size += act_size;

    __if_unlikely (result == FS_CODE_E_EOF) {
      break;
    }

    destroy_ipc_message(msg);
  }

  delete_ipc_message(msg);

  return written_size;
}
