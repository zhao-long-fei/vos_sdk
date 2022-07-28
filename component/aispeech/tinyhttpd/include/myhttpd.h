#ifndef MYHTTPD_H
#define MYHTTPD_H

void my_accept_request(int client);
void my_bad_request(int);
int my_startup(short *port);
void my_unimplemented(int);
int isConnection(int fd);
void download(int fd, char *filename);
void error_die(const char *sc);

#endif