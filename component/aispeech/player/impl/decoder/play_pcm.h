#ifndef PLAY_PCM_H
#define PLAY_PCM_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int pcm_decoder_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
play_decoder_err_t pcm_decoder_process(play_decoder_t *pd);
bool pcm_decoder_has_post(play_decoder_t *pd);
void pcm_decoder_destroy(play_decoder_t *pd);

#define DEFAULT_PCM_DECODER {\
    .type = "pcm", \
    .init = pcm_decoder_init, \
    .process = pcm_decoder_process, \
    .has_post = pcm_decoder_has_post, \
    .destroy = pcm_decoder_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
