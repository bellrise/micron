/* drv.h - driver structure
   Copyright (c) 2024 bellrise */

#ifndef MICRON_DRV_H
#define MICRON_DRV_H 1

#include <micron/micron.h>

struct drv
{
    void *_data; /* local driver data */

    const char *name;               /* driver name */
    const char *desc;               /* driver description */
    i32 (*init)(struct drv *, ...); /* initialize driver with a set of params */
    usize (*read)(struct drv *, void *buffer, usize n);  /* read from driver */
    usize (*write)(struct drv *, void *buffer, usize n); /* write to driver */
};

enum drv_errs
{
    DVE_OK = 0,
    DVE_ERR = 1,  /* generic error */
    DVE_NPIO = 2, /* no empty PIO hardware */
    DVE_NSM = 3,  /* no empty state machine */
};

/* Find an installed driver based on the name. Returns a pointer to the drv
   struct if such a driver exists, otherwise NULL. */
struct drv *drv_find(const char *name);

#endif /* MICRON_DRV_H */
