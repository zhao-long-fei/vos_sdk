#ifndef GH_THREADPOOL_H
#define GH_THREADPOOL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gh.h"

typedef struct gh_threadpool* gh_threadpool_handle_t;

typedef struct {
    //最大线程数
    int max;
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
} gh_thread_pool_cfg_t;

typedef struct gh_thread_job gh_thread_job_t;

struct gh_thread_job {
    char *tag;
    void (*do_job)(gh_thread_job_t *obj);
    void (*free_job)(gh_thread_job_t *obj);
    void *args;
};

/*
 * 创建线程池句柄
 */
gh_threadpool_handle_t gh_threadpool_create(gh_thread_pool_cfg_t *cfg);

/*
 * 添加任务到线程池
 */
gh_err_t gh_threadpool_add_job(gh_threadpool_handle_t obj, gh_thread_job_t *job);

/*
 * 销毁线程池
 */
void gh_threadpool_destroy(gh_threadpool_handle_t obj);

#ifdef __cplusplus
}
#endif
#endif
