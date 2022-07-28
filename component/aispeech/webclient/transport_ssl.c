#include "transport_ssl.h"
#include "transport_tcp.h"

#include "gh.h"

#include "mbedtls/platform.h"                                                      
#include "mbedtls/net_sockets.h"                                                   
#include "mbedtls/ssl.h"                                                           
#include "mbedtls/entropy.h"                                                       
#include "mbedtls/ctr_drbg.h"                                                      
#include "mbedtls/error.h"                                                         
#include "mbedtls/certs.h"   


#define XLOG_TAG "ssl"
#define XLOG_LVL XLOG_LEVEL_INFO
#include "xlog.h"

typedef struct {
    const char *cacert;
    int cacert_len;
    const char *client_cert;
    int client_cert_len;
    const char *client_key;
    int client_key_len;
    const char *client_key_password;
    int client_key_password_len;
    const char *common_name;
    bool common_name_check;
} tls_cfg_t;

typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;
} tls_t;

typedef struct {
    mbedtls_x509_crt *public_cert;
    mbedtls_pk_context *pk_key;
    const unsigned char *publiccert_pem_buf;
    unsigned int publiccert_pem_bytes;
    const unsigned char *privkey_pem_buf;
    unsigned int privkey_pem_bytes;
    const unsigned char *privkey_password;
    unsigned int privkey_password_len;
}tls_pki_t;

typedef struct ssl_transport {
    int is_connected:1;
    int is_handshaking:1;
    int is_handshake_done:1;
    tls_cfg_t cfg;
    tls_t *tls;
    tcp_transport_t *tcp;
} ssl_transport_t;


tls_t *tls_init() {
    tls_t *tls = gh_calloc(1, sizeof(*tls));
    if (tls) {
        mbedtls_net_init(&tls->server_fd);
        mbedtls_ssl_init(&tls->ssl);
        mbedtls_ctr_drbg_init(&tls->ctr_drbg);
        mbedtls_ssl_config_init(&tls->conf);
        mbedtls_entropy_init(&tls->entropy);
        mbedtls_x509_crt_init(&tls->cacert);
        mbedtls_x509_crt_init(&tls->client_cert);
    }
    return tls;
}

static int set_pki_context(tls_t *tls, const tls_pki_t *pki) {
    int ret;
    if (pki->publiccert_pem_buf &&
        pki->privkey_pem_buf &&
        pki->public_cert &&
        pki->pk_key) {
        mbedtls_x509_crt_init(pki->public_cert);
        mbedtls_pk_init(pki->pk_key);
        ret = mbedtls_x509_crt_parse(pki->public_cert, pki->publiccert_pem_buf, pki->publiccert_pem_bytes);
        if (ret < 0) {
            XLOG_W("mbedtls_x509_crt_parse error: 0x%x", -ret);
            return -1;
        }
        if (!pki->privkey_pem_buf) return -1;
        ret = mbedtls_pk_parse_key(pki->pk_key,
                pki->privkey_pem_buf,
                pki->privkey_pem_bytes,
                pki->privkey_password,
                pki->privkey_password_len);
        if (ret < 0) {
            XLOG_W("mbedtls_pk_parse_keyfile error: 0x%x", -ret);
            return -1;
        }
        ret = mbedtls_ssl_conf_own_cert(&tls->conf, pki->public_cert, pki->pk_key);
        if (ret < 0) {
            XLOG_W("mbedtls_ssl_conf_own_cert error: 0x%x", -ret);
            return -1;
        }
    } else {
        return -1;
    }
    return 0;
}

