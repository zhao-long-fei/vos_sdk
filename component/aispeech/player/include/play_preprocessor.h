/*
 * 预处理器，即音频预处理，主要负责音频数据的获取，例如文件读取、下载
 * 包含三个阶段，分别为init、poll、destroy
 * init阶段完成预处理器的初始化工作
 * poll阶段输出音频特征信息和数据流
 * destroy阶段负责预处理器的销毁
 *
 * 预处理器包括但不限于以下实体
 * http、m3u8流
 * 读取本地文件
 * 读取flash数据
 * 获取codec录音数据
 * 其他能够输出音频数据的实体
 *
 *
 * 参考file_preprocessor.c实现自己的预处理器
 */

#ifndef PLAY_PREPROCESSOR_H
#define PLAY_PREPROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct play_preprocessor play_preprocessor_t;

typedef struct {
    const char *type;
    int samplerate;
    int channels;
    int bits;
} play_preprocessor_info_t;

/*
 * 输出音频信息
 * 返回值说明
 * -1-->终止预处理
 */
typedef int (*preprocessor_output_info)(void *userdata, play_preprocessor_info_t *info);

/*
 * 输出音频数据
 * 返回值说明
 * -1-->终止预处理
 */
typedef int (*preprocessor_output_data)(void *userdata, const void *buf, int len);

typedef struct {
    //预处理目标
    const char *target;
    preprocessor_output_info output_info;
    preprocessor_output_data output_data;
    void *userdata;
} play_preprocessor_cfg_t;

typedef enum {
    //继续调用poll
    PREPROCESSOR_ERR_CONTINUE = 0,
    //预处理被终止
    PREPROCESSOR_ERR_ABORT,
    //预处理完成
    PREPROCESSOR_ERR_DONE,
    //内部错误
    PREPROCESSOR_ERR_ERROR,
} preprocessor_err_t;

struct play_preprocessor {
    const char *type;
    int (*init)(play_preprocessor_t *pp, play_preprocessor_cfg_t *cfg);
    /*
     * 预处理线程循环调用poll驱动预处理器持续运行
     */
    preprocessor_err_t (*poll)(play_preprocessor_t *pp, int timeout);
    /*
     * 是否已经输出音频信息
     */
    bool (*has_post)(play_preprocessor_t *pp);
    void (*destroy)(play_preprocessor_t *pp);
    void *priv;
};

/*
 * 调用逻辑伪代码
 *
static bool force_exit = false;
static void run(void *args) {
    play_preprocessor_t *pp;
    play_preprocessor_cfg_t cfg;

    int ret = pp->init(pp, &cfg);
    if (0 != ret) {
        //TODO
    }
    preprocessor_err_t err;
    do {
        //以100ms超时调用
        err = pp->poll(pp, 100);
    } while (err == PREPROCESSOR_ERR_CONTINUE && !force_exit);
    if (err == xxxx) {
        //TODO
    }
    pp->destroy(pp);
}
 */

#ifdef __cplusplus
}
#endif
#endif
