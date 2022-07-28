#ifndef PLAY_TS_H
#define PLAY_TS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_decoder.h"

int play_ts_init(play_decoder_t *pd, play_decoder_cfg_t *cfg);
play_decoder_err_t play_ts_process(play_decoder_t *pd);
bool play_ts_has_post(play_decoder_t *pd);
void play_ts_destroy(play_decoder_t *pd);

#define DEFAULT_TS_DECODER {\
        .type = "ts", \
        .init = play_ts_init, \
        .process = play_ts_process, \
        .has_post = play_ts_has_post, \
        .destroy = play_ts_destroy, \
    }

#ifdef __cplusplus
}
#endif
#endif
