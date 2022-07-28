#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "msgtothird.h"
#include "gh.h"

static int g_robot_msg_pid_recv;     // 与dreame通信的消息队列号
static int g_robot_msg_pid_send; 

int create_host_msgsend(){
    int qid;
    int msglen; 
    key_t msgkey;   
    msgkey = ftok(ROBOT_CTRL_MSGQ_PATH_NAME_S, ROBOT_CTRL_MSGQ_ID_S);
    if (msgkey == -1)
    {
        printf("create msgque send failed\r\n");
        return -1;
    }
    printf("create msgque send success \r\n");
    g_robot_msg_pid_send = msgget(msgkey, IPC_CREAT | 0666);
    if (g_robot_msg_pid_send == -1)
    {
        printf("get msg que send failed\r\n");
        return -1;
    }  
    printf("msg-send key=[%d] pid = [%d]\r\n",msgkey,  g_robot_msg_pid_send );
    return g_robot_msg_pid_send;
}

int create_host_msgrecv()
{
    int msglen; 
    key_t msgkey;
    msgkey = ftok(ROBOT_CTRL_MSGQ_PATH_NAME_R, ROBOT_CTRL_MSGQ_ID_R);
    if (msgkey == -1)
    {
        printf("create msgque failed\r\n");
        return 0;
    }
    printf("create msgque success\r\n");
    g_robot_msg_pid_recv = msgget(msgkey, IPC_CREAT | 0666);
    if (g_robot_msg_pid_recv == -1)
    {
        printf("get msg que failed\r\n");
        return 0;
    }  
    printf("msg-pub key=[%d] pid = [%d]\r\n",msgkey,  g_robot_msg_pid_recv );
    return g_robot_msg_pid_recv;
}

int send_msg_to_host(const char *buf){
    dreame_msgbuf_send msg_send;
    dreame_msgbuf_send msg_send_empty;
    msg_send.msgtype = ROBOT_CTRL_MSGID_CLIENT_TO_SERVER_R;
    msg_send_empty.msgtype = ROBOT_CTRL_MSGID_CLIENT_TO_SERVER_R;
    strncpy(msg_send.content, buf, ROBOT_CTRL_MSGQ_BUF_SIZE_S);
    if(msgsnd(g_robot_msg_pid_send, &msg_send, strlen(msg_send.content)+1, IPC_NOWAIT) == -1)
    {
        printf("send json to host failed [%d] [%s]\r\n",g_robot_msg_pid_send, strerror(errno));
        while(1){
            if(msgrcv(g_robot_msg_pid_send,&msg_send_empty,ROBOT_CTRL_MSGQ_BUF_SIZE_S, ROBOT_CTRL_MSGID_CLIENT_TO_SERVER_R,IPC_NOWAIT) == -1){
                break;
            }
        }
        printf("clean msg\r\n");
        return 0;
    }else{
        printf("send json to host success\r\n");
        return 1;
    }   
}





