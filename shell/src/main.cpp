#include <cstdlib>
#include <iostream>
#include <string>

void disp_terminal() {
  std::cout << "$> " << std::flush;
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
    std::cout << "\e[2J\e[1;1H" << std::flush;
  } else if (command == "exit") {
    exit(0);
  } else {
    std::cout << "Unknown command: " << command << std::endl;
  }
}

int main() {
  while (true) {
    disp_terminal();
    read_command();
  }
}
