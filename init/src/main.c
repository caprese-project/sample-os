#include <crt/global.h>
#include <init/launch.h>
#include <service/fs.h>

static task_context_t mm_ctx;
static task_context_t apm_ctx;
static task_context_t fs_ctx;
static task_context_t ramfs_ctx;
static task_context_t dm_ctx;
static task_context_t shell_ctx;

int main(void) {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  __this_task_cap = root_boot_info->root_task_cap;

  launch_mm(&mm_ctx);
  launch_apm(&apm_ctx);
  launch_fs(&fs_ctx);
  launch_ramfs(&ramfs_ctx);

  while (!fs_mounted("/init")) {
    unwrap_sysret(sys_system_yield());
  }

  launch_dm(&dm_ctx);
  launch_shell(&shell_ctx);

  while (true) {
    unwrap_sysret(sys_system_yield());
  }
}
