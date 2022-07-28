#include "webclient.h"

#include "list.h"
#include "gh.h"

#include "webclient_transport.h"
#include "transport_tcp.h"
#include "transport_ssl.h"
#include "http_parser.h"

#include <ctype.h>

#define XLOG_TAG "webclient"
#define XLOG_LVL XLOG_LEVEL_INFO
#include "xlog.h"

#define DEFAULT_HTTP_PATH "/"
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443
#define DEFAULT_DATA_BUF (4 * 1024)
#define DEFAULT_HTTP_USER_AGENT "AISPEECH HTTP Client/1.0"

static char *do_strcat(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    char *d = gh_malloc(len1 + len2 + 1);
    if (d) {
        memcpy(d, s1, len1);
        memcpy(d + len1, s2, len2);
        d[len1 + len2] = '\0';
    }
    return d;
}

//===========================================================================
typedef struct http_header_item {
    struct list_head next;
    char *k;
    char *v;
} http_header_item_t;

typedef struct http_headers {
    struct list_head h;
} http_headers_t;

static int http_headers_init(http_headers_t *headers) {
    INIT_LIST_HEAD(&headers->h);
    return 0;
}

static http_header_item_t *http_headers_get_header(http_headers_t *headers, const char *key) {
    http_header_item_t *item;
    list_for_each_entry(item, &headers->h, next) {
        if (0 == strcasecmp(item->k, key)) {
            return item;
        }
    }
    return NULL;
}

static http_header_item_t *http_headers_detach_header(http_headers_t *headers) {
    if (list_empty(&headers->h)) return NULL;
    http_header_item_t *item = list_first_entry(&headers->h, struct http_header_item, next);
    if (item) {
        list_del(&item->next);
    }
    return item;
}

static bool http_headers_is_empty(http_headers_t *headers) {
    return list_empty(&headers->h); 
}

static int http_headers_add_header(http_headers_t *headers, http_header_item_t *item) {
    list_add_tail(&item->next, &headers->h);
    return 0;
}

static int http_headers_set_header2(http_headers_t *headers, const char *key, const char *value) {
    http_header_item_t *item = gh_calloc(1, sizeof(*item));
    item->k = key;
    item->v = value;
    list_add_tail(&item->next, &headers->h);
    return 0;
}

static int http_headers_set_header(http_headers_t *headers, const char *key, const char *value) {
    http_header_item_t *item = http_headers_get_header(headers, key);
    if (!item) {
        item = gh_calloc(1, sizeof(*item));
        item->k = gh_strdupx(NULL, key);
        item->v = gh_strdupx(NULL, value);
        list_add_tail(&item->next, &headers->h);
    } else {
        gh_strdupx(&item->v, value);
    }
    return 0;
}

static int http_headers_clear_header(http_headers_t *headers, const char *key) {
    http_header_item_t *item = http_headers_get_header(headers, key);
    if (item) {
        list_del(&item->next);
        gh_free(item->k);
        gh_free(item->v);
        gh_free(item);
    }
    return 0;
}

static void http_headers_cleanup(http_headers_t *headers) {
    http_header_item_t *item, *item2;
    list_for_each_entry_safe(item, item2, &headers->h, next) {
        list_del(&item->next);
        gh_free(item->k);
        gh_free(item->v);
        gh_free(item);
    }
}

static void http_headers_destroy(http_headers_t *headers) {
    http_headers_cleanup(headers);
}
//===========================================================================

typedef struct {
    char *url;
    char *scheme;
    char *host;
    int port;
    char *path;
    char *query;
    webclient_method_t method;
} connection_info_t;

typedef struct {
    //The HTTP data received from the server
    char buf[DEFAULT_DATA_BUF];
    //buf中剩余数据
    int left;
    //The HTTP data after decoding
    char *raw_data;
    //The HTTP data len after decoding
    int raw_len;
    //The destination address of the data to be copied to after decoding
    char *output_ptr;
} data_buf_t;

typedef struct {
    //发送的总字节数(包括请求行、请求头、body)
    int all_bytes;
    //已经发送的body长度
    int body_alreay_sent;
} request_info_t;

typedef struct {
    int status_code;
    //已经接收的总字节数
    int all_bytes;
    //已经收到的body长度
    int body_alreay_recved;     //已经收到的body数据长度
    int body_alreay_recved2;    //chunked编码时body_alreay_recved2是解码后的数据的长度
    bool enable_drop_data;
} response_info_t;

