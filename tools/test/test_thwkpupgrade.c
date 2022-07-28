#include "thfesp.h"
#include <stdio.h>
#include "cJSON.h"

int method = -1;
int status = -1;
int wakeup_flag = 0;
int dfu_finish = 0;
int get_thversion = 0;
#define USE_UPGRADE 
#define USE_DEBUG
FILE* fp_bf;

static int origin_callback(void* userdata, int type, char* msg, int len) {
//	printf("[Test] origin: %d\n", len);
	return 0;
}

static int wakeup_callback(void* userdata, int type, char* msg, int len) {
	printf("[Test] wakeup: %s\n", msg);

	wakeup_flag = 1;
	return 0;
}

static int doa_callback(void* userdata, int type, char* msg, int len) {
	printf("[Test] doa: %s\n", msg);
	return 0;
}

static int beforming_callback(void* userdata, int type, char* msg, int len) {
	if (type == THFESP_MSG_TYPE_JSON) {
		//printf("[Test] wakeup_type: %s\n", msg);
	}
	else {
		//  printf("[Test] beforming len: %d\n", len);
//			fwrite(msg, 1, len, fp_bf);
//			fflush(fp_bf);
	}
	return 0;
}

static int message_callback(void* userdata, int type, char* msg, int len) {
	if (type == THFESP_MSG_TYPE_JSON) {
		printf("[Test] message: %s\n", msg);
		cJSON* root_js = cJSON_Parse(msg);
		cJSON* type_js = root_js ? cJSON_GetObjectItem(root_js, "type") : NULL;
		if (type_js && !strcmp(type_js->valuestring, "thfespStatus")) {
			cJSON* status_js = root_js ? cJSON_GetObjectItem(root_js, "status") : NULL;
			if (status_js && !strcmp(status_js->valuestring, "idle")) {
				status = 0; //空闲状态
			}
			else {
				status = 1;
			}
		}

		if (type_js && !strcmp(type_js->valuestring, "thupgradeMethod")) {
			cJSON* method_js = root_js ? cJSON_GetObjectItem(root_js, "method") : NULL;
			if (method_js && !strcmp(method_js->valuestring, "dfu")) {
				method = 1; //dfu模式
			} else {
				method = 0;
			}
		}
		if (type_js && !strcmp(type_js->valuestring, "thVersion")) {
			cJSON* version_js = root_js ? cJSON_GetObjectItem(root_js, "version") : NULL;
			if (version_js) {
				printf("th version: %s\n", version_js->valuestring);
			}
			cJSON* error_js = root_js ? cJSON_GetObjectItem(root_js, "error") : NULL;
			if (error_js) {
				printf("th version error: %s\n", error_js->valuestring);
			}
			get_thversion = 1;
		}
		if (root_js) cJSON_Delete(root_js);
	}
	return 0;
}

static int upgrade_callback(void* userdata, int type, char* msg, int len) {
	if (type == THFESP_MSG_TYPE_JSON) {
		printf("[Test] upgrade: %s\n", msg);
		cJSON* root_js = cJSON_Parse(msg);
		cJSON* method_js = root_js ? cJSON_GetObjectItem(root_js, "method") : NULL;
		cJSON* result_js = root_js ? cJSON_GetObjectItem(root_js, "result") : NULL;
		if (method_js && !strcmp(method_js->valuestring, "dfu")) {
			if (result_js && !strcmp(result_js->valuestring, "success")) {
				printf("dfu success\n");
				dfu_finish = 1;
			}
			else {
				dfu_finish = 2;
				printf("dfu failed\n");
			}

		}
		if (root_js) cJSON_Delete(root_js);
	}
	return 0;
}

void test_run(struct thfesp* thfesp) {
	thfesp_start(thfesp, NULL);
	while(1){
		usleep(1000000000);
	}
	thfesp_stop(thfesp);
/*
#ifdef USE_LOOP
	while (1) {
#endif
		thfesp_start(thfesp, NULL);
		while (1) {
#ifdef USE_LOOP
			if (wakeup_flag) break;
#endif
			usleep(100000);
		}

		wakeup_flag = 0;
#ifdef USE_UPLOAD
		usleep(2000000); //唤醒音频是4.5S，唤醒点前3.5秒，后1秒 
#endif
		thfesp_stop(thfesp);
		//usleep(3000000);
#ifdef USE_LOOP
	}
#endif
*/
}

void test_dfu(struct thfesp* thfesp, char* path) {
#ifdef USE_UPGRADE
	char* get_msg = "{\"type\":\"thfespStatus\"}";
	thfesp_get_message(thfesp, get_msg);

	get_msg = "{\"type\":\"thupgradeMethod\"}";
	thfesp_get_message(thfesp, get_msg);

	while (1) {
		//等待获取应用状态, 升级模式
		if (method != -1 && status != -1) {
			printf("[Test] thfespStatus & thupgradeMethod received\n");
			break;
		}
		usleep(100000);
	}

	if (status != 0 && status != -1) { //处于运行状态
		thfesp_stop(thfesp);
		status = 0;
	}

	char* thBinPath[1024] = { 0 };
	sprintf(thBinPath, "{\"thBinPath\":\"%s\"}", path);
	if (status == 0 && method == 1) { //处于空闲状态与DFU模式
		thfesp_dfu(thfesp, thBinPath);
		while (1) {
			if (dfu_finish == 1 || dfu_finish == 2) break;
		}
	} else {
		printf("[Test] not dfu mode\n");
	}

	printf("[Test] dfu finish\n");
#endif
}

void test_get_thversion(struct thfesp* thfesp) {
	char* get_msg = "{\"type\":\"thfespStatus\"}";
	thfesp_get_message(thfesp, get_msg);

	if (status != 0 && status != -1) { //处于运行状态
		thfesp_stop(thfesp);
		status = 0;
	}

	get_msg = "{\"type\":\"thVersion\"}";
	thfesp_get_message(thfesp, get_msg);
	while (1) {
		if (get_thversion == 1) break;
		usleep(1000000);
	}
}

int main() {
	printf("this is test\n");

//	fp_bf = fopen("out.pcm", "wb+");

#ifdef USE_DEBUG
	char* cfg = "{\
		\"sample_bit\":16,\
		\"sample_rate\":16000,\
		\"origin_channels\":6,\
		\"debug_origin\":1,\
		\"debug_asr\":1,\
		\"debug_asr_out\":1,\
		\"debug_beforming\":1,\
		\"debug_wakeup_info\":1,\
		\"log_enable\":1\
	}";
#else
	char* cfg = "{\
		\"sample_bit\":16,\
		\"sample_rate\":16000\
	}";
#endif

	struct thfesp* thfesp = thfesp_new(cfg);
	int ret;
	ret = thfesp_register(thfesp, THFESP_CALLBACK_ORIGIN_PCM, origin_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_WAKEUP, wakeup_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_DOA, doa_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_BEAMFORMING, beforming_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_MESSAGE, message_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_UPGRADE, upgrade_callback, NULL);

//    test_get_thversion(thfesp);

//	char* fw_path = "/data/test_thsdk/TH14_OW_ZHMI_RB_V1.0.01_2022_0120_1543_70mm.fw";
//	test_dfu(thfesp, fw_path);

	test_run(thfesp);
	thfesp_delete(thfesp);

//	fclose(fp_bf);
	return 0;
}