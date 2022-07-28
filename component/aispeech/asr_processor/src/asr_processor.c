#include "asr_processor.h"
#include "gh_thread.h"
#include "gh_queue.h"
#include "gh_ringbuf.h"
#include "gh_event_group.h"
#include "gh_time.h"
#include "gh_timer.h"
#include "xlog.h"
#include "gh_mutex.h"
#include "duilite.h"
#include "cJSON.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/stat.h>
#include "monitor_process.h"
#include "utils.h"

#define LOG_D(TAG, format, ...) XLOG_DEBUG(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_I(TAG, format, ...) XLOG_INFO(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_W(TAG, format, ...) XLOG_WARN(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_E(TAG, format, ...) XLOG_ERROR(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)

#define BUF_READ_ONCE_SIZE 1024 //20ms 16kHz/1ch/16bit
#define WAIT_DELAY 100
typedef struct {
    gh_ringbuf_handle_t rb_asr;
    gh_ringbuf_handle_t rb_dds;
    gh_ringbuf_handle_t rb_local_vad;
    gh_queue_handle_t q_asr;
    gh_queue_handle_t q_dds_helper;
    gh_queue_handle_t q_process;
    gh_thread_handle_t t_asr;
    gh_thread_handle_t t_dds;
    gh_thread_handle_t t_dds_helper;
    gh_thread_handle_t t_process;
    gh_thread_handle_t net_check;
    gh_event_group_handle_t done;
    gh_thread_handle_t t_local_vad;  
    gh_queue_handle_t q_vad;
    char *vad_cfg;
    char *vad_param;
    int vad_begin_timeout;
    int vad_end_timeout;
    char *asr_cfg;
    char *asr_param;
    bool doing_asr;
    bool doing_dds;
    // dds feed音频退出标志位
    int running;
    int dds_reset;
    int status;
    int dds_timeout;
    struct dds_msg *dds;
    char dev_mac[32];
    asr_event_cb event_cb;
    test_cb test_cb;
    test_audio_cb test_audio_cb;
    dds_ev_callback dds_cb;
    int dds_login_flag;
    gh_timer_handle_t dds_timer;
    gh_timer_handle_t dds_full_timer;
    int dds_full_timeout;
    void *userdata;
    int speak_timeout;
    gh_timer_handle_t speak_timer;
    int net_status_check_lock;  // 启动识别后，不再切换标志位，等待对话流程结束再释放
    int net_check_running;   // 决定是否启用本地网络检查标志位
} asr_processor_t;

typedef struct{
    char devInfo[128];
    char *productId;
    char *aliasKey;
    char *productKey;
    char *productSecret;
}dds_auth_t;
static dds_auth_t dds_auth;

static int dds_reload();

#define BIT_ASR_DONE (1 << 0)
#define BIT_DDS_DONE (1 << 1)
#define BIT_VAD_DONE (1 << 2)
#define BIT_DDS_OR_ASR_DONE (1 << 3)
#define BIT_DDS_CHANGE_MODE (1 << 4)
#define BIT_DDS_READY_DONE (1 << 5)

typedef struct {
    asr_event_t type;
    int status;
    char *result;
} asr_msg_t;
extern asr_engine_e asr_type;
static asr_processor_t s_ap;

dds_type_e dds_type = DDS_HALF_DUPLEX;

static const char *msg_table[] = {
    "ASR_EV_LOCAL",
    "ASR_EV_CLOUD",
    "ASR_EV_DM",
    "ASR_EV_ERROR",
    "ASR_EV_TIMEOUT",
    "ASR_VAD_BEGIN_TIMEOUT",
    "CMD_VAD_ONESHOT",
    "CMD_VAD_RUN",
    "CMD_ASR_RUN",
    "CMD_ASR_STOP",
    "CMD_ASR_RESET",
    "CMD_NETWORK_UPDATE",
    "INFO_ASR_ERROR",
    "INFO_ASR_LOCAL",
    "INFO_ASR_CLOUD",
    "INFO_ASR_DM",
    "INFO_SPEECH_BEGIN_TIMEOUT",
    "INFO_SPEECH_END_TIMEOUT",
    "INFO_SPEECH_END",
    "INFO_SPEECH_BEGIN",
    "CMD_DDS_FULL",
    "INFO_ASR_FULL_DM",
    "CMD_DDS_FULL_STOP",
};

static const char *asr_engin_table[] = {
    "asr_engin_unknown",
    "ASR_ENGINE_LOCAL",
    "ASR_ENGINE_DUI",
    "ASR_ENGINE_BOTH",
};

int asr_set_dui_or_local(asr_engine_e asr_engin){
    if(!s_ap.net_status_check_lock) return -1;
    s_ap.net_check_running = 0;
    gh_event_bits_t bits;
    gh_err_t err = GH_ERR_OK;
    bits = BIT_DDS_OR_ASR_DONE;
    gh_event_group_clear_bits(s_ap.done, &bits);
    err = gh_event_group_wait_bits(s_ap.done, &bits, true, true, -1);
    LOG_I(asr,"set_dds_or_asr [%s] [%d]",asr_engin_table[asr_engin],s_ap.net_status_check_lock);
    asr_type = asr_engin;
    return 0;
}

asr_engine_e asr_get_current_type(){
    return asr_type;
}

dds_type_e asr_get_duplex_type(){
    return dds_type;
}

void asr_restore_net_check(){
    s_ap.net_check_running = 1;
}

static int asr_callback(void *userdata, int type, char *data, int len) {
    LOG_I(asr, "ASR: %.*s", len, data);
    asr_msg_t out = {
        .type = INFO_ASR_LOCAL,
        .result = gh_strndup(data, len),
    };
    gh_queue_send(s_ap.q_process, &out, -1);
    return 0;
}

int asr_processor_stop();
int asr_processor_start();

static void dds_timer_cb(void *userdata) {
    LOG_I(asr, "dds time out\r\n");
    s_ap.dds_timeout = 0;
    s_ap.net_status_check_lock = 1;
    asr_processor_stop();
}

static char *dds_type_table[]={
    "DDS_HALF_DUPLEX",
    "DDS_FULL_DUPLEX",
};

int set_dds_type(dds_type_e duplex_type){
    if(!s_ap.net_status_check_lock) return -1;
    dds_type = duplex_type;
    LOG_I(asr,"duplex_type = [%s]",dds_type_table[duplex_type]);
    dds_reload();
}

static void speak_timer_cb(void *userdata){
    LOG_I(asr, "speak timer out \r\n");
    asr_processor_stop();
}

