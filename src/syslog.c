/* syslog.c - system logging
   Copyright (c) 2024 bellrise */

#include <micron/micron.h>
#include <micron/syslog.h>
#include <pico/printf.h>
#include <pico/time.h>
#include <stdarg.h>
#include <string.h>

void syslog_impl(const char *file, const char *fmt, ...)
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
    printf("\033[m\n");
    va_end(args);
}
