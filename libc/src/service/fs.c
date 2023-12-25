#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/fs.h>
#include <string.h>

id_cap_t fs_mount(endpoint_cap_t ep_cap, const char* root_path) {
  assert(ep_cap != 0);
  assert(root_path != NULL);

  size_t root_path_len = strlen(root_path) + 1;

  message_buffer_t msg_buf;

  __if_unlikely (root_path_len > sizeof(msg_buf.data) - sizeof(msg_buf.data[0])) {
    return 0;
  }

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));

  msg_buf.data_part_length = 1 + (root_path_len + sizeof(msg_buf.data[0]) - 1) / sizeof(msg_buf.data[0]);

  assert(msg_buf.data_part_length <= sizeof(msg_buf.data) / sizeof(msg_buf.data[0]));

  msg_buf.data[msg_buf.cap_part_length] = FS_MSG_TYPE_MOUNT;

  char* str_start = (char*)(&msg_buf.data[msg_buf.cap_part_length + 1]);

  memcpy(str_start, root_path, root_path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

void fs_unmount(id_cap_t id_cap) {
  assert(id_cap != 0);

  message_buffer_t msg_buf;

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = msg_buf_transfer(id_cap);

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = FS_MSG_TYPE_UNMOUNT;

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK) {
    return;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 1);
}

bool fs_mounted(const char* root_path) {
  assert(root_path != NULL);

  size_t root_path_len = strlen(root_path) + 1;

  message_buffer_t msg_buf;

  __if_unlikely (root_path_len > sizeof(msg_buf.data) - sizeof(msg_buf.data[0])) {
    return false;
  }

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 1 + (root_path_len + sizeof(msg_buf.data[0]) - 1) / sizeof(msg_buf.data[0]);

  assert(msg_buf.data_part_length <= sizeof(msg_buf.data) / sizeof(msg_buf.data[0]));

  msg_buf.data[0] = FS_MSG_TYPE_MOUNTED;

  char* str_start = (char*)(&msg_buf.data[1]);

  memcpy(str_start, root_path, root_path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return false;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK) {
    return false;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 2);

  return msg_buf.data[1] != 0;
}

id_cap_t fs_open(const char* path) {
  assert(path != NULL);

  size_t path_len = strlen(path) + 1;

  message_buffer_t msg_buf;

  __if_unlikely (path_len > sizeof(msg_buf.data) - sizeof(msg_buf.data[0])) {
    return 0;
  }

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 1 + (path_len + sizeof(msg_buf.data[0]) - 1) / sizeof(msg_buf.data[0]);

  assert(msg_buf.data_part_length <= sizeof(msg_buf.data) / sizeof(msg_buf.data[0]));

  msg_buf.data[0] = FS_MSG_TYPE_OPEN;

  char* str_start = (char*)(&msg_buf.data[1]);

  memcpy(str_start, path, path_len);

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

void fs_close(id_cap_t fd) {
  assert(fd != 0);

  message_buffer_t msg_buf;

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 1;

  msg_buf.data[0] = FS_MSG_TYPE_CLOSE;

  msg_buf.data[1] = fd;

  sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK) {
    return;
  }

  sys_cap_destroy(fd);

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 1);
}

ssize_t fs_read(id_cap_t fd, void* buf, size_t count) {
  assert(fd != 0);
  assert(buf != NULL);

  __if_unlikely (count == 0) {
    return 0;
  }

  message_buffer_t msg_buf;

  const size_t chunk_size = sizeof(msg_buf.data) - sizeof(uintptr_t) * 2;

  size_t read_size = 0;
  while (read_size < count) {
    size_t chunk_read_size = count - read_size;
    if (chunk_read_size > chunk_size) {
      chunk_read_size = chunk_size;
    }

    msg_buf.cap_part_length  = 1;
    msg_buf.data_part_length = 2;

    msg_buf.data[0] = msg_buf_delegate(fd);

    msg_buf.data[1] = FS_MSG_TYPE_READ;
    msg_buf.data[2] = chunk_read_size;

    sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

    __if_unlikely (sysret_failed(sysret)) {
      return -1;
    }

    __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK && msg_buf.data[msg_buf.cap_part_length] != FS_CODE_E_EOF) {
      return -1;
    }

    assert(msg_buf.cap_part_length == 0);
    assert(msg_buf.data_part_length >= 2);

    size_t act_size = msg_buf.data[1];
    memcpy((char*)buf + read_size, &msg_buf.data[2], act_size);

    read_size += act_size;

    __if_unlikely (msg_buf.data[msg_buf.cap_part_length] == FS_CODE_E_EOF) {
      break;
    }
  }

  return read_size;
}

ssize_t fs_write(id_cap_t fd, const void* buf, size_t count) {
  assert(fd != 0);
  assert(buf != NULL);

  __if_unlikely (count == 0) {
    return 0;
  }

  message_buffer_t msg_buf;

  const size_t chunk_size = sizeof(msg_buf.data) - sizeof(uintptr_t) * 3;

  size_t write_size = 0;
  while (write_size < count) {
    size_t chunk_write_size = count - write_size;
    if (chunk_write_size > chunk_size) {
      chunk_write_size = chunk_size;
    }

    msg_buf.cap_part_length  = 1;
    msg_buf.data_part_length = 2 + (chunk_write_size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    msg_buf.data[0] = msg_buf_delegate(fd);

    msg_buf.data[1] = FS_MSG_TYPE_WRITE;
    msg_buf.data[2] = chunk_write_size;

    memcpy(&msg_buf.data[3], (const char*)buf + write_size, chunk_write_size);

    sysret_t sysret = sys_endpoint_cap_call(__fs_ep_cap, &msg_buf);

    __if_unlikely (sysret_failed(sysret)) {
      return -1;
    }

    __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != FS_CODE_S_OK && msg_buf.data[msg_buf.cap_part_length] != FS_CODE_E_EOF) {
      return -1;
    }

    assert(msg_buf.cap_part_length == 0);
    assert(msg_buf.data_part_length == 2);

    size_t act_size = msg_buf.data[1];
    write_size += act_size;

    __if_unlikely (msg_buf.data[msg_buf.cap_part_length] == FS_CODE_E_EOF) {
      break;
    }
  }

  return write_size;
}