static int ringbuf_done(int finish){
    gh_event_bits_t bits;
    gh_err_t err = GH_ERR_OK;
    if(!finish){
        s_ap.running = 1;
        gh_ringbuf_abort(s_ap.rb_local_vad);
        bits = BIT_VAD_DONE;
        err = gh_event_group_wait_bits(s_ap.done, &bits, true, true, WAIT_DELAY);
        LOG_I(asr,"abort vad rb ,err=[%d]",err);        
    }
    if (asr_type == ASR_ENGINE_DUI)
    {
        err = gh_ringbuf_can_read(s_ap.rb_dds,1,100);
        if(err == GH_ERR_OK || err == GH_ERR_TIMEOUT){
            if(finish)
                gh_ringbuf_finish(s_ap.rb_dds);
            else
                gh_ringbuf_abort(s_ap.rb_dds);
            bits = BIT_DDS_DONE;
            err = gh_event_group_wait_bits(s_ap.done, &bits, true, true, WAIT_DELAY);
            LOG_I(asr, "[%s] dds rb -> err=[%d]",finish?"finish":"abort", err);
        }
    }
    else if(asr_type == ASR_ENGINE_LOCAL)
    {
        err = gh_ringbuf_can_read(s_ap.rb_asr,1,100);
        if(err == GH_ERR_OK || err == GH_ERR_TIMEOUT){
            if(finish)
                gh_ringbuf_finish(s_ap.rb_asr);
            else
                gh_ringbuf_abort(s_ap.rb_asr);
            bits = BIT_ASR_DONE;
            err = gh_event_group_wait_bits(s_ap.done, &bits, true, true, WAIT_DELAY);
            LOG_I(asr, "[%s] asr rb -> err=[%d]",finish?"finish":"abort", err);
		}
    }else if(asr_type == ASR_ENGINE_BOTH){}
    else{}    
    return 0;
}

static void asr_run(void *args) {
    LOG_D(asr, "READY");
    struct duilite_asr *asr_engine = duilite_asr_new(s_ap.asr_cfg, asr_callback, NULL); 
    if (!asr_engine) {
        LOG_E(asr, "new asr engine error, asr_cfg = [%s]\r\n",s_ap.asr_cfg);
        return;
    }
    char *buf = gh_malloc(BUF_READ_ONCE_SIZE);
    asr_msg_t in;
    gh_err_t err;
    while (1) {
        LOG_D(asr, "wait msg");
        err = gh_queue_read(s_ap.q_asr, &in, -1);
        if (err == GH_ERR_ABORT) break;
        LOG_I(asr, "get msg, type: %s", msg_table[in.type]);
        if (in.type == CMD_ASR_RUN) {
            duilite_asr_start(asr_engine, s_ap.asr_param);
            int bytes;
            while (1) {
                bytes = BUF_READ_ONCE_SIZE;
                err = gh_ringbuf_blocked_read(s_ap.rb_asr, buf, &bytes);
                if (err == GH_ERR_ABORT) {
                    LOG_W(asr, "before duilite_asr_cancel");
                    duilite_asr_cancel(asr_engine);
                    LOG_W(asr, "abort");
                    break;
                } else if (err == GH_ERR_FINISHED) {
                    if (bytes) duilite_asr_feed(asr_engine, buf, bytes);
                    LOG_W(asr, "before duilite_asr_stop");
                    duilite_asr_stop(asr_engine);
                    LOG_W(asr, "done");
                    break;
                }
                duilite_asr_feed(asr_engine, buf, bytes);
            }
            gh_event_bits_t bits = BIT_ASR_DONE;
            gh_event_group_set_bits(s_ap.done, &bits);
        }
    }
    duilite_asr_delete(asr_engine);
    gh_free(buf);
    LOG_D(asr, "EXIT");
}

#ifdef DDS_ENABLE

int asr_dds_custom_tts(const char *voiceId, const char *text, int volume, double speed){
    if (!voiceId || !text) return -1;
    printf(".......asr_dds_custom_tts......\r\n");
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_CUSTOM_TTS_TEXT);
    dds_msg_set_string(msg, "text", text);
    dds_msg_set_string(msg, "voiceId", voiceId);
    dds_msg_set_double(msg, "speed", speed);
    dds_msg_set_integer(msg, "volume", volume);
    dds_send(msg);
    dds_msg_delete(msg);
    return 0;
}

static void asr_dds_start() {
    s_ap.running = 1;
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
    dds_msg_set_string(msg, "action", "start");
#ifdef USE_CLOUD_VAD
//  dds_msg_set_boolean(msg, "keepConnection", 1);  
//  dds_msg_set_string(msg, "asrParams", "{\"realBack\":true,\"enableCloudVAD\":true, \"enableCloudVADRecEnd\":true}");
#endif
    dds_send(msg);
    dds_msg_delete(msg);
}

static void asr_dds_feed(const char *buf, int size) {

    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_AUDIO_STREAM);
    dds_msg_set_bin(msg, "audio", buf, size);
    dds_send(msg);
    dds_msg_delete(msg);
}

static void asr_dds_stop() {
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
    dds_msg_set_string(msg, "action", "end");
    dds_send(msg);
    dds_msg_delete(msg);
    s_ap.running = 0;
}

static void asr_dds_reset() {
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_RESET);
    dds_msg_set_boolean(msg, "keepConnection", 1);   
    dds_send(msg);
    dds_msg_delete(msg);
}

static void dds_helper_run(void *args) {
    LOG_D(ddsh, "READY");
    asr_msg_t in;
    char *buf = gh_malloc(BUF_READ_ONCE_SIZE);
    gh_err_t err;
    while (1) {
        LOG_I(ddsh, "wait msg");
        err = gh_queue_read(s_ap.q_dds_helper, &in, -1);
        if (err == GH_ERR_ABORT) break;
        LOG_I(ddsh, "get dds msg, type: %s", msg_table[in.type]);
        if (in.type == CMD_ASR_RUN) {
            asr_dds_start();
            LOG_I(asr, "start dds s_ap.running = [%d]",s_ap.running);
            int bytes;
            gh_err_t err;
            while ( s_ap.running ) {
                s_ap.dds_reset = 0;
                bytes = BUF_READ_ONCE_SIZE;
                err = gh_ringbuf_blocked_read(s_ap.rb_dds, buf, &bytes);
                if (err == GH_ERR_ABORT) {
                    asr_dds_reset();
                    break;
                } else if (err == GH_ERR_FINISHED) {
                    if (bytes) asr_dds_feed(buf, bytes);
                    asr_dds_stop();
                    break;
                }                
                asr_dds_feed(buf, bytes);
            }
            gh_event_bits_t bits = BIT_DDS_DONE;
            gh_event_group_set_bits(s_ap.done, &bits);
        }else if(in.type == CMD_DDS_FULL){
            asr_dds_start();
            LOG_I(asr, "start dds full");
            int bytes;
            gh_err_t err;
            while ( s_ap.running ) {
                bytes = BUF_READ_ONCE_SIZE;
                err = gh_ringbuf_blocked_read(s_ap.rb_local_vad, buf, &bytes);
                if (err == GH_ERR_ABORT) {
                    printf("1111-GH_ERR_ABORT\r\n");
                    asr_dds_stop(); 
                    break;
                } else if (err == GH_ERR_FINISHED) {
                    if (bytes) asr_dds_feed(buf, bytes);
                    printf("1111-GH_ERR_FINISHED\r\n");
                    asr_dds_stop();
                    break;
                }                
                asr_dds_feed(buf, bytes);
            }
            gh_event_bits_t bits = BIT_VAD_DONE;
            gh_event_group_set_bits(s_ap.done, &bits);           
        }
    }
    gh_free(buf);
    LOG_D(ddsh, "EXIT");
}

