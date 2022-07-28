#ifndef GH_EVENT_GROUP_H
#define GH_EVENT_GROUP_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

/*
 * event group模块
 * 用于多线程间同步
 */

typedef struct gh_event_group* gh_event_group_handle_t;

/*
 * 注意
 * 由于freertos最大可以设置为24位，即[23:0]bit
 * 编写跨平台应用时要注意此限制
 */
typedef uint32_t gh_event_bits_t;

/*
 * 创建event_group句柄
 */
gh_event_group_handle_t gh_event_group_create();

/*
 * 设置新的bit标志并返回原有的bit值
 *
 * gh_event_bits_t bits = (1 << 0 | 1 << 1);
 * gh_event_group_set_bits(obj, &bits)
 */
gh_err_t gh_event_group_set_bits(gh_event_group_handle_t obj, gh_event_bits_t *bits);

/*
 * 清除bit标志并返回原有的bit值
 * gh_event_bits_t bits = (1 << 0 | 1 << 1);
 * gh_event_group_clear_bits(obj, &bits)
 */
gh_err_t gh_event_group_clear_bits(gh_event_group_handle_t obj, gh_event_bits_t *bits);

/*
 * 等待特定组合bit标志(and/or)并返回原有的bit值
 * gh_event_bits_t bits = (1 << 0 | 1 << 1);
 * gh_event_group_wait_bits(obj, &bits, true or false, true or flase, 0 or -1 or > 0);
 *
 * 返回值
 * GH_ERR_OK
 * GH_ERR_TIMEOUT
 */
gh_err_t gh_event_group_wait_bits(gh_event_group_handle_t obj, gh_event_bits_t *bits, bool all, bool clear, int timeout);

/*
 * 获取当前bit标志
 * gh_event_bits_t bits = gh_event_group_get_bits(obj);
 */
gh_event_bits_t gh_event_group_get_bits(gh_event_group_handle_t obj);

/*
 * 销毁event_group句柄
 *
 * 注意
 * 禁止在有线程仍在wait时进行销毁操作
 */
void gh_event_group_destroy(gh_event_group_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
