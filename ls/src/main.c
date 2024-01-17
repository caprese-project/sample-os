#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
  DIR*           dir;
  struct dirent* ent;
  const char*    dirname;

  if (argc == 1) {
    dirname = getenv("PWD");
  } else {
    dirname = argv[1];
  }

  dir = opendir(dirname);
  if (dir == NULL) {
    fprintf(stderr, "failed to opendir %s\n", dirname);
    return 1;
  }

  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.') {
      continue;
    }

    printf("%s\t", ent->d_name);
  }
  printf("\n");

  closedir(dir);
  return 0;
}
