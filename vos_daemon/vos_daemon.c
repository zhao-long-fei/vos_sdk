#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/stat.h>

static int get_pid(const char* process_name);
static int get_usb_dev();
static int search_usb();
static int get_filesize( const char *path );
static char* read_text_file( const char * filepath );
#define USB_INFO_FILE "/data/speech_auth/usb"
#define TH_USB_1 "03fe:0301"
#define TH_USB_2 "5448:152e"
#define PROCESS_NAME "vos_cleaner"

static int get_usb_dev()
{
    unlink(USB_INFO_FILE);
    system("lsusb >/data/speech_auth/usb");
    return 0;
}

/***************************************************************
 * 守护进程示例，宗旨是在有太行USB节点，且vos_sdk程序没有启动时候
 * 将vos_sdk程序拉起来。
 * *************************************************************/

/*
   return 1: 太行有usb节点 无固件
   return 2: 太行有USB节点 有固件
   return 0：无太行USB节点
   return -1: 节点查询失败
*/

static int search_usb()
{
    char *usb_info = NULL;
    int file_len = 0;
    if(access(USB_INFO_FILE, F_OK) != -1){
        file_len = get_filesize(USB_INFO_FILE);
        usb_info = (char *)malloc(file_len);
        usb_info = read_text_file(USB_INFO_FILE);
        printf("usb_info = [%s]\r\n",usb_info);
        if(usb_info && strstr(usb_info, TH_USB_1)){
            if(usb_info) free(usb_info);
            return 1;
        }else if(usb_info && strstr(usb_info, TH_USB_2)){
            if(usb_info) free(usb_info);
            return 2;
        }else{
            if(usb_info) free(usb_info);
            return 0;
        }
    }else{
        return -1;
    }
}

static int get_filesize( const char *path )
{
	struct stat	fs;
	int	rc = -1;
	if ( stat(path, &fs) == 0 )
		rc = fs.st_size;
	return rc;
}

static char* read_text_file( const char * filepath )
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

static int get_pid(const char* process_name)
{
	char cmd[512];
	sprintf(cmd, "pgrep %s", process_name);	
	FILE *pstr = popen(cmd,"r");		
	if(pstr == NULL){
		return 0;	
	}
	char buff[512];
	memset(buff, 0, sizeof(buff));
	if(NULL == fgets(buff, 512, pstr)){
		return 0;
	}
	return atoi(buff);
}

static int get_pid_sys(const char* process_name){
    unlink("/data/speech_auth/process");
    char cmd[128] = {0};
    sprintf(cmd, "pgrep %s >/data/speech_auth/process", process_name);
    system(cmd);
    int len = get_filesize("/data/speech_auth/process");
    if (len > 0)
    {
        char *buf = NULL;
    //    buf = (char *)malloc(len + 1);
        buf = read_text_file("/data/speech_auth/process");
        int pid = atoi(buf);
        if (buf)
            free(buf);
        return pid;
    }
    return 0;
}

static int ps_last = 1;
static int ps_this = 1;
static int ps = 1;

int main()
{
    int usb_ret = 0;
    int pid = 0;
    if(access("/data/speech_auth", F_OK) == -1){
        printf("no /data/speech_auth, create it\r\n");
        system("mkdir /data/speech_auth");
    }else{
    usleep(10000);
    unlink("/data/speech_auth/process");
    usleep(10000);
    unlink("/data/speech_auth/usb");
    usleep(10000);
    }
    while(1){
        get_usb_dev();
        usb_ret = search_usb();
        ps_last = ps_this;
        printf("usb_ret = [%d]\r\n",usb_ret);
        if(usb_ret == 1){
            printf("存在usb节点，无固件\r\n");
            ps_this = 1;
        }else if(usb_ret == 2){
            printf("存在usb节点，有固件\r\n");
            ps_this = 2;
        }else if(usb_ret == 0){
            printf("无USB节点\r\n");
            ps_this = 0;
        }
        if( (0==ps_last) && ((1==ps_this)||(2==ps_this)) ){
            ps = 1;  // 从无节点到有节点
        }else if((2==ps_last) && (1==ps_this)){
            ps = 2; // 从有节点有固件变成有节点无固件
        }else{
            ps = 0;
        }

        pid = get_pid_sys(PROCESS_NAME);
        printf("pid = [%d]\r\n",pid);
        if(0 == pid){
            printf("当前vos_cleaner程序没有运行\r\n");
            if(1 == usb_ret || 2 == usb_ret){
                printf("监测到存在USB节点，程序未运行，拉起程序\r\n");
              system("sh /usr/bin/speech_ctrl.sh");
            //  system("./vos_cleaner ");
            //  system("sh /data/speech_ctrl.sh");
            }
        }else{           
            if((1 == ps) || (2 == ps)){
                printf("关闭程序\r\n");
                system("killall -9 vos_cleaner");
                usleep(1000000);
                printf("重启程序\r\n");
                system("sh /usr/bin/speech_ctrl.sh");
            //  system("./vos_cleaner ");
            //  system("sh /data/speech_ctrl.sh");
            }
        }
       usleep(3000000);
    }
}