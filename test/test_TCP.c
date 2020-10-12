/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program for EGI TCP moudle, to transmit/exchange data between
a server and several clients.

Usage:	test_TCP [-hsca:p:]
Example:
	./test_TCP -s -p 8765			( As server )
	./test_TCP -c -a 192.168.9.9 -p 8765	( As client )

	( Use test_garph.c to show net interface traffic speed )

Note:
1. This test MAY hamper other wifi devices!
2. When the TCP server quits the session, the client quits also.
3. TEST: Single TCP packet Max. payload:  MTU(1500)-IPhead(20)-TCPhead(20)-TimestampOption(12)    =1448 Bytes.
   see below.
4. ----------- ERR LOG -----------------
[2020-10-10 18:38:26] [LOGLV_TEST] nrcv=1448 < packsize=2572 bytes   (  several times! )
[2020-10-10 18:38:36] [LOGLV_TEST] recv(): Err'Resource temporarily unavailable'.
[2020-10-10 18:39:11] [LOGLV_TEST] SVR Msg: sndErrs=0, sndRepeats=0, sndEagains=0, sndZeros=0, rcvErrs=17, rcvRepeats=0
[2020-10-10 18:39:11] [LOGLV_TEST] Client: sndErrs=0, sndRepeats=0, sndEagains=0, sndZeros=0, rcvErrs=4, rcvRepeats=1
[2020-10-12 09:58:29] [LOGLV_TEST] Client 9302: SVR Msg: sndErrs=0, sndRepeats=0, sndEagains=0, sndZeros=0, rcvErrs=39, rcvRepeats=0
[2020-10-12 09:58:29] [LOGLV_TEST] Client 9302: sndErrs=0, sndRepeats=0, sndEagains=0, sndZeros=0, rcvErrs=6, rcvRepeats=1
[2020-10-12 09:57:17] [LOGLV_TEST] Client 9308: SVR Msg: sndErrs=0, sndRepeats=0, sndEagains=0, sndZeros=0, rcvErrs=45, rcvRepeats=0
[2020-10-12 09:57:17] [LOGLV_TEST] Client 9308: sndErrs=0, sndRepeats=0, sndEagains=0, sndZeros=0, rcvErrs=8, rcvRepeats=3
 NOTE: TEST shows 2 types of errors/repeats:
	rcvErr:		EAGAIN, Err'Resource temporarily unavailable'
	rcvRepeats.	Recv() an incomplete packet, repeat to call recv().


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
#include <egi_log.h>

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


#if 0 /*========== TEST: INET_MSGDATA ===========*/

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

  /* Start egi log */
  if(egi_init_log("/mmc/test_tcp.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  }
  EGI_PLOG(LOGLV_TEST,"Start logging...");

  /* A. --- Work as a TCP Server */
  if( SetTCPServer ) {
	printf("Set as TCP Server.\n");

        /* Create TCP server */
        EGI_TCP_SERV *userv=inet_create_tcpServer(strAddr, port, 4); //8765, 4);
        if(userv==NULL) {
		printf("Fail to create a TCP server!\n");
                exit(1);
	}

        /* Set callback */
        userv->session_handler=TEST_tcpServer_session_handler;

        /* Process TCP server routine */
        inet_tcpServer_routine(userv);

        /* Free and destroy */
        inet_destroy_tcpServer(&userv);
  }

  /* B. --- Work as a TCP Client */
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
	TEST_tcpClient_routine(uclit, gms);

        /* Free and destroy */
        inet_destroy_tcpClient(&uclit);
  }

	free(strAddr);
	egi_quit_log();
        return 0;
}


