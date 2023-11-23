#include <mm/init.h>
#include <mm/server.h>

int main(endpoint_cap_t init_task_ep_cap) {
  endpoint_cap_t ep_cap = init(init_task_ep_cap);
  run(ep_cap);
}
