/* drv.c - driver controller
   Copyright (c) 2024 bellrise */

#include "micron_drvlist.h"

#include <micron/drv.h>
#include <string.h>

struct drv *drv_find(const char *name)
{
    for (u32 i = 0; i < n_drv_decls; i++) {
        if (!strcmp(drv_decls[i]->name, name))
            return (struct drv *) drv_decls[i];
    }

    return NULL;
}
