#include <cstdlib>
#include <iostream>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/apm.h>
#include <service/mm.h>
#include <string>
#include <vector>

endpoint_cap_t           kill_notify_ep_cap;
std::vector<std::string> path_env;

void init() {
  setenv("PATH", "/init:/bin", false);

  std::string paths = getenv("PATH");
  size_t      pos   = 0;
  while (true) {
    size_t next_pos = paths.find(':', pos);
    if (next_pos == std::string::npos) [[unlikely]] {
      path_env.push_back(paths.substr(pos));
      break;
    }
    path_env.push_back(paths.substr(pos, next_pos - pos));
    pos = next_pos + 1;
  }
}

std::string path_join(const std::string& path, const std::string& name) {
  if (path.empty()) [[unlikely]] {
    return name;
  }

  if (path.back() == '/') [[unlikely]] {
    return path + name;
  }

  return path + "/" + name;
}

void disp_terminal() {
  std::cout << "$> " << std::flush;
}

[[noreturn]] void do_exit(const std::vector<std::string>& arguments) {
  int status = 0;
  if (!arguments.empty()) {
    status = std::stoi(arguments[0]);
  }
  exit(status);
}

void do_command(const std::string& command, const std::vector<std::string>& arguments) {
  if (command == "exit") {
    do_exit(arguments);
  }

  std::unique_ptr<const char*[]> argv = std::make_unique<const char*[]>(arguments.size() + 2);
  for (size_t i = 0; i < arguments.size(); ++i) {
    argv[i + 1] = arguments[i].c_str();
  }
  argv[arguments.size() + 1] = nullptr;

  for (const auto& path : path_env) {
    std::string full_path = path_join(path, command);
    argv[0]               = full_path.c_str();
    task_cap_t task       = apm_create(full_path.c_str(), nullptr, APM_CREATE_FLAG_DEFAULT, argv.get());

    if (task == 0) {
      continue;
    }

    message_t msg;
    sys_task_cap_set_kill_notify(task, kill_notify_ep_cap);
    sys_endpoint_cap_receive(kill_notify_ep_cap, &msg);
    sys_cap_destroy(task);

    return;
  }

  std::cout << "Failed to create " << command << std::endl;
}

int main() {
  kill_notify_ep_cap = mm_fetch_and_create_endpoint_object();
  if (kill_notify_ep_cap == 0) [[unlikely]] {
    abort();
  }

  init();

  while (true) {
    disp_terminal();

    std::string line;
    std::getline(std::cin, line);

    if (line.empty()) [[unlikely]] {
      continue;
    }

    std::string              command;
    std::vector<std::string> arguments;

    size_t pos = line.find(' ');
    if (pos != line.npos) {
      command = line.substr(0, pos);
      while (true) {
        size_t next_pos = line.find(' ', pos + 1);
        if (next_pos == line.npos) [[unlikely]] {
          arguments.push_back(line.substr(pos + 1));
          break;
        }
        arguments.push_back(line.substr(pos + 1, next_pos - pos - 1));
        pos = next_pos;
      }
    } else {
      command = line;
    }

    do_command(command, arguments);
  }
}
