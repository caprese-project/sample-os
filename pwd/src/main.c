#include <stdio.h>
#include <stdlib.h>

int main() {
  const char* pwd = getenv("PWD");
  if (pwd == NULL) {
    return 1;
  }
  puts(pwd);
  return 0;
}
