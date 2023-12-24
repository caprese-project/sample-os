#ifndef LIBC_SERVICE_APM_H_
#define LIBC_SERVICE_APM_H_

#include <apm/ipc.h>
#include <libcaprese/cap.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  task_cap_t apm_create(const char* path, const char* app_name, int flags);
  task_cap_t apm_lookup(const char* app_name);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_SERVICE_APM_H_
