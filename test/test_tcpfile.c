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
#include <arpa/inet.h> /* inet_ntoa() */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <egi_inet.h>
#include <egi_timer.h>
#include <egi_log.h>
#include <egi_utils.h>

#define SERVER_PORT 5678
static int packsize=1448; /* packet size */
static int gms;		  /* interval gap delay */

/* --- A private message ---- */
typedef struct file_leads {
	char fname[128]; /* Request/Send File name */
	int packnums;   /* Total number of packets */
	int packsize;	/* Size of each packet  */
	int tailsize;    /* Size of the last packet, applys only if tailsize>0 */
} FILE_LEADS;

/* Session/Process handler */
void* server_send_file(void *arg);
int client_receive_file(EGI_TCP_CLIT *uclit, const char* fname);


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


/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{
	int opt;
	bool SetTCPServer;
	char *strAddr=NULL;		/* Default NULL, to be decided by system */
	unsigned short int port=0; 	/* Default 0, to be selected by system */


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
	printf("Set test packsize=%d (MSG+DATA)\n",packsize);
	sleep(2);

  /* Default signal handlers for TCP processing */
  inet_default_sigAction();

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
        EGI_TCP_SERV *userv=inet_create_tcpServer(strAddr, port, 4); //8765, 4);
        if(userv==NULL) {
		printf("Fail to create a TCP server!\n");
                exit(1);
	}

        /* Set session handler for each client */
        userv->session_handler=server_send_file;

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
        EGI_TCP_CLIT *uclit=inet_create_tcpClient(strAddr, port, 4); //"192.168.8.1", 8765, 4);
        if(uclit==NULL)
                exit(1);

	/* Run client routine */
	client_receive_file(uclit, "test_file");

        /* Free and destroy */
        inet_destroy_tcpClient(&uclit);
  }

	free(strAddr);
	egi_quit_log();
        return 0;
}


/* ------------ A detached thread function ---------------
TCP Server Session Handler

One thread for one client!
--------------------------------------------------------*/
void* server_send_file(void *arg)
{
	int ret=0;
	int i;
	EGI_INETPACK *ipack=NULL;	/* Private packet struct */
	FILE_LEADS *FileLeads=NULL;	/* A pointer to ipack header */

        EGI_TCP_SERV_SESSION *session=(EGI_TCP_SERV_SESSION *)arg;
        if(session==NULL)
		return (void *)-1;
	struct sockaddr_in *addrCLIT=&session->addrCLIT;

        int sfd=session->csFD;  /* Session FD */

        /* Detach thread */
        if(pthread_detach(pthread_self())!=0) {
                printf("Session_%d: Fail to detach thread for TCP session with client '%s:%d'.\n",
                                                session->sessionID, inet_ntoa(addrCLIT->sin_addr), ntohs(addrCLIT->sin_port) );
                // go on...
        }

	/* Mmap a file */
	const char *fname="/mmc/test_file";
	EGI_FILEMMAP *fmap=egi_fmap_create(fname);
	if(fmap==NULL)
		return (void *)-2;

	/* Create ipack, make FILE_LEADS Header ipack. */
	ipack=inet_ipacket_create(sizeof(FILE_LEADS), 0);     /* Only headsize, without privdata */
	if(ipack==NULL)
		return (void *)-3;


	/* 1. Receive request from client */
	if( inet_tcp_recv(sfd, (void *)ipack, ipack->packsize)!=0 ) {
		ret=-4;
		goto END_FUNC;
	}

	/* 2. Check request FILE_LEADS */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);
	if( strcmp(basename(fname),FileLeads->fname)!=0 ) {	/* Check file name */
		/* Reply with empty FileLeads */
		bzero(FileLeads, sizeof(FILE_LEADS));
		inet_tcp_send(sfd, (void *)ipack, ipack->packsize);
		ret=-5;
		goto END_FUNC;
	}

	/* 3. Nest FileLeads into ipack */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);
	snprintf(FileLeads->fname, sizeof(FileLeads->fname)-1, "%s", basename(fname));
	FileLeads->packsize=packsize;
	FileLeads->packnums=fmap->fsize/packsize;
	FileLeads->tailsize=fmap->fsize%packsize;

	/* 4. Send ipack to inform Client to be ready to receive packets. */
	if( inet_tcp_send(sfd, (const void*)ipack, ipack->packsize)!=0 ) {
		ret=-6;
		goto END_FUNC;
	}

	/* 5. Send packets with pure file data! */
	for(i=0; i < FileLeads->packnums; i++) {
		tm_delayms(gms);
		if( inet_tcp_send(sfd, (const void*)(fmap->fp +i*packsize ), packsize)!=0 ) {
			ret=-7;
			goto END_FUNC;
		}
	}

	/* 6. Send file tail packet */
	if( FileLeads->tailsize>0 ) {
		tm_delayms(gms);
		if( inet_tcp_send(sfd, (const void*)(fmap->fp +i*packsize ), packsize)!=0 ) {
			ret=-7;
			goto END_FUNC;
		}
	}

	/* 7. Confirm from client */


