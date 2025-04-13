/* wspico2.c
   Copyright (c) 2025 bellrise */

#include <micron/drv.h>
#include <micron/mem.h>
#include <micron/micron.h>
#include <micron/syslog.h>
#include <pico/printf.h>
#include <pico/time.h>
#include <stdlib.h>

u16 color_convert(struct drv *display, u32 rgb)
{
    u16 res;
    display->ioctl(display, WSPICO2RGB565, rgb, &res);
    return res;
}
#define TIMER_START(NAME) uint64_t __timer##NAME = time_us_64()
#define TIMER_STOP(NAME)                                                       \
    syslog_impl("timer[" #NAME "].c", "\n", "took %.2f ms",                    \
                (float) (time_us_64() - __timer##NAME) / 1000)

void user_main()
{
    struct drv *display;
    u16 *pixels;
    u16 color;

    display = drv_find("wspico2");
    display->init(display);

    /* For the pico2 display we need exactly 150 pages (320x240x2). */

    pixels = page_alloc(150, 0);

    display->ioctl(display, WSPICO2FILL, 0);
    display->ioctl(display, WSPICO2ATTACH, pixels);
    display->ioctl(display, WSPICO2SYNC);

    for (i32 j = 0; j < 50; j++) {
        color = color_convert(display, j % 2 == 0 ? 0xffffff : 0);
        for (i32 i = 0; i < 320 * 240; i++)
            pixels[i] = color;

        display->ioctl(display, WSPICO2SYNC);
    }
}
