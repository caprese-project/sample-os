#include <crt/global.h>
#include <internal/branch.h>
#include <service/apm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int printenv(const char* env) {
  const char* value = getenv(env);

  __if_unlikely (value == NULL) {
    return 1;
  }

  printf("%s=%s\n", env, value);

  return 0;
}

char buf[1024];

int main(int argc, char* argv[]) {
  if (argc >= 2) {
    return printenv(argv[1]);
  } else {
    const char* env = "";
    while (true) {
      size_t value_size = sizeof(buf);
      if (!apm_nextenv(__this_task_cap, env, buf, &value_size)) {
        return 1;
      }

      if (strnlen(buf, value_size) == 0) {
        break;
      }

      env = buf;
      printenv(env);
    }
  }
  return 0;
}
