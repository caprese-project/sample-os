#include <stdio.h>

int main(void) {
  printf("\e[2J\e[1;1H");
  fflush(stdout);
  return 0;
}
