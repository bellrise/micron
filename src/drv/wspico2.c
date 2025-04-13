/* wspico2.c - Waveshare Pico LCD 2
   Copyright (c) 2025 bellrise */

#include "micron/micron.h"

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <micron/drv.h>
#include <micron/errno.h>
#include <micron/syslog.h>
#include <pico/time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define DRV_NAME "wspico2"

#define WSPICO2_PIN_DC   8  /* data/command control */
#define WSPICO2_PIN_CS   9  /* chip select */
#define WSPICO2_PIN_CLK  10 /* clock */
#define WSPICO2_PIN_MOSI 11 /* read end */
#define WSPICO2_PIN_RST  12 /* reset signal */
#define WSPICO2_PIN_BL   13

/* https://newhavendisplay.com/content/datasheets/ST7789V.pdf */

#define ST7789V_CASET     0x2a /* column address set */
#define ST7789V_COLMOD    0x3a /* interface pixel format */
#define ST7789V_DISPOFF   0x28 /* display off */
#define ST7789V_DISPON    0x29 /* display on */
#define ST7789V_FRCTR2    0xc6 /* frame rate control */
#define ST7789V_GCTRL     0xb7 /* gate control */
#define ST7789V_INVON     0x21 /* display inversion on */
#define ST7789V_LCMCTRL   0xc0 /* LCM control */
#define ST7789V_MADCTL    0x36 /* memory data access control */
#define ST7789V_NVGAMCTRL 0xe1 /* negative voltage gate control */
#define ST7789V_PORCTRL   0xb2 /* porch setting */
#define ST7789V_PVGAMCTRL 0xe0 /* positive voltage gate control */
#define ST7789V_PWCTRL1   0xd0 /* power control 1 */
#define ST7789V_RAMWR     0x2c /* memory write */
#define ST7789V_RASET     0x2b /* row address set */
#define ST7789V_SLPOUT    0x11 /* sleep out */
#define ST7789V_VCOMS     0xbb /* VCOM setting */
#define ST7789V_VDVSET    0xc4 /* VDV setting */
#define ST7789V_VDVVRHEN  0xc2 /* VDV&VHR enable */
#define ST7789V_VRHS      0xc3 /* VHR setting */

struct wspico2
{
    i16 w;
    i16 h;
    i32 pwm_slice;
    u8 *buffer;
};

#define DATA ((struct wspico2 *) self->_data)

static usize wspico2_write(struct drv *self, void *buffer, usize n);
static usize wspico2_read(struct drv *self, void *buffer, usize n);

static void wspico2_cmd(u8 byte)
{
    gpio_put(WSPICO2_PIN_CS, 0);
    gpio_put(WSPICO2_PIN_DC, 0);
    spi_write_blocking(spi1, &byte, 1);
}

static void wspico2_put(u8 byte)
{
    gpio_put(WSPICO2_PIN_CS, 0);
    gpio_put(WSPICO2_PIN_DC, 1);
    spi_write_blocking(spi1, &byte, 1);
    gpio_put(WSPICO2_PIN_CS, 1);
}