static int dds_handler(void *userdata, struct dds_msg *msg) {
    int type;
    int errorId;
    char *str_value = NULL;
    char *error = NULL;
    char *value = NULL;
    dds_msg_get_integer(msg, "type", &type);
    switch (type)
    {
    case DDS_EV_OUT_DUI_LOGIN:
    {
        LOG_I(asr, "dds login event\r\n");
        if (!dds_msg_get_string(msg, "deviceName", &str_value))
        {
            LOG_I(dds, "DDS_EV_OUT_DUI_LOGIN deviceName = [%s]", str_value);
        }
        if (!dds_msg_get_string(msg, "error", &error))
        {
            LOG_I(dds, "DDS_EV_OUT_DUI_LOGIN error = [%s]", error);
            if (strstr(error, "auth"))
            {
                unlink(VOS_AUTH_FILE);
                s_ap.dds_login_flag = 0;
                exit(0);
            }
        }
        break;
    }
    case DDS_EV_OUT_WEB_CONNECT:
    {
        dds_msg_get_string(msg, "result", &str_value);
        LOG_I(asr, "dds----[%s]", str_value);
        break;
    }
    case DDS_EV_OUT_STATUS:
    {
        dds_msg_get_string(msg, "status", &str_value);
        char *trigger;
        dds_msg_get_string(msg, "status", &trigger);
        LOG_I(dds, "DDS_EV_OUT_STATUS status: %s trigger:%s", str_value, trigger);
        break;
    }
    case DDS_EV_OUT_ERROR:
    {
        char *source = NULL;
        dds_msg_get_string(msg, "source", &source);
        dds_msg_get_integer(msg, "errorId", &errorId);
        if(!dds_msg_get_string(msg, "error", &error)){
            LOG_W(dds, "DDS_EV_OUT_ERROR error: %s, errorId: %d", error, errorId);
            if(strstr(error,"auth") || strstr(error,"register failed") || strstr(error,"mac is different")){
                unlink(VOS_AUTH_FILE);
                unlink("/data/speech_auth/Profile.txt");
                exit(0);
            }
            if (source && (0 == strcmp(source, "tts"))) {
            }else if(errorId == 1003 || errorId==1004 || errorId==1005 || errorId==1006 || errorId==1002)
            {
                asr_msg_t out = {
                    .type = INFO_ASR_ERROR,
                };
                gh_queue_send(s_ap.q_process, &out, -1);
            }
        }
        break;
    }
    case DDS_EV_OUT_ASR_RESULT:
    {
        LOG_I(asr, "get dds asr result\r\n");
        if (!dds_msg_get_string(msg, "var", &str_value))
        {
            LOG_I(dds, "DDS_EV_OUT_ASR_RESULT var: %s", str_value);
        }
        if (!dds_msg_get_string(msg, "text", &str_value))
        {
            LOG_I(dds, "DDS_EV_OUT_ASR_RESULT text: %s", str_value);
        }
        if (!dds_msg_get_string(msg, "pinyin", &str_value))
        {
            LOG_I(dds, "DDS_EV_OUT_ASR_RESULT pinyin: %s", str_value);
        }
        break;
    }
    case DDS_EV_OUT_DUI_RESPONSE:
    {
        LOG_I(asr, "get dds DDS_EV_OUT_DUI_RESPONSE\r\n");
        dds_msg_get_string(msg, "response", &str_value);
//      LOG_I(asr, "dui response -- [%s]\r\n",str_value);
        if(dds_type == DDS_FULL_DUPLEX){
        asr_msg_t out = {
            .type = INFO_ASR_FULL_DM,
            .result = gh_strdup(str_value)};
            gh_queue_send(s_ap.q_process, &out, -1);
        }else{
        asr_msg_t out = {
            .type = INFO_ASR_DM,
            .result = gh_strdup(str_value)};
            gh_queue_send(s_ap.q_process, &out, -1);
        }
        LOG_I(asr, "asr end gh_queue_send\r\n");
        break;
    }
    case DDS_EV_OUT_VAD_RESULT:
    {
        LOG_I(asr, "vad result");
        int state;
        if (!dds_msg_get_integer(msg, "vadState", &state))
        {
            LOG_I(asr, "vadState : %d", state);
            if (2 == state)
            {
                asr_processor_stop();
            }
        }
        break;
    }
    default:
        break;
    }
    if (s_ap.dds_cb){
        s_ap.dds_cb(s_ap.userdata, msg);
    }
    return 0;
}

#endif

enum {
    IDLE = 0,
    SPEECHING,
    WAIT_FOR_ASR,
    MAX_STATE
};

static const char *state_table[] = {
    "IDLE",
    "SPEECHING",
    "WAIT_FOR_ASR"
};

typedef int (*process_run_cb)(asr_msg_t *in);

static int cmd_asr_run_when_idle(asr_msg_t *in) {
#ifdef USE_LOCAL_VAD
    gh_ringbuf_start(s_ap.rb_local_vad);
#endif
    return SPEECHING;
//  return WAIT_FOR_ASR;
}

static int cmd_dds_full_when_wait_for_idle(asr_msg_t *in){
    LOG_I(asr,"dds full");
    s_ap.net_status_check_lock = 0;
    gh_ringbuf_start(s_ap.rb_local_vad);
    gh_queue_send(s_ap.q_dds_helper, in, -1); // 给dds发送 CMD_DDS_FULL
    return IDLE;
}

static int cmd_dds_full_stop_when_wait_for_idle(asr_msg_t *in){
    LOG_I(asr, "cmd_dds_full_stop_when_wait_for_idle");
    gh_event_bits_t bits;
    gh_err_t err = GH_ERR_OK;
    gh_ringbuf_abort(s_ap.rb_local_vad);
    bits = BIT_VAD_DONE;
    err = gh_event_group_wait_bits(s_ap.done, &bits, true, true, WAIT_DELAY);
    LOG_I(asr,"abort vad rb ,err=[%d]",err);        
    s_ap.net_status_check_lock = 1;
    return IDLE;
}

