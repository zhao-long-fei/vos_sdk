#ifndef PLAY_FLAC_H
#define PLAY_FLAC_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int flac_decoder_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
int flac_decoder_process(play_decoder_t *pd);
bool flac_decoder_has_post(play_decoder_t *pd);
void flac_decoder_destroy(play_decoder_t *pd);

#define DEFAULT_FLAC_DECODER {\
    .init = flac_decoder_init, \
    .process = flac_decoder_process, \
    .has_post = flac_decoder_has_post, \
    .destroy = flac_decoder_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
