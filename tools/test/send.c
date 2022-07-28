#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include<unistd.h>  
#include<sys/types.h>  
#include<sys/stat.h>  
static int send_js_to_zhuimi(char *param_js)
{
    printf("发送dui平台返回的数据给追觅\r\n");
    if(access("./messagefifo",W_OK) == -1)
    {
        perror("guandap wufa xieru");
        return -1;
    }
    int rd = open ("./messagefifo", O_RDONLY | O_NONBLOCK);  
    int fd = open ("./messagefifo", O_WRONLY |O_NONBLOCK, S_IRWXU);
//    int fd = open("./messagefifo",O_WRONLY);
    if(-1 == fd)
    {
        printf("open fifo error\r\n");
        return -1;
    }
//    ftruncate(fd,0);
//    lseek(fd,0,SEEK_SET);
    char *send_buf = NULL;
    send_buf = strdup(param_js);
    write(fd,send_buf,strlen(send_buf));
    close(fd);
    return 0;
}

int main()
{
    printf("start send test\r\n");
    unlink("./messagefifo");
    int m = mkfifo("./messagefifo",S_IRUSR | S_IWUSR);
    if(-1 == m)
    {
        perror("mkfifo error");
        return -1;
    }
    char p;
    while(1)
    {
        p = getc(stdin);
        switch(p)
        {
            case '1':
            send_js_to_zhuimi("test10000000000");
            break;
            case '2':
            send_js_to_zhuimi("test20000000000");
            break;
            case '3':
            send_js_to_zhuimi("test30000000000");
            break;
            case '4':
            send_js_to_zhuimi("test40000000000");
            break;
            case '5':
            send_js_to_zhuimi("test50000000000");
            break;
        }

    }
}