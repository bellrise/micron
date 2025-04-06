/* console.c
   Copyright (c) 2024 bellrise */

#include <micron/mem.h>
#include <micron/micron.h>
#include <micron/syslog.h>

void user_main()
{
    void *p, *q, *r;
    syslog("Hello from userland");

    p = page_alloc(10, PF_USER);
    q = page_alloc(6, PF_USER);
    r = page_alloc(16, PF_ALLOC);

    page_free(q);

    q = page_alloc(1, PF_USER);
    p = page_alloc(1, PF_USER);
    q = page_alloc(1, PF_USER);

    page_free(p);

    mem_info();
}
