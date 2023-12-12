#include <cstdlib>
#include <libcaprese/syscall.h>
#include <map>
#include <mm/ipc.h>
#include <mm/memory_manager.h>
#include <mm/task_table.h>

extern id_cap_t __this_id_cap;

namespace {
  constexpr uintptr_t round_up(uintptr_t value, uintptr_t align) {
    return (value + align - 1) / align * align;
  }

  constexpr uintptr_t round_down(uintptr_t value, uintptr_t align) {
    return value / align * align;
  }

  task_table table;
} // namespace

task_table::task_table(): user_space_end(0), max_page(0) {
  user_space_end = unwrap_sysret(sys_system_user_space_end());
  max_page       = get_max_page();
}

task_table::~task_table() {
  // TODO: Release resources.
}

int task_table::attach(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, size_t stack_available, size_t total_available, size_t stack_commit) {
  assert(unwrap_sysret(sys_cap_type(id)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(task)) == CAP_TASK);
  assert(unwrap_sysret(sys_cap_type(root_page_table)) == CAP_PAGE_TABLE);

  if (table.contains(id)) [[unlikely]] {
    return MM_CODE_E_ALREADY_ATTACHED;
  }

  if (stack_available == 0) {
    stack_available = default_stack_available;
  }

  if (total_available == 0) {
    total_available = default_total_available;
  }

  if (stack_available > total_available) [[unlikely]] {
    return MM_CODE_E_ILL_ARGS;
  }

  if (stack_available < stack_commit) [[unlikely]] {
    return MM_CODE_E_ILL_ARGS;
  }

  task_info& info = table[id] = {
    .task_cap        = task,
    .stack_available = stack_available,
    .total_available = total_available,
    .stack_commit    = 0,
    .total_commit    = 0,
    .page_table_caps = {},
    .virt_page_caps  = {},
  };

  info.page_table_caps[max_page][0] = root_page_table;

  if (id != __this_id_cap) {
    if (stack_commit > 0) {
      grow_stack(id, stack_commit);
    }

    if (sysret_failed(sys_task_cap_set_reg(task, REG_STACK_POINTER, user_space_end))) [[unlikely]] {
      return MM_CODE_E_FAILURE;
    }
  } else {
    info.stack_commit = stack_commit;
    info.total_commit = stack_commit;
  }

  return MM_CODE_S_OK;
}

