/********************************************************
 * 参考tinyhttpd实现的一个简单的OTA升级服务器，用于给MCU传输升级包使用
 *
 * *********************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
//#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: OTA_server/0.1.0\r\n"
char ota_filename[32] = {0};
void my_accept_request(int client);
int my_startup(short port);
int isConnection(int fd);
void download(int fd, char *filename);
void error_die(const char *sc);

void error_die(const char *sc)
{
    perror(sc);
}

int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return (i);
}

void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

int my_startup(short port)
{
    int httpd = 0;
    struct sockaddr_in name;
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
//  name.sin_port = htons(*port);
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");

    // if (*port == 0) /* if dynamically allocating a port */
    // {
    //     int namelen = sizeof(name);
    //     if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
    //         error_die("getsockname");
    //     *port = ntohs(name.sin_port);
    // }

    if (listen(httpd, 2) < 0)
        error_die("listen");
    return (httpd);
}

void my_accept_request(int client)
{
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    char *query_string = NULL;
    numchars = get_line(client, buf, sizeof(buf));
    i = 0;
    j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';
    printf("url=[%s]\r\n", buf);
    if (strcasecmp(method, "GET"))
    {
        unimplemented(client);
        return;
    }
    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;

    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    sprintf(path, "%s", url);

    printf("path=[%s]\r\n", path);

    if (stat(path, &st) == -1)
    {
        while ((numchars > 0) && strcmp("\n", buf)) 
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {
        printf("存在OTA升级文件\r\n");
        printf("send file path = [%s]\r\n", path);
        download(client, path);
    }
    close(client);
}

#define BUFSIZE 1024
#define FILELEN 4194304
void download(int fd, char *filename)
{
    FILE *resoure = NULL;
    char *p = NULL;
    int numchars = 1;
    char buf[BUFSIZE];
    long file_len = 0;

    printf("line=[%d]\r\n", __LINE__);
    buf[0] = 'A';
    buf[1] = '\0';

    while ((numchars > 0) && strcmp("\n", buf))
    {
        numchars = get_line(fd, buf, sizeof(buf));
    }
    printf("line=[%d]\r\n", __LINE__);
    resoure = fopen(filename, "r");
    if (resoure == NULL)
    {
        printf("file open failed\r\n");
        fclose(resoure);
        return;
    }
    printf("line=[%d]\r\n", __LINE__);
    fseek(resoure, 0, SEEK_END);
    file_len = ftell(resoure);
    fclose(resoure);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Connection:keep-alive\r\nContent-Type: application/octet-stream\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: %ld\r\n", file_len);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(fd, buf, strlen(buf), 0);
    printf("line=[%d]\r\n", __LINE__);

    long seek = 0;
    long wait_len = file_len - seek;
    long send_len = 0;
    double mb = 0;
    if (wait_len > FILELEN)
        send_len = FILELEN;
    else
        send_len = wait_len;
    p = (char *)malloc(sizeof(char) * send_len);
    int sendnum = 0;
    while (1)
    {
        resoure = fopen(filename, "r");
        if (resoure == NULL)
        {
            printf("file open false\r\n");
            fclose(p);
            return;
        }
        if (!fread(p, sizeof(unsigned char), send_len, resoure))
            break;
        fclose(resoure);
        if (isConnection(fd) == -1)
        {
            break;
        }
        send(fd, p, send_len, 0);
        printf("sendnum:%d-\n", sendnum);
        seek = seek + send_len;
        wait_len = file_len - seek;
        if (wait_len > FILELEN)
            send_len = FILELEN;
        else
            send_len = wait_len;
        mb = (double)((double)seek / (double)1048576);
        printf("seek=%ld===>%lfMB\r\n", seek, mb);
    }
    free(p);
    p = NULL;
    printf("Doeload end\n");
}

int isConnection(int fd)
{
    struct tcp_info info;
    int len = sizeof(info);
    getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
    if (info.tcpi_state == TCP_ESTABLISHED)
        return 0;
    else
        return -1;
}

#if 0
int main(void)
{
    int server_sock = -1;
    int client_sock = -1;
    short port = 0;
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);
    server_sock = my_startup(&port);
    if (server_sock < 0)
    {
        printf("httpd server create failed\r\n");
    }
    printf("httpd running on port %d\r\n", port);
    get_otaname(ota_filename);
    printf("待升级文件名--[%s]\r\n",ota_filename);
    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
        if (client_sock == -1)
        {
            error_die("accept");
        }
        my_accept_request(client_sock);
        if (client_sock > 0)
        {
            printf("client port = [%d]\r\n", client_sock);
        }
    }
    close(server_sock);
    return 0;
}

#endif
