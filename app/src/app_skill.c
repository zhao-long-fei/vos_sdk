#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "syslog.h"
#include "xlog.h"
#include "http_preprocessor.h"
#include "flash_preprocessor.h"
#include "player_manager.h"
#include "cJSON.h"
#include "app_skill.h"
#include "app_vol_ctrl.h"
#include "gh.h"
#include "app.h"
#include "TH_process.h"
#include "voice_message_prase.h"
#include "protocol_conversion.h"

const dui_skill_ctrl_map_t skill_ctrl_map[] = {
	{ "DUI.MediaController.Pause",			DUI_PLAYER_CTRL_PAUSE			 },
	{ "DUI.MediaController.continue",		DUI_PLAYER_CTRL_RESUME			 },
	{ "DUI.MediaController.Play",			DUI_PLAYER_CTRL_RESUME			 },
	{ "DUI.MediaController.Prev",			DUI_PLAYER_CTRL_PREV			 },
	{ "com.aispeech.music.previous",		DUI_PLAYER_CTRL_PREV			 },
	{ "DUI.MediaController.Next",			DUI_PLAYER_CTRL_NEXT			 },
	{ "DUI.MediaController.Switch", 		DUI_PLAYER_CTRL_NEXT			 },
	{ "DUI.MediaController.change", 		DUI_PLAYER_CTRL_NEXT			 },
	{ "com.aispeech.music.next",			DUI_PLAYER_CTRL_NEXT			 },
	{ "DUI.MediaController.Stop",			DUI_PLAYER_CTRL_STOP			 },
	{ "DUI.MediaController.Forward",		DUI_PLAYER_CTRL_FORWARD 		 },
	{ "DUI.MediaController.Backward",		DUI_PLAYER_CTRL_BACKWARD		 },
	{ "DUI.MediaController.mute",			DUI_PLAYER_CTRL_MUTE			 },
	{ "DUI.MediaController.cancelmute", 	DUI_PLAYER_CTRL_CANCELMUTE		 },
	{ "DUI.MediaController.Replay", 		DUI_PLAYER_CTRL_REPLAY			 },
	{ "DUI.MediaController.sequence",		DUI_PLAYER_CTRL_SEQUENCY		 },
	{ "DUI.MediaController.sequenceoff",	DUI_PLAYER_CTRL_LOOP			 },
	{ "DUI.MediaController.random", 		DUI_PLAYER_CTRL_RANDOM			 },
	{ "DUI.MediaController.randomoff",		DUI_PLAYER_CTRL_SEQUENCY		 },
	{ "DUI.MediaController.loop",			DUI_PLAYER_CTRL_LOOP			 },
	{ "DUI.MediaController.cancelloop", 	DUI_PLAYER_CTRL_SEQUENCY		 },
	{ "DUI.MediaController.orderplay",		DUI_PLAYER_CTRL_SEQUENCY		 },
	{ "DUI.MediaController.collect",		DUI_PLAYER_CTRL_FAVORITE		 },
	{ "DUI.MediaController.collect.play",	DUI_PLAYER_CTRL_PLAYFAVORITE	 },
	{ "DUI.MediaController.collect.cancel", DUI_PLAYER_CTRL_CANCELFAVORITE	 },
	{ "DUI.MediaController.SetVolume",		DUI_PLAYER_CTRL_SET_VOICE		 },
	{ "DUI.System.Sounds.OpenMode", 		DUI_PLAYER_CTRL_MUTE			 },
	{ "DUI.System.Sounds.CloseMode",		DUI_PLAYER_CTRL_CANCELMUTE		 },    
	{ "DUI.MediaController.SwitchPlayMode", DUI_PLAYER_CTRL_LOOP			 },
	{ "DUI.MediaController.SetPlayMode",	DUI_PLAYER_CTRL_PLAYMODE		 },
	{ "toy.control.pause",			        DUI_PLAYER_CTRL_PAUSE},
	{ "toy.control.continue",		        DUI_PLAYER_CTRL_RESUME},
    { "sys.clean.call", 					DUI_CLEAN_ROBOT_CLEAN},
    { "sys.charging.call", 					DUI_CLEAN_ROBOT_CHARGING},
    {"sys.action.call",                     DUI_CLEAN_ROBOT_SKILLS},
    {"sys.local_clean.call",                DUI_CLEAN_ROBOT_LOCAL_CLEAN},
    {"sys.mapping.call",                    DUI_CLEAN_ROBOT_MAPPING},
    {"sys.station.call",                    DUI_CLEAN_ROBOT_STATION},
    {"sys.call_clean.call",                 DUI_CLEAN_ROBOT_CALL_CLEAN},
    {"sys.station_task.call",               DUI_CLEAN_ROBOT_STATION_TASK},
    {"sys.pattern_switch.call",             DUI_CLEAN_ROBOT_PATTERN_SWITCH},
    {"sys.pattern_query.call",              DUI_CLEAN_ROBOT_PATTERN_QUERY},
    {"sys.location_query.call",             DUI_CLEAN_ROBOT_LOCATION_QUERY},
    {"sys.task_intervene",                  DUI_CLEAN_ROBOT_TASK_INTERVENE},
	{"DUI.SmartHome.Device.TurnOn",             DUI_SH_DEVICE_TURN_ON},
    {"DUI.SmartHome.Device.TurnOff",            DUI_SH_DEVICE_TURN_OFF},
    {"DUI.SmartHome.OpenMode",                  DUI_SH_OPEN_MODE},
	{"DUI.System.Display.SetBrightness", 		DUI_SYSTEM_SET_BRIGHTNESS},
    {"DUI.MediaController.SetVolume", 			DUI_SYSTEM_SET_VOLUME},
};

