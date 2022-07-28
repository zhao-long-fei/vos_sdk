#ifndef FILE_PREPROCESSOR_H
#define FILE_PREPROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_preprocessor.h"

int file_preprocessor_init(play_preprocessor_t *pp, play_preprocessor_cfg_t *cfg);
preprocessor_err_t file_preprocessor_poll(play_preprocessor_t *pp, int timeout);
bool file_preprocessor_has_post(play_preprocessor_t *pp);
void file_preprocessor_destroy(play_preprocessor_t *pp);

#define DEFAULT_FILE_PREPROCESSOR { \
    .type = "file", \
    .init = file_preprocessor_init, \
    .poll = file_preprocessor_poll, \
    .has_post = file_preprocessor_has_post, \
    .destroy = file_preprocessor_destroy, \
}

#ifdef __cplusplus
}
#endif
#endif
