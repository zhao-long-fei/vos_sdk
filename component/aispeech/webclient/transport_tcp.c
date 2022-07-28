#include "transport_tcp.h"

#include "gh.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define XLOG_TAG "tcp"
#define XLOG_LVL XLOG_LEVEL_INFO
#include "xlog.h"

typedef struct tcp_transport {
    int fd;
    int is_connecting:1;    //正在连接
    int is_connected:1;
} tcp_transport_t;

static int wait_connected(int fd, int timeout) {
    fd_set s, e;
    FD_ZERO(&s);
    FD_ZERO(&e);
    FD_SET(fd, &s);
    FD_SET(fd, &e);
    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = timeout % 1000 * 1000,
    };
    int ret = select(fd + 1, NULL, &s, &e, &tv);
    if (ret > 0 && (FD_ISSET(fd, &s) || FD_ISSET(fd, &e))) {
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
        if (err == 0 || err == EAGAIN || err == EWOULDBLOCK) return 0;
        return -1;
    }
    return -1;
}

static int resolve_dns(const char *host, struct sockaddr_in *ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    he = gethostbyname(host);
    if (!he) return -1;
    addr_list = (struct in_addr **)he->h_addr_list;
    if (addr_list[0] == NULL) return -1;
    ip->sin_family = AF_INET;
    memcpy(&ip->sin_addr, addr_list[0], sizeof(ip->sin_addr));
    return 0;
}

static webclient_trans_err_t tcp_connect(webclient_transport_t *t, const char *host, int port, int timeout) {
    tcp_transport_t *tcp = webclient_transport_get_priv(t);
    return tcp_transport_connect(tcp, host, port, timeout);
}

static int wait_writeable(int fd, int timeout) {
    fd_set s;
    FD_ZERO(&s);
    FD_SET(fd, &s);
    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = timeout % 1000 * 1000,
    };
    int ret = select(fd + 1, NULL, &s, NULL, &tv);
    if (ret > 0 && FD_ISSET(fd, &s)) return 0;
    return -1;
}

static int wait_readable(int fd, int timeout) {
    fd_set s;
    FD_ZERO(&s);
    FD_SET(fd, &s);
    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = timeout % 1000 * 1000,
    };
    int ret = select(fd + 1, &s, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(fd, &s)) return 0;
    return -1;
}

static webclient_trans_err_t tcp_read(webclient_transport_t *t, void *buf, int len, int timeout, int *rlen) {
    tcp_transport_t *tcp = webclient_transport_get_priv(t);
    return tcp_transport_read(tcp, buf, len, timeout, rlen);
}

static webclient_trans_err_t tcp_write(webclient_transport_t *t, const void *buf, int len, int timeout, int *wlen) {
    tcp_transport_t *tcp = webclient_transport_get_priv(t);
    return tcp_transport_write(tcp, buf, len, timeout, wlen);
}

static void tcp_close(webclient_transport_t *t) {
    tcp_transport_t *tcp = webclient_transport_get_priv(t);
    tcp_transport_close(tcp);
}

static void tcp_destroy(webclient_transport_t *t) {
    tcp_transport_t *tcp = webclient_transport_get_priv(t);
    tcp_transport_destroy(tcp);
}

tcp_transport_t *tcp_transport_init() {
    tcp_transport_t *tcp = gh_calloc(1, sizeof(*tcp));
    if (tcp) {
        tcp->fd = -1;
    }
    return tcp;
}

