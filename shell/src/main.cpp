#include <iostream>
#include <string>

void disp_terminal() {
  std::cout << "$> " << std::flush;
}

void read_command() {
  std::string command;
  std::getline(std::cin, command);
  std::cout << "Command: " << command << std::endl;
}

int main() {
  while (true) {
    disp_terminal();
    read_command();
  }
}
