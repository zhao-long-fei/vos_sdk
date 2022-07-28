#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <errno.h>
//#define _USE_DNS
#define MAX_WAIT_TIME   1
#define MAX_NO_PACKETS  1
#define ICMP_HEADSIZE 8 
#define PACKET_SIZE     4096
static struct timeval tvsend,tvrecv;	
static struct sockaddr_in dest_addr,recv_addr;
static int sockfd;
static pid_t pid;
static char sendpacket[PACKET_SIZE];
static char recvpacket[PACKET_SIZE];
 
//函数定义
static void timeout(int signo);
static unsigned short cal_chksum(unsigned short *addr,int len);
static int pack(int pkt_no,char *sendpacket);
static int send_packet(int pkt_no,char *sendpacket);
static int recv_packet(int pkt_no,char *recvpacket);
static int unpack(int cur_seq,char *buf,int len);
static void tv_sub(struct timeval *out,struct timeval *in);
static void _CloseSocket();

static int ping(char *ipaddr, int timeout);


int getIpFormDomainName(char* domain_name, char* ip, unsigned char ipLen)
{
	struct hostent* host = NULL;
	host = gethostbyname(domain_name);

	if(NULL == host)
	{
		return -1;
	}

	inet_ntop(host->h_addrtype, host->h_addr, ip, ipLen);

	return 0;
}


int net_status_check()
{
    int net_res = 0;
    int net_ok_flag = 0;
    int i;
	char hostname[32];

	if (getIpFormDomainName("www.baidu.com", hostname, 31)) {
		//printf("get ip of baidu fail\r\n");
		return 0;
	}
 	
    for(i=0; i<5; i++)
    {
        //net_res = NetIsOk();
        #if 1
        if(0 == ping(hostname, 2000))
        {
			net_res = 1;
		}
		#endif
        if(1 == net_res) net_ok_flag++;
    }
    if(net_ok_flag >= 1) return 1;
    return 0;
}

int NetIsOk()
{     
	double rtt;
	struct hostent *host;
	struct protoent *protocol;
	int i,recv_status;
 
#ifdef _USE_DNS //如果定义该宏，则可以使用域名进行判断网络连接，例如www.baidu.com
	/* 设置目的地址信息 */
	char hostname[32];
	sprintf(hostname,"%s","114.114.114.114");
    memset(&dest_addr,0,sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;	
 
	if((host=gethostbyname(hostname))==NULL) 
	{
	//	printf("[NetStatus]  error : Can't get serverhost info!\n");
		return -1;
	}
 
	bcopy((char*)host->h_addr,(char*)&dest_addr.sin_addr,host->h_length);
#else //如果不使用域名，则只能用ip地址直接发送icmp包，例如谷歌的地址：8.8.8.8
	dest_addr.sin_addr.s_addr = inet_addr("114.114.114.114");
#endif
	
 
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) 
	{	/* 创建原始ICMP套接字 */
	//	printf("[NetStatus]  error : socket\r\n");
		return -1;
	}
 
	int iFlag;
	if(iFlag = fcntl(sockfd,F_GETFL,0)<0)
	{
	//	printf("[NetStatus]  error : fcntl(sockfd,F_GETFL,0)\r\n");
		_CloseSocket();
		return -1;
	}
	iFlag |= O_NONBLOCK;
	if(iFlag = fcntl(sockfd,F_SETFL,iFlag)<0)
	{
	//	printf("[NetStatus]  error : fcntl(sockfd,F_SETFL,iFlag )\r\n");
		_CloseSocket();
		return -1;
	}
 
	pid=getpid();
	for(i=0;i<MAX_NO_PACKETS;i++)
	{		
	
		if(send_packet(i,sendpacket)<0)
		{
			//printf("[NetStatus]  error : send_packet\r\n");
			_CloseSocket();
			return -1;
		}	
 
		if(recv_packet(i,recvpacket)>0)
		{
			_CloseSocket();
           // printf("[NetStatus]  success\r\n");
			return 1;
		}
	} 
	_CloseSocket();     	
	return -1;
}
 