struct webclient {
    connection_info_t cinfo;
    data_buf_t dbuf;
    request_info_t request_info;
    response_info_t response_info;
    http_headers_t request_headers;
    http_headers_t response_headers;
    webclient_transport_list_t *transport_list;
    webclient_transport_t *transport;

    http_parser_settings settings;
    http_parser parser;

    char *current_header_key;
    char *current_header_value;


    webclient_state_t state;

    int is_chunked:1;
    int is_chunk_complete:1;
    int all_headers_recved:1;
    long content_length;
};

static void clear_cinfo(webclient_t *obj) {
    gh_free(obj->cinfo.url);
    gh_free(obj->cinfo.scheme);
    gh_free(obj->cinfo.host);
    gh_free(obj->cinfo.path);
    gh_free(obj->cinfo.query);
    memset(&obj->cinfo, 0, sizeof(connection_info_t));
}

static int on_url(http_parser *hp, const char *at, size_t length) {
#if 0
    webclient_t *obj = hp->data;
    XLOG_D("OK");
#endif
    return 0;
}

static int on_status(http_parser *hp, const char *at, size_t length) {
    webclient_t *obj = hp->data;
    XLOG_D("%.*s", length, at);
    return 0;
}

/*
 * 注意
 * 接收处理中已经保证接收到一行才调用http_parser_execute，所以
 * on_header_field/on_header_value回调里面都是完整的数据
 */
static int on_header_field(http_parser *hp, const char *at, size_t length) {
    webclient_t *obj = hp->data;

    if (obj->current_header_key) {
        gh_free(obj->current_header_key);
        obj->current_header_key = NULL;
    }
    obj->current_header_key = gh_strndup(at, length);
    XLOG_D("key:[%s]", obj->current_header_key);
    return 0;
}

static int on_header_value(http_parser *hp, const char *at, size_t length) {
    webclient_t *obj = hp->data;
    if (!obj->current_header_key) return 0;
    if (0 == strcasecmp(obj->current_header_key, "Transfer-Encoding") &&
            0 == strncasecmp("chunked", at, length)) {
        obj->is_chunked = 1;
    }
    obj->current_header_value = gh_strndup(at, length);
    XLOG_D("value:[%s]", obj->current_header_value);

    http_headers_set_header2(&obj->response_headers, obj->current_header_key, obj->current_header_value);

    obj->current_header_key = NULL;
    obj->current_header_value = NULL;
    return 0;
}

static int on_body(http_parser *hp, const char *at, size_t length) {
    webclient_t *obj = hp->data;
    obj->dbuf.raw_data = at;
    if (obj->dbuf.output_ptr) {
        memcpy(obj->dbuf.output_ptr, at, length);
        obj->dbuf.output_ptr += length;
    }
    obj->dbuf.raw_len += length;

    obj->response_info.body_alreay_recved2 += length;
    return 0;
}

static int on_message_begin(http_parser *hp) {
    //webclient_t *obj = hp->data;
    return 0;
}

static int on_headers_complete(http_parser *hp) {
    webclient_t *obj = hp->data;
    obj->all_headers_recved = 1;
    obj->content_length = hp->content_length;
    obj->response_info.status_code = hp->status_code;
    return 0;
}

static int on_message_complete(http_parser *hp) {
    webclient_t *obj = hp->data;
    obj->state = WEBCLIENT_STATE_FINISHED;
    obj->is_chunk_complete = 1;
    return 0;
}

static int on_chunk_header(http_parser *hp) {
    webclient_t *obj = hp->data;
    XLOG_D("chunk size: %lld", hp->content_length);
    return 0;
}

static int on_chunk_complete(http_parser *hp) {
    //webclient_t *obj = hp->data;
    XLOG_D("on chunk complete");
    return 0;
}

