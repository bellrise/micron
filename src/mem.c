/* mem.c - memory control
   Copyright (c) 2024 bellrise */

#include <micron/buildconfig.h>
#include <micron/mem.h>
#include <micron/syslog.h>

i32 heap_init(struct meminfo *mem)
{
    extern void *_sbrk(int incr);
    extern u8 __StackLimit;
    extern u8 __bss_end__;

    /* Allocate some memory for our own uses. */

    mem->heap_size = MICRON_CONFIG_MEM_HEAP * 1024;
    mem->heap_start = _sbrk(mem->heap_size);
    mem->heap_end = _sbrk(0);

    syslog("System heap %zu kB", mem->heap_size >> 10);
    syslog("Malloc heap %zu kB",
           ((uptr) &__StackLimit - (uptr) &__bss_end__ - mem->heap_size) >> 10);

    return 0;
}

i32 heap_close(struct meminfo *__unused mem)
{
    return 0;
}
