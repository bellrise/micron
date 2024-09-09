/* net.h - network operations
   Copyright (c) 2024 bellrise */

#ifndef MICRON_NET_H
#define MICRON_NET_H 1

#include <micron/buildconfig.h>

#if MICRON_CONFIG_NET

# include <lwip/netif.h>
# include <micron/micron.h>
# include <netif/ethernet.h>
# include <pico/util/queue.h>

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
    i32 nsocks;
    struct netsock *socks[32]; /* open netsocks */
    u32 netsock_rx;            /* RX on netsocks */
    u32 netsock_tx;            /* TX on netsocks */
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
    NC_STAT = 5,    /* xstat(value) -> u32 */
    NC_CLOSE = 6,   /* close(sock) -> i32 */
};

enum netctrl_stat
{
    NCSTAT_RX = 1, /* received bytes */
    NCSTAT_TX = 2, /* send bytes */
};

/* Initialize the TCP/IP stack, and start the network thread on
   the second core. */
i32 net_init();

ip_addr_t net_iface_ip();

u32 net_rx();
u32 net_tx();

struct netsock *net_socket();

struct netsock *net_accept(struct netsock *);
i32 net_connect(struct netsock *, ip_addr_t ip, u16 port);
i32 net_bind(struct netsock *, ip_addr_t ip, u16 port);
usize net_read(struct netsock *, void *buffer, usize size);
usize net_write(struct netsock *, const void *buffer, usize size);
i32 net_close(struct netsock *);

#endif /* MICRON_CONFIG_NET */
#endif /* MICRON_NET_H */
