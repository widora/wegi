/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transfer a file by TCP.

Usage:	test_tcpfile [-hsca:p:]

Example:
	./test_tcpfile -s -p 8765			( As server )
	./test_tcpfile -c -a 192.168.9.9 -p 8765	( As client )

	( Use test_garph.c to show net interface traffic speed )

Note:

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <sys/types.h> /* this header file is not required on Linux */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h> /* inet_ntoa() */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <egi_inet.h>
#include <egi_timer.h>
#include <egi_log.h>
#include <egi_utils.h>

#define SERVER_PORT 5678
#define SERVER_FDIR "/mmc"
#define CLIENT_FDIR "/mmc"

static int packsize=1448; /* packet size */
static int gms;		  /* interval gap delay */
static char fname[EGI_NAME_MAX];

static EGI_TCP_SERV *userv;
static EGI_TCP_CLIT *uclit;
static bool SetTCPServer;

/* --- A private message ---- */
typedef struct file_leads {
	char fname[EGI_NAME_MAX]; /* Request/Send File name */
	int packnums;   /* Total number of packets */
	int packsize;	/* Size of each packet  */
	int tailsize;    /* Size of the last packet, applys only if tailsize>0 */
} FILE_LEADS;

/* Session/Process handler */
void* server_session_process(void *arg);
int client_request_file(EGI_TCP_CLIT *uclit, const char* fname);


enum transmit_mode {
        mode_2Way       =0,  /* Server and Client PingPong mode */
        mode_S2C        =1,  /* Server to client only */
        mode_C2S        =2,  /* Client to server only */
};

static enum transmit_mode trans_mode;
static bool verbose_on;

void show_help(const char* cmd)
{
	printf("Usage: %s [-hscva:p:g:m:] \n", cmd);
       	printf("        -h   help \n");
   	printf("        -s   work as TCP Server\n");
   	printf("        -c   work as TCP Client (default)\n");
	printf("	-v   verbose to print rcvSize\n");
	printf("	-a:  IP address\n");
	printf("	-p:  Port number\n");
	printf("	-g:  gap sleep in ms, default 10ms\n");
	printf("	-m:  Transmit mode, 0-Twoway(default), 1-S2C, 2-C2S \n");
	printf("	-k:  Packet size, default 1448Bytes\n");
}

void inet_sigint_handler(int signum)
{
	static int depth=0;
	int i;

	if(signum!=SIGINT)
		return;


	/* Shut down connection */
	if(SetTCPServer) {

		printf(" WARNING: To close the server will close all sessions!\n");
		depth++;
		if(depth<3) return;

		if(inet_tcp_getState(userv->sockfd)==TCP_LISTEN ) {
		        printf("Catch a SIGINT, start to close userv...!\n");
			shutdown(userv->sockfd,SHUT_RDWR);
			/* Wait close */
			while( inet_tcp_getState(userv->sockfd)!= TCP_CLOSE )
				sleep(1);
		}

		/* TEST .... */
		for(i=0; i<5; i++) {
			inet_tcp_getState(userv->sockfd);
			sleep(1);
		}

		inet_destroy_tcpServer(&userv);
	}
	else {
		if( inet_tcp_getState(uclit->sockfd)==TCP_ESTABLISHED ) {
		        printf("Catch a SIGINT, start to close uclit...!\n");
			shutdown(uclit->sockfd,SHUT_RDWR);
			/* Wait close */
			while( inet_tcp_getState(uclit->sockfd)!= TCP_CLOSE )
				sleep(1);
		}

		/* TEST .... */
		for(i=0; i<5; i++) {
			inet_tcp_getState(uclit->sockfd);
			sleep(1);
		}

		inet_destroy_tcpClient(&uclit);
	}


	/* free client */
	exit(1);
}


