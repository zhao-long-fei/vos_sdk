
#ifdef __cplusplus
extern "C"
{
#endif

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#include "voice_message_prase.h"
#include "utils.h"
#include "lstLib.h"
#include "mbedtls/base64.h"
#include "cJSON.h"
#include "player_manager.h"
#include "file_preprocessor.h"
#include "protocol_conversion.h"

extern int g_th_cb_time;		/*语音唤醒倒计时*/
extern int g_th_wake_up;		/*语音唤醒标志*/

#define SAFE_FREE(x)	{if (x)	free(x);	(x) = 0;}

#define VOICE_CONFIG_PATH			"/userdata/app/vos_sdk/voice_rules"		/*语音规则文件本地路径*/
#define VOICE_HIT_LOCAL_SOUND	 	"/userdata/app/audio/haodezhuren.wav"

/*语音规则文件存储链表节点*/
typedef struct {
	NODE node;
	voice_rule_s rule;
}rule_node_s;

static int g_voice_rules_inited = 0;
/*语音规则列表操作互斥锁，目前未加锁，只有进程起来时会读取规则文件，其他时候都只是查寻*/
pthread_mutex_t g_voice_rules_lock = PTHREAD_MUTEX_INITIALIZER;
LIST g_voice_rules;

/*播放音频文件接口，调用的speech语音sdk接口，播放可以打断*/
static void play_my_sound(char* audio_path) 
{
#if 1
    //player_manager_stop();
    player_manager_play_t play = {
        .target = {
            .target = audio_path,
            .preprocessor = DEFAULT_FILE_PREPROCESSOR,
        },
        .can_terminated = 1,
        .can_discarded = 1,
        .priority = PLAY_PRIORITY_LOW,
    };
    player_manager_play(&play, true);
#else
	char play_cmd[1024] = {0};
	sprintf(play_cmd, "aplay %s", audio_path);
	system(play_cmd);
#endif
	
}

/*START***********连接server***********/

#ifdef LINK_UI
#define SOCK_PATH_LVGL "/userdata/srceen_app_server" 	/*和LVGL本地soket通信媒介文件*/
int g_iSock4Lvgl = -1;									/*通信socket*/

/*
	连接lvgl服务
*/
int link_server(void)
{
    if (g_iSock4Lvgl != -1)
    {
        return -1;
    }

    struct sockaddr_un address;
    int result;
    int len;

    if ((g_iSock4Lvgl = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror ("socket");
       	return -1;
    }
 
    address.sun_family = AF_UNIX;
    strcpy (address.sun_path, SOCK_PATH_LVGL);
    len = sizeof (address);
 
    /*向服务器发送连接请求*/
    while (1)
    {
        result = connect(g_iSock4Lvgl, (struct sockaddr *)&address, len);
        if (result == 0)
        {
            return 0;
        }
        else 
        {
            //printf ("ensure the server is up\n");
            //perror ("connect gui server:");
            sleep(1);
            continue;
        }
    }
}


/*
	向服务发送数据
*/
int send_server(unsigned char *data, int len)
{
    int last = len;
    int index = 0;
    int ret;

    if (g_iSock4Lvgl == -1)
    {
        return -1;
    }
	
	if (NULL == data || 0 == len) {
		return -1;
	}

    printf("send server, data:%s!!!!\n", data);

    while (last > 0)
    {
        ret = write(g_iSock4Lvgl, data + index, last);
        if (ret <= 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                continue;
            }
            else
            {
                perror("ERRRRRRRR");
                printf("error : %d\n", errno);
                return -2;
            }
        }
        else
        {
            index += ret;
            last -= ret;
        }
    }
	printf("send OK!!!!\n");

    return 0;
}

int send_msg_to_gui(unsigned char *data, int len)
{
	return  send_server(data, len);
}

/*
	关闭连接
*/
int close_server(void)
{
    if (g_iSock4Lvgl == -1)
    {
        return -1;
    }

    close(g_iSock4Lvgl);
    g_iSock4Lvgl = -1;
    return 0;
}

#endif


/*
	lvgl服务数据轮询读取和重连线程
*/
static void *proc_server_thr(void *args) 
{
	int ret = 0, len = 0;
	char buf[1024] = {0};
	prctl(PR_SET_NAME, (unsigned long)"proc_server_thr");

	link_server();
    while(1) {
		/*读数据*/
		memset(buf, 0, sizeof(buf));
		ret = read(g_iSock4Lvgl, buf, sizeof(buf) - 1);		
		if (ret <= 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
            	printf("no msg or interrupted\r\n");
                continue;
            }
            else
            {
                perror("read err");
                printf("error : %d\n", errno);
				close_server();
                link_server();
            }
        }
		len  = ret;
		printf("rcv server msg:%s, len:%d.\r\n", buf, len);
		/*ADD LIST*/
		if (0 >= len) {
			continue;
		}

		if (0 == strcmp(buf, "updateDevice")) {
			refresh_search_config(CONFIG_TYPE_DEVICE);
		} else if (0 == strcmp(buf, "updateScene")) {
			refresh_search_config(CONFIG_TYPE_SCENE);
		} else {
			
		}
		//usleep(10000);
	}
	
    return NULL;
}

