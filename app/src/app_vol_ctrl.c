
#include "syslog.h"

#include "xlog.h"

#include "flash_preprocessor.h"

#include "player_manager.h"

#include "app_vol_ctrl.h"


//机器音量
static int s_cur_volume = 80;
static int s_last_volume = 80;



 int set_volume(int volume) {
	int ret = -1;
	if(volume < VOLUME_MIN_VALUE){
		volume = VOLUME_MIN_VALUE;
	}
	if(volume > VOLUME_MAX_VALUE){
		volume = VOLUME_MAX_VALUE;
	}
	// 此处通过调用alsa接口实现音量调节
	
	/*
	ret = dsp_managet_set_volume(VOLUME_TYPE_SET,volume);
	if(ret == 0) {
		s_cur_volume = volume;
		switch (volume)
		{
		case VOLUME_MIN_VALUE:
		// 添加自己的音量调节音频
			//app_flash_media_play(MEDIA_VOL_MIN, app_main_local_play_cb, false);
			break;
		case 40:
			//app_flash_media_play(MEDIA_VOL_40, app_main_local_play_cb, false);
			break;
		case 60:
			//app_flash_media_play(MEDIA_VOL_60, app_main_local_play_cb, false);
			break;
		case 80:
			//app_flash_media_play(MEDIA_VOL_80, app_main_local_play_cb, false);
			break;
		case VOLUME_MAX_VALUE:
			//app_flash_media_play(MEDIA_VOL_MAX, app_main_local_play_cb, false);
			break;
		default:
			break;
		}
	}
	*/
	return ret;
}

 int increase_volume() {
	int ret = -1;
	int volume = s_cur_volume + 20;
	ret = set_volume(volume);
	return ret;
}

 int decrease_volume() {
	int ret = -1;
	int volume;
	if(s_cur_volume == VOLUME_MAX_VALUE) {
		volume = s_cur_volume - (100 - VOLUME_MAX_VALUE);
	}
	else {
		volume = s_cur_volume - 20;
	}
	ret = set_volume(volume);
	return ret;
}

 int set_max_volume(void)
 {
 	return set_volume(VOLUME_MAX_VALUE);
 }
 
 int set_min_volume(void)
 {
 	return set_volume(VOLUME_MIN_VALUE);
 }

 
 int restore_volume(void)
 {
	return set_volume(s_cur_volume);
 }

 int get_volume() {
	return s_cur_volume;
}