static int info_asr_full_dm_when_idle(asr_msg_t *in){
    s_ap.event_cb(s_ap.userdata, ASR_EV_DM, in->result, strlen(in->result));
    return IDLE;
}

static int info_asr_dm_when_idle(asr_msg_t *in) {
    LOG_I(asr, "dds_timeout=[%d]",s_ap.dds_timeout);
    if(s_ap.dds_timeout)
        s_ap.event_cb(s_ap.userdata, ASR_EV_DM, in->result, strlen(in->result));
    else
        s_ap.event_cb(s_ap.userdata, ASR_EV_TIMEOUT, in->result, strlen(in->result));
    return IDLE;
}

static int cmd_asr_stop_when_speeching(asr_msg_t *in) {

    LOG_I(asr, "cmd_asr_stop_when_speeching 结束识别，等待识别结果dds_or_asr_flag=[%d]",asr_type);
    ringbuf_done(1);
    s_ap.net_status_check_lock = 1;
    return WAIT_FOR_ASR;
}

static int cmd_asr_stop_when_wait_for_asr(asr_msg_t *in){
    LOG_I(asr, "cmd_asr_stop_when_wait_for_asr over, dds_or_asr_flag=[%d] ",asr_type);
    ringbuf_done(1);
    s_ap.net_status_check_lock = 1;
    return WAIT_FOR_ASR;
}

// 给vad发送cmd_vad_run
static int cmd_vad_run_when_wait_for_idle(asr_msg_t *in){
    LOG_I(asr, "open VAD");
    s_ap.running = 1;
    s_ap.net_status_check_lock = 0;
    gh_ringbuf_start(s_ap.rb_local_vad);
    gh_queue_send(s_ap.q_vad, in, -1);
    if(asr_type == ASR_ENGINE_DUI)
        gh_ringbuf_start(s_ap.rb_dds);
    else if(asr_type == ASR_ENGINE_LOCAL)
        gh_ringbuf_start(s_ap.rb_asr);
    else if(asr_type == ASR_ENGINE_BOTH){}
    else{}
    return SPEECHING;
}

static int cmd_vad_run_when_wait_for_speeching(asr_msg_t *in){
    LOG_I(asr, "cmd_vad_run_when_wait_for_speeching");
    s_ap.net_status_check_lock = 0;
    gh_ringbuf_start(s_ap.rb_local_vad);
    if(asr_type == ASR_ENGINE_DUI){
        gh_ringbuf_start(s_ap.rb_dds);
    }else if(asr_type == ASR_ENGINE_LOCAL){
        gh_ringbuf_start(s_ap.rb_asr);
    }else if(asr_type == ASR_ENGINE_BOTH){}
    else{}
    gh_queue_send(s_ap.q_vad, in, -1);
    return SPEECHING;
}

static int cmd_vad_run_when_wait_for_asr(asr_msg_t *in){
    LOG_I(asr, "cmd_vad_run_when_wait_for_asr  -> wait_for_wakeup");
    s_ap.event_cb(s_ap.userdata, CMD_IGNORE, NULL, 0);
    return IDLE;
}

static int cmd_asr_reset_when_speeching(asr_msg_t *in) {
   LOG_I(asr, "In the process wakeup event,reset the process\r\n");
    ringbuf_done(0);
    asr_msg_t out = {
        .type = CMD_VAD_RUN,
    };
    gh_queue_send(s_ap.q_process, &out, -1);
    LOG_I(asr,"send CMD_VAD_RUN to process");
    return SPEECHING;
}

static int cmd_asr_run_when_speeching(asr_msg_t *in) {
    if(asr_type == ASR_ENGINE_DUI)   // 走dds 发送 CMD_ASR_RUN
    {
    //  asr_dds_start();  // 建立连接需要时间，所以改为唤醒之后，就去建立连接
        gh_queue_send(s_ap.q_dds_helper, in, -1);
        LOG_I(asr,"start dds");
    }else if(asr_type == ASR_ENGINE_LOCAL){
        gh_queue_send(s_ap.q_asr, in, -1);
        LOG_I(asr,"start asr");
    }else if(asr_type == ASR_ENGINE_BOTH){}
    else{}
    return WAIT_FOR_ASR;
}

static int info_asr_error_when_speeching(asr_msg_t *in) {
    if (s_ap.status) {
        s_ap.event_cb(s_ap.userdata, ASR_EV_ERROR, NULL, 0);
    }
    return IDLE;
}

static int info_speech_begin_when_speeching(asr_msg_t *in){
    //发送 CMD_ASR_RUN 开启ASR识别
    LOG_I(asr,"process_run get INFO_SPEECH_BEGIN，sendCMD_ASR_RUN\r\n");
    asr_msg_t out = {
        .type = CMD_ASR_RUN,
    };
    gh_queue_send(s_ap.q_process, &out, -1);
    return SPEECHING;
}

static int info_asr_local_when_wait_for_asr(asr_msg_t *in) {
    LOG_I(asr,"send result to process_run");
    s_ap.event_cb(s_ap.userdata, ASR_EV_LOCAL, in->result, strlen(in->result));
    s_ap.net_status_check_lock = 1;
    return IDLE;
}

static int info_asr_cloud_when_wait_for_asr(asr_msg_t *in) {
    s_ap.event_cb(s_ap.userdata, ASR_EV_CLOUD, in->result, strlen(in->result));
    return WAIT_FOR_ASR;
}

static int info_asr_dm_when_wait_for_asr(asr_msg_t *in) {
    LOG_I(asr,"*****func=[%s] line=[%d] s_ap.dds_time_out = [%d]",__func__,__LINE__,s_ap.dds_timeout);
    if(s_ap.dds_timeout)
        s_ap.event_cb(s_ap.userdata, ASR_EV_DM, in->result, strlen(in->result));
    else
        s_ap.event_cb(s_ap.userdata, ASR_EV_TIMEOUT, in->result, strlen(in->result));
    s_ap.net_status_check_lock = 1;
    return IDLE;
}

static int info_asr_dm_when_speeching(asr_msg_t *in) {
    LOG_I(asr,"*****func=[%s] line=[%d] s_ap.dds_time_out = [%d]",__func__,__LINE__,s_ap.dds_timeout);
    if(s_ap.dds_timeout)
        s_ap.event_cb(s_ap.userdata, ASR_EV_DM, in->result, strlen(in->result));
    else
        s_ap.event_cb(s_ap.userdata, ASR_EV_TIMEOUT, in->result, strlen(in->result));
    s_ap.net_status_check_lock = 1;
    return IDLE;
}