static int set_client_cfg(const char *hostname, tls_cfg_t *cfg, tls_t *tls) {
    int ret;
    if (cfg->common_name_check) {
        char *host = hostname;
        if (cfg->common_name) {
            host = cfg->common_name;
        }
        if (0 != (ret = mbedtls_ssl_set_hostname(&tls->ssl, host))) {
            XLOG_W("mbedtls_ssl_set_hostname error: 0x%x", -ret);
            return -1;
        }
    }
    if (0 != (ret = mbedtls_ssl_config_defaults(&tls->conf,
                    MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT))) {
        XLOG_W("mbedtls_ssl_config_defaults error: 0x%x", -ret);
        return -1;
    }
    mbedtls_ssl_conf_renegotiation(&tls->conf, MBEDTLS_SSL_RENEGOTIATION_ENABLED);
    if (cfg->cacert) {
        ret = mbedtls_x509_crt_parse(&tls->cacert, cfg->cacert, cfg->cacert_len);
        if (ret < 0) {
            XLOG_W("mbedtls_x509_crt_parse error: 0x%x", -ret);
            return -1;
        }
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->cacert, NULL);
    } else {
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
    }
    if (cfg->client_cert && cfg->client_key) {
        tls_pki_t pki = {
            .public_cert = &tls->client_cert,
            .pk_key = &tls->client_key,
            .publiccert_pem_buf = cfg->client_cert,
            .publiccert_pem_bytes = cfg->client_cert_len,
            .privkey_pem_buf = cfg->client_key,
            .privkey_pem_bytes = cfg->client_key_len,
            .privkey_password = cfg->client_key_password,
            .privkey_password_len = cfg->client_key_password_len,
        };
        if (0 != set_pki_context(tls, &pki)) {
            return -1;
        }
    } else if (cfg->client_cert || cfg->client_key) {
        XLOG_W("must provide both client_cert and client_key for mutual authentication");
        return -1;
    }
    return 0;
}

void tls_destroy(tls_t *tls) {
    if (!tls) return;
    mbedtls_x509_crt_free(&tls->cacert);
    mbedtls_x509_crt_free(&tls->client_cert);
    mbedtls_pk_free(&tls->client_key);
    mbedtls_entropy_free(&tls->entropy);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ctr_drbg_free(&tls->ctr_drbg);
    mbedtls_ssl_free(&tls->ssl);
    gh_free(tls);
}

static webclient_trans_err_t ssl_connect(webclient_transport_t *t, const char *host, int port, int timeout) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    webclient_trans_err_t err; 
    int ret;
    if (ssl->is_handshake_done) {
        XLOG_W("already connected");
        return WEBCLIENT_TRANS_OK;
    }
    if (!ssl->is_connected) {
        err = tcp_transport_connect(ssl->tcp, host, port, timeout);
        if (err != WEBCLIENT_TRANS_OK) return err;
        XLOG_W("tcp connected");
        ssl->is_connected = 1;
        ssl->is_handshaking = 1;
        if (!ssl->tls) {
            tls_t *tls = tls_init();
            tls->server_fd.fd = tcp_transport_get_fd(ssl->tcp);
            if (0 != set_client_cfg(host, &ssl->cfg, tls)) {
                tls_destroy(tls);
                XLOG_W("set_client_cfg error");
                return WEBCLIENT_TRANS_ERROR;
            }
            if (0 != (ret = mbedtls_ctr_drbg_seed(&tls->ctr_drbg,
                            mbedtls_entropy_func, &tls->entropy, NULL, 0))) {
                XLOG_W("mbedtls_ctr_drbg_seed error: 0x%x", -ret);
                tls_destroy(tls);
                return WEBCLIENT_TRANS_ERROR;
            }
            mbedtls_ssl_conf_rng(&tls->conf, mbedtls_ctr_drbg_random, &tls->ctr_drbg);
            if (0 != (ret = mbedtls_ssl_setup(&tls->ssl, &tls->conf))) {
                XLOG_W("mbedtls_ssl_setup error: 0x%x", -ret);
                tls_destroy(tls);
                return WEBCLIENT_TRANS_ERROR;
            }
            mbedtls_ssl_set_bio(&tls->ssl, &tls->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
            ssl->tls = tls;
        }
    }

    if (ssl->is_handshaking) {
        do {
            XLOG_W("ssh handshaking");
            ret = mbedtls_ssl_handshake(&ssl->tls->ssl);
            if (ret == 0) {
                XLOG_W("ssl handshake done");
                ssl->is_handshaking = 0;
                ssl->is_handshake_done = 1;
                return WEBCLIENT_TRANS_OK;
            } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
                if (tcp_transport_can_read(ssl->tcp, timeout)) continue;
                return WEBCLIENT_TRANS_TIMEOUT;
            } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (tcp_transport_can_write(ssl->tcp, timeout)) continue;
                return WEBCLIENT_TRANS_TIMEOUT;
            } else {
                XLOG_W("mbedtls_ssl_handshake error: 0x%x", -ret);
                break;
            }
        } while (1);
    }
    return WEBCLIENT_TRANS_ERROR;
}