webclient_t *webclient_init(webclient_cfg_t *cfg) {
    if (!cfg->url) return NULL;
    
    webclient_t *obj = gh_calloc(1, sizeof(*obj));
    if (!obj) return NULL;

    {
        http_parser_settings_init(&obj->settings);
        obj->settings.on_message_begin = on_message_begin;
        obj->settings.on_url = on_url;
        obj->settings.on_status = on_status;
        obj->settings.on_header_field = on_header_field;
        obj->settings.on_header_value = on_header_value;
        obj->settings.on_headers_complete = on_headers_complete;
        obj->settings.on_body = on_body;
        obj->settings.on_message_complete = on_message_complete;
        obj->settings.on_chunk_header = on_chunk_header;
        obj->settings.on_chunk_complete = on_chunk_complete;
    }
    obj->parser.data = obj;

    http_headers_init(&obj->request_headers);
    http_headers_init(&obj->response_headers);

    obj->transport_list = webclient_transport_list_init();
    webclient_transport_t *tcp = transport_tcp_init();
    webclient_transport_set_port(tcp, DEFAULT_HTTP_PORT);
    webclient_transport_list_add(obj->transport_list, "http", tcp);

#ifdef WEBCLIENT_ENABLE_SSL
    webclient_transport_t *ssl = transprot_ssl_init();
    webclient_transport_set_port(ssl, DEFAULT_HTTPS_PORT);
    webclient_transport_list_add(obj->transport_list, "https", ssl);
    
    if (cfg->cert) {
        transport_ssl_set_cert_data(ssl, cfg->cert, strlen(cfg->cert));
    }
    if (cfg->client_cert) {
        transport_ssl_set_client_cert_data(ssl, cfg->client_cert, strlen(cfg->client_cert));
    }
    if (cfg->client_key) {
        transport_ssl_set_client_key_data(ssl, cfg->client_key, strlen(cfg->client_key));
    }
    if (cfg->cert_common_name_check) {
        transport_ssl_common_name_check(ssl);
    }
#endif


    obj->cinfo.method = cfg->method;

    obj->state = WEBCLIENT_STATE_INIT;

    if (WEBCLIENT_ERR_OK != webclient_set_url(obj, cfg->url)) {
        goto error_exit;
    }

    return obj;
error_exit:
    webclient_destroy(obj);
    return NULL;
}

webclient_state_t webclient_get_state(webclient_t *obj) {
    return obj->state;
}

webclient_err_t webclient_set_url(webclient_t *obj, const char *url) {
    if (!url) return WEBCLIENT_ERR_ERROR;
    //http_parser_parse_url送入的url必须有scheme部分
    char *url2;
    if (0 == strncmp(url, "http://", 7) || 0 == strncmp(url, "https://", 8)) {
        url2 = gh_strdupx(NULL, url);
    } else {
        url2 = do_strcat("http://", url);
    }
    struct http_parser_url purl;
    http_parser_url_init(&purl);
    if (0 != http_parser_parse_url(url2, strlen(url2), 0, &purl)) {
        XLOG_W("invalid url: %s", url2);
        goto error_exit;
    }
    char *old_host = obj->cinfo.host;
    int old_port = obj->cinfo.port;
    if (purl.field_data[UF_HOST].len <= 0) {
        XLOG_W("invalid host: %s", url2);
        goto error_exit;
    }
    obj->cinfo.host = gh_strndup(url2 + purl.field_data[UF_HOST].off, purl.field_data[UF_HOST].len);
    if (!obj->cinfo.host) {
        obj->cinfo.host = old_host;
        goto error_exit;
    }
        
    if (old_host && 
            0 != strcmp(old_host, obj->cinfo.host)) {
        //host不一致需要关闭连接
        webclient_close(obj);
    }
    gh_free(old_host);

    if (purl.field_data[UF_SCHEMA].len) {
        gh_free(obj->cinfo.scheme);
        obj->cinfo.scheme = gh_strndup(url2 + purl.field_data[UF_SCHEMA].off, purl.field_data[UF_SCHEMA].len);
        if (0 == strcmp(obj->cinfo.scheme, "http")) {
            obj->cinfo.port = DEFAULT_HTTP_PORT;
        } else {
            obj->cinfo.port = DEFAULT_HTTPS_PORT;
        }
    }

    if (purl.field_data[UF_PORT].len) {
        obj->cinfo.port = strtol((const char*)(url2 + purl.field_data[UF_PORT].off), NULL, 10);
    }
    if (old_port != obj->cinfo.port) {
        //端口号不一致需要关闭连接
        webclient_close(obj);
    }

    gh_free(obj->cinfo.path);
    if (purl.field_data[UF_PATH].len) {
        obj->cinfo.path = gh_strndup(url2 + purl.field_data[UF_PATH].off, purl.field_data[UF_PATH].len);
    } else {
        obj->cinfo.path = gh_strdupx(NULL, "/");
    }

    gh_free(obj->cinfo.query);
    obj->cinfo.query = NULL;
    if (purl.field_data[UF_QUERY].len) {
        obj->cinfo.query = gh_strndup(url2 + purl.field_data[UF_QUERY].off, purl.field_data[UF_QUERY].len);
    }
    XLOG_I("host: [%s], port: [%d], path: [%s], query: [%s]",
            obj->cinfo.host, obj->cinfo.port, obj->cinfo.path, obj->cinfo.query ? obj->cinfo.query : "");

    gh_free(obj->cinfo.url);
    obj->cinfo.url = url2;

    http_headers_cleanup(&obj->request_headers);
    //添加默认的http header
    http_headers_set_header(&obj->request_headers, "Host", obj->cinfo.host);
    http_headers_set_header(&obj->request_headers, "User-Agent", DEFAULT_HTTP_USER_AGENT);
    http_headers_set_header(&obj->request_headers, "Content-Length", "0");
    return WEBCLIENT_ERR_OK;

error_exit:
    gh_free(url2);
    return WEBCLIENT_ERR_ERROR;
}

