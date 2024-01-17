#include <apm/elf_loader.h>
#include <apm/ipc.h>
#include <apm/server.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <cstring>
#include <functional>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/mm.h>
#include <utility>

namespace {
  std::map<std::string, task, std::less<>> task_table;
  std::map<uint32_t, task&>                tid_reference_table;
} // namespace

task::task(std::string_view name, uint32_t parent_tid, const std::vector<std::string_view>& args) noexcept: name(name), tid(0) {
  cap_space_cap = mm_fetch_and_create_cap_space_object();
  if (!cap_space_cap) [[unlikely]] {
    return;
  }

  root_page_table_cap = mm_fetch_and_create_page_table_object();
  if (!root_page_table_cap) [[unlikely]] {
    return;
  }

  for (int i = 0; i < get_max_page(); ++i) {
    cap_space_page_table_caps[i] = mm_fetch_and_create_page_table_object();
    if (!cap_space_page_table_caps[i]) [[unlikely]] {
      return;
    }
  }

  task_cap = mm_fetch_and_create_task_object(cap_space_cap.get(), root_page_table_cap.get(), cap_space_page_table_caps[0].get(), cap_space_page_table_caps[1].get(), cap_space_page_table_caps[2].get());
  if (!task_cap) [[unlikely]] {
    return;
  }

  tid = unwrap_sysret(sys_task_cap_tid(task_cap.get()));

  size_t argv_len = 0;
  for (const auto& arg : args) {
    argv_len += arg.size() + 1;
  }

  size_t    argv_index_len  = sizeof(char*) * args.size();
  size_t    argv_total_len  = (argv_len + sizeof(char*) - 1) / sizeof(char*) * sizeof(char*) + argv_index_len;
  uintptr_t user_space_end  = unwrap_sysret(sys_system_user_space_end());
  uintptr_t argv_index_root = user_space_end - argv_total_len;
  uintptr_t argv_root       = argv_index_root + argv_index_len;

  std::unique_ptr<char[]> stack_data      = std::make_unique<char[]>(argv_total_len);
  char*                   argv_data       = stack_data.get() + argv_index_len;
  char**                  argv_index_data = reinterpret_cast<char**>(stack_data.get());
  size_t                  pos             = 0;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string_view& arg = args[i];

    char* ptr = argv_data + pos;
    memcpy(ptr, arg.data(), arg.size());
    ptr[arg.size()] = '\0';

    argv_index_data[i] = reinterpret_cast<char*>(argv_root + pos);

    pos += arg.size() + 1;
  }

  size_t stack_commit = std::max<size_t>(4 * KILO_PAGE_SIZE, (argv_len + KILO_PAGE_SIZE - 1) / KILO_PAGE_SIZE * KILO_PAGE_SIZE);
  mm_id_cap           = mm_attach(task_cap.get(), root_page_table_cap.get(), 0, 0, stack_commit, stack_data.get(), argv_total_len);

  sys_task_cap_set_reg(task_cap.get(), REG_ARG_0, args.size());
  sys_task_cap_set_reg(task_cap.get(), REG_ARG_1, argv_index_root);

  ep_cap = mm_fetch_and_create_endpoint_object();

  if (parent_tid != 0 && task_exists(parent_tid)) {
    task& parent_task = lookup_task(parent_tid);
    this->env         = parent_task.env;
  }
}

task::task(std::string_view name, task_cap_t task_cap, endpoint_cap_t ep_cap) noexcept: task_cap(task_cap), ep_cap(ep_cap), name(name), tid(0) {
  tid = unwrap_sysret(sys_task_cap_tid(task_cap));
}

task::task(task&& other) noexcept
    : task_cap(std::move(other.task_cap)),
      cap_space_cap(std::move(other.cap_space_cap)),
      root_page_table_cap(std::move(other.root_page_table_cap)),
      mm_id_cap(std::move(other.mm_id_cap)),
      ep_cap(std::move(other.ep_cap)),
      name(std::move(other.name)),
      env(std::move(other.env)),
      tid(other.tid) {
  for (size_t i = 0; i < std::size(cap_space_page_table_caps); ++i) {
    cap_space_page_table_caps[i] = std::move(other.cap_space_page_table_caps[i]);
  }
}

task& task::operator=(task&& other) noexcept {
  if (this != &other) {
    task_cap            = std::move(other.task_cap);
    cap_space_cap       = std::move(other.cap_space_cap);
    root_page_table_cap = std::move(other.root_page_table_cap);
    mm_id_cap           = std::move(other.mm_id_cap);
    ep_cap              = std::move(other.ep_cap);
    name                = std::move(other.name);
    env                 = std::move(other.env);
    tid                 = other.tid;

    for (size_t i = 0; i < std::size(cap_space_page_table_caps); ++i) {
      cap_space_page_table_caps[i] = std::move(other.cap_space_page_table_caps[i]);
    }
  }

  return *this;
}

task::~task() noexcept {
  // TODO: Release task's resources. (e.g. files)
}

const caprese::unique_cap& task::get_task_cap() const noexcept {
  return task_cap;
}

