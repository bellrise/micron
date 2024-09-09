/* mem.h - memory control
   Copyright (c) 2024 bellrise */

#ifndef MICRON_MEM_H
#define MICRON_MEM_H 1

#include <micron/micron.h>

struct meminfo
{
    u8 *heap_start;
    u8 *heap_end;
    uptr heap_size;
};

i32 heap_init();
i32 heap_close();
usize malloc_heap_free();

#endif /* MICRON_MEM_H */