const char *webclient_get_url(webclient_t *obj) {
    return obj->cinfo.url;
}

webclient_err_t webclient_set_header(webclient_t *obj, const char *key, const char *value) {
    http_headers_set_header(&obj->request_headers, key, value);
    return WEBCLIENT_ERR_OK;
}

static const char *method_tables[WEBCLIENT_METHOD_MAX] = {
    [WEBCLIENT_METHOD_GET] = "GET",
    [WEBCLIENT_METHOD_POST] = "POST",
};

webclient_err_t webclient_set_method(webclient_t *obj, webclient_method_t method) {
    obj->cinfo.method = method;
    return WEBCLIENT_TRANS_OK;
}

webclient_err_t webclient_prepared(webclient_t *obj) {
    if (obj->cinfo.query) {
        obj->dbuf.left = snprintf(obj->dbuf.buf, DEFAULT_DATA_BUF, "%s %s?%s HTTP/1.1\r\n", method_tables[obj->cinfo.method],
                obj->cinfo.path,
                obj->cinfo.query);
    } else {
        obj->dbuf.left = snprintf(obj->dbuf.buf, DEFAULT_DATA_BUF, "%s %s HTTP/1.1\r\n", method_tables[obj->cinfo.method],
                obj->cinfo.path);
    }
    XLOG_D("%s", obj->dbuf.buf);
    http_parser_init(&obj->parser, HTTP_RESPONSE);
    obj->state = WEBCLIENT_STATE_PREPARED;
    obj->all_headers_recved = 0;
    obj->is_chunked = 0;
    obj->is_chunk_complete = 0;

    obj->request_info.all_bytes = 0;
    obj->request_info.body_alreay_sent = 0;

    obj->response_info.all_bytes = 0;
    obj->response_info.body_alreay_recved = 0;
    obj->response_info.body_alreay_recved2 = 0;
    obj->response_info.status_code = 0;
    obj->response_info.enable_drop_data = false;
    obj->dbuf.output_ptr = NULL;
    obj->dbuf.raw_len = 0;
    http_headers_cleanup(&obj->response_headers);
    return WEBCLIENT_TRANS_OK;
}

webclient_err_t webclient_connect(webclient_t *obj, int timeout) {
    if (obj->state != WEBCLIENT_STATE_PREPARED) return WEBCLIENT_ERR_ERROR;
    XLOG_I("connecting to %s:%d", obj->cinfo.host, obj->cinfo.port);
    obj->transport = webclient_transport_list_get_transport(obj->transport_list, obj->cinfo.scheme);
    if (!obj->transport) {
        XLOG_W("not found %s", obj->cinfo.scheme);
        return WEBCLIENT_ERR_ERROR;
    }
    webclient_trans_err_t err = webclient_transport_connect(obj->transport, obj->cinfo.host, obj->cinfo.port, timeout);
    if (err == WEBCLIENT_TRANS_ERROR) return WEBCLIENT_ERR_ERROR;
    if (err == WEBCLIENT_TRANS_TIMEOUT) return WEBCLIENT_ERR_TIMEOUT;
    obj->state = WEBCLIENT_STATE_CONNECTED;
    XLOG_I("connected");
    return WEBCLIENT_ERR_OK;
}

