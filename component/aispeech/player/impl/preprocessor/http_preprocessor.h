#ifndef HTTP_PREPROCESSOR_H
#define HTTP_PREPROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_preprocessor.h"

int http_preprocessor_init(play_preprocessor_t *pp, play_preprocessor_cfg_t *cfg);
preprocessor_err_t http_preprocessor_poll(play_preprocessor_t *pp, int timeout);
bool http_preprocessor_has_post(play_preprocessor_t *pp);
void http_preprocessor_destroy(play_preprocessor_t *pp);

#define DEFAULT_HTTP_PREPROCESSOR { \
    .type = "http", \
    .init = http_preprocessor_init, \
    .poll = http_preprocessor_poll, \
    .has_post = http_preprocessor_has_post, \
    .destroy = http_preprocessor_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
