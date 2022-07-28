#ifndef ALSA_DEVICE_H
#define ALSA_DEVICE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_device.h"

int alsa_device_open(play_device_t *pd, play_device_cfg_t *cfg);
int alsa_device_start(play_device_t *pd);
int alsa_device_write(play_device_t *pd, const char *buf, int len, int timeout);
int alsa_device_stop(play_device_t *pd);
int alsa_device_abort(play_device_t *pd);
void alsa_device_close(play_device_t *pd);

#define DEFAULT_ALSA_DEVICE { \
    .open = alsa_device_open, \
    .start = alsa_device_start, \
    .write = alsa_device_write, \
    .stop = alsa_device_stop, \
    .abort = alsa_device_abort, \
    .close = alsa_device_close, \
}

#ifdef __cplusplus
}
#endif
#endif
