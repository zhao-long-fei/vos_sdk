#include <stdio.h>
#include <stdlib.h>
#include "dds.h"
#include "gh.h"
#include "gh_thread.h"
#include "gh_queue.h"
#include "gh_ringbuf.h"
#include "gh_event_group.h"
#include "gh_time.h"
#include "gh_timer.h"
#include "gh_mutex.h"
#include <fcntl.h>
#include <signal.h>

FILE *fp;
gh_thread_handle_t t_dds_helper;
gh_thread_handle_t t_dds;
gh_ringbuf_handle_t rb_dds;
struct dds_msg *dds;
static int running;
static int startflag = 0;

static void start_dds()
{
    printf("###func=[%s] [%d]\r\n", __func__, __LINE__);
    running = 1;
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
    dds_msg_set_string(msg, "action", "start");
    dds_msg_set_string(msg, "asrParams", "{\"vadEnable\":true,\"enableRealTimeFeedback\":true,\"res\":\"aihome\"}");
    dds_send(msg);
    dds_msg_delete(msg);
}

static void feed_dds(const char *buf, int size)
{
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_AUDIO_STREAM);
    dds_msg_set_bin(msg, "audio", buf, size);
    dds_send(msg);
    dds_msg_delete(msg);
}

static void stop_dds()
{
    printf("stop dds\r\n");
    startflag = 0;
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
    dds_msg_set_string(msg, "action", "end");
    dds_send(msg);
    dds_msg_delete(msg);
    running = 0;
}

static void reset_dds()
{
    printf("reset dds\r\n");
    struct dds_msg *msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_RESET);
    dds_msg_set_boolean(msg, "keepConnection", 1); //是否关连接
    dds_send(msg);
    dds_msg_delete(msg);
}

static int dds_handler(void *userdata, struct dds_msg *msg)
{
    int type;
    int errorId;
    char *str_value;
    char *str_text;
    char *error;
    dds_msg_get_integer(msg, "type", &type);
    switch (type)
    {
    case DDS_EV_OUT_DUI_LOGIN:
    {
        printf("dds login event\r\n");
        dds_msg_get_string(msg, "deviceName", &str_value);
        dds_msg_get_string(msg, "error", &error);
        break;
    }
    case DDS_EV_OUT_WEB_CONNECT:
    {
        dds_msg_get_string(msg, "result", &str_value);
        printf("dds----[%s]\r\n", str_value);
        break;
    }
    case DDS_EV_OUT_STATUS:
    {
        dds_msg_get_string(msg, "status", &str_value);
        printf("DDS_EV_OUT_STATUS [ %s ]\r\n", str_value);
        break;
    }
    case DDS_EV_OUT_ERROR:
    {
        dds_msg_get_string(msg, "error", &str_value);
        char *source = NULL;
        dds_msg_get_string(msg, "source", &source);
        dds_msg_get_integer(msg, "errorId", &errorId);
        break;
    }
    case DDS_EV_OUT_ASR_RESULT:
    {
        printf("get dds asr result\r\n");
        char *value = NULL;
        if (!dds_msg_get_string(msg, "text", &value))
        {
            printf("DDS_EV_OUT_ASR_RESULT text = [%s] \r\n", value);
        }
        if (!dds_msg_get_string(msg, "var", &value))
        {
            printf("DDS_EV_OUT_ASR_RESULT var = [%s] \r\n", value);
        }
        if (!dds_msg_get_string(msg, "pinyin", &value))
        {
            printf("DDS_EV_OUT_ASR_RESULT pinyin = [%s] \r\n", value);
            stop_dds();
        }
        printf("\r\n");
        break;
    }
    case DDS_EV_OUT_DUI_RESPONSE:
    {
        printf("get dds DDS_EV_OUT_DUI_RESPONSE\r\n");
        dds_msg_get_string(msg, "response", &str_value);
        break;
    }
    case DDS_EV_OUT_ASR_RAW:
        break;
    default:
        break;
    }
    return 0;
}

static void dds_helper_run(void *args)
{
    gh_err_t err;
    char *buf = gh_malloc(1024);
    while (1)
    {
        if (startflag)
        {
            int bytes;
            bytes = 1024;
            err = gh_ringbuf_blocked_read(rb_dds, buf, &bytes);
            if (err == GH_ERR_ABORT)
            {
                reset_dds();
                break;
            }
            else if (err == GH_ERR_FINISHED)
            {
                if (bytes)
                    feed_dds(buf, bytes);
                stop_dds();
                break;
            }
            feed_dds(buf, bytes);
            usleep(32000);
        }
    }
    gh_free(buf);
}

static void dds_run(void *args)
{
    printf("func=[%s]----line=[%d]\r\n", __func__, __LINE__);
    struct dds_opt opt = {
        ._handler = dds_handler};
    int ret = dds_start(dds, &opt);
    if (ret != 0)
    {
        printf("dds start error\r\n");
    }
    else
    {
        printf("dds start success\r\n");
    }
}

static void sigint_handler(int sig)
{
    printf("cloud vad test stop\r\n");
    exit(0);
}

static int test_tianqi()
{
    struct dds_msg *msg = NULL;
    msg = dds_msg_new();
    dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
    dds_msg_set_string(msg, "action", "start");
    dds_msg_set_string(msg, "asrParams", "{\"enableRealTimeFeedback\":true,\"enableVAD\":true,\"enablePunctuation\":false,\"language\":\"zh-CN\",\"res\":\"aihome\",\"realBack\":true}");
    dds_send(msg);
    dds_msg_delete(msg);
    msg = NULL;
    FILE *f = fopen("testaudio/tianqi.wav", "rb");
    fseek(f, 44, SEEK_SET);
    char data[3200];
    int len;
    struct dds_msg *m;
    while(1)
    {
        len = fread(data, 1, sizeof(data), f);
        if(len <= 0) break;
        m = dds_msg_new();
        dds_msg_set_type(m, DDS_EV_IN_AUDIO_STREAM);
        dds_msg_set_bin(m, "audio", data, len);
        dds_send(m);
        dds_msg_delete(m);
        usleep(100000);       
    }
    fclose(f);
}

