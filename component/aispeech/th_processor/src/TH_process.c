#include "TH_process.h"
#include "gh_thread.h"
#include "gh_queue.h"
#include "gh_ringbuf.h"
#include "gh_event_group.h"
#include "xlog.h"
#include "cJSON.h"
#include "thfesp.h"
#include <sys/time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include "gh_thread.h"
#include "gh_queue.h"
#include "gh_ringbuf.h"
#include "gh_event_group.h"
#include "gh_time.h"
#include "gh_timer.h"
#include "gh_mutex.h"
#include "utils.h"
#include "app.h"

/*****qiudong*****/
int g_th_cb_time = 0;		/*语音唤醒倒计时*/
int g_th_wake_up = 0;		/*语音唤醒标志*/
int TH_WAKE_UP_TIME	= 12;   /*睡眠倒计时时间。th芯片默认10s退出唤醒，取值需要>=11s，根据后期测试优化*/
/****************/

#define LOG_D(TAG, format, ...) XLOG_IEBUG(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_I(TAG, format, ...) XLOG_INFO(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_W(TAG, format, ...) XLOG_WARN(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_E(TAG, format, ...) XLOG_ERROR(#TAG, __func__, __LINE__, format, ##__VA_ARGS__)

typedef struct
{
	gh_thread_handle_t t_th;
	gh_thread_handle_t t_th_watch;
	char *th_cfg;
	char *th_auth_cfg;
	void *userdata;
	th_event_cb event_cb;
	th_bf_audio_cb bf_audio_cb;
	th_fct_cb fct_audio_cb;
	th_login_cb login_cb;
	th_asr_cb asr_cb;
	int words_count;
	char **words;
	int dfu_enable;
	char *fw_path;
	char version[128];
	int doa;
	gh_event_group_handle_t done;
} th_processor_t;

static th_processor_t s_th;
typedef struct th_flag_struct{
 	int method;
 	int status;
 	int wakeup_flag;
 	int dfu_finish;
 	int get_thversion;
 	int download_finish;
	int watch_dog_flag;
	int watch_dog_run;
	int fct_test_flag;
	int fct_test_mode;
	int need_download;
	int need_reset;
}th_flag;

typedef enum{
	TH_IDLE = 1,
	TH_RUNNING = 2,
	TH_WAIT = 3,
	TH_STATUS_UNKONW,
}th_status_enum;

typedef enum{
	TH_DFU_MODE = 1,
	TH_DOWNLOAD_MODE,
	TH_UPDATE_MODE_UNKONW,
}th_upload_mothed_enum;

static th_flag th_flag_t = {-1,-1,0,0,0,0,0,0,0,0,0,0};
struct thfesp *thfesp = NULL;

#define BUF_READ_ONCE_SIZE 1024

#define BIT_STATUS_DONE (1 << 0)
#define BIT_METHOD_DONE (1 << 1)
#define BIT_VERSION_DONE (1 << 2)
#define BIT_RUNNING (1 << 3)

pthread_mutex_t watch_dog_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timeval watch_timeout = {0, 1000000};
static void get_th_version(struct thfesp *m_thfesp);
#define ONE_CHANLE_AUDIO_LEN 1024
static int origin_callback(void* userdata, int type, char* msg, int len) {
	pthread_mutex_timedlock(&watch_dog_mutex,&watch_timeout);
	if(len > 0) {
		th_flag_t.watch_dog_run = 3;
		th_flag_t.watch_dog_flag = 0;
	}
	pthread_mutex_unlock(&watch_dog_mutex);
	if(th_flag_t.fct_test_flag){
		s_th.fct_audio_cb(msg,len);
	}	
	return 0;
}

static void watch_dog_run(void *args)
{
	while(1){
		while (th_flag_t.watch_dog_run)
		{
			pthread_mutex_timedlock(&watch_dog_mutex, &watch_timeout);
			th_flag_t.watch_dog_flag++;
			pthread_mutex_unlock(&watch_dog_mutex);
			gh_delayms(2000);
			if ((th_flag_t.watch_dog_flag > 15) && th_flag_t.watch_dog_run==3)
			{
				LOG_I(TH, ">>>>>>>>>>>>>>>>>>>> over timeout no audio >>>>>>>>>>>>>>>>>>>>>\r\n");
				exit(0);
			}
			if ((th_flag_t.watch_dog_flag > 30) && th_flag_t.watch_dog_run==2)
			{
				LOG_I(TH, ">>>>>>>>>>>>>>>>>>>> over timeout no dfu or download down >>>>>>>>>>>>>>>>>>>>>\r\n");
				exit(0);
			}
			if ((th_flag_t.watch_dog_flag > 20) && th_flag_t.watch_dog_run==1)
			{
				LOG_I(TH, ">>>>>>>>>>>>>>>>>>>> over timeout no arecord >>>>>>>>>>>>>>>>>>>>>\r\n");
				exit(0);
			}
		}
		gh_delayms(1000);
	}
}