/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{
	int ret;
	int opt;
	char *strAddr=NULL;		/* Default NULL, to be decided by system */
	unsigned short int port=0; 	/* Default 0, to be selected by system */


  /* Default signal handlers for TCP processing */
  //inet_default_sigAction();
  if( inet_register_sigAction(SIGPIPE, inet_sigpipe_handler)<0 ) exit(1);
  if( inet_register_sigAction(SIGINT, inet_sigint_handler)<0 ) exit(2);


#if 0 /*========== TEST: INET_MSGDATA ===========*/

EGI_INET_MSGDATA *msgdata=NULL;
EGI_INET_MSGDATA *ptr;
int cnt=0;


//setvbuf(stdout,NULL,_IONBF,0);
while(1) {

  msgdata=inet_msgdata_create(128*1024);
  //printf("MSGDATA: msg[] %d bytes, data[] %d bytes\n", sizeof(*msgdata), inet_msgdata_getDataSize(msgdata));

  ptr=msgdata;
  inet_msgdata_reallocData(&msgdata, 256*1024); /* >=256K will trigger to mem area movement! */
  if(ptr!=msgdata)
	printf("realloc: mem area moved!\n");

  inet_msgdata_free(&msgdata);
  usleep(10000);
  cnt++;

  printf("\r cnt=%d",cnt);
  fflush(stdout);
}

exit(0);
#endif  /* ===== END TEST ===== */


	/* Defaults */
	port=SERVER_PORT;
	SetTCPServer=false;
	gms=10;

	/* Parse input option */
	while( (opt=getopt(argc,argv,"hscva:p:g:m:k:"))!=-1 ) {
		switch(opt) {
			case 'h':
				show_help(argv[0]);
				exit(0);
				break;
			case 's':
				SetTCPServer=true;
				break;
			case 'c':
				SetTCPServer=false;
				break;
			case 'v':
				printf("Set verbose ON.\n");
				verbose_on=true;
				break;
			case 'a':
				strAddr=strdup(optarg);
				printf("Set address='%s'\n",strAddr);
				break;
			case 'p':
				port=atoi(optarg);
				printf("Set port=%d\n",port);
				break;
			case 'g':
				gms=atoi(optarg);
				break;
			case 'm':
				trans_mode=atoi(optarg);
				break;
			case 'k':
				packsize=atoi(optarg);
				packsize=packsize>sizeof(EGI_INETPACK) ? packsize : sizeof(EGI_INETPACK);
				break;
		}
	}

	printf("Set as TCP %s\n", SetTCPServer ? "Server" : "Client");
	printf("Set gms=%dms\n",gms);
	if(SetTCPServer)
		printf("Set test packsize=%d bytes.\n",packsize);
	sleep(1);


  /* Start egi log */
  if(egi_init_log("/mmc/test_tcpfile.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  }
  EGI_PLOG(LOGLV_TEST,"Start logging...");

  /* A. -------- Work as a TCP File Server --------*/
  if( SetTCPServer ) {
	printf("Set as TCP Server.\n");

        /* Create TCP server */
        userv=inet_create_tcpServer(strAddr, port, 4); //8765, 4);
        if(userv==NULL) {
		printf("Fail to create a TCP server!\n");
                exit(1);
	}

        /* Set session handler for each client */
        userv->session_handler=server_session_process;

        /* Process TCP server routine */
        inet_tcpServer_routine(userv);

        /* Free and destroy */
        inet_destroy_tcpServer(&userv);
  }


  /* B. ------- Work as a TCP File Client --------*/
  else {
	printf("Set as TCP Client.\n");

	if( strAddr==NULL || port==0 ) {
		printf("Please provide Server IP address and port number!\n");
		show_help(argv[0]);
		exit(2);
	}

        /* Create TCP client */
        uclit=inet_create_tcpClient(strAddr, port, 4); //"192.168.8.1", 8765, 4);
        if(uclit==NULL)
                exit(1);

	/* Client session process: Request and receive files */
	while(1) {

		printf("Please input file name:");
		fflush(stdout);
		fgets(fname, EGI_NAME_MAX,stdin);
		fname[strlen(fname)-1]='\0';
		if(fname[0]==0)
			break;
		printf("fname:%s\n",fname);

		if( client_request_file(uclit, fname) >0 ) {
			/* >0 EPIPE or peer socket closed */

			inet_tcp_getState(uclit->sockfd);

			/* Reconnect ??... */
			//break;
		}
	}

        /* Free and destroy */
        inet_destroy_tcpClient(&uclit);
  }

	free(strAddr);
	egi_quit_log();
        return 0;
}


/* ------------ A detached thread function ---------------
TCP Server Session Handler

1. It exits as the client exits.
2. Packsize is set/controlled by the server.

One thread for one client!
--------------------------------------------------------*/
void* server_session_process(void *arg)
{
	int ret=0;
	int i;
	EGI_INETPACK *ipack=NULL;	/* Private packet struct */
	FILE_LEADS *FileLeads=NULL;	/* A pointer to ipack header */

	EGI_FILEMMAP *fmap=NULL;
	char fpath[EGI_PATH_MAX];

        EGI_TCP_SERV_SESSION *session=(EGI_TCP_SERV_SESSION *)arg;
        if(session==NULL)
		return (void *)-1;
	struct sockaddr_in *addrCLIT=&session->addrCLIT;
	int sessionID=session->sessionID;
        int sfd=session->csFD;  /* Session FD */

        /* Detach thread */
        if(pthread_detach(pthread_self())!=0) {
                printf("Session_%d: Fail to detach thread for TCP session with client '%s:%d'.\n",
                                                session->sessionID, inet_ntoa(addrCLIT->sin_addr), ntohs(addrCLIT->sin_port) );
                // go on...
        }


while(1) {  ////////////////////////////     LOOP TEST     ///////////////////////////////

	/* Create ipack, make FILE_LEADS Header ipack. */
	ipack=inet_ipacket_create(sizeof(FILE_LEADS), 0);     /* Only headsize, without privdata */
	if(ipack==NULL)
		return (void *)-3;

printf("Session_%d: Wait for request...\n", sessionID );
	/* 1. Receive request from client */
	if( inet_tcp_recv(sfd, (void *)ipack, ipack->packsize)!=0 ) {
		ret=-4;
		goto END_FUNC;
	}

	/* 2. Check request FILE_LEADS */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);

	/* Search file in DIR */
	bzero(fpath,sizeof(fpath));
	snprintf(fpath,EGI_PATH_MAX-1,"%s/%s",SERVER_FDIR,FileLeads->fname);
	printf("Session_%d: Client '%s:%d' requet file:%s\n", sessionID, inet_ntoa(addrCLIT->sin_addr), ntohs(addrCLIT->sin_port), fpath);
	if( access(fpath, R_OK)!=0 ) {
		printf("Access Err'%s'\n", strerror(errno));
		/* Reply with empty FileLeads */
		bzero(FileLeads, sizeof(FILE_LEADS));
		inet_tcp_send(sfd, (void *)ipack, ipack->packsize);
		inet_ipacket_free(&ipack);
		continue; /* continue to ---> listen request! */
	}

	/* Mmap file */
	fmap=egi_fmap_create(fpath);
	if(fmap==NULL)
		pthread_exit(NULL);

	/* 3. Nest FileLeads into ipack for reply */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);
	//bzero(FileLeads->fname, EGI_NAME_MAX);
	snprintf(FileLeads->fname, EGI_NAME_MAX-1, "%s", basename(fpath));
	FileLeads->packsize=packsize;
	FileLeads->packnums=fmap->fsize/packsize;
	FileLeads->tailsize=fmap->fsize%packsize;

	/* 4. Send ipack to inform Client to be ready to receive packets. */
	if( inet_tcp_send(sfd, (const void*)ipack, ipack->packsize)!=0 ) {
		ret=-6;
		goto END_FUNC;
	}

	/* 5. Send packets with pure file data! */
printf("Session_%d: Send packets...\n", sessionID);
	for(i=0; i < FileLeads->packnums; i++) {
		tm_delayms(gms);
		if( inet_tcp_send(sfd, (const void*)(fmap->fp +i*packsize ), packsize)!=0 ) {
			ret=-7;
			goto END_FUNC;
		}
	}

	/* 6. Send file tail packet */
printf("Session_%d: Send tail...\n", sessionID);
	if( FileLeads->tailsize>0 ) {
		tm_delayms(gms);
		if( inet_tcp_send(sfd, (const void*)(fmap->fp +i*packsize ), FileLeads->tailsize)!=0 ) {
			ret=-7;
			goto END_FUNC;
		}
	}

	/* 7. Free fmap and ipack ---> loop back */
printf("Session_%d: Free fmap and ipack....\n", sessionID);
	egi_fmap_free(&fmap);
	inet_ipacket_free(&ipack);

 } /* END while() */


  END_FUNC:
	/* Free fmap and ipack*/
	egi_fmap_free(&fmap);
	inet_ipacket_free(&ipack);

	/* Finish session */
        session->cmd=0;
        session->alive=false;
        close(sfd);
	session->csFD=-1;

	printf("Session_%d ends, ret=%d\n",sessionID,ret);
	return (void *)ret;  /* End whole process? */
}



