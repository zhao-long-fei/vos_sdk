#pragma GCC diagnostic ignored "-Wformat-security"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mic_ft.h"
#include "spectrum_calc.h"
#include "cJSON.h"

//#include <android/log.h>

//define printf(f, ...) __android_log_print(ANDROID_LOG_INFO, "MICTEST", f, ##__VA_ARGS__)

//#define FT_DEBUG
//#ifdef FT_DEBUG
//#define ft_debug(...)           printf(__VA_ARGS__)
//#else
//#define ft_debug(...)
//#endif

#define FT_VISABLE_ATTR    __attribute__ ((visibility ("default")))

#define SILENCE_TIME 1 /*time in second*/
#define SINE_TIME 4
#define VOICE_TIME 3.7
#define TIME_WIN_LEN 2

#define FS 16000

#define FFT_SIZE 1024
#define FFT_BINS 513

#define SENS_BASE 10000

#define BIN_400HZ 26 /*floor(1000*fft_size/Fs) */
//#define BIN_500HZ 32 /*floor(1000*fft_size/Fs) */
//#define BIN_1KHZ 64 /*floor(1000*fft_size/Fs) */
//#define BIN_4KHZ 256 /*floor(1000*fft_size/Fs) */

struct ft_engine {
    int ready;
    int sinePosStart;
    int sinePosEnd;

    int sineBin;

    int channelCount;
    int cacheSize; // = 5000 * 32 * 6;

    int fs;

    short *rawdata;
    int length;

    char *result_json;
};

/************************************************************
  Function   : ft_confParse()

  Description:
  Calls      :
  Called By  :
  Input      :
  Output     :
  Return     :
  Others     :

  History    :
    2017/04/26, Youhai.Jiang create

************************************************************/
void ft_confParse(ft_engine_t *pstFtEngine, FILE *fp) {
    char line[128];
    char *ptr = NULL;

    while (!feof(fp)) {
        memset(line, '\0', sizeof(line));
        ptr = fgets(line, sizeof(line), fp);

        if (NULL == ptr) {
            continue;
        }

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }


        if (!strncmp(line, "CFG_cache_size", strlen("CFG_cache_size"))) {
            sscanf(line, "CFG_cache_size = %d\n", &pstFtEngine->cacheSize);
//            ft_debug("CFG_cache_size = %d\n", &pstFtEngine->cacheSize);
            continue;
        }

        if (!strncmp(line, "CFG_channel_count", strlen("CFG_channel_count"))) {
            sscanf(line, "CFG_channel_count = %d\n", &pstFtEngine->channelCount);
//            ft_debug("CFG_channel_count = %d\n", &pstFtEngine->channelCount);
            continue;
        }

        if (!strncmp(line, "CFG_sine_pos_start", strlen("CFG_sine_pos_start"))) {
            sscanf(line, "CFG_sine_pos_start = %d\n", &pstFtEngine->sinePosStart);
            continue;
        }

        if (!strncmp(line, "CFG_sine_pos_end", strlen("CFG_sine_pos_end"))) {
            sscanf(line, "CFG_sine_pos_end = %d\n", &pstFtEngine->sinePosEnd);
            continue;
        }

        if (!strncmp(line, "CFG_sine_bin", strlen("CFG_sine_bin"))) {
            sscanf(line, "CFG_sine_bin = %d\n", &pstFtEngine->sineBin);
            continue;
        }

        if (!strncmp(line, "CFG_fs", strlen("CFG_fs"))) {
            sscanf(line, "CFG_fs = %d\n", &pstFtEngine->fs);
            continue;
        }
    }
}

/************************************************************
  Function   : ft_engine_new()

  Description:
  Calls      :
  Called By  :
  Input      :
  Output     :
  Return     :
  Others     :

  History    :
    2017/04/26, Youhai.Jiang create

************************************************************/
FT_VISABLE_ATTR ft_engine_t *ft_engine_new(int channelCount, int sineBin, int fs) {
    ft_engine_t *pstFtEngine = NULL;

    pstFtEngine = (ft_engine_t *) calloc(1, sizeof(ft_engine_t));
    if (NULL == pstFtEngine) {
        printf("%s(): malloc fail.\n", __FUNCTION__);
        if (pstFtEngine) {
            free(pstFtEngine);
        }

        if (pstFtEngine->rawdata) {
            free(pstFtEngine->rawdata);
        }
        return NULL;
    }

    pstFtEngine->channelCount = channelCount;
    pstFtEngine->cacheSize = 5000 * 32 * pstFtEngine->channelCount / sizeof(short);
    pstFtEngine->sineBin = sineBin;
    pstFtEngine->fs = fs;

    int sine_pos_start = (int) ((SILENCE_TIME + ((SINE_TIME - TIME_WIN_LEN) / 2)) *
                                pstFtEngine->fs);
    pstFtEngine->sinePosStart = sine_pos_start;

    int sine_pos_end = (int) ((SILENCE_TIME + ((SINE_TIME + TIME_WIN_LEN) / 2)) *
                              pstFtEngine->fs);
    pstFtEngine->sinePosEnd = sine_pos_end;

    /* malloc for rqaw data */
    pstFtEngine->rawdata = (short *) calloc(1, sizeof(short) * pstFtEngine->cacheSize);

    return pstFtEngine;
}

