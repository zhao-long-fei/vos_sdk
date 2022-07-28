#ifndef TRANSPORT_SSL_H
#define TRANSPORT_SSL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "webclient_transport.h"


void transport_ssl_set_cert_data(webclient_transport_t *t, const char *data, int len);
void transport_ssl_set_client_cert_data(webclient_transport_t *t, const char *data, int len);
void transport_ssl_set_client_key_data(webclient_transport_t *t, const char *data, int len);
void transport_ssl_common_name_check(webclient_transport_t *t);

webclient_transport_t *transprot_ssl_init();

#ifdef __cplusplus
}
#endif
#endif
