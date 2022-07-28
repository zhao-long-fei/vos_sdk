#ifndef TRANSPORT_TCP_H
#define TRANSPORT_TCP_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>

#include "webclient_transport.h"

typedef struct tcp_transport tcp_transport_t;

tcp_transport_t *tcp_transport_init();
webclient_trans_err_t tcp_transport_connect(tcp_transport_t *tcp, const char *host, int port, int timeout);
webclient_trans_err_t tcp_transport_read(tcp_transport_t *tcp, void *buf, int len, int timeout, int *rlen);
webclient_trans_err_t tcp_transport_write(tcp_transport_t *tcp, const void *buf, int len, int timeout, int *wlen);
int tcp_transport_get_fd(tcp_transport_t *tcp);
bool tcp_transport_can_read(tcp_transport_t *tcp, int timeout);
bool tcp_transport_can_write(tcp_transport_t *tcp, int timeout);
void tcp_transport_close(tcp_transport_t *tcp);
void tcp_transport_destroy(tcp_transport_t *tcp);

webclient_transport_t *transport_tcp_init();

#ifdef __cplusplus
}
#endif
#endif
