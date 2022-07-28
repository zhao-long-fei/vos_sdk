#ifndef PLAYER_H
#define PLAYER_H
#ifdef __cplusplus
extern "C" {
#endif

/*
 * 播放器实现
 * 设计思路
 * 一次音频的播放需要经历三个过程，分别为预处理、解码、播放
 * 故设计上采用多线程架构，分别为预处理线程、解码线程、播放线程
 * 预处理线程负责音频流的预处理，一般是数据流的获取
 * 解码线程负责音频流的解包解码
 * 播放线程负责具体音频设备的打开关闭及播放
 * 管理线程负责外部输入处理和内部消息同步以及触发用户回调
 */

#include "play_preprocessor.h"
#include "play_decoder.h"
#include "play_device.h"

/*
 * 初始化播放器
 */
int player_init();

typedef struct player player_t;

/*
 * 播放事件类型
 */
typedef enum {
    //运行中(player_play发起播放后触发)
    PLAYER_EV_RUNNING,
    //暂停播放
    PLAYER_EV_PAUSED,
    //继续播放
    PLAYER_EV_RESUMED,
    //播放被终止
    PLAYER_EV_STOPPED,
    //播放完成
    PLAYER_EV_DONE,
} player_ev_t;

/*
 * 播放事件回调
 */
typedef void (*player_ev_cb)(player_t *player, player_ev_t ev, void *userdata);

/*
 * 播放器配置
 */
typedef struct {
    //标签
    const char *tag;

    //预处理输出ringbuf大小
    int preprocess_rb_size;     //建议>=10kbyte
    //解码输出ringbuf大小
    int decode_rb_size;         //建议>=20kbyte

    //播放设备配置
    char *device;
    play_device_t play_device;

    //事件回调（每次都执行）
    player_ev_cb cb;
    void *userdata;

    /*
     * rtos系统创建线程时需要
     */
    int preprocess_stack_size;
    int preprocess_priority;
    int preprocess_core_id;

    int decode_stack_size;
    int decode_priority;
    int decode_core_id;

    int play_stack_size;
    int play_priority;
    int play_core_id;
} player_cfg_t;

/*
 * 创建播放对象
 */
player_t *player_create(player_cfg_t *cfg);

/*
 * 由于播放器的暂停／恢复／停止操作不是立即完成的，所以存在一个中间状态。
 * 例如PLAYER_STATE_PAUSING状态说明播放器已经收到暂停请求正在处理还未处理完毕，处理完毕后会通过PLAYER_EV_PAUSED回调告知业务线程。
 */
typedef enum {
    //空闲
    PLAYER_STATE_IDLE,
    //正在播放
    PLAYER_STATE_RUNNING,
    //正在暂停
    PLAYER_STATE_PAUSING,
    //已经暂停
    PLAYER_STATE_PAUSED,
    //正在恢复
    PLAYER_STATE_RESUMING,
    //正在终止
    PLAYER_STATE_STOPPING,
} player_state_t;

/*
 * 获取播放器状态
 */
player_state_t player_get_state(player_t *player);

/*
 * 播放参数
 */
typedef struct {
    //播放目标
    char *target;
    //此次播放使用的预处理器
    play_preprocessor_t preprocessor;

    //此次播放是否设置事件回调
    player_ev_cb cb;
    void *userdata;

    //指定播放参数
    int samplerate;
    int bits;
    int channels;
} player_play_t;

/*
 * 请求播放
 */
int player_play(player_t *player, player_play_t *play);

/*
 * 获取当前播放目标
 * 在下一次播放发起之前一直有效
 */
const char *player_get_target(player_t *player);

//可以在回调中调用
const char *player_get_target2(player_t *player);

/*
 * 暂停播放
 * 异步接口
 */
int player_pause(player_t *player);

/*
 * 继续播放
 * 异步接口
 */
int player_resume(player_t *player);

/*
 * 终止播放
 * 异步接口
 */
int player_stop(player_t *player);

/*
 * 销毁播放对象
 * 必须播放已经结束后才可以调用
 */
void player_destroy(player_t *player);

void player_deinit();

#ifdef __cplusplus
}
#endif
#endif
