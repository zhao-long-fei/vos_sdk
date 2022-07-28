#ifndef GH_H
#define GH_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

/*
 * 错误码
 */
typedef enum {
    GH_ERR_LARGE = -8,
    GH_ERR_PROHIBIT = -7,
    GH_ERR_EMPTY = -6,
    GH_ERR_FULL = -5,
    GH_ERR_TIMEOUT = -4,
    GH_ERR_FINISHED = -3,
    GH_ERR_ABORT = -2,
    GH_ERR_ERROR = -1,
    GH_ERR_OK = 0,
} gh_err_t;

/*
 * 内存分配相关接口
 * 建议在程序开始时设置
 * 未调用的情况下默认使用glibc malloc/calloc/realloc/free
 */
typedef struct {
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nmemb, size_t size);
    void *(*realloc)(void *ptr, size_t size);
    void (*free)(void *ptr);
} gh_alloc_cfg_t;

gh_err_t gh_alloc_config(gh_alloc_cfg_t *cfg);

/*
 * ghost库统一使用下列接口进行内存分配和释放
 */
void *gh_malloc(size_t size);
void *gh_calloc(size_t nmemb, size_t size);
void *gh_realloc(void *ptr, size_t size);
void gh_free(void *ptr);

/*
 * 字符串拷贝相关接口
 */

/*
 * char *s = gh_strndup("123", 2);
 * gh_free(s);
 */
#define gh_strndup(s, len) gh_strndupx(NULL, s, len)

/*
 * char *s = gh_strdup("123");
 * gh_free(s);
 */
#define gh_strdup(s) gh_strdupx(NULL, s)

/*
 * 1.
 * char *s = gh_strdupx(NULL, "123");
 * gh_free(s);
 *
 * 2.
 * char *s = NULL;
 * s = gh_strdupx(&s, "123");
 * gh_free(s);
 *
 * 3.
 * char *s = gh_strndup("1234", 3)
 * char *s2 = gh_strdupx(&s, "4567");
 * if (s2) {
 *  //s2和s指向同一地址
 * }
 * gh_free(s);
 *
 * 4.
 * char *s = gh_strndup("1234", 3)
 * char *s2 = gh_strdupx(&s, "45");
 * if (s2) {
 *  //s2和s指向同一地址
 * }
 * gh_free(s);
 */
char *gh_strdupx(char **s1, const char *s2);

char *gh_strndupx(char **s1, const char *s2, int len);

/*
 * 1.
 * char *s = NULL;
 * char *s2 = "world";
 * int len = gh_strprintf(&s, "hello %s", s2);
 * if (-1 != len) {
 *     gh_free(s);
 * }
 *
 * 2.
 * 拼接字符串
 * char *s = NULL;
 * char *s1 = gh_strndup(NULL, "123");
 * char *s2 = gh_strndup(NULL, "456");
 * 
 * int len = gh_strprintf(&s, "%s-%s", s1, s2)
 * if (-1 != len) {
 *     gh_free(s);
 * }
 */
int gh_strprintf(char **s, const char *format, ...);
int gh_strvsprintf(char **s, const char *format, va_list ap);

#ifdef __cplusplus
}
#endif
#endif
