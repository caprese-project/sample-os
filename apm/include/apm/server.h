#ifndef APM_SERVER_H_
#define APM_SERVER_H_

#include <libcaprese/cap.h>

extern endpoint_cap_t apm_ep_cap;

[[noreturn]] void run();

#endif // APM_SERVER_H_
