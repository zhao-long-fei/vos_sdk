#ifndef GH_MUTEX_H
#define GH_MUTEX_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

/*
 * 互斥锁
 */

/*
 * 创建互斥锁
 */
typedef struct gh_mutex* gh_mutex_handle_t;

gh_mutex_handle_t gh_mutex_create();

/*
 * 上锁
 */
gh_err_t gh_mutex_lock(gh_mutex_handle_t obj);

/*
 * 开锁
 */
gh_err_t gh_mutex_unlock(gh_mutex_handle_t obj);

/*
 * 销毁互斥锁
 *
 * 注意
 * 禁止在上锁状态下进行销毁操作
 */
void gh_mutex_destroy(gh_mutex_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