webclient_err_t webclient_send_header(webclient_t *obj, int timeout) {
    if (obj->state != WEBCLIENT_STATE_CONNECTED) return WEBCLIENT_ERR_ERROR;
    webclient_trans_err_t err;
    int wlen;
    do {
        if (obj->dbuf.left > 0) {
            //继续发送
            err = webclient_transport_write(obj->transport, obj->dbuf.buf, obj->dbuf.left, timeout, &wlen);
            if (err == WEBCLIENT_TRANS_OK) {
                //发送了部分数据
                obj->dbuf.left -= wlen;
                if (obj->dbuf.left > 0) {
                    memmove(obj->dbuf.buf, obj->dbuf.buf + wlen, obj->dbuf.left);
                    continue;
                }
            } else if (err == WEBCLIENT_TRANS_TIMEOUT) {
                return WEBCLIENT_ERR_TIMEOUT;
            } else if (err == WEBCLIENT_TRANS_ERROR) {
                return WEBCLIENT_ERR_ERROR;
            }
        }
        //get next header
        http_header_item_t *item = http_headers_detach_header(&obj->request_headers);
        if (!item) {
            obj->state = WEBCLIENT_STATE_READY_SEND_BODY;
            return WEBCLIENT_ERR_OK;
        }

        obj->dbuf.left = snprintf(obj->dbuf.buf, DEFAULT_DATA_BUF, "%s: %s\r\n%s",
                item->k, item->v,
                http_headers_is_empty(&obj->request_headers) ? "\r\n" : "");
        gh_free(item->k);
        gh_free(item->v);
        gh_free(item);
    } while (1);
}

webclient_err_t webclient_send_body(webclient_t *obj, const void *buf, int len, int timeout, int *wlen) {
    if (obj->state != WEBCLIENT_STATE_READY_SEND_BODY) return WEBCLIENT_ERR_ERROR;

    
    if (len == 0)  {
        obj->state = WEBCLIENT_STATE_READY_RECV_HEADER;
        return WEBCLIENT_ERR_OK;
    }
    webclient_trans_err_t err = webclient_transport_write(obj->transport, buf, len, timeout, wlen); 
    if (err == WEBCLIENT_TRANS_ERROR) return WEBCLIENT_ERR_ERROR;
    if (err == WEBCLIENT_TRANS_TIMEOUT) return WEBCLIENT_ERR_TIMEOUT;
    return WEBCLIENT_ERR_OK;
}

/*
 * 一般情况下都是以"\r\n"结尾，但是不排除只有"\n"的情况
 */
static int check_one_line(const char *s, int buf_len) {
    const unsigned char *buf = s;
    int i;
    for (i = 0; i < buf_len; i++) {
        if (!isprint(buf[i]) && buf[i] != '\r' && buf[i] != '\n' && buf[i] < 128) {
            return -1;
        } else if (buf[i] == '\r' && i + 1 < buf_len && buf[i + 1] == '\n') {
            return i + 2;
        } else if (buf[i] == '\n') {
            return i + 1;
        }
    }
    return 0;
}

webclient_err_t webclient_read_header(webclient_t *obj, int timeout) {
    if (obj->state != WEBCLIENT_STATE_READY_RECV_HEADER) return WEBCLIENT_ERR_ERROR;
    webclient_trans_err_t err;
    int bytes;
    while (!obj->all_headers_recved) {
        bytes = 0;
        char *pbuf = obj->dbuf.buf + obj->dbuf.left;
        err = webclient_transport_read(obj->transport, pbuf, DEFAULT_DATA_BUF - obj->dbuf.left, timeout, &bytes); 
        if (err == WEBCLIENT_TRANS_TIMEOUT) return WEBCLIENT_ERR_TIMEOUT;
        if (err == WEBCLIENT_TRANS_ERROR) return WEBCLIENT_ERR_ERROR;
        obj->response_info.all_bytes += bytes;
        obj->dbuf.left += bytes;
        do {
            //一行一行送入parser
            int count = check_one_line(obj->dbuf.buf, obj->dbuf.left);
            if (count == -1) {
                XLOG_W("invalid formate");
                return WEBCLIENT_ERR_ERROR;
            } else if (count == 0) {
                //数据不足
                if (obj->dbuf.left == DEFAULT_DATA_BUF) {
                    XLOG_W("too long one line");
                    return WEBCLIENT_ERR_ERROR;
                }
                break;
            } else {
                int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, count);
                if (parserd != count) {
                    XLOG_W("parse error");
                    return WEBCLIENT_ERR_ERROR;
                }
                obj->dbuf.left -= count;
                memmove(obj->dbuf.buf, obj->dbuf.buf + count, obj->dbuf.left);
            }
        } while (!obj->all_headers_recved);
    };
    XLOG_I("parse headers finished, left bytes: %d", obj->dbuf.left);
    obj->state = WEBCLIENT_STATE_READY_RECV;
    return WEBCLIENT_ERR_OK;
}