static int cmd_asr_reset_when_wait_for_asr(asr_msg_t *in) {
    LOG_I(asr,"cmd_asr_reset_when_wait_for_asr");
    s_ap.running = 1;
    ringbuf_done(0);
    asr_msg_t out = {
        .type = CMD_VAD_RUN,
    };
    gh_queue_send(s_ap.q_process, &out, -1);
    return SPEECHING;
}

static int cmd_asr_run_when_wait_for_asr(asr_msg_t *in) {
    LOG_I(asr,"cmd_asr_run_when_wait_for_asr\r\n");
    return WAIT_FOR_ASR;
}

static int info_asr_error_when_wait_for_asr(asr_msg_t *in) {
    s_ap.event_cb(s_ap.userdata, ASR_EV_ERROR, NULL, 0);
    return IDLE;
}
// VAD超时，此处后面看是否有识别结果下来
static int info_speech_end_timeout_when_wait_for_asr(asr_msg_t *in){
    return IDLE;
}

// 长时间未检测到人声，这里直接给主线程抛失败
static int info_speech_begin_timeout_when_wait_for_speeching(asr_msg_t *in){
    s_ap.event_cb(s_ap.userdata, ASR_VAD_BEGIN_TIMEOUT, NULL, 0);
    return IDLE;
}

// vad检测到人声终止，结束dds  关闭ringbuf
static int info_speech_end_when_wait_for_asr(asr_msg_t *in){
    LOG_I(asr,"info_speech_end_when_wait_for_asr");
    ringbuf_done(1);
    s_ap.net_status_check_lock = 1;
    return WAIT_FOR_ASR;
}

static int info_speech_end_when_speeching(asr_msg_t *in){
    LOG_I(asr,"info_speech_end_when_speeching");
    ringbuf_done(1);
    s_ap.net_status_check_lock = 1;
    return IDLE;
}

#ifdef USE_LOCAL_VAD

typedef struct {
    double begin_time;
    int status; //0-1-2
    bool first;
} vad_info_t;

static double get_timestamp() {
    double now;
    struct timeval t;
    gettimeofday(&t, NULL);
    now = (double)t.tv_sec + ((double)(t.tv_usec / 1000)) / 1000.0;
    return now;
}

static void throw_vad_status(int status) {
    asr_msg_t out = {
        .type = status,
    };
    gh_queue_send(s_ap.q_process, &out, -1);
}

static bool check_time_expire(double now, double before, int timeout) {
    if (1000 * (now - before) >= timeout) return true;
    return false;
}

static int vad_callback(void *userdata, int type, char *result, int len) {
    vad_info_t *info = (vad_info_t *)userdata;
    if (type == DUILITE_MSG_TYPE_JSON) {
        LOG_I(vad, "%s", result);
        cJSON *js = cJSON_Parse(result);
        cJSON *status_js = cJSON_GetObjectItem(js, "status");
        info->status = status_js->valueint;
        if(2 == info->status)
        {
            LOG_I(asr,"####vad end,stop dds\r\n");
            asr_processor_stop();
        }
        cJSON_Delete(js);
    }else{
        if(1 == info->status){
            if(info->first){
                info->first = false;
                info->begin_time = get_timestamp();
                LOG_I(asr,"VAD detection sound,start dds or asr\r\n");
                throw_vad_status(INFO_SPEECH_BEGIN);                
            }
            int count = len / BUF_READ_ONCE_SIZE;
            int remain = len % BUF_READ_ONCE_SIZE;
            const char *p = result;
            for (int i = 0; i < count; i++) {
                asr_audio_feed(p, BUF_READ_ONCE_SIZE);
                p += BUF_READ_ONCE_SIZE;
            }
            if (remain) {
                asr_audio_feed(p, remain);
            }
        }
    }
    return 0;
}

static void local_vad_run(void *args) {
    LOG_I(vad, "READY,cfg=[%s]",s_ap.vad_cfg);
    vad_info_t info;
    struct duilite_vad *vad_engine = duilite_vad_new(s_ap.vad_cfg, vad_callback, &info);
    if (!vad_engine) {
        LOG_W(vad, "new engine error");
        return;
    }
    char *buf = gh_malloc(BUF_READ_ONCE_SIZE);
    int bytes;
    gh_err_t err;
    asr_msg_t in;
    gh_err_t err_rb;
    double first_end_time, now_time, start_time;
    while (1) {
        LOG_I(vad, "wait msg");
        err = gh_queue_read(s_ap.q_vad, &in, -1);
        if (err == GH_ERR_ABORT) break;
        LOG_I(vad, "get msg, type: %s", msg_table[in.type]);
        if (in.type == CMD_VAD_RUN) {
            info.first = true;
            info.status = 0;
            int ret = duilite_vad_start(vad_engine, NULL);
            if(ret)
                LOG_E(vad, "### vad start failed\r\n");
            start_time = get_timestamp();
            while (1) {
                bytes = BUF_READ_ONCE_SIZE;
                err_rb = gh_ringbuf_blocked_read(s_ap.rb_local_vad, buf, &bytes);
                if(err_rb == GH_ERR_ABORT) {
                    duilite_vad_cancel(vad_engine);
                    LOG_W(vad, "abort");
                    break;
                }
                duilite_vad_feed(vad_engine, buf, bytes);
                if (0 == info.status) {
                    now_time = get_timestamp();
                    if (check_time_expire(now_time, start_time, s_ap.vad_begin_timeout)) {
                        gh_ringbuf_abort(s_ap.rb_local_vad);
                        throw_vad_status(INFO_SPEECH_BEGIN_TIMEOUT); // 未检测到人声
                        duilite_vad_cancel(vad_engine);
                        LOG_W(vad, "begin timeout");
                        break;
                    }
                } else if (1 == info.status) { 
                    now_time = get_timestamp();
                    if (check_time_expire(now_time, info.begin_time, s_ap.vad_end_timeout)) {
                        gh_ringbuf_abort(s_ap.rb_local_vad);
                        throw_vad_status(INFO_SPEECH_END_TIMEOUT);  // 说话超时
                        duilite_vad_cancel(vad_engine);
                        LOG_W(vad, "end timeout");
                        break;
                    }
                } else if (2 == info.status) {
                    LOG_I(asr,"####info.status = 2\r\n");
                    gh_ringbuf_abort(s_ap.rb_local_vad);
                //    throw_vad_status(INFO_SPEECH_END);
                    duilite_vad_stop(vad_engine);
                    break;
                } 
            }
            gh_event_bits_t bits = BIT_VAD_DONE;
            gh_event_group_set_bits(s_ap.done, &bits);
        }
    }
    duilite_vad_delete(vad_engine);
    gh_free(buf);
    LOG_I(vad, "EXIT");
}
#endif