/*END***********连接server***********/


/*
	添加语音规则
*/
int add_voice_rule_node(voice_rule_s* rule_in)
{
	int ret = 0, num = 0, i = 0, opt_num = 0;
	rule_node_s *p_node_add = NULL;
	voice_then_s *then_add = NULL;
	voice_on_s *on_add = NULL;
	voice_opt_s *opt_add = NULL;

	if (NULL == rule_in) {
		printf("%s, param err\r\n", __FUNCTION__);
		return -1;
	}

	p_node_add = (rule_node_s*)malloc(sizeof(rule_node_s));
	if (NULL == p_node_add) {
		printf("%s, malloc err\r\n", __FUNCTION__);
		return -1;
	}
	memset(p_node_add, 0, sizeof(rule_node_s));
	memcpy(&p_node_add->rule, rule_in, sizeof(voice_rule_s));
	p_node_add->rule.on_num = 0;
	p_node_add->rule.then_num = 0;
	p_node_add->rule.voice_on = NULL;
	p_node_add->rule.voice_then = NULL;

	on_add = (voice_on_s*)malloc(sizeof(voice_on_s) * (rule_in->on_num));
	if (NULL == on_add) {
		printf("%s, malloc err\r\n", __FUNCTION__);
		ret = -1;
		goto EXIT;
	}
	p_node_add->rule.voice_on = on_add;
	memcpy(p_node_add->rule.voice_on, rule_in->voice_on, sizeof(voice_on_s) * (rule_in->on_num));
	p_node_add->rule.on_num = rule_in->on_num;

	then_add = (voice_then_s*)malloc(sizeof(voice_then_s) * rule_in->then_num);
	if (NULL == then_add) {
		printf("%s, malloc err\r\n", __FUNCTION__);
		ret = -1;
		goto EXIT;
	}
	p_node_add->rule.voice_then = then_add;
	memcpy(p_node_add->rule.voice_then, rule_in->voice_then, sizeof(voice_then_s) * (rule_in->then_num));
	p_node_add->rule.then_num = rule_in->then_num;
	num = rule_in->then_num;
	
	for (i = 0; i < num; i++) {
		p_node_add->rule.voice_then[i].opt_num = 0;
		p_node_add->rule.voice_then[i].opt = NULL;
	}
	
	for (i = 0; i < num; i++) {
		opt_num = rule_in->voice_then[i].opt_num;
		opt_add = (voice_opt_s*)malloc(sizeof(voice_opt_s) * opt_num);
		if (NULL == opt_add) {
			printf("%s, malloc err\r\n", __FUNCTION__);
			ret = -1;
			goto EXIT;
		}
		p_node_add->rule.voice_then[i].opt = opt_add;
		memcpy(p_node_add->rule.voice_then[i].opt, rule_in->voice_then[i].opt, sizeof(voice_opt_s) * opt_num);
		p_node_add->rule.voice_then[i].opt_num = opt_num;
	}

	lstAdd(&g_voice_rules, (NODE*)p_node_add);

EXIT:
	if (ret) {
		SAFE_FREE(p_node_add->rule.voice_on);
		if (p_node_add->rule.voice_then) {
			for (i = 0; i < p_node_add->rule.then_num; i++) {
				SAFE_FREE(p_node_add->rule.voice_then[i].opt);
			}
		}
		SAFE_FREE(p_node_add->rule.voice_then);
	}
	
	return ret;
}


