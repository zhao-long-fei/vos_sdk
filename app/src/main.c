#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "duilite.h"
#include "asr_processor.h"
#include "xlog.h"
#include "gh_mutex.h"
#include "gh_queue.h"
#include "gh_thread.h"
#include "gh_event_group.h"
#include "cJSON.h"
#include "taskstack.h"
//#include "capture_impl.h"
#include "player_manager.h"
#include "http_preprocessor.h"
#include "file_preprocessor.h"
#include "m3u8_preprocessor.h"
#include "alsa_device.h"
#include "netstatus.h"
#include <fcntl.h>
#include "dds.h"
//#include "device_upload.h"
#include "TH_process.h"
#include <ctype.h>
#include <assert.h>
#include <sys/time.h>
#include <signal.h>
#include "app.h"
#include "monitor_process.h"
#include "msgtothird.h"
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/stat.h>
#include "factory_test.h"
#include "utils.h"
#include "voice_message_prase.h"

#define BIT_TH_LOGIN (1 << 0)
#define BIT_USB_READY (1 << 1)
#define BIT_NET_READY (1 << 2)
#define BIT_DDS_READY (1 << 3)

typedef struct {
    char *word;
    char *audio;
} wakeup_audio_t;

typedef struct {
    gh_queue_handle_t q;
    taskstack_t fsm_stack;
    taskstack_t app_stack;
    wakeup_audio_t *words;
    int words_count;
    char *begin_timeout_prompt;
    char *end_timeout_prompt;
    char *error_prompt;
    gh_thread_handle_t t_monitor;
    gh_thread_handle_t t_getmsg;
    gh_thread_handle_t t_station;
    // device_upload_client_t *asr;
    // device_upload_client_t *wakeup;
    // device_upload_client_t *beamforming;
    gh_thread_handle_t t_check;
    gh_thread_handle_t t_taskstack;
    gh_thread_handle_t t_test;
    gh_event_group_handle_t appdone;
} app_t;

static app_t s_app;

typedef enum {
    APP_EV_WAKEUP_MAJOR,
    APP_EV_WAKEUP_MINOR,
    APP_EV_SPEECH_BEGIN,
    APP_EV_SPEECH_BEGIN_TIMEOUT,
    APP_EV_SPEECH_ABORT,
    APP_EV_SPEECH_END,
    APP_EV_SPEECH_END_TIMEOUT,
    APP_EV_ASR_LOCAL,
    APP_EV_ASR_DM,
    APP_EV_ASR_ERROR,
    APP_EV_RESET,
    APP_EV_PROMPT_COMPLETE,
    APP_EV_ASR_TIMEOUT,
    APP_VAD_BEGIN_TIMEOUT,
    APP_CMD_IGNORE,
    MAX_MSG,
} app_msg_type_t;

asr_engine_e asr_type = ASR_ENGINE_LOCAL;

static const char *msg_table[] = {
    "APP_EV_WAKEUP_MAJOR",
    "APP_EV_WAKEUP_MINOR",
    "APP_EV_SPEECH_BEGIN",
    "APP_EV_SPEECH_BEGIN_TIMEOUT",
    "APP_EV_SPEECH_ABORT",
    "APP_EV_SPEECH_END",
    "APP_EV_SPEECH_END_TIMEOUT",
    "APP_EV_ASR_LOCAL",
    "APP_EV_ASR_DM",
    "APP_EV_ASR_ERROR",
    "APP_EV_RESET",
    "APP_EV_PROMPT_COMPLETE",
    "APP_EV_ASR_TIMEOUT",
    "APP_VAD_BEGIN_TIMEOUT",
    "APP_CMD_IGNORE",
};

typedef struct {
    app_msg_type_t type;
    int wakeup_index;
    char *result;
} app_msg_t;

enum {
    WAIT_FOR_WAKEUP,
    WAIT_FOR_SPEECHING,
    SPEECHING,
    WAIT_FOR_ASR,
    PLAYING,
    MAX_STATE,
};

typedef enum {
    HOST_IDLE,
    HOST_WAKE_UPS,
    HOST_VOICE_PLAY,
    HOST_SPOT_MODE,
    HOST_FCT_TEST,
    HOST_OTA,
    HOST_GEARS,
}host_type;

static const char *state_table[] = {
    "WAIT_FOR_WAKEUP",
    "WAIT_FOR_SPEECHING",
    "SPEECHING",
    "WAIT_FOR_ASR",
    "PLAYING",
    "MAX_STATE",
};

static int wake_state_flag_last = 0;
static int wake_state_flag_this = 0;
static int bf_state_flag_last = 0;
static int bf_state_flag_this = 0;

static void *xlog_init_lock() {
    return gh_mutex_create();
}

static int xlog_take_lock(void *ptr) {
    gh_mutex_lock(ptr);
    return 0;
}

static int xlog_give_lock(void *ptr) {
    gh_mutex_unlock(ptr);
    return 0;
}

static void xlog_destroy_lock(void *ptr) {
    gh_mutex_destroy(ptr);
}

// 参数为原始音频通道数
static void factory_test_start(int channels){
    fct_start(channels);
    gh_delayms(100);
    thsdk_start_fct();
}

static void factory_test_end(){
    thsdk_close_fct();
    gh_delayms(100);
    fct_end();
}
/********************************************
 * asr_type:ASR_ENGINE_LOCAL  走本地识别
 *          ASR_ENGINE_DUI   走云端识别
 * *****************************************/
static int set_dui_or_local(asr_engine_e asr_type){
    return asr_set_dui_or_local(asr_type);
}

/********************************************
 * 恢复使用vos程序进行网络判断
 * *****************************************/
static void restore_net_check(){
    asr_restore_net_check();
}

/************************************************
 * 获取当前在线识别还是离线识别状态
 * ************************************************/
static asr_engine_e get_dui_or_local(){
    return asr_get_current_type();
}

/****************************************************
 * 设置在线识别方式为全双工或者半双工。请在程序启动之后设置，
 * 避免频繁动态切换。
 * dds_type :DDS_HALF_DUPLEX 半双工
 *           DDS_FULL_DUPLEX 全双工
 * 返回：-1：失败，语音识别流程中不可以设置
 *      0 :成功
 * ***************************************************/
static void set_dui_type(dds_type_e dds_type){
    return set_dds_type(dds_type);
}

/****************************************
 * 获取当前全双工还是半双工状态
 * ******************************************/
static dds_type_e get_dui_type(){
    return asr_get_duplex_type();
}

/*************************************************
 * 向太行固件设置参数
 * 示例：切换唤醒阈值：cmd = "{\"env\":\"words=xiao mei xiao mei;thresh=0.85;major=1;\"}"
 * 切换唤醒识别固件 cmd = "{\"type\":\"workMode\", \"mode\": 1}"
 * 切换通话固件 cmd = "{\"type\":\"workMode\", \"mode\": 2}"
 * ***********************************************/
static void set_info_to_th(const char *cmd){
    th_set_info(cmd);
}
/*****************************************************
 * 获取太行信息
 * 示例：获取当前太行是识别模式还是通话模式：cmd = "{\"type\":\"workMode\"}"
 * 结果在th_event_callback中获取
 * ***************************************************/
static void get_info_from_th(const char *cmd)
{
    th_get_info(cmd);
}

