

#ifndef VOL_CTRL_H__
#define VOL_CTRL_H__

#define VOLUME_MAX_VALUE 90
#define VOLUME_MIN_VALUE 20
#define VOLUME_INIT_VALUE 60


int set_volume(int volume) ;

int set_max_volume(void) ;

int set_min_volume(void) ;

int restore_volume(void) ;

int increase_volume(void);

int decrease_volume(void); 

int get_volume(void);

#endif /* AC_CTRL_H__ */
