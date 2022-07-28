#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define ROBOT_CTRL_MSGQ_BUF_SIZE_S 512
#define ROBOT_CTRL_MSGQ_ID_S 44
#define ROBOT_CTRL_MSGQ_PATH_NAME_S "/data"
#define ROBOT_CTRL_MSGQ_SERVER_MSG 1


typedef struct mymsgsub
{
    long msgtype;
    char content[ROBOT_CTRL_MSGQ_BUF_SIZE_S];
}dreame_msgbuf_pub;


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

int main()
{
    create_dreame_msgsub();

    while(1){
    dreame_msgbuf_pub msg_pub;
    memset(&msg_pub, 0, sizeof(msg_pub));
    msg_pub.msgtype = 1;
    strcpy(msg_pub.content, "hello hello hello hello");
    if(msgsnd(g_robot_msg_pid_sub, &msg_pub, strlen(msg_pub.content)+1, IPC_NOWAIT) == -1)
    {
        printf("send json to zhuimi failed\r\n");
        perror("send error-->");
    }else{
        printf("send json [%s] to zhuimi success\r\n",msg_pub.content);
    }
    usleep(2000000);
    }
}