/* wifi.c - Wi-Fi operations
   Copyright (c) 2024 bellrise */

#include <micron/buildconfig.h>
#include <micron/errno.h>
#include <micron/syslog.h>
#include <micron/wifi.h>
#include <pico/cyw43_arch.h>
#include <stdio.h>

static int wifi_scan_callback(struct net *net,
                              const cyw43_ev_scan_result_t *result)
{
    char mac[32];

    if (!result)
        return 0;

    syslog("Found ssid '%s'", result->ssid, mac);

    if (!strcmp((const char *) result->ssid, net->w_ssid))
        net->w_found_networks++;

    return 0;
}

i32 wifi_connect(struct net *net)
{
    cyw43_wifi_scan_options_t scan_opts;
    i32 err;

    cyw43_arch_enable_sta_mode();

    /* Scan only matching SSIDs. */

    memset(&scan_opts, 0, sizeof(scan_opts));
    strcpy((char *) scan_opts.ssid, net->w_ssid);

    syslog("Scanning for Wi-Fi networks");

    err = cyw43_wifi_scan(&cyw43_state, &scan_opts, net,
                          (void *) wifi_scan_callback);
    if (err) {
        syslog(LOG_ERR "Failed to start scan (err=%d)", err);
        return 1;
    }

    /* Wait until the scan has finished. */

    while (1) {
        if (!cyw43_wifi_scan_active(&cyw43_state))
            break;
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }

    /* Connect to the network with a timeout of 10s. */

    if (!net->w_found_networks) {
        syslog("No network named %s found", net->w_ssid);
        return ENOENT;
    }

    syslog("Connecting to %s", net->w_ssid);

    err = cyw43_arch_wifi_connect_timeout_ms(
        net->w_ssid, MICRON_CONFIG_NET_PASSWD, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (err) {
        syslog(LOG_ERR "Wi-Fi connect timeout (err=%d)", err);
        return 1;
    }

    net->w_connected = true;

    return 0;
}

static void print_ifaces()
{
    struct netif *iface;
    char addr[16];
    char gate[16];
    char mac[24];
    u32 mask_addr;
    u32 mask;

    iface = netif_list;

    while (1) {
        ipaddr_ntoa_r(&iface->ip_addr, addr, 16);
        ipaddr_ntoa_r(&iface->gw, gate, 16);
        snprintf(mac, 24, "%02x:%02x:%02x:%02x:%02x:%02x", iface->hwaddr[0],
                 iface->hwaddr[1], iface->hwaddr[2], iface->hwaddr[3],
                 iface->hwaddr[4], iface->hwaddr[5]);

        mask = 0;
        mask_addr = ntohl(iface->netmask.addr);
        while (mask_addr) {
            mask++;
            mask_addr <<= 1;
        }

        syslog("%.2s: ip %s/%u via %s hw %s %s", iface->name, addr, mask, gate,
               mac, netif_is_link_up(iface) ? "UP" : "DOWN");

        iface = iface->next;
        if (!iface)
            break;
    }
}

i32 wifi_init(struct net *net)
{
    net->w_ssid = MICRON_CONFIG_NET_SSID;

    syslog("Trying to connect with '%s' '%s'", MICRON_CONFIG_NET_SSID,
           MICRON_CONFIG_NET_PASSWD);

    while (1) {
        /* Try to connect to Wi-Fi. */

        if (wifi_connect(net)) {
            syslog("Failed to connect to Wi-Fi, trying again in 5s...");
            sleep_ms(5000);
            continue;
        }

        break;
    }

    print_ifaces();
    net->iface = netif_default;

    return 0;
}
