#ifndef TH_SDH_H
#define TH_SDK_H
typedef enum {
    //主唤醒事件
    //同时主动进入ONESHOT判断逻辑
    FRONT_EV_WAKEUP_MAJOR,
    //次唤醒事件
    FRONT_EV_WAKEUP_MINOR,
    //检测到语音开始
    FRONT_EV_SPEECH_BEGIN,
    //在ONESHOT检测过程中可能会出现VAD重启的操作，通过此事件告知开发者需要重启识别收音
    FRONT_EV_SPEECH_ABORT,
    //检测到语音开始超时
    FRONT_EV_SPEECH_BEGIN_TIMEOUT,
    //检测到说话结束
    FRONT_EV_SPEECH_END,
    //检测到说话结束超时
    FRONT_EV_SPEECH_END_TIMEOUT,
    //系统停止
    FRONT_EV_SYSTEM_STOPPED,
    FRONT_EV_TH_STATUS,
} th_event_t;

typedef int (*th_event_cb)(void *userdata, th_event_t ev, const void *result, int len);
typedef int (*th_bf_audio_cb)(void *userdata,const void *buf,int len);
typedef int (*th_fct_cb)(const void *buf,int len);
typedef int (*th_login_cb)(int flag);
typedef int (*th_asr_cb)(char *result,int flag);

typedef struct {
    int index;      //对应于cfg中words数组的下标
    const char *word;
    int doa;
} th_wakeup_info_t;

typedef struct {
    char *cfg;
    char *auth_cfg;
    void *userdata;
    th_event_cb event_cb;
    th_bf_audio_cb bf_audio_cb;
    th_fct_cb fct_audio_cb;
    th_login_cb login_cb;
    th_asr_cb asr_cb;
} th_processor_cfg_t;

int thsdk_close_fct();
int thsdk_start_fct();
void th_set_wakeup_words(int level);
int get_doa();
int th_set_info(const char *cmd);
int th_get_info(const char *cmd);
#endif