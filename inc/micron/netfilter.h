/* netfilter.h - network packet filtering rules
   Copyright (c) 2024 bellrise */

#ifndef MICRON_NETFILTER_H
#define MICRON_NETFILTER_H 1

#include <lwip/ip.h>

bool netfilter_lan_addr(const ip_addr_t *addr);

#endif /* MICRON_NETFILTER_H */
