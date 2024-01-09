#ifndef MM_TASK_TABLE_H_
#define MM_TASK_TABLE_H_

#include <libcaprese/cxx/id_map.h>
#include <libcaprese/syscall.h>
#include <map>

struct task_info {
  task_cap_t                                           task_cap;
  size_t                                               stack_available;
  size_t                                               total_available;
  size_t                                               stack_commit;
  size_t                                               total_commit;
  std::map<int, std::map<uintptr_t, page_table_cap_t>> page_table_caps;
  std::map<uintptr_t, virt_page_cap_t>                 virt_page_caps;
};

class task_table {
  uintptr_t                  user_space_end;
  int                        max_page;
  caprese::id_map<task_info> table;

  static constexpr uintptr_t default_stack_available = MEGA_PAGE_SIZE;
  static constexpr uintptr_t default_total_available = static_cast<uintptr_t>(32) * GIGA_PAGE_SIZE;

public:
  task_table();
  ~task_table();

  task_table(const task_table&)            = delete;
  task_table(task_table&&)                 = delete;
  task_table& operator=(const task_table&) = delete;
  task_table& operator=(task_table&&)      = delete;

  int        attach(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, size_t stack_available, size_t total_available, size_t stack_commit, bool internal, const void* stack_data, size_t stack_data_size);
  int        detach(id_cap_t id);
  int        vmap(id_cap_t id, int level, int flags, uintptr_t va_base, uintptr_t* act_va_base);
  int        vremap(id_cap_t src_id, id_cap_t dst_id, int flags, uintptr_t src_va_base, uintptr_t dst_va_base, uintptr_t* act_va_base);
  int        vpmap(id_cap_t id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
  int        vpremap(id_cap_t src_id, id_cap_t dst_id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
  int        grow_stack(id_cap_t id, size_t size, const void* data, size_t data_size);
  task_info& get_task_info(id_cap_t id);

private:
  uintptr_t        random_va(id_cap_t id, int level);
  page_table_cap_t walk(id_cap_t id, int level, uintptr_t va_base);
  int              map(id_cap_t id, int level, int flags, uintptr_t va_base, const void* data, size_t data_size);
  int              remap(id_cap_t src_id, id_cap_t dst_id, int level, int flags, uintptr_t src_va_base, uintptr_t dst_va_base);
};

int        attach_task(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, size_t stack_available, size_t total_available, size_t stack_commit, bool internal, const void* stack_data, size_t stack_data_size);
int        detach_task(id_cap_t id);
int        vmap_task(id_cap_t id, int level, int flags, uintptr_t va_base, uintptr_t* act_va_base);
int        vremap_task(id_cap_t src_id, id_cap_t dst_id, int flags, uintptr_t src_va_base, uintptr_t dst_va_base, uintptr_t* act_va_base);
int        vpmap_task(id_cap_t id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
int        vpremap_task(id_cap_t src_id, id_cap_t dst_id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
int        grow_stack(id_cap_t id, size_t size);
task_info& get_task_info(id_cap_t id);

#endif // MM_TASK_TABLE_H_