static int send_packet(int pkt_no,char *sendpacket)
{    
	int packetsize;       
	packetsize=pack(pkt_no,sendpacket); 
	gettimeofday(&tvsend,NULL);    
	if(sendto(sockfd,sendpacket,packetsize,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr) )<0)
	{      
		//printf("[NetStatus]  error : sendto error\r\n");
		return -1;
	}
	return 1;
}
 
 
static int pack(int pkt_no,char*sendpacket)
{       
	int i,packsize;
	struct icmp *icmp;
	struct timeval *tval;
	icmp=(struct icmp*)sendpacket;
	icmp->icmp_type=ICMP_ECHO;   //设置类型为ICMP请求报文
	icmp->icmp_code=0;
	icmp->icmp_cksum=0;
	icmp->icmp_seq=pkt_no;
	icmp->icmp_id=pid;			//设置当前进程ID为ICMP标示符
	packsize=ICMP_HEADSIZE+sizeof(struct timeval);
	tval= (struct timeval *)icmp->icmp_data;
	gettimeofday(tval,NULL);
	icmp->icmp_cksum=cal_chksum( (unsigned short *)icmp,packsize); 
	return packsize;
}
 
 
static unsigned short cal_chksum(unsigned short *addr,int len)
{       
	int nleft=len;
	int sum=0;
	unsigned short *w=addr;
	unsigned short answer=0;
	while(nleft>1)		//把ICMP报头二进制数据以2字节为单位累加起来
	{       
		sum+=*w++;
		nleft-=2;
	}
	if( nleft==1)		//若ICMP报头为奇数个字节,会剩下最后一字节.把最后一个字节视为一个2字节数据的高字节,这个2字节数据的低字节为0,继续累加
	{
		*(unsigned char *)(&answer)=*(unsigned char *)w;
		sum+=answer;
	}
	sum=(sum>>16)+(sum&0xffff);
	sum+=(sum>>16);
	answer=~sum;
	return answer;
}
 
 
static int recv_packet(int pkt_no,char *recvpacket)
{       	
	int n,fromlen;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(sockfd,&rfds);
	signal(SIGALRM,timeout);
	fromlen=sizeof(recv_addr);
	alarm(MAX_WAIT_TIME);
	struct timeval tv; 
	int ret;
	tv.tv_sec = 0;
	tv.tv_usec = 300000;
	while(1)
	{
		usleep(10000);
		ret = select(sockfd+1, &rfds, NULL, NULL, &tv);
		if(ret == 0){
			return -1;
		}else if(ret == -1)
		{
			return -1;
		}else{
			if (FD_ISSET(sockfd,&rfds))
			{  
				if( (n=recvfrom(sockfd,recvpacket,PACKET_SIZE,0,(struct sockaddr *)&recv_addr,&fromlen)) <0)
				{   
				if(errno==EINTR)
					return -1;
				}
			}
		}
		gettimeofday(&tvrecv,NULL); 
		if(unpack(pkt_no,recvpacket,n)==-1)
			continue;
		return 1;
	}
}
 
static int unpack(int cur_seq,char *buf,int len)
{    
	int iphdrlen;
	struct ip *ip;
	struct icmp *icmp;
	ip=(struct ip *)buf;
	iphdrlen=ip->ip_hl<<2;		//求ip报头长度,即ip报头的长度标志乘4
	icmp=(struct icmp *)(buf+iphdrlen);		//越过ip报头,指向ICMP报头
	len-=iphdrlen;		//ICMP报头及ICMP数据报的总长度
	if( len<8)
		return -1;       
	if( (icmp->icmp_type==ICMP_ECHOREPLY) && (icmp->icmp_id==pid) && (icmp->icmp_seq==cur_seq))
		return 0;	
	else return -1;
}
 
 
static void timeout(int signo)
{
//	printf("Request Timed Out\n");
}
 
static void tv_sub(struct timeval *out,struct timeval *in)
{       
	if( (out->tv_usec-=in->tv_usec)<0)
	{       
		--out->tv_sec;
		out->tv_usec+=1000000;
	}
	out->tv_sec-=in->tv_sec;
}
 
static void _CloseSocket()
{
	close(sockfd);
	sockfd = 0;
}

static int ping(char *ipaddr, int timeout)
{
	struct timeval timeo;
	int sockfd;
	struct sockaddr_in addr;
	struct sockaddr_in from;
	struct ip *iph;
	struct icmp *icmp;

	char sendpacket[256];
	char recvpacket[256];

	int n;
	pid_t pid;
	int maxfds = 0;
	fd_set readfds;
	int fromlen = 0;

	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ipaddr);

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		printf("ip:%s,socket error.\n", ipaddr);
		return -1;
	}

	timeo.tv_sec = timeout / 1000;
	timeo.tv_usec = timeout % 1000;

	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)) == -1)
	{
		printf("ip:%s,setsockopt error.\n", ipaddr);
		close(sockfd);
		return -1;
	}

	memset(sendpacket, 0, sizeof(sendpacket));

	// 取得PID，作为Ping的Sequence ID
	pid = getpid();
	int packsize;
	icmp=(struct icmp*)sendpacket;
	icmp->icmp_type=ICMP_ECHO;
	icmp->icmp_code=0;
	icmp->icmp_cksum=0;
	icmp->icmp_seq=0;
	icmp->icmp_id=pid;
	packsize=8+56;

	icmp->icmp_cksum = cal_chksum((unsigned short *)icmp,packsize);

	n = sendto(sockfd, (char *)&sendpacket, packsize, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (n < 1)
	{
		//printf("ip:%s,sendto error.\n", ipaddr);
		close(sockfd);
		return -1;
	}

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		maxfds = sockfd + 1;
		n = select(maxfds, &readfds, NULL, NULL, &timeo);
		if (n <= 0)
		{
			//printf("ip:%s,Time out error.\n", ipaddr);
			close(sockfd);
			return -1;
		}

		memset(recvpacket, 0, sizeof(recvpacket));
		fromlen = sizeof(from);

		n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
		if (n < 1)
		{
			close(sockfd);
			return -1;
		}

		// 判断是否是自己Ping的回复
		char *from_ip = (char *)inet_ntoa(from.sin_addr);
		if (strcmp(from_ip, ipaddr) != 0)
		{
			//printf("ip:%s, from_ip:%s ip wrong\n", ipaddr, from_ip);
			continue;
		}

		iph = (struct ip *)recvpacket;
		icmp = (struct icmp *)(recvpacket + (iph->ip_hl<<2));

		// 判断Ping回复包的状态
		if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == pid)
		{
			//printf("ping ip:%s from ip:%s icmp_type:%d,icmp_id:%d successful!!!\n",
			//    ipaddr, from_ip, icmp->icmp_type, icmp->icmp_id);
			break;
		}
		else
		{
			continue;
		}
	}

	close(sockfd);

	return 0;
}

