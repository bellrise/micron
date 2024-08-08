/* buildconfig.h - build configuration
   Copyright (c) 2024 bellrise */

#ifndef MICRON_BUILDCONFIG_H
#define MICRON_BUILDCONFIG_H 1

/* This file will just include the micron_genconfig generated header file. */

#if __has_include("micron_genconfig.h")
# include "micron_genconfig.h"
#else
# error "Missing the micron_genconfig.h file"
#endif

#endif /* MICRON_BUILDCONFIG_H */
