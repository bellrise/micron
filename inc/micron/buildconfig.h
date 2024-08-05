/* buildconfig.h - build configuration
   Copyright (c) 2024 bellrise */

#ifndef MICRON_BUILDCONFIG_H
#define MICRON_BUILDCONFIG_H 1

#if __has_include("micron_genconfig.h")
# include "micron_genconfig.h"
#endif

#ifndef MICRON_CONFIG_MEM
# define MICRON_CONFIG_MEM_HEAP 128 /* heap size in kB */
# define MICRON_CONFIG_MEM      1
#endif /* MICRON_CONFIG_MEM */

#ifndef MICRON_CONFIG_NET
# define MICRON_CONFIG_NET_SSID   "" /* auto-connect network SSID */
# define MICRON_CONFIG_NET_PASSWD "" /* auto-connect network passwd */
# define MICRON_CONFIG_NET        1
#endif /* MICRON_CONFIG_NET */

#endif /* MICRON_BUILDCONFIG_H */
