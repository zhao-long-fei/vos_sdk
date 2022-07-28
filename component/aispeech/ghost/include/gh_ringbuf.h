#ifndef GH_RINGBUF_H
#define GH_RINGBUF_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

/*
 * ringbuf模块
 * 用于多线程间字节流通信（一个生产者一个消费者）
 * 支持阻塞读写、非阻塞读写、自动扩容（存在最大限制）
 * 支持阻塞模式下的通知机制，即进入和退出读写阻塞时通过回调通知
 * 支持可读可写探测
 * 支持读水平线，即蓄水池机制
 */

/*
 * 读写指针示意图
 * +--------------------------------------------------------+
 * |            BUF                                         +
 * +--------------------------------------------------------+
 *          r                       w
 *          |    fill               |
 *
 *          w                       r
 * | fill   |                       | fill                  |
 */

typedef struct gh_ringbuf* gh_ringbuf_handle_t;

/*
 * 创建ringbuf句柄
 *
 * size: ringbuf大小
 */
gh_ringbuf_handle_t gh_ringbuf_create(int size);

/*
 * 清空数据以及abort/finish标志位并允许写入
 *
 * 注意
 * ringbuf read相关接口可以先于gh_ringbuf_start接口被调用
 */
#define gh_ringbuf_start(obj) gh_ringbuf_start2(obj, 0)

/*
 * 设置读水平线并允许写入
 * 注意
 * read_level必须不大于ringbuf size
 *
 * 注意
 * 1.ringbuf read相关接口可以先于gh_ringbuf_start接口被调用
 * 2.清除abort/finish标志位
 */
gh_err_t gh_ringbuf_start2(gh_ringbuf_handle_t obj, int read_level);

/*
 * 超时等待指定的可读数据量
 *
 * 注意
 *
 * len不得大于ringbuf size，否则内部会将len设置为ringbuf size
 * gh_ringbuf_can_read返回GH_ERR_Ok有以下情况：
 * rb中至少有len bytes数据；
 * 写端已经关闭，此时仍有部分数据可以读取(0 < obj->fill < len)
 * 请求的len大于ringbuf size
 *
 * 返回值
 * GH_ERR_OK        有数据可读
 * GH_ERR_FINISHED  写端已经关闭并且无数据可读
 * GH_ERR_TIMEOUT   数据不足
 * GH_ERR_ABORT     读写被终止
 */
gh_err_t gh_ringbuf_can_read(gh_ringbuf_handle_t obj, int len, int timeout);

/*
 * 超时等待指定的可写空间
 * 注意
 *
 * 返回值
 * GH_ERR_OK        可以写入
 * GH_ERR_TIMEOUT   数据不足
 * GH_ERR_ABORT     读写被终止
 *
 * len不得大于ringbuf size，否则内部会将len设置为ringbuf size
 */
gh_err_t gh_ringbuf_can_write(gh_ringbuf_handle_t obj, int len, int timeout);

/*
 * 阻塞读取请求的字节数
 *
 * 注意
 * buf可以为NULl（特殊使用场景中用于丢弃数据）
 * 请求len可以大于ringbuf size
 *
 * 返回值
 * GH_ERR_OK 已经读取到请求长度的数据
 * GH_ERR_FINISHED 实际读取len长度的数据，并且写端已经关闭
 * GH_ERR_ABORT 读写被终止
 */
gh_err_t gh_ringbuf_blocked_read(gh_ringbuf_handle_t obj, void *buf, int *len);

/*
 * 尽可能的读取请求的字节数
 *
 * 注意
 * buf可以为NULl（特殊使用场景中用于丢弃数据）
 *
 * 返回值
 * GH_ERR_OK 实际读取len长度的数据
 * GH_ERR_FINISHED 写端已经关闭（无数据可读）
 * GH_ERR_ABORT 读写被终止
 */
gh_err_t gh_ringbuf_try_read(gh_ringbuf_handle_t obj, void *buf, int *len);

/*
 * ringbuf在运行后才可以写入（即调用start后）,否则失败
 *
 * 阻塞写入请求的字节数
 *
 * 注意
 * 请求len可以大于ringbuf size
 *
 * 返回值
 * GH_ERR_OK 已经写入请求的字节数
 * GH_ERR_ABORT 读写被终止
 * GH_ERR_PROHIBIT start未调用
 */
gh_err_t gh_ringbuf_blocked_write(gh_ringbuf_handle_t obj, const void *buf, int *len);

/*
 * ringbuf在运行后才可以写入（即调用start后）,否则失败
 *
 * 尽可能写入请求的字节数
 *
 * 返回值
 * GH_ERR_OK 实际写入len长度的数据
 * GH_ERR_ABORT 读写被终止
 * GH_ERR_FULL 缓冲满
 * GH_ERR_PROHIBIT start未调用
 */
gh_err_t gh_ringbuf_try_write(gh_ringbuf_handle_t obj, const void *buf, int *len);

/*
 * ringbuf在运行后才可以写入（即调用start后）,否则失败
 *
 * 写入请求的字节数
 *
 * max最大允许的内部buf大小
 *
 * 返回值
 * GH_ERR_OK 数据全部写入
 * GH_ERR_LARGE 所需空间过大
 * GH_ERR_ABORT 读写被终止
 * GH_ERR_PROHIBIT start未调用
 */
gh_err_t gh_ringbuf_write_all(gh_ringbuf_handle_t obj, const void *buf, int *len, int max);

typedef enum {
    //读可能阻塞
    GH_RINGBUF_MAY_READ_BLOCK,
    //继续读取
    GH_RINGBUF_READ_CONTINUE,
    //写可能阻塞
    GH_RINGBUF_MAY_WRITE_BLOCK,
    //继续写入
    GH_RINGBUF_WRITE_CONTINUE,
} gh_ringbuf_cb_type_t;

typedef void (*gh_ringbuf_cb)(gh_ringbuf_cb_type_t t, void *userdata);

/*
 * 设置回调
 */
gh_err_t gh_ringbuf_set_cb(gh_ringbuf_handle_t obj, gh_ringbuf_cb cb, void *userdata);

/*
 * 终止读写
 */
gh_err_t gh_ringbuf_abort(gh_ringbuf_handle_t obj);

/*
 * 写端关闭
 */
gh_err_t gh_ringbuf_finish(gh_ringbuf_handle_t obj);

/*
 * 可读数据量
 */
int gh_ringbuf_get_available(gh_ringbuf_handle_t obj);

/*
 * 销毁ringbuf句柄
 *
 * 注意
 * 禁止ringbuf在读写阻塞期间进行销毁操作
 */
void gh_ringbuf_destroy(gh_ringbuf_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
