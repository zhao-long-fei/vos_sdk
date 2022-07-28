#ifndef XLOG_H
#define XLOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "xlog_def.h"
#include <stdbool.h>

/*
 * 日志模块
 * 支持多线程环境使用
 * 支持动态开关日志输出
 * 支持动态设置全局日志输出等级
 * 支持hexdump
 * 支持DEBUG/INFO/WARN/ERROR等级输出
 * 支持tag过滤及等级设定
 */

/*
 * hexdump每行打印宽度
 */
#define XLOG_DUMP_LINE_WIDTH 16

/*
 * 默认输出等级
 */
#ifndef XLOG_OUTPUT_LEVEL
#define XLOG_OUTPUT_LEVEL XLOG_LEVEL_DEBUG
#endif

typedef struct {
    void *(*init_lock)();
    int (*take_lock)(void *lock);
    int (*give_lock)(void *lock);
    void (*destroy_lock)(void *lock);
} xlog_cfg_t;

/*
 * 初始化日志模块
 * 
 * 注意
 * 如果在多线程环境中使用需要设置xlog_cfg_t结构用于设置互斥锁，单线程环境中直接设置为NULL即可
 */
XLOG_EXPORT int xlog_init(xlog_cfg_t *cfg);

/*
 * 使能输出
 *
 * 默认输出打开
 */
XLOG_EXPORT void xlog_enable_output(bool enable);

/*
 * 设置输出日志等级
 * 
 * 默认为XLOG_OUTPUT_LEVEL
 */
XLOG_EXPORT void xlog_set_output_level(xlog_level_t level);

/*
 * 设置特定TAG的输出等级
 *
 * 支持”*“操作以此支持所有TAG设置相同的等级
 *
 * 举例
 * 将“main”设置为XLOG_LEVEL_INFO
 * xlog_set_tag_level("main", XLOG_LEVEL_INFO);
 *
 * 所有tag设置为XLOG_LEVEL_WARNING
 * xlog_set_tag_level("*", XLOG_LEVEL_WARNING);
 */
XLOG_EXPORT void xlog_set_tag_level(const char *tag, xlog_level_t level);

XLOG_EXPORT void xlog_deinit();

/*
 * 日志输出接口
 *
 * 可以直接使用XLOG_{D/I/W/E}接口，但是必须在include xlog.h之前定义XLOG_TAG和XOG_LVL宏
 * 例如
 * #define XLOG_TAG "foo"
 * #define XLOG_LVL XLOG_LEVEL_DEBUG
 * XLOG_D("debug");
 * XLOG_I("info");
 * XLOG_W("warning");
 * XLOG_E("error");
 *
 *
 * 如果某个文件包含多个模块，每个模块需要使用不同的TAG，可以使用XLOG_DEBUG/XLOG_INFO等接口实现
 * 举例
 * 需要使用foo
 * #define FOO_D(TAG, format, ...) XLOG_DEBUG(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
 *
 * FOO_D(foo, "hello")
 */
#define XLOG_D(format, ...) XLOG_DEBUG(XLOG_TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define XLOG_I(format, ...) XLOG_INFO(XLOG_TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define XLOG_W(format, ...) XLOG_WARN(XLOG_TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define XLOG_E(format, ...) XLOG_ERROR(XLOG_TAG, __func__, __LINE__, format, ##__VA_ARGS__)

#define XLOG_HEX_D(format, buf, len, ...) XLOG_HEX_DUMP_D(XLOG_TAG, __func__, __LINE__, format, buf, len, ##__VA_ARGS__)
#define XLOG_HEX_I(format, buf, len, ...) XLOG_HEX_DUMP_I(XLOG_TAG, __func__, __LINE__, format, buf, len, ##__VA_ARGS__)
#define XLOG_HEX_W(format, buf, len, ...) XLOG_HEX_DUMP_W(XLOG_TAG, __func__, __LINE__, format, buf, len, ##__VA_ARGS__)
#define XLOG_HEX_E(format, buf, len, ...) XLOG_HEX_DUMP_E(XLOG_TAG, __func__, __LINE__, format, buf, len, ##__VA_ARGS__)

#if (XLOG_LVL > XLOG_LEVEL_DEBUG) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_DEBUG)                
    #define XLOG_DEBUG(TAG, func, line, format, ...)
#else                                                                           
    #define XLOG_DEBUG(TAG, func, line, format, ...) xlog_output(XLOG_LEVEL_DEBUG, TAG, func, line, format, ##__VA_ARGS__)
#endif

#if (XLOG_LVL > XLOG_LEVEL_INFO) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_INFO)                
    #define XLOG_INFO(TAG, func, line, format, ...)
#else                                                                           
    #define XLOG_INFO(TAG, func, line, format, ...) xlog_output(XLOG_LEVEL_INFO, TAG, func, line, format, ##__VA_ARGS__)
#endif

#if (XLOG_LVL > XLOG_LEVEL_WARNING) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_WARNING)                
    #define XLOG_WARN(TAG, func, line, format, ...)
#else                                                                           
    #define XLOG_WARN(TAG, func, line, format, ...) xlog_output(XLOG_LEVEL_WARNING, TAG, func, line, format, ##__VA_ARGS__)
#endif

#if (XLOG_LVL > XLOG_LEVEL_ERROR) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_ERROR)                
    #define XLOG_ERROR(TAG, func, line, format, ...)
#else                                                                           
    #define XLOG_ERROR(TAG, func, line, format, ...) xlog_output(XLOG_LEVEL_ERROR, TAG, func, line, format, ##__VA_ARGS__)
#endif
                                                                                   
#if (XLOG_LVL > XLOG_LEVEL_DEBUG) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_DEBUG)                
    #define XLOG_HEX_DUMP_D(TAG, func, line, format, buf, len, ...)
#else                                                                           
    #define XLOG_HEX_DUMP_D(TAG, func, line, format, buf, len, ...) xlog_hexdump(XLOG_LEVEL_DEBUG, TAG, func, line, buf, len, format, ##__VA_ARGS__)
#endif

#if (XLOG_LVL > XLOG_LEVEL_INFO) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_INFO)                
    #define XLOG_HEX_DUMP_I(TAG, func, line, format, buf, len, ...)
#else                                                                           
    #define XLOG_HEX_DUMP_I(TAG, func, line, format, buf, len, ...) xlog_hexdump(XLOG_LEVEL_INFO, TAG, func, line, buf, len, format, ##__VA_ARGS__)
#endif

#if (XLOG_LVL > XLOG_LEVEL_WARNING) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_WARNING)                
    #define XLOG_HEX_DUMP_W(TAG, func, line, format, buf, len, ...)
#else                                                                           
    #define XLOG_HEX_DUMP_W(TAG, func, line, format, buf, len, ...) xlog_hexdump(XLOG_LEVEL_WARNING, TAG, func, line, buf, len, format, ##__VA_ARGS__)
#endif

#if (XLOG_LVL > XLOG_LEVEL_ERROR) || (XLOG_OUTPUT_LEVEL > XLOG_LEVEL_ERROR)                
    #define XLOG_HEX_DUMP_E(TAG, func, line, format, buf, len, ...)
#else                                                                           
    #define XLOG_HEX_DUMP_E(TAG, func, line, format, buf, len, ...) xlog_hexdump(XLOG_LEVEL_ERROR, TAG, func, line, buf, len, format, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif
#endif
