#include <cstdlib>
#include <libcaprese/syscall.h>
#include <map>
#include <mm/ipc.h>
#include <mm/memory_manager.h>
#include <mm/task_table.h>

namespace {
  struct id_cap_compare {
    bool operator()(id_cap_t lhs, id_cap_t rhs) const {
      intptr_t result = static_cast<intptr_t>(unwrap_sysret(sys_id_cap_compare(lhs, rhs)));
      return result < 0;
    }
  };

  struct task_info {
    task_cap_t                                           task_cap;
    page_table_cap_t                                     root_page_table_cap;
    uintptr_t                                            brk_root;
    uintptr_t                                            brk_pos;
    uintptr_t                                            brk_tail;
    std::map<int, std::map<uintptr_t, page_table_cap_t>> page_table_caps;
    std::map<uintptr_t, virt_page_cap_t>                 virt_page_caps;
  };

  std::map<id_cap_t, task_info, id_cap_compare> task_table;

  struct system_info_t {
    uintptr_t user_space_end;
    uintptr_t mmu_mode;
    int       max_page_level;

    system_info_t() {
      user_space_end = unwrap_sysret(sys_system_user_space_end());
      mmu_mode       = unwrap_sysret(sys_arch_mmu_mode());

      switch (mmu_mode) {
        case RISCV_MMU_SV39:
          max_page_level = RISCV_MMU_SV39_MAX_PAGE;
          break;
        case RISCV_MMU_SV48:
          max_page_level = RISCV_MMU_SV48_MAX_PAGE;
          break;
        default:
          abort();
      }
    }
  };

  const system_info_t system_info;

  constexpr uintptr_t round_up(uintptr_t value, uintptr_t align) {
    return (value + align - 1) / align * align;
  }

  constexpr uintptr_t round_down(uintptr_t value, uintptr_t align) {
    return value / align * align;
  }

  int map(task_info& info, uintptr_t va) {
    assert(va % MEGA_PAGE_SIZE == 0);

    if (info.virt_page_caps.contains(va)) {
      return MM_CODE_S_OK;
    }

    int index = get_page_table_index(va, MEGA_PAGE);

    page_table_cap_t page_table_cap = info.root_page_table_cap;

    for (int level = system_info.max_page_level; level >= MEGA_PAGE; --level) {
      uintptr_t va_base = round_down(va, get_page_size(level));

      if (!info.page_table_caps[level].contains(va_base)) {
        mem_cap_t mem_cap = fetch_mem_cap(KILO_PAGE_SIZE, KILO_PAGE_SIZE, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
        if (mem_cap == 0) [[unlikely]] {
          return MM_CODE_E_FAILURE;
        }

        page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
        if (sysret_failed(sys_page_table_cap_map_table(page_table_cap, get_page_table_index(va, level), next_page_table_cap))) [[unlikely]] {
          return MM_CODE_E_FAILURE;
        }

        info.page_table_caps[level][va_base] = next_page_table_cap;
      }

      page_table_cap = info.page_table_caps[level][va_base];
    }

    mem_cap_t mem_cap = fetch_mem_cap(MEGA_PAGE_SIZE, MEGA_PAGE_SIZE, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
    if (mem_cap == 0) [[unlikely]] {
      return MM_CODE_E_FAILURE;
    }

    virt_page_cap_t virt_page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, MEGA_PAGE));
    if (sysret_failed(sys_page_table_cap_map_page(page_table_cap, index, true, true, false, virt_page_cap))) {
      return MM_CODE_E_FAILURE;
    }
    info.virt_page_caps[va] = virt_page_cap;

    // TODO: Release virt page resources.

    return MM_CODE_S_OK;
  }

  int unmap(task_info& info, uintptr_t va) {
    assert(va % MEGA_PAGE_SIZE == 0);

    if (!info.virt_page_caps.contains(va)) {
      return MM_CODE_S_OK;
    }

    assert(info.page_table_caps[MEGA_PAGE].contains(va));

    int              index          = get_page_table_index(va, MEGA_PAGE);
    virt_page_cap_t  virt_page_cap  = info.virt_page_caps[va];
    page_table_cap_t page_table_cap = info.page_table_caps[MEGA_PAGE][va];
    if (sysret_failed(sys_page_table_cap_unmap_page(page_table_cap, index, virt_page_cap))) {
      return MM_CODE_E_FAILURE;
    }

    info.virt_page_caps.erase(va);

    return MM_CODE_S_OK;
  }