static void net_check_run(void *args)
{
	int times = 1;
    while(1){
        gh_delayms(20);
        if (s_ap.net_check_running){
            if (s_ap.net_status_check_lock){
                int net_ret = 0;
                net_ret = net_status_check();
                if (net_ret){
					if (ASR_ENGINE_DUI != asr_type){
						XLOG_I("USE ***********ASR_ENGINE_DUI***********\r\n");
					}
                    asr_type = ASR_ENGINE_DUI;
                }else{
					if (ASR_ENGINE_LOCAL != asr_type){
						XLOG_I("USE ***********ASR_ENGINE_LOCAL***********\r\n");
					}
                    asr_type = ASR_ENGINE_LOCAL;
                }
            }
        }else{
            gh_event_bits_t bits = BIT_DDS_OR_ASR_DONE;
            gh_event_group_set_bits(s_ap.done, &bits);
        }
    }
}

static const process_run_cb process_run_table[MAX_STATE][ASR_MAX_MSG] = {
    [IDLE] = {
        [CMD_VAD_RUN] = cmd_vad_run_when_wait_for_idle,
        [CMD_ASR_RESET] = cmd_vad_run_when_wait_for_idle,  /*IDLE状态的reset  走完整的语音流程*/
        [CMD_DDS_FULL] = cmd_dds_full_when_wait_for_idle,
        [INFO_ASR_DM] = info_asr_dm_when_idle,
        [INFO_ASR_FULL_DM] = info_asr_full_dm_when_idle,
        [CMD_ASR_STOP] = cmd_dds_full_stop_when_wait_for_idle,
    },
    [SPEECHING] = {
        [CMD_VAD_RUN] = cmd_vad_run_when_wait_for_speeching,
        [CMD_ASR_STOP] = cmd_asr_stop_when_speeching,
        [CMD_ASR_RESET] = cmd_asr_reset_when_speeching,   /* SPEECHING中的reset */
        [CMD_ASR_RUN] = cmd_asr_run_when_speeching,
        [INFO_ASR_ERROR] = info_asr_error_when_speeching,
        [INFO_SPEECH_BEGIN] = info_speech_begin_when_speeching,
        [INFO_SPEECH_BEGIN_TIMEOUT] = info_speech_begin_timeout_when_wait_for_speeching,
        [INFO_SPEECH_END] = info_speech_end_when_speeching,
        [INFO_ASR_DM] = info_asr_dm_when_speeching, 
    },
    [WAIT_FOR_ASR] = {
        [CMD_ASR_STOP] = cmd_asr_stop_when_wait_for_asr,
        [CMD_VAD_RUN] = cmd_vad_run_when_wait_for_asr,
        [INFO_ASR_LOCAL] = info_asr_local_when_wait_for_asr,  /*收到asr识别结果*/
        [INFO_ASR_CLOUD] = info_asr_cloud_when_wait_for_asr,
        [INFO_ASR_DM] = info_asr_dm_when_wait_for_asr,   /*dds识别结果*/
        [CMD_ASR_RESET] = cmd_asr_reset_when_wait_for_asr,
        [CMD_ASR_RUN] = cmd_asr_run_when_wait_for_asr, 
        [INFO_ASR_ERROR] = info_asr_error_when_wait_for_asr,
        [INFO_SPEECH_END_TIMEOUT] = info_speech_end_timeout_when_wait_for_asr,
        [INFO_SPEECH_END] = info_speech_end_when_wait_for_asr,
    }
    
};

static void dds_run(void *args) {
    LOG_I(dds, "dds_run start");
    struct dds_opt opt = {
        ._handler = dds_handler,
    };
    gh_event_bits_t bits = BIT_DDS_READY_DONE;
    gh_event_group_set_bits(s_ap.done, &bits);  
    while(1){
        gh_event_bits_t bits_a = BIT_DDS_READY_DONE;
        gh_event_group_wait_bits(s_ap.done, &bits_a, true, true, -1);
        dds_start(s_ap.dds, &opt);
        gh_event_bits_t bits_b = BIT_DDS_CHANGE_MODE;
        gh_event_group_set_bits(s_ap.done, &bits_b);        
    }
    dds_msg_delete(s_ap.dds);
    LOG_I(dds, "dds login, exit");
}

static int dds_reload(){
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_EXIT);
    dds_send(msg);
    dds_msg_delete(msg);
    msg = NULL;

    gh_event_bits_t bits_a = BIT_DDS_CHANGE_MODE;
    gh_event_group_wait_bits(s_ap.done, &bits_a, true, true, -1);
    if(s_ap.dds){
        dds_msg_delete(s_ap.dds);
        s_ap.dds = NULL;
    }
    s_ap.dds = dds_msg_new();
    dds_msg_set_string(s_ap.dds, "productId", dds_auth.productId);
    dds_msg_set_string(s_ap.dds, "aliasKey", dds_auth.aliasKey);
    dds_msg_set_string(s_ap.dds, "devInfo", dds_auth.devInfo);
    dds_msg_set_string(s_ap.dds, "productKey", dds_auth.productKey);
    dds_msg_set_string(s_ap.dds, "productSecret", dds_auth.productSecret);
    dds_msg_set_string(s_ap.dds, "savedProfile", VOS_AUTH_FILE);
    LOG_I(asr, "dds reload mode = [%s]\r\n",dds_type_table[dds_type]);
    if(dds_type == DDS_FULL_DUPLEX)
        dds_msg_set_boolean(s_ap.dds, "fullDuplex", 1);
    gh_event_bits_t bits_b = BIT_DDS_READY_DONE;
    gh_event_group_set_bits(s_ap.done, &bits_b);   
}