END_FUNC:
	/* Free fmap and ipack*/
	egi_fmap_free(&fmap);
	inet_ipacket_free(&ipack);

	/* Finish session */
        session->cmd=0;
        session->alive=false;
        close(sfd);
	session->csFD=-1;

	return (void *)ret;
}



/*---------------------------------------------
TCP Client Session Proces
To receive a file from the TCP server.

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
int client_receive_file(EGI_TCP_CLIT *uclit, const char* fname)
{
	int ret=0;
	int i;
	EGI_INETPACK *ipack=NULL;	/* Private packet struct */
	FILE_LEADS *FileLeads=NULL;	/* A pointer to ipack header */

	if(uclit==NULL)
		return -1;

	/* Create ipack, make FILE_LEADS Header of the ipack. */
	ipack=inet_ipacket_create(sizeof(FILE_LEADS), 0);     /* Only headsize, without privdata */
	if(ipack==NULL)
		return -2;

	/* Nest FileLeads into ipack, and put request file name. */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);
	snprintf(FileLeads->fname, sizeof(FileLeads->fname)-1, "%s", fname);

	/* 1. Send request ipack to the server */
	if( inet_tcp_send(uclit->sockfd, (const void*)ipack, ipack->packsize)!=0 ) {
		ret=-3;
		goto END_FUNC;
	}

	/* 2. Recevie reply from the server */
	if( inet_tcp_recv(uclit->sockfd, (void *)ipack, ipack->packsize)!=0 ) {
		ret=-4;
		goto END_FUNC;
	}

	/* 3. Check reply msg */
	FileLeads=(FILE_LEADS *)IPACK_HEADER(ipack);
	if(FileLeads->fname[0]==0) {
		printf("File '%s' NOT found in the server!\n",fname);
		ret=-5;
		goto END_FUNC;
	}

	/* 4. Prepare buff and open file for write in */
	char *buff=NULL;
	int fd;
	int nwrite;
	char fpath[128]="/mmc/";
	strcat(fpath,fname);

	buff=calloc(1, FileLeads->packsize);
	if(buff==NULL) {
		printf("Fail to calloc buff!\n");
		ret=-6;
		goto END_FUNC;
	}

	fd=open(fpath, O_RDWR|O_CREAT|O_CLOEXEC,S_IRWXU|S_IRWXG);
        if(fd<0) {
                printf("Fail to create '%s' for RDWR!\n", fpath);
                ret=-6;
		goto END_FUNC;
        }

	/* 5. Receive file packets from the server, packsize */
	for(i=0; i < FileLeads->packnums; i++) {
		printf("\r %d/%d", i,FileLeads->packnums);
		fflush(stdout);

		if( inet_tcp_recv(uclit->sockfd, (void *)buff, FileLeads->packsize)!=0 ) {
			ret=-7;
			goto END_FUNC;
		}
		/* Write to file */
		nwrite=write(fd, buff, FileLeads->packsize);
		if(nwrite!=FileLeads->packsize) {
			printf("Fail to write to file!, nwrite=%d\n", nwrite);
			ret=-8;
			goto END_FUNC;
		}
	}
	/* Receive tail, tailsize */
	if( FileLeads->tailsize>0 ) {
		if( inet_tcp_recv(uclit->sockfd, (void *)buff, FileLeads->tailsize)!=0 ) {
			ret=-7;
			goto END_FUNC;
		}
		/* Write to file */
		nwrite=write(fd, buff, FileLeads->tailsize);
		if(nwrite!=FileLeads->tailsize) {
			printf("Fail to write to file!, nwrite=%d\n", nwrite);
			ret=-8;
			goto END_FUNC;
		}
	}

END_FUNC:
	/* close file */
	close(fd);

	/* Free fmap and ipack*/
	inet_ipacket_free(&ipack);

	return ret;
}
