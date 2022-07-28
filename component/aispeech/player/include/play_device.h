/*
 * 播放设备
 * 负责特定设备的数据流写入
 * file[用于debug]
 * alsa
 * dsp
 *
 * 参考alsa_device.c实现自己的播放设备
 */
#ifndef PLAY_DEVICE_H
#define PLAY_DEVICE_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    //播放类型
    char *type;
    char *device;
    int samplerate;
    int bits;
    int channels;
    //开发者在open接口内指定
    //某些驱动接口必须送入固定长度数据块
    //在open接口内部对此变量进行赋值
    int bytes;
} play_device_cfg_t;

typedef struct play_device play_device_t;

struct play_device {
    int (*open)(play_device_t *pd, play_device_cfg_t *cfg);
    int (*start)(play_device_t *pd);
    int (*write)(play_device_t *pd, const char *buf, int len, int timeout);
    int (*stop)(play_device_t *pd);
    int (*abort)(play_device_t *pd);
    void (*close)(play_device_t *pd);
    void *priv;
};

/*
 * 逻辑调用伪代码
 *
static void run(void *args) {
    play_device_t *pd;
    play_device_cfg_t cfg;
    int ret = pd->open(pd, &cfg);
    if (0 != ret || cfg.bytes <= 0) {
        //TODO
    }
    pd->start(pd);
    char *buf = malloc(cfg.bytes);
    int read_bytes;
    do {
        read_bytes = read_pipe(buf, cfg.bytes);
        if (read_bytes > 0) {
            pd->write(pd, buf, read_bytes);
        }
    } while (force_stop == 0 && read_bytes == 0);
    if (force_stop) {
        pd->abort(pd);
    } else {
        pd->stop(pd);
    }
    pd->close(pd);
}
 */

#ifdef __cplusplus
}
#endif
#endif
