/* mem.c - memory control
   Copyright (c) 2024 bellrise */

#include <micron/buildconfig.h>
#include <micron/mem.h>
#include <micron/syslog.h>

struct meminfo __micron_meminfo;
extern void *_sbrk(int incr);
extern u8 __StackLimit;
extern u8 __bss_end__;

i32 heap_init()
{

    /* Allocate some memory for our own uses. */

    __micron_meminfo.heap_size = MICRON_CONFIG_MEM_HEAP * 1024;
    __micron_meminfo.heap_start = _sbrk(__micron_meminfo.heap_size);
    __micron_meminfo.heap_end = _sbrk(0);

    syslog("System heap %zu kB", __micron_meminfo.heap_size >> 10);
    syslog("Malloc heap %zu kB", ((uptr) &__StackLimit - (uptr) &__bss_end__
                                  - __micron_meminfo.heap_size)
                                     >> 10);

    return 0;
}

i32 heap_close()
{
    return 0;
}

usize malloc_heap_free()
{
    return (usize) &__StackLimit - (usize) _sbrk(0);
}