/*------------------------------------------------------------
TCP client requests the server for a file, If the file exists,
TCP server transmits it in packets, the client then receive
and save it to CLIENT_FDIR.

Return:
	>0	EPIPE broken or Peer socket closed!
	0	OK
	<0	Fails
------------------------------------------------------------*/
int client_request_file(EGI_TCP_CLIT *uclit, const char* fname)
{
	int ret=0;
	int i;
	EGI_INETPACK *ipack=NULL;	/* Private packet struct */
	FILE_LEADS *FileLeads=NULL;	/* A pointer to ipack header */

	/* Temp buff and file */
	char *buff=NULL;
	int fd=-1;
	int nwrite;
	char fpath[EGI_PATH_MAX];

	/* Check input */
	if(uclit==NULL)
		return -1;

printf("Create ipack...\n");
	/* Create ipack, make FILE_LEADS Header of the ipack. */
	ipack=inet_ipacket_create(sizeof(FILE_LEADS), 0);     /* Only headsize, without privdata */
	if(ipack==NULL)
		return -2;

printf("Nest fileleads...\n");
	/* Nest FileLeads into ipack, and put request file name. */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);
	snprintf(FileLeads->fname, sizeof(FileLeads->fname)-1, "%s", fname);

printf("Send request...\n");
	/* 1. Send request ipack to the server */
	ret=inet_tcp_send(uclit->sockfd, (const void*)ipack, ipack->packsize);
	if( ret != 0 ) {
		/* >0 EPIPE broken */
		if(ret<0) ret=-3;
		goto END_FUNC;
	}

