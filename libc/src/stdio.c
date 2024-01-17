#include <crt/global.h>
#include <internal/branch.h>
#include <service/fs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t __dummy_read(void*, size_t, size_t, FILE*) {
  return 0;
}

static size_t __dummy_write(const void*, size_t, size_t, FILE*) {
  return 0;
}

static int __dummy_ungetc(int, FILE*) {
  return EOF;
}

FILE* __stdin() {
  static FILE fstdin = {
    .__fd         = 0,
    .__mode       = _IONBF,
    .__ungetc_buf = (char)EOF,
    .__buf        = NULL,
    .__buf_size   = 0,
    .__buf_pos    = 0,
    .__read       = __dummy_read,
    .__write      = __dummy_write,
    .__ungetc     = __dummy_ungetc,
  };
  return &fstdin;
}

FILE* __stdout() {
  static FILE fstdout = {
    .__fd         = 0,
    .__mode       = _IONBF,
    .__ungetc_buf = (char)EOF,
    .__buf        = NULL,
    .__buf_size   = 0,
    .__buf_pos    = 0,
    .__read       = __dummy_read,
    .__write      = __dummy_write,
    .__ungetc     = __dummy_ungetc,
  };
  return &fstdout;
}

FILE* __stderr() {
  static FILE fstderr = {
    .__fd         = 0,
    .__mode       = _IONBF,
    .__ungetc_buf = (char)EOF,
    .__buf        = NULL,
    .__buf_size   = 0,
    .__buf_pos    = 0,
    .__read       = __dummy_read,
    .__write      = __dummy_write,
    .__ungetc     = __dummy_ungetc,
  };
  return &fstderr;
}

static size_t __read(void* __restrict ptr, size_t size, size_t nmemb, struct __FILE* __restrict stream) {
  __if_unlikely (__fs_ep_cap == 0) {
    return 0;
  }

  __if_unlikely (stream->__fd == 0) {
    return 0;
  }

  ssize_t n = fs_read(stream->__fd, ptr, size * nmemb);
  if (n < 0) {
    return 0;
  }

  return n;
}

static size_t __write(const void* __restrict ptr, size_t size, size_t nmemb, struct __FILE* __restrict stream) {
  __if_unlikely (__fs_ep_cap == 0) {
    return 0;
  }

  __if_unlikely (stream->__fd == 0) {
    return 0;
  }

  ssize_t n = fs_write(stream->__fd, ptr, size * nmemb);
  if (n < 0) {
    return 0;
  }

  return n;
}

static int __ungetc(int ch, struct __FILE*) {
  // TODO: impl
  return ch;
}

int __finitialize(const char* __restrict filename, int, FILE* __restrict stream) {
  id_cap_t fd = 0;

  if (filename[0] == '/') {
    fd = fs_open(filename);
  } else {
    const char* cwd      = getenv("PWD");
    size_t      cwd_len  = strlen(cwd);
    char*       abs_path = malloc(strlen(cwd) + 1 + strlen(filename) + 1);

    if (abs_path == NULL) {
      return EOF;
    }

    strcpy(abs_path, cwd);

    if (cwd[cwd_len - 1] != '/') {
      strcat(abs_path, "/");
    }

    strcat(abs_path, filename);

    fd = fs_open(abs_path);

    free(abs_path);
  }

  __if_unlikely (fd == 0) {
    return EOF;
  }

  stream->__fd         = (uintptr_t)fd;
  stream->__mode       = stream->__mode | _IONBF;
  stream->__ungetc_buf = EOF;
  stream->__buf        = NULL;
  stream->__buf_size   = 0;
  stream->__buf_pos    = 0;
  stream->__read       = __read;
  stream->__write      = __write;
  stream->__ungetc     = __ungetc;

  return 0;
}

int __ffinalize(FILE*) {
  return 0;
}
