#ifndef MM_TASK_TABLE_H_
#define MM_TASK_TABLE_H_

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
  struct id_cap_compare {
    bool operator()(id_cap_t lhs, id_cap_t rhs) const {
      intptr_t result = static_cast<intptr_t>(unwrap_sysret(sys_id_cap_compare(lhs, rhs)));
      return result < 0;
    }
  };

  uintptr_t                                     user_space_end;
  int                                           max_page;
  std::map<id_cap_t, task_info, id_cap_compare> table;

  static constexpr uintptr_t default_stack_available = MEGA_PAGE_SIZE;
  static constexpr uintptr_t default_total_available = static_cast<uintptr_t>(32) * GIGA_PAGE_SIZE;

public:
  task_table();
  ~task_table();

  task_table(const task_table&)            = delete;
  task_table(task_table&&)                 = delete;
  task_table& operator=(const task_table&) = delete;
  task_table& operator=(task_table&&)      = delete;

  int        attach(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, size_t stack_available, size_t total_available, size_t stack_commit, bool internal);
  int        detach(id_cap_t id);
  int        vmap(id_cap_t id, int level, int flags, uintptr_t va_base, uintptr_t* act_va_base);
  int        vremap(id_cap_t src_id, id_cap_t dst_id, int flags, uintptr_t src_va_base, uintptr_t dst_va_base, uintptr_t* act_va_base);
  int        vpmap(id_cap_t id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
  int        vpremap(id_cap_t src_id, id_cap_t dst_id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
  int        grow_stack(id_cap_t id, size_t size);
  task_info& get_task_info(id_cap_t id);

private:
  uintptr_t        random_va(id_cap_t id, int level);
  page_table_cap_t walk(id_cap_t id, int level, uintptr_t va_base);
  int              map(id_cap_t id, int level, int flags, uintptr_t va_base);
  int              remap(id_cap_t src_id, id_cap_t dst_id, int level, int flags, uintptr_t src_va_base, uintptr_t dst_va_base);
};

int        attach_task(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, size_t stack_available, size_t total_available, size_t stack_commit, bool internal);
int        detach_task(id_cap_t id);
int        vmap_task(id_cap_t id, int level, int flags, uintptr_t va_base, uintptr_t* act_va_base);
int        vremap_task(id_cap_t src_id, id_cap_t dst_id, int flags, uintptr_t src_va_base, uintptr_t dst_va_base, uintptr_t* act_va_base);
int        vpmap_task(id_cap_t id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
int        vpremap_task(id_cap_t src_id, id_cap_t dst_id, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base, uintptr_t* act_va_base);
int        grow_stack(id_cap_t id, size_t size);
task_info& get_task_info(id_cap_t id);

#endif // MM_TASK_TABLE_H_
