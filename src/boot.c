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

struct meminfo __micron_meminfo;
struct net __micron_net;

int main()
{
    stdio_usb_init();
    stdio_set_driver_enabled(&stdio_usb, true);
    stdio_filter_driver(&stdio_usb);
    stdio_set_translate_crlf(&stdio_usb, false);

    while (!stdio_usb_connected())
        sleep_ms(50);

    syslog(LOG_INFO "Micron " MICRON_STRVER);

    /* Initialize the system */

    heap_init(&__micron_meminfo);
    net_init(&__micron_net);

    /* Enter the userland */

    syslog("Starting userland");
    extern void user_main();
    user_main();

    /* Exit the system */

    net_close(&__micron_net);
    heap_close(&__micron_meminfo);

    reset_usb_boot(0, 0);
}
