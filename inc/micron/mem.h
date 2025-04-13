/* mem.h - memory control
   Copyright (c) 2024-2025 bellrise */

#ifndef MICRON_MEM_H
#define MICRON_MEM_H 1

#include <micron/micron.h>

#define PAGE_SIZE      1024
#define PAGE_SIZE_BITS 10
#define PAGE_SIZE_MASK 0x3FF

#define PF_ALLOC (1 << 0)
#define PF_USER  (1 << 1)
#define PF_START (1 << 7)

struct meminfo
{
    u8 *heap_start;
    u8 *heap_end;
    uptr heap_size;
    u32 n_pages;
    u8 *pagemap;
    u8 *pages_start;
};

i32 _mem_init();
i32 _mem_close();

void mem_info();
usize malloc_heap_free_left();

void *page_alloc(u32 pages, u8 flags);
i32 page_free(void *addr);

#endif /* MICRON_MEM_H */
