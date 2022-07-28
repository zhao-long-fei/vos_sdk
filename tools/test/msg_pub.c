#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#define ROBOT_CTRL_MSGQ_BUF_SIZE_S 512
#define ROBOT_CTRL_MSGQ_ID_S 32
#define ROBOT_CTRL_MSGQ_PATH_NAME_S "/tmp"
#define ROBOT_CTRL_MSGQ_SERVER_MSG 1


typedef struct mymsgsub
{
    long msgtype;
    char content[ROBOT_CTRL_MSGQ_BUF_SIZE_S];
}dreame_msgbuf_sub;
int g_robot_msg_pid_sub;
static int create_dreame_msgsub(){
    int qid;
    int msglen; 
    key_t msgkey;   
    msgkey = ftok(ROBOT_CTRL_MSGQ_PATH_NAME_S, ROBOT_CTRL_MSGQ_ID_S);
    if (msgkey == -1)
    {
        printf("create msgque sub failed\r\n");
        return -1;
    }
    printf("create msgque sub success \r\n");
    g_robot_msg_pid_sub = msgget(msgkey, IPC_CREAT | 0666);
    if (g_robot_msg_pid_sub == -1)
    {
        printf("get msg que sub failed\r\n");
        return -1;
    }  
    printf("msg-sub key=[%d] pid = [%d]\r\n",msgkey,  g_robot_msg_pid_sub );
    return 0;
}

static void sigint_handler(int sig)
{
    exit(0);
}

int main(void *args)
{
    create_dreame_msgsub();

    dreame_msgbuf_sub msg_sub;
    while(1){
        memset(&msg_sub,0,sizeof(msg_sub));
        msg_sub.msgtype = ROBOT_CTRL_MSGQ_SERVER_MSG;
        if(msgrcv(g_robot_msg_pid_sub, &msg_sub, ROBOT_CTRL_MSGQ_BUF_SIZE_S, ROBOT_CTRL_MSGQ_SERVER_MSG, 0) != -1){
            printf("\n recv dreame msg [%s]\r\n",msg_sub.content);
        }else{
          //  printf("No message available for msgrcv()\n");
        }
        usleep(2000000);
    }
}