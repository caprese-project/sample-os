#include <apm/ipc.h>
#include <assert.h>
#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/apm.h>
#include <string.h>

task_cap_t apm_create(const char* path, const char* app_name, int flags) {
  assert(path != NULL);

  size_t path_len     = strlen(path) + 1;
  size_t app_name_len = app_name != NULL ? strlen(app_name) + 1 : 0;
  size_t total_len    = path_len + app_name_len;

  message_buffer_t msg_buf = {};

  __if_unlikely (total_len > sizeof(msg_buf.data) - sizeof(msg_buf.data[0]) * 2) {
    return 0;
  }

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 2 + (total_len + sizeof(msg_buf.data[0]) - 1) / sizeof(msg_buf.data[0]);

  assert(msg_buf.data_part_length <= sizeof(msg_buf.data) / sizeof(msg_buf.data[0]));

  msg_buf.data[0] = APM_MSG_TYPE_CREATE;
  msg_buf.data[1] = flags;

  char* str_start = (char*)(&msg_buf.data[2]);

  memcpy(str_start, path, path_len);
  if (app_name != NULL) {
    memcpy(str_start + path_len, app_name, app_name_len);
  } else {
    str_start[path_len] = '\0';
  }

  sysret_t sysret = sys_endpoint_cap_call(__apm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != APM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

task_cap_t apm_lookup(const char* app_name) {
  assert(app_name != NULL);

  size_t app_name_len = strlen(app_name) + 1;

  message_buffer_t msg_buf = {};

  __if_unlikely (app_name_len > sizeof(msg_buf.data) - sizeof(msg_buf.data[0])) {
    return 0;
  }

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 1 + (app_name_len + sizeof(msg_buf.data[0]) - 1) / sizeof(msg_buf.data[0]);

  assert(msg_buf.data_part_length <= sizeof(msg_buf.data) / sizeof(msg_buf.data[0]));

  msg_buf.data[0] = APM_MSG_TYPE_LOOKUP;

  memcpy(&msg_buf.data[1], app_name, app_name_len);

  sysret_t sysret = sys_endpoint_cap_call(__apm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != APM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}
