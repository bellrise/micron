/* user.c - "userland" program
   Copyright (c) 2024 bellrise */

#include <lwip/ip_addr.h>
#include <micron/buildconfig.h>
#include <micron/micron.h>
#include <micron/net.h>
#include <micron/syslog.h>
#include <pico/printf.h>
#include <pico/time.h>
#include <string.h>

struct http_client
{
    u32 nlines;
    u32 linesize;
    u32 bufsize;
    char **lines;
    char *buf1;
    char *http_header_buf;
};

static void split_http_line(char *line, char **method, char **path, char **ver)
{
    char *p;

    /* TODO: this is kind of stinky, rewrite this someday */

    p = line;

    *method = p;

    while (*p != ' ' && *p)
        p++;
    while (*p == ' ' && *p)
        p++;

    p[-1] = 0;
    *path = p;

    while (*p != ' ' && *p)
        p++;
    while (*p == ' ' && *p)
        p++;

    p[-1] = 0;
    *ver = p;
}

static void send_res(struct http_client *http, struct netsock *client,
                     const char *statusname, const char *content_type,
                     const char *payload)
{
    const char *http_fmt;

    http_fmt = "HTTP/1.1 %s\r\n"
               "Server: micron-http\r\n"
               "Content-Length: %d\r\n"
               "Content-Type: %s\r\n"
               "\r\n";

    snprintf(http->http_header_buf, http->bufsize, http_fmt, statusname,
             strlen(payload), content_type);
    net_write(client, http->http_header_buf, strlen(http->http_header_buf));
    net_write(client, payload, strlen(payload));
}

static void send_ok(struct http_client *http, struct netsock *client,
                    const char *content_type, const char *payload)
{
    send_res(http, client, "200 OK", content_type, payload);
}

static void route_(struct http_client *http, struct netsock *client)
{
    const char *json_fmt;
    char *json_res;

    json_res = http->buf1;
    json_fmt = "{\"firmware_version\": \"%s\"}";

    snprintf(json_res, http->bufsize, json_fmt, MICRON_STRVER);

    send_ok(http, client, "application/json", json_res);
}

static void route_metrics(struct http_client *http, struct netsock *client)
{
    const char *reply_fmt;
    usize uptime_ms;
    float uptime;
    char *reply;

    reply = http->buf1;
    uptime_ms = time_us_64() / 1000;
    uptime = (float) uptime_ms / 1000;

    reply_fmt = "# HELP uptime_total System uptime in unix format\n"
                "# TYPE uptime_total counter\n"
                "uptime_total %.2f\n"
                "# HELP netstat_rx_bytes Received bytes on netsockets\n"
                "# TYPE netstat_rx_bytes counter\n"
                "netstat_rx_bytes %d\n"
                "# HELP netstat_tx_bytes Sent bytes on netsockets\n"
                "# TYPE netstat_tx_bytes counter\n"
                "netstat_tx_bytes %d\n";

    snprintf(reply, http->bufsize, reply_fmt, uptime, net_tx(), net_rx());

    send_ok(http, client, "text/plain", reply);
}

static void route_404(struct http_client *http, struct netsock *client)
{
    const char *json;

    json = "{\"status\": 404, \"error\": \"no such endpoint\"}";
    send_res(http, client, "404 Not Found", "application/json", json);
}

static void unsupported_ver(struct netsock *client)
{
    const char *http_res;

    http_res = "HTTP/1.1 505 HTTP Version Not Supported\r\n"
               "Server: micron-http\r\n"
               "\r\n";

    net_write(client, http_res, strlen(http_res));
}

static void accept_client(struct http_client *http, struct netsock *server)
{
    struct netsock *client;
    char *rq_method;
    char *rq_path;
    char *rq_ver;
    i32 nline;
    i32 wline;
    u8 c;

    client = net_accept(server);
    nline = 0;
    wline = 0;

    for (u32 i = 0; i < http->nlines; i++)
        *http->lines[i] = 0;

    while (1) {
        net_read(client, &c, 1);

        if (c == '\r')
            continue;

        /* Empty line, end of header. */
        if (c == '\n' && !strlen(http->lines[nline]))
            break;

        if (c == '\n') {
            http->lines[nline][wline] = 0;
            nline++;
            wline = 0;
            continue;
        }

        http->lines[nline][wline++] = (char) c;
    }

    /* Stupid routing */

    split_http_line(http->lines[0], &rq_method, &rq_path, &rq_ver);
    printf("[\033[1mhttp\033[m] \033[32m%s\033[0m \033[35m%s\033[m "
           "\033[34m%s\033[0m\n",
           rq_method, rq_path, rq_ver);

    if (strcmp(rq_ver, "HTTP/1.1")) {
        unsupported_ver(client);
        goto end;
    }

    if (!strcmp(rq_method, "GET")) {
        if (!strcmp(rq_path, "/"))
            route_(http, client);
        else if (!strcmp(rq_path, "/metrics"))
            route_metrics(http, client);
        else
            route_404(http, client);
    } else {
        route_404(http, client);
    }

end:

    /* Close the connection after replying. */

    net_close(client);
}

static void http_service()
{
    struct http_client http_client;
    struct netsock *server;
    i32 err;

    server = net_socket();
    err = net_bind(server, net_iface_ip(), 80);

    if (err) {
        printf("Failed to bind() address\n");
        return;
    }

    http_client.nlines = 32;
    http_client.bufsize = 1024;
    http_client.linesize = 256;

    http_client.buf1 = malloc(http_client.bufsize);
    http_client.http_header_buf = malloc(http_client.bufsize);
    http_client.lines = malloc(http_client.nlines * sizeof(char *));

    for (u32 i = 0; i < http_client.nlines; i++)
        http_client.lines[i] = malloc(http_client.linesize);

    while (1)
        accept_client(&http_client, server);
}

void user_main()
{
    /* Serve HTTP requests on :80. */
    http_service();
}