const caprese::unique_cap& task::get_mm_id_cap() const noexcept {
  return mm_id_cap;
}

const caprese::unique_cap& task::get_ep_cap() const noexcept {
  return ep_cap;
}

const std::string& task::get_name() const noexcept {
  return name;
}

uint32_t task::get_tid() const noexcept {
  return tid;
}

bool task::set_env(std::string_view env, std::string_view value) noexcept {
  if (env.empty()) {
    return false;
  }

  this->env.emplace(env, value);

  return true;
}

bool task::get_env(std::string_view env, std::string& value) const noexcept {
  if (env.empty()) {
    return false;
  }

  if (!this->env.contains(env)) {
    return false;
  }

  value = this->env.find(env)->second;

  return true;
}

bool task::next_env(std::string_view env, std::string& value) const noexcept {
  auto iter = this->env.end();

  if (env.empty()) {
    iter = this->env.begin();
  } else {
    iter = this->env.upper_bound(env);
  }

  if (iter == this->env.end()) {
    return false;
  }

  value = iter->first;

  return true;
}

void task::set_register(uintptr_t reg, uintptr_t value) noexcept {
  sys_task_cap_set_reg(task_cap.get(), reg, value);
}

uintptr_t task::get_register(uintptr_t reg) const noexcept {
  return unwrap_sysret(sys_task_cap_get_reg(task_cap.get(), reg));
}

cap_t task::transfer_cap(cap_t cap) const noexcept {
  return unwrap_sysret(sys_task_cap_transfer_cap(task_cap.get(), cap));
}

bool task::load_program(std::reference_wrapper<std::istream> data) {
  if (!task_cap) [[unlikely]] {
    return false;
  }

  elf_loader loader(std::ref(*this), data);

  return loader.load();
}

void task::kill() const {
  sys_task_cap_switch(task_cap.get());
}

void task::switch_task() const {
  sys_task_cap_switch(task_cap.get());
}

void task::resume() const {
  sys_task_cap_resume(task_cap.get());
}

void task::suspend() const {
  sys_task_cap_suspend(task_cap.get());
}

bool create_task(std::string_view name, std::reference_wrapper<std::istream> data, int flags, uint32_t parent_tid, const std::vector<std::string_view>& args) {
  if (task_table.contains(name)) [[unlikely]] {
    return false;
  }

  if (flags & APM_CREATE_FLAG_DETACHED) [[unlikely]] {
    parent_tid = 0;
  }

  task task(name, parent_tid, args);

  if (!task.load_program(data)) {
    return false;
  }

  task_cap_t copied_task_cap = unwrap_sysret(sys_task_cap_copy(task.get_task_cap().get()));
  task_cap_t dst_task_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task.get_task_cap().get(), copied_task_cap));
  task.set_register(REG_ARG_2, dst_task_cap);

  endpoint_cap_t copied_apm_cap = unwrap_sysret(sys_endpoint_cap_copy(apm_ep_cap));
  endpoint_cap_t dst_apm_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task.get_task_cap().get(), copied_apm_cap));
  task.set_register(REG_ARG_3, dst_apm_cap);

  endpoint_cap_t copied_mm_cap = unwrap_sysret(sys_endpoint_cap_copy(__mm_ep_cap));
  endpoint_cap_t dst_mm_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task.get_task_cap().get(), copied_mm_cap));
  task.set_register(REG_ARG_4, dst_mm_cap);

  id_cap_t copied_mm_id_cap = unwrap_sysret(sys_id_cap_copy(task.get_mm_id_cap().get()));
  id_cap_t dst_mm_id_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task.get_task_cap().get(), copied_mm_id_cap));
  task.set_register(REG_ARG_5, dst_mm_id_cap);

  endpoint_cap_t copied_ep_cap = unwrap_sysret(sys_endpoint_cap_copy(task.get_ep_cap().get()));
  endpoint_cap_t dst_ep_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task.get_task_cap().get(), copied_ep_cap));
  task.set_register(REG_ARG_6, dst_ep_cap);

  auto        result   = task_table.emplace(name, std::move(task));
  class task& task_ref = result.first->second;
  tid_reference_table.emplace(task_ref.get_tid(), task_ref);

  if ((flags & APM_CREATE_FLAG_SUSPENDED) == 0) {
    task_table.find(name)->second.resume();
  }

  return true;
}

bool attach_task(std::string_view name, task_cap_t task_cap, endpoint_cap_t ep_cap) {
  if (task_table.contains(name)) [[unlikely]] {
    return false;
  }

  auto  result   = task_table.emplace(name, task(name, task_cap, ep_cap));
  task& task_ref = result.first->second;
  tid_reference_table.emplace(task_ref.get_tid(), task_ref);

  return true;
}

bool task_exists(std::string_view name) {
  return task_table.contains(name);
}

bool task_exists(uint32_t tid) {
  return tid_reference_table.contains(tid);
}

task& lookup_task(std::string_view name) {
  return task_table.find(name)->second;
}

task& lookup_task(uint32_t tid) {
  return tid_reference_table.at(tid);
}
