#ifndef ALSA_PREPROCESSOR_H
#define ALSA_PREPROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif
#include "play_preprocessor.h"

int alsa_preprocessor_init(play_preprocessor_t *pp, play_preprocessor_cfg_t *cfg);
preprocessor_err_t alsa_preprocessor_poll(play_preprocessor_t *pp, int timeout);
void alsa_preprocessor_destroy(play_preprocessor_t *pp);

#define DEFAULT_ALSA_PREPROCESSOR { \
    .type = "alsa", \
    .init = alsa_preprocessor_init, \
    .poll = alsa_preprocessor_poll, \
    .destroy = alsa_preprocessor_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
