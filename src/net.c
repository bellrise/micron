/* net.c - network operations
   Copyright (c) 2024 bellrise */

#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/raw.h>
#include <lwip/tcp.h>
#include <micron/buildconfig.h>
#include <micron/net.h>
#include <micron/syslog.h>
#include <netif/ethernet.h>
#include <pico/cyw43_arch.h>

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
    syslog("wifi_scan: ssid %s bssid %s", result->ssid, mac);

    if (!strcmp((const char *) result->ssid, net->w_ssid))
        net->w_found_networks++;

    return 0;
}

static i32 wifi_connect(struct net *net)
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
    if (err) {
        syslog("Failed to connect to Wi-Fi network");
        return 1;
    }

    syslog("Connected to '%s' network", net->w_ssid);
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

static bool netfilter_same_network(struct net *net, const ip_addr_t *remote)
{
    u32 network;

    /* The most basic of net filters: allow only IP addresses coming from the
       same network. */

    network = net->iface->ip_addr.addr & net->iface->netmask.addr;
    return network == (remote->addr & net->iface->netmask.addr);
}

/**
 * Callback when receiving ICMP packets. Allows only ICMP requests from the same
 * network, and only ECHO REQUEST packets. The rest is dropped.
 */
static u8 icmp_recv(struct net *net, struct raw_pcb *block, struct pbuf *packet,
                    const ip_addr_t *remote_addr)
{
    struct icmp_hdr *icmp;
    struct pbuf *reply;
    u32 payload_len;
    u32 rq_data;

    /* Drop any packets that are not from the same network and that have
       a strange size. */

    if (!netfilter_same_network(net, remote_addr))
        goto drop;
    if (packet->len < sizeof(struct ip_hdr) + sizeof(struct icmp_hdr))
        goto drop;

    icmp = packet->payload + sizeof(struct ip_hdr);
    payload_len = packet->len - sizeof(struct ip_hdr) - sizeof(struct icmp_hdr);
    rq_data = icmp->data;

    /* Reply only to echo requests. */

    if (icmp->type != ICMP_ECHO)
        goto drop;

    reply = pbuf_alloc(PBUF_IP, packet->len - sizeof(struct ip_hdr), PBUF_RAM);
    icmp = reply->payload;

    /* Echo reply, send back the same data & payload as the request. */

    icmp->type = ICMP_ER;
    icmp->data = rq_data;
    icmp->chksum = 0;
    icmp->code = 0;

    memcpy(reply->payload + sizeof(struct icmp_hdr),
           packet->payload + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr),
           payload_len);

    icmp->chksum = inet_chksum(icmp, sizeof(struct icmp_hdr) + payload_len);
    raw_sendto(block, reply, remote_addr);
    pbuf_free(reply);

drop:
    pbuf_free(packet);
    return 1;
}

static void icmp_serve(struct net *net)
{
    struct raw_pcb *icmp_control;

    if (!(icmp_control = raw_new_ip_type(IPADDR_TYPE_V4, IP_PROTO_ICMP)))
        return syslog("icmp: Failed to initialize");

    /* Serve ICMP requests on the default interface. */

    raw_bind_netif(icmp_control, net->iface);
    raw_recv(icmp_control, (raw_recv_fn) icmp_recv, net);

    syslog("icmp: Serving ICMP packets");
}

i32 net_init(struct net *net)
{
    memset(net, 0, sizeof(*net));

    net->w_ssid = MICRON_CONFIG_NET_SSID;

    if (cyw43_arch_init()) {
        syslog(LOG_ERR "Failed to initialize cyw43 device");
        return 1;
    }

    if (wifi_connect(net)) {
        syslog("Failed to connect to Wi-Fi");
        return 1;
    }

    print_ifaces();

    net->iface = netif_default;
    icmp_serve(net);

    return 0;
}

i32 net_close(struct net *__unused net)
{
    cyw43_arch_deinit();
    return 0;
}
