#ifndef GH_THREAD_H
#define GH_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

/*
 * 线程创建
 */

typedef struct {
    //线程别名
    const char *tag;
    //线程执行函数
    void (*run)(void *args);
    //线程入口参数
    void *args;
    /*
     * rtos创建线程时需要配置
     * 其他平台使用默认配置
     */
    //优先级
    int priority;
    //栈大小
    int stack_size;
    //运行核id
    int core_id;
} gh_thread_cfg_t;

typedef struct gh_thread* gh_thread_handle_t;

#if 0
/*
 * 注意
 * 目前在windows上无法实现detached类型的线程，故此接口暂时不可用
 */

/*
 * 创建一个detached类型的线程，此类线程必须在进程退出时自身已经退出
 */
gh_err_t gh_thread_create(gh_thread_cfg_t *cfg);
#endif

/*
 * 创建一个joinable类型的线程，此类线程必须调用gh_thread_destroy进行清理
 * 
 * 类linux平台上调用pthread_create
 * freertos平台上调用xTaskCreate
 * windows平台上调用_beginthreadex
 */
gh_thread_handle_t gh_thread_create_ex(gh_thread_cfg_t *cfg);

/*
 * 等待线程主动退出并销毁相关资源
 *
 * linux平台上采用pthread_join方式
 * freertos平台上采用二值信号量实现
 * windows平台上采用WaitForSingleObject实现
 */
void gh_thread_destroy(gh_thread_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
