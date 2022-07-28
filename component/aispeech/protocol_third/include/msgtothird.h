#ifndef MSGTOTHIRD_H
#define MSGTOTHIRD_H


#define ROBOT_CTRL_MSGQ_BUF_SIZE_R 512
#define ROBOT_CTRL_MSGQ_ID_R 44
#define ROBOT_CTRL_MSGQ_PATH_NAME_R "/data"
#define ROBOT_CTRL_MSGID_CLIENT_TO_SERVER_R 1


#define ROBOT_CTRL_MSGQ_BUF_SIZE_S 512
#define ROBOT_CTRL_MSGQ_ID_S 32
#define ROBOT_CTRL_MSGQ_PATH_NAME_S  "/tmp"
#define ROBOT_CTRL_MSGQ_SERVER_MSG_S 1

typedef struct mymsgpub
{
    long msgtype;
    char content[ROBOT_CTRL_MSGQ_BUF_SIZE_R];
}dreame_msgbuf_recv;

typedef struct mymsgsub
{
    long msgtype;
    char content[ROBOT_CTRL_MSGQ_BUF_SIZE_S];
}dreame_msgbuf_send;


int release_msg();
int send_msg_to_host(const char *buf);
int create_dreame_msgrecv();
int create_dreame_msgsend();

#endif