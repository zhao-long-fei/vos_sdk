#ifndef M3U8_PREPROCESSOR_H
#define M3U8_PREPROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_preprocessor.h"

int m3u8_preprocessor_init(play_preprocessor_t *pp, play_preprocessor_cfg_t *cfg);
preprocessor_err_t m3u8_preprocessor_poll(play_preprocessor_t *pp, int timeout);
bool m3u8_preprocessor_has_post(play_preprocessor_t *pp);
void m3u8_preprocessor_destroy(play_preprocessor_t *pp);

#define DEFAULT_M3U8_PREPROCESSOR { \
    .type = "m3u8", \
    .init = m3u8_preprocessor_init, \
    .poll = m3u8_preprocessor_poll, \
    .has_post = m3u8_preprocessor_has_post, \
    .destroy = m3u8_preprocessor_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
