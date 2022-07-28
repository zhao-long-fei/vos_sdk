#ifndef GH_SEMAPHORE_H
#define GH_SEMAPHORE_H
#ifdef __cplusplus
}
#endif

#include "gh.h"

/*
 * 信号量
 * 计数型信号量
 * 二值信号量
 */

typedef struct gh_semaphore* gh_semaphore_handle_t;

/*
 * 创建二值信号量且状态为不可获取状态
 */
gh_semaphore_handle_t gh_semaphore_create_binary();

/*
 * 创建计数型信号量
 * value为初始值
 * max为最大值
 */
gh_semaphore_handle_t gh_semaphore_create_count(int value, int max);

/*
 * 获取信号量
 * gh_semaphore_take(obj, -1 or 0 or > 0);
 */
gh_err_t gh_semaphore_take(gh_semaphore_handle_t obj, int timeout);
gh_err_t gh_semaphore_give(gh_semaphore_handle_t obj);
int gh_semaphore_get_value(gh_semaphore_handle_t obj);

/*
 * 注意
 * 禁止在有线程在等待时进行销毁操作
 */
void gh_semaphore_destroy(gh_semaphore_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