static int asr_callback(void *userdata, int type, char *msg, int len){
	LOG_I(TH,"*****************************asr: %s\n", msg);
	g_th_cb_time = TH_WAKE_UP_TIME;
	asr_result_type_e asr_result_type = ASR_IN_TH_RESULT;
	s_th.asr_cb(msg,asr_result_type);
	return 0;
}

int thsdk_start_fct(){
	th_flag_t.fct_test_flag = 1;
}

int thsdk_close_fct(){
	th_flag_t.fct_test_flag = 0;
}

int get_doa(){
	return s_th.doa;
}

th_status_enum get_th_status(){
	gh_err_t err_status;
    th_flag_t.status = -1;
	char *get_msg = "{\"type\":\"thfespStatus\"}";
	thfesp_get_message(thfesp, get_msg);
	gh_event_bits_t  bits = BIT_STATUS_DONE;
	err_status = gh_event_group_wait_bits(s_th.done, &bits, true, true, 3000);
	if(th_flag_t.status == 0){
		return TH_IDLE;
	}else if(th_flag_t.status == 1){
        return TH_RUNNING;
	}else{
		return TH_STATUS_UNKONW;
	}
}

th_upload_mothed_enum get_th_upload_mothed(){
	th_flag_t.method = -1;
	char *get_msg = "{\"type\":\"thupgradeMethod\"}";
	thfesp_get_message(thfesp, get_msg);
	gh_err_t err_method;
	gh_event_bits_t bits = BIT_METHOD_DONE;
	err_method = gh_event_group_wait_bits(s_th.done, &bits, true, true, 3000);
    if(th_flag_t.method == 0){
        return TH_DFU_MODE;
	}else if(th_flag_t.method == 1){
		return TH_DOWNLOAD_MODE;
	}else{
		return TH_UPDATE_MODE_UNKONW;
	}
}


static int doa_callback(void *userdata, int type, char *msg, int len){
//	printf("*****************************doa: %s\n", msg);
	return 0;
}

static int doa_detail_callback(void *userdata, int type, char *msg, int len){
//	printf("*****************************doa detail: %s\n", msg);
	return 0;
}

static int wakeup_callback(void *userdata, int type, char *msg, int len)
{
	char *wakeup_word = NULL;
	int major_status = 0;
	int wakeup_status = 0;
	int doa = 0;
	g_th_cb_time = TH_WAKE_UP_TIME + 2;
	g_th_wake_up = 1;

	printf("%s:msg:%s\r\n", __FUNCTION__, msg);
	
	if (type == THFESP_MSG_TYPE_JSON)
	{
		LOG_I(TH,"get wake up message-->[%d]=[%s]", len, msg);
		cJSON *wk_js = cJSON_Parse(msg);
		if (wk_js)
		{
			cJSON *wakeup_word_js = cJSON_GetObjectItem(wk_js, "wakeup_word");
			if(wakeup_word_js)
				wakeup_word = gh_strdup(wakeup_word_js->valuestring);
			cJSON *doa_js = cJSON_GetObjectItem(wk_js, "doa");
			doa = doa_js? doa_js->valueint : 361;
		}
        cJSON *eventType_js = cJSON_GetObjectItem(wk_js,"eventType");
		cJSON *eventInfo_js = cJSON_GetObjectItem(wk_js,"eventInfo");
		char event[1024] = {0};
		if(eventInfo_js){
			strncpy(event,cJSON_Print(eventInfo_js),sizeof(event));
			cJSON *major_js = cJSON_GetObjectItem(eventInfo_js,"major");
			if(major_js)
				major_status = major_js->valueint;
		}
		switch(eventType_js->valueint){
			case 1:
			case 2:
				if (major_status)
					s_th.event_cb(s_th.userdata, FRONT_EV_WAKEUP_MAJOR, NULL, sizeof(th_wakeup_info_t));
				else
					s_th.event_cb(s_th.userdata, FRONT_EV_WAKEUP_MINOR, NULL, sizeof(th_wakeup_info_t));
			break;
			case 3:{
			    asr_result_type_e asr_result_type = ASR_IN_TH_QUICK_CMD;
				s_th.asr_cb(event, asr_result_type);
			}
			break;
		}
		s_th.doa = doa;
		if(wk_js) cJSON_Delete(wk_js);
		if(wakeup_word) free(wakeup_word);

	}
	return 0;
}
// 是否上传音频标志位交给app设置吧
static int beforming_callback(void *userdata, int type, char *msg, int len)
{
	if (type == THFESP_MSG_TYPE_JSON)
	{
		LOG_I(TH, "beforming result = [%s]", msg);
		cJSON *js = cJSON_Parse(msg);
		if (js)
		{
			cJSON *wakeup_type_js = cJSON_GetObjectItem(js, "wakeup_type");
			if (wakeup_type_js)
				LOG_I(TH,"beforming_callback-wakeup_type=[%d]", wakeup_type_js->valueint);
		}
		if(js) cJSON_Delete(js);
	}
	else
	{
		s_th.bf_audio_cb(s_th.userdata, msg, len);
	}
	return 0;
}

