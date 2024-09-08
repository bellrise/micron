/* net.c - network operations
   Copyright (c) 2024 bellrise */

#include <boards/pico_w.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/raw.h>
#include <lwip/stats.h>
#include <lwip/tcp.h>
#include <micron/buildconfig.h>
#include <micron/mem.h>
#include <micron/micron.h>
#include <micron/net.h>
#include <micron/netfilter.h>
#include <micron/syslog.h>
#include <micron/wifi.h>
#include <netif/ethernet.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>

/**
 * Callback when receiving ICMP packets. Allows only ICMP requests from the same
 * network, and only ECHO REQUEST packets. The rest is dropped.
 */
static u8 icmp_recv(struct net *__unused net, struct raw_pcb *block,
                    struct pbuf *packet, const ip_addr_t *remote_addr)
{
    struct icmp_hdr *icmp;
    struct pbuf *reply;
    u32 payload_len;
    u32 icmp_size;
    u32 ip_size;
    u32 rq_data;

    icmp_size = sizeof(struct icmp_hdr);
    ip_size = sizeof(struct ip_hdr);

    /* Drop any packets that are not from the same network and that have
       a strange size. */

    if (!netfilter_lan_addr(remote_addr))
        goto drop;
    if (packet->len < ip_size + icmp_size)
        goto drop;

    icmp = packet->payload + ip_size;
    payload_len = packet->len - ip_size - icmp_size;
    rq_data = icmp->data;

    /* Reply only to echo requests. */

    if (icmp->type != ICMP_ECHO)
        goto drop;

    reply = pbuf_alloc(PBUF_IP, packet->len - ip_size, PBUF_RAM);
    icmp = reply->payload;

    /* Echo reply, send back the same data & payload as the request. */

    icmp->type = ICMP_ER;
    icmp->data = rq_data;
    icmp->chksum = 0;
    icmp->code = 0;

    memcpy(reply->payload + icmp_size, packet->payload + icmp_size + ip_size,
           payload_len);

    icmp->chksum = inet_chksum(icmp, icmp_size + payload_len);
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

static struct netsock *netsock_create(struct net *net)
{
    struct netsock *sock;

    sock = malloc(sizeof(*sock));
    sock->tmpbuf = malloc(MICRON_CONFIG_NET_RWBUF);
    sock->id = ++net->last_netsock_id;
    sock->addr.addr = 0;
    sock->port = 0;
    sock->connected = false;
    sock->packet_read_offset = 0;
    sock->net = net;
    queue_init(&sock->rbuf, sizeof(u8), MICRON_CONFIG_NET_RWBUF);
    queue_init(&sock->wbuf, sizeof(u8), MICRON_CONFIG_NET_RWBUF);
    queue_init(&sock->waiting_client, sizeof(uptr), 1);

    return sock;
}

static void netsock_push(struct netsock *sock)
{
    u32 len;

    /* Move data from the write buffer into the TCP/IP stack. Note that we
       cannot send more data that can fit into the TCP queue, so the rest
       just stays our wbuf queue and blocks. */

    len = imin(tcp_sndbuf(sock->tcp), queue_get_level(&sock->wbuf));

    for (u32 i = 0; i < len; i++)
        queue_remove_blocking(&sock->wbuf, &sock->tmpbuf[i]);

    tcp_write(sock->tcp, sock->tmpbuf, len, 0);
    sock->net->netsock_tx += len;
}

static void net_push_all(struct net *net)
{
    struct netsock *sock;

    /* Push all data in write buffers in open netsocks. */

    for (i32 i = 0; i < net->nsocks; i++) {
        if (!net->socks[i])
            continue;
        sock = net->socks[i];
        if (queue_get_level(&sock->wbuf))
            netsock_push(sock);
    }
}

static i32 netsock_close(struct net *net, struct netsock *sock)
{
    /* Remove the socket from the netsock list. */

    for (i32 i = 0; i < net->nsocks; i++) {
        if (net->socks[i] == sock)
            net->socks[i] = NULL;
    }

    /* Send any data we have left. */

    while (!queue_is_empty(&sock->wbuf))
        netsock_push(sock);

    if (tcp_close(sock->tcp)) {
        syslog(LOG_ERR "failed to tcp_close(%d)", sock->id);
        return 1;
    }

    queue_free(&sock->rbuf);
    queue_free(&sock->wbuf);
    queue_free(&sock->waiting_client);
    free(sock->tmpbuf);
    free(sock);

    return 0;
}

static i8 netsock_tcp_recv(struct netsock *sock, struct tcp_pcb *__unused tcp,
                           struct pbuf *packet, i8 __unused err)
{
    u32 to_read;

    if (!packet) {
        sock->connected = false;
        return 0;
    }

