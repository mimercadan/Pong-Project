#ifndef NET_H
#define NET_H
#include <stdint.h>
#include <sys/types.h>

/* Encabezado provisional (4 bytes). Se ajusta cuando el equipo cierre el protocolo.
   | version 1B | tipo 1B | longitud 2B (orden de red) | payload... */
#define PROTO_VERSION 1
#define HEADER_SIZE   4
#define MAX_PAYLOAD   512

typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t length;   /* bytes del payload */
} msg_header_t;

ssize_t recv_all(int fd, void *buf, size_t n);   /* lee exactamente n bytes */
ssize_t send_all(int fd, const void *buf, size_t n); /* envía exactamente n bytes */

/* Devuelve 1 si leyó un mensaje, 0 si el cliente cerró, -1 si hubo error */
int recv_message(int fd, msg_header_t *h, uint8_t *payload);
int send_message(int fd, uint8_t type, const uint8_t *payload, uint16_t len);

#endif
