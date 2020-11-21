/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program of tranferring a file under UDP protocol.

Usage Example:
	./test_udpfile -s -p 8765		( As server )
	./test_udp -c -a 192.168.9.9 -p 8765	( As client )

	( Use test_garph.c to show net interface traffic speed )

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <fcntl.h>
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
#include <egi_utils.h>

EGI_FILEMMAP *fmap;   	/* For server: send file. */
int fd;	      		/* For client: save file. */

/* A private header for my IPACK */
typedef struct {
	int		code;
	unsigned int	seq;	/* Packet sequence number, from 0 */
	unsigned int	total;  /* Total packets */
	//unsigned long long all_size
#define CODE_NONE	0	/* none */
#define CODE_DATA	1	/* Data availabe, seq/total */
#define	CODE_REQUEST	2	/* Client ask for file stransfer */
#define CODE_FINISH	3	/* Client ends session */
} PRIV_HEADER;

/* Following pointers to be used as reference only! */
PRIV_HEADER  *header_rcv;
PRIV_HEADER  *header_snd;
EGI_INETPACK *ipack_rcv;
EGI_INETPACK *ipack_snd;

struct sockaddr_in  clientAddr;
struct sockaddr_in  serverAddr;

/* UDP packet information */
static unsigned int packsize;    	/* 1448,  packet size */
static unsigned int datasize=1024*4;    /* IPACK.data size */
static unsigned int tailsize;		/* Defined as size of last pack, when pack_seq == pack_total-1 */
static unsigned int pack_seq;	   	/* Sequence number for current IPACK */
static unsigned int pack_total;    	/* Total number of IPACKs */

enum transmit_mode {
	mode_2Way	=0,  /* Server and Client PingPong mode */
	mode_S2C	=1,  /* Server to client only */
	mode_C2S	=2,  /* Client to server only */
};
static enum transmit_mode trans_mode;
static bool verbose_on;

void print_help(const char* cmd)
{
	printf("Usage: %s [-hscva:p:m:] \n", cmd);
       	printf("        -h   help \n");
   	printf("        -s   work as UDP Server\n");
   	printf("        -c   work as UDP Client (default)\n");
	printf("	-v   verbose to print rcvSize\n");
	printf("	-a:  Server IP address\n");
	printf("	-y:  Client IP address, default NULL.\n");
	printf("	-p:  Port number, default 0.\n");
	printf("	-m:  Transmit mode, 0-Twoway(default), 1-S2C, 2-C2S \n");
}

/* Callback functions, define routine jobs. */
int Client_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
								 	   char *sndBuff, int *sndSize );
int Server_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
					 struct sockaddr_in *sndAddr, char *sndBuff, int *sndSize);

