/* onewire.c - onewire driver
   Copyright (c) 2024 bellrise */

#include "micron/micron.h"

#include <hardware/pio.h>
#include <micron/drv.h>
#include <stdarg.h>
#include <stdlib.h>

#define DRV_NAME "onewire"

#if __has_include("piocode/onewire.h")
# include "piocode/onewire.h"
#else
# error "missing onewire piocode"
#endif

struct wire
{
    pio_hw_t *pio; /* the PIO in use */
    i32 sm;        /* the state machine number in use */
    u32 pin;       /* the GPIO data pin for the 1-wire connection */
};

static usize wire_write(struct drv *self, void *buffer, usize n);
static usize wire_read(struct drv *self, void *buffer, usize n);

/**
 * wire_init(self, u32 gpio_pin)
 * Open a 1-Wire connection on the given GPIO pin.
 */
static i32 wire_init(struct drv *self, ...)
{
    pio_sm_config config;
    struct wire *wire;
    va_list params;
    i32 pio_offset;

    va_start(params, self);

    wire = malloc(sizeof(*wire));
    wire->pio = NULL;
    self->_data = wire;

    /* Collect the GPIO parameter. */

    wire->pin = va_arg(params, u32);
    va_end(params);

    /* Select a PIO. */

    if (pio_can_add_program(pio0, &onewire_program))
        wire->pio = pio0;
    else if (pio_can_add_program(pio1, &onewire_program))
        wire->pio = pio1;
    if (!wire->pio)
        return DVE_NPIO;

    pio_offset = pio_add_program(wire->pio, &onewire_program);
    if (pio_offset < 0)
        return DVE_NPIO;

    /* Claim a state machine. */

    wire->sm = pio_claim_unused_sm(wire->pio, false);
    if (wire->sm < 0)
        return DVE_NSM;

    pio_gpio_init(wire->pio, wire->pin);
    config = onewire_program_get_default_config(pio_offset);

    /* Divide clock to make it slower, select only 1 GPIO pin to interface with,
       autopush after 8 bits of data. */

    sm_config_set_clkdiv_int_frac(&config, onewire_clkdiv, 0);
    sm_config_set_set_pins(&config, wire->pin, 1);
    sm_config_set_out_pins(&config, wire->pin, 1);
    sm_config_set_in_pins(&config, wire->pin);
    sm_config_set_in_shift(&config, true, true, 8);

    /* Start the state machine. */

    pio_sm_init(wire->pio, wire->sm, pio_offset, &config);
    pio_sm_set_enabled(wire->pio, wire->sm, true);

    return DVE_OK;
}

#define WIRE ((struct wire *) self->_data)

static usize wire_write(struct drv *self, void *buffer, usize n)
{
    pio_sm_put_blocking(WIRE->pio, WIRE->sm, onewire_writeb);
    pio_sm_put_blocking(WIRE->pio, WIRE->sm, n - 1);

    for (usize i = 0; i < n; i++)
        pio_sm_put_blocking(WIRE->pio, WIRE->sm, ((u8 *) buffer)[i]);

    return DVE_OK;
}

static usize wire_read(struct drv *self, void *buffer, usize n)
{
    /* Note that we get 32-bit words from the state machine FIFO, we need to
       shift the high byte into the first position. */

    for (usize i = 0; i < n; i++) {
        pio_sm_put_blocking(WIRE->pio, WIRE->sm, onewire_readb);
        ((u8 *) buffer)[i] = pio_sm_get_blocking(WIRE->pio, WIRE->sm) >> 24;
    }

    return DVE_OK;
}

struct drv drv_onewire_decl = {
    .name = "onewire",
    .desc = "1-Wire driver",
    .init = wire_init,
    .read = wire_read,
    .write = wire_write,
    ._data = NULL,
};
