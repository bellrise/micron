/* net/ctrl.c - netctrl public API
   Copyright (c) 2024 bellrise */

#include <micron/net.h>

static void netctrl(uptr cmd, void *args, usize n_args, void *res, usize n_res)
{
    extern struct net __micron_net;

    /* Communication between the main thread and the network thread is done by
       using command queues. The user can send a command, like NC_SOCKET to
       create a socket - and the network queue will allocate and initialize
       the new socket once it has some time (isn't doing anything else).

       Then, it sends the result (in this case, a single pointer to a struct
       netsock) to the ctrlres queue. */

    queue_add_blocking(&__micron_net.netctrl, &cmd);
    for (usize i = 0; i < n_args; i++)
        queue_add_blocking(&__micron_net.netctrl, &((uptr *) args)[i]);
    for (usize i = 0; i < n_res; i++)
        queue_remove_blocking(&__micron_net.ctrlres, &((uptr *) res)[i]);
}

struct netsock *net_socket()
{
    struct netsock *sock;

    netctrl(NC_SOCKET, NULL, 0, &sock, 1);

    return sock;
}

i32 net_connect(struct netsock *sock, ip_addr_t ip, u16 port)
{
    uptr args[3];
    uptr res;

    args[0] = (uptr) sock;
    args[1] = (uptr) ip.addr;
    args[2] = port;

    netctrl(NC_CONNECT, args, 3, &res, 1);

    return res;
}

i32 net_bind(struct netsock *sock, ip_addr_t ip, u16 port)
{
    uptr args[3];
    uptr res;

    args[0] = (uptr) sock;
    args[1] = (uptr) ip.addr;
    args[2] = port;

    netctrl(NC_BIND, args, 3, &res, 1);

    return res;
}

i32 net_close(struct netsock *sock)
{
    uptr res;

    netctrl(NC_CLOSE, &sock, 1, &res, 1);

    return res;
}

struct netsock *net_accept(struct netsock *sock)
{
    struct netsock *client;

    netctrl(NC_ACCEPT, &sock, 1, NULL, 0);
    queue_remove_blocking(&sock->waiting_client, &client);

    return client;
}

u32 net_rx()
{
    u32 type;
    u32 rx;

    type = NCSTAT_RX;
    netctrl(NC_STAT, &type, 1, &rx, 1);

    return rx;
}

u32 net_tx()
{
    u32 type;
    u32 tx;

    type = NCSTAT_TX;
    netctrl(NC_STAT, &type, 1, &tx, 1);

    return tx;
}

usize net_read(struct netsock *sock, void *buffer, usize size)
{
    for (usize i = 0; i < size; i++)
        queue_remove_blocking(&sock->rbuf, &((u8 *) buffer)[i]);
    return size;
}

usize net_write(struct netsock *sock, const void *buffer, usize size)
{
    for (usize i = 0; i < size; i++)
        queue_add_blocking(&sock->wbuf, &((u8 *) buffer)[i]);
    return size;
}
