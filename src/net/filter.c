/* netfilter.c - network filtering rules
   Copyright (c) 2024 bellrise */

#include <micron/netfilter.h>

bool netfilter_lan_addr(const ip_addr_t *remote)
{
    /* Check for 192.168.0.0/16 or 10.0.0.0/8 addresses. */

    return (remote->addr & 0xffff) == 0xa8c0 || (remote->addr & 0xff) == 10;
}