    /* Because we have a limited amount of space in the read buffer, we can only
       read so many bytes. If we don't consume the whole packet at once, store
       the offset in the netsock and return INPROGRESS to notify lwip about our
       intention to read the packet again later. */

    to_read = imin(packet->len - sock->packet_read_offset,
                   sock->rbuf.element_count - queue_get_level(&sock->rbuf));
    pbuf_copy_partial(packet, sock->tmpbuf, to_read, sock->packet_read_offset);
    tcp_recved(tcp, to_read);
    sock->net->netsock_rx += to_read;

    for (u32 i = 0; i < to_read; i++)
        queue_add_blocking(&sock->rbuf, &((u8 *) sock->tmpbuf)[i]);

    /* Once we read the whole packet, free it and return OK. */

    if (packet->len == to_read + sock->packet_read_offset) {
        sock->packet_read_offset = 0;
        pbuf_free(packet);
        return ERR_OK;
    }

    sock->packet_read_offset += to_read;

    return ERR_INPROGRESS;
}

static i8 netsock_tcp_connected(struct netsock *__unused sock,
                                struct tcp_pcb *tcp, i8 __unused err)
{
    syslog("netsock_tcp: connected to %s:%d", ipaddr_ntoa(&tcp->remote_ip),
           tcp->remote_port, tcp->flags);
    return 0;
}

static void netsock_tcp_err(struct netsock *__unused sock, i8 err)
{
    syslog("netsock_tcp: TCP/IP failure (%d)", err);
}

static i32 net_add_sock(struct net *net, struct netsock *sock)
{
    for (i32 i = 0; i < net->nsocks; i++) {
        if (net->socks[i])
            continue;
        net->socks[i] = sock;
        return 0;
    }

    return 1;
}

static i8 netsock_tcp_accept(struct netsock *sock, struct tcp_pcb *tcp_client,
                             i8 __unused err)
{
    struct netsock *client;

    if (err) {
        syslog(LOG_ERR "lwip reported err %d", err);
        return 0;
    }

    if (!tcp_client)
        return 0;

    /* Accept the connection only if the netsock is waiting for a client,
       otherwise drop it. */

    if (!sock->waiting_for_client) {
        tcp_close(tcp_client);
        return ERR_CLSD;
    }

    /* Drop anything that isn't from the LAN network. */

    if (!netfilter_lan_addr(&tcp_client->remote_ip)) {
        tcp_close(tcp_client);
        return ERR_CLSD;
    }

    /* Create the new client netsock, and return it to the user by pushing
       it onto the waiting_client queue. */

    client = netsock_create(sock->net);
    client->addr.addr = tcp_client->remote_ip.addr;
    client->port = tcp_client->remote_port;
    client->tcp = tcp_client;
    client->connected = true;

    tcp_arg(tcp_client, client);
    tcp_err(tcp_client, (tcp_err_fn) netsock_tcp_err);
    tcp_recv(tcp_client, (tcp_recv_fn) netsock_tcp_recv);

    /* If we don't have space for the connection, abort it. */

    if (net_add_sock(sock->net, client)) {
        syslog(LOG_ERR "no space for new netsock");
        tcp_close(tcp_client);
        return ERR_CLSD;
    }

    sock->waiting_for_client = false;
    queue_add_blocking(&sock->waiting_client, &client);

    return 0;
}

static void netctrl_socket(struct net *net)
{
    struct netsock *sock;
    uptr nullptr;

    /* socket() -> sock */

    /* Create the netsock structure. */

    sock = netsock_create(net);

    /* Create the TCP control block. */

    if (!(sock->tcp = tcp_new_ip_type(IPADDR_TYPE_V4)))
        goto err;

    tcp_arg(sock->tcp, sock);
    tcp_err(sock->tcp, (tcp_err_fn) netsock_tcp_err);
    tcp_recv(sock->tcp, (tcp_recv_fn) netsock_tcp_recv);

    /* Save the netsock in the socket list, so we can access it later,
       and send it to the user. */

    if (net_add_sock(net, sock)) {
        syslog(LOG_ERR "no space for netsock");
        goto err;
    }

    queue_add_blocking(&net->ctrlres, &sock);

    return;

err:
    queue_free(&sock->rbuf);
    queue_free(&sock->wbuf);
    net->last_netsock_id--;
    free(sock->tmpbuf);
    free(sock);

    nullptr = 0;
    queue_add_blocking(&net->ctrlres, &nullptr);
}

static void netctrl_connect(struct net *net)
{
    struct netsock *sock;
    iptr err;

    /* connect(sock, ip, port) -> err */

    queue_remove_blocking(&net->netctrl, &sock);
    queue_remove_blocking(&net->netctrl, &sock->addr.addr);
    queue_remove_blocking(&net->netctrl, &sock->port);

    err = tcp_connect(sock->tcp, &sock->addr, sock->port,
                      (tcp_connected_fn) netsock_tcp_connected);
    if (err)
        syslog("netsock_tcp: connect failed (%d)", err);

    sock->connected = true;
    queue_add_blocking(&net->ctrlres, &err);
}

static void netctrl_bind(struct net *net)
{
    struct netsock *sock;
    iptr nerr;
    i8 err;

    /* bind(sock, ip, port) -> err */

    queue_remove_blocking(&net->netctrl, &sock);
    queue_remove_blocking(&net->netctrl, &sock->addr.addr);
    queue_remove_blocking(&net->netctrl, &sock->port);

    err = tcp_bind(sock->tcp, &sock->addr, sock->port);
    if (err)
        syslog("netsock_tcp: bind failed (%d)", err);

    sock->tcp = tcp_listen_with_backlog_and_err(sock->tcp, 2, &err);
    if (!sock->tcp)
        syslog("netsock_tcp: listen failed (%d)", err);

    syslog("netctrl: bind+listen %s:%d", ipaddr_ntoa(&sock->addr), sock->port);

    tcp_arg(sock->tcp, sock);
    tcp_accept(sock->tcp, (tcp_accept_fn) netsock_tcp_accept);

    nerr = err;
    queue_add_blocking(&net->ctrlres, &nerr);
}

static void netctrl_accept(struct net *net)
{
    struct netsock *server;

    /* accept(sock) -> sock */

    queue_remove_blocking(&net->netctrl, &server);
    server->waiting_for_client = true;

    /* We don't wait for the client connection here, because we have other
       important things to do - return the client once it actually has
       connected, blocking the main thread until something happens. */
}

static void netctrl_stat(struct net *net)
{
    i32 value;

    queue_remove_blocking(&net->netctrl, &value);
    switch (value) {
    case NCSTAT_RX:
        queue_add_blocking(&net->ctrlres, &net->netsock_rx);
        break;
    case NCSTAT_TX:
        queue_add_blocking(&net->ctrlres, &net->netsock_tx);
        break;
    }
}

static void netctrl_close(struct net *net)
{
    struct netsock *sock;
    i32 res;

    queue_remove_blocking(&net->netctrl, &sock);
    res = netsock_close(net, sock);
    queue_add_blocking(&net->ctrlres, &res);
}

static void collect_netctrl(struct net *net)
{
    u32 cmd;

    if (queue_is_empty(&net->netctrl))
        return;

    queue_remove_blocking(&net->netctrl, &cmd);
    switch (cmd) {
    case NC_SOCKET:
        netctrl_socket(net);
        break;
    case NC_CONNECT:
        netctrl_connect(net);
        break;
    case NC_BIND:
        netctrl_bind(net);
        break;
    case NC_ACCEPT:
        netctrl_accept(net);
        break;
    case NC_STAT:
        netctrl_stat(net);
        break;
    case NC_CLOSE:
        netctrl_close(net);
        break;
    }
}

struct net __micron_net;

static void net_thread()
{
    struct net *net;
    usize heap_free;
    i32 link;
    i32 rssi;

    net = &__micron_net;

    icmp_serve(net);

    /* Continuously poll for events on the network interface, because we should
       be running on the other core. Apart from working the IP stack, collect
       netctrl commands, and move data from the queues onto the TCP/IP stack. */

    while (1) {
        link = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        if (link < 0) {
            syslog(LOG_ERR "wifi: link changed: %d", link);
            break;
        }

        /* WARNING: this line is very important - it seems like it isn't doing
           much, but not checking the connection RSSI will end up stopping all
           network traffic from reaching interface. :( */
        cyw43_wifi_get_rssi(&cyw43_state, &rssi);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);

        heap_free = malloc_heap_free();
        if (heap_free < 16384)
            syslog(LOG_WARN "low heap memory: %d kB", heap_free >> 10);

        collect_netctrl(net);
        net_push_all(net);
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(50));
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
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

i32 net_init()
{
    struct net *net;

    if (!MICRON_CONFIG_NET)
        return 0;

    /* Initialize the network stack, and connect to Wi-Fi. */

    net = &__micron_net;
    memset(net, 0, sizeof(*net));

    net->w_ssid = MICRON_CONFIG_NET_SSID;
    net->last_netsock_id = 0;
    net->nsocks = 3;
    queue_init(&net->netctrl, sizeof(uptr), 32);
    queue_init(&net->ctrlres, sizeof(uptr), 32);

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND)) {
        syslog(LOG_ERR "Failed to initialize cyw43 device");
        return 1;
    }

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

    /* Run the network stuff on the other core. */

    multicore_launch_core1(net_thread);

    return 0;
}

ip_addr_t net_iface_ip()
{
    return __micron_net.iface->ip_addr;
}