int main()
{
    signal(SIGINT, sigint_handler);
    rb_dds = gh_ringbuf_create(900000);
    dds = dds_msg_new();
    dds_msg_set_string(dds, "productId", "279608047");
    dds_msg_set_string(dds, "aliasKey", "test");
    dds_msg_set_string(dds, "productSecret", "0d656d73326541ff344990d007035367");
    dds_msg_set_string(dds, "productKey", "44aafb87e2ec1f45e994dc9b1afa011d");
    dds_msg_set_string(dds, "deviceProfile", "ZvTxH9g2YdKYwISQBCaxa2zamK/W0fetqJ+ZJpokV6rypBDbQSmLGoTdnpOTkIjdxYuNiprT3ZuaiZacmraRmZDdxYTdj42Qm4qci7ab3cXdzcjGyc/Hz8vI3dPdm5qJlpyasZ6Smt3F3b3Lyc+6u7rGubrHut3T3Y+TnouZkI2S3cXdk5aRiofd092Snpzdxd2dy8nPmpuaxpmax5rdgtPdm5qJlpyasZ6Smt3F3b3Lyc+6u7rGubrHut3T3ZuaiZacmqyanI2ai93F3ZnLxp7ImszLzcbMx8vGysudyJmby8idzJqemZrLz8fP3dPdj42Qm4qci7ab3cXdzcjGyc/Hz8vI3dPdj42Qm4qci6+KnZOWnLSaht3F3cfJysrHxs6Zy57IzM/IyZmayp3JnczHz8bPns+Zms3Kxs/PmsfNxsbMnM2dy8yZy53Lms6czpmdzsvOnpyZyZ3LmsnGm8bLzpuenpydns7MmsmancyZzZvHyZvHxp3HmpnOxsbMysbIzcqdy8qcmsbJnJnOzZzHzcmbm8+am5vG3dPdj42QmZaTmrmWkZiaja+NlpGL3cXdy8+cnsbMmp7Gz8yeyM+Zx5nPm5rIyc7Izpubxsedz83Gz5vHnZvKnMjInpuamcbJysvPnc7GyM6ZzczMzsbNnMfPypvJy5vIzcedns6emZvPms3Gm8vMm8vPnsjGz5zOncybmsnLy8mZyZ6byp7Ny87PnZmcx8aZxprLnJ6exsvd092MnJCPmt3FpN2ek5Pd092bm4zdooI=");

    // {
    //     gh_thread_cfg_t tc = {
    //         .tag = "ddsh",
    //         .run = dds_helper_run,
    //     };
    //     t_dds_helper = gh_thread_create_ex(&tc);
    // }
    {
        gh_thread_cfg_t tc = {
            .tag = "dds",
            .run = dds_run,
        };
        t_dds = gh_thread_create_ex(&tc);
    }
    char p;
    char data[1024] = {0};
    int len;
    FILE *f;
#if 1
    while (1)
    {
        memset(data, 0, sizeof(data));
        p = getc(stdin);
        switch (p)
        {
        case 't':
            test_tianqi();
            break;
        case 's':
        {
            stop_dds();
        }
        break;
        default:
            break;
        }
    }
#else
    while (1)
    {
        memset(data, 0, sizeof(data));
        p = getc(stdin);
        switch (p)
        {
        case '1':
        {
            printf("测试今天天气怎么样\r\n");
            f = fopen("testaudio/tianqi.wav", "rb");
            if (f)
            {
                start_dds();
                gh_ringbuf_start(rb_dds);
                printf("open success\r\n");
                fseek(f, 44, SEEK_SET);
                while (1)
                {
                    len = fread(data, 1, 1024, f);
                    if (len <= 0)
                        break;
                    startflag = 1;
                    gh_ringbuf_try_write(rb_dds, data, &len);
                    usleep(32000);
                }
            }
        }
        break;
        case '2':
        {
            printf("测试日期\r\n");
            f = fopen("testaudio/riqi.wav", "rb");
            if (f)
            {
                start_dds();
                printf("open success\r\n");
                usleep(1000);
                gh_ringbuf_start(rb_dds);
                fseek(f, 44, SEEK_SET);
                while (1)
                {
                    len = fread(data, 1, 1024, f);
                    if (len <= 0)
                        break;
                    startflag = 1;
                    gh_ringbuf_try_write(rb_dds, data, &len);
                }
            }
        }
        break;
        case '3':
        {
            printf("测试清扫\r\n");
            f = fopen("testaudio/clean.wav", "rb");
            if (f)
            {
                start_dds();
                printf("open success\r\n");
                gh_ringbuf_start(rb_dds);
                fseek(f, 44, SEEK_SET);
                while (1)
                {
                    len = fread(data, 1, 1024, f);
                    if (len <= 0)
                        break;
                    startflag = 1;
                    gh_ringbuf_try_write(rb_dds, data, &len);
                }
            }
        }
        break;
        case '0':
        {
            stop_dds();
        }
        break;
        default:
            break;
        }
        gh_ringbuf_finish(rb_dds);
        startflag = 0;
        if (f)
            fclose(f);
        usleep(10000);
    }
#endif
    dds_msg_delete(dds);
    gh_ringbuf_destroy(rb_dds);
}