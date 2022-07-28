#ifndef GH_QUEUE_H
#define GH_QUEUE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

/*
 * queue模块
 * 队列用于多线程间数据通信，队列中每次放入一个消息
 * 支持头部读取和插入
 * 支持尾部读取和插入
 * 支持超时读写操作
 */

typedef struct gh_queue* gh_queue_handle_t;

/*
 * 创建queue句柄
 */
gh_queue_handle_t gh_queue_create(int slot_size, int slot_count);

/*
 * 允许消息写入
 */
gh_err_t gh_queue_start(gh_queue_handle_t obj);

/*
 * 终止消息读写
 */
gh_err_t gh_queue_abort(gh_queue_handle_t obj);

/*
 * 获取队列中第一个消息
 */
gh_err_t gh_queue_peek(gh_queue_handle_t obj, void *slot);

/*
 * 从头部读取消息
 */
gh_err_t gh_queue_read(gh_queue_handle_t obj, void *slot, int timeout);

/*
 * 从尾部读取消息
 */
gh_err_t gh_queue_read_back(gh_queue_handle_t obj, void *slot, int timeout);

/*
 * 尾部写入写入消息
 */
gh_err_t gh_queue_send(gh_queue_handle_t obj, const void *slot, int timeout);

/*
 * 头部写入消息
 */
gh_err_t gh_queue_send_front(gh_queue_handle_t obj, const void *slot, int timeout);

/*
 * 标记不再写入
 */
gh_err_t gh_queue_finish(gh_queue_handle_t obj);

/*
 * 清空slot内容
 */
gh_err_t gh_queue_reset(gh_queue_handle_t obj);

/*
 * 获取队列中的消息数
 */
int gh_queue_get_available(gh_queue_handle_t obj);

/*
 * 销毁queue句柄
 *
 * 注意
 * 禁止queue在读写阻塞期间进行销毁操作
 */
void gh_queue_destroy(gh_queue_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
