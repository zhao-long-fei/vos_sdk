#ifndef ASR_PROCESSOR_H
#define ASR_PROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef DDS_ENABLE
#include "dds.h"
#endif
#include "app.h"

/*
 * ASR PROCESSOR说明
 *
 * 注意
 * ASR PROCESSOR集成duilite/dds SDK，故在使用之前必须进行授权
 * 开发者外部调用duilite_library_load接口进行授权管理
 *
 * 包含的模块
 *  +duilite asr --负责本地识别
 *  +dds --负责云端全链路
 *  +调度--负责模块间的同步和调度管理
 */

typedef enum {
    ASR_EV_LOCAL,
    ASR_EV_CLOUD,
    ASR_EV_DM,
    ASR_EV_ERROR,
    ASR_EV_TIMEOUT,
    ASR_VAD_BEGIN_TIMEOUT,
    CMD_VAD_ONESHOT,
    CMD_VAD_RUN,
    CMD_ASR_RUN,
    CMD_ASR_STOP,
    CMD_ASR_RESET,
    CMD_NETWORK_UPDATE,
    INFO_ASR_ERROR,
    INFO_ASR_LOCAL,
    INFO_ASR_CLOUD,
    INFO_ASR_DM,
    INFO_SPEECH_BEGIN_TIMEOUT,
    INFO_SPEECH_END_TIMEOUT,
    INFO_SPEECH_END,
    INFO_SPEECH_BEGIN,
    CMD_IGNORE,
    CMD_DDS_FULL,
    INFO_ASR_FULL_DM,
    CMD_DDS_FULL_STOP,
    ASR_MAX_MSG,
}asr_event_t;

// typedef enum {
//     CMD_ASR_RUN,
//     CMD_ASR_STOP,
//     CMD_ASR_RESET,
//     CMD_NETWORK_UPDATE,
//     INFO_ASR_ERROR,
//     INFO_ASR_LOCAL,
//     INFO_ASR_CLOUD,
//     INFO_ASR_DM,
//     INFO_SPEECH_BEGIN,
//     INFO_SPEECH_BEGIN_TIMEOUT,
//     INFO_SPEECH_END_TIMEOUT,
//     MAX_MSG
// } asr_msg_type_t;

typedef int (*test_audio_cb)(void *userdata, const void *buf, int len);

typedef int (*asr_event_cb)(void *userdata, asr_event_t ev, const void *result, int len);

typedef int (*test_cb)(void *userdata, int ev, void *info, int len);
typedef struct {
    char *cfg;
    asr_event_cb event_cb;
    test_cb test_cb;
    test_audio_cb test_audio_cb;
#ifdef DDS_ENABLE
    //特别说明
    //由于dds sdk消息采用dds_ev_callback的形式，asr processor内部只关注asr结果相关的处理，
    //故在dds产生回调时先由asr processor处理需要的消息，然后再将原始的dds回调通知外部
    dds_ev_callback dds_cb;
#endif
    void *userdata;
} asr_processor_cfg_t;

int asr_processor_init(asr_processor_cfg_t *cfg);

/*
 * 开启识别
 * 每次需要识别的时候调用
 *
 * 注意
 * asr processor内部会根据配置的网络状态来决定采取本地识别还是云端识别
 */
int asr_processor_start();

/*
 * 送入音频数据
 */
int asr_processor_feed(const void *buf, int len);

/*
 * 等待识别结果
 */
int asr_processor_stop();

/*
 * 更新网络状态
 */
int asr_processor_update_network_status(int status);

/*
 * 取消识别
 */
int asr_processor_reset();

void asr_processor_deinit();

int asr_dds_custom_tts(const char *voiceId, const char *text, int volume, double speed);

int asr_set_dui_or_local(asr_engine_e asr_type);

void asr_reset_net_check();

int set_dds_type(dds_type_e dds_type);

asr_engine_e asr_get_current_type();

dds_type_e asr_get_duplex_type();

#ifdef __cplusplus
}
#endif
#endif