static int message_callback(void *userdata, int type, char *msg, int len)
{
	if (type == THFESP_MSG_TYPE_JSON)
	{
		cJSON *root_js = cJSON_Parse(msg);
		cJSON *type_js = root_js ? cJSON_GetObjectItem(root_js, "type") : NULL;
		if (type_js && !strcmp(type_js->valuestring, "thfespStatus"))
		{
			cJSON *status_js = root_js ? cJSON_GetObjectItem(root_js, "status") : NULL;
			if (status_js && !strcmp(status_js->valuestring, "idle"))
			{
				th_flag_t.status = 0; //空闲状态
			}
			else
			{
				th_flag_t.status = 1;
			}
			gh_event_bits_t bits = BIT_STATUS_DONE;
            gh_event_group_set_bits(s_th.done, &bits);
			LOG_I(TH,"TH status %s",status_js->valuestring);
		}

		if (type_js && !strcmp(type_js->valuestring, "thupgradeMethod"))
		{
			cJSON *method_js = root_js ? cJSON_GetObjectItem(root_js, "method") : NULL;
			if (method_js && !strcmp(method_js->valuestring, "dfu"))
			{
				th_flag_t.method = 0;
			}
			else if(method_js && !strcmp(method_js->valuestring, "download"))
			{
				th_flag_t.method = 1;
			}
			gh_event_bits_t bits = BIT_METHOD_DONE;
            gh_event_group_set_bits(s_th.done, &bits);
			LOG_I(TH,"TH method %s",method_js->valuestring);
		}

		if (type_js && !strcmp(type_js->valuestring, "fwVersion") && !th_flag_t.get_thversion)
		{
			cJSON *version_js = root_js ? cJSON_GetObjectItem(root_js, "version") : NULL;
			if (version_js && (strlen(version_js->valuestring)>10) ) // 表示获取到的是版本号，过短可认为不是太行版本号
			{
			    LOG_I(TH,"th version: %s", msg);
				if(!strstr(s_th.version, version_js->valuestring) && !th_flag_t.need_reset){
					th_flag_t.need_reset = 1;
				}
				gh_event_bits_t bits = BIT_RUNNING;
            	gh_event_group_set_bits(s_th.done, &bits);
			}
			cJSON *error_js = root_js ? cJSON_GetObjectItem(root_js, "error") : NULL;
			if (error_js)
			{
				LOG_I(TH,"th version error: %s", error_js->valuestring);
			}
			th_flag_t.get_thversion = 1;
		}

		if(type_js && !strcmp(type_js->valuestring,"workMode"))
		{
			printf("msg = [%s]\r\n",msg);
			cJSON *mode_js = cJSON_GetObjectItem(root_js,"mode");
			s_th.event_cb(s_th.userdata,FRONT_EV_TH_STATUS,msg,strlen(msg));
		}

		if (root_js)
			cJSON_Delete(root_js);
	}
	return 0;
}