  int shrink(task_info& info, size_t size) {
    uintptr_t new_pos  = info.brk_pos - size;
    uintptr_t new_tail = round_up(new_pos, MEGA_PAGE_SIZE);

    for (uintptr_t page = info.brk_tail - MEGA_PAGE_SIZE; page >= new_tail; page -= MEGA_PAGE_SIZE) {
      int result = unmap(info, page);
      if (result != MM_CODE_S_OK) [[unlikely]] {
        return result;
      }
    }

    info.brk_pos  = new_pos;
    info.brk_tail = new_tail;

    return MM_CODE_S_OK;
  }

  int grow(task_info& info, size_t size) {
    uintptr_t new_pos  = info.brk_pos + size;
    uintptr_t new_tail = round_up(new_pos, MEGA_PAGE_SIZE);

    for (uintptr_t page = info.brk_tail; page < new_pos; page += MEGA_PAGE_SIZE) {
      int result = map(info, page);
      if (result != MM_CODE_S_OK) [[unlikely]] {
        return result;
      }
    }

    info.brk_pos  = new_pos;
    info.brk_tail = new_tail;

    return MM_CODE_S_OK;
  }
} // namespace

int attach_task(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, uintptr_t heap_root, uintptr_t heap_tail) {
  if (task_table.contains(id)) [[unlikely]] {
    return MM_CODE_E_ALREADY_ATTACHED;
  }

  if (heap_root % MEGA_PAGE_SIZE != 0 || heap_tail % MEGA_PAGE_SIZE != 0) [[unlikely]] {
    return MM_CODE_E_ILL_ARGS;
  }

  task_table[id] = {
    .task_cap            = task,
    .root_page_table_cap = root_page_table,
    .brk_root            = heap_root,
    .brk_pos             = heap_tail,
    .brk_tail            = heap_tail,
    .page_table_caps     = {},
    .virt_page_caps      = {},
  };

  return MM_CODE_S_OK;
}

int detach_task(id_cap_t id) {
  if (!task_table.contains(id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  // TODO: release resources

  task_table.erase(id);

  return MM_CODE_S_OK;
}

int sbrk_task(id_cap_t id, intptr_t increment, uintptr_t* new_brk_pos) {
  assert(new_brk_pos != nullptr);

  if (!task_table.contains(id)) [[unlikely]] {
    return MM_CODE_E_NOT_ATTACHED;
  }

  task_info& info = task_table[id];

  intptr_t head    = static_cast<intptr_t>(info.brk_root);
  intptr_t tail    = static_cast<intptr_t>(info.brk_tail);
  intptr_t old_pos = static_cast<intptr_t>(info.brk_pos);
  intptr_t new_pos = old_pos + increment;

  if (new_pos < head) [[unlikely]] {
    return MM_CODE_E_OUT_OF_RANGE;
  }

  intptr_t heap_end = system_info.user_space_end - MM_MAX_STACK_SIZE;
  if (new_pos >= heap_end) [[unlikely]] {
    return MM_CODE_E_OUT_OF_RANGE;
  }

  if (increment < 0) {
    if (new_pos >= tail || new_pos > old_pos) [[unlikely]] {
      return MM_CODE_E_OUT_OF_RANGE;
    }

    int result = shrink(info, old_pos - new_pos);
    if (result != MM_CODE_S_OK) {
      return result;
    }
  } else if (increment > 0) {
    if (new_pos < old_pos) [[unlikely]] {
      return MM_CODE_E_OUT_OF_RANGE;
    }

    int result = grow(info, new_pos - old_pos);
    if (result != MM_CODE_S_OK) {
      return result;
    }
  }

  *new_brk_pos = info.brk_pos;

  return MM_CODE_S_OK;
}
