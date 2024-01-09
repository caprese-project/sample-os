#ifndef APM_TASK_MANAGER_H_
#define APM_TASK_MANAGER_H_

#include <cstdint>
#include <functional>
#include <istream>
#include <libcaprese/cap.h>
#include <libcaprese/cxx/raii.h>
#include <map>
#include <string>
#include <vector>

class task {
  caprese::unique_cap                task_cap;
  caprese::unique_cap                cap_space_cap;
  caprese::unique_cap                root_page_table_cap;
  caprese::unique_cap                cap_space_page_table_caps[3];
  caprese::unique_cap                mm_id_cap;
  caprese::unique_cap                ep_cap;
  std::string                        name;
  std::map<std::string, std::string> env;
  uint32_t                           tid;

public:
  task(std::string name, uint32_t parent_tid, const std::vector<std::string>& args) noexcept;
  task(std::string name, task_cap_t task_cap, endpoint_cap_t ep_cap) noexcept;

  task(const task&)            = delete;
  task& operator=(const task&) = delete;

  task(task&& other) noexcept;
  task& operator=(task&& other) noexcept;

  ~task() noexcept;

  const caprese::unique_cap& get_task_cap() const noexcept;
  const caprese::unique_cap& get_mm_id_cap() const noexcept;
  const caprese::unique_cap& get_ep_cap() const noexcept;
  const std::string&         get_name() const noexcept;
  uint32_t                   get_tid() const noexcept;

  bool set_env(const std::string& env, std::string value) noexcept;
  bool get_env(const std::string& env, std::string& value) const noexcept;

  void      set_register(uintptr_t reg, uintptr_t value) noexcept;
  uintptr_t get_register(uintptr_t reg) const noexcept;

  cap_t transfer_cap(cap_t cap) const noexcept;

  bool load_program(std::reference_wrapper<std::istream> data);

  void kill() const;
  void switch_task() const;
  void resume() const;
  void suspend() const;
};

bool  create_task(std::string name, std::reference_wrapper<std::istream> data, int flags, uint32_t parent_tid, const std::vector<std::string>& args);
bool  attach_task(std::string name, task_cap_t task_cap, endpoint_cap_t ep_cap);
bool  task_exists(const std::string& name);
bool  task_exists(uint32_t tid);
task& lookup_task(const std::string& name);
task& lookup_task(uint32_t tid);

#endif // APM_TASK_MANAGER_H_
