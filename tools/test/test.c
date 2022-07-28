#include "hal_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

int main()
{
    int run = 1;
    char p;
    while(1){
    p = getc(stdin);
    switch(p){
        case '1':
        {
            printf("line=[%d] \n",__LINE__);
            audio_hw_device_t *p_dev = NULL;
            audio_stream_in_t *inStream = NULL;
            printf("line=[%d] \n",__LINE__);
            int sample_rate = 16000;
            int channels = 14;
            int rc = load_audio_hw_device(&p_dev);
            printf("line=[%d] \n",__LINE__);
            if (rc) {
	            printf("load_audio_hw_device() error %d\n", rc);
            } 
            rc = p_dev->init_check(p_dev);
            if (rc) {
	            printf("audio_hw_device_t init check error %d\n", rc);
            }
            rc = p_dev->open_input_stream(p_dev, AUDIO_SOURCE_VOICE_RECOGNITION, sample_rate, channels, &inStream);
            if (rc) {
                printf("audio_hw_device_t open_input_stream return error %d\n", rc);
            }
            int size = size = (sample_rate * channels * sizeof(short) / 1000) * 40; 
            char *data = NULL;
            data = malloc(size);
            FILE *fd = fopen("./test.pcm","wb");
            int bytes;
            while(run)
            {
                rc = inStream->read(inStream, data, size);
		        if (rc <= 0) {
			        printf("Error(%d) capturing sample\n", rc);
			        break;
		        }
                bytes = fwrite(data, 1, size, fd);
            }
            fclose(fd);
            free(data);
        }
        break;
        case 'q':
        {
            run = 0;         
            exit(0);
        }
        break;
    }
    }
}