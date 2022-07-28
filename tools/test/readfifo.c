#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char buf[1024] = {0};
    int fd = -1;
#if 1
    while(1){
         if(access("./messagefifo",F_OK) != -1)
             break;
        usleep(10000);
    }
    printf("wenjian cunzai\r\n");
    while(1){
        fd = open("./messagefifo", O_RDONLY);
        if(-1 == fd){
            continue;
        }else{
            printf("open success\r\n");
            goto loop;
        }
        usleep(10000);
    }
loop:
    printf("************start to wait message***************\r\n");
    while(1){
        memset(buf,0,sizeof(buf));
        int res = read(fd, buf, 1024);
        if(res > 0)
        {
            printf("\n\n %s \n\n",buf);
        }
        usleep(10000);
    }
    close(fd);
#endif
}