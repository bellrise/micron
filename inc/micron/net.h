/* net.h - network operations
   Copyright (c) 2024 bellrise */

#ifndef MICRON_NET_H
#define MICRON_NET_H 1

#include <micron/micron.h>
#include <netif/ethernet.h>

struct net
{
    const char *ssid;
    struct eth_addr bssid;
    i32 wcn_found_networks;
    bool wcn_connected;
};

i32 net_init(struct net *net);
i32 net_close(struct net *net);

#endif /* MICRON_NET_H */
