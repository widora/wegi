/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program for EGI UDP moudle, to transmit/exchange data between
a server and several clients.

Usage:	test_TCP [-hsca:p:]
Example:
	./test_TCP -s -p 8765			( As server )
	./test_TCP -c -a 192.168.9.9 -p 8765	( As client )

	( Use test_garph.c to show net interface traffic speed )

Note:
1. This test MAY hamper other wifi devices!
2. When the TCP server quits the session, the client quits also.

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
#include <egi_inet.h>
#include <egi_timer.h>
#include <errno.h>

enum transmit_mode {
	mode_2Way	=0,  /* Server and Client PingPong mode */
	mode_S2C	=1,  /* Server to client only */
	mode_C2S	=2,  /* Client to server only */
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
}


/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{
	int gms;
	int opt;
	bool SetTCPServer=false;
	char *strAddr=NULL;		/* Default NULL, to be decided by system */
	unsigned short int port=0; 	/* Default 0, to be selected by system */
	EGI_CLOCK eclock={0};

#if 0 /*========== TEST: ===========*/

EGI_INET_MSGDATA *msgdata=NULL;

while(1) {
  msgdata=inet_msgdata_create(64*1024);
  printf("MSGDATA: msg[] %d bytes, data[] %d bytes\n", sizeof(*msgdata), inet_msgdata_getDataSize(msgdata));

  sprintf(msgdata->data+1024,"Yes!");
  printf("%s\n",msgdata->data+1024);

  inet_msgdata_free(&msgdata);
  usleep(10000);
}

exit(0);

#endif  /* ===== END TEST ===== */

	/* Parse input option */
	while( (opt=getopt(argc,argv,"hscva:p:g:m:"))!=-1 ) {
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
				printf("Set gms=%dms\n",gms);
				break;
			case 'm':
				trans_mode=atoi(optarg);
				break;
			default:
				SetTCPServer=false;
				gms=10;
				break;
		}
	}
	sleep(2);

  /* Default signal handlers for TCP processing */
  inet_default_sigAction();

  /* A. --- Work as a UDP Server */
  if( SetTCPServer ) {
	printf("Set as TCP Server.\n");

        /* Create UDP server */
        EGI_TCP_SERV *userv=inet_create_tcpServer(strAddr, port, 4); //8765, 4);
        if(userv==NULL) {
		printf("Fail to create a TCP server!\n");
                exit(1);
	}

        /* Set callback */
        userv->session_handler=TEST_tcpServer_session_handler;

        /* Process UDP server routine */
        inet_tcpServer_routine(userv);

        /* Free and destroy */
        inet_destroy_tcpServer(&userv);
  }

  /* B. --- Work as a UDP Client */
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

        /* -------- C/S session process ------- */
	int i;

	EGI_INET_MSGDATA *msgdata=NULL; /* MSG+DATA, use same MSGDATA for send and recv  */