/*
	语音规则配置文件读取到内存
*/
int voice_rules_config_read(char* file_path)
{
	int ret = 0, array_size = 0, i = 0, j = 0, k = 0, on_num = 0, then_num = 0, opt_num = 0;
	cJSON *json_array = NULL, *json_array_item = NULL, *json_tmp = NULL;
	voice_rule_s voice_rule;
	voice_on_s* on_add = NULL;
	voice_then_s* then_add = NULL;
	voice_opt_s* opt_add = NULL;
	
	if ((NULL == file_path)){
        printf("%s, input para err.\n", __FUNCTION__);
        return -1;
    }
	memset(&voice_rule, 0, sizeof(voice_rule));
	
	ret = json_config_open_by_name(file_path, &json_array);
	if (ret) {
		printf("%s, file open err ret:%d \n", __FUNCTION__, ret);
        return -1;
	}

	if (!json_array) {
		printf("%s, config file fomat err.\n", __FUNCTION__);
        return -2;
	}
	array_size = cJSON_GetArraySize(json_array);
    printf("%s, array_size:%d\n", __FUNCTION__, array_size);
    for (i = 0; i < array_size; i++) {
        json_array_item = cJSON_GetArrayItem(json_array, i);
        if (NULL == json_array_item){
            continue;
        }

		if (NULL != (json_tmp = cJSON_GetObjectItem(json_array_item, "version"))){
			strncpy(voice_rule.version, json_tmp->valuestring, VERSION_LEN - 1);
		}

        if (NULL != (json_tmp = cJSON_GetObjectItem(json_array_item, "rule_id"))){
            voice_rule.rule_id = atoi(json_tmp->valuestring);
        }

		/*on*/
		cJSON *on_array = NULL, *on_item = NULL;
        on_array = cJSON_GetObjectItem(json_array_item, "on");
        if (NULL == on_array){
            goto RULE_CONTINUE;
        }
		
        on_num = cJSON_GetArraySize(on_array);
		if (0 == on_num) {
			ret = -1;
			goto EXIT;
		}
		on_add = (voice_on_s*)malloc(sizeof(voice_on_s) * on_num);
		if (NULL == on_add) {
			printf("%s, on_add malloc fail.\n", __FUNCTION__);
        	ret = -1;
			goto EXIT;
		}
		memset(on_add, 0, sizeof(sizeof(voice_on_s) * on_num));
		voice_rule.on_num = on_num;
		voice_rule.voice_on = on_add;
		
        for (j = 0; j < on_num; j++) {
			on_item = cJSON_GetArrayItem(on_array, j);
        	if (NULL == on_item){
            	continue;
        	}
			json_tmp = cJSON_GetObjectItem(on_item, "topic");
        	if (json_tmp){
            	strncpy(on_add[j].topic, json_tmp->valuestring, sizeof(on_add[j].topic) - 1);
        	}
        }
		/*end on*/

		/*then*/
		cJSON *then_array = NULL, *then_item = NULL;
		then_array = cJSON_GetObjectItem(json_array_item, "then");
        if (NULL == then_array){
        	goto RULE_CONTINUE;
        }
		
        then_num = cJSON_GetArraySize(then_array);
		if (0 == then_num) {
			ret = -1;
			goto EXIT;
		}
		then_add = (voice_then_s*)malloc(sizeof(voice_then_s) * then_num);
		if (NULL == then_add) {
			printf("%s, then_add malloc fail.\n", __FUNCTION__);
        	ret = -1;
			goto EXIT;
		}
		memset(then_add, 0, sizeof(sizeof(voice_then_s) * then_num));
		voice_rule.then_num = then_num;
		voice_rule.voice_then = then_add;
		
        for (j = 0; j < then_num; j++) {
			then_item = cJSON_GetArrayItem(then_array, j);
        	if (NULL == then_item){
            	continue;
        	}
			
			json_tmp = cJSON_GetObjectItem(then_item, "type");
        	if (json_tmp) {
				then_add[j].then_type = atoi(json_tmp->valuestring);
        	} else {
				then_add[j].then_type = THEN_UNKNOW;
			}

			json_tmp = cJSON_GetObjectItem(then_item, "level");
        	if (json_tmp){
            	then_add[j].level = atoi(json_tmp->valuestring);
        	} else {
				then_add[j].level = 0;/*0 level为非法*/
			}
			
			json_tmp = cJSON_GetObjectItem(then_item, "location");
        	if (json_tmp){
            	strncpy(then_add[j].location, json_tmp->valuestring, sizeof(then_add[j].location) - 1);
        	}

 			if (THEN_SCENE == then_add[j].then_type) {
				json_tmp = cJSON_GetObjectItem(then_item, "sence_name");
        		if (json_tmp){
            		strncpy(then_add[j].thing_name, json_tmp->valuestring, sizeof(then_add[j].thing_name) - 1);
        		}
			} else if (THEN_ATTR == then_add[j].then_type) {
				json_tmp = cJSON_GetObjectItem(then_item, "dev_name");
        		if (json_tmp){
            		strncpy(then_add[j].thing_name, json_tmp->valuestring, sizeof(then_add[j].thing_name) - 1);
        		}
				json_tmp = cJSON_GetObjectItem(then_item, "dev_type");
        		if (json_tmp) {
            		then_add[j].thing_type = atoi(json_tmp->valuestring);
        		} else {
					then_add[j].thing_type = DEV_TYPE_UNKNOW;
				}
			} else {
				
			}

			/*opt_item*/
			cJSON *opt_array = NULL, *opt_item = NULL;
			opt_array = cJSON_GetObjectItem(then_item, "opt_item");
        	if ((NULL == opt_array) && (THEN_ATTR == then_add[j].then_type)){
            	continue;
        	}
			opt_num = cJSON_GetArraySize(opt_array);

			opt_add = (voice_opt_s*)malloc(sizeof(voice_opt_s) * opt_num);
			if (NULL == opt_add) {
				printf("%s, opt_add malloc fail.\n", __FUNCTION__);
        		ret = -1;
				goto EXIT;
			}
			memset(opt_add, 0, sizeof(sizeof(voice_opt_s) * opt_num));
			then_add[j].opt_num = opt_num;
			then_add[j].opt = opt_add;
			
			for (k = 0; k < opt_num; k++) {
				opt_item = cJSON_GetArrayItem(opt_array, k);
				if (NULL == then_item){
            		continue;
        		}
				json_tmp = cJSON_GetObjectItem(opt_item, "identifier");
				if (json_tmp){
            		strncpy(opt_add[k].opt_identifier, json_tmp->valuestring, sizeof(opt_add[k].opt_identifier) - 1);
        		}
				json_tmp = cJSON_GetObjectItem(opt_item, "value");
				if (json_tmp){
            		strncpy(opt_add[k].opt_value, json_tmp->valuestring, sizeof(opt_add[k].opt_value) - 1);
        		}
			}
			/*end opt*/			
        }
		/*end then*/

		/*ADD LIST*/
		add_voice_rule_node(&voice_rule);
		
		/*RULE CONTINUE*/
RULE_CONTINUE:
		SAFE_FREE(on_add);
		on_num = 0;
		if (then_add) {
			for(j = 0; j < then_num; j++) {
				SAFE_FREE(then_add[j].opt);
			}
			opt_num = 0;
		}
		SAFE_FREE(then_add);
		then_num = 0;
		memset(&voice_rule, 0, sizeof(voice_rule));
    }


EXIT:
	SAFE_FREE(voice_rule.voice_on);
	if (voice_rule.voice_then) {
		for (i = 0; i < voice_rule.then_num; i++) {
			SAFE_FREE(voice_rule.voice_then[i].opt);
		}
	}
	SAFE_FREE(voice_rule.voice_then);
	cJSON_Delete(json_array);
	return ret;
}