/*----------------------------
	    Main()
----------------------------*/
int main(int argc, char **argv)
{
	int opt;
	bool SetUDPServer=false;
	char *svrAddr=NULL;		/* Default NULL, to be decided by system */
	char *cltAddr=NULL;
	unsigned short int port=0; 	/* Default 0, to be selected by system */
	char *fpath="udp_test.jpg";

	/* Parse input option */
	while( (opt=getopt(argc,argv,"hscva:y:p:m:"))!=-1 ) {
		switch(opt) {
			case 'h':
				print_help(argv[0]);
				exit(0);
				break;
			case 's':
				SetUDPServer=true;
				break;
			case 'c':
				SetUDPServer=false;
				break;
			case 'v':
				printf("Set verbose ON.\n");
				verbose_on=true;
				break;
			case 'a':
				svrAddr=strdup(optarg);
				printf("Set server address='%s'\n",svrAddr);
				break;
			case 'y':
				cltAddr=strdup(optarg);
				printf("Set clietn address='%s'\n",cltAddr);
				break;
			case 'p':
				port=atoi(optarg);
				printf("Set port=%d\n",port);
				break;
			case 'm':
				trans_mode=atoi(optarg);
				break;
			default:
				SetUDPServer=false;
				break;
		}
	}


  /* A. --- Work as a UDP Server */
  if( SetUDPServer ) {
	printf("Set as UDP Server, set port=%d \n", port);

        /* Mmap file and get total_packs for transmitting. */
        fmap=egi_fmap_create(fpath);
        if(fmap==NULL) {
		exit(EXIT_FAILURE);
        }
	pack_total=fmap->fsize/datasize;
	tailsize=fmap->fsize - pack_total*datasize;
	if(tailsize)
		pack_total +=1;
	else
		tailsize=datasize;

	/* Preset pack_seq=pack_total, as for holdon. */
	pack_seq=pack_total;

        /* Create UDP server */
        EGI_UDP_SERV *userv=inet_create_udpServer(svrAddr, port, 4); //8765, 4);
        if(userv==NULL)
                exit(EXIT_FAILURE);

        /* Set callback */
        userv->callback=Server_Callback;

        /* Process UDP server routine */
        inet_udpServer_routine(userv);

        /* Free and destroy */
        inet_destroy_udpServer(&userv);
	egi_fmap_free(&fmap);
  }

  /* B. --- Work as a UDP Client */
  else {
	printf("Set as UDP Client, targets to server at %s:%d. \n", svrAddr, port);

	if( svrAddr==NULL || port==0 ) {
		printf("Please provide Server IP address and port number!\n");
		print_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Open file for write */
        fd=open(fpath, O_RDWR|O_CREAT, 0666); /* BigFile- */
        if(fd<0) {
                printf("Fail to create '%s' for RDWR!", fpath);
		exit(EXIT_FAILURE);
        }

        /* Create UDP client */
        EGI_UDP_CLIT *uclit=inet_create_udpClient(svrAddr, port, cltAddr, 4);
        if(uclit==NULL)
                exit(EXIT_FAILURE);

        /* Set callback */
        uclit->callback=Client_Callback; //inet_udpClient_TESTcallback;

        /* Process UDP server routine */
        inet_udpClient_routine(uclit);

        /* Free and destroy */
        inet_destroy_udpClient(&uclit);
	close(fd);
  }

	free(svrAddr);
	free(cltAddr);
        return 0;
}


/*----------------------------------------------------------------------
A callback routine for UPD server.

Note:
If *sndSize NOT re_assigned to a positive value, server rountine will NOT
proceed to sendto().

@rcvAddr:       Client address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndAddr:       Client address, to which sndBuff will be sent.
@sndBuff:       Data to send to sndAddr
@sndSize:       Data size of sndBuff

Return:
        0       OK
        <0      Fails, the server_routine() will NOT proceed to sendto().
	=1	No client.
------------------------------------------------------------------------*/
int Server_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                           		struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize)
{
	int data_off=0;

	/* Note: *sndSize preset to -1 in routine() */

	/* 1. Check and parse receive data  */
	if(rcvSize>0) {
		if(verbose_on)
			printf("Received %d bytes data.\n", rcvSize);

		/* Convert rcvData to IPACK, which is just a reference */
		ipack_rcv=(EGI_INETPACK *)rcvData;
		header_rcv=IPACK_HEADER(ipack_rcv);

		if(header_rcv==NULL) {
			printf("header_rcv is NULL!\n");
			return -1;
		}

		/* Parse CODE */
		switch(header_rcv->code) {
			case CODE_REQUEST:
				/* Get clientAddr, and reset pack_seq */
				printf("Client requests for file transfer!\n");
				if( pack_seq==pack_total ) {  /* pack_seq inited as pack_total */
					pack_seq=0;
					clientAddr=*rcvAddr;
				}
				else
					printf("Transmission already started!\n");
				break;
			case CODE_FINISH:   /* Finish, clear clientAddr */
				printf("Client end session!\n");
				bzero(&clientAddr,sizeof(clientAddr));
				*sndAddr=clientAddr;

				/* reset pack_seq */
				pack_seq=pack_total;

				/* End routine */
				// *cmdcode=UDPCMD_END_ROUTINE;
				break;
			default:
				return -1;
				break;
		}
		/* Proceed on ...*/
	}
	else if(rcvSize==0) {
		/* A Client probe from EGI_UDP_CLIT means a client is created! */
		printf("Client Probe from: '%s:%d'.\n", inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port));

		/* nothing to send */
		//return 0;

		/* Proceed on... */
	}
	else  {  /* rcvSize<0 */
		/* Proceed on... */
		// printf("rcvSize<0\n");
		//return -1; to  Proceed on...
	}


	/* 2. If NO client */
	if( clientAddr.sin_addr.s_addr ==0 )
		return 1;

	/* 3. Carry on request job */
	if( pack_seq < pack_total ) {

		/* Nest IPACK into sndBuff, fill in header info. */
		ipack_snd=(EGI_INETPACK *)sndBuff;
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
		header_snd=IPACK_HEADER(ipack_snd);
		header_snd->code=CODE_DATA;
		header_snd->seq=pack_seq;
		header_snd->total=pack_total;

		/* Load data into ipack_snd */
		data_off=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);
		if( pack_seq != pack_total-1) {
			/* Set packsize before loadData */
			ipack_snd->packsize=data_off+datasize;
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap->fp+pack_seq*datasize, datasize)!=0 )
				return -1;
		}
		else {
			/* Set packsize before loadData */
			ipack_snd->packsize=data_off+tailsize;
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap->fp+pack_seq*datasize, tailsize)!=0 )
				return -1;
		}

		/* Set sndAddr and sndsize */
		*sndAddr=clientAddr;
		*sndSize=ipack_snd->packsize;

		/* Increase pack_seq, as next sequence number. */
		printf("Sent out pack[%d](%dBs), of total %d packs.\n", pack_seq, ipack_snd->packsize, pack_total);
		pack_seq++;

	}
	/* Finish, clear clientAddr */
	else if(pack_seq==pack_total) {
			printf("Ok, all packs sent out!\n");
			bzero(&clientAddr,sizeof(clientAddr));
	                *sndAddr=clientAddr;
	}

	return 0;
}

