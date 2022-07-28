#ifndef DEVICE_UPLOAD_H
#define DEVICE_UPLOAD_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef API_EXPORT
#define API_EXPORT __attribute((visibility("default")))
#endif

typedef struct device_upload_client device_upload_client_t;

typedef struct {
    char *url;
}device_upload_cfg_t;

API_EXPORT device_upload_client_t *device_upload_client_open(device_upload_cfg_t *cfg);
API_EXPORT int device_upload_client_send_text(device_upload_client_t *obj, const void *buf, int len);
API_EXPORT int device_upload_client_send_binary(device_upload_client_t *obj, const void *buf, int len);
API_EXPORT void device_upload_client_close(device_upload_client_t *obj);

#ifdef __cplusplus
}
#endif
#endif
