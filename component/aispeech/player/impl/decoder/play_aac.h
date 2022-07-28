#ifndef PLAY_AAC_H
#define PLAY_AAC_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int aac_decoder_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
play_decoder_err_t aac_decoder_process(play_decoder_t *pd);
bool aac_decoder_has_post(play_decoder_t *pd);
void aac_decoder_destroy(play_decoder_t *pd);

#define DEFAULT_AAC_DECODER { \
        .type = "aac", \
        .init = aac_decoder_init, \
        .process = aac_decoder_process, \
        .has_post = aac_decoder_has_post, \
        .destroy = aac_decoder_destroy, \
    }

#ifdef __cplusplus
}
#endif
#endif
