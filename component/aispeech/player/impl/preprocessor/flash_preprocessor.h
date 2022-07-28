#ifndef FLASH_PREPROCESSOR_H
#define FLASH_PREPROCESSOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "play_preprocessor.h"

int flash_preprocessor_init(play_preprocessor_t *pp, play_preprocessor_cfg_t *cfg);
preprocessor_err_t flash_preprocessor_poll(play_preprocessor_t *pp, int timeout);
bool flash_preprocessor_has_post(play_preprocessor_t *pp);
void flash_preprocessor_destroy(play_preprocessor_t *pp);
                                                                                
#define DEFAULT_FLASH_PREPROCESSOR \
    { \
        .type = "flash", \
        .init = flash_preprocessor_init, \
        .poll = flash_preprocessor_poll, \
        .has_post = flash_preprocessor_has_post, \
        .destroy = flash_preprocessor_destroy \
    }                                                                           
                                                                                
#ifdef __cplusplus                                                              
}                                                                               
#endif                                                                          
#endif
