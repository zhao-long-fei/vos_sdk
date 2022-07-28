#include "thfesp.h"
#include <stdio.h>
#include "cJSON.h"

int method = -1;
int status = -1;
int wakeup_flag = 0;
int dfu_finish = 0;
int get_thversion = 0;
#define USE_UPGRADE 

static int origin_callback(void* userdata, int type, char* msg, int len) {
	printf("[Test] origin: %d\n", len);
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
		if(method_js && !strcmp(method_js->valuestring, "download")){
			printf("升级完成\r\n");
			if(result_js && !strcmp(result_js->valuestring, "success")){
				printf("升级成功\r\n");
			}else{
				printf("升级失败\r\n");
			}
		}
		if (root_js) cJSON_Delete(root_js);
	}
	return 0;
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

int main(int argc, char *argv[]) {
	printf("this is test\n");

	char* cfg = "{\
		\"sample_bit\":16,\
		\"sample_rate\":16000\
	}";

	struct thfesp* thfesp = thfesp_new(cfg);
	int ret;
	ret = thfesp_register(thfesp, THFESP_CALLBACK_ORIGIN_PCM, origin_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_MESSAGE, message_callback, NULL);
	ret = thfesp_register(thfesp, THFESP_CALLBACK_UPGRADE, upgrade_callback, NULL);

    test_get_thversion(thfesp);
	char fw_path_buf[1024] = {0};
    if(argc > 1)
	{
		sprintf(fw_path_buf,"{\"thBinPath\":\"./%s\"}",argv[1]);
	}else{
		sprintf(fw_path_buf,"{\"thBinPath\":\"./TH14_WAKEUP_ZHMI_ROBT_V1.0.03_202204071359.fw\"}");
	}
	printf("太行固件路径 [%s] \r\n",fw_path_buf);
    int res = thfesp_download(thfesp,fw_path_buf);
	
    if(res == 0)
    {
        printf("开始升级\r\n");
    }else
    {
        printf("升级失败\r\n");
    }
    int sec = 1000 * 1000 * 20;
    usleep(sec);
	thfesp_delete(thfesp);
	return 0;
}