/*******************************************************************
 * 产测时候，计算mic一致性。如果是双麦产品，mic3 mic4填写-200
 * *****************************************************************/
static float clac_uniform(float mic1,float mic2,float mic3,float mic4){
    float max,min;
    max = mic1 > mic2 ? mic1:mic2;
    if(mic3!=-200 && mic4!=-200){
        max = max > mic3 ? max:mic3;
        max = max > mic4 ? max:mic4;
    }
    min = mic1 < mic2 ? mic1:mic2;
    if(mic3!=-200 && mic4!=-200){
        min = min < mic3 ? min:mic3;
        min = min < mic4 ? min:mic4; 
    }
    return (max - min);
}

void export_fct_result(char *result){
    thsdk_close_fct();
    printf("result = [%s]\r\n",result);
    //TODO   此处处理产测算法上报的结果
}

#ifdef DDS_ENABLE
static void send_native_response(char *result) {
    XLOG_I("send_native_response\r\n");
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_NATIVE_RESPONSE);
    if (result) {
        dds_msg_set_string(msg, "json", result);
    }
    dds_send(msg);
    dds_msg_delete(msg);
}
#endif

static void player_manager_callback(void *userdata,player_manager_state_t prev_state, player_manager_state_t cur_state) {
    const char *tt[] = {
        "IDLE",
        "PROMPT_PLAYING",
        "MEDIA_PLAYING",
        "MEDIA_PAUSED",
    };
    XLOG_W("[%s]=====>[%s]", tt[prev_state], tt[cur_state]);
}

static int beamforming_audio_write = 0;
static int beamforming_audio_callback(void *userdata, const void *buf, int len) {
    static int index = 1;
    if (!beamforming_audio_write) {
        beamforming_audio_write = 1;
        struct timeval now;
        gettimeofday(&now, NULL);
        char *text = NULL;
        int bytes = gh_strprintf(&text, "{\"index\":\"%d\", \"timestamp\":\"beamforming/bf-%ld.%03d.pcm\", \"op\":\"start\"}", index,now.tv_sec, now.tv_usec / 1000);
 //     device_upload_client_send_text(s_app.beamforming, text, bytes);
        gh_free(text);
    }
 // device_upload_client_send_binary(s_app.beamforming, buf, len);
    if(0 == bf_state_flag_last && 1 == bf_state_flag_this){ //pc连线
        bf_state_flag_this = 0;
        beamforming_audio_write = 0;
        index++;
    }      
    return 0;
}

static int wakeup_audio_write = 0;
static int wakeup_audio_callback(void *userdata, const void *buf, int len) {
    static int index = 1;
    if (!wakeup_audio_write) {
        wakeup_audio_write  = 1;
        struct timeval now;
        gettimeofday(&now, NULL);
        char *text = NULL;
        int bytes = gh_strprintf(&text, "{\"index\":\"%d\", \"timestamp\":\"wakeup/wakeup-%ld.%03d.pcm\", \"op\":\"start\"}",index, now.tv_sec, now.tv_usec / 1000);
 //     device_upload_client_send_text(s_app.wakeup, text, bytes);
        gh_free(text);
    }
 //   device_upload_client_send_binary(s_app.wakeup, buf, len);
    if(0 == wake_state_flag_last && 1 == wake_state_flag_this){
        wake_state_flag_this = 0;
        wakeup_audio_write = 0;
        index++;
    }    
    return 0;
}

static int asr_audio_write = 0;
static int asr_audio_callback(void *userdata, const void *buf, int len) {
    static int index = 0;
    struct timeval now;
    gettimeofday(&now, NULL);
    if (len == 0 || len == -1) {
        asr_audio_write = 0;
            char *text = NULL;
            index++;
            int bytes = gh_strprintf(&text, "{\"index\":\"%d\", \"timestamp\":\"asr/asr-%ld.%03d\", \"op\":\"%s\"}", index, now.tv_sec, now.tv_usec / 1000, len == 0 ? "stop" : "abort");
            gh_free(text);
    } else {
        if (!asr_audio_write) {
            char *text = NULL;
            index++;
            int bytes = gh_strprintf(&text, "{\"index\":\"%d\", \"timestamp\":\"asr/asr-%ld.%03d.pcm\", \"op\":\"start\"}", index, now.tv_sec, now.tv_usec / 1000);
            gh_free(text);
            asr_audio_write = 1;
        }
        asr_processor_feed(buf, len);
    }
    return 0;
}

/***************************************
 * FRONT_EV_TH_STATUS: 太行信息上报。
 *                     eg: {"type":"workMode","mode",1}   识别模式
 *                         {"type":"workMode","mode",2}   通话模式
 * FRONT_EV_WAKEUP_MAJOR   
 * FRONT_EV_WAKEUP_MINOR: 唤醒事件
 * **************************************/

static int th_event_callback(void *userdata, th_event_t ev, void *info, int len) {
    do {
        switch(ev){
            case FRONT_EV_TH_STATUS:
            {
                printf("FRONT_EV_TH_STATUS->[%s]\r\n",info);
            }
            break;
            case FRONT_EV_WAKEUP_MAJOR:
            case FRONT_EV_WAKEUP_MINOR:
            {
                app_msg_t out;
                memset(&out, 0, sizeof(out));
                out.type = (ev == FRONT_EV_WAKEUP_MAJOR) ? APP_EV_WAKEUP_MAJOR : APP_EV_WAKEUP_MINOR;
				gh_queue_send(s_app.q, &out, -1);
            }
            break;
        }
    } while (0);
    return 0;
}

// 产测feed音频
static int th_fct_audio_callback(const char *buf, int len){
    fct_audio_feed(buf, len);
    return 0;
}

static int th_beamforming_audio_callback(void *userdata, const void *buf, int len)
{
    // 太行送来bf音频，送asr模块，请求dds
    asr_processor_feed(buf, len);
    return 0;
}

#ifdef DDS_ENABLE

static const char *player_event_table[]={
    "PLAYER_EV_RUNNING",
    "PLAYER_EV_PAUSED",
    "PLAYER_EV_RESUMED",
    "PLAYER_EV_STOPPED",
    "PLAYER_EV_DONE",
};

/*
使用tts合成播报回调
//运行中(player_play发起播放后触发)    PLAYER_EV_RUNNING,
//暂停播放    PLAYER_EV_PAUSED,
//继续播放    PLAYER_EV_RESUMED,
//播放被终止  PLAYER_EV_STOPPED,
//播放完成    PLAYER_EV_DONE,
*/
static void tts_play_cb(player_t *player, player_ev_t event, void *userdata) {
    XLOG_I("player event [%s]\r\n", player_event_table[event]);
    //tts正常播放结束时回调
    if (event == PLAYER_EV_DONE) {
        XLOG_I("tts play over");
    }
}

