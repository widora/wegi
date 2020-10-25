/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transfer a file by TCP.

Usage:	test_tcpfile [-hsca:p:]

Example:
	./test_tcpfile -s -p 8765 -k 688888		( As server )
	./test_tcpfile -c -a 192.168.9.9 -p 8765	( As client )

	( Use test_garph.c to show net interface traffic speed )

Note:
1. A TCP client (as a file receiver) runs with higher CPU Load than a TCP server (as file provider),
   especillay when the client save files to miniSD card.
					CPU Load Comparison
   SEVRER_FDIR in miniSD, CLIENT_FDIR in /tmp:		Sever 0.4~1.9   Client: 1.0~2.7
   SERVER_FDIR in miniSD, CLIENT_FDIR in miniSD:	Sever 0.4~1.9   Client: 3~5
   (Low speed with low CPU Load range.)

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
-----------------------------------------------------------------------*/
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
#include <egi_cstring.h>

#define SERVER_PORT 5678
#define SERVER_FDIR "/mmc"
#define CLIENT_FDIR "/tmp"

static int packsize=1448; /* packet size */
static int gms;		  /* interval gap delay */
static char fname[EGI_NAME_MAX];

static EGI_TCP_SERV *userv;
static EGI_TCP_CLIT *uclit;
static bool SetTCPServer;

/*** 	--- A private IPACK data[] ---

  PriveHEAD is a msgdata_head_type.

       PrivHEAD 		       PrivDATA
 ---------------------		---------------------
 IHEAD_REQUEST_LIST		NULL
 IHEAD_REQUEST_FILE		an FILE_LEAD, fname[] only
 IHEAD_REPLY_FLISTLEAD		an LIST_LEAD
 IHEAD_REPLY_FILELEAD		an FILE_LEAD
 IHEAD_FAILS			NULL

*/

typedef enum {
	IHEAD_REQUEST_LIST  =0,
	IHEAD_REQUEST_FILE,
	IHEAD_REPLY_FLISTLEAD,
	IHEAD_REPLY_FILELEAD,
	IHEAD_FAILS,
	IHEAD_UNDEFINE,
} IHEAD_TYPE;

typedef struct list_lead {
        uint32_t        offs_size;       /* txtgroup->offs[] */
        uint32_t        buff_size;       /* txtgroup->buff[] */
} LIST_LEAD;

typedef struct file_lead {
	char fname[EGI_NAME_MAX]; /* Request/Send File name */
	int packnums;   /* Total number of packets */
	int packsize;	/* Size of each packet  */
	int tailsize;    /* Size of the last packet, applys only if tailsize>0 */
} FILE_LEAD;

enum transmit_mode {
        mode_2Way       =0,  /* Server and Client PingPong mode */
        mode_S2C        =1,  /* Server to client only */
        mode_C2S        =2,  /* Client to server only */
};

static enum transmit_mode trans_mode;
static bool verbose_on;


/* Session/Process handler */
void* server_session_process(void *arg);
int   client_request_file(EGI_TCP_CLIT *uclit, const char* fname);
int   client_request_filelist(EGI_TCP_CLIT *uclit);

/* Util functions */
EGI_TXTGROUP *get_filelist(const char *path);
void show_help(const char* cmd);
void inet_sigint_handler(int signum);



/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{
	int ret;
	int opt;
	char *strAddr=NULL;		/* Default NULL, to be decided by system */
	unsigned short int port=0; 	/* Default 0, to be selected by system */
	int req;
  	int fcnt=0;

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
	sleep(2);


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
        userv=inet_create_tcpServer(strAddr, port, 4, 15, 15);
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
        uclit=inet_create_tcpClient(strAddr, port, 4, 15, 15);
        if(uclit==NULL)
                exit(1);

        /* Select request */
	while(1) {
goto TEST;
		printf("Select request(F/L/Q):");
		req=fgetc(stdin);
		//req=getchar();
		while(getchar()!='\n')continue;
		switch(req) {
			case 'F': case 'f':
				printf("Client session process: Request and receive files.\n");
				while(1) {
					printf("Please input file name(Enter to quit):");
					fflush(stdout);
					fgets(fname, EGI_NAME_MAX,stdin);
					fname[strlen(fname)-1]='\0';
					if(fname[0]==0) break;
					printf("fname:%s\n",fname);
TEST:
	sleep(1);
	sprintf(fname,"程序员节大礼包.xxx");
					if( client_request_file(uclit, fname) >0 ) {
						/* >0 EPIPE or peer socket closed */
						if(inet_tcp_getState(uclit->sockfd)==TCP_CLOSE) {
							EGI_PLOG(LOGLV_TEST,"Peer socket closed, quit now.");
							exit(1);
						}
						break;
					}
					printf("Succeed requesting files: %d\n", ++fcnt);
goto TEST;
				}
				break;

			case 'L': case 'l':
				printf("Client session process: Request file list.\n");
        			if( client_request_filelist(uclit) >0 ) {
			        	/* >0 EPIPE or peer socket closed */
        				inet_tcp_getState(uclit->sockfd);
        			}
				break;

			case 'Q': case 'q':
				printf("Quit!\n");
				exit(1);
				break;
			default:
				printf(":%c\n",req);
				break;
		}
	}

        /* Free and destroy */
        inet_destroy_tcpClient(&uclit);
  }

	free(strAddr);
	egi_quit_log();
        return 0;
}