/*发送控制命令给server*/
int send_ctrl_protocol(char* data, int len)
{
	int ret = 0, out_len = 0;
	send_server(data, len);
	return ret;
}


/*
	控制设备属性
	【param in】
	location 空间 name 设备名
	dev_type 设备类型
	identifier 属性名
	value	属性值
*/

int attr_opt_ctrl_by_voice(char* location, char* name, char dev_type, char* identifier, char* value)
{
	int ret = 0, i = 0;
	thing_search_info_s search_info;
	thing_ctrl_protocol_s ctrl_protocol;

	if (!location || !name || !identifier || !value) {
		printf("%s, param err\r\n", __FUNCTION__);
		return -1;
	}
	memset(&search_info, 0, sizeof(thing_search_info_s));
	memset(&ctrl_protocol, 0, sizeof(thing_ctrl_protocol_s));
	
	printf("%s, location:%s, name:%s, dev_type:%d, identifier:%s, value:%s.\r\n", __FUNCTION__,
		location, name, dev_type, identifier, value);

	/*search and pack*/
	if (strlen(location)) {
		if (strlen(name)) {
			search_info.search_type = LOC_DEV_NAME;
			search_info.location = location;
			search_info.name = name;
			search_info.dev_type = dev_type;
		} else if (DEV_TYPE_UNKNOW != dev_type) {
			search_info.search_type = LOC_DEV_TYPE;
			search_info.location = location;
			search_info.dev_type = dev_type; 
		} else {
			printf("%s, search info err\r\n", __FUNCTION__);
		}
	} else if (strlen(name)) {
		search_info.search_type = DEV_NAME;
		search_info.name = name;
		search_info.dev_type = dev_type;
	} else if (DEV_TYPE_UNKNOW != dev_type) {
		search_info.search_type = DEV_TYPE;
		search_info.dev_type = dev_type;
	} else {
		printf("%s, search info err\r\n", __FUNCTION__);
	}

	search_info.identifier = identifier;
	search_info.value = value;

	ret = generate_ctrl_protocol_data(&search_info, &ctrl_protocol);
	if (ret) {
		printf("%s, generate_ctrl_protocol_data err, ret:%d\r\n", __FUNCTION__, ret);
	}

	ret = send_ctrl_protocol(ctrl_protocol.data, ctrl_protocol.length);

	SAFE_FREE(ctrl_protocol.data);
	return ret;
}


