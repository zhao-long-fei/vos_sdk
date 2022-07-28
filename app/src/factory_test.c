#include "factory_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "mic_ft.h"
#include "gh_thread.h"
#include "gh_queue.h"
#include "gh_ringbuf.h"
#include "gh_event_group.h"
#include "gh_time.h"
#include "gh_timer.h"
#include "gh_mutex.h"
typedef struct {
    gh_ringbuf_handle_t rb_fct;
    gh_thread_handle_t t_fct;
}fct_processor_t;

static fct_processor_t s_fct;
static ft_test_t ft_var;
#define FCT_BUF_SIZE 4096
#define FCT_BUF_READ_ONCE_SIZE 1024
static int gFactoryEnable = 0;
static int gFactoryNew = 0;
static export_result_cb export_cb = NULL;

void fct_audio_feed(char *buf, int len){
    printf("len=%d\r\n",len);
    gh_ringbuf_try_write(s_fct.rb_fct,buf,&len);
}

extern void export_fct_result(char *result);

static void  factory_run(void *ags){
    char *result_json = NULL;
    char *buf = gh_malloc(FCT_BUF_READ_ONCE_SIZE);
    int result = 0;
    int bytes;
    gh_err_t err;
    printf("fct test run..................\r\n");
    while(1){
        while(gFactoryEnable){
            bytes = FCT_BUF_READ_ONCE_SIZE;
            err = gh_ringbuf_blocked_read(s_fct.rb_fct, buf, &bytes);
            if(err == GH_ERR_FINISHED || err == GH_ERR_ABORT){
                printf("产测feed音频结束\r\n");
            }
            if (0 == ft_engine_feed(ft_var.ft_enging, buf, bytes))
            {
                printf("fct engine feed end\n");
                result = ft_engine_calc(ft_var.ft_enging, &result_json);
                if (result == 0)
                {
                    printf("calc success\r\n");
                    if (strlen(result_json) > 0)
                    {
                        printf("收到产测结果 == [%s] \r\n", result_json);
                        if(export_cb)
                            export_cb(result_json);
                        fct_end();
                    }   
                }
                else
                {
                    printf("calc failed\r\n");
                    if(export_cb)
                        export_cb(NULL);
                    fct_end();
                }
            }
        }
        gh_delayms(100);
    }
    if(buf) free(buf);
}

int fct_start(int channels){
    memset(&ft_var, 0, sizeof(ft_test_t));
    ft_var.ft_enging = ft_engine_new(channels, 16, 16000);
    if(!ft_var.ft_enging) return -1;
    gFactoryEnable = 1;
    gh_ringbuf_start(s_fct.rb_fct);
    return 0;
}

void fct_end(){
    gFactoryEnable = 0;
    gh_ringbuf_finish(s_fct.rb_fct);
    if(ft_var.ft_enging)
        ft_engine_delete(ft_var.ft_enging);
}

void fct_init(export_result_cb export_result_cb){
    s_fct.rb_fct = gh_ringbuf_create(FCT_BUF_SIZE);
    {
        gh_thread_cfg_t tc = {
            .tag = "fct_test",
            .run = factory_run,
        };
        export_cb = export_result_cb;
        s_fct.t_fct = gh_thread_create_ex(&tc);
    }
}

void fct_deinit()
{
    gh_thread_destroy(s_fct.t_fct);
    gh_ringbuf_destroy(s_fct.rb_fct);
}