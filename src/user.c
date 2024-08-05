/* user.c - "userland" program
   Copyright (c) 2024 bellrise */

#include <micron/buildconfig.h>
#include <micron/micron.h>
#include <micron/syslog.h>
#include <pico/printf.h>
#include <pico/stdlib.h>

void user_main()
{
    printf("Hello userland!\n");
}