int task_table::detach(id_cap_t id) {
  // TODO: Release resources.

  if (!table.contains(id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  table.erase(id);

  return MM_CODE_S_OK;
}

int task_table::vmap(id_cap_t id, int level, int flags, uintptr_t va_base, virt_page_cap_t* dst, uintptr_t* act_va_base) {
  if (!table.contains(id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  if (level < KILO_PAGE || level > max_page) [[unlikely]] {
    return MM_CODE_E_ILL_ARGS;
  }

  task_info& info = table[id];

  if (va_base == 0) {
    va_base = random_va(id, level);
  }

  if (va_base % get_page_size(level)) [[unlikely]] {
    return MM_CODE_E_ILL_ARGS;
  }

  if (user_space_end - info.stack_available - get_page_size(level) <= va_base) [[unlikely]] {
    return MM_CODE_E_ILL_ARGS;
  }

  int result = map(id, level, flags, va_base, dst);
  if (result != MM_CODE_S_OK) [[unlikely]] {
    return result;
  }

  *act_va_base = va_base;
  return result;
}

int task_table::vremap(id_cap_t src_id, id_cap_t dst_id, int flags, uintptr_t dst_va_base, virt_page_cap_t page, uintptr_t* act_va_base) {
  if (!table.contains(src_id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  if (!table.contains(dst_id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  if (table[src_id].virt_page_caps.contains(page)) [[unlikely]] {
    return MM_CODE_E_NOT_MAPPED;
  }

  if (!unwrap_sysret(sys_virt_page_cap_mapped(page))) [[unlikely]] {
    return MM_CODE_E_NOT_MAPPED;
  }

  int level = static_cast<int>(unwrap_sysret(sys_virt_page_cap_level(page)));

  if (dst_va_base == 0) {
    dst_va_base = random_va(dst_id, level);
  }

  if (table[dst_id].virt_page_caps.contains(dst_va_base)) [[unlikely]] {
    return MM_CODE_E_ALREADY_MAPPED;
  }

  int result = remap(src_id, dst_id, level, flags, dst_va_base, page);
  if (result != MM_CODE_S_OK) [[unlikely]] {
    return result;
  }

  *act_va_base = dst_va_base;
  return result;
}

task_info& task_table::get_task_info(id_cap_t id) {
  return table.at(id);
}

uintptr_t task_table::random_va(id_cap_t id, int level) {
  assert(table.contains(id));
  assert(level >= KILO_PAGE && level <= max_page);

  return 0;
}

page_table_cap_t task_table::walk(id_cap_t id, int level, uintptr_t va_base) {
  assert(table.contains(id));
  assert(va_base % get_page_size(level) == 0);
  assert(va_base > 0);
  assert(va_base <= user_space_end - get_page_size(level));

  task_info& info = table[id];

  page_table_cap_t page_table_cap = info.page_table_caps[max_page][0];

  for (int lv = max_page - 1; lv >= level; --lv) {
    uintptr_t base = get_page_table_base_addr(va_base, lv);

    if (!info.page_table_caps[lv].contains(base)) {
      int index = get_page_table_index(base, level);

      mem_cap_t mem_cap = fetch_mem_cap(KILO_PAGE_SIZE, KILO_PAGE_SIZE, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
      if (mem_cap == 0) [[unlikely]] {
        return 0;
      }

      page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
      if (sysret_failed(sys_page_table_cap_map_table(page_table_cap, index, next_page_table_cap))) [[unlikely]] {
        return 0;
      }

      info.page_table_caps[lv][base] = next_page_table_cap;
    }

    page_table_cap = info.page_table_caps[lv][base];
  }

  return page_table_cap;
}

int task_table::map(id_cap_t id, int level, int flags, uintptr_t va_base, virt_page_cap_t* dst) {
  assert(table.contains(id));
  assert(va_base % get_page_size(level) == 0);
  assert(va_base > 0);
  assert(va_base <= user_space_end - get_page_size(level));

  task_info& info = table[id];

  if (info.total_available < info.total_commit + get_page_size(level)) [[unlikely]] {
    return MM_CODE_E_OVERFLOW;
  }

  if (info.virt_page_caps.contains(va_base)) [[unlikely]] {
    return MM_CODE_E_ALREADY_MAPPED;
  }

  page_table_cap_t page_table_cap = walk(id, level, va_base);
  if (page_table_cap == 0) [[unlikely]] {
    return MM_CODE_E_FAILURE;
  }

  bool readable   = flags & MM_VMAP_FLAG_READ;
  bool writable   = flags & MM_VMAP_FLAG_WRITE;
  bool executable = flags & MM_VMAP_FLAG_EXEC;

  int fetch_flags = 0;
  if (readable) {
    fetch_flags |= MM_FETCH_FLAG_READ;
  }
  if (writable) {
    fetch_flags |= MM_FETCH_FLAG_WRITE;
  }
  if (executable) {
    fetch_flags |= MM_FETCH_FLAG_EXEC;
  }

  mem_cap_t mem_cap = fetch_mem_cap(get_page_size(level), get_page_size(level), fetch_flags);
  if (mem_cap == 0) [[unlikely]] {
    return MM_CODE_E_FAILURE;
  }

  int index = get_page_table_index(va_base, level);

  virt_page_cap_t virt_page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, level));
  if (sysret_failed(sys_page_table_cap_map_page(page_table_cap, index, readable, writable, executable, virt_page_cap))) {
    return MM_CODE_E_FAILURE;
  }

  info.virt_page_caps[va_base] = virt_page_cap;

  if (dst != nullptr) {
    *dst = virt_page_cap;
  }

  info.total_commit += get_page_size(level);

  return MM_CODE_S_OK;
}

int task_table::remap(id_cap_t src_id, id_cap_t dst_id, int level, int flags, uintptr_t dst_va_base, virt_page_cap_t page) {
  assert(table.contains(src_id));
  assert(table.contains(dst_id));
  assert(dst_va_base % get_page_size(level) == 0);
  assert(dst_va_base > 0);
  assert(dst_va_base <= user_space_end - get_page_size(level));
  assert(unwrap_sysret(sys_virt_page_cap_mapped(page)));
  assert(static_cast<int>(unwrap_sysret(sys_virt_page_cap_level(page))) == level);

  task_info& src_info = table[src_id];
  task_info& dst_info = table[dst_id];

  uintptr_t src_va_base = unwrap_sysret(sys_virt_page_cap_virt_addr(page));

  assert(src_info.virt_page_caps.contains(src_va_base));
  assert(unwrap_sysret(sys_virt_page_cap_phys_addr(src_info.virt_page_caps[src_va_base])) == unwrap_sysret(sys_virt_page_cap_phys_addr(page)));

  if (dst_info.total_available < dst_info.total_commit + get_page_size(level)) [[unlikely]] {
    return MM_CODE_E_OVERFLOW;
  }

  if (dst_info.virt_page_caps.contains(dst_va_base)) [[unlikely]] {
    return MM_CODE_E_ALREADY_MAPPED;
  }

  page_table_cap_t src_page_table_cap = walk(src_id, level, src_va_base);
  if (src_page_table_cap == 0) [[unlikely]] {
    return MM_CODE_E_FAILURE;
  }

  page_table_cap_t dst_page_table_cap = walk(dst_id, level, dst_va_base);
  if (dst_page_table_cap == 0) [[unlikely]] {
    return MM_CODE_E_FAILURE;
  }

  int  index      = get_page_table_index(dst_va_base, level);
  bool readable   = flags & MM_VMAP_FLAG_READ;
  bool writable   = flags & MM_VMAP_FLAG_WRITE;
  bool executable = flags & MM_VMAP_FLAG_EXEC;

  if (sysret_failed(sys_page_table_cap_remap_page(dst_page_table_cap, index, readable, writable, executable, page, src_page_table_cap))) [[unlikely]] {
    return MM_CODE_E_FAILURE;
  }

  dst_info.virt_page_caps[dst_va_base] = page;
  src_info.virt_page_caps.erase(src_va_base);

  dst_info.total_commit += get_page_size(level);
  src_info.total_commit -= get_page_size(level);

  return MM_CODE_S_OK;
}

int task_table::grow_stack(id_cap_t id, size_t size) {
  if (!table.contains(id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  size = round_up(size, KILO_PAGE_SIZE);

  task_info& info = table[id];

  if (info.stack_available < info.stack_commit + size) [[unlikely]] {
    return MM_CODE_E_OVERFLOW;
  }

  uintptr_t end   = user_space_end - info.stack_commit;
  uintptr_t start = end - size;

  assert(end % KILO_PAGE_SIZE == 0);
  assert(start % KILO_PAGE_SIZE == 0);

  while (start < end) {
    size_t sz = end - start;
    for (int level = max_page; level >= KILO_PAGE; --level) {
      size_t page_size = get_page_size(level);
      if (sz >= page_size && start % page_size == 0) {
        int result = map(id, level, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, start, nullptr);
        if (result != MM_CODE_S_OK) [[unlikely]] {
          return result;
        }
        start += page_size;
      }
    }
  }

  info.stack_commit += size;

  return MM_CODE_S_OK;
}

int attach_task(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, size_t stack_available, size_t total_available, size_t stack_commit) {
  return table.attach(id, task, root_page_table, stack_available, total_available, stack_commit);
}

int detach_task(id_cap_t id) {
  return table.detach(id);
}

int vmap_task(id_cap_t id, int level, int flags, uintptr_t va_base, virt_page_cap_t* dst, uintptr_t* act_va_base) {
  return table.vmap(id, level, flags, va_base, dst, act_va_base);
}

int vremap_task(id_cap_t src_id, id_cap_t dst_id, int flags, uintptr_t dst_va_base, virt_page_cap_t page, uintptr_t* act_va_base) {
  return table.vremap(src_id, dst_id, flags, dst_va_base, page, act_va_base);
}

int grow_stack(id_cap_t id, size_t size) {
  return table.grow_stack(id, size);
}

task_info& get_task_info(id_cap_t id) {
  return table.get_task_info(id);
}