static int upgrade_callback(void *userdata, int type, char *msg, int len)
{
	if (type == THFESP_MSG_TYPE_JSON)
	{
		LOG_I(TH,"[Test] upgrade: %s", msg);
		cJSON *root_js = cJSON_Parse(msg);
		cJSON *method_js = root_js ? cJSON_GetObjectItem(root_js, "method") : NULL;
		cJSON *result_js = root_js ? cJSON_GetObjectItem(root_js, "result") : NULL;
		if (method_js && !strcmp(method_js->valuestring, "dfu"))
		{
			if (result_js && !strcmp(result_js->valuestring, "success"))
			{
				LOG_I(TH,"dfu success");
				th_flag_t.dfu_finish = 1;
			}
			else
			{
				th_flag_t.dfu_finish = 2;
				LOG_I(TH,"dfu failed");
			}
		}
		if(method_js && !strcmp(method_js->valuestring, "download")){
			if(result_js && !strcmp(result_js->valuestring, "success")){
				LOG_I(TH,"download success\n");
				th_flag_t.download_finish = 1;
			}else{
				LOG_I(TH,"download failed\n");
				th_flag_t.download_finish = 2;
			}
		}
		if (root_js)
			cJSON_Delete(root_js);
	}
	return 0;
}

#define USE_UPGRADE
static void start_dfu(struct thfesp *m_thfesp, char *path)
{
#ifdef USE_UPGRADE
	th_status_enum th_status_e;
	th_upload_mothed_enum th_upload_mothed_e;
    while(1){
		th_status_e = get_th_status();
		if(th_status_e != TH_IDLE){
			thfesp_stop(thfesp);
		}else{
			break;
		}
		gh_delayms(10);
	}

	th_upload_mothed_e = get_th_upload_mothed();
    
	char *thBinPath[1024] = {0};
	sprintf(thBinPath, "{\"thBinPath\":\"%s\"}", path);
	LOG_I(TH,"status=[%d] method=[%d] 待升级 thBinPath=[%s]", th_flag_t.status, th_flag_t.method, thBinPath);
	if (th_upload_mothed_e == TH_DFU_MODE)
	{ 
		thfesp_dfu(m_thfesp, thBinPath);
		while (1)
		{
			if (th_flag_t.dfu_finish == 1)
				break;
			if(th_flag_t.dfu_finish == 2)
				exit(0);
			LOG_I(TH,"dfuing...........");
			gh_delayms(200);
		}
	}else if(th_upload_mothed_e == TH_DOWNLOAD_MODE){
		if(th_flag_t.need_download == 1){
			thfesp_download(m_thfesp, thBinPath);
			while (1)
			{
				if (th_flag_t.download_finish == 1)
					break;
				if (th_flag_t.download_finish == 2)
					exit(0);
				LOG_I(TH, "download...........");
				gh_delayms(200);
			}
		}
	}
	gh_delayms(1500);
	LOG_I(TH,"************************************upgrade finish");
	th_flag_t.download_finish = 0;
	th_flag_t.dfu_finish = 0;
#endif
}

int th_set_info(const char *cmd){
	th_status_enum th_status_e;
    while(1){
		th_status_e = get_th_status();
		if(th_status_e != TH_IDLE){
			thfesp_stop(thfesp);
		}else{
			break;
		}
		gh_delayms(10);
	}

	int ret = thfesp_set_params(thfesp, cmd);

	ret = thfesp_start(thfesp,NULL);
	printf("thfesp_start ret = [%d]\r\n",ret);
	return ret;
}

int th_get_info(const char *cmd){
	th_status_enum th_status_e;
    while(1){
		th_status_e = get_th_status();
		if(th_status_e != TH_IDLE){
			thfesp_stop(thfesp);
		}else{
			break;
		}
		gh_delayms(10);
	}

	int ret = thfesp_get_message(thfesp, cmd);

	ret = thfesp_start(thfesp,NULL);
	printf("thfesp_start ret = [%d]\r\n",ret);
	return ret;

}

static float thresh_last = 0.8;
static float thresh_this = 0.6;
void th_set_wakeup_words(int level){  
	char *wakeupWords = "{\"env\":\"words=xiao yan xiao yan;thresh=%f;major=1;\"}";
	thresh_last = thresh_this;
	switch(level){
		case 1:
		case 2:
			thresh_this = 0.85;
		break;
		case 3:
		case 4:
			thresh_this = 0.6;
		break;
		default:
			thresh_this = 0.85;
		break;
	}
	if(thresh_last == thresh_this) return;
	char wakeupWords_buf[256] = {0};
	th_set_info(wakeupWords_buf);
}

