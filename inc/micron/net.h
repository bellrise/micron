/* net.h - network operations
   Copyright (c) 2024 bellrise */

#ifndef MICRON_NET_H
#define MICRON_NET_H 1

#include <lwip/netif.h>
#include <micron/micron.h>
#include <netif/ethernet.h>
#include <pico/util/queue.h>

struct net
{
    struct netif *iface;     /* lwip interface */
    const char *w_ssid;      /* wlan SSID */
    struct eth_addr w_bssid; /* wlan BSSID */
    i32 w_found_networks;    /* wlan found matching networks */
    bool w_connected;        /* true for connected wlan */
    queue_t netctrl;         /* control queue */
    queue_t ctrlres;         /* reply queue */
    u32 last_netsock_id;
    struct netsock *socks[32]; /* open netsocks */
};

struct netsock
{
    u8 id;
    queue_t rbuf;
    queue_t wbuf;
    ip_addr_t addr;
    u16 port;
    bool connected;
    bool waiting_for_client;
    queue_t waiting_client;
    struct tcp_pcb *tcp;
    struct net *net;
    u8 *tmpbuf;
    u32 packet_read_offset;
};

enum netctrl_cmd
{
    NC_SOCKET = 1,  /* socket() -> sock */
    NC_CONNECT = 2, /* connect(sock, ip, port) -> i32 */
    NC_BIND = 3,    /* bind(sock, ip, port) -> i32 */
    NC_ACCEPT = 4,  /* accept(sock) -> sock */
};

/* Initialize the TCP/IP stack, and start the network thread on
   the second core. */
i32 net_init();

ip_addr_t net_iface_ip();

struct netsock *socket();
i32 connect(struct netsock *, ip_addr_t ip, u16 port);
i32 bind(struct netsock *, ip_addr_t ip, u16 port);
struct netsock *accept(struct netsock *);

usize net_read(struct netsock *, void *buffer, usize size);
usize net_write(struct netsock *, const void *buffer, usize size);

#endif /* MICRON_NET_H */
