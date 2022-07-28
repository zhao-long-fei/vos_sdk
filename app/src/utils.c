#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

/*****************************************************
 * 获取指定网卡的MAC地址
 * ***************************************************/
int get_mac_addr(const char *ifname, unsigned char *ifaddr)
{
	int rc = -1, skfd;
	struct ifreq ifr;
	if (ifname)
		strcpy(ifr.ifr_name, ifname);
    else
        return -1;
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		if (ioctl(skfd, SIOCGIFHWADDR, &ifr) != -1)
		{
			sprintf(ifaddr,"%02X%02X%02X%02X%02X%02X",
			(unsigned char)ifr.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_hwaddr.sa_data[5]
			);
			rc = 0;	
		}
		close(skfd);
	}
	return rc;
}

int is_same_word(const char *s1, const char *s2) {
    while (s1[0] && s2[0]) {
        if (isspace(s1[0])) {
            s1++;
            continue;
        }
        if (isspace(s2[0])) {
            s2++;
            continue;
        }
        if (*s1 != *s2) return 0;
        s1++;
        s2++;
    }
    if (strlen(s1)) {
        while (s1[0]) {
            if (!isspace(s1[0])) return 0;
            s1++;
        }
    }
    if (strlen(s2)) {
        while (s2[0]) {
            if (!isspace(s2[0])) return 0;
            s2++;
        }
    }
    return 1;
}

char* file_read( const char * filepath )
{
	char* text = NULL;
	FILE* fp = NULL;

	if( (fp = fopen(filepath,"rb")) != NULL ) {
		int filelen, readlen;
		fseek(fp, 0L, SEEK_END);
		filelen = ftell(fp);
		if( (filelen>0) && ((text = malloc(sizeof(char)*(filelen+1)))!=NULL)) {
			fseek(fp, 0L, SEEK_SET);
			if( (readlen = fread(text, sizeof(char), filelen, fp)) > 0 ) {
				text[ readlen ] = '\0';
			} else {
				free(text);
				text = NULL;
			}
		}
		fclose(fp);
	}
	return text;
}

int get_filesize( const char *path )
{
	struct stat	fs;
	int	rc = -1;
	if ( stat(path, &fs) == 0 )
		rc = fs.st_size;
	return rc;
}

char *config_file_read(char *file, int *len) {
    FILE *fd = fopen(file, "r");
    int size;
    char *data = NULL;
    if (fd) {
        fseek(fd, 0, SEEK_END);
        size = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        data = (char *)malloc(size + 1);
        if (data) {
            fread(data, 1, size, fd);
            if (len) *len = size;
        }
        fclose(fd);
    }
    return data;
}