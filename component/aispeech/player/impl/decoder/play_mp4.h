#ifndef PLAY_MP4_H
#define PLAY_MP4_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int mp4_decoder_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
int mp4_decoder_process(play_decoder_t *pd);
bool mp4_decoder_has_post(play_decoder_t *pd);
void mp4_decoder_destroy(play_decoder_t *pd);

#define DEFAULT_MP4_DECODER {\
    .init = mp4_decoder_init, \
    .process = mp4_decoder_process, \
    .has_post = mp4_decoder_has_post, \
    .destroy = mp4_decoder_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
