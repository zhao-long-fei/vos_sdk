#ifndef PLAYER_MANAGER_H
#define PLAYER_MANAGER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "player.h"

/*
 * player manager内部状态描述
 */
typedef enum {
    //空闲(设备静音)
    STATE_IDLE = 0,
    //提示音播放中
    STATE_PROMPT_PLAYING,
    //音乐播放中
    STATE_MEDIA_PLAYING,
    //音乐暂停(设备静音)
    STATE_MEDIA_PAUSED,
    STATE_MAX,
} player_manager_state_t;

typedef void (*player_manager_cb)(void *userdata,
        player_manager_state_t prev_state, player_manager_state_t cur_state);

typedef struct {
    player_cfg_t prompt_cfg;
    player_cfg_t media_cfg;
    player_manager_cb cb;
    void *userdata;
    
    //rtos创建线程时需要配置
    int stack_size;
    int priority;
    int core_id;
} player_manager_cfg_t;


int player_manager_init(player_manager_cfg_t *cfg);

/*
 * 排队优先级
 */
typedef enum {
    PLAY_PRIORITY_LOW,
    PLAY_PRIORITY_BELOW_NORMAL,
    PLAY_PRIORITY_NORMAL,
    PLAY_PRIORITY_ABOVE_NORMAL,
    PLAY_PRIORITY_HIGH,
} play_priority_t;

/*
 * can_terminated指当前播放可以被终止
 * can_discarded指未播放的可以被丢弃
 * priority用于排队
 */

//播放请求参数
typedef struct {                                                                
    player_play_t target;
    char *title;                                                        
    char *subtitle;                                                     
    char *label;                                                        
    //是否加入列表                                                      
    bool add;                                                           
    /*
     * 仅对prompt生效
     */
    int can_terminated;
    int can_discarded;
    play_priority_t priority;
} player_manager_play_t; 

int player_manager_play(player_manager_play_t *target, bool is_prompt);
int player_manager_stop();
int player_manager_pause();
int player_manager_resume();

void player_manager_next();
void player_manager_prev();

#define PLAYLIST_ITEM_BIT_URL (1 << 0)
#define PLAYLIST_ITEM_BIT_TITLE (1 << 1)
#define PLAYLIST_ITEM_BIT_SUBTITLE (1 << 2)
#define PLAYLIST_ITEM_BIT_LABEL (1 << 3)
#define PLAYLIST_ITEM_BIT_ALL (PLAYLIST_ITEM_BIT_URL | PLAYLIST_ITEM_BIT_TITLE | PLAYLIST_ITEM_BIT_SUBTITLE | PLAYLIST_ITEM_BIT_LABEL)

typedef struct {
    int flags;
    player_play_t target;
    char *title;
    char *subtitle;
    char *label;
} player_manager_playlist_item_t;

//清空原始列表并添加新列表同时开始播放
void player_manager_playlist_update(player_manager_playlist_item_t *item, int count);

//往现有列表中添加
void player_manager_playlist_add(player_manager_playlist_item_t *item, int count);

//根据flags字段来查询相应内容
//成功返回0，失败返回-1
int player_manager_playlist_query(player_manager_playlist_item_t *item);

void player_manager_item_free(player_manager_playlist_item_t *item);

//收藏
//index取值-2:-1:0，分别代表上一首，下一首，当前，第几首
//void player_manager_collect(int index);
//void player_manager_collect_cancel(int index);


typedef enum {
    PLAY_MODE_SINGLE,
    PLAY_MODE_SINGLE_LOOP,
    PLAY_MODE_LOOP,
    PLAY_MODE_RANDOM,
    PLAY_MODE_SEQUENCE
} play_mode_t;

void player_manager_set_mode(play_mode_t mode);
int player_manager_get_mode(play_mode_t *mode);
void player_manager_favorite_set();
void player_manager_favorite_play();


void player_manager_deinit();
#ifdef __cplusplus
}
#endif
#endif
