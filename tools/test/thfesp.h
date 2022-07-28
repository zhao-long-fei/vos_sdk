#ifndef __THFESP_H__
#define __THFESP_H__

#ifdef __cplusplus
extern "C" {
#endif

#define THFESP_VERSION "THFESP 0.1.9"

#if (!(defined THFESP_CALL) || !(defined THFESP_IMPORT_OR_EXPORT))
	#if defined _WIN32
		#if defined _WIN64
			#define THFESP_CALL __stdcall
		#else
			#define THFESP_CALL
		#endif

		#ifdef THFESP_IMPLEMENTION
			#define THFESP_IMPORT_OR_EXPORT __declspec(dllexport)
		#else
			#define THFESP_IMPORT_OR_EXPORT __declspec(dllimport)
		#endif
#if 0
	#elif defined __ANDROID__
		#define THFESP_CALL
		#define THFESP_IMPORT_OR_EXPORT
		#undef  JNIEXPORT
		#define JNIEXPORT __attribute ((visibility("default")))
#endif
	#elif defined __APPLE__
		#define THFESP_CALL
		#define THFESP_IMPORT_OR_EXPORT
	#elif defined __unix__
		#define THFESP_CALL
		#define THFESP_IMPORT_OR_EXPORT __attribute ((visibility("default")))
	#else
		#define THFESP_CALL
		#define THFESP_IMPORT_OR_EXPORT
	#endif
#endif

#define THFESP_MSG_TYPE_JSON     0
#define THFESP_MSG_TYPE_BINARY   1

enum thfesp_callback_type {
	THFESP_CALLBACK_ORIGIN_PCM = 0,
	THFESP_CALLBACK_WAKEUP,
	THFESP_CALLBACK_DOA,
	THFESP_CALLBACK_BEAMFORMING,
	THFESP_CALLBACK_ORIGIN_AEC_PCM,
	THFESP_CALLBACK_AEC_PCM,
	THFESP_CALLBACK_UPGRADE,
	THFESP_CALLBACK_MESSAGE,
};

struct thfesp;

typedef int (*thfesp_callback)(void *userdata, int type, char *msg, int len);

THFESP_IMPORT_OR_EXPORT struct thfesp * THFESP_CALL thfesp_new(char * cfg);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_register(struct thfesp * thfesp, int callback_type, thfesp_callback callback, void *userdata);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_start(struct thfesp * thfesp, char *cfg);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_stop(struct thfesp * thfesp);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_set_params(struct thfesp * thfesp, char *json);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_get_message(struct thfesp * thfesp, char *json);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_download(struct thfesp * thfesp, char *json);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_dfu(struct thfesp * thfesp, char *json);

THFESP_IMPORT_OR_EXPORT int THFESP_CALL thfesp_delete(struct thfesp * thfesp);

#ifdef __cplusplus
}
#endif

#endif
