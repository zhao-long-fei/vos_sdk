#ifndef VOICE_PROTOCOL_CONV_H__
#define VOICE_PROTOCOL_CONV_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define LINK_UI	1


#define LOCATION	0b00000001		/*空间*/
#define DEV_NAME	0b00000010		/*设备名*/
#define DEV_TYPE	0b00000100		/*设备类型*/
#define SCENE_NAME	0b00001000		/*场景名*/

/*组合搜索条件*/
#define LOC_DEV_NAME	(LOCATION | DEV_NAME)
#define LOC_DEV_TYPE	(LOCATION | DEV_TYPE)
#define LOC_SCENE_NAME	(LOCATION | SCENE_NAME)

/*设备类型定义*/
typedef enum {
	DEV_TYPE_UNKNOW,
	DEV_TYPE_SWITCH,
	DEV_TYPE_OUTLET,
	DEV_TYPE_LIGHT,
	DEV_TYPE_CURTAIN,
	DEV_TYPE_AIRCONDITIONER,
	DEV_TYPE_FRESHAIR,
	DEV_TYPE_FLOORHEAT,
	DEV_TYPE_MAX
}dev_type_e;

typedef struct {
	char* 		product_key;
	dev_type_e 	dev_type;
}dev_type_map_s;


/*动作下执行语音搜索结构体*/
typedef struct {
	char 	search_type;
	char 	dev_type;
	char*	location;
	char*	name;
	char*	identifier;
	char*	value;
}thing_search_info_s;

/*控制协议结构体，采用json格式*/
typedef struct {
	char*	data;
	int		length;
}thing_ctrl_protocol_s;

/*
typedef struct {
	int					protocol_num;
	protocol_data_s		protocol_data[4];
}thing_ctrl_protocol_s;
*/

/*设备类型定义*/
typedef enum {
	CONFIG_TYPE_UNKNOW,
	CONFIG_TYPE_DEVICE,
	CONFIG_TYPE_SCENE,
	CONFIG_TYPE_ALL
}config_type_e;


/*根据搜索内容转化控制协议*/
int generate_ctrl_protocol_data(thing_search_info_s* search_info, thing_ctrl_protocol_s* ctrl_protocol);
/*打开json配置文件*/
int json_config_open_by_name(char *path_name, cJSON **json_flie);
/*刷新lvgl的场景和设备配置文件， 目前在每次控制前刷新一次，之后可以根据lvgl通知再刷新，提高效率*/
int refresh_search_config(int type);



#ifdef __cplusplus
}
#endif

#endif