/*
	场景控制
	【param in】
	location 空间
	name	场景名
*/
int scene_opt_ctrl_by_voice(char* location, char* name)
{
	int ret = 0, i = 0;
	thing_search_info_s search_info;
	thing_ctrl_protocol_s ctrl_protocol;
	
	if (!location || !name) {
		printf("%s, param err\r\n", __FUNCTION__);
		return -1;
	}
	memset(&search_info, 0, sizeof(thing_search_info_s));
	memset(&ctrl_protocol, 0, sizeof(thing_ctrl_protocol_s));
	
	printf("%s, location:%s, name:%s\r\n", __FUNCTION__, location, name);
	/*search and pack*/
	if (strlen(location)) {
		if (strlen(name)) {
			search_info.search_type = LOC_SCENE_NAME;
			search_info.location = location;
			search_info.name = name;
		} else {
			printf("%s, search info err\r\n", __FUNCTION__);
		}
	} else if (strlen(name)) {
		search_info.search_type = SCENE_NAME;
		search_info.name = name;
	} else {
		printf("%s, search info err\r\n", __FUNCTION__);
	}

	ret = generate_ctrl_protocol_data(&search_info, &ctrl_protocol);
	if (ret) {
		printf("%s, generate_ctrl_protocol_data err, ret:%d\r\n", __FUNCTION__, ret);
	}

	ret = send_ctrl_protocol(ctrl_protocol.data, ctrl_protocol.length);

	SAFE_FREE(ctrl_protocol.data);
	
	return ret;
}

