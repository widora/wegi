/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ether.h>   /* ETH_P_ALL */
#include <unistd.h>
#include <netpacket/packet.h> /* sockaddr_ll */
#include <netdb.h> /* gethostbyname() */
#include "egi_timer.h"
#include "egi_log.h"
#include "egi_iwinfo.h"


#ifndef IW_NAME
#define IW_NAME "apcli0"
#endif


/*------------------------------------------
Get wifi strength value

*rssi:	strength value

Return
	0	OK
	<0	Fails
-------------------------------------------*/
int iw_get_rssi(int *rssi)
{
	int sockfd;
	struct iw_statistics stats;
	struct iwreq req;

	memset(&stats,0,sizeof(struct iw_statistics));
	memset(&req,0,sizeof(struct iwreq));
	sprintf(req.ifr_name,"apcli0");
	req.u.data.pointer=&stats;
	req.u.data.length=sizeof(struct iw_statistics);
	#ifdef CLEAR_UPDATED
	req.u.data.flags=1;
	#endif

	if((sockfd=socket(AF_INET,SOCK_DGRAM,0))==-1)
	{
		//perror("Could not create simple datagram socket");
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to create SOCK_DGRAM socket: %s \n",
								   __func__, strerror(errno));
		return -1;
	}
	if(ioctl(sockfd,SIOCGIWSTATS,&req)==-1)
	{
		//perror("Error performing SIOCGIWSTATS");
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call ioctl(socket,SIOCGIWSTATS,...): %s \n",
								   __func__, strerror(errno));
		close(sockfd);
		return -2;
	}

	if(rssi != NULL)
		*rssi=(int)(signed char)stats.qual.level;

	close(sockfd);
	return 0;
}

/*-----------------------------------------------------------
A rough method to get current wifi speed

ws:	speed in (bytes/s)

Note:
1. If there is no actual income stream, recvfrom()
   will take more time than expected.
2. It will cause caller to exit sometimes,when you start to
   run a app which will increase income stream from nothing.

Return
	0	OK
	<0	Fails
----------------------------------------------------------*/
int  iw_get_speed(int *ws)
{
	int 			sock;
	struct ifreq 		ifstruct;
	struct sockaddr_ll	sll;
	struct sockaddr_in	addr;
	char buf[2048];
	int ret;
	int count;
	int len;
	len=sizeof(addr);

	struct timeval timeout;
	timeout.tv_sec=IW_TRAFFIC_SAMPLE_SEC;
	timeout.tv_usec=0;

	/* reset ws first */
	*ws=0;
	count=0;

	/* create a socket */
	if( (sock=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL))) == -1 )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to create SOCK_RAW socket: %s \n",
								   __func__, strerror(errno));
		return -1;
	}
	sll.sll_family=PF_PACKET;
	sll.sll_protocol=htons(ETH_P_ALL);
	strcpy(ifstruct.ifr_name,"apcli0");
	if(ioctl(sock,SIOCGIFINDEX,&ifstruct)==-1)
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call ioctl(sock, SIOCGIFINDEX, ...): %s \n",
								   __func__, strerror(errno));
		close(sock);
		return -2;
	}
	sll.sll_ifindex=ifstruct.ifr_ifindex;

	/* bind socket with address */
	ret=bind(sock,(struct sockaddr*)&sll, sizeof(struct sockaddr_ll));
	if(ret !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call bind(): %s \n", __func__, strerror(errno));
		close(sock);
		return -3;
	}

	/* set timeout option */
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

	/* set port_reuse option */
	int optval=1;/*YES*/
	ret=setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if(ret !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to call setsockopt(): %s \n", __func__, strerror(errno));
		/* go on anyway */
	}

	printf("iw_get_speed:----------- start recvfrom() and tm_pulse counting ------------\n");
	while(1)
	{
		/* use pulse timer [0] */
		if(tm_pulseus(IW_TRAFFIC_SAMPLE_SEC*1000000, 0)) {
			EGI_PLOG(LOGLV_INFO,"egi_iwinfo: tm pulse OK!\n");
			break;
		}

		// EGI_PLOG(LOGLV_INFO,"%s: ....... start calling recvfrom( ) .......\n", __func__);
		ret=recvfrom(sock,(char *)buf, sizeof(buf), 0, (struct sockaddr *)&addr, (socklen_t *)&len);
		if(ret<=0) {
			if( ret == EWOULDBLOCK ) {
				EGI_PLOG(LOGLV_CRITICAL,"%s: Fail to call recvfrom() ret=EWOULDBLOCK. \n"
												 , __func__);
				continue;
			}
			else if (ret==EAGAIN) {
				EGI_PLOG(LOGLV_CRITICAL,"%s: Fail to call recvfrom() ret=EAGAIN. \n",__func__);
				continue;
			}
			else {
				EGI_PLOG(LOGLV_ERROR,"%s: Fail to call recvfrom()... ret=%d:%s \n",
									__func__, ret, strerror(errno));
				close(sock);
				return -4;
			}
		}
		//EGI_PLOG(LOGLV_INFO,"%s: ....... recvfrom( ) get %d bytes .......\n", __func__, ret);

/* Debug results....
				-----  BUG  -----
......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 486 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 294 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 294 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 294 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 294 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 118 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 294 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-05-18 16:57:28] [LOGLV_INFO] iw_get_speed: ....... recvfrom( ) get 486 bytes .......
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
.....
				-----  BUG  -----

egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-06-30 11:26:37] [LOGLV_INFO] [2019-06-30 11:26:37] heart_beat msg is sent out.
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-06-30 11:26:37] [LOGLV_INFO] Message from the server: {"M":"checked"}

egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-06-30 11:27:07] [LOGLV_INFO] [2019-06-30 11:27:07] heart_beat msg is sent out.
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-06-30 11:27:07] [LOGLV_INFO] Message from the server: {"M":"checked"}

egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-06-30 11:27:37] [LOGLV_INFO] [2019-06-30 11:27:37] heart_beat msg is sent out.
egi_push_log(): log_buff[] is full, fail to push strlog, unlock mutex and return...
EGI_Logger: [2019-06-30 11:27:37] [LOGLV_INFO] Message from the server: {"M":"checked"}
......
				-----  NORMAL  ---

GI_Logger: [2019-06-30 13:06:34] [LOGLV_INFO] ---------  trap into iw_get_speed( )  -------- >>>>>
iw_get_speed:----------- start recvfrom() and tm_pulse counting ------------
EGI_Logger: [2019-06-30 13:06:40] [LOGLV_INFO] egi_iwinfo: tm pulse OK!
EGI_Logger: [2019-06-30 13:06:40] [LOGLV_INFO] ---------  get out of iw_get_speed( )  ---------- <<<<<
EGI_Logger: [2019-06-30 13:06:40] [LOGLV_INFO] xxxxxxxxxx   maxrss=2208, ixrss=0, idrss=0, isrss=0    xxxxxxxxx
EGI_Logger: [2019-06-30 13:06:40] [LOGLV_INFO] raw data for json_data: load=1.700000, ws=21305.000000, maxrss=2208.000000. 
EGI_Logger: [2019-06-30 13:06:40] [LOGLV_CRITICAL] Finish creating update_json: 
{
  "M":"update",
  "ID":"421",
  "V":{
    "546":"1.70",
    "961":"21305.00",
    "430":"2208.00"
  }
}


*/


		count+=ret;
	}

	if(ws !=NULL)
		*ws=count/IW_TRAFFIC_SAMPLE_SEC;

	close(sock);
	return 0;
}



