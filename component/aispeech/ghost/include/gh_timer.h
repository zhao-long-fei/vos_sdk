#ifndef GH_TIMER_H
#define GH_TIMER_H
#ifdef __cplusplus
extern "C"
#endif

#include "gh.h"

typedef struct gh_timer* gh_timer_handle_t;

typedef void (*gh_timer_expired_cb)(void *userdata);

typedef struct {
    bool auto_reload;   //自动重载
    int timeout;        //超时时间
    gh_timer_expired_cb cb; //超时回调
    void *userdata;
} gh_timer_cfg_t;

/*
 * 创建定时器句柄
 */
gh_timer_handle_t gh_timer_create(gh_timer_cfg_t *cfg);

/*
 * 开始计时
 */
gh_err_t gh_timer_start(gh_timer_handle_t obj);

//推迟delay毫秒
gh_err_t gh_timer_retard(gh_timer_handle_t obj, int delay);

/*
 * 终止计时
 */
gh_err_t gh_timer_stop(gh_timer_handle_t obj);

/*
 * 销毁定时器句柄
 */
void gh_timer_destroy(gh_timer_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