/*
	语音控制执行
	【param in】
	then_in 要执行的动作
	then_num 动作数量
*/
int voice_then_ctrl(voice_then_s* then_in, int then_num)
{
	int ret  = 0, i = 0, j = 0;

	if (NULL == then_in || 0 >= then_num) {
		printf("%s, param NULL\r\n", __FUNCTION__);
		return -1;
	}

	/*打印then信息*/
	for (i = 0; i < then_num; i++) {
		printf("thing_type:%d\r\n", then_in[i].then_type);
		printf("thing_location:%s, ", then_in[i].location);
		printf("thing_name:%s, ", then_in[i].thing_name);
		printf("\r\n");
		if (THEN_ATTR == then_in[i].then_type) {
			for (j = 0; j < then_in[i].opt_num; j++) {
				attr_opt_ctrl_by_voice(then_in[i].location, then_in[i].thing_name, 
					then_in[i].thing_type, then_in[i].opt[j].opt_identifier, then_in[i].opt[j].opt_value);
			}
		} else if (THEN_SCENE == then_in[i].then_type) {
			scene_opt_ctrl_by_voice(then_in[i].location, then_in[i].thing_name);
		}
	}

	return ret;
}

/*语音控制去重变量*/
char tmp_voice[512] = {0};	/*上一条语音缓存*/
int voice_ache_flag = 0; 	/*语音缓存读写标志，用作读写一致性*/
time_t last_voice_time = 0;	/*上一次语音控制命令缓存时间*/
/*****************/

int voice_cheak_skip(char* voice_on)
{
	if (NULL == voice_on) {
		printf("%s, param err\r\n", __FUNCTION__);
	}

	if (0 == voice_ache_flag) {
		voice_ache_flag = 1;
		printf("********voice_on:%s, last_voice:%s********\r\n", voice_on, tmp_voice);
		if (0 == strcmp(tmp_voice, voice_on)) {
			printf("the same voice, skip it\r\n");
			voice_ache_flag = 0;
			return -3;/*-3 标识语音重复，跳过执行*/
		} 
		voice_ache_flag = 0;
	}

	return 0;
}

int cache_voice_on(char* voice_on)
{
	if (NULL == voice_on) {
		printf("%s, param err\r\n", __FUNCTION__);
	}

	if (0 == voice_ache_flag) {
		voice_ache_flag = 1;
		memset(tmp_voice, 0, sizeof(tmp_voice));
		memcpy(tmp_voice, voice_on, strlen(voice_on));
		last_voice_time = time(NULL);
		voice_ache_flag = 0;
	}

	return 0;
}


