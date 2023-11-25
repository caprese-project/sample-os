#ifndef MM_INTERNAL_HEAP_H_
#define MM_INTERNAL_HEAP_H_

#include <stddef.h>
#include <stdint.h>

extern uintptr_t        internal_heap_root;
extern size_t           internal_heap_page_count;
extern page_table_cap_t root_page_table_cap;
extern page_table_cap_t inter_page_table_cap;
extern mem_cap_t*       mem_caps;
extern size_t           num_mem_caps;

void init_first_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end, uintptr_t heap_root);
void extend_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end);

#endif // MM_INTERNAL_HEAP_H_