static void wspico2_driver_configure(struct wspico2 *display)
{
    i32 slice;

    /* Initialize SPI, setting some ports to use it instead of the I2C. */

    spi_init(spi1, 64 * 1000 * 1000);
    gpio_set_function(WSPICO2_PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(WSPICO2_PIN_MOSI, GPIO_FUNC_SPI);

    /* Initialize GPIO pins. */

    gpio_init(WSPICO2_PIN_RST);
    gpio_init(WSPICO2_PIN_DC);
    gpio_init(WSPICO2_PIN_CS);
    gpio_init(WSPICO2_PIN_BL);

    gpio_set_dir(WSPICO2_PIN_RST, GPIO_OUT);
    gpio_set_dir(WSPICO2_PIN_DC, GPIO_OUT);
    gpio_set_dir(WSPICO2_PIN_CS, GPIO_OUT);
    gpio_set_dir(WSPICO2_PIN_BL, GPIO_OUT);

    gpio_put(WSPICO2_PIN_CS, 1);
    gpio_put(WSPICO2_PIN_DC, 0);
    gpio_put(WSPICO2_PIN_BL, 1);

    /* Initialize PWM on BL pin */

    gpio_set_function(WSPICO2_PIN_BL, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(WSPICO2_PIN_BL);
    pwm_set_wrap(slice, 100);
    pwm_set_chan_level(slice, PWM_CHAN_B, 1);
    pwm_set_clkdiv(slice, 50);
    pwm_set_enabled(slice, true);

    /* Hardware reset. */

    gpio_put(WSPICO2_PIN_CS, 1);
    sleep_ms(100);
    gpio_put(WSPICO2_PIN_RST, 0);
    sleep_ms(100);
    gpio_put(WSPICO2_PIN_RST, 1);
    sleep_ms(100);

    display->pwm_slice = slice;
}

static int wspico2_display_configure(struct wspico2 *display)
{
    u8 cmd;
    u8 len;

    /* clang-format off */

    const u8 inittab[] = {
        ST7789V_MADCTL, 1, 0x00,
        ST7789V_COLMOD, 1, 0x05,
        ST7789V_INVON, 0,
        ST7789V_CASET, 4, 0x00, 0x00, 0x01, 0x3F,
        ST7789V_RASET, 4, 0x00, 0x00, 0x00, 0xEF,
        ST7789V_PORCTRL, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
        ST7789V_GCTRL, 1, 0x35,
        ST7789V_VCOMS, 1, 0x1F,
        ST7789V_LCMCTRL, 1, 0x2C,
        ST7789V_VDVVRHEN, 1, 0x01,
        ST7789V_VRHS, 1, 0x12,
        ST7789V_VDVSET, 1, 0x20,
        ST7789V_FRCTR2, 1, 0x0F,
        ST7789V_PWCTRL1, 2, 0xA4, 0xA1,
        ST7789V_PVGAMCTRL, 14, 0xD0, 0x08, 0x11, 0x08, 0x0C, 0x15, 0x39,
                               0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2D,
        ST7789V_NVGAMCTRL, 14, 0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39,
                               0x44, 0x51, 0x0B, 0x16, 0x14, 0x2F, 0x31,
        ST7789V_SLPOUT, 0,
        ST7789V_DISPON, 0,
        0
    };

    /* clang-format on */

    for (i32 i = 0;;) {
        cmd = inittab[i++];
        len = inittab[i++];

        if (!cmd)
            break;

        wspico2_cmd(cmd);
        for (i32 j = 0; j < len; j++)
            wspico2_put(inittab[i + j]);

        i += len;
    }

    pwm_set_chan_level(display->pwm_slice, PWM_CHAN_B, 100);

    return 0;
}

static inline unsigned short convert_fullcolor(int f)
{
    unsigned short c;

    /*
       Convert a full-space 8 bit RGB into the display 5-6-5 bpc.

       RRRR RRRR GGGG GGGG BBBB BBBB
       v         v         v
       RRRR R    GGGG GG   BBBB B
    */

    c = ((f & 0xf80000) >> 8) | ((f & 0xfc00) >> 5) | ((f & 0xf8) >> 3);
    c = ((c & 0xff00) >> 8) | ((c & 0xff) << 8);

    return c;
}

static void wspico2_setwindow(u16 x, u16 y, u16 w, u16 h)
{
    wspico2_cmd(ST7789V_CASET);
    wspico2_put(x >> 8);
    wspico2_put(x & 0xff);
    wspico2_put((w - 1) >> 8);
    wspico2_put((w - 1) & 0xff);

    wspico2_cmd(ST7789V_RASET);
    wspico2_put(y >> 8);
    wspico2_put(y & 0xff);
    wspico2_put((h - 1) >> 8);
    wspico2_put((h - 1) & 0xff);
}

/**
 * wspico2_init(self)
 * Setup & initialize the Waveshare Pico LCD 2 display.
 */
static i32 wspico2_init(struct drv *self, ...)
{
    struct wspico2 *display;

    display = malloc(sizeof(*display));
    display->w = 320;
    display->h = 240;

    self->_data = display;

    wspico2_driver_configure(display);
    wspico2_display_configure(display);

    return EOK;
}

static i32 wspico2_fill_with_dma(u16 rgb565_color)
{
    dma_channel_config conf;
    u16 color;
    i32 chan;

    chan = dma_claim_unused_channel(true);
    conf = dma_channel_get_default_config(chan);

    color = rgb565_color;

    /* In order to fill the whole display with a single color, we don't need
       to allocate any memory as we can use a wrapping-read DMA channel to
       transfer our single color to SPI1. Note that we have to wrap because
       the SPI register is only 8 bits wide, and we want to send 16 bits
       of RGB565 color. */

    channel_config_set_transfer_data_size(&conf, DMA_SIZE_8);
    channel_config_set_dreq(&conf, spi_get_dreq(spi1, true));
    channel_config_set_read_increment(&conf, true);
    channel_config_set_write_increment(&conf, false);
    channel_config_set_ring(&conf, false, 1);

    wspico2_setwindow(0, 0, 240, 320);
    wspico2_cmd(ST7789V_RAMWR);
    gpio_put(WSPICO2_PIN_DC, 1);

    dma_channel_configure(chan, &conf, &spi_get_hw(spi1)->dr, &color,
                          240 * 320 * 2, true);
    dma_channel_wait_for_finish_blocking(chan);
    dma_channel_unclaim(chan);

    return EOK;
}

static i32 wspico2_fill(u32 color)
{
    return wspico2_fill_with_dma(convert_fullcolor(color));
}

static i32 wspico2_sync(struct wspico2 *display)
{
    if (!display->buffer)
        return EINVAL;

    dma_channel_config conf;
    i32 chan;

    chan = dma_claim_unused_channel(true);
    conf = dma_channel_get_default_config(chan);

    channel_config_set_transfer_data_size(&conf, DMA_SIZE_8);
    channel_config_set_dreq(&conf, spi_get_dreq(spi1, true));
    channel_config_set_read_increment(&conf, true);
    channel_config_set_write_increment(&conf, false);

    wspico2_setwindow(0, 0, 240, 320);
    wspico2_cmd(ST7789V_RAMWR);
    gpio_put(WSPICO2_PIN_DC, 1);

    dma_channel_configure(chan, &conf, &spi_get_hw(spi1)->dr, display->buffer,
                          240 * 320 * 2, true);
    dma_channel_wait_for_finish_blocking(chan);
    dma_channel_unclaim(chan);

    return EOK;
}

static i32 wspico2_ioctl(struct drv *self, u32 cmd, ...)
{
    va_list args;
    void *ptr;
    u16 color;

    va_start(args, cmd);

    switch (cmd) {
    case WSPICO2FILL:
        return wspico2_fill(va_arg(args, u32));
    case WSPICO2ATTACH:
        DATA->buffer = va_arg(args, void *);
        return EOK;
    case WSPICO2RGB565:
        color = convert_fullcolor(va_arg(args, u32));
        ptr = va_arg(args, u16 *);
        *(u16 *) ptr = color;
        return EOK;
    case WSPICO2SYNC:
        return wspico2_sync(DATA);
    }

    va_end(args);
    return EOK;
}

usize wspico2_write(struct drv *self, void *buffer, usize n)
{
    gpio_put(WSPICO2_PIN_DC, 1);
    if ((usize) spi_write_blocking(spi1, buffer, n) != n)
        return EIO;
    return EOK;
}

usize wspico2_read(struct drv *self, void *buffer, usize n)
{
    return EOK;
}

struct drv drv_wspico2_decl = {
    .name = "wspico2",
    .desc = "Waveshare Pico LCD 2 driver",
    .init = wspico2_init,
    .ioctl = wspico2_ioctl,
    .read = wspico2_read,
    .write = wspico2_write,
    ._data = NULL,
};