/************************************************************
  Function   : ft_engine_delet()

  Description:
  Calls      :
  Called By  :
  Input      :
  Output     :
  Return     :
  Others     :

  History    :
    2017/04/26, Youhai.Jiang create

************************************************************/
FT_VISABLE_ATTR void ft_engine_delete(ft_engine_t *pstFtEngine) {
    if (pstFtEngine->rawdata) {
        free(pstFtEngine->rawdata);
    }

    if (pstFtEngine->result_json) {
        free(pstFtEngine->result_json);
    }

    free(pstFtEngine);
}

short *to_short_array(signed char *src, int len) {
    int count = len >> 1;
    short *dest = (short *) calloc(count, sizeof(short));;
    for (int i = 0; i < count; i++) {
        dest[i] = (short) (src[i * 2 + 1] << 8 | src[2 * i] & 0xff);
    }
    return dest;
}

/************************************************************
  Function   : ft_engine_feed()

  Description:
  Calls      :
  Called By  :
  Input      :
  Output     :
  Return     :
  Others     :

  History    :
    2017/04/27, Youhai.Jiang create

************************************************************/
FT_VISABLE_ATTR int ft_engine_feed(ft_engine_t *pstFtEngine, signed char *psByte, int len) {
    int ret = -1;

    short *psData = to_short_array(psByte, len);
    len = len / 2;

    for (int i = 0; i < len; i++) {
        if (pstFtEngine->length + i < pstFtEngine->cacheSize) {
            pstFtEngine->rawdata[pstFtEngine->length + i] = psData[i];
        } else {
            break;
        }
    }

    pstFtEngine->length += len;

    int maxLength = pstFtEngine->sinePosEnd * pstFtEngine->channelCount;

    if (pstFtEngine->length > maxLength) {
        pstFtEngine->length = maxLength;
    }

    if (pstFtEngine->length >= maxLength) {
        ret = 0;
        pstFtEngine->ready = 1;
    }

    free(psData);
    return ret;
}

/************************************************************
  Function   : ft_engine_calc()

  Description:
  Calls      :
  Called By  :
  Input      :
  Output     :
  Return     :
  Others     :

  History    :
    2017/04/27, Youhai.Jiang create

************************************************************/
FT_VISABLE_ATTR int ft_engine_calc(ft_engine_t *pstFtEngine, char **result_json) {
    if (!pstFtEngine->ready) {
        char *err = "Test data is not enough.\n";
        fprintf(stderr, err);
        return -1;
    } else {
        int i;
        static double mic_sine_spec[FFT_BINS];
        double mic_sen;
        char item_name[128];

        cJSON *root_js = cJSON_CreateObject();
        cJSON *amp_js = cJSON_CreateObject();

        for (i = 0; i < pstFtEngine->channelCount; ++i) {
            spectrum_calc_feed(pstFtEngine->rawdata, mic_sine_spec, pstFtEngine->channelCount, i,
                               pstFtEngine->sinePosStart - 1, pstFtEngine->sinePosEnd - 1,
                               FFT_SIZE);
            //计算出DB值
            float delta;
            if (pstFtEngine->sineBin == BIN_400HZ) {
                delta = 41.938;
            } else {
                delta = 44.2782;
            }
            mic_sen = 10 * log10(mic_sine_spec[pstFtEngine->sineBin] / SENS_BASE) - delta;

            sprintf(item_name, "channel_%d_amp", i);
            if (isnan(mic_sen) == 0 && isinf(mic_sen) == 0) {
                cJSON_AddNumberToObject(amp_js, item_name, mic_sen);
            } else {
                cJSON_AddStringToObject(amp_js, item_name, "-200");
            }

        }


        if (!root_js || !amp_js) {
            fprintf(stderr, "cJSON_CreateObject failed\n");
            return -1;
        }

        cJSON_AddItemToObject(root_js, "amplitude", amp_js);

        pstFtEngine->result_json = cJSON_Print(root_js);
        *result_json = pstFtEngine->result_json;

        cJSON_Delete(root_js);

        return 0;
    }
}

