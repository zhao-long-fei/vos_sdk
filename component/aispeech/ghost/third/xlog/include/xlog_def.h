#ifndef XLOG_DEF_H
#define XLOG_DEF_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>


#ifndef XLOG_EXPORT
#ifdef __GNUC__
#define XLOG_EXPORT __attribute ((visibility("default")))
#else
#define XLOG_EXPORT
#endif
#endif

typedef int xlog_level_t;

/*
 * 日志等级
 */
#define XLOG_LEVEL_DEBUG 0
#define XLOG_LEVEL_INFO 1
#define XLOG_LEVEL_WARNING 2
#define XLOG_LEVEL_ERROR 3

XLOG_EXPORT void xlog_output(xlog_level_t level, const char *tag, const char *func, int line, const char *format, ...);

XLOG_EXPORT void xlog_hexdump(xlog_level_t level, const char *tag, const char *func, int line, const char *buf, int len, const char *format, ...);

#ifdef __cplusplus
}
#endif
#endif
