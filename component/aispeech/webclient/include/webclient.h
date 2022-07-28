#ifndef WEBCLIENT_H
#define WEBCLIENT_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/*
 * 一次完整HTTP的请求包括以下步骤
 * (init -->
 * [set_url] --> [set_method] --> [set_header] --> preprared --> connect -->
 * send_header --> send_body --> recv_header --> read body --> finished -->
 * destroy)
 *
 * 其中set_url/set_method/set_header是可选设置
 *
 * webclient 模块包括以下接口
 *
 * webcliet_init用于创建一个webclient句柄
 * webclient_get_state用于获取当前webclient的状态，init之后为WEBCLIENT_STATE_INIT
 * webclient_set_url用于设置新的url（默认内部会设置Content-Length/User-Agent/Host头部）
 * webclient_get_url用于获取已经设置的url
 * webclient_set_method用于设置新的请求方法，默认是GET方法
 * webclient_set_header用于设置特定的请求头部，支持覆盖默认值
 * webclient_get_request_header用于获取已经设置的头部
 * webclient_prepared用于告知webclient已经准备好
 * webclient_connect用于发起tcp连接
 * webclient_send_header用于发送请求行和请求头部
 * webclient_send_body用于发送body数据
 * webclient_read_header读取响应数据并解析头部
 * webclient_get_status_code用于获取响应码
 * webclient_get_response_header用于获取响应头部
 * webcilent_read用于读取响应数据
 * webclient_drop_response用于读空响应数据
 * webclient_close关闭连接并清空相关数据
 * webclient_destroy销毁webclient句柄
 * 
 */

typedef struct webclient webclient_t;

typedef enum {
    //已经初始化
    WEBCLIENT_STATE_INIT,
    //已经准备好
    WEBCLIENT_STATE_PREPARED,
    //已经连接上，可以发送请求行
    WEBCLIENT_STATE_CONNECTED,
    //可以发送body数据
    WEBCLIENT_STATE_READY_SEND_BODY,
    //可以接收头部
    WEBCLIENT_STATE_READY_RECV_HEADER,
    //可以接收数据
    WEBCLIENT_STATE_READY_RECV,
    //请求完毕
    WEBCLIENT_STATE_FINISHED,
} webclient_state_t;

typedef enum {
    WEBCLIENT_METHOD_GET = 0,
    WEBCLIENT_METHOD_POST,
    WEBCLIENT_METHOD_MAX,
} webclient_method_t;

typedef struct {
    const char *url;    //[http[s]://]xxxxx[:port][/path?query]
    webclient_method_t method;
    //SSL相关
    const char *cert;
    const char *client_cert;
    const char *client_key;
    bool cert_common_name_check;
} webclient_cfg_t;

typedef enum {
    WEBCLIENT_ERR_FINISHED = -3,
    //超时
    //可以根据具体情况来决定是否需要重试
    WEBCLIENT_ERR_TIMEOUT = -2,
    WEBCLIENT_ERR_ERROR = -1,
    WEBCLIENT_ERR_OK = 0,
} webclient_err_t;

/*
 * 创建webclient句柄
 */
webclient_t *webclient_init(webclient_cfg_t *cfg);

/*
 * 设置新的请求URL并且内部默认会设置Host/User-Agent/Content-Length头部
 * 注意
 * 如果新的URL与先前的URL地址（域名或者端口号）不一致，则内部会主动关闭连接
 * webclient_init内部也会调用webclient_set_url进行url的设置
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 * WEBCLIENT_ERR_ERROR
 */
webclient_err_t webclient_set_url(webclient_t *obj, const char *url);

/*
 * 获取webclient内部状态
 */
webclient_state_t webclient_get_state(webclient_t *obj);

/*
 * 获取已经设置的URL
 */
const char *webclient_get_url(webclient_t *obj);

/*
 * 设置请求方法
 * 默认是GET方法
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 */
webclient_err_t webclient_set_method(webclient_t *obj, webclient_method_t method);

/*
 *设置请求头部
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 */
webclient_err_t webclient_set_header(webclient_t *obj, const char *key, const char *value);

/*
 * 获取已经设置的请求头部
 */
const char *webclient_get_request_header(webclient_t *obj, const char *key);

/*
 * 告知webclient已经准备好
 * 在此之后必须调用connect接口进行后续操作
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 */
webclient_err_t webclient_prepared(webclient_t *obj);

/*
 * 发起tcp连接
 *
 * 返回值
 * WEBCLIENT_ERR_OK 连接成功
 * WEBCLIENT_ERR_TIMEOUT 超时
 * WEBCLIENT_ERR_ERROR
 */
webclient_err_t webclient_connect(webclient_t *obj, int timeout);

/*
 * webclient内部将用户设置的请求行和头部按照http协议要求发送出去
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 * WEBCLIENT_ERR_TIMEOUT
 * WEBCLIENT_ERR_ERROR
 */
webclient_err_t webclient_send_header(webclient_t *obj, int timeout);

/*
 * 写入请求body数据
 * len = 0代表最后一次写入
 *
 * 注意必须调用此接口，否则内部状态无法切换
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 * WEBCLIENT_ERR_TIMEOUT
 * WEBCLIENT_ERR_ERROR
 */
webclient_err_t webclient_send_body(webclient_t *obj, const void *buf, int len, int timeout, int *wlen);

/*
 * 读取响应头部数据
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 * WEBCLIENT_ERR_TIMEOUT
 * WEBCLIENT_ERR_ERROR
 */
webclient_err_t webclient_read_header(webclient_t *obj, int timeout);

/*
 * 获取响应码
 * 
 * 在webclient_read_header返回OK后才可以调用
 */
int webclient_get_status_code(webclient_t *obj);

/*
 * 获取body长度，即Content-Length
 *
 * 注意
 * 如果是chunked编码则返回-1
 * 
 * 在webclient_read_header返回OK后才可以调用
 */
int webclient_get_content_length(webclient_t *obj);

/*
 * 获取此次请求的响应头部
 * 在webclient_read_header返回OK后才可以调用
 */
const char *webclient_get_response_header(webclient_t *obj, const char *key);

/*
 * 读取body数据
 *
 * 注意
 * 在webclient_send_body返回OK后才可以调用
 * 如果响应头部还未接收，则内部会先从网络连接中读取头部然后再读取body部分
 * 需要持续调用此接口，直到读取全部数据（此时返回WEBCLIENT_ERR_FINISHED）
 *
 * 返回值
 * WEBCLIENT_ERR_OK
 * WEBCLIENT_ERR_TIMEOUT
 * WEBCLIENT_ERR_ERROR
 * WEBCLIENT_ERR_FINISHED
 */
webclient_err_t webclient_read(webclient_t *obj, char *buf, int len, int timeout, int *rlen);

/*
 * 从连接中读取所有数据并丢弃
 * 需要持续调用此接口，直到读取全部数据（此时返回WEBCLIENT_ERR_FINISHED）
 * 在webclient_send_body返回OK后才可以调用
 *
 * 返回值
 * WEBCLIENT_ERR_TIMEOUT
 * WEBCLIENT_ERR_ERROR
 * WEBCLIENT_ERR_FINISHED
 */
webclient_err_t webclient_drop_response(webclient_t *obj, int timeout);

//webclient_err_t webclient_perform(webclient_t *obj, int timeout);

/*
 * 关闭连接并清空相关数据
 */
void webclient_close(webclient_t *obj);

/*
 * 销毁webclient句柄
 */
void webclient_destroy(webclient_t *obj);

#ifdef __cplusplus
}
#endif
#endif