/*
	解析并执行语音控制
	【param in】：voice_on 离线在线语音输出的中文字符串
	【ret】：0；匹配命中; -1:param err; -3:2s内语音重复; -2:未匹配到
*/
int voice_on_prase(char* voice_on)
{
	int ret = -2, i = 0, have_then = 0;
	rule_node_s *p_node = NULL;

	if (NULL == voice_on) {
		printf("%s, param NULL\r\n", __FUNCTION__);
		return -1;
	}

	if (voice_cheak_skip(voice_on)) {
		return -3;
	}

	LIST_FOR_EACH(rule_node_s, p_node, &g_voice_rules) {
		for (i = 0; i < p_node->rule.on_num; i++) {
			if (0 == strcmp(p_node->rule.voice_on[i].topic, voice_on)) {
				/*首次命中语音规则触发项，播放提示语音：“好的”*/
				if (0 == have_then) {
					play_my_sound(VOICE_HIT_LOCAL_SOUND);
					cache_voice_on(voice_on);
					ret = 0;
					have_then = 1;
				}
				voice_then_ctrl(p_node->rule.voice_then, p_node->rule.then_num);
			}
		}
	}
	
	return ret;
}

/*
	语音唤醒检测；
	语音缓存重置检测；
*/
static void *voice_sleep_cheak(void *args) 
{
	int wake_up = 0;
	char*  wake_up_start = 	"[{\"ctrlType\":\"voice_wakeup\",\"value\":\"start\"}]";
	char*  wake_up_stop = 	"[{\"ctrlType\":\"voice_wakeup\",\"value\":\"stop\"}]";
	prctl(PR_SET_NAME, (unsigned long)"voice_sleep_cheak");
	int aa = 0;
    while(1) {
#if 1
		/*轮询检测唤醒时间，唤醒超时之后做业务处理，次数根据业务需要打开条件#if 1*/
		//printf("%s, num is %d.\r\n", __FUNCTION__, g_th_cb_time);
		if (g_th_wake_up) {
			g_th_wake_up = 0;
			wake_up = 1;
			send_server(wake_up_start, strlen(wake_up_start));
		}
		if (wake_up && (--g_th_cb_time) <= 0) {
			/*如果离线环境*/
			if (g_th_cb_time > -5) {
				wake_up = 0;
				//asr_dds_custom_tts("zhilingfp", "好的，再见", 80, 1.0);//离线情况播放
				send_server(wake_up_stop, strlen(wake_up_stop));
			}
			/*异常情况复位*/
			if (g_th_cb_time < -5) {
				wake_up = 0;
				g_th_cb_time = 0;
				send_server(wake_up_stop, strlen(wake_up_stop));
			}
		}
#endif
		/*每2秒重置语音命令缓存，此缓存机制一定程度防止短时间内执行重复语音命令*/
		if (0 == voice_ache_flag) {
			voice_ache_flag = 1;
			if (last_voice_time && (time(NULL) - last_voice_time >= 2)) {
				memset(tmp_voice, 0, sizeof(tmp_voice));
				last_voice_time = 0;
			}
			voice_ache_flag = 0;
		}
		
		sleep(1);
		
	}
    return NULL;
}

/**
	语音规则配置初始化
*/
int voice_rules_prase_init(void)
{
	int ret = 0;
	pthread_t pid = 0;

	if (g_voice_rules_inited) {
		printf("voice rules already inited\r\n");
		return -1;
	} else {
		g_voice_rules_inited = 1;
		printf("voice_rules_prase_init\r\n");
	}

	lstInit(&g_voice_rules);
	ret = voice_rules_config_read(VOICE_CONFIG_PATH);
	refresh_search_config(CONFIG_TYPE_ALL);

	ret = pthread_create(&pid, NULL, voice_sleep_cheak, NULL);
	if (ret) {
		printf("%s, pthread_create err\r\n", __FUNCTION__);
		ret = -1;
	}

	ret = pthread_create(&pid, NULL, proc_server_thr, NULL);
	if (ret) {
		printf("%s, pthread_create err\r\n", __FUNCTION__);
		ret = -1;
	}
	
	return ret;
}


#ifdef __cplusplus
}
#endif