/*-----------------------------------------------------------------------
A callback routine for UPD client.

Note:
If *sndSize NOT re_assigned to a positive value, server rountine will NOT
proceed to sendto().

@rcvAddr:       Server/Sender address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndBuff:       Data to send to UDP server
@sndSize:       Data size of sndBuff

Return:
        0       OK
        <0      Fails
        <0      Fails, the client_routine() will NOT proceed to sendto().
-----------------------------------------------------------------------*/
int Client_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
	                                                              char *sndBuff, int *sndSize )
{
	int size;
	int nwrite;

	/* Check pack_total first */
	if( pack_total==0 ) {
		/* Nest IPACK into sndBuff, fill in header info. */
		void *ptmp=sndBuff;
		ipack_snd=(EGI_INETPACK *)ptmp;
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
		header_snd=IPACK_HEADER(ipack_snd);
		header_snd->code=CODE_REQUEST;

		/* Update IPACK packsize, just a header! */
		ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);

		/* Set sndsize */
		*sndSize=ipack_snd->packsize;
		printf("Finish request ipack.\n");
	}

	/* Receive file data */
	if( rcvSize > 0 ) {
		if(verbose_on)
			printf("Received %d bytes data.\n", rcvSize);

		/* Convert rcvData to IPACK, which is just a reference */
		ipack_rcv=(EGI_INETPACK *)rcvData;
		header_rcv=IPACK_HEADER(ipack_rcv);

		/* Parse CODE */
		if(header_rcv==NULL) {
			printf("header_rcv is NULL!\n");
			return -1;
		}

		/* Parse CODE */
		switch(header_rcv->code) {
			case CODE_DATA:
				/* Receive file data */
				printf("Receive pack[%d](%dBs), of total %d packs.\n",
								header_rcv->seq, ipack_rcv->packsize, header_rcv->total);
				/* Check sequence number */
				if(  header_rcv->seq != pack_total ) {
					printf("Sequence number error! pack_seq=%d, expect seq=%d, Sequence number error!\n",
							header_rcv->seq, pack_total );
					exit(EXIT_FAILURE);
					// *cmdcode=UDPCMD_END_ROUTINE;
				}

	                	/* Write to file */
				size=IPACK_PRIVDATASIZE(ipack_rcv);
		                nwrite=write(fd, ipack_rcv->data, size);
                		if(nwrite!=size) {
		                        if(nwrite<0)
                		                printf("write() ipack_rcv: Err'%s'", strerror(errno));
                        		else
                                		printf("Short write to file!, nwrite=%d of %d\n", nwrite, size);
				}

				/* Increase pack_total, also as next sequence number */
				pack_total++;

				/* Check pack_total */
				if(pack_total==header_rcv->total) {
					printf("Finish receive file!\n");

					/* Feed back to server */
					/* Nest IPACK into sndBuff, fill in header info. */
					ipack_snd=(EGI_INETPACK *)sndBuff;
					header_snd=IPACK_HEADER(ipack_snd);
					header_snd->code=CODE_FINISH;

					/* Update IPACK packsize */
					ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);

					/* Set sndsize */
					*sndSize=ipack_snd->packsize;

					/* End routine */
					*cmdcode=UDPCMD_END_ROUTINE;
				}
				break;
			case CODE_FINISH:
				break;
			default:
				break;

		   } /* END switch */

	}
	else if( rcvSize == 0 ) {
		printf("rcvSize==0\n");
		printf("rcvSize==0 from: '%s:%d'.\n", inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port));
	}
	else  { /* rcvSize <0 */
		//printf("rcvSize<0\n");
		//return -1;  To proceed on, Do not stop next sendto()!
	}

	return 0;
}

