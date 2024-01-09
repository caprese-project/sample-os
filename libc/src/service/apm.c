#include <apm/ipc.h>
#include <assert.h>
#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/apm.h>
#include <string.h>

static size_t round_up(size_t value, size_t align) {
  return (value + align - 1) / align * align;
}

task_cap_t apm_create(const char* path, const char* app_name, int flags) {
  assert(path != NULL);

  size_t path_len     = strlen(path) + 1;
  size_t app_name_len = app_name != NULL ? strlen(app_name) + 1 : 1;
  size_t total_len    = round_up(path_len, sizeof(uintptr_t)) + round_up(app_name_len, sizeof(uintptr_t));

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2 + total_len);
  __if_unlikely (msg == NULL) {
    return 0;
  }

  set_ipc_data(msg, 0, APM_MSG_TYPE_CREATE);
  set_ipc_data(msg, 1, flags);
  set_ipc_data_array(msg, 2, path, path_len);

  if (app_name != NULL) {
    set_ipc_data_array(msg, 2 + round_up(path_len, sizeof(uintptr_t)) / sizeof(uintptr_t), app_name, app_name_len);
  } else {
    set_ipc_data(msg, 2 + round_up(path_len, sizeof(uintptr_t)) / sizeof(uintptr_t), 0);
  }

  sysret_t sysret = sys_endpoint_cap_call(__apm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return 0;
  }

  int        result   = get_ipc_data(msg, 0);
  task_cap_t task_cap = move_ipc_cap(msg, 1);

  delete_ipc_message(msg);

  __if_unlikely (result != APM_CODE_S_OK) {
    return 0;
  }

  return task_cap;
}

endpoint_cap_t apm_lookup(const char* app_name) {
  assert(app_name != NULL);

  size_t app_name_len = strlen(app_name) + 1;

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 1 + app_name_len);
  __if_unlikely (msg == NULL) {
    return 0;
  }

  set_ipc_data(msg, 0, APM_MSG_TYPE_LOOKUP);
  set_ipc_data_array(msg, 1, app_name, app_name_len);

  sysret_t sysret = sys_endpoint_cap_call(__apm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return 0;
  }

  int            result = get_ipc_data(msg, 0);
  endpoint_cap_t ep_cap = move_ipc_cap(msg, 1);

  delete_ipc_message(msg);

  __if_unlikely (result != APM_CODE_S_OK) {
    return 0;
  }

  return ep_cap;
}

bool apm_attach(task_cap_t task_cap, endpoint_cap_t ep_cap, const char* app_name) {
  assert(task_cap != 0);
  assert(app_name != NULL);

  size_t app_name_len = strlen(app_name) + 1;

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 3 + app_name_len);
  __if_unlikely (msg == NULL) {
    return false;
  }

  set_ipc_data(msg, 0, APM_MSG_TYPE_ATTACH);
  set_ipc_cap(msg, 1, task_cap, false);
  set_ipc_cap(msg, 2, ep_cap, false);
  set_ipc_data_array(msg, 3, app_name, app_name_len);

  sysret_t sysret = sys_endpoint_cap_call(__apm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    delete_ipc_message(msg);
    return false;
  }

  int result = get_ipc_data(msg, 0);

  delete_ipc_message(msg);

  return result == APM_CODE_S_OK;
}