static webclient_trans_err_t ssl_read(webclient_transport_t *t, void *buf, int len, int timeout, int *rlen) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    if (!ssl->is_handshake_done) return WEBCLIENT_TRANS_ERROR;
    unsigned char *pbuf = buf;
    int plen = 0;
    while (plen < len) {
        int bytes = mbedtls_ssl_read(&ssl->tls->ssl, pbuf, len - plen);
        if (0 == bytes) {
            if (tcp_transport_can_read(ssl->tcp, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == MBEDTLS_ERR_SSL_WANT_READ) {
            if (tcp_transport_can_read(ssl->tcp, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (tcp_transport_can_write(ssl->tcp, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            XLOG_W("ssl peer close notify");
            if (plen > 0) break;
            return WEBCLIENT_TRANS_ERROR;
        } else if (bytes > 0) {
            XLOG_HEX_D("<read>: %d", pbuf, bytes, bytes);
            pbuf += bytes;
            plen += bytes; 
        } else {
            return WEBCLIENT_TRANS_ERROR;
        }
    }
    *rlen = plen;
    return WEBCLIENT_TRANS_OK;
}

static webclient_trans_err_t ssl_write(webclient_transport_t *t, const void *buf, int len, int timeout, int *wlen) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    if (!ssl->is_handshake_done) return WEBCLIENT_TRANS_ERROR;
    const unsigned char *pbuf = buf;
    int plen = 0;
    while (plen < len) {
        int bytes = mbedtls_ssl_write(&ssl->tls->ssl, pbuf, len - plen);
        if (0 == bytes) {
            if (tcp_transport_can_write(ssl->tcp, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == MBEDTLS_ERR_SSL_WANT_READ) {
            if (tcp_transport_can_read(ssl->tcp, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (tcp_transport_can_write(ssl->tcp, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes > 0) {
            XLOG_HEX_D("<write>:%d", pbuf, bytes, bytes);
            plen += bytes;
            pbuf += bytes;
        } else {
            return WEBCLIENT_TRANS_ERROR;
        }
    }
    *wlen = plen;
    return WEBCLIENT_TRANS_OK;
}

static void ssl_close(webclient_transport_t *t) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    tcp_transport_close(ssl->tcp);
    tls_destroy(ssl->tls);
    ssl->tls = NULL;
}

static void ssl_destroy(webclient_transport_t *t) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    tcp_transport_destroy(ssl->tcp);
    gh_free(ssl);
}

ssl_transport_t *ssl_transport_init() {
    ssl_transport_t *ssl = gh_calloc(1, sizeof(*ssl));
    if (ssl) {
        ssl->tcp = tcp_transport_init();
    }
    return ssl;
}

void transport_ssl_common_name_check(webclient_transport_t *t) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    ssl->cfg.common_name_check = true;
}

void transport_ssl_set_cert_data(webclient_transport_t *t, const char *data, int len) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    ssl->cfg.cacert = data;
    ssl->cfg.cacert_len = len;
}

void transport_ssl_set_client_cert_data(webclient_transport_t *t, const char *data, int len) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    ssl->cfg.client_cert = data;
    ssl->cfg.client_cert_len = len;
}

void transport_ssl_set_client_key_data(webclient_transport_t *t, const char *data, int len) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    ssl->cfg.client_key = data;
    ssl->cfg.client_key_len = len;
}

void transport_ssl_set_client_key_password(webclient_transport_t *t, const char *password, int passwordlen) {
    ssl_transport_t *ssl = webclient_transport_get_priv(t);
    ssl->cfg.client_key_password = password;
    ssl->cfg.client_key_password_len = passwordlen;
}

webclient_transport_t *transprot_ssl_init() {
    webclient_transport_t *t = webclient_transport_init();
    ssl_transport_t *ssl = ssl_transport_init();
    webclient_transport_set_io(t, ssl_connect, ssl_read, ssl_write, ssl_close, ssl_destroy);
    webclient_transport_set_priv(t, ssl);
    return t;
}
