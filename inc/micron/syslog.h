/* syslog.h - logging functions
   Copyright (c) 2024 bellrise */

#ifndef MICRON_SYSLOG_H
#define MICRON_SYSLOG_H 1

#define LOG_ERR  "\033[1;31m"
#define LOG_BOLD "\033[1;37m"
#define LOG_WARN "\033[1;33m"
#define LOG_NOTE "\033[90m"

/**
 * syslog
 * Print some information to the system log.
 * @fmt: printf-like format string
 * @...: format arguments
 */
#define syslog(...) syslog_impl(__FILE__, __VA_ARGS__)

void syslog_impl(const char *file, const char *fmt, ...);

#endif /* MICRON_SYSLOG_H */
