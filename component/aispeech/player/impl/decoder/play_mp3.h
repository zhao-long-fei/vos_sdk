#ifndef PLAY_MP3_H
#define PLAY_MP3_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int mp3_decoder_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
play_decoder_err_t mp3_decoder_process(play_decoder_t *pd);
bool mp3_decoder_has_post(play_decoder_t *pd);
void mp3_decoder_destroy(play_decoder_t *pd);

#define DEFAULT_MP3_DECODER {\
    .type = "mp3", \
    .init = mp3_decoder_init, \
    .process = mp3_decoder_process, \
    .has_post = mp3_decoder_has_post, \
    .destroy = mp3_decoder_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
