#ifndef WEBCLIENT_TRANSPORT_H
#define WEBCLIENT_TRANSPORT_H
#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    WEBCLIENT_TRANS_CLOSE = -3,
    WEBCLIENT_TRANS_TIMEOUT = -2,
    WEBCLIENT_TRANS_ERROR = -1,
    WEBCLIENT_TRANS_OK,
} webclient_trans_err_t;

typedef struct webclient_transport_list webclient_transport_list_t;
typedef struct webclient_transport webclient_transport_t;

typedef webclient_trans_err_t (*webclient_connect_func)(webclient_transport_t *t, const char *host, int port, int timeout);
typedef webclient_trans_err_t (*webclient_read_func)(webclient_transport_t *t, void *buf, int len, int timeout, int *rlen);
typedef webclient_trans_err_t (*webclient_write_func)(webclient_transport_t *t, const void *buf, int len, int timeout, int *wlen);
typedef void (*webclient_close_func)(webclient_transport_t *t);
typedef void (*webclient_destroy_func)(webclient_transport_t *t);

webclient_transport_list_t *webclient_transport_list_init();
int webclient_transport_list_add(webclient_transport_list_t *obj, const char *schema, webclient_transport_t *t);
webclient_transport_t *webclient_transport_list_get_transport(webclient_transport_list_t *obj, const char *schema);
void webclient_transport_list_clean(webclient_transport_list_t *obj);
void webclient_transport_list_destroy(webclient_transport_list_t *obj);

webclient_transport_t *webclient_transport_init();
int webclient_transport_set_priv(webclient_transport_t *t, void *priv);
void *webclient_transport_get_priv(webclient_transport_t *t);
int webclient_transport_set_port(webclient_transport_t *t, int port);
int webclient_transport_get_port(webclient_transport_t *t);
int webclient_transport_set_io(webclient_transport_t *t,
        webclient_connect_func connect_func,
        webclient_read_func read_func,
        webclient_write_func write_func,
        webclient_close_func close_func,
        webclient_destroy_func destroy_func);
webclient_trans_err_t webclient_transport_connect(webclient_transport_t *t, const char *host, int port, int timeout);
webclient_trans_err_t webclient_transport_read(webclient_transport_t *t, void *buf, int len, int timeout, int *rlen);
webclient_trans_err_t webclient_transport_write(webclient_transport_t *t, const void *buf, int len, int timeout, int *wlen);
void webclient_transport_close(webclient_transport_t *t);
void webclient_transport_destroy(webclient_transport_t *t);

#ifdef __cplusplus
}
#endif
#endif
