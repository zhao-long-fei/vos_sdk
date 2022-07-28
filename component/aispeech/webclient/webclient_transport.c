#include "webclient_transport.h"

#include "gh.h"
#include "list.h"

struct webclient_transport_list {
    struct list_head header;
};

struct webclient_transport {
    struct list_head next;
    char *schema;
    int port;
    webclient_connect_func connect_func;
    webclient_read_func read_func;
    webclient_write_func write_func;
    webclient_close_func close_func;
    webclient_destroy_func destroy_func;
    void *priv;
};

webclient_transport_list_t *webclient_transport_list_init() {
    webclient_transport_list_t *obj = gh_calloc(1, sizeof(*obj));
    if (obj) {
        INIT_LIST_HEAD(&obj->header);
    }
    return obj;
}

int webclient_transport_list_add(webclient_transport_list_t *obj, const char *schema, webclient_transport_t *t) {
    t->schema = gh_strdupx(NULL, schema);
    list_add_tail(&t->next, &obj->header);
    return 0;
}

webclient_transport_t *webclient_transport_list_get_transport(webclient_transport_list_t *obj, const char *schema) {
     if (!schema) {
         if (list_empty(&obj->header)) return NULL;
         return list_first_entry(&obj->header, struct webclient_transport, next);
     }
     webclient_transport_t *item;
     list_for_each_entry(item, &obj->header, next) {
         if (0 == strcasecmp(item->schema, schema)) return item;
     }
     return NULL;
 }

void webclient_transport_list_clean(webclient_transport_list_t *obj) {
    webclient_transport_t *item, *item2;
    list_for_each_entry_safe(item, item2, &obj->header, next) {
        list_del(&item->next);
        webclient_transport_destroy(item);
    }
}

void webclient_transport_list_destroy(webclient_transport_list_t *obj) {
    webclient_transport_list_clean(obj);
    gh_free(obj);
}

webclient_transport_t *webclient_transport_init() {
    webclient_transport_t *t = gh_calloc(1, sizeof (*t));
    return t;
}

int webclient_transport_set_priv(webclient_transport_t *t, void *priv) {
    t->priv = priv;;
    return 0;
}

void *webclient_transport_get_priv(webclient_transport_t *t) {
    return t->priv;
}

int webclient_transport_set_port(webclient_transport_t *t, int port) {
    t->port = port;
    return 0;
}

int webclient_transport_get_port(webclient_transport_t *t) {
    return t->port;
}

void webclient_transport_destroy(webclient_transport_t *t) {
    if (t->destroy_func) {
        t->destroy_func(t);
    }
    gh_free(t->schema);
    gh_free(t);
}

webclient_trans_err_t webclient_transport_connect(webclient_transport_t *t, const char *host, int port, int timeout) {
    webclient_trans_err_t err = WEBCLIENT_TRANS_OK;
    if (t && t->connect_func) {
        err = t->connect_func(t, host, port, timeout);
    }
    return err;
}

webclient_trans_err_t webclient_transport_read(webclient_transport_t *t, void *buf, int len, int timeout, int *rlen) {
    webclient_trans_err_t err = WEBCLIENT_TRANS_OK;
    if (t && t->read_func) {
        err = t->read_func(t, buf, len, timeout, rlen);
    }
    return err;
}

webclient_trans_err_t webclient_transport_write(webclient_transport_t *t, const void *buf, int len, int timeout, int *wlen) {
    webclient_trans_err_t err = WEBCLIENT_TRANS_OK;
    if (t && t->write_func) {
        err = t->write_func(t, buf, len, timeout, wlen);
    }
    return err;
}

int webclient_transport_set_io(webclient_transport_t *t,
        webclient_connect_func connect_func,
        webclient_read_func read_func,
        webclient_write_func write_func,
        webclient_close_func close_func,
        webclient_destroy_func destroy_func) {
    t->connect_func = connect_func;
    t->read_func = read_func;
    t->write_func = write_func;
    t->close_func = close_func;
    t->destroy_func = destroy_func;
    return 0;
}

void webclient_transport_close(webclient_transport_t *t) {
    if (t && t->close_func) {
        t->close_func(t);
    }
}
