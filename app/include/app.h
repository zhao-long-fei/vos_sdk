#ifndef APP_H
#define APP_H

#define VOS_VERSION "vos_sdk_v1.0.4_build_220701"
// 程序运行中间文件存放路径，例如授权文件，usb检查文件，状态检查文件等
#define VOS_OPERATING_DIR "/userdata/speech_auth"
// 授权文件存放路径
#define VOS_AUTH_FILE "/userdata/speech_auth/Profile.txt"
//#define CREATE_AUTH_CMD "touch /userdata/speech_auth/Profile.txt"
// 检查USB信息文件存放路径
#define SAVE_USB_INFO_FILE "/userdata/speech_auth/usb"
#define SAVE_USB_CMD "lsusb >/userdata/speech_auth/usb"

// 太行没有固件的USB pid:vid
#define TH_USB_PVID_BOOT "03fe:0301"
// 太行烧录完固件的USB pid:vid
#define TH_USB_RUN "5448:152e"

#define XLOG_TAG "app"
#define XLOG_LVL XLOG_LEVEL_INFO

// 离线识别阈值
#define ASR_RESULT_CONF 0.6

typedef enum{
	ASR_ENGINE_LOCAL = 1,
	ASR_ENGINE_DUI = 2,
	ASR_ENGINE_BOTH = 3,
}asr_engine_e;

typedef enum{
    ASR_IN_TH_RESULT = 1,
	ASR_IN_TH_QUICK_CMD,
	ASR_IN_AP_RESULT,
}asr_result_type_e;

typedef enum {
    //合成请求返回的URL
    TTS_ATTR_URL,
    //请求出错
    TTS_ATTR_ERROR,
    TTS_ATTR_MAX,
} dds_event_type_e;

typedef enum{
    DDS_HALF_DUPLEX,
    DDS_FULL_DUPLEX,
}dds_type_e;

int set_wake_doa(int doa);
int get_wake_doa();
#endif