#include <apm/elf_loader.h>
#include <apm/ipc.h>
#include <apm/server.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <utility>

namespace {
  task_manager task_manager_instance;
} // namespace

task::task(std::string name) noexcept: name(std::move(name)), tid(0) {
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

  mm_id_cap = mm_attach(task_cap.get(), root_page_table_cap.get(), 0, 0, 4 * KILO_PAGE_SIZE);

  ep_cap = mm_fetch_and_create_endpoint_object();
}

task::task(std::string name, task_cap_t task_cap, endpoint_cap_t ep_cap) noexcept: task_cap(task_cap), ep_cap(ep_cap), name(std::move(name)), tid(0) {
  tid = unwrap_sysret(sys_task_cap_tid(task_cap));
}

task::task(task&& other) noexcept
    : task_cap(std::move(other.task_cap)),
      cap_space_cap(std::move(other.cap_space_cap)),
      root_page_table_cap(std::move(other.root_page_table_cap)),
      mm_id_cap(std::move(other.mm_id_cap)),
      ep_cap(std::move(other.ep_cap)),
      name(std::move(other.name)),
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

bool task_manager::create(std::string name, std::reference_wrapper<std::istream> data, int flags) {
  if (tasks.contains(name)) [[unlikely]] {
    return false;
  }

  task task(name);

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

  tasks.emplace(name, std::move(task));

  if ((flags & APM_CREATE_FLAG_SUSPENDED) == 0) {
    tasks.at(name).resume();
  }

  return true;
}

bool task_manager::attach(std::string name, task_cap_t task_cap, endpoint_cap_t ep_cap) {
  if (tasks.contains(name)) [[unlikely]] {
    return false;
  }

  tasks.emplace(std::move(name), task(std::move(name), task_cap, ep_cap));

  return true;
}

bool task_manager::exists(const std::string& name) const {
  return tasks.contains(name);
}

task& task_manager::lookup(const std::string& name) {
  return tasks.at(name);
}

const task& task_manager::lookup(const std::string& name) const {
  return tasks.at(name);
}

bool create_task(std::string name, std::reference_wrapper<std::istream> data, int flags) {
  return task_manager_instance.create(std::move(name), data, flags);
}

bool attach_task(std::string name, task_cap_t task_cap, endpoint_cap_t ep_cap) {
  return task_manager_instance.attach(std::move(name), task_cap, ep_cap);
}

bool task_exists(const std::string& name) {
  return task_manager_instance.exists(name);
}

task& lookup_task(const std::string& name) {
  return task_manager_instance.lookup(name);
}
