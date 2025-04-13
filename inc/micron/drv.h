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
    i32 (*ioctl)(struct drv *, u32 cmd, ...);            /* control command */
    usize (*read)(struct drv *, void *buffer, usize n);  /* read from driver */
    usize (*write)(struct drv *, void *buffer, usize n); /* write to driver */
};

enum drv_errs
{
    DE_OK = 0,
    DE_ERR = 1,  /* generic error */
    DE_NPIO = 2, /* no empty PIO hardware */
    DE_NSM = 3,  /* no empty state machine */
};

/* Find an installed driver based on the name. Returns a pointer to the drv
   struct if such a driver exists, otherwise NULL. */
struct drv *drv_find(const char *name);

#define __cmd(C1, C2, X, ...) ((C1) << 24 | (C2) << 16 | (X & 0xFFFF))

/* wspico2 display driver */

#define WSPICO2FILL   __cmd('w', '2', 1, u32 color)
#define WSPICO2ATTACH __cmd('w', '2', 2, void *buffer)
#define WSPICO2RGB565 __cmd('w', '2', 3, u32 rgb_color, u16 *rgb565_color)
#define WSPICO2SYNC   __cmd('w', '2', 4)

#endif /* MICRON_DRV_H */
