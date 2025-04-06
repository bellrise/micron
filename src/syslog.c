/* syslog.c - system logging
   Copyright (c) 2024 bellrise */

#include <hardware/watchdog.h>
#include <micron/micron.h>
#include <micron/syslog.h>
#include <pico/printf.h>
#include <pico/time.h>
#include <stdarg.h>
#include <string.h>

void syslog_impl(const char *file, const char *end, const char *fmt, ...)
{
    va_list args;
    char *dotpos;
    char *slash;
    float time;

    va_start(args, fmt);

    dotpos = strchr(file, '.');
    slash = strrchr(file, '/');
    if (slash != NULL)
        file = slash + 1;

    time = (float) time_us_64() / 1000000;
    printf("[\033[32m% 10.4f\033[m] \033[33m%.*s:\033[m ", time,
           (int) ((uptr) dotpos - (uptr) file), file);
    vprintf(fmt, args);
    printf("%s", end);
    va_end(args);
}

void crash_impl(const char *file, int line, const char *fmt, ...)
{
    va_list args;
    char *dotpos;
    char *slash;

    va_start(args, fmt);

    dotpos = strchr(file, '.');
    slash = strrchr(file, '/');
    if (slash != NULL)
        file = slash + 1;

    printf("\033[1;31mPANIC in %.*s on %d\n",
           (int) ((uptr) dotpos - (uptr) file), file, line);
    vprintf(fmt, args);
    printf("\033[m\n\n");
    va_end(args);

    /* Sleep 5s, and force reboot */

    sleep_ms(5000);
    watchdog_reboot(0, 0, 0);
}