/*--------------------------------------------------------------
Send request and get reply by HTTP protocol.

Note: the caller must ensure enough space for msgsend and msgrecv

host:	  remote host
request:  request string
reply:    replay string from the host

Return
	0	OK
	<0	Fails
---------------------------------------------------------------*/
int  iw_http_request(char *host, char *request, char *reply)
{
	int ret;
	int port=80;
	int sock;
	struct sockaddr_in host_addr;
	struct hostent * remoteHost;
	char strmsg[256];

	if( host==NULL || request==NULL || reply==NULL)
		return -1;

	if( (remoteHost=gethostbyname(host)) == NULL )
	{
		printf("%s: Fail to get host by name %s.\n",__func__,host);
		return -2;
	}

	bzero(&host_addr,sizeof(host_addr));
	host_addr.sin_family=AF_INET;
	host_addr.sin_port=htons(port);
	host_addr.sin_addr.s_addr=((struct in_addr *)(remoteHost->h_addr))->s_addr;

	memset(strmsg,0,sizeof(strmsg));
	strcat(strmsg,"GET ");
	strcat(strmsg,request);
	strcat(strmsg," HTTP/1.1\r\n");
	//SVR NOT SUPPORT strcat(strmsg," Content-Type: charset=uft-8\r\n");
	strcat(strmsg,"User-Agent: Mozilla/5.0 \r\n");
	strcat(strmsg,"HOST: ");

	strcat(strmsg,host);
	strcat(strmsg,"\r\n\r\n");
	printf("%s REQUEST string: %s\n",__func__,strmsg);

	sock=socket(AF_INET,SOCK_STREAM,0);
	if(sock<0) {
		printf("%s() socket error: %s \n",__func__, strerror(errno));
		return -3;
	}

	printf("connect to host... \n");
	ret=connect(sock, (struct sockaddr *)&host_addr, sizeof(host_addr));
	if(ret<0) {
		printf("%s() connect error: %s \n",__func__, strerror(errno));
		return -4;
	}

	printf("send strmsg to host...\n");
	ret=send(sock,strmsg,strlen(strmsg),0);
	if(ret<=0) {
		printf("%s() send error: %s \n",__func__, strerror(errno));
		return -5;
	}

	printf("receive reply from host...\n");
	ret=recv(sock,reply,256-1,0);
	if(ret<=0) {
		printf("%s() recv error: %s \n",__func__, strerror(errno));
		return -6;
	}

	reply[ret]='\0'; /* string end token */

	close(sock);

	return 0;
}
