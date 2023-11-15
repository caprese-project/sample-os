#include <stdbool.h>

int main() {
  while(true) {
    asm volatile("wfi");
  }

  return 0;
}
