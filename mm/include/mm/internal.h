#ifndef MM_INTERNAL_H_
#define MM_INTERNAL_H_

#include <libcaprese/cap.h>
#include <stddef.h>

extern cap_t  root_page_table_cap;
extern cap_t  inter_page_table_cap;
extern cap_t* mem_caps;
extern size_t num_mem_caps;

#endif // MM_INTERNAL_H_
