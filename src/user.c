/* user.c - "userland" program
   Copyright (c) 2024 bellrise */

#include <lwip/err.h>
#include <lwip/inet_chksum.h>
#include <lwip/prot/icmp.h>
#include <lwip/raw.h>
#include <micron/buildconfig.h>
#include <micron/micron.h>
#include <micron/net.h>
#include <micron/syslog.h>
#include <pico/cyw43_arch.h>
#include <pico/printf.h>

void user_main()
{
    printf("Hello userland!\n");

    while (1) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }
}