static int tts_play_job(taskstack_task_t *job) {
    player_manager_play_t play = {
        .target = {
            .target = job->argv[0],
            .preprocessor = DEFAULT_HTTP_PREPROCESSOR,
            .cb = tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
}

static void tts_play_job_free(taskstack_task_t *job) {
    gh_free(job->argv[0]);
    gh_free(job->argv);
}

/*
 * "text":"大家好",
 * "voiceId":"音色ID字串",
 * "speed":1.0,
 * "volume":80
 * voiceId请参考https://www.duiopen.com/docs/ct_cloud_TTS_Voice
 */
//tts_custom("zhilingfa", "大家好", 80, 1.0);

static int tts_custom(const char *voiceId, const char *text, int volume, double speed){
    int ret = asr_dds_custom_tts(voiceId, text, volume, speed);
    return ret;
}

static int dds_callback(void *userdata, struct dds_msg *msg) {
    int type;
    dds_msg_get_integer(msg, "type", &type);
    switch(type){
        case DDS_EV_OUT_TTS: {
            char *source = NULL;
            char *str_value = NULL;
            dds_msg_get_string(msg, "source", &source);
            dds_msg_get_string(msg, "speakUrl", &str_value);
            if (source && (0 == strcmp(source, "external"))){
                XLOG_I("tts -> speakurl = [%s] \r\n",str_value);
            }else{
                return -1;
            }
            if (0 == strncmp(str_value, "https://", 8)) {
                str_value += 8;
            }
            taskstack_task_t job;
            job.do_job = tts_play_job;
            job.cleanup = tts_play_job_free;
            job.argc = 1;
            job.argv = (char **)gh_calloc(1, sizeof(char *));
            job.argv[0] = gh_strdup(str_value);
            taskstack_push(&s_app.app_stack, &job);
        }
        break;
        case DDS_EV_OUT_ERROR:{
            char *source = NULL;
            char *error = NULL;
            int errorid = NULL;
            dds_msg_get_string(msg, "source", &source);
            dds_msg_get_string(msg, "error", &error);
            dds_msg_get_integer(msg,"errorId",&errorid);
            XLOG_I("error type [%s],error [%s],errorid [%d]",source,error,errorid);
        }
        break;
        case DDS_EV_OUT_DUI_LOGIN:{
            char *device_name = NULL;
            if (!dds_msg_get_string(msg, "deviceName", &device_name))
            {
                gh_event_bits_t bits = BIT_DDS_READY;
                gh_event_group_set_bits(s_app.appdone, &bits); 
            }
        }
        break;
        default:
        break;
    }
    return 0;
}
#endif

static int asr_event_callback(void *userdata, asr_event_t ev, const void *result, int len) {
    do {
        app_msg_t out;
        memset(&out, 0, sizeof(out));
        if (ev == ASR_EV_LOCAL) {
            out.type = APP_EV_ASR_LOCAL;
            out.result = gh_strndup(result, len);
        } else if (ev == ASR_EV_CLOUD) {
            break;
        } else if (ev == ASR_EV_DM) {
            out.type = APP_EV_ASR_DM;
            out.result = gh_strndup(result, len);
        } else if (ev == ASR_EV_ERROR) {
            out.type = APP_EV_ASR_ERROR;
        } else if (ev == ASR_EV_TIMEOUT) {
            out.type = APP_EV_ASR_TIMEOUT;
        }else if (ev = ASR_VAD_BEGIN_TIMEOUT){
            out.type = APP_VAD_BEGIN_TIMEOUT;  // vad未检测到人声
        }else if (ev == CMD_IGNORE){
            out.type = APP_CMD_IGNORE; 
        }
        gh_queue_send(s_app.q, &out, -1);
    } while (0);
    return 0;
}

static int speech_continue_job(taskstack_task_t *job) {
    //开启识别进行对话
    asr_engine_e asr_type = get_dui_or_local();
    dds_type_e dds_type = get_dui_type();
    XLOG_I("asr_type [%d] dds_type [%d]\r\n",asr_type,dds_type);
    if(asr_type == ASR_ENGINE_DUI && dds_type == DDS_FULL_DUPLEX)
        return WAIT_FOR_WAKEUP;
    asr_processor_start();
    return WAIT_FOR_ASR;
}

#ifdef DDS_ENABLE
static int asr_result_process_job(taskstack_task_t *job) {
    //speakUrl已经处理过，此处不再处理
    //只需要依次检测识别结果是哪种类型
    //1.音乐播放
    //2.native api
    //3.command
    //4.继续对话
    //5.无事可做
    cJSON *js = cJSON_Parse(job->argv[0]);
    cJSON *dm_js = cJSON_GetObjectItem(js, "dm");
    cJSON *widget_js = cJSON_GetObjectItem(dm_js, "widget");
    cJSON *shouldEndSession_js = cJSON_GetObjectItem(dm_js, "shouldEndSession");

    if (widget_js) {
        bool m3u8_flag = false;
        cJSON *content_js = cJSON_GetObjectItem(widget_js, "content");
        cJSON *type_js = cJSON_GetObjectItem(widget_js, "type");
        if (content_js && type_js && 0 == strcmp(type_js->valuestring, "media")) {
            int count = cJSON_GetArraySize(content_js);
            play_preprocessor_t pproc = DEFAULT_HTTP_PREPROCESSOR;
            player_manager_playlist_item_t pi[5];
            for(int i=0; i<5; i++){
				memset((void *)&pi[i], 0, sizeof(player_manager_playlist_item_t));
				pi[i].target.preprocessor = pproc;
            }
            
            if (count > 0) {
                if (count > 5) count = 5;
                //更新播放列表并播放
                int i;
                for (i = 0; i < count; i++) {
                    cJSON *item_js = cJSON_GetArrayItem(content_js, i);
                    cJSON *linkUrl_js = cJSON_GetObjectItem(item_js, "linkUrl");
                    cJSON *title_js = cJSON_GetObjectItem(item_js, "title");
                    cJSON *subTitle_js = cJSON_GetObjectItem(item_js, "subTitle");
                    cJSON *label_js = cJSON_GetObjectItem(item_js, "label");
                    cJSON *extra_js = cJSON_GetObjectItem(item_js, "extra");
                    if (extra_js) {
                        cJSON *resType_js = cJSON_GetObjectItem(extra_js, "resType");
                        if (resType_js && 0 == strcmp(resType_js->valuestring, "m3u8")){
                            m3u8_flag = true;
                            // XLOG_I("resType: %s",resType_js->valuestring);
                        }
                    }
                    pi[i].target.target = linkUrl_js->valuestring;
                    pi[i].title = title_js ? title_js->valuestring : NULL;
                    pi[i].subtitle = subTitle_js ? subTitle_js->valuestring : NULL;
                    pi[i].label= label_js ? label_js->valuestring : NULL;
                }
                if(m3u8_flag) {   
                    player_manager_play_t play = {
                        .target = {
                            .target = pi[0].target.target,
                            .preprocessor = DEFAULT_M3U8_PREPROCESSOR,
                        },
                    };
                    player_manager_play(&play, false);
                    m3u8_flag = false;
                }
                else {
                    player_manager_playlist_update(pi, count);
                }
            }
            cJSON_Delete(js);
            return WAIT_FOR_WAKEUP;
        }
    }
    cJSON *command_js = cJSON_GetObjectItem(dm_js, "command");
    cJSON *native_api_js = cJSON_GetObjectItem(dm_js, "api");
    cJSON *dataFrom_js = cJSON_GetObjectItem(dm_js, "dataFrom");
    // cJSON *param_js = cJSON_GetObjectItem(dm_js, "param");
    bool is_native = false;
    if (dataFrom_js && 0 == strcmp(dataFrom_js->valuestring, "native") && native_api_js && 0 != strlen(native_api_js->valuestring)) {
        is_native = true;
        //native api
        //根据具体业务设置pending job
        //TODO
    } else if (command_js) {
        app_skill_handler(command_js,is_native); 
        //TODO
    }
    if (shouldEndSession_js && shouldEndSession_js->type == cJSON_False) {
        if (!is_native)
        {
            // native api时不需要语音多轮
            //进行多论对话
            asr_engine_e asr_type = get_dui_or_local();
            dds_type_e dds_type = get_dui_type();
            if (asr_type == ASR_ENGINE_DUI && dds_type == DDS_FULL_DUPLEX){
                cJSON_Delete(js);
                return WAIT_FOR_WAKEUP;
            }
            asr_processor_start();
            cJSON_Delete(js);
            return WAIT_FOR_ASR;
        }
    }else if(shouldEndSession_js && shouldEndSession_js->type == cJSON_True){
        // 停止对话
        XLOG_I("dialogue over");
        asr_processor_stop();
    }else{}
    //native api时需要等待response
    if (is_native) {    
        //TODO
        send_native_response(NULL); 
    }
    cJSON_Delete(js);
    return WAIT_FOR_WAKEUP;
}
#endif

static void asr_result_process_job_free(taskstack_task_t *job) {
    gh_free(job->argv[0]);
    gh_free(job->argv);
}

static void asr_tts_play_job_free(taskstack_task_t *job) {
    gh_free(job->argv[0]);
    gh_free(job->argv);
}

/*
程序播放回调
//运行中(player_play发起播放后触发)    PLAYER_EV_RUNNING,
//暂停播放    PLAYER_EV_PAUSED,
//继续播放    PLAYER_EV_RESUMED,
//播放被终止    PLAYER_EV_STOPPED,
//播放完成    PLAYER_EV_DONE,
*/
static void asr_tts_play_cb(player_t *player, player_ev_t event, void *userdata) {
    XLOG_I("player event [%s]\r\n", player_event_table[event]);
    //tts正常播放结束时回调
    if (event == PLAYER_EV_DONE || event == PLAYER_EV_STOPPED) {
        app_msg_t out = {
            .type = APP_EV_PROMPT_COMPLETE,
        };
        gh_queue_send(s_app.q, &out, -1);
    }
}
#ifdef DDS_ENABLE
static int asr_tts_play_job(taskstack_task_t *job) {
    //执行对话播放结果播放任务，同时添加对话正常结束时，解析识别结果的任务
    // 这里发送播报开始
    player_manager_play_t play = {
        .target = {
            .target = job->argv[0],
            .preprocessor = DEFAULT_HTTP_PREPROCESSOR,
            .cb = asr_tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);

    //job->argv[1]是识别结果，此处直接赋值给asr_result_process_job使用

    taskstack_task_t new_job;
    new_job.do_job = asr_result_process_job;
    new_job.cleanup = asr_result_process_job_free;
    new_job.argc = 1;
    new_job.argv = (char **)gh_calloc(1, sizeof(char *));
    new_job.argv[0] = job->argv[1];
    taskstack_push(&s_app.fsm_stack, &new_job);
    return PLAYING;
}

extern int g_th_cb_time;
extern int TH_WAKE_UP_TIME;

static void app_do_parse_dui(char *result) {
	int ret = 0, hy_speek_flag = 1;
	g_th_cb_time = TH_WAKE_UP_TIME;
    cJSON *js = cJSON_Parse(result);
    if (!js) return;
    cJSON *dm_js = cJSON_GetObjectItem(js, "dm");
    cJSON *shouldEndSession_js = NULL;
	cJSON *nlg_js = NULL;
	char*  wake_up_stop = 	"[{\"ctrlType\":\"voice_wakeup\",\"value\":\"stop\"}]";
    if (dm_js) {
        shouldEndSession_js = cJSON_GetObjectItem(dm_js, "shouldEndSession");
        cJSON *input_js = cJSON_GetObjectItem(dm_js, "input");
		if(input_js && 0 == strcmp(input_js->valuestring, "") && shouldEndSession_js && shouldEndSession_js->type == cJSON_False) {
            XLOG_I("%s",input_js->valuestring);
            cJSON *input_js = cJSON_GetObjectItem(dm_js, "input");
            taskstack_task_t job = {
                .do_job = speech_continue_job,
            };
            taskstack_push(&s_app.fsm_stack, &job);
            cJSON_Delete(js);
            return;
        }
		if (input_js) {
			/*hongyan语义解析*/
			ret = voice_on_prase(input_js->valuestring);
			if (-3 == ret) {/*2s内重复语音，不处理*/
				cJSON_Delete(js);
				return;
			} else if (0 == ret) {/*命中本地语音匹配规则，返回*/
				printf("\"%s\",hit local voice rules\r\n", input_js->valuestring);
				cJSON_Delete(js);
				return;
			} else {
			/*继续往下进行在线技能匹配*/
			}
		}

#if 1
		/*过滤"我听不懂你说什么"播放*/
    	nlg_js = cJSON_GetObjectItem(dm_js, "nlg");
		if (nlg_js && (strstr(nlg_js->valuestring, "听不懂") ||
			strstr(nlg_js->valuestring, "抱歉"))) {
			hy_speek_flag = 0;
		}
		
#endif
	}

	//"error":{"errMsg":"dialogue timeout.","errId":"010416"}
	cJSON *error_js = cJSON_GetObjectItem(js, "error");
	if (error_js) {
		cJSON *errid_js = cJSON_GetObjectItem(error_js, "errId");
		if (errid_js && 0 == strcmp(errid_js->valuestring, "010416")) {
			printf("DUI connect over time, stop asr\r\n");//超时退出，为了不打断音乐等，不播放 “好的，再见”
			g_th_cb_time = -1;
			hy_speek_flag = 0;
			asr_processor_stop();
			cJSON_Delete(js);
    		return;
		} else if (0 == strcmp(errid_js->valuestring, "010403")) {
			printf("DUI exit commond\r\n");//主动退出命令，需要播放 “好的，再见”
			g_th_cb_time = -1;
		} else {

		}
	}
 
	/*0508 qiudong 注释*/
	/*思必驰代码，解析speakUrl，调用接口播放DUI平台返回语音*/
	/*此处语音播放之后就返回了，播放之后不再解析其他字段，类似“音乐”“天气”等播报类技能走该分支*/
	/*智能家居技能返回一般没有speakUrl字段，不会走该分支*/
    cJSON *speakUrl_js = cJSON_GetObjectItem(js, "speakUrl");
    if (speakUrl_js && hy_speek_flag) {
        //对话合成音先优先播放
        char *speakUrl = speakUrl_js->valuestring;
#if 1
        if (0 == strncmp(speakUrl, "https://", 8)) {
            //https播放会比较慢
            speakUrl += 8;
        }
#endif
        //增加对话结果播放任务
        //开始播报任务
        taskstack_task_t job;
        job.do_job = asr_tts_play_job;
        job.cleanup = asr_tts_play_job_free;
        job.argc = 2;
        job.argv = (char **)gh_calloc(2, sizeof(char *));
        job.argv[0] = gh_strdup(speakUrl);
        job.argv[1] = gh_strdup(result);
        taskstack_push(&s_app.fsm_stack, &job);
        cJSON_Delete(js);
        return;
    }

	/*开始技能字段解析*/
    // cJSON *dm_js = cJSON_GetObjectItem(js, "dm");
    cJSON *widget_js = cJSON_GetObjectItem(dm_js, "widget");

    cJSON *command_js = cJSON_GetObjectItem(dm_js, "command");
    cJSON *native_api_js = cJSON_GetObjectItem(dm_js, "api");
    cJSON *dataFrom_js = cJSON_GetObjectItem(dm_js, "dataFrom");
    // cJSON *param_js = cJSON_GetObjectItem(dm_js, "param");
    bool is_native = false;
    if (dataFrom_js && 0 == strcmp(dataFrom_js->valuestring, "native") && native_api_js && 0 != strlen(native_api_js->valuestring)) {
        //native api
        //根据具体业务设置pending job
        is_native = true;
        //native api
        //根据具体业务设置pending job
        //TODO
        app_skill_handler(dm_js,is_native); // 业务上的native技能在此设置
    } else if (command_js) {
        //TODO
        app_skill_handler(command_js,is_native); // 控制技能执行函数 /*qiuodng:智能家居技能解析走该分支*/
    }
    if (shouldEndSession_js && shouldEndSession_js->type == cJSON_False) {
        if (!is_native) {
            //native api时不需要语音多轮
            //开启多轮对话
            //添加继续对话任务
            taskstack_task_t job = {
                .do_job = speech_continue_job,
            };
            taskstack_push(&s_app.fsm_stack, &job);
            cJSON_Delete(js);
            return;
        }
    }else if(shouldEndSession_js && shouldEndSession_js->type == cJSON_True){
        // 停止对话
        XLOG_I("dialogue over");
        asr_processor_stop();
    }else{}
    //native api时需要等待response
    if (is_native) {    
        send_native_response(NULL);
    }
    cJSON_Delete(js);
    return;
}
#endif

const char *asr_result_table[] = {
    "ASR_UNKONW",
    "ASR_IN_TH_RESULT",
    "ASR_IN_TH_QUICK_CMD",
    "ASR_IN_AP_RESULT",
};

/*************************************************************************
 * function:处理离线识别结果。通过conf置信度，判断是否进入离线识别结果处理函数中
 * ***********************************************************************/
static int app_do_parse_asr(char *result, asr_result_type_e asr_result_type){
    XLOG_I("app_do_parse_asr result [%s] [%s]\r\n",result, asr_result_table[asr_result_type]);
    cJSON *result_js = cJSON_Parse(result);
    if(!result_js) return -1;
    switch (asr_result_type)
    {
    case ASR_IN_AP_RESULT:{
        cJSON *pinyin_js = cJSON_GetObjectItem(result_js, "pinyin");
		cJSON *rec_js = cJSON_GetObjectItem(result_js,"rec");
        cJSON *conf_js = cJSON_GetObjectItem(result_js, "conf");
        if (!pinyin_js && !conf_js){
            cJSON_Delete(result_js);
            return -1;
        }
        if (conf_js->valuedouble < ASR_RESULT_CONF)   
        {
            XLOG_I("asr result conf is too low,discard\r\n");
            cJSON_Delete(result_js);
            return -2;
        }
        //handle_offline_protol(pinyin_js->valuestring);
        voice_on_prase(rec_js->valuestring);
        cJSON_Delete(result_js);
    }
        break;
    case ASR_IN_TH_RESULT:{
        cJSON *eventInfo_js = cJSON_GetObjectItem(result_js,"eventInfo");
        if(!eventInfo_js) {
            cJSON_Delete(result_js);
            return -1;
        }
        cJSON *pinyin_js = cJSON_GetObjectItem(eventInfo_js,"pinyin");
		cJSON *rec_js = cJSON_GetObjectItem(eventInfo_js,"rec");
        cJSON *conf_js = cJSON_GetObjectItem(eventInfo_js,"conf");
        if(!pinyin_js && !conf_js){
            cJSON_Delete(result_js);
            return -1;
        }
        if(conf_js->valuedouble < ASR_RESULT_CONF){
            XLOG_I("fast command conf is too low,discard\r\n");
            cJSON_Delete(result_js);
            return -2;             
        }
        //handle_offline_protol(pinyin_js->valuestring);
        voice_on_prase(rec_js->valuestring);
        cJSON_Delete(result_js);
    }
    break;
    case ASR_IN_TH_QUICK_CMD:{
        cJSON *wakeupWord_js = cJSON_GetObjectItem(result_js,"wakeupWord");
        cJSON *conf_js = cJSON_GetObjectItem(result_js,"conf");
        if(!wakeupWord_js && !conf_js){
            cJSON_Delete(result_js);
            return -1;
        }
        // 数字宏定义
        if(conf_js->valuedouble < ASR_RESULT_CONF){
            XLOG_I("fast command conf is too low,discard\r\n");
            cJSON_Delete(result_js);
            return -2;            
        }
        //handle_offline_protol(wakeupWord_js->valuestring);
        voice_on_prase(wakeupWord_js->valuestring);
        cJSON_Delete(result_js);
    }
        break;
    default:
        cJSON_Delete(result_js);
        break;
    }
    return 0;
}

static void task_stack_clean() {
    int depth = taskstack_get_depth(&s_app.fsm_stack);
    while (depth > 0) {
        taskstack_pop_clean(&s_app.fsm_stack);
        depth--;
    }
}

static void do_wakeup(app_msg_t *in) {
    player_manager_stop();
    task_stack_clean();
#if 1
    player_manager_play_t play = {
        .target = {
            .target = s_app.words[in->wakeup_index].audio,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
#else
	char play_cmd[1024] = {0};
	int i = 0;
	sprintf(play_cmd, "aplay -Dplug:dmix %s", s_app.words[in->wakeup_index].audio);
	for (i = 0; i < 3; i++) {
		if (0 == system(play_cmd)) {
			break;
		}
		sleep(2);
	}
#endif
}

typedef int (*app_run_cb)(app_msg_t *in);

static int wakeup_major_when_wait_for_wakeup(app_msg_t *in) {
    //唤醒之后就开启识别的原因
    //1.如果是云端识别，此时可以提前建立连接
    //2.保证检测到语音开始信号时，识别的输入buf已经可以写入（避免数据丢失）
#ifdef LOCAL_ASR_IN_TH
    asr_engine_e asr_current_type = get_dui_or_local();
    printf("asr_current_type = [%d]\r\n",asr_current_type);
    if(asr_current_type == ASR_ENGINE_LOCAL){
        XLOG_I("use th local asr,wait for asr result:\r\n");
        do_wakeup(in);
        return WAIT_FOR_WAKEUP;
    }
#endif
    do_wakeup(in);
    asr_processor_start();      
    return WAIT_FOR_ASR;
}

static int wakeup_minor_when_wait_for_wakeup(app_msg_t *in) {
#ifdef LOCAL_ASR_IN_TH
    asr_engine_e asr_current_type = get_dui_or_local();
    if(asr_current_type == ASR_ENGINE_LOCAL){
        XLOG_I("use th local asr,wait for asr result:\r\n");
        do_wakeup(in);
        return WAIT_FOR_WAKEUP;
    }
#endif
    do_wakeup(in);
   asr_processor_start(); 
   return WAIT_FOR_ASR;
}

static int asr_dm_when_wait_for_wakeup(app_msg_t*in) {
    #ifdef DDS_ENABLE
    app_do_parse_dui(in->result);
    #endif
    return WAIT_FOR_WAKEUP;
}

static int wakeup_major_when_wait_for_speeching(app_msg_t *in) {
    do_wakeup(in);
    asr_processor_start();
    return WAIT_FOR_SPEECHING;
}

static int wakeup_minor_when_wait_for_speeching(app_msg_t *in) {
    do_wakeup(in);
    asr_processor_start();
    return WAIT_FOR_SPEECHING;
}

static int speech_begin_when_wait_for_speeching(app_msg_t *in) {
    return SPEECHING;
}

static int speech_begin_timeout_when_wait_for_speeching(app_msg_t *in) {
    asr_processor_reset();
    player_manager_play_t play = {
        .target = {
            .target = s_app.begin_timeout_prompt,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
            .cb = asr_tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
    return PLAYING;
}

static int asr_error_when_wait_for_speeching(app_msg_t *in) {
    player_manager_play_t play = {
        .target = {
            .target = s_app.error_prompt,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
            .cb = asr_tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
    return PLAYING;
}

static int wakeup_major_when_speeching(app_msg_t *in) {
    do_wakeup(in);
    asr_processor_start();
    return WAIT_FOR_ASR;
}

static int wakeup_minor_when_speeching(app_msg_t *in) {
    do_wakeup(in);
    asr_processor_start();
    return WAIT_FOR_ASR;
}

static int speech_end_when_speeching(app_msg_t *in) {
    asr_processor_stop();
    return WAIT_FOR_ASR;
}

static int speech_abort_when_speeching(app_msg_t *in) {
    asr_processor_start();
    return WAIT_FOR_ASR;
}

static int speech_end_timeout_when_speeching(app_msg_t *in) {
    asr_processor_reset();
    player_manager_play_t play = {
        .target = {
            .target = s_app.end_timeout_prompt,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
            .cb = asr_tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
    return PLAYING;
}

static int asr_error_when_speeching(app_msg_t *in) {
    player_manager_play_t play = {
        .target = {
            .target = s_app.error_prompt,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
            .cb = asr_tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
    return PLAYING;
}

static int wakeup_major_when_wait_for_asr(app_msg_t *in) {
    do_wakeup(in);
    asr_processor_reset();
    return WAIT_FOR_ASR;
}

static int wakeup_minor_when_wait_for_asr(app_msg_t *in) {
    do_wakeup(in);
    asr_processor_reset();;
    return WAIT_FOR_ASR;
}

static int asr_error_when_wait_for_asr(app_msg_t *in) {
    player_manager_play_t play = {
        .target = {
            .target = s_app.error_prompt,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
            .cb = asr_tts_play_cb,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
    return PLAYING;
}

static int asr_timeout_when_wait_for_asr(app_msg_t *in){
    return WAIT_FOR_WAKEUP;
}

static int vad_begin_time_out_when_wait_for_asr(app_msg_t *in){
    XLOG_I("VAD can not detection sound\r\n");
    return WAIT_FOR_WAKEUP;
}

static int vad_run_when_wait_for_asr(app_msg_t *in){
    return WAIT_FOR_WAKEUP;
}

static int vad_run_when_playing(app_msg_t *in){
    return WAIT_FOR_WAKEUP;
}

#ifdef DDS_ENABLE
static int asr_dm_when_wait_for_asr(app_msg_t *in) {
    XLOG_I("asr_dm_when_wait_for_asr\r\n");
    app_do_parse_dui(in->result);
    return WAIT_FOR_WAKEUP;
}
#endif

static int asr_local_when_wait_for_asr(app_msg_t *in) {
    XLOG_I("asr  result-[%s]\r\n",in->result);
    asr_result_type_e asr_result_type = ASR_IN_AP_RESULT;
    app_do_parse_asr(in->result, asr_result_type);
    return WAIT_FOR_WAKEUP;
}

static int wakeup_major_when_playing(app_msg_t *in) {
#ifdef LOCAL_ASR_IN_TH
    if(asr_type == ASR_ENGINE_LOCAL){
        XLOG_I("use th local asr,wait for asr result:\r\n");
        do_wakeup(in);
        return WAIT_FOR_WAKEUP;
    }
#endif
    do_wakeup(in);
    asr_processor_start();
    return WAIT_FOR_ASR;
}

static int wakeup_minor_when_playing(app_msg_t *in) {
#ifdef LOCAL_ASR_IN_TH
    if(asr_type == ASR_ENGINE_LOCAL){
        XLOG_I("use th local asr,wait for asr result:\r\n");
        do_wakeup(in);
        return WAIT_FOR_WAKEUP;
    }
#endif
    do_wakeup(in);
    asr_processor_start();
    return WAIT_FOR_ASR;
}

static int prompt_complete_when_playing(app_msg_t *in) {
    //TODO
    XLOG_I("play done prompt_complete_when_playing\r\n");
    return WAIT_FOR_WAKEUP;
}

static const app_run_cb app_run_table[MAX_STATE][MAX_MSG] = {
    [WAIT_FOR_WAKEUP] = {
        [APP_EV_WAKEUP_MAJOR] = wakeup_major_when_wait_for_wakeup,
        [APP_EV_WAKEUP_MINOR] = wakeup_minor_when_wait_for_wakeup,
#ifdef DDS_ENABLE
        [APP_EV_ASR_DM] = asr_dm_when_wait_for_asr,
#endif
    },
    [WAIT_FOR_SPEECHING] = {
        [APP_EV_WAKEUP_MAJOR] = wakeup_major_when_wait_for_speeching,
        [APP_EV_WAKEUP_MINOR] = wakeup_minor_when_wait_for_speeching,
        [APP_EV_SPEECH_BEGIN] = speech_begin_when_wait_for_speeching,
        [APP_EV_SPEECH_BEGIN_TIMEOUT] = speech_begin_timeout_when_wait_for_speeching,
        [APP_EV_ASR_ERROR] = asr_error_when_wait_for_speeching,
    },
    [SPEECHING] = {
        [APP_EV_WAKEUP_MAJOR] = wakeup_major_when_speeching,
        [APP_EV_WAKEUP_MINOR] = wakeup_minor_when_speeching,
        [APP_EV_SPEECH_END] = speech_end_when_speeching,
        [APP_EV_SPEECH_ABORT] = speech_abort_when_speeching,
        [APP_EV_SPEECH_END_TIMEOUT] = speech_end_timeout_when_speeching,
        [APP_EV_ASR_ERROR] = asr_error_when_speeching,
    },
    [WAIT_FOR_ASR] = {
        [APP_EV_WAKEUP_MAJOR] = wakeup_major_when_wait_for_asr,
        [APP_EV_WAKEUP_MINOR] = wakeup_minor_when_wait_for_asr,
        [APP_EV_ASR_ERROR] = asr_error_when_wait_for_asr,
#ifdef DDS_ENABLE
        [APP_EV_ASR_DM] = asr_dm_when_wait_for_asr,
#endif
        [APP_EV_ASR_LOCAL] = asr_local_when_wait_for_asr,
        [APP_EV_ASR_TIMEOUT] = asr_timeout_when_wait_for_asr,
        [APP_VAD_BEGIN_TIMEOUT] = vad_begin_time_out_when_wait_for_asr,
        [APP_CMD_IGNORE] = vad_run_when_wait_for_asr,
    },
    [PLAYING] = {
        [APP_EV_WAKEUP_MAJOR] = wakeup_major_when_playing,
        [APP_EV_WAKEUP_MINOR] = wakeup_minor_when_playing,
        [APP_EV_PROMPT_COMPLETE] = prompt_complete_when_playing,
        [APP_CMD_IGNORE] = vad_run_when_playing,
#ifdef DDS_ENABLE
        [APP_EV_ASR_DM] = asr_dm_when_wait_for_asr,
#endif
    },
};

static void taskstack_run(){
    taskstack_init(&s_app.app_stack, 10);
    while(1){
        taskstack_task_t job;
        if (0 == taskstack_pop(&s_app.app_stack, &job)) {
            job.do_job(&job);
            if (job.cleanup) {
                job.cleanup(&job);
            }
        
		}
	gh_delayms(10);
    }
    taskstack_destroy(&s_app.app_stack);
}

static void sigint_handler(int sig)
{
    exit(0);
}

static int get_usb_dev()
{
    unlink(SAVE_USB_INFO_FILE);
    system(SAVE_USB_CMD);
    return 0;
}

static int search_usb()
{
    get_usb_dev();
    char *usb_info = NULL;
    int file_len = 0;
    if(access(SAVE_USB_INFO_FILE, F_OK) != -1){
        file_len = get_filesize(SAVE_USB_INFO_FILE);
        usb_info = file_read(SAVE_USB_INFO_FILE);
        XLOG_I("usb_info = [%s]",usb_info);
        if(usb_info && strstr(usb_info, TH_USB_PVID_BOOT)){
            if(usb_info) free(usb_info);
            return 1;
        }else if(usb_info && strstr(usb_info, TH_USB_RUN)){
            if(usb_info) free(usb_info);
            return 2;
        }else{
            if(usb_info) free(usb_info);
            return 0;
        }
    }else{
        return -1;
    }
}

static void app_usb_check(){
    int usb_flag = 0;
    int check_count = 0;
    while(1){
        usb_flag = search_usb();
        if(usb_flag == 1 || usb_flag == 2){
            XLOG_I("TH USB is exist");
            break;
        }
        check_count++;
        if(check_count > 60)
        {
            XLOG_E("check USB time out, th usb not exist");
            exit(0);
        }
        gh_delayms(1000);
    }
}

static void app_net_check(){
	int i = 0;
    if(access(VOS_AUTH_FILE, F_OK) == -1)
    {
        while(1){
            int net_ret = 0;
            net_ret = net_status_check();
			if (0 == ++i % 20) {
				XLOG_I("current net status [%d] \r\n",net_ret);
				printf("首次使用语音功能, 请先联网授权\r\n");
			}
            if(1 == net_ret) break;
            gh_delayms(200);
        }
    }else{
        XLOG_I("profile is exist\r\n");
    }
}

static char *dds_type_table[]={
    "DDS_HALF_DUPLEX",
    "DDS_FULL_DUPLEX",
};

static const char *asr_engin_table[] = {
    "asr_engin_unknown",
    "ASR_ENGINE_LOCAL",
    "ASR_ENGINE_DUI",
    "ASR_ENGINE_BOTH",
};

static void test_run(){
    char a;
    while(1){
        a = getc(stdin);
        switch(a){
            case '1':
                tts_custom("qiumum_0gushi", "我们过了江，进了车站。我买票，他忙着照看行李。行李太多", 80, 1.0);
            break;
            case '2':
                printf("开启产测\r\n");
                factory_test_start(4);
            break;
            case '3':
                printf("使用唤醒识别固件\r\n");
                char * set_msg_asr = "{\"type\":\"workMode\", \"mode\": 1}";
                set_info_to_th(set_msg_asr);
                char * get_msg = "{\"type\":\"workMode\"}";
                get_info_from_th(get_msg);
            break;
            case '4':
                printf("使用通话固件\r\n");
                char * set_msg_talk = "{\"type\":\"workMode\", \"mode\": 2}";
                set_info_to_th(set_msg_talk);
                get_info_from_th(get_msg);
            break;
            case '5':
                printf("使用全双工\r\n");
                set_dui_type(DDS_FULL_DUPLEX);
            break;
            case '6':
                printf("使用半双工\r\n");
                set_dui_type(DDS_HALF_DUPLEX);
            break;
            case '7':
                printf("手动关闭rb\r\n");
                asr_processor_stop();
            break;
            case '8':
                printf("使用离线方式\r\n");
                set_dui_or_local(ASR_ENGINE_LOCAL);
            break;
            case '9':
                printf("使用在线方式\r\n");
                set_dui_or_local(ASR_ENGINE_DUI);
            break;
            case 'a':
                printf("恢复使用vos网络判断\r\n");
                restore_net_check();
            break;
            case 'b':
                printf("获取当前设备状态\r\n");
                asr_engine_e asr_engine = get_dui_or_local();
                dds_type_e dds_type = get_dui_type();
                printf("[%s] -- [%s] \r\n",dds_type_table[dds_type],asr_engin_table[asr_engine]);
            break;
            default:
            break;
        }
        gh_delayms(20);
    }
}

static int th_login_callback(int login_flag)
{
    gh_event_bits_t bits = BIT_TH_LOGIN;
    if(login_flag)
        gh_event_group_set_bits(s_app.appdone, &bits);
}

int get_current_exe_path(char* inbuf, int len)
{

	char* current_absolute_path = inbuf;

	//获取当前程序绝对路径
	int i = 0;
	int cnt = readlink("/proc/self/exe", current_absolute_path, len);

	if (cnt == len)
	{
		printf("***Error***\n");
		return -1;
	}

	for (i = cnt; i >= 0; i--)
	{
        if (current_absolute_path[i] == '/')
        {
            current_absolute_path[i + 1] = '\0';
            break;
        }
	}
	printf("current absolute path:%s\n", current_absolute_path);
	return 0;
}


int main(int argc, char **argv) {
    printf("********************************************************\r\n");
    printf("*****思必驰VOS程序启动***********************************\r\n");
    printf("*****当前版本号 : [%s]***********************\r\n",VOS_VERSION);
    printf("********************************************************\r\n");

	/*鸿雁语音识别配置规则初始化*/
	voice_rules_prase_init();
	char current_path[256] = {0};
	char vos_cfg_path[256] = {0};
	
	get_current_exe_path(current_path, sizeof(current_path));
	sprintf(vos_cfg_path, "%s%s", current_path, "app_cfg.json");

    if(access(VOS_OPERATING_DIR, F_OK) == -1){
        printf("vos no %s, create it\r\n",VOS_OPERATING_DIR);
        char vos_auth_path[64] = {0};
        sprintf(vos_auth_path,"mkdir %s",VOS_OPERATING_DIR);
        system(vos_auth_path);
    }

    {
        xlog_cfg_t cfg = {
            .init_lock = xlog_init_lock,
            .take_lock = xlog_take_lock,
            .give_lock = xlog_give_lock,
            .destroy_lock = xlog_destroy_lock,
        };
        xlog_init(&cfg);
        xlog_set_output_level(XLOG_LEVEL_DEBUG);
    }
    s_app.appdone = gh_event_group_create();
	
    app_net_check();

    do {
        char *cfg = config_file_read(vos_cfg_path, NULL);
        assert(cfg != NULL);
        XLOG_D("user cfg: %s", cfg);
        cJSON *js = cJSON_Parse(cfg);
        gh_free(cfg);
        assert(js != NULL);

        cJSON *asr_processor_js = cJSON_GetObjectItem(js, "asr_processor");
        assert(asr_processor_js != NULL);
        cJSON *th_processor_js = cJSON_GetObjectItem(js,"TH_processor");
        cJSON *dds_js = cJSON_GetObjectItem(asr_processor_js,"dds");
        assert(dds_js != NULL);
        char *dds_cfg = cJSON_Print(dds_js);
        char *asr_processor_cfg = cJSON_Print(asr_processor_js);
        char *th_processor_cfg = cJSON_Print(th_processor_js);
        cJSON *app_js = cJSON_GetObjectItem(js, "app");
        assert(app_js != NULL);
        cJSON *app_wakeup_js = cJSON_GetObjectItem(app_js, "wakeup");
        assert(app_wakeup_js != NULL);
        s_app.words_count = cJSON_GetArraySize(app_wakeup_js);
        s_app.words = gh_calloc(s_app.words_count, sizeof(wakeup_audio_t));

        int i;
        for (i = 0; i < s_app.words_count; i++) {
            cJSON *item_js = cJSON_GetArrayItem(app_wakeup_js, i);
            cJSON *word_js = cJSON_GetObjectItem(item_js, "word");
            cJSON *audio_js = cJSON_GetObjectItem(item_js, "audio");
            s_app.words[i].word = gh_strdup(word_js->valuestring);
            assert(s_app.words[i].word != NULL);
            s_app.words[i].audio = gh_strdup(audio_js->valuestring);
            assert(s_app.words[i].audio != NULL);
        }
        cJSON *app_vad_js = cJSON_GetObjectItem(app_js, "vad");
        assert(app_vad_js != NULL);
        cJSON *beginTimeoutAudio_js = cJSON_GetObjectItem(app_vad_js, "beginTimeoutAudio");
        s_app.begin_timeout_prompt = gh_strdup(beginTimeoutAudio_js->valuestring);
        cJSON *endTimeoutAudio_js = cJSON_GetObjectItem(app_vad_js, "endTimeoutAudio");
        s_app.end_timeout_prompt = gh_strdup(endTimeoutAudio_js->valuestring);
        cJSON *network_js = cJSON_GetObjectItem(app_js, "network");
        assert(network_js != NULL);
        cJSON *errorAudio_js = cJSON_GetObjectItem(network_js, "errorAudio");
        s_app.error_prompt = gh_strdup(errorAudio_js->valuestring);

        cJSON_Delete(js);

        player_init();
        {
            player_manager_cfg_t cfg = {
                .prompt_cfg = {
                    .tag = "prompt",
                    .preprocess_rb_size = 60 * 1024,
                    .decode_rb_size = 50 * 1024,
                    .device = "default",
                    .play_device = DEFAULT_ALSA_DEVICE,
                },
                .media_cfg = {
                    .tag = "media",
                    .preprocess_rb_size = 60 * 1024,
                    .decode_rb_size = 50 * 1024,
                    .device = "default",
                    .play_device = DEFAULT_ALSA_DEVICE,
                },
                .cb = player_manager_callback,
            };
            player_manager_init(&cfg);
        }

// 等待太行USB节点准备就绪
        app_usb_check();
        fct_init(export_fct_result); // 产测线程
#ifdef USE_TH_PROCESS
        {
            th_processor_cfg_t cfg = {
                .cfg = th_processor_cfg,
                .auth_cfg = dds_cfg,
                .event_cb = th_event_callback,
                .bf_audio_cb = th_beamforming_audio_callback,
                .fct_audio_cb = th_fct_audio_callback,
                .login_cb = th_login_callback,
                .asr_cb = app_do_parse_asr,
            };
            th_processor_init(&cfg);
        }
#endif

//等待TH登录完成
        gh_event_bits_t bits = BIT_TH_LOGIN;
        gh_event_group_wait_bits(s_app.appdone,&bits, true, true, -1);

        {
            asr_processor_cfg_t cfg = {
                .cfg = asr_processor_cfg,
                .event_cb = asr_event_callback,
                #ifdef DDS_ENABLE
                .dds_cb = dds_callback,
                .test_cb = th_event_callback,
                .test_audio_cb = th_beamforming_audio_callback
                #endif
            };
            asr_processor_init(&cfg);
            free(asr_processor_cfg);
            asr_processor_update_network_status(1);
        }

        {
            gh_thread_cfg_t tc = {
                .tag = "taskstack",
                .run = taskstack_run,
            };
            s_app.t_taskstack = gh_thread_create_ex(&tc);
        }
#ifdef USE_TEST_MODE
       {
            gh_thread_cfg_t tc = {
                .tag = "testrun",
                .run = test_run,
            };
            s_app.t_test = gh_thread_create_ex(&tc);
       }
#endif

		/*	等待太行登录完成，假如未授权会卡在这里；
			设备出场未联网之前是无法获得授权，并登录成功的；
		*/
        bits = BIT_DDS_READY;
        gh_event_group_wait_bits(s_app.appdone,&bits, true, true, -1);

        // 如果需要切全双工  半双工，在此之后进行。更推荐方式是配置aimakefile内的编译宏

        s_app.q = gh_queue_create(sizeof(app_msg_t), 5);
        taskstack_init(&s_app.fsm_stack, 10);
        signal(SIGINT, sigint_handler);
        app_msg_t in;
        int state = WAIT_FOR_WAKEUP;

#if 1
		while(DDS_FULL_DUPLEX != get_dui_type())
		{
			sleep(2);
			set_dui_type(DDS_FULL_DUPLEX);
			printf("NOW DUI_TYPE IS:%d\r\n", get_dui_type());
		}
		
#endif
        while (1) {
            XLOG_I("wait msg");
            gh_queue_read(s_app.q, &in, -1);
            XLOG_I("get msg, type: %s, state: %s", msg_table[in.type], state_table[state]);
            app_run_cb cb = app_run_table[state][in.type];
            if (cb) {
                state = cb(&in);
            } else {
                XLOG_W("refused");
            }
            if (state == WAIT_FOR_WAKEUP || state == WAIT_FOR_ASR) {
                //get pending job
                taskstack_task_t job;
                if (0 == taskstack_pop(&s_app.fsm_stack, &job)) {
                    state = job.do_job(&job);
                    if (job.cleanup) {
                        job.cleanup(&job);
                    }
                } else {
                    //TODO
                }
            }
            gh_free(in.result);
            XLOG_I("\napp fsm change -> %s\r\n", state_table[state]);
        }
        gh_thread_destroy(s_app.t_taskstack);
        asr_processor_deinit();
        TH_processor_deinit();
        taskstack_destroy(&s_app.fsm_stack);
        fct_deinit();
        if(s_app.begin_timeout_prompt) free(s_app.begin_timeout_prompt);
        if(s_app.end_timeout_prompt) free(s_app.end_timeout_prompt);
        if(s_app.error_prompt) free(s_app.error_prompt);
        if(asr_processor_cfg) free(asr_processor_cfg);
        if(th_processor_cfg) free(th_processor_cfg);
        gh_event_group_destroy(s_app.appdone);
    } while (0);
    xlog_deinit();
}