//当前媒体播放器状态
static int s_playplaying = MEDIA_IDLE;

int dui_get_current_playing(void) {
    return s_playplaying;
}

void dui_set_current_playing(int stat)
{
	s_playplaying = stat;
}

static dui_skill_ctrl_t dui_skill_ctrl_find(const char* cmd_name, const dui_skill_ctrl_map_t skill_ctrl_map[], int map_size)
{
    if(NULL != cmd_name)
    {
        int i;
        for(i = 0; i < map_size; i++)
        {
            if(0 == strcmp(cmd_name, skill_ctrl_map[i].cmd_name))
                return skill_ctrl_map[i].skill_ctrl;
        }
    }
    return DUI_SKILL_CTRL_UNKNOWN;
}

static void handle_dui_protol(dui_skill_ctrl_t dui_skill_ctrl, cJSON *param_js)
{
	cJSON* location = cJSON_GetObjectItem(param_js, "location");
	cJSON* dev_name = cJSON_GetObjectItem(param_js, "deviceType");
	cJSON* object = cJSON_GetObjectItem(param_js, "object");
	cJSON* mode = cJSON_GetObjectItem(param_js, "mode");
	cJSON* count = cJSON_GetObjectItem(param_js, "count");
	
	char *loc_str = NULL, *dn_str = NULL, *mode_str = NULL, *attr_str = NULL;
	char dev_type = DEV_TYPE_UNKNOW;

	if (location) {
		loc_str = location->valuestring;
	} else {
		loc_str = "";
	}

	if (dev_name) {
		dn_str = dev_name->valuestring;
	} else {
		dn_str = "";
	}

	if (mode) {
		mode_str = mode->valuestring;
	} else {
		mode_str = "";
	}

	if (object) {
		if (strstr(object->valuestring, "童锁")) {
			attr_str = "ChildLockSwitch";
		} else {
			attr_str = "PowerSwitch";
		}
		
	} else {
		attr_str = "PowerSwitch";
	}

	if (strstr(dn_str, "灯") ||
		strstr(dn_str, "开关")) {
		dev_type = DEV_TYPE_SWITCH;
	} else if (strstr(dn_str, "插座")) {
		dev_type = DEV_TYPE_OUTLET;
	} else {
		printf("dui skill, not support dev_type:%s.\r\n", dn_str);
	}

	/*所有类型操作*/
	if (count && (0 == strcmp(count->valuestring, "all"))) {
		dn_str = "";/*品类控制需要名字为空字符串入参*/
	}
	
	switch(dui_skill_ctrl)
    {
    case DUI_SH_DEVICE_TURN_ON:
		attr_opt_ctrl_by_voice(loc_str, dn_str, dev_type, attr_str, "1");
		break;
	case DUI_SH_DEVICE_TURN_OFF:
		attr_opt_ctrl_by_voice(loc_str, dn_str, dev_type, attr_str, "0");
		break;
	case DUI_SH_OPEN_MODE:
        scene_opt_ctrl_by_voice(loc_str, mode_str);
    	break;
    default:
    	break;
    }

}

