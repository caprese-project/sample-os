#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/fs.h>
#include <string.h>

id_cap_t fs_mount(endpoint_cap_t ep_cap, const char* root_path) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_MOUNT);
  set_ipc_cap(msg, 1, unwrap_sysret(sys_endpoint_cap_copy(ep_cap)), false);
  set_ipc_data_str(msg, 2, root_path);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return 0;
  }

  __if_unlikely (get_ipc_data(msg, 0) != FS_CODE_S_OK) {
    delete_ipc_message(msg);
    return 0;
  }

  id_cap_t id_cap = move_ipc_cap(msg, 1);

  delete_ipc_message(msg);

  return id_cap;
}

void fs_unmount(id_cap_t id_cap) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_UNMOUNT);
  set_ipc_cap(msg, 1, id_cap, false);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return;
  }

  delete_ipc_message(msg);
}

bool fs_mounted(const char* root_path) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_MOUNTED);
  set_ipc_data_str(msg, 1, root_path);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  __if_unlikely (get_ipc_data(msg, 0) != FS_CODE_S_OK) {
    delete_ipc_message(msg);
    return false;
  }

  bool mounted = get_ipc_data(msg, 1);

  delete_ipc_message(msg);

  return mounted;
}

bool fs_info(const char* path, struct fs_file_info* dst) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_INFO);
  set_ipc_data_str(msg, 1, path);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  __if_unlikely (get_ipc_data(msg, 0) != FS_CODE_S_OK) {
    delete_ipc_message(msg);
    return false;
  }

  const void* data = get_ipc_data_ptr(msg, 1);
  __if_unlikely (data == NULL) {
    delete_ipc_message(msg);
    return false;
  }

  memcpy(dst, data, sizeof(struct fs_file_info));
  delete_ipc_message(msg);

  return true;
}

bool fs_create(const char* path, int type) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_CREATE);
  set_ipc_data_str(msg, 1, path);
  set_ipc_data(msg, 2, type);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  bool result = get_ipc_data(msg, 0) == FS_CODE_S_OK;

  delete_ipc_message(msg);

  return result;
}

bool fs_remove(const char* path) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_REMOVE);
  set_ipc_data_str(msg, 1, path);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  bool result = get_ipc_data(msg, 0) == FS_CODE_S_OK;

  delete_ipc_message(msg);

  return result;
}

id_cap_t fs_open(const char* path) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_OPEN);
  set_ipc_data_str(msg, 1, path);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return 0;
  }

  __if_unlikely (get_ipc_data(msg, 0) != FS_CODE_S_OK) {
    delete_ipc_message(msg);
    return 0;
  }

  id_cap_t fd = move_ipc_cap(msg, 1);

  delete_ipc_message(msg);

  return fd;
}

void fs_close(id_cap_t fd) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_CLOSE);
  set_ipc_cap(msg, 1, fd, false);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return;
  }

  delete_ipc_message(msg);
}

ssize_t fs_read(id_cap_t fd, void* buf, size_t count) {
  __if_unlikely (count > FS_READ_MAX_SIZE) {
    return -1;
  }

  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return -1;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_READ);
  set_ipc_cap(msg, 1, fd, true);
  set_ipc_data(msg, 2, count);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return -1;
  }

  __if_unlikely (get_ipc_data(msg, 0) != FS_CODE_S_OK && get_ipc_data(msg, 0) != FS_CODE_E_EOF) {
    delete_ipc_message(msg);
    return -1;
  }

  ssize_t n = get_ipc_data(msg, 1);
  if (n > 0) {
    const void* data = get_ipc_data_ptr(msg, 2);
    __if_unlikely (data == NULL) {
      delete_ipc_message(msg);
      return -1;
    }

    memcpy(buf, data, n);
  }

  delete_ipc_message(msg);

  return n;
}

ssize_t fs_write(id_cap_t fd, const void* buf, size_t count) {
  __if_unlikely (count > FS_WRITE_MAX_SIZE) {
    return -1;
  }

  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return -1;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_WRITE);
  set_ipc_cap(msg, 1, fd, true);
  set_ipc_data(msg, 2, count);
  set_ipc_data_array(msg, 3, buf, count);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return -1;
  }

  __if_unlikely (get_ipc_data(msg, 0) != FS_CODE_S_OK) {
    delete_ipc_message(msg);
    return -1;
  }

  ssize_t n = get_ipc_data(msg, 1);

  delete_ipc_message(msg);

  return n;
}

bool fs_seek(id_cap_t fd, intptr_t offset, int whence) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_SEEK);
  set_ipc_cap(msg, 1, fd, true);
  set_ipc_data(msg, 2, offset);
  set_ipc_data(msg, 3, whence);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  bool result = get_ipc_data(msg, 0) == FS_CODE_S_OK;

  delete_ipc_message(msg);

  return result;
}

intptr_t fs_tell(id_cap_t fd) {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
  __if_unlikely (msg == NULL) {
    return -1;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_TELL);
  set_ipc_cap(msg, 1, fd, true);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, msg);
  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return -1;
  }

  intptr_t offset = get_ipc_data(msg, 1);

  delete_ipc_message(msg);

  return offset;
}
