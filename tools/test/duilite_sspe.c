#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include "duilite.h"
#include "cJSON.h"
#define BUFFER 3200
 
char *auth_cfg = "";
long start_time = 0;
long end_time = 0;
static int RC=0;

long int _get_time()
{   
    long int at = 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    at = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    return at;
}

static void usage()
{
    fprintf(stdout, "Usage: ./duilite_sspe [options]\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "options:\n");
    fprintf(stdout, "  -w [wakeup resources file]\n");
    fprintf(stdout, "  -b [sspe resources file]\n");
    fprintf(stdout, "  -e [env param]\n");
    fprintf(stdout, "  -k [rollBack default=1200]\n");
    fprintf(stdout, "  -m [maxVolume default=0]\n");
    fprintf(stdout, "  -f [input audio file]\n");
    fprintf(stdout, "  -c [channels num default=4]\n");
    fprintf(stdout, "  -o [input file offset]\n");
    fprintf(stdout, "  -s [performance test]\n");
    fprintf(stdout, "  -l [loop times]\n");
    fprintf(stdout, "\n");
}

static int wakeup_callback(void *user_data, int type, char *msg, int len)
{
    printf("[FESPL WK INFO] %.*s\n", len, msg);
    return 0;
}
 
static int doa_callback(void *user_data, int type, char *msg, int len)
{
    printf("[FESPL DOA INFO] %.*s\n", len, msg);
    return 0;
}
 
static int beamforming_callback(void *user_data, int type, char *msg, int len)
{
    if (type == DUILITE_MSG_TYPE_JSON)
    {
        printf("[FESPL BF INFO] %.*s\n", len, msg);
    }
    else
    {
#ifdef USE_SAVE
        FILE *bf = fopen("./fespl.pcm", "ab+");
        if (bf)
        {
            fwrite(msg, 1, len, bf);
            fclose(bf);
        }
#endif
    }

    return 0;
}
 
int main(int argc, char **argv)
{
    int opt;
    char *res_wakeup = NULL;
    char *res_sspe = NULL;
    char *env_cfg = NULL;
    int rollback = 1200;
    int maxvolume = 0;
    char *audiofile = NULL;
    int channel = 4;
    int offset;
    int offset_flag=0;
    int len;
    FILE *in_fd = NULL;
    struct duilite_sspe *fespl;
    char *newcfg = NULL;
    char *start_param="{}"; /*reserved argument*/
    int count = 1;
    char buf[BUFFER * channel];

    //args parsing
    while((opt = getopt(argc, argv, "w:b:c:e:k:m:f:o:h")) != -1)
    {
        switch(opt) 
        {
            case 'w': /* wakeup res file */
                res_wakeup = optarg;
                break;
            case 'b': /* sspe res file */
                res_sspe = optarg;
                break;
            case 'e': /* cfg env param */
                env_cfg = optarg;
                break;
            case 'k': /* rollBack default=1200 */
                rollback = atoi(optarg);
                break;
            case 'm': /* maxVolume default=0 */
                maxvolume = atoi(optarg);
                break;           
            case 'f': /* input audio file*/
                audiofile = optarg;
                break;
            case 'c': /* audio channel num*/
                channel = atoi(optarg);
                break; 
            case 'o': /*read audio file offset*/
                offset = atoi(optarg);
                offset_flag = 1;
                break;
            case 'h':   /* help */
            default:
                usage();
                exit(1);
        }
    }
    if(argc < 3)
    {
        usage();
        return -1;
    }
    cJSON * root =  cJSON_CreateObject();

    if( ((access(res_wakeup,F_OK))!=-1) && ((access(res_sspe,F_OK))!=-1) )
    {
        cJSON_AddItemToObject(root, "wakeupBinPath", cJSON_CreateString(res_wakeup));
        cJSON_AddItemToObject(root, "sspeBinPath", cJSON_CreateString(res_sspe));
        cJSON_AddItemToObject(root, "env", cJSON_CreateString(env_cfg));
        cJSON_AddItemToObject(root, "rollBack", cJSON_CreateNumber(rollback));
        cJSON_AddItemToObject(root, "maxVolume", cJSON_CreateNumber(maxvolume));
    }
    else
    {
        printf("[ERR] wakeup/sspe res file not exit.\n");
        RC=123;
        goto end;
    }

    newcfg=cJSON_Print(root);
    printf("[INFO] New cfg:\n%s\n", newcfg);

    //check input file
    if((access(audiofile,F_OK)) != 0)
    {
        printf("[ERR] input file:%s not exist.\n",audiofile);
        RC=123;
        goto end;
    }

    //load auth
    duilite_library_load(auth_cfg);
 
    fespl = duilite_sspe_new(newcfg);
    if(!fespl)
    {
        printf("[ERR] fespl new fail.\n");
        RC=1;
        goto end;
    }
 
    duilite_sspe_register(fespl, DUILITE_CALLBACK_FESPL_WAKEUP, wakeup_callback, NULL);
    duilite_sspe_register(fespl, DUILITE_CALLBACK_FESPL_DOA, doa_callback, NULL);
    duilite_sspe_register(fespl, DUILITE_CALLBACK_FESPL_BEAMFORMING, beamforming_callback, NULL);


    while(1)
    {
        if(0 != duilite_sspe_start(fespl, start_param))
        {
            printf("[ERR] fespl start fail.\n");
            RC=2;
            goto end;
        }

        in_fd = fopen(audiofile, "r");
        assert(in_fd != NULL);
        if(offset_flag == 1 )
        {
            fseek(in_fd, offset, SEEK_SET);
        }
        else if(strstr(audiofile,".wav"))
        {
            fseek(in_fd, 44, SEEK_SET);
        }

        start_time = _get_time();
        while (1)
        {
            len = fread(buf, 1, sizeof(buf), in_fd);
            if (0 == len)
            {
                break;
            }
            duilite_sspe_feed(fespl, buf, len);
        }
        end_time=_get_time();
        printf("[Cost time]:%ld ms\n", end_time-start_time);
        if(duilite_sspe_stop(fespl) != 0)
        {
            printf("[ERR] duilite_sspe_stop failed\n");
            RC = 4;
            goto end;
        }
    }
end:
    if(in_fd)
    {
        fclose(in_fd);
        in_fd=NULL;
    }
    if(root)
    {
        cJSON_Delete(root);
        root=NULL;
    }
    if(newcfg)
    {
        free(newcfg);
        newcfg=NULL;
    }
    if(fespl)
    {
        duilite_sspe_delete(fespl);
        duilite_library_release();
        fespl=NULL;
    }

    if(RC == 0)
        printf("[Result]:TPASS\n");
    else
        printf("[Result]:TFAIL %d\n",RC);

    return 0;
}
