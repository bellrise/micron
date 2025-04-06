/* syslog.h - logging functions
   Copyright (c) 2024 bellrise */

#ifndef MICRON_SYSLOG_H
#define MICRON_SYSLOG_H 1

#define LOG_ERR  "\033[1;31m"
#define LOG_BOLD "\033[1;38m"
#define LOG_WARN "\033[1;33m"
#define LOG_NOTE "\033[90m"

void syslog_impl(const char *file, const char *end, const char *fmt, ...);
void crash_impl(const char *file, int line, const char *fmt, ...);

/**
 * syslog
 * Print some information to the system log.
 * @fmt: printf-like format string
 * @...: format arguments
 */
#define syslog(...) syslog_impl(__FILE__, "\033[m\n", __VA_ARGS__)
#define crash(...)  crash_impl(__FILE__, __LINE__, __VA_ARGS__)

#endif /* MICRON_SYSLOG_H */
