#ifndef PLAYLIST_H
#define PLAYLIST_H
#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "play_preprocessor.h"
#include "player_manager.h"

struct playlist_item {
    struct list_head next;
    player_play_t target;
    bool is_favorite;
    char *title;
    char *subtitle;
    char *label;
    int index;
};

void playlist_manager_init();
void playlist_manager_init2(int playlist_size, int favorite_size);
void playlist_manager_add_begin();
void playlist_manager_add(const struct playlist_item *item);
void playlist_manager_add_end();
struct playlist_item *playlist_manager_current();

struct playlist_item *playlist_manager_prev();
struct playlist_item *playlist_manager_next();

//播放结束时需要根据播放模式来获取下一首
struct playlist_item *playlist_manager_next2();

//设置/获取播放模式
void playlist_manager_set_mode(play_mode_t mode);
void playlist_manager_get_mode(play_mode_t *mode);

//收藏当前歌曲
void playlist_manager_collect();

struct playlist_item *playlist_manager_favorite();

bool playlist_manager_check_favorite();


void playlist_manager_deinit();

#ifdef __cplusplus
}
#endif
#endif
