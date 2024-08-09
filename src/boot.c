/* Micron - main.c
   Copyright (c) 2024 bellrise */

#include <micron/mem.h>
#include <micron/micron.h>
#include <micron/net.h>
#include <micron/syslog.h>
#include <pico/bootrom.h>
#include <pico/printf.h>
#include <pico/stdio.h>
#include <pico/stdio_usb.h>
#include <pico/time.h>

extern void user_main();

int main()
{
    stdio_usb_init();
    stdio_set_driver_enabled(&stdio_usb, true);
    stdio_filter_driver(&stdio_usb);
    stdio_set_translate_crlf(&stdio_usb, false);

    while (!stdio_usb_connected())
        sleep_ms(50);

    syslog(LOG_BOLD "Micron " MICRON_STRVER);

    /* Initialize the system */

    heap_init();
    net_init();
    user_main();

    /* If we happen to exit user mode, just reboot the board. */

    reset_usb_boot(0, 0);
}
