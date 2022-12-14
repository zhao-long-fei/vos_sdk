#ifndef __DDS_H__
#define __DDS_H__

#ifdef __cplusplus
extern "C" {
#endif

#if (!(defined DDS_CALL) || !(defined DDS_IMPORT_OR_EXPORT))
	#if defined _WIN32
		#if defined _WIN64
			#define DDS_CALL __stdcall
		#else
			#define DDS_CALL
		#endif

		#ifdef DDS_IMPLEMENTION
			#define DDS_IMPORT_OR_EXPORT __declspec(dllexport)
		#else
			#define DDS_IMPORT_OR_EXPORT __declspec(dllimport)
		#endif
#if 0
	#elif defined __ANDROID__
		#define DDS_CALL
		#define DDS_IMPORT_OR_EXPORT
		#undef  JNIEXPORT
		#define JNIEXPORT __attribute ((visibility("default")))
#endif
	#elif defined __APPLE__
		#define DDS_CALL
		#define DDS_IMPORT_OR_EXPORT
	#elif defined __unix__
		#define DDS_CALL
		#define DDS_IMPORT_OR_EXPORT __attribute ((visibility("default")))
	#else
		#define DDS_CALL
		#define DDS_IMPORT_OR_EXPORT
	#endif
#endif

#define DDS_VERSION     "DDS 0.2.108"
#define DDS_VERSION_NUM 2108

/* callback event */
#define DDS_EV_OUT_RECORD_AUDIO                    1
#define DDS_EV_OUT_NATIVE_CALL                     2
#define DDS_EV_OUT_COMMAND                         3
#define DDS_EV_OUT_MEDIA                           4
#define DDS_EV_OUT_STATUS                          5
#define DDS_EV_OUT_TTS                             6
#define DDS_EV_OUT_ERROR                           7
#define DDS_EV_OUT_ASR_RESULT                      8
#define DDS_EV_OUT_DUI_RESPONSE                    9
#define DDS_EV_OUT_DUI_LOGIN                       10
#define DDS_EV_OUT_CINFO_RESULT                    11
#define DDS_EV_OUT_OAUTH_RESULT                    12
#define DDS_EV_OUT_PRODUCT_CONFIG_RESULT		   13
#define DDS_EV_OUT_WEB_CONNECT					   14
#define DDS_EV_OUT_DUI_DEVICENAME				   15
#define DDS_EV_OUT_CINFO_V2_RESULT                 16
#define DDS_EV_OUT_ASR_RAW                         17
#define	DDS_EV_OUT_SYNC_RESULT					   18
#define DDS_EV_OUT_VPR_REGISTER                    19
#define DDS_EV_OUT_VPR_VERIFY                      20
#define DDS_EV_OUT_VPR_UNREGISTER                  21
#define DDS_EV_OUT_ASR_VPR_RESULT                  22
#define	DDS_EV_OUT_REFRESH_TOKEN                   23
#define	DDS_EV_OUT_LASR_RT_RESULT                  24
#define	DDS_EV_OUT_LASR_RT_RAW                     25
#define	DDS_EV_OUT_LASR_RAW                        26
#define DDS_EV_OUT_REQUEST_ID                      27
#define DDS_EV_OUT_VAD_RESULT                      28
#define DDS_EV_OUT_TTS_MULTIPLE                    29
#define DDS_EV_OUT_LASR_FILE_TRANSFER_RESULT       30
#define DDS_EV_OUT_SPEAK                           31
#define DDS_EV_OUT_TTS_VOICEID_CHECK_RESULT		   32
#define	DDS_EV_OUT_NLU_RESULT					   33


/* external event */
#define DDS_EV_IN_SPEECH                           101
#define DDS_EV_IN_WAKEUP                           102
#define DDS_EV_IN_NATIVE_RESPONSE                  103
#define DDS_EV_IN_RESET                            104
#define DDS_EV_IN_EXIT                             105
#define DDS_EV_IN_CUSTOM_TTS_TEXT                  106
#define DDS_EV_IN_AUDIO_STREAM                     107
#define DDS_EV_IN_PLAYER_STATUS                    108
#define DDS_EV_IN_NLU_TEXT						   109
#define DDS_EV_IN_WAKEUP_WORD					   110
#define DDS_EV_IN_CINFO_OPERATE                    111
#define DDS_EV_IN_OAUTH_OPERATE                    112
#define DDS_EV_IN_DM_INTENT		           		   113
#define DDS_EV_IN_COMMON_WAKEUP_WORD               114
#define DDS_EV_IN_PHRASE_HINTS                     115
#define DDS_EV_IN_PRODUCT_CONFIG				   116
#define DDS_EV_IN_REQUEST_ID                       117
#define DDS_EV_IN_SKILL_SETTINGS                   118
#define DDS_EV_IN_SYSTEM_SETTINGS                  119
#define DDS_EV_IN_SET_USERID                       120
#define DDS_EV_IN_SET_OAUTH                        121
#define DDS_EV_IN_CLEAR_OAUTH                      122
#define DDS_EV_IN_ASR_PARAMS					   123
#define	DDS_EV_IN_DM_CONTEXT_SYNC				   124
#define DDS_EV_IN_TTS_RESET						   125
#define DDS_EV_IN_EXTRA_QUERY_STRING			   126
#define DDS_EV_IN_PROFILE_REGISTER                 127
#define DDS_EV_IN_TTS_STOP                         128
#define DDS_EV_IN_LASR_RT_START                    129
#define DDS_EV_IN_LASR_RT_STOP                     130
#define DDS_EV_IN_LASR_FILE_UPLOAD                 131
#define DDS_EV_IN_LASR_TASK_CREATE                 132
#define DDS_EV_IN_LASR_TASK_QUERY                  133
#define DDS_EV_IN_VPR_REGISTER                     134
#define DDS_EV_IN_VPR_VERIFY                       135
#define DDS_EV_IN_VPR_UNREGISTER                   136
#define DDS_EV_IN_AUDIO_LASR                       137
#define DDS_EV_IN_TTS_VOICEID_CHECK                138
#define DDS_EV_IN_LASR_RT_QUERY_STRING	 		   139

/* error id */
#define DDS_ERROR_BASE      1000
#define DDS_ERROR_FATAL     (DDS_ERROR_BASE + 1)
#define DDS_ERROR_TIMEOUT   (DDS_ERROR_BASE + 2)
#define DDS_ERROR_NETWORK   (DDS_ERROR_BASE + 3)
#define DDS_ERROR_SERVER    (DDS_ERROR_BASE + 4)
#define DDS_ERROR_LOGIC     (DDS_ERROR_BASE + 5)
#define DDS_ERROR_INPUT     (DDS_ERROR_BASE + 6)
#define DDS_ERROR_INCOMPLETE_AUDIO     (DDS_ERROR_BASE + 7)

#define DDS_LASR_UPLOAD_BEGIN         0
#define DDS_LASR_UPLOAD_SLICE         1
#define DDS_LASR_UPLOAD_END           2
#define DDS_LASR_TASK_BEGIN           3
#define DDS_LASR_TASK_PROGRESS        4
#define DDS_LASR_TASK_END             5

struct dds_msg;

typedef int (*dds_ev_callback)(void *userdata, struct dds_msg *msg);

struct dds_opt {
	dds_ev_callback _handler;
	void *userdata;
};

DDS_IMPORT_OR_EXPORT int DDS_CALL dds_start(struct dds_msg *conf, struct dds_opt *opt);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_send(struct dds_msg *msg);
DDS_IMPORT_OR_EXPORT char * DDS_CALL dds_get_version();
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_decrypt_profile(char *profile, char *plain);

/* message pack or unpack */
DDS_IMPORT_OR_EXPORT struct dds_msg * DDS_CALL dds_msg_new();
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_delete(struct dds_msg *msg);
DDS_IMPORT_OR_EXPORT void DDS_CALL dds_msg_print(struct dds_msg *msg);

DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_type(struct dds_msg *msg, int value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_integer(struct dds_msg *msg, const char *key, int value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_double(struct dds_msg *msg, const char *key, double value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_boolean(struct dds_msg *msg, const char *key, int value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_string(struct dds_msg *msg, const char *key, const char *value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_bin(struct dds_msg *msg, const char *key, const char *value, int value_len);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_set_bin_p(struct dds_msg *msg, const char *key, const char *value, int value_len);

DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_type(struct dds_msg *msg, int *value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_integer(struct dds_msg *msg, const char *key, int *value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_double(struct dds_msg *msg, const char *key, double *value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_boolean(struct dds_msg *msg, const char *key, int *value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_string(struct dds_msg *msg, const char *key, char **value);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_bin(struct dds_msg *msg, const char *key, char **value, int *value_len);
DDS_IMPORT_OR_EXPORT int DDS_CALL dds_msg_get_bin_p(struct dds_msg *msg, const char *key, char **value, int *value_len);
DDS_IMPORT_OR_EXPORT char* DDS_CALL dds_msg_to_json(struct dds_msg *msg);

#ifdef __cplusplus
}
#endif
#endif