static void th_run(void *args)
{
	LOG_I(TH,"TH_run start");
    char dev_mac[32] = {0};
	int get_mac_ret = -1;
	char cfg_buf[2048] = {0};
	int profile_len = 0;
	cJSON *js = cJSON_Parse(s_th.th_cfg);
	cJSON *th_sdk_cfg_js = cJSON_GetObjectItem(js,"th_sdk_cfg");
	char *cfg = cJSON_Print(th_sdk_cfg_js);
	LOG_I(TH,"th----cfg = [%s]",cfg);

	thfesp = thfesp_new(cfg);
	if(thfesp){
		LOG_I(TH,"TH sdk open success");
		s_th.login_cb(1);
	}else{
		s_th.login_cb(0);
		LOG_I(TH,"TH sdk open failed");
		unlink(VOS_AUTH_FILE);
		exit(0);
	}

	int ret = thfesp_register(thfesp, THFESP_CALLBACK_WAKEUP, wakeup_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_BEAMFORMING, beforming_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_MESSAGE, message_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_UPGRADE, upgrade_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_DOA, doa_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_ORIGIN_PCM, origin_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_ASR, asr_callback, NULL);
loop_reset:
	if (s_th.th_cfg)
	{
		cJSON *js = cJSON_Parse(s_th.th_cfg);
		cJSON *dfu_enable_js = cJSON_GetObjectItem(js, "dfu_enable");
		cJSON *fw_path_js = cJSON_GetObjectItem(js, "fw_path");
		cJSON *fw_version_js = cJSON_GetObjectItem(js, "fw_version");
		if (dfu_enable_js->valueint)
		{ 
			th_flag_t.watch_dog_flag = 0;
			th_flag_t.watch_dog_run = 2;
			start_dfu(thfesp, fw_path_js->valuestring);
			strncpy(s_th.version, fw_version_js->valuestring,sizeof(s_th.version));
		}
		cJSON_Delete(js);
	}

	if( !thfesp_start(thfesp, NULL) ){
        char * set_msg_asr = "{\"type\":\"workMode\", \"mode\": 1}";
        th_set_info(set_msg_asr);
		gh_event_bits_t bits = BIT_RUNNING;
	    gh_event_group_wait_bits(s_th.done, &bits, true, true, -1);
		if(th_flag_t.need_reset == 1){
			LOG_E(TH,"TH need upgreade...........................");
			thfesp_stop(thfesp);
			gh_delayms(50);
			thfesp_reset(thfesp);
			gh_delayms(3000);
			th_flag_t.get_thversion = 0;
			th_flag_t.get_thversion = 0;
			th_flag_t.need_reset = 0;
			goto loop_reset;
		}
		th_flag_t.watch_dog_flag = 0;
		th_flag_t.watch_dog_run = 1;
		while (1)
		{
			gh_delayms(10000);
		}
	}
	thfesp_stop(thfesp);
	thfesp_delete(thfesp);
	return;
}

int th_processor_init(th_processor_cfg_t *cfg)
{
	LOG_I(TH,"th_processor_init");
	do
	{
		s_th.done = gh_event_group_create();
		s_th.th_cfg = cfg->cfg;
		s_th.th_auth_cfg = cfg->auth_cfg;
		s_th.userdata = cfg->userdata;
		s_th.event_cb = cfg->event_cb;
		s_th.bf_audio_cb = cfg->bf_audio_cb;
		s_th.fct_audio_cb = cfg->fct_audio_cb;
		s_th.login_cb = cfg->login_cb;
		s_th.asr_cb = cfg->asr_cb;
		{
			gh_thread_cfg_t tc = {
				.tag = "TH",
				.run = th_run,
			};
			s_th.t_th = gh_thread_create_ex(&tc);
		}
		{
			gh_thread_cfg_t tc = {
				.tag = "th_watch_dog",
				.run = watch_dog_run,
			};
			s_th.t_th_watch = gh_thread_create_ex(&tc);
		}
	} while (0);
	return 0;
}

void TH_processor_deinit()
{
	if(s_th.th_cfg) free(s_th.th_cfg);
	if(s_th.userdata) free(s_th.userdata);
	gh_thread_destroy(s_th.t_th);
	gh_thread_destroy(s_th.t_th_watch);
	gh_event_group_destroy(s_th.done);
}
