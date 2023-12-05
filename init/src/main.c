#include <crt/global.h>
#include <init/mem.h>
#include <init/task.h>
#include <libcaprese/root_boot_info.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdlib.h>

extern const char _apm_elf_start[];
extern const char _apm_elf_end[];
extern const char _mm_elf_start[];
extern const char _mm_elf_end[];
extern char       _init_end[];

static endpoint_cap_t create_ep_cap(root_boot_info_t* root_boot_info) {
  mem_cap_t ep_mem_cap = fetch_mem_cap(root_boot_info, false, true, true, false, unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT)), unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT)));
  if (ep_mem_cap == 0) {
    abort();
  }

  return unwrap_sysret(sys_mem_cap_create_endpoint_object(ep_mem_cap));
}

int main() {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  __this_task_cap = root_boot_info->root_task_cap;

  uintptr_t  mm_heap_root;
  task_cap_t mm_task_cap = create_task(root_boot_info, _mm_elf_start, _mm_elf_end - _mm_elf_start, &mm_heap_root);
  if (mm_task_cap == 0) {
    abort();
  }

  uintptr_t  apm_heap_root;
  task_cap_t apm_task_cap = create_task(root_boot_info, _apm_elf_start, _apm_elf_end - _apm_elf_start, &apm_heap_root);
  if (apm_task_cap == 0) {
    abort();
  }

  endpoint_cap_t ep_cap      = create_ep_cap(root_boot_info);
  endpoint_cap_t ep_cap_copy = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  endpoint_cap_t ep_cap_dst  = unwrap_sysret(sys_task_cap_transfer_cap(mm_task_cap, ep_cap_copy));

  unwrap_sysret(sys_task_cap_set_reg(mm_task_cap, REG_ARG_0, ep_cap_dst));
  unwrap_sysret(sys_task_cap_set_reg(mm_task_cap, REG_ARG_1, mm_heap_root));

  unwrap_sysret(sys_task_cap_resume(mm_task_cap));
  unwrap_sysret(sys_task_cap_switch(mm_task_cap));

  message_buffer_t msg_buf;
  msg_buf.cap_part_length = root_boot_info->num_mem_caps;
  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    msg_buf.data[i] = root_boot_info->mem_caps[i];
  }
  msg_buf.data_part_length                       = 2;
  msg_buf.data[root_boot_info->num_mem_caps + 0] = root_boot_info->arch_info.dtb_start;
  msg_buf.data[root_boot_info->num_mem_caps + 1] = root_boot_info->arch_info.dtb_end;
  unwrap_sysret(sys_endpoint_cap_send_long(ep_cap, &msg_buf));

  unwrap_sysret(sys_endpoint_cap_receive(ep_cap, &msg_buf));

  if (msg_buf.cap_part_length != 1) {
    abort();
  }

  endpoint_cap_t mm_ep_cap = msg_buf.data[0];
  if (unwrap_sysret(sys_cap_type(mm_ep_cap)) != CAP_ENDPOINT) {
    abort();
  }

  id_cap_t apm_mm_id_cap = mm_attach(mm_ep_cap, apm_task_cap, apm_heap_root);
  if (apm_mm_id_cap == 0) {
    abort();
  }

  endpoint_cap_t mm_ep_cap_dst = unwrap_sysret(sys_task_cap_transfer_cap(apm_task_cap, mm_ep_cap));
  id_cap_t apm_mm_id_cap_dst = unwrap_sysret(sys_task_cap_transfer_cap(apm_task_cap, apm_mm_id_cap));

  unwrap_sysret(sys_task_cap_set_reg(apm_task_cap, REG_ARG_0, mm_ep_cap_dst));
  unwrap_sysret(sys_task_cap_set_reg(apm_task_cap, REG_ARG_1, apm_mm_id_cap_dst));

  unwrap_sysret(sys_task_cap_resume(apm_task_cap));
  unwrap_sysret(sys_task_cap_switch(apm_task_cap));

  while (true) {
    unwrap_sysret(sys_system_yield());
  }

  return 0;
}
