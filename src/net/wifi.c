/* wifi.c - Wi-Fi operations
   Copyright (c) 2024 bellrise */

#include <micron/buildconfig.h>
#include <micron/syslog.h>
#include <micron/wifi.h>
#include <pico/cyw43_arch.h>
#include <stdio.h>

static int wifi_scan_callback(struct net *net,
                              const cyw43_ev_scan_result_t *result)
{
    char mac[32];
    char *offset;

    if (!result)
        return 0;

    offset = mac;

    for (u32 i = 0; i < 6; i++) {
        snprintf(offset, 4, "%02x:", result->bssid[i]);
        offset += 3;
    }

    offset[-1] = 0;
    syslog(LOG_NOTE "wifi: ssid '%s' bssid %s", result->ssid, mac);

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
        return 1;
    }

    err = cyw43_arch_wifi_connect_timeout_ms(
        net->w_ssid, MICRON_CONFIG_NET_PASSWD, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (err)
        return 1;

    syslog("Connected to '%s' network", net->w_ssid);
    net->w_connected = true;

    return 0;
}