webclient_trans_err_t tcp_transport_connect(tcp_transport_t *tcp, const char *host, int port, int timeout) {
    if (tcp->is_connected) {
        XLOG_W("already connected");
        return WEBCLIENT_TRANS_OK;
    }

    if (-1 == tcp->fd) {
        tcp->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == tcp->fd) {
            XLOG_W("create tcp socket error");
            return WEBCLIENT_TRANS_ERROR;
        }
        //非阻塞
        int flags = fcntl(tcp->fd, F_GETFL, 0);
        fcntl(tcp->fd, F_SETFL, flags | O_NONBLOCK);
    }

    if (!tcp->is_connecting) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        //TODO
        //异步DNS实现
        if (1 != inet_pton(AF_INET, host, &addr.sin_addr)) {
            if (-1 == resolve_dns(host, &addr)) {
                return WEBCLIENT_TRANS_ERROR;
            }
        }
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        tcp->is_connecting = 1;
        if (0 == connect(tcp->fd, (struct sockaddr *)(&addr), sizeof(struct sockaddr))) {
            tcp->is_connecting = 0;
            tcp->is_connected = 1;
            return WEBCLIENT_TRANS_OK;
        } else {
            if (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN) {
                if (0 == wait_connected(tcp->fd, timeout)) {
                    tcp->is_connecting = 0;
                    tcp->is_connected = 1;
                    return WEBCLIENT_TRANS_OK;
                } else {
                    return WEBCLIENT_TRANS_TIMEOUT;
                }
            } else {
                tcp->is_connecting = 0;
                return WEBCLIENT_TRANS_ERROR;
            }
        }
    } else {
        if (0 == wait_connected(tcp->fd, timeout)) {
            tcp->is_connecting = 0;
            tcp->is_connected = 1;
            return WEBCLIENT_TRANS_OK;
        } else {
            return WEBCLIENT_TRANS_TIMEOUT;
        }
    }
}
static int cycle_flag = 0;
webclient_trans_err_t tcp_transport_read(tcp_transport_t *tcp, void *buf, int len, int timeout, int *rlen) {
    if (!tcp->is_connected) return WEBCLIENT_TRANS_ERROR;
    char *pbuf = buf;
    int plen = 0;
    while (plen < len) {
        int bytes = recv(tcp->fd, pbuf, len - plen, 0);
        if (0 == bytes) {
            if (0 == wait_readable(tcp->fd, timeout)){
                cycle_flag++;
                if(cycle_flag > 10){
                    cycle_flag = 0;
                    return WEBCLIENT_TRANS_TIMEOUT;
                }
                continue;
            }
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (0 == wait_readable(tcp->fd, timeout)) continue;
                if (plen > 0) break;
                return WEBCLIENT_TRANS_TIMEOUT;
            } else {
                return WEBCLIENT_TRANS_ERROR;
            }
        } else {
            XLOG_HEX_D("<read>: %d", pbuf, bytes, bytes);
            pbuf += bytes;
            plen += bytes; 
        }
    }
    cycle_flag = 0;
    *rlen = plen;
    return WEBCLIENT_TRANS_OK;
}

webclient_trans_err_t tcp_transport_write(tcp_transport_t *tcp, const void *buf, int len, int timeout, int *wlen) {
    if (!tcp->is_connected) return WEBCLIENT_TRANS_ERROR;
    const char *pbuf = buf;
    int plen = 0;
    while (plen < len) {
        int bytes = send(tcp->fd, pbuf, len - plen, 0);
        if (0 == bytes) {
            if (0 == wait_writeable(tcp->fd, timeout)) continue;
            if (plen > 0) break;
            return WEBCLIENT_TRANS_TIMEOUT;
        } else if (bytes == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (0 == wait_writeable(tcp->fd, timeout)) continue;
                if (plen > 0) break;
                return WEBCLIENT_TRANS_TIMEOUT;
            } else {
                return WEBCLIENT_TRANS_ERROR;
            }
        } else {
            XLOG_HEX_D("<write>:%d", pbuf, bytes, bytes);
            plen += bytes;
            pbuf += bytes;
        }
    }
    *wlen = plen;
    return WEBCLIENT_TRANS_OK;
}

bool tcp_transport_can_read(tcp_transport_t *tcp, int timeout) {
    return (0 == wait_readable(tcp->fd, timeout)) ? true : false;
}

bool tcp_transport_can_write(tcp_transport_t *tcp, int timeout) {
    return (0 == wait_writeable(tcp->fd, timeout)) ? true : false;
}

void tcp_transport_close(tcp_transport_t *tcp) {
    if (tcp->fd != -1) {
        close(tcp->fd);
        tcp->fd = -1;
        tcp->is_connected = 0;
        tcp->is_connecting = 0;
    }
}

void tcp_transport_destroy(tcp_transport_t *tcp) {
    gh_free(tcp);
}

int tcp_transport_get_fd(tcp_transport_t *tcp) {
    return tcp->fd;
}

webclient_transport_t *transport_tcp_init() {
    webclient_transport_t *t = webclient_transport_init();
    tcp_transport_t *tcp = tcp_transport_init();
    webclient_transport_set_io(t, tcp_connect, tcp_read, tcp_write, tcp_close, tcp_destroy);
    webclient_transport_set_priv(t, tcp);
    return t;
}
