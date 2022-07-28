/*
 * 解码器
 * 一般用于解码预处理输出的音频数据
 * 常见的音频格式包括
 * pcm *
 * wav *
 * mp3 *
 * aac(adts aac、adif aac、mp4 aac、m4a aac)
 * flac
 * ts(解包)
 * bypass
 *
 *
 * 参考play_wav.c实现自己的解码器
 */
#ifndef PLAY_DECODER_H
#define PLAY_DECODER_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>

typedef enum {
    //完成
    PLAY_DECODER_DONE = 0,
    //输入被终止
    //即input返回-1
    PLAY_DECODER_INPUT_ABORT,
    //数据不足
    PLAY_DECODER_INPUT_UNDERFLOW,
    //输出被终止
    //即output返回-1
    PLAY_DECODER_OUTPUT_ABORT,
    //解码出错
    PLAY_DECODER_DECODE_ERROR,
}play_decoder_err_t;

typedef struct play_decoder play_decoder_t;

typedef struct {
    const char *type;
    int samplerate;
    int channels;
    int bits;
} play_decoder_info_t;

typedef int (*decoder_throw_info)(void *userdata, play_decoder_info_t *info);
typedef int (*decoder_input_data)(void *userdata, char *buf, int len);
typedef int (*decoder_output_data)(void *userdata, const void *buf, int len);

typedef struct {
    decoder_throw_info throw_info;
    decoder_input_data input;
    decoder_output_data output;
    void *userdata;
}play_decoder_cfg_t;

struct play_decoder {
    const void *type;
    int (*init)(play_decoder_t *pd, play_decoder_cfg_t *cfg);
    /*
     * process内部实现解包解码过程，其从input回调中获取原始数据，将解码后的数据通过output回调输出
     */
    play_decoder_err_t (*process)(play_decoder_t *pd);
    /*
     * 是否已经输出音频特征信息
     */
    bool (*has_post)(play_decoder_t *pd);
    void (*destroy)(play_decoder_t *pd);
    void *priv;
};

/*
 * 调用逻辑伪代码
static void run(void *args) {
    play_decoder_t *pd;
    play_decoder_cfg_t cfg = {
        .throw_info = throw_info,
        .input = audio_input,
        .output = audio_output,
    };
    int ret = pd->init(pd, &cfg);
    if (0 != ret) {
        //TODO
    }
    play_decoder_err_t err = pd->process(pd);
    if (err == xxxx) {
        //TODO
        //收尾处理
    }
    pd->destroy(pd);
}
 */

#ifdef __cplusplus
}
#endif
#endif