int webclient_get_status_code(webclient_t *obj) {
    return obj->response_info.status_code;
}

int webclient_get_content_length(webclient_t *obj) {
    if (obj->is_chunked) return -1;
    return obj->content_length;
}

webclient_err_t webclient_drop_response(webclient_t *obj, int timeout) {
    if (obj->state == WEBCLIENT_STATE_FINISHED) return WEBCLIENT_ERR_FINISHED;
    if (obj->state != WEBCLIENT_STATE_READY_RECV_HEADER && obj->state != WEBCLIENT_STATE_READY_RECV) return WEBCLIENT_ERR_ERROR;
    obj->response_info.enable_drop_data = true;
    webclient_trans_err_t err;
    int bytes;
    int len = 1024;
    while (1) {
        if (obj->all_headers_recved) {
            int ridx = 0;
            if (obj->dbuf.raw_len) {
                int left = obj->dbuf.raw_len;
                if (left > len) {
                    left = len;
                }
                obj->dbuf.raw_len -= left;
                obj->dbuf.raw_data += left;
                ridx = left;
            }
            int need_read = len - ridx;
            if (need_read == 0) {
                return WEBCLIENT_ERR_OK;
            }

            bool is_data_remain = true;
            while (need_read > 0 && is_data_remain) {
                if (obj->is_chunked) {
                    is_data_remain = !obj->is_chunk_complete;
                } else {
                    is_data_remain = obj->response_info.body_alreay_recved < obj->content_length;
                }
                XLOG_D("is_chunk_complete: %d, body_alreay_recved: %d, body_alreay_recved2: %d", obj->is_chunk_complete ? 1 : 0, obj->response_info.body_alreay_recved, obj->response_info.body_alreay_recved2);
                if (!is_data_remain) {
                    return WEBCLIENT_ERR_FINISHED;
                }
                int byte_to_read = need_read;
                if (byte_to_read > DEFAULT_DATA_BUF) {
                    byte_to_read = DEFAULT_DATA_BUF;
                }
                if (obj->dbuf.left) {
                    //有数据未parse
                    if (byte_to_read > obj->dbuf.left) {
                        byte_to_read  = obj->dbuf.left;
                    }
                    int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, byte_to_read);
                    XLOG_D("byte_to_read: %d, parserd: %d", byte_to_read, parserd);
                    if (parserd != byte_to_read) {
                        XLOG_W("parse error");
                        return WEBCLIENT_ERR_ERROR;
                    }
                    obj->response_info.body_alreay_recved += byte_to_read;
                    obj->dbuf.left -= parserd;
                    if (obj->dbuf.left) {
                        //TODO
                        memmove(obj->dbuf.buf, obj->dbuf.buf + parserd, obj->dbuf.left);
                    }
                    ridx += obj->dbuf.raw_len;
                    need_read -= obj->dbuf.raw_len;
                    obj->dbuf.raw_len = 0;
                    obj->dbuf.output_ptr = NULL;
                } else {
                    bytes = 0;
                    err = webclient_transport_read(obj->transport, obj->dbuf.buf, byte_to_read, timeout, &bytes); 
                    if (err == WEBCLIENT_TRANS_TIMEOUT) {
                        return WEBCLIENT_ERR_TIMEOUT;
                    } else if (err == WEBCLIENT_TRANS_ERROR) {
                        XLOG_W("read error");
                        return WEBCLIENT_ERR_ERROR;
                    }

                    obj->response_info.all_bytes += bytes;
                    int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, bytes);
                    if (parserd != bytes) {
                        XLOG_W("parse error");
                        return WEBCLIENT_ERR_ERROR;
                    }
                    obj->response_info.body_alreay_recved += bytes;
                    ridx += obj->dbuf.raw_len;
                    need_read -= obj->dbuf.raw_len;
                    obj->dbuf.raw_len = 0;
                    obj->dbuf.output_ptr = NULL;
                }
            }
            break;
        } else {
            //一行一行读取头部
            do {
                bytes = 0;
                char *pbuf = obj->dbuf.buf + obj->dbuf.left;
                err = webclient_transport_read(obj->transport, pbuf, DEFAULT_DATA_BUF - obj->dbuf.left, timeout, &bytes); 
                if (err == WEBCLIENT_TRANS_TIMEOUT) return WEBCLIENT_ERR_TIMEOUT;
                if (err == WEBCLIENT_TRANS_ERROR) return WEBCLIENT_ERR_ERROR;
                obj->response_info.all_bytes += bytes;
                obj->dbuf.left += bytes;
                do {
                    //一行一行送入parser
                    int count = check_one_line(obj->dbuf.buf, obj->dbuf.left);
                    if (count == -1) {
                        XLOG_W("invalid formate");
                        return WEBCLIENT_ERR_ERROR;
                    } else if (count == 0) {
                        //数据不足
                        if (obj->dbuf.left == DEFAULT_DATA_BUF) {
                            XLOG_W("too long one line");
                            return WEBCLIENT_ERR_ERROR;
                        }
                        break;
                    } else {
                        int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, count);
                        if (parserd != count) {
                            XLOG_W("parse error");
                            return WEBCLIENT_ERR_ERROR;
                        }
                        obj->dbuf.left -= count;
                        memmove(obj->dbuf.buf, obj->dbuf.buf + count, obj->dbuf.left);
                    }
                } while (!obj->all_headers_recved);
            } while (!obj->all_headers_recved);
            XLOG_I("parse headers finished, left bytes: %d", obj->dbuf.left);
            obj->state = WEBCLIENT_STATE_READY_RECV;
        }
    }
    return WEBCLIENT_ERR_OK;
}

