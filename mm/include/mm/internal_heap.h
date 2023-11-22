#ifndef MM_INTERNAL_HEAP_H_
#define MM_INTERNAL_HEAP_H_

#include <stddef.h>
#include <stdint.h>

extern size_t internal_heap_page_count;

void init_first_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end);
void extend_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end);

#endif // MM_INTERNAL_HEAP_H_
