#include <cstdlib>
#include <iostream>
#include <libcaprese/syscall.h>
#include <map>
#include <memory>
#include <service/apm.h>
#include <service/fs.h>
#include <service/mm.h>
#include <sstream>
#include <string>
#include <vector>

endpoint_cap_t                                                   kill_notify_ep_cap;
std::vector<std::string>                                         path_env;
std::map<std::string, void (*)(const std::vector<std::string>&)> builtin_commands;

[[noreturn]] void do_exit(const std::vector<std::string>& arguments) {
  int status = 0;
  if (!arguments.empty()) {
    status = std::stoi(arguments[0]);
  }
  exit(status);
}

void do_setenv(const std::vector<std::string>& arguments) {
  if (arguments.size() != 2) [[unlikely]] {
    std::cout << "Usage: setenv <name> <value>" << std::endl;
    return;
  }

  setenv(arguments[0].c_str(), arguments[1].c_str(), true);
}

void do_cd(const std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    setenv("PWD", "/", true);
  } else if (arguments.size() == 1) {
    setenv("PWD", arguments[0].c_str(), true);
  } else {
    std::cout << "Usage: cd [<path>]" << std::endl;
  }
}

void init() {
  setenv("PATH", "/init:/bin", false);
  setenv("PWD", "/", false);

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

  builtin_commands["exit"]   = do_exit;
  builtin_commands["setenv"] = do_setenv;
  builtin_commands["cd"]     = do_cd;
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

std::string find_program(const std::string& command) {
  for (const auto& path : path_env) {
    std::string full_path = path_join(path, command);
    if (fs_exists(full_path.c_str())) {
      return full_path;
    }
  }
  return "";
}

void do_command(const std::string& command, const std::vector<std::string>& arguments) {
  if (builtin_commands.contains(command)) {
    builtin_commands[command](arguments);
    return;
  }

  std::unique_ptr<const char*[]> argv = std::make_unique<const char*[]>(arguments.size() + 2);
  for (size_t i = 0; i < arguments.size(); ++i) {
    argv[i + 1] = arguments[i].c_str();
  }
  argv[arguments.size() + 1] = nullptr;

  std::string program = find_program(command);
  if (program.empty()) {
    std::cout << "Command not found: " << command << std::endl;
    return;
  }

  argv[0] = program.c_str();

  task_cap_t task = apm_create(program.c_str(), nullptr, APM_CREATE_FLAG_DEFAULT, argv.get());
  if (task == 0) {
    std::cout << "Failed to create program: " << command << std::endl;
    return;
  }

  message_t msg;
  sys_task_cap_set_kill_notify(task, kill_notify_ep_cap);
  sys_endpoint_cap_receive(kill_notify_ep_cap, &msg);
  sys_cap_destroy(task);
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

    std::stringstream stream(line);

    std::string              command;
    std::vector<std::string> arguments;

    stream >> command;

    while (true) {
      std::string argument;
      stream >> argument;
      if (argument.empty()) [[unlikely]] {
        break;
      }
      arguments.push_back(std::move(argument));
    }

    do_command(command, arguments);
  }
}
