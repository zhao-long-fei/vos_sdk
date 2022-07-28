#ifndef TOOLSFILE_H
#define TOOLSFILE_H

extern int get_mac_addr(const char *ifname, unsigned char *ifaddr);
extern int is_same_word(const char *s1, const char *s2);
extern char*file_read( const char * filepath);
extern int get_filesize( const char *path );
extern char *config_file_read(char *file, int *len);

#endif