//	char buff[EGI_MAX_TCP_PDATA_SIZE];

        int nsnd,nrcv;          /* returned bytes for each call  */
        int sndsize;            /* accumulated size */
        int rcvsize;

        int msgsize=sizeof(EGI_INET_MSGDATA);      /* MSGDATA withou data[] */
	int datasize=2*1024;  	/* Expected data size */
	int packsize=msgsize+datasize;		/* packsize=msgsize+datasize, from server */

	/* Err and Repeats counter */
	int sndErrs=0;
	int sndRepeats=0;
	int sndZeros=0;
	int sndEagains=0;
	int rcvErrs=0;
	int rcvRepeats=0;

	/* Create MSGDATA */
	msgdata=inet_msgdata_create(datasize);
	if(msgdata==NULL) exit(1);

	while(1) {

		/* Start ECLOCK */
		egi_clock_start(&eclock);

		/* DELAY: control speed */
		usleep(gms);

	        /* 1. Prepare request Msg  */
	        sprintf(msgdata->msg.cmsg, "TCP client %d request for data.", getpid());
		inet_msgdata_updateTimeStamp(msgdata);

		/* 2. Send a MSG to server to request data reply, addjust msgsize to ingore msgdata.data[] */
		sndsize=0;
        	while( sndsize < msgsize) {
			/* flags: MSG_CONFIRM, MSG_DONTWAIT */
	        	nsnd=send(uclit->sockfd, (void *)msgdata+sndsize, msgsize-sndsize, 0);
	        	//nsnd=sendto(uclit->sockfd, buff+sndsize, msgsize-sndsize, 0, &uclit->addrSERV, sizeof(struct sockaddr));
			if(nsnd>0) {
				/* Count sndsize */
				sndsize += nsnd;

				if(sndsize==msgsize) {
					/* OK */
				}
				else if( sndsize < msgsize ) {
					sndRepeats ++;
					printf(" sndsize < msgsize! continue to sendto() \n" );
					continue; /* ------> continue sendto() */
				}
				else { /* sndsize > msgsize, impossible! */
					sndErrs ++;
					printf(" sndsize > msgsize, send data error!\n");
				}
			}
	        	else if(nsnd<0) {
        	        	switch(errno) {
                	        	case EAGAIN:  //EWOULDBLOCK, for datastream it woudl block */
						sndEagains ++;
                        	             	printf("Fail to send MSG to the server, Err'%s'\n", strerror(errno));
						continue; /* ------> continue sendto() */
	                                	break;
		                        case EPIPE:
        		                     	printf("An EPIPE signals that the server quits the session! quit now!\n");
						exit(1);
	                                	break;
	        	                default: {
						sndErrs ++;
                        	             	printf("Fail to send MSG to the server, Err'%s'\n", strerror(errno));
						continue; /* --------> continue to sendto() */
					}
                		}
         		}
	        	else if(nsnd==0) {
				sndZeros ++;
        	        	printf(" ----- nsnd==0! --------\n");
         		}
	   	}

		/* 3. Receive data from server */
		rcvsize=0;
		while( rcvsize < packsize ) {
			//nrcv=read(uclit->sockfd, buff, sizeof(buff));
			/*** set MSG_WAITALL to wait for all data. however, "the call may still return less data, if a signal is caught,
        	         *   an error or disconnect occurs, or the next data to be received is of a different type than that returned."(man)
			 *   stream socket:  all data is deamed as a stream, while as you can adjust datasize each time for recv()...
			 *   you may select datasize same as the sending end, but NOT necessary.
			 */
			nrcv=recv(uclit->sockfd, (void *)msgdata+rcvsize, packsize-rcvsize, MSG_WAITALL); /* self_defined datasize! */
			if(nrcv>0) {
				/* counter rcvsize */
				rcvsize += nrcv;

				if( rcvsize == packsize ) {
					/* OK */
				}
				else if( rcvsize < packsize ) {
					rcvRepeats ++;
					 printf("nrcv=%d, packsize=%d bytes\n", nrcv, packsize);
				}
				else  {  /* rcvsize > packsize, impossible! */
					rcvErrs ++;
				}
			}
			else if(nrcv==0) {
				printf("The server end session! quit now...\n");
				exit(0);
			}
			else {
				printf("recv(): Err'%s'\n", strerror(errno));
				rcvErrs ++;
			}
		} /* End while() recv() */


		/* 4. Parse received MSGDATA */
		if( nrcv >= MSGDATA_CMSG_SIZE ) {
			printf("%s\n",msgdata->msg.cmsg);
			printf("nl_datasize=%d, Datasize=%d\n", msgdata->msg.nl_datasize, inet_msgdata_getDataSize(msgdata));
			printf("Server time stamp: %ld.%06ld\n", msgdata->msg.tmstamp.tv_sec, msgdata->msg.tmstamp.tv_usec);
			/* Only msgdata->data[] is available */
        		for(i=0;i<100;i++)
		                printf("%d ", msgdata->data[i]);
			printf("\n");
		}
                printf("sndErrs=%d, sndRepeats=%d, sndEagains=%d, sndZeros=%d, rcvErrs=%d, rcvRepeats=%d\n",
	                                                sndErrs, sndRepeats, sndEagains, sndZeros, rcvErrs, rcvRepeats);

		/* Stop ECLOCK */
		egi_clock_stop(&eclock);
		int tmms;
		tmms=egi_clock_readCostUsec(&eclock)/1000;
		/* This is instant speed, most time much bigger.., but some tmms is also much bigger.  */
		printf("--- Cost:%dms, RX:%dkBps---\n\n", tmms, packsize*1000/1024/tmms );

	}
	/* -------- END C/S session process ------- */

        /* Free and destroy */
        inet_destroy_tcpClient(&uclit);
  }


	free(strAddr);
        return 0;
}


