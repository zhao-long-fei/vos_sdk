#ifndef GH_MAILBOX_H
#define GH_MAILBOX_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

/*
 * 邮箱用于多线程间数据通信，邮箱中每次放入一个邮件(指针)
 *
 * 支持头部读取和插入
 * 支持尾部读取和插入
 * 支持超时读写操作
 */

typedef struct gh_mailbox* gh_mailbox_handle_t;

/*
 * 创建邮箱句柄
 */
gh_mailbox_handle_t gh_mailbox_create(int size);

/*
 * 邮箱可以写入
 */
gh_err_t gh_mailbox_start(gh_mailbox_handle_t obj);


/*
 * 终止邮箱读写
 */
gh_err_t gh_mailbox_abort(gh_mailbox_handle_t obj);

/*
 * 从头部读取邮件
 */
gh_err_t gh_mailbox_read(gh_mailbox_handle_t obj, void **slot, int timeout);

/*
 * 从尾部读取邮件
 */
gh_err_t gh_mailbox_read_back(gh_mailbox_handle_t obj, void **slot, int timeout);

/*
 * 在尾部写入一个邮件
 */
gh_err_t gh_mailbox_send(gh_mailbox_handle_t obj, const void *slot, int timeout);

/*
 * 在头部写入一个邮件
 */
gh_err_t gh_mailbox_send_front(gh_mailbox_handle_t obj, const void *slot, int timeout);

/*
 * 标记邮箱不再写入
 */
gh_err_t gh_mailbox_finish(gh_mailbox_handle_t obj);

/*
 * 检查邮箱头部第一个邮件
 */
gh_err_t gh_mailbox_peek(gh_mailbox_handle_t obj, void **slot);

/*
 * 销毁mailbox句柄
 *
 * 注意
 * 禁止mailbox在读写阻塞期间进行销毁操作
 */
void gh_mailbox_destroy(gh_mailbox_handle_t obj);


#ifdef __cplusplus
}
#endif
#endif
