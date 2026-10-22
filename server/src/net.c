#include "net.h"
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* TCP puede entregar medio mensaje o dos pegados: repetimos recv hasta completar n */
ssize_t recv_all(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, (char *)buf + got, n - got, 0);
        if (r <= 0) return r;          /* 0 = se cerró, -1 = error o timeout */
        got += r;
    }
    return (ssize_t)got;
}

ssize_t send_all(int fd, const void *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t s = send(fd, (const char *)buf + sent, n - sent, 0);
        if (s <= 0) return -1;
        sent += s;
    }
    return (ssize_t)sent;
}

int recv_message(int fd, msg_header_t *h, uint8_t *payload) {
    uint8_t raw[HEADER_SIZE];
    ssize_t r = recv_all(fd, raw, HEADER_SIZE);
    if (r <= 0) return (int)r;

    h->version = raw[0];
    h->type    = raw[1];
    uint16_t len_net;
    memcpy(&len_net, raw + 2, 2);
    h->length  = ntohs(len_net);       /* de orden de red a orden local */

    if (h->version != PROTO_VERSION || h->length > MAX_PAYLOAD) return -1;

    if (h->length > 0) {
        r = recv_all(fd, payload, h->length);
        if (r <= 0) return (int)r;
    }
    return 1;
}

int send_message(int fd, uint8_t type, const uint8_t *payload, uint16_t len) {
    uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
    if (len > MAX_PAYLOAD) return -1;

    buf[0] = PROTO_VERSION;
    buf[1] = type;
    uint16_t len_net = htons(len);
    memcpy(buf + 2, &len_net, 2);
    if (len) memcpy(buf + HEADER_SIZE, payload, len);

    return send_all(fd, buf, HEADER_SIZE + len) < 0 ? -1 : 0;
}
