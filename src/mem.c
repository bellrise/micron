/* mem.c - memory control
   Copyright (c) 2024 bellrise */

#include <micron/buildconfig.h>
#include <micron/errno.h>
#include <micron/mem.h>
#include <micron/syslog.h>
#include <pico/printf.h>
#include <string.h>

struct meminfo __micron_meminfo;
extern void *_sbrk(i32 incr);
extern u8 __StackLimit;
extern u8 __bss_end__;

i32 mem_init()
{
    struct meminfo *info;
    u32 total_pages;
    u32 free_pages;
    u32 meta_pages;
    uptr ptr;

    /* Allocate some memory for the system. First, we need to align the SBRK to
       the closest page size. */

    info = &__micron_meminfo;

    ptr = (uptr) _sbrk(0);
    if (ptr & PAGE_SIZE_MASK) {
        _sbrk(PAGE_SIZE - (ptr & PAGE_SIZE_MASK));
    }

    info->heap_size = MICRON_CONFIG_MEM_HEAP * 1024;
    info->heap_start = _sbrk(info->heap_size);
    info->heap_end = _sbrk(0);

    /* Reserve enough bytes to store the whole page map (1B per page),
       and an additional page for padding. The rest is usable. */

    total_pages = info->heap_size >> PAGE_SIZE_BITS;
    meta_pages = (total_pages >> 10) + 2;
    free_pages = total_pages - meta_pages;

    info->pagemap = info->heap_start;
    info->n_pages = free_pages;
    info->pages_start = info->heap_start + (meta_pages << PAGE_SIZE_BITS);

    memset(info->heap_start, 0, info->n_pages);

    return 0;
}

static void dump_pagemap()
{
    struct meminfo *info;
    u32 left_to_show;
    u32 to_show;
    uptr index;
    uptr offset;
    const char *m;

    info = &__micron_meminfo;
    offset = (uptr) info->pages_start;
    index = 0;
    left_to_show = info->n_pages;

    syslog("Page map (line is %zu kB), %u+%u pages:",
           (32 * PAGE_SIZE) >> PAGE_SIZE_BITS, info->n_pages,
           MICRON_CONFIG_MEM_HEAP - info->n_pages);

    while (left_to_show > 0) {
        syslog_impl(__FILE__, "", "  %08p  ", offset);
        to_show = imin(left_to_show, 32);

        for (u32 i = 0; i < to_show; i++) {
            m = "m-";
            if (info->pagemap[index + i] & PF_ALLOC)
                m = "33mA";
            if (info->pagemap[index + i] & PF_USER)
                m = "34mU";
            printf("\033[%s%s\033[m",
                   info->pagemap[index + i] & PF_START ? "1;" : "", m);
        }

        index += to_show;
        offset += PAGE_SIZE * to_show;
        left_to_show -= to_show;

        printf("\n");
    }
}

static void print_range(void *a, void *b, const char *name)
{
    syslog("  %08p - %08p % 8.2f kB %s", (uptr) a, (uptr) b,
           (float) (b - a) / 1024, name);
}

void mem_info()
{
    struct meminfo *info;

    info = &__micron_meminfo;

    syslog("System heap %zu kB", info->heap_size >> 10, info->heap_start,
           info->heap_end);
    syslog("Malloc heap %zu kB",
           ((uptr) &__StackLimit - (uptr) &__bss_end__ - info->heap_size)
               >> 10);

    syslog("Memory map:");
    print_range(info->pagemap, info->heap_start + info->n_pages,
                "\033[33mpagemap");
    print_range(info->heap_start + info->n_pages + 1, info->pages_start - 1,
                "\033[31mreserved");
    print_range(info->pages_start,
                info->pages_start + (info->n_pages << PAGE_SIZE_BITS) - 1,
                "\033[1;32musable");
    print_range(info->heap_end, &__StackLimit, "malloc-heap");
    dump_pagemap();
}

i32 mem_close()
{
    return 0;
}

usize malloc_heap_free_left()
{
    return (usize) &__StackLimit - (usize) _sbrk(0);
}

static bool is_empty_range(u32 offset, u32 size)
{
    for (u32 i = 0; i < size; i++) {
        if (offset + i >= __micron_meminfo.n_pages)
            return false;
        if (__micron_meminfo.pagemap[offset + i])
            return false;
    }

    return true;
}

static void mark_pages(u32 offset, u32 size, u8 flag)
{
    for (u32 i = 0; i < size; i++) {
        if (offset + i >= __micron_meminfo.n_pages)
            break;
        __micron_meminfo.pagemap[offset + i] = flag;
    }
}

void *page_alloc(u32 pages, u8 flags)
{
    u32 walker;

    /* page_alloc always returns a contiguous array of memory on success, so
       we need to find a fitting free area. The algorithm in use is first-fit,
       which means the first big-enough block gets used. This is not helpful
       for fragmentation - this algorithm always can be re-written. */

    walker = 0;

    while (walker < __micron_meminfo.n_pages) {
        if (is_empty_range(walker, pages)) {
            mark_pages(walker, pages, flags | PF_ALLOC);
            __micron_meminfo.pagemap[walker] |= PF_START;
            return __micron_meminfo.pages_start + (walker << PAGE_SIZE_BITS);
        }

        walker++;
    }

    return NULL;
}

i32 page_free(void *addr)
{
    uptr page_index;
    uptr offset;
    uptr walker;

    if ((uptr) addr < (uptr) __micron_meminfo.pages_start)
        panic("invalid pointer, addr out of range");

    offset = (uptr) addr - (uptr) __micron_meminfo.pages_start;
    page_index = offset >> PAGE_SIZE_BITS;
    walker = page_index + 1;

    if (!(__micron_meminfo.pagemap[page_index] & PF_START))
        return EINVAL;

    /* Walk from the PF_START page until another PF_START or empty page
       is found, marking them as 'free' (with a zero flag). */

    __micron_meminfo.pagemap[page_index] = 0;

    while (walker <= __micron_meminfo.n_pages) {
        if (!__micron_meminfo.pagemap[walker]
            || __micron_meminfo.pagemap[walker] & PF_START)
            break;
        __micron_meminfo.pagemap[walker++] = 0;
    }

    return 0;
}
