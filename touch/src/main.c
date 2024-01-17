#include <stdio.h>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: touch <filename>\n");
    return 1;
  }

  FILE* fp = fopen(argv[1], "w");
  if (fp == NULL) {
    printf("Error: could not open file %s\n", argv[1]);
    return 1;
  }

  fclose(fp);

  return 0;
}