static char duilite_auth[1024] = {0};
static int do_dds_auth(cJSON *dds_js)
{
    int get_mac_ret = -1;
    memset(&dds_auth,0,sizeof(dds_auth_t));
#ifdef DDS_ENABLE
    cJSON *dds_productId_js = cJSON_GetObjectItem(dds_js, "productId");
    cJSON *dds_aliasKey_js = cJSON_GetObjectItem(dds_js, "aliasKey");
    cJSON *dds_productKey_js = cJSON_GetObjectItem(dds_js, "productKey");
    cJSON *dds_productSecret_js = cJSON_GetObjectItem(dds_js, "productSecret");
    if (!dds_productId_js || !dds_aliasKey_js || !dds_productKey_js || dds_productSecret_js)
    {
        LOG_I(asr, "The authorization information is incomplete");
    }

    while (1)
    {
        get_mac_ret = get_mac_addr("wlan0", s_ap.dev_mac);
        if (0 == get_mac_ret)
            break;
    }

    s_ap.dds_login_flag = 0;
    if (s_ap.dds)
        dds_msg_delete(s_ap.dds);
    s_ap.dds = dds_msg_new();

    dds_msg_set_string(s_ap.dds, "productId", dds_productId_js->valuestring);
    dds_msg_set_string(s_ap.dds, "aliasKey", dds_aliasKey_js->valuestring);

    sprintf(dds_auth.devInfo, "{\"deviceName\":\"%s\",\"platform\":\"linux\"}", s_ap.dev_mac);
    LOG_I(asr,"-------devInfo=[%s] ", dds_auth.devInfo);
    dds_msg_set_string(s_ap.dds, "devInfo", dds_auth.devInfo);
    if (dds_productKey_js)
        dds_msg_set_string(s_ap.dds, "productKey", dds_productKey_js->valuestring);
    if (dds_productSecret_js)
        dds_msg_set_string(s_ap.dds, "productSecret", dds_productSecret_js->valuestring);
    dds_msg_set_string(s_ap.dds, "savedProfile", VOS_AUTH_FILE);
#ifdef USE_DDS_FULL_DUPLEX
    dds_msg_set_boolean(s_ap.dds, "fullDuplex", 1);
    dds_type = DDS_FULL_DUPLEX;
#endif
    // dds_msg_set_boolean(s_ap.dds, "logEnable", 1);//开启日志
    // dds_msg_set_string(s_ap.dds, "logFile", "./dds.log");//设置日志文件路径
 
    dds_auth.productId = gh_strdup(dds_productId_js->valuestring);
    dds_auth.aliasKey = gh_strdup(dds_aliasKey_js->valuestring);
    dds_auth.productKey = gh_strdup(dds_productKey_js->valuestring);
    dds_auth.productSecret = gh_strdup(dds_productSecret_js->valuestring);

// 授权DUILITE
    cJSON *auth_js = cJSON_CreateObject();
    cJSON_AddStringToObject(auth_js,"productId", dds_productId_js->valuestring);
    cJSON_AddStringToObject(auth_js,"productKey", dds_productKey_js->valuestring);
    cJSON_AddStringToObject(auth_js,"productSecret", dds_productSecret_js->valuestring);
    cJSON_AddStringToObject(auth_js,"savedProfile", VOS_AUTH_FILE);
    cJSON *devInfo_js = cJSON_Parse(dds_auth.devInfo);
    if(devInfo_js)
        cJSON_AddItemToObject(auth_js,"devInfo",devInfo_js);
    char *auth = cJSON_Print(auth_js);
     strncpy(duilite_auth, auth, sizeof(duilite_auth));
    free(auth);
    auth = NULL;
    cJSON_Delete(auth_js);

#endif
    return 0;
}

int asr_processor_update_network_status(int status);
static void process_run(void *args) {
    LOG_D(asrp, "READY");
    asr_msg_t in;
    int state = IDLE;
    asr_processor_update_network_status(1);

    while (1) {
        LOG_W(asrp, "wait msg");
        gh_queue_read(s_ap.q_process, &in, -1);
        LOG_W(asrp, "asr  get msg, type: %s, state: %s", msg_table[in.type], state_table[state]);
        if (in.type == CMD_NETWORK_UPDATE) {
            s_ap.status = in.status; 
            asr_type = ASR_ENGINE_DUI;
            continue;
        }
        process_run_cb cb = process_run_table[state][in.type];
        if (cb) {
            state = cb(&in);
        } else {
            LOG_W(asrp, "refused");
        }
        LOG_W(asrp, "asr——process fsm change %s", state_table[state]);
        gh_free(in.result);
    }
    LOG_D(asrp, "EXIT");
}
 
int asr_processor_init(asr_processor_cfg_t *cfg) {
    LOG_D(asrp, "cfg: %s", cfg->cfg);
    LOG_I(asr,"func=[%s]----line=[%d]",__func__,__LINE__);
    s_ap.done = gh_event_group_create();
    s_ap.net_check_running = 1;
    cJSON *js = cJSON_Parse(cfg->cfg);
    if (!js) return -1;
    do {
        cJSON *asr_js = cJSON_GetObjectItem(js, "asr");
        if(asr_js){
            cJSON *asr_cfg_js = cJSON_GetObjectItem(asr_js, "cfg");
            s_ap.asr_cfg = cJSON_Print(asr_cfg_js);
        }
        cJSON *asr_param_js = cJSON_GetObjectItem(asr_js, "param");
        if(asr_param_js)
            s_ap.asr_param = cJSON_Print(asr_param_js);
        cJSON *speak_timeout_js = cJSON_GetObjectItem(js,"speak_timeout");
        s_ap.speak_timeout = speak_timeout_js ? speak_timeout_js->valuestring : 0;

#ifdef DDS_ENABLE     
        cJSON *dds_js = cJSON_GetObjectItem(js, "dds");
        if(dds_js){
            do_dds_auth(dds_js);
            printf("The authorization information is ready\r\n");
        }else{
            printf("no auth message,the process can not run!!!!!!!!\r\n");
            return -1;
        }
#endif
#ifdef USE_LOCAL_VAD
       cJSON *vad_js = cJSON_GetObjectItem(js, "vad");
       if(vad_js){
            cJSON *vad_cfg_js = cJSON_GetObjectItem(vad_js, "cfg");
            if(vad_cfg_js){
                s_ap.vad_cfg = cJSON_Print(vad_cfg_js);
            }
        cJSON *begin_timeout_js = cJSON_GetObjectItem(vad_js, "beginTimeout");
        cJSON *end_timeout_js = cJSON_GetObjectItem(vad_js, "endTimeout");
        s_ap.vad_begin_timeout = begin_timeout_js ? begin_timeout_js->valueint : 5000;
        s_ap.vad_end_timeout = end_timeout_js ? end_timeout_js->valueint : 10000;
       }
#endif

#define ASR_RINGBUF_SIZE 16000 //500ms
#ifdef DDS_ENABLE
        s_ap.rb_dds = gh_ringbuf_create(ASR_RINGBUF_SIZE);
#endif
        s_ap.rb_asr = gh_ringbuf_create(ASR_RINGBUF_SIZE);

#ifdef USE_LOCAL_VAD 
        s_ap.rb_local_vad = gh_ringbuf_create(ASR_RINGBUF_SIZE); 
        s_ap.q_vad = gh_queue_create(sizeof(asr_msg_t), 4);
#endif

        s_ap.q_asr = gh_queue_create(sizeof(asr_msg_t), 2);
#ifdef DDS_ENABLE
        s_ap.q_dds_helper = gh_queue_create(sizeof(asr_msg_t), 5);
#endif
        s_ap.q_process = gh_queue_create(sizeof(asr_msg_t), 5);

        s_ap.event_cb = cfg->event_cb;
        s_ap.test_cb = cfg->test_cb;
        s_ap.test_audio_cb = cfg->test_audio_cb;
#ifdef DDS_ENABLE
        s_ap.dds_cb = cfg->dds_cb;
#endif
        s_ap.userdata = cfg->userdata;
        s_ap.net_status_check_lock = 1;

        int ret = duilite_library_load(duilite_auth);
        while(1){
            if(ret){
                LOG_E(do_auth,"duilite auth failed, exit\r\n");
                unlink("/data/speech_auth/Profile.txt");
                exit(0);
                break;
            }else{
                LOG_I(do_auth, "duilite auth success");
                break;
            }
            gh_delayms(10);
        }

#ifdef DDS_ENABLE

        {
            gh_thread_cfg_t tc = {
                .tag = "dds",
                .run = dds_run,
            };
            s_ap.t_dds = gh_thread_create_ex(&tc); 
        }

        {
            gh_thread_cfg_t tc = {
                .tag = "ddsh",
                .run = dds_helper_run,
            };
            s_ap.t_dds_helper = gh_thread_create_ex(&tc); 
        }
#endif

        {
            gh_thread_cfg_t tc = {
                .tag = "asr",
                .run = asr_run,
            };
            s_ap.t_asr = gh_thread_create_ex(&tc); 
        }


        {
            gh_thread_cfg_t tc = {
                .tag = "process",
                .run = process_run,
            };
            s_ap.t_process = gh_thread_create_ex(&tc); 
        }

#ifdef USE_LOCAL_VAD
        {
            gh_thread_cfg_t tc = {
                .tag = "vad",
                .run = local_vad_run,
            };
            s_ap.t_local_vad = gh_thread_create_ex(&tc);
        }
#endif   
        {
            gh_thread_cfg_t tc = {
                .tag = "net_check",
                .run = net_check_run,
            };
            s_ap.net_check = gh_thread_create_ex(&tc);
        }
#ifdef USE_TIMEOUT
        {
            gh_timer_cfg_t cfg = {
                .cb = dds_timer_cb,
                .timeout = 15000,
            };
            s_ap.dds_timer = gh_timer_create(&cfg);         
        }
#endif
        cJSON_Delete(js);
        return 0;
    }while (0);
    return -1;
}