static void dui_dreame_masg_handle(dui_skill_ctrl_t dui_skill_ctrl, cJSON *param_js);
static int dui_skills_ctrl_inner(dui_skill_ctrl_t dui_skill_ctrl,cJSON *param_js,cJSON *input_js,bool is_native)
{
    cJSON* skill_js = NULL;
    cJSON* mode_js  = NULL;
    char* tts_out = NULL;
    int doa = 0;
    player_manager_playlist_item_t query = {0};
    player_manager_play_t play = {0};
    query.flags = PLAYLIST_ITEM_BIT_URL | PLAYLIST_ITEM_BIT_TITLE | PLAYLIST_ITEM_BIT_SUBTITLE | PLAYLIST_ITEM_BIT_LABEL;
    int ret = -1;
	int volume = 40;
	int brightness = 40;
	char cmd_buf[128] = {0};
    printf("dui_skills_ctrl_inner skill = [%d]\r\n",dui_skill_ctrl);
    switch(dui_skill_ctrl)
    {
    case DUI_PLAYER_CTRL_PAUSE:
        player_manager_pause();
    break;
    case DUI_PLAYER_CTRL_RESUME:
        player_manager_resume();
    break;
    case DUI_PLAYER_CTRL_PREV:
        player_manager_prev();
    break;
    case DUI_PLAYER_CTRL_NEXT:
        player_manager_next();
    break;
    case DUI_PLAYER_CTRL_SET_VOICE:
	case DUI_SYSTEM_SET_VOLUME:
		
        skill_js = cJSON_GetObjectItem(param_js,"volume");
        if(skill_js){
            if(0 == strcmp(skill_js->valuestring,"+")){
                sprintf(cmd_buf, "[{\"ctrlType\":\"sound\",\"method\":\"add\",\"offset\":\"20\"}]");
            }else if(0 == strcmp(skill_js->valuestring,"-")){
				sprintf(cmd_buf, "[{\"ctrlType\":\"sound\",\"method\":\"sub\",\"offset\":\"20\"}]");
            }else if(0 == strcmp(skill_js->valuestring,"max")){
				sprintf(cmd_buf, "[{\"ctrlType\":\"sound\",\"method\":\"abs\",\"offset\":\"100\"}]");
            }else if(0 == strcmp(skill_js->valuestring,"min")){
				sprintf(cmd_buf, "[{\"ctrlType\":\"sound\",\"method\":\"abs\",\"offset\":\"1\"}]");
            }else{
                volume = (int)atoi(skill_js->valuestring);
				sprintf(cmd_buf, "[{\"ctrlType\":\"sound\",\"method\":\"abs\",\"offset\":\"%d\"}]", volume);
            }
			send_msg_to_gui(cmd_buf, strlen(cmd_buf));
        }
    break;
	case DUI_PLAYER_CTRL_MUTE:
		sprintf(cmd_buf, "[{\"ctrlType\":\"sound\",\"method\":\"abs\",\"offset\":\"0\"}]");
		send_msg_to_gui(cmd_buf, strlen(cmd_buf));
	break;
	case DUI_SYSTEM_SET_BRIGHTNESS:
		skill_js = cJSON_GetObjectItem(param_js,"brightness");
        if(skill_js){
            if(0 == strcmp(skill_js->valuestring,"+")){
                sprintf(cmd_buf, "[{\"ctrlType\":\"brightness\",\"method\":\"add\",\"offset\":\"20\"}]");
            }else if(0 == strcmp(skill_js->valuestring,"-")){
				sprintf(cmd_buf, "[{\"ctrlType\":\"brightness\",\"method\":\"sub\",\"offset\":\"20\"}]");
            }else if(0 == strcmp(skill_js->valuestring,"max")){
				sprintf(cmd_buf, "[{\"ctrlType\":\"brightness\",\"method\":\"abs\",\"offset\":\"100\"}]");
            }else if(0 == strcmp(skill_js->valuestring,"min")){
				sprintf(cmd_buf, "[{\"ctrlType\":\"brightness\",\"method\":\"abs\",\"offset\":\"1\"}]");
            }else{
                volume = (int)atoi(skill_js->valuestring);
				sprintf(cmd_buf, "[{\"ctrlType\":\"brightness\",\"method\":\"abs\",\"offset\":\"%d\"}]", volume);
            }
			send_msg_to_gui(cmd_buf, strlen(cmd_buf));
        }
	break;
    case DUI_PLAYER_CTRL_STOP:
        player_manager_stop();
    break;
    case DUI_SH_DEVICE_TURN_ON:
	case DUI_SH_DEVICE_TURN_OFF:
	case DUI_SH_OPEN_MODE:
        handle_dui_protol(dui_skill_ctrl, param_js);  // smarthome skill
    break;
    default:
		ret = -1;
    	break;

    }
    return ret;
}

int app_skill_handler(cJSON *command_js,bool is_native)
{
    cJSON *api_js = cJSON_GetObjectItem(command_js, "api");
    if (api_js && 0 != strlen(api_js->valuestring)) {
        cJSON *param_js = cJSON_GetObjectItem(command_js, "param");

        dui_skill_ctrl_t skill_ctrl = DUI_SKILL_CTRL_UNKNOWN;
        void* param = NULL;

        skill_ctrl = dui_skill_ctrl_find(api_js->valuestring, 
            skill_ctrl_map, sizeof(skill_ctrl_map)/sizeof(dui_skill_ctrl_map_t));
        return dui_skills_ctrl_inner(skill_ctrl,param_js,param,is_native);
    }
    return -1;  
}

/***************************************
 * 离线识别命令词解析参考
 * ************************************/
int handle_offline_protol(const char *rec,int spot)
{
    if(!strcmp(rec,"kai deng")){
    }
    else if(!strcmp(rec,"wo xiang kan shu")){  
    }
}