printf("Recv reply...\n");
	/* 2. Recevie reply from the server */
	ret=inet_tcp_recv(uclit->sockfd, (void *)ipack, ipack->packsize);
	if( ret!=0 ) {
		/* >0 Peer socket closed. */
		if(ret<0)ret=-4;
		goto END_FUNC;
	}
sleep(1);
printf("Check reply...\n");
	/* 3. Check reply msg */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);

	if(FileLeads->fname[0]==0) {
		printf("File '%s' NOT found in the server!\n",fname);
		ret=-5;
		goto END_FUNC;
	}
	printf("Ready to receive total %d packets, with each %dbytes, and a tail packet of %dbytes\n",
					FileLeads->packnums, FileLeads->packsize, FileLeads->tailsize);

	/* 4. Prepare buff and open file for write in */
	/* Temp. buff */
	buff=calloc(1, FileLeads->packsize);
	if(buff==NULL) {
		printf("Fail to calloc buff!\n");
		ret=-6;
		goto END_FUNC;
	}
	/* Open file for write */
	bzero(fpath,EGI_PATH_MAX);
	snprintf(fpath,EGI_PATH_MAX-1,"%s/%s",CLIENT_FDIR,fname);
	fd=open(fpath, O_RDWR|O_CREAT|O_CLOEXEC,S_IRWXU|S_IRWXG);
        if(fd<0) {
                printf("Fail to create '%s' for RDWR!\n", fpath);
                ret=-6;
		goto END_FUNC;
        }

	/* 5. Receive file packets from the server, packsize */
	for(i=0; i < FileLeads->packnums; i++) {
		printf("\r %d/%d", i+1,FileLeads->packnums+1);
		fflush(stdout);

		ret=inet_tcp_recv(uclit->sockfd, (void *)buff, FileLeads->packsize);
		if( ret!=0 ) {
			/* >0 Peer socket close */
			if(ret<0) ret=-7;
			goto END_FUNC;
		}

		/* Write to file */
		nwrite=write(fd, buff, FileLeads->packsize);
		if(nwrite!=FileLeads->packsize) {
			printf("Fail to write to file!, nwrite=%d for %d\n", nwrite, FileLeads->packsize);
			ret=-8;
			goto END_FUNC;
		}
	}
	/* Receive tail, tailsize */
	if( FileLeads->tailsize>0 ) {
		printf("\r %d/%d", FileLeads->packnums+1,FileLeads->packnums+1);
		ret=inet_tcp_recv(uclit->sockfd, (void *)buff, FileLeads->tailsize);
		if( ret!=0 ) {
			/* >0 Peer socket close */
			ret=-8;
			goto END_FUNC;
		}
		/* Write to file */
		nwrite=write(fd, buff, FileLeads->tailsize);
		if(nwrite!=FileLeads->tailsize) {
			printf("Fail to write to file!, nwrite=%d for %d\n", nwrite, FileLeads->tailsize);
			ret=-9;
			goto END_FUNC;
		}
	}


END_FUNC:
	/* close buff and fd */
	if(buff)
		free(buff);
	if(fd>0)
            if(close(fd)<0)
		printf("Fail to close fd, Err'%s'\n",strerror(errno));

	/* Free fmap and ipack*/
	inet_ipacket_free(&ipack);

	printf("Client_%d quits, ret=%d\n",getpid(),ret);
	return ret;
}
