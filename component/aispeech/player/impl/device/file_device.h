#ifndef FILE_DEVICE_H
#define FILE_DEVICE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_device.h"

int file_device_open(play_device_t *pd, play_device_cfg_t *cfg);
int file_device_start(play_device_t *pd);
int file_device_write(play_device_t *pd, const char *buf, int len, int timeout);
int file_device_stop(play_device_t *pd);
int file_device_abort(play_device_t *pd);
void file_device_close(play_device_t *pd);

#define DEFAULT_FILE_DEVICE { \
    .open = file_device_open, \
    .start = file_device_start, \
    .write = file_device_write, \
    .stop = file_device_stop, \
    .abort = file_device_abort, \
    .close = file_device_close, \
}

#ifdef __cplusplus
}
#endif
#endif
