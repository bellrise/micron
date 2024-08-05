/* net.h - network operations
   Copyright (c) 2024 bellrise */

#ifndef MICRON_NET_H
#define MICRON_NET_H 1

#include <lwip/netif.h>
#include <micron/micron.h>
#include <netif/ethernet.h>

struct net
{
    struct netif *iface;     /* lwip interface */
    const char *w_ssid;      /* wlan SSID */
    struct eth_addr w_bssid; /* wlan BSSID */
    i32 w_found_networks;    /* wlan found matching networks */
    bool w_connected;        /* true for connected wlan */
};

i32 net_init(struct net *net);
i32 net_close(struct net *net);

#endif /* MICRON_NET_H */
