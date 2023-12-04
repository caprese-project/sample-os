#include <assert.h>
#include <crt/global.h>
#include <crt/heap.h>
#include <internal/attribute.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct __heap_block_header {
  struct __heap_block_header* next;
  size_t                      size;
} __heap_block_header_t;

__heap_block_header_t* __heap_block_free_list;

static void __heap_split(__heap_block_header_t* header, size_t size) {
  if (header->size - size >= sizeof(__heap_block_header_t) + alignof(max_align_t)) {
    __heap_block_header_t* new_header = (__heap_block_header_t*)((uintptr_t)(header + 1) + size);
    new_header->next                  = header->next;
    new_header->size                  = header->size - size - sizeof(__heap_block_header_t);
    header->next                      = new_header;
  }
}

__weak void __heap_init() {
  __heap_block_free_list       = (__heap_block_header_t*)__brk_start;
  __heap_block_free_list->next = __heap_block_free_list;
  __heap_block_free_list->size = (__brk_pos - __brk_start) - sizeof(__heap_block_header_t);
}

__weak void* __heap_alloc(size_t size) {
  __if_unlikely (size == 0) {
    return NULL;
  }

  size = (size + alignof(max_align_t) - 1) / alignof(max_align_t) * alignof(max_align_t);

  __heap_block_header_t* header = __heap_block_free_list;
  __heap_block_header_t* prev   = NULL;
  do {
    if ((header->size & 1) == 0 && header->size >= size) {
      __heap_split(header, size);
      header->size = size | 1;
      return header + 1;
    }
    prev   = header;
    header = header->next;
  } while (header != __heap_block_free_list);

  __heap_block_header_t* new_header = __heap_sbrk();
  new_header->next                  = __heap_block_free_list;
  new_header->size                  = MEGA_PAGE_SIZE - sizeof(__heap_block_header_t);
  prev->next                        = new_header;

  __heap_split(new_header, size);
  new_header->size |= 1;

  return new_header + 1;
}

__weak void __heap_free(void* ptr) {
  __if_unlikely (ptr == NULL) {
    abort();
  }

  __heap_block_header_t* header = (__heap_block_header_t*)ptr - 1;
  if ((header->size & 1) == 0) {
    abort();
  }

  header->size &= ~(size_t)1;
}

__weak void* __heap_sbrk() {
  return (void*)mm_sbrk(__mm_ep_cap, __mm_id_cap, MEGA_PAGE_SIZE);
}