webclient_err_t webclient_read(webclient_t *obj, char *buf, int len, int timeout, int *rlen) {
    *rlen = 0;
    if (obj->response_info.enable_drop_data) return WEBCLIENT_ERR_ERROR;
    if (obj->state == WEBCLIENT_STATE_FINISHED) return WEBCLIENT_ERR_FINISHED;
    if (obj->state != WEBCLIENT_STATE_READY_RECV_HEADER && obj->state != WEBCLIENT_STATE_READY_RECV) return WEBCLIENT_ERR_ERROR;

    webclient_trans_err_t err;
    int bytes;
    while (1) {
        if (obj->all_headers_recved) {
            int ridx = 0;
            if (obj->dbuf.raw_len) {
                int left = obj->dbuf.raw_len;
                if (left > len) {
                    left = len;
                }
                memcpy(buf, obj->dbuf.raw_data, left);
                obj->dbuf.raw_len -= left;
                obj->dbuf.raw_data += left;
                ridx = left;
            }
            int need_read = len - ridx;
            if (need_read == 0) {
                *rlen = len;
                return WEBCLIENT_ERR_OK;
            }

            bool is_data_remain = true;
            while (need_read > 0 && is_data_remain) {
                if (obj->is_chunked) {
                    is_data_remain = !obj->is_chunk_complete;
                } else {
                    is_data_remain = obj->response_info.body_alreay_recved < obj->content_length;
                }
                XLOG_D("is_chunk_complete: %d, body_alreay_recved: %d, body_alreay_recved2: %d", obj->is_chunk_complete ? 1 : 0, obj->response_info.body_alreay_recved, obj->response_info.body_alreay_recved2);
                if (!is_data_remain) {
                    *rlen = ridx;
                    if (ridx) {
                        return WEBCLIENT_ERR_OK;
                    } else {
                        return WEBCLIENT_ERR_FINISHED;
                    }
                }
                int byte_to_read = need_read;
                if (byte_to_read > DEFAULT_DATA_BUF) {
                    byte_to_read = DEFAULT_DATA_BUF;
                }
                if (obj->dbuf.left) {
                    //有数据未parse
                    if (byte_to_read > obj->dbuf.left) {
                        byte_to_read  = obj->dbuf.left;
                    }
                    obj->dbuf.output_ptr = buf + ridx;
                    int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, byte_to_read);
                    XLOG_D("byte_to_read: %d, parserd: %d", byte_to_read, parserd);
                    if (parserd != byte_to_read) {
                        XLOG_W("parse error");
                        return WEBCLIENT_ERR_ERROR;
                    }
                    obj->response_info.body_alreay_recved += byte_to_read;
                    obj->dbuf.left -= parserd;
                    if (obj->dbuf.left) {
                        //TODO
                        memmove(obj->dbuf.buf, obj->dbuf.buf + parserd, obj->dbuf.left);
                    }
                    ridx += obj->dbuf.raw_len;
                    need_read -= obj->dbuf.raw_len;
                    obj->dbuf.raw_len = 0;
                    obj->dbuf.output_ptr = NULL;
                    *rlen = ridx;
                } else {
                    bytes = 0;
                    err = webclient_transport_read(obj->transport, obj->dbuf.buf, byte_to_read, timeout, &bytes); 
                    if (err == WEBCLIENT_TRANS_TIMEOUT) {
                        *rlen = ridx;
                        if (ridx) {
                            return WEBCLIENT_ERR_OK;
                        } else {
                            return WEBCLIENT_ERR_TIMEOUT;
                        }
                    } else if (err == WEBCLIENT_TRANS_ERROR) {
                        XLOG_W("read error");
                        return WEBCLIENT_ERR_ERROR;
                    }

                    obj->response_info.all_bytes += bytes;
                    obj->dbuf.output_ptr = buf + ridx;
                    int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, bytes);
                    if (parserd != bytes) {
                        XLOG_W("parse error");
                        return WEBCLIENT_ERR_ERROR;
                    }
                    obj->response_info.body_alreay_recved += bytes;
                    ridx += obj->dbuf.raw_len;
                    need_read -= obj->dbuf.raw_len;
                    obj->dbuf.raw_len = 0;
                    obj->dbuf.output_ptr = NULL;
                    *rlen = ridx;
                }
            }
            break;
        } else {
            //一行一行读取头部
            do {
                bytes = 0;
                char *pbuf = obj->dbuf.buf + obj->dbuf.left;
                err = webclient_transport_read(obj->transport, pbuf, DEFAULT_DATA_BUF - obj->dbuf.left, timeout, &bytes); 
                if (err == WEBCLIENT_TRANS_TIMEOUT) return WEBCLIENT_ERR_TIMEOUT;
                if (err == WEBCLIENT_TRANS_ERROR) return WEBCLIENT_ERR_ERROR;
                obj->response_info.all_bytes += bytes;
                obj->dbuf.left += bytes;
                do {
                    //一行一行送入parser
                    int count = check_one_line(obj->dbuf.buf, obj->dbuf.left);
                    if (count == -1) {
                        XLOG_W("invalid formate");
                        return WEBCLIENT_ERR_ERROR;
                    } else if (count == 0) {
                        //数据不足
                        if (obj->dbuf.left == DEFAULT_DATA_BUF) {
                            XLOG_W("too long one line");
                            return WEBCLIENT_ERR_ERROR;
                        }
                        break;
                    } else {
                        int parserd = http_parser_execute(&obj->parser, &obj->settings, obj->dbuf.buf, count);
                        if (parserd != count) {
                            XLOG_W("parse error");
                            return WEBCLIENT_ERR_ERROR;
                        }
                        obj->dbuf.left -= count;
                        memmove(obj->dbuf.buf, obj->dbuf.buf + count, obj->dbuf.left);
                    }
                } while (!obj->all_headers_recved);
            } while (!obj->all_headers_recved);
            XLOG_I("parse headers finished, left bytes: %d", obj->dbuf.left);
            obj->state = WEBCLIENT_STATE_READY_RECV;
        }
    }
    return WEBCLIENT_ERR_OK;
}

const char *webclient_get_request_header(webclient_t *obj, const char *key) {
    http_header_item_t *item = http_headers_get_header(&obj->request_headers, key);
    if (item && item->v) return item->v;
    return NULL;
}

const char *webclient_get_response_header(webclient_t *obj, const char *key) {
    http_header_item_t *item = http_headers_get_header(&obj->response_headers, key);
    if (item && item->v) return item->v;
    return NULL;
}


void webclient_close(webclient_t *obj) {
    http_headers_cleanup(&obj->request_headers);
    http_headers_cleanup(&obj->response_headers);
    webclient_transport_close(obj->transport);
    obj->state = WEBCLIENT_STATE_INIT;
}

void webclient_destroy(webclient_t *obj) {
    if (!obj) return;
    webclient_close(obj);
    webclient_transport_list_destroy(obj->transport_list);
    clear_cinfo(obj);
    gh_free(obj);
}
