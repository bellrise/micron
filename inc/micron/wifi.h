/* wifi.h - Wi-Fi operations
   Copyright (c) 2024 bellrise */

#ifndef MICRON_WIFI_H
#define MICRON_WIFI_H 1

#include <micron/net.h>

/* Scan & connect to Wi-Fi using net->ssid & CONFIG_NET_PASSWD. */
i32 wifi_connect(struct net *net);

/* Initialize Wi-Fi setup in net device. */
i32 wifi_init(struct net *net);

#endif /* MICRON_WIFI_H */
