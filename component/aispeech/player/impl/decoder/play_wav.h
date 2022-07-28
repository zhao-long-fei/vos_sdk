#ifndef PLAY_WAV_H
#define PLAY_WAV_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int wav_decoder_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
play_decoder_err_t wav_decoder_process(play_decoder_t *pd);
bool wav_decoder_has_post(play_decoder_t *pd);
void wav_decoder_destroy(play_decoder_t *pd);

#define DEFAULT_WAV_DECODER {\
    .type = "wav", \
    .init = wav_decoder_init, \
    .process = wav_decoder_process, \
    .has_post = wav_decoder_has_post, \
    .destroy = wav_decoder_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
