#include <cstdlib>
#include <iostream>
#include <libcaprese/syscall.h>
#include <service/apm.h>
#include <service/mm.h>
#include <string>

endpoint_cap_t kill_notify_ep_cap;

void disp_terminal() {
  std::cout << "$> " << std::flush;
}

void do_command(const std::string& command) {
  task_cap_t task = apm_create(("/init/" + command).c_str(), nullptr, APM_CREATE_FLAG_DEFAULT);
  if (task == 0) [[unlikely]] {
    std::cout << "Failed to create " << command << std::endl;
  } else {
    message_t msg;
    sys_task_cap_set_kill_notify(task, kill_notify_ep_cap);
    sys_endpoint_cap_receive(kill_notify_ep_cap, &msg);
    sys_cap_destroy(task);
  }
}

void read_command() {
  std::string line;
  std::getline(std::cin, line);

  if (line.empty()) [[unlikely]] {
    return;
  }

  std::string command = line.substr(0, line.find(' '));

  if (command == "echo") {
    std::cout << line.substr(5) << std::endl;
  } else if (command == "clear") {
    do_command(command);
  } else if (command == "exit") {
    exit(0);
  } else {
    std::cout << "Unknown command: " << command << std::endl;
  }
}

int main() {
  kill_notify_ep_cap = mm_fetch_and_create_endpoint_object();
  if (kill_notify_ep_cap == 0) [[unlikely]] {
    abort();
  }

  while (true) {
    disp_terminal();
    read_command();
  }
}
