#ifndef VOICE_MSG_PRASE_H__
#define VOICE_MSG_PRASE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define VERSION_LEN		16 /*协议版本长度*/

/*语音触发类型，未使用*/
typedef enum {
	ON_TYPE_UNKNOE,
	ON_LOCAL_VOICE,/*本地语音*/
	ON_CLOUD_VOICE,/*在线语音*/
	ON_TYPE_MAX
}voice_on_type_e;

/*语音执行动作类型*/
typedef enum {
	THEN_UNKNOW,
	THEN_SCENE,	/*场景*/
	THEN_ATTR,	/*设备属性控制*/
	THEN_EVENT, /*事件触发*/
	THEN_MAX
}voice_then_type_e;

/*语音触发结构体*/
typedef struct {
	char topic[128];	/*语音转译之后的中文字符串*/
}voice_on_s;

/*设备属性控制结构体*/
typedef struct {
	char opt_identifier[32];	/*属性名*/
	char opt_value[32];			/*属性值，目前按str类型解析*/
}voice_opt_s;

/*语音控制动作结构体*/
typedef struct {
	char level;				/*优先级：可以用于处理多个动作执行优先关系，暂未使用*/
	char then_type;			/*执行类型：详见voice_then_type_e*/
	char thing_type;		/*用于设备类型*/
	char opt_num;			/*属性参数数量*/
	char thing_name[128];	/*控制对象名，可以是设备名或者场景名等*/
	char location[128];		/*空间名*/
	voice_opt_s* opt;		/*属性参数*/
}voice_then_s;


/*本地语音规则文件，支持多个语音触发voice_on对应多个执行动作voice_then*/
typedef struct {
	char version[VERSION_LEN];	/*版本，用于后期版本升级和兼容处理*/
	int rule_id;				/*规则id，暂无实际用处*/
	char on_num;				/*语音触发数量*/
	char then_num;				/*执行动作数量*/
	voice_on_s *voice_on;		/*一条规则下所有的on的意图必须完全一样，不一样就需要重新配一条规则*/
	voice_then_s *voice_then;	/*语音执行动作*/
}voice_rule_s;

/**
	语音规则配置初始化
*/
extern int voice_rules_prase_init(void);

/**
	解析并执行语音控制
	【param in】：voice_on 离线在线语音输出的中文字符串
*/
extern int voice_on_prase(char* voice_on);

/*
	控制设备属性
	【param in】
	location 空间 name 设备名
	dev_type 设备类型
	identifier 属性名
	value	属性值
*/
extern int attr_opt_ctrl_by_voice(char* location, char* name, char dev_type, char* identifier, char* value);

/*
	场景控制
	【param in】
	location 空间
	name	场景名
*/
extern int scene_opt_ctrl_by_voice(char* location, char* name);


/*
给gui发送消息
*/
extern int send_msg_to_gui(unsigned char *data, int len);


#ifdef __cplusplus
}
#endif

#endif