/*---------------------------
  	Print help
-----------------------------*/
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

/*---------------------------------
	  SIGINT handler
-----------------------------------*/
void inet_sigint_handler(int signum)
{
	static int depth=0;
	int i;

	if(signum!=SIGINT)
		return;

	/* Shut down connection */
	if(SetTCPServer) {

		printf(" WARNING_%d/%d: To close the server will close all sessions!\n", ++depth, 3);
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


/* ------------ A detached thread function ---------------
TCP Server Session Handler

1. It exits as the client exits.
2. Packsize is set/controlled by the server.

One thread for one client!
---------------------------------------------------------*/
void* server_session_process(void *arg)
{
	int ret=0;
	int i;
	EGI_INETPACK *ipack=NULL;	/* Private packet struct */
	IHEAD_TYPE msgtype;
	LIST_LEAD  ListLead;
	FILE_LEAD  FileLead;		/* A pointer to ipack header */
	EGI_TXTGROUP *txtgroup=NULL;
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

	printf("Session_%d: Wait for request...\n", sessionID );
	/* Receive request from client */
	if( inet_ipacket_tcpRecv(sfd, &ipack) !=0 ) {
		ret=-4; goto END_FUNC;
	}

        /* Check out msgtype */
	msgtype=*(IHEAD_TYPE *)IPACK_HEADER(ipack);

	/* Switch to requested session */
	switch( msgtype ) {
		case IHEAD_REQUEST_LIST:
			printf("Session_%d: Client requests file list!\n",sessionID);
			txtgroup=get_filelist(SERVER_FDIR);

			/* Free old ipack */
			inet_ipacket_free(&ipack);

			/* If fails */
			if(txtgroup==NULL) {
				msgtype=IHEAD_FAILS;
				ipack=inet_ipacket_create(sizeof(msgtype), (void *)&msgtype , 0, NULL);
				ret=inet_ipacket_tcpSend(sfd, ipack);
				if(ret>0 ) goto END_FUNC;
				inet_ipacket_free(&ipack);
			}
			/* */
			else {
				/* Send list head */
				msgtype=IHEAD_REPLY_FLISTLEAD;
				ListLead.offs_size=txtgroup->size;
				ListLead.buff_size=txtgroup->buff_size;
				ipack=inet_ipacket_create(sizeof(msgtype), &msgtype , sizeof(ListLead), &ListLead);
				printf("Session_%d: Send file LIST_LEAD...\n", sessionID);
				ret=inet_ipacket_tcpSend(sfd, ipack);
				if(ret>0 ) goto END_FUNC;
				inet_ipacket_free(&ipack);

			/* TODO: Replace by inet_ipacket_pushData()  */

				/* Send offs[] data */
				ipack=inet_ipacket_create( 0, NULL, txtgroup->size*sizeof(typeof(*txtgroup->offs)), txtgroup->offs);
				printf("Session_%d: Send txtgroup offs[]...\n", sessionID);
				ret=inet_ipacket_tcpSend(sfd, ipack);
				if(ret>0) goto END_FUNC;
				inet_ipacket_free(&ipack);
				/* Send buff[] data */
				ipack=inet_ipacket_create( 0, NULL, txtgroup->buff_size*sizeof(typeof(*txtgroup->buff)), txtgroup->buff);
				printf("Session_%d: Send txtgroup buff[]...\n", sessionID);
				ret=inet_ipacket_tcpSend(sfd, ipack);
				if(ret>0 ) goto END_FUNC;
				inet_ipacket_free(&ipack);
				printf("Session_%d: File list is sent out!\n", sessionID);
			}

			break;

		case IHEAD_REQUEST_FILE:
			/* 1. Check out file name in ipack->privdata */
			bzero(fpath,sizeof(fpath));
			snprintf(fpath,EGI_PATH_MAX-1,"%s/%s",SERVER_FDIR, (char *)IPACK_PRIVDATA(ipack));
			printf("Session_%d: Client '%s:%d' requets '%s'.\n",
					sessionID, inet_ntoa(addrCLIT->sin_addr), ntohs(addrCLIT->sin_port), fpath);
			/* Free old ipack */
			inet_ipacket_free(&ipack);

			/* 2. Search file in DIR */
			if( access(fpath, R_OK)!=0 ) {
				/* File NOT found */
				printf("Access Err'%s'\n", strerror(errno));
				msgtype=IHEAD_FAILS;
				/* Send an empty ipacket */
				ipack=inet_ipacket_create(sizeof(msgtype), &msgtype, 0, NULL);
				ret=inet_ipacket_tcpSend(sfd, ipack);
				if(ret!=0) goto END_FUNC;
				inet_ipacket_free(&ipack);
				continue; /* continue to ---> listen request! */
			}
			/* Mmap file */
			fmap=egi_fmap_create(fpath);
			if(fmap==NULL) goto END_FUNC;

			/* 3. Nest FileLead into ipack for reply */
			snprintf(FileLead.fname, EGI_NAME_MAX-1, "%s", basename(fpath));
			FileLead.packsize=packsize;
			FileLead.packnums=fmap->fsize/packsize;
			FileLead.tailsize=fmap->fsize%packsize;
			msgtype=IHEAD_REPLY_FILELEAD;
			ipack=inet_ipacket_create(sizeof(msgtype), &msgtype, sizeof(FileLead), &FileLead);
			//if(ipack==NULL) goto END_FUNC;

			/* 4. Send ipack to inform Client to be ready to receive packets. */
			ret=inet_ipacket_tcpSend(sfd, ipack);
			if(ret!=0) goto END_FUNC;

			/* 5. Send packets with pure file data! */
			printf("Session_%d: Send packets...\n", sessionID);
			for(i=0; i < FileLead.packnums; i++) {
				tm_delayms(gms);
				ret=inet_tcp_send(sfd, (const void*)(fmap->fp +i*packsize ), packsize);
				if(ret!=0) goto END_FUNC;
			}

			/* 6. Send file tail packet */
			printf("Session_%d: Send tail...\n", sessionID);
			if( FileLead.tailsize>0 ) {
				tm_delayms(gms);
				ret=inet_tcp_send(sfd, (const void*)(fmap->fp +i*packsize ), FileLead.tailsize);
				if(ret!=0) goto END_FUNC;
			}

			/* 7. Free fmap and ipack ---> loop back */
			printf("Session_%d: File data is sent out!\n", sessionID);
			printf("Session_%d: Free fmap and ipack....\n", sessionID);
			egi_fmap_free(&fmap);
			inet_ipacket_free(&ipack);
			break;

		default:
			printf("Undefined request!\n");

	} /* END switch() */

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

	/* ret>0 for TIMEOUT */
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
	FILE_LEAD  FileLead;
	IHEAD_TYPE msgtype;

	/* Temp buff and file */
	char *buff=NULL;
	int fd=-1;
	int nwrite;
	char fpath[EGI_PATH_MAX];

	/* Check input */
	if(uclit==NULL)
		return -1;

	/* Create a request ipack:   Header: IHEAD_REQUEST_FILE, privdata: filename */
	msgtype=IHEAD_REQUEST_FILE;
	ipack=inet_ipacket_create(sizeof(msgtype), (void *)&msgtype, strlen(fname)+1, (void *)fname); /* +1 EOF */
	if(ipack==NULL)
		return -2;

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
	ret=inet_ipacket_tcpRecv(uclit->sockfd, &ipack);
	if( ret!=0 ) {
		/* >0 Peer socket closed. */
		if(ret<0)ret=-4;
		goto END_FUNC;
	}

	printf("Check reply...\n");
	/* 3. Check reply msg */
	msgtype=*(IHEAD_TYPE *)IPACK_HEADER(ipack);
	if(msgtype==IHEAD_FAILS || msgtype!=IHEAD_REPLY_FILELEAD ) {
		printf("File '%s' NOT found in the server!\n",fname);
		ret=-5;	goto END_FUNC;
	}
	else if(msgtype!=IHEAD_REPLY_FILELEAD) {
		printf("Server reply FAILS!\n");
		ret=-5; goto END_FUNC;
	}

	FileLead=*(FILE_LEAD *)IPACK_PRIVDATA(ipack);
	printf("Ready to receive total %d IPACKs, with each %dbytes, and a tail packet of %dbytes\n",
						FileLead.packnums, FileLead.packsize, FileLead.tailsize);

	/* 4. Prepare buff and open file for write in */
	/* Temp. buff */
	buff=calloc(1, FileLead.packsize);
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
                EGI_PLOG(LOGLV_TEST, "Fail to create '%s' for RDWR!", fpath);
                ret=-6;
		goto END_FUNC;
        }

	/* 5. Receive file packets from the server, packsize */
	for(i=0; i < FileLead.packnums; i++) {
		printf("\r %d/%d", i+1,FileLead.packnums+1);
		fflush(stdout);

		ret=inet_tcp_recv(uclit->sockfd, (void *)buff, FileLead.packsize);
		if( ret!=0 ) {
			/* >0 Peer socket close */
			if(ret<0) ret=-7;
			goto END_FUNC;
		}

		/* Write to file */
		nwrite=write(fd, buff, FileLead.packsize);
		if(nwrite!=FileLead.packsize) {
			if(nwrite<0)
				EGI_PLOG(LOGLV_TEST,"write() packsize: Err'%s'", strerror(errno));
			else
				printf("Fail to write to file!, nwrite=%d for %d\n", nwrite, FileLead.packsize);
			ret=-8;
			goto END_FUNC;
		}
	}
	/* Receive tail, tailsize */
	if( FileLead.tailsize>0 ) {
		printf("\r %d/%d", FileLead.packnums+1,FileLead.packnums+1);
		ret=inet_tcp_recv(uclit->sockfd, (void *)buff, FileLead.tailsize);
		if( ret!=0 ) {
			/* >0 Peer socket close */
			if(ret<0)ret=-8;
			goto END_FUNC;
		}
		/* Write to file */
		nwrite=write(fd, buff, FileLead.tailsize);
		if(nwrite!=FileLead.tailsize) {
			if(nwrite<0)
				EGI_PLOG(LOGLV_TEST,"write() tailsie: Err'%s'", strerror(errno));
			else
				printf("Fail to write to file!, nwrite=%d for %d\n", nwrite, FileLead.tailsize);
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

/*------------------------------------------------------------
TCP client requests the server for a file list.
TCP server put the list in an EGI_TXTGROUP and pack it in ipack,
the client receive it and print out.

Return:
	>0	EPIPE broken or Peer socket closed!
	0	OK
	<0	Fails
------------------------------------------------------------*/
int client_request_filelist(EGI_TCP_CLIT *uclit)
{
	int ret=0;
	int i;
	EGI_INETPACK *ipack=NULL;	/* Private packet struct */
	IHEAD_TYPE msgtype;
	LIST_LEAD  ListLead;
	EGI_TXTGROUP *txtgroup=NULL;

	/* Check input */
	if(uclit==NULL)
		return -1;

	printf("Create ipack...\n");
	/* Create a request ipack, make IHEAD_REQUEST_LIST Header of the ipack. */
	msgtype=IHEAD_REQUEST_LIST;
	ipack=inet_ipacket_create(sizeof(msgtype), (void *)&msgtype,0,NULL);     /* Only headsize, without privdata */
	//if(ipack==NULL) return -2;

	printf("Send request...\n");
	/* 1. Send request ipack to the server */
	ret=inet_ipacket_tcpSend(uclit->sockfd, ipack);
	if( ret != 0 ) {
		/* >0 EPIPE broken */
		if(ret<0) ret=-3;
		goto END_FUNC;
	}

	printf("Recv reply...\n");
	/* 2. Recevie reply from the server */
	ret=inet_ipacket_tcpRecv(uclit->sockfd, &ipack);
	if( ret!=0 ) {
		/* >0 Peer socket closed. */
		if(ret<0)ret=-4;
		goto END_FUNC;
	}

	/* Check out msgtype */
	msgtype=*(IHEAD_TYPE *)IPACK_HEADER(ipack);
	if(msgtype==IHEAD_REPLY_FLISTLEAD)
		printf("ipack head type: IHEAD_REPLY_FLISTLEAD \n");
	else if(msgtype==IHEAD_FAILS) {
		printf("ipack head type: IHEAD_FAILS \n");
		goto END_FUNC;
	}
	else {
		printf("ipack head type: Undefined! \n");
		goto END_FUNC;
	}
	/* Check out LIST_LEAD */
	ListLead=*(LIST_LEAD *)IPACK_PRIVDATA(ipack);
	printf("ListLead: offs_size=%d, buff_size=%d\n", ListLead.offs_size, ListLead.buff_size);

	/* Create txtgroup */
	txtgroup=cstr_txtgroup_create(ListLead.offs_size, ListLead.buff_size);
	if(txtgroup==NULL) goto END_FUNC;

	/* Receive and fill to txtgroup.offs[] */
	ret=inet_ipacket_tcpRecv(uclit->sockfd, &ipack);
	if(ret!=0) goto END_FUNC;
	memcpy(txtgroup->offs, IPACK_PRIVDATA(ipack), sizeof(typeof(*txtgroup->offs))*ListLead.offs_size);
	txtgroup->size=ListLead.offs_size;

	/* Receive and fill to txtgroup.buff[] */
	ret=inet_ipacket_tcpRecv(uclit->sockfd, &ipack);
	if(ret!=0) goto END_FUNC;
	memcpy(txtgroup->buff, IPACK_PRIVDATA(ipack), sizeof(typeof(*txtgroup->buff))*ListLead.buff_size);
	txtgroup->buff_size=ListLead.buff_size;

	/* Print out txtgroup */
        for(i=0; i< txtgroup->size; i++) {
                printf("TXT[%d]: %s\n", i, txtgroup->buff+txtgroup->offs[i]);
	}

END_FUNC:
	/* Free txtgroup */
	cstr_txtgroup_free(&txtgroup);

	/* Free fmap and ipack*/
	inet_ipacket_free(&ipack);

	printf("Client_%d session ends, ret=%d\n",getpid(),ret);
	return ret;
}


#include <dirent.h>
/*---------------------------------------------
Get a list of all regular files in the given
directory. then put then into an EGI_TXTGOURP.

Return:
	A pointer to EGI_TXTGROUP 	OK
	NULL				Fails
----------------------------------------------*/
EGI_TXTGROUP *get_filelist(const char *path)
{
	DIR *dir;
	struct dirent *file;
	struct stat sb;
        char fullpath[EGI_PATH_MAX+EGI_NAME_MAX];
        char tmptxt[EGI_PATH_MAX+EGI_NAME_MAX+64];

	EGI_TXTGROUP *txtgroup=NULL;

        /* Create txtgroup */
        txtgroup=cstr_txtgroup_create(0,0);
        if(txtgroup==NULL)
		exit(1);

	/* Open dir */
        if(!(dir=opendir(path))) {
		cstr_txtgroup_free(&txtgroup);
		return NULL;
	}

        /* Get file names */
        while((file=readdir(dir))!=NULL) {
           	/* If NOT a regular file */
		snprintf(fullpath, EGI_PATH_MAX+EGI_NAME_MAX-1,"%s/%s",path ,file->d_name);
                if( stat(fullpath, &sb)<0 || !S_ISREG(sb.st_mode) ) {
			//printf("%s \n",fullpath);
                        continue;
		}

		/* Print filename and size into tmptxt */
                if( ((sb.st_size)>>20) > 0)
                        snprintf(tmptxt,sizeof(tmptxt)-1,"%-32s  %.1fM", file->d_name, sb.st_size/1024.0/1024.0);
                else if ( ((sb.st_size)>>10) > 0)
                        snprintf(tmptxt,sizeof(tmptxt)-1,"%-32s  %.1fk", file->d_name, sb.st_size/1024.0);
                else
                        snprintf(tmptxt,sizeof(tmptxt)-1,"%-32s  %ldB", file->d_name, sb.st_size);

		/* Push to txtgroup */
                //cstr_txtgroup_push(txtgroup, fullpath);
                cstr_txtgroup_push(txtgroup, tmptxt);
	}

	closedir(dir);

	return txtgroup;
}