// 唤醒之后，调用此函数，开启本轮识别
int asr_processor_start() {
    if(asr_type == ASR_ENGINE_DUI && dds_type == DDS_FULL_DUPLEX){
        asr_msg_t out_b = {
            .type = CMD_DDS_FULL,
        };        
        gh_queue_send(s_ap.q_process, &out_b, -1);
        printf("start full duplex asr\r\n");
    }else{
        LOG_I(asr,"end queue send CMD_ASR_RUN to s_ap.q_process");
#ifdef USE_LOCAL_VAD
        LOG_I(asr,"send CMD_VAD_RUN \r\n");
        asr_msg_t out_b = {
            .type = CMD_VAD_RUN,
        };
        gh_queue_send(s_ap.q_process, &out_b, -1);
#endif

#ifdef USE_TIMEOUT
    gh_timer_start(s_ap.dds_timer);
    s_ap.dds_timeout = 1;
#endif
    }
    return 0;
}

int asr_processor_feed(const void *buf, int len) {
    int wlen = len;
#ifdef USE_LOCAL_VAD
    gh_ringbuf_try_write(s_ap.rb_local_vad, buf, &wlen);
    wlen = len;  
#endif
    return 0;
}

int asr_audio_feed(const void *buf, int len){
    int wlen = len;
    if(asr_type == ASR_ENGINE_DUI){
        gh_ringbuf_try_write(s_ap.rb_dds, buf, &wlen);
        return 0;
    }
    wlen = len;
    if(asr_type == ASR_ENGINE_LOCAL){
        gh_ringbuf_try_write(s_ap.rb_asr, buf, &wlen);
        return 0;
    }
}

int asr_processor_stop() {
    asr_msg_t out = {
        .type = CMD_ASR_STOP,
    };
    gh_queue_send(s_ap.q_process, &out, -1);
    LOG_I(asr,"end send queue to s_ap.q_process");
#ifdef USE_TIMEOUT
    gh_timer_stop(s_ap.dds_timer);
#endif
    return 0;
}

int asr_processor_reset() {
    LOG_I(asr,">asr_processor_reset line=[%d]",__LINE__);
    if(dds_type == DDS_FULL_DUPLEX)
        return;
    asr_msg_t out = {
        .type = CMD_ASR_RESET,
    };
    gh_timer_start(s_ap.dds_timer);
    gh_queue_send(s_ap.q_process, &out, -1);
    if(asr_type == ASR_ENGINE_DUI)
        gh_ringbuf_start(s_ap.rb_dds);
    else if(asr_type == ASR_ENGINE_LOCAL)
        gh_ringbuf_start(s_ap.rb_asr);
    else if(asr_type == ASR_ENGINE_BOTH){}
    else{}
    return 0;
}

int asr_processor_update_network_status(int status) {
    asr_msg_t out = {
        .type = CMD_NETWORK_UPDATE,
        .status = status,
    };
    gh_queue_send(s_ap.q_process, &out, -1);
    return 0;
}

void asr_processor_deinit() {
#ifdef USE_LOCAL_ASR
    gh_thread_destroy(s_ap.t_asr);
#endif
#ifdef DDS_ENABLE
    gh_thread_destroy(s_ap.t_dds);
    gh_thread_destroy(s_ap.t_dds_helper);
#endif
    gh_thread_destroy(s_ap.t_process);

    gh_queue_destroy(s_ap.q_asr);
#ifdef DDS_ENABLE    
    gh_queue_destroy(s_ap.q_dds_helper);
#endif
    gh_queue_destroy(s_ap.q_process);

    gh_ringbuf_destroy(s_ap.rb_asr);
#ifdef USE_LOCAL_VAD
    gh_ringbuf_destroy(s_ap.rb_local_vad);
    gh_queue_destroy(s_ap.q_vad);
    if (s_ap.vad_param) {
        free(s_ap.vad_param);
        s_ap.vad_param = NULL;
    }
#endif
#ifdef USE_TIMEOUT
    gh_timer_destroy(s_ap.dds_timer);
#endif
    gh_timer_destroy(s_ap.dds_full_timer);
#ifdef DDS_ENABLE
    gh_ringbuf_destroy(s_ap.rb_dds);
#endif
    gh_event_group_destroy(s_ap.done);
    if (s_ap.asr_cfg) {
        free(s_ap.asr_cfg);
        s_ap.asr_cfg = NULL;
    }
    if (s_ap.asr_param) {
        free(s_ap.asr_param);
        s_ap.asr_param = NULL;
    }
}
