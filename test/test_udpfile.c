/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program of tranferring a file under UDP protocol.
For one Server one Client scenario.

Usage Example:
	./test_udpfile -s -p 8765		( As server )
	./test_udp -c -a 192.168.9.9:8765	( As client )

	( Use test_graph.c to show net interface traffic speed )

Note:
0. Packet size is controlled by the UDP Server.

1. The UDP Server routine just dump all data into knernel to send, and
   will NOT check/confirm whether they finally reach to the Client or lost.

2. Once receives Client's request, the UDP server send out all packs in sequence,
   while the UDP client may fail and break if any pack is lost, then a
   round of request/receive process will start again.

   (If IPACKs reach to the Client in wrong sequence, ---OK! Just memcpy
    them to the corresponding position in the EGI_FILEMMAP.)
   Examples:
   "Sequence number error! pack_seq=32, expect seq=31, Sequence number error!"

3. Big packet_size and small send_waitus will be more probable to cause packets drop
   and reach to Client with disordered sequence.
   Two directly connected nodes will be more likely to cause packets drop and out of sequence,
   comparing with two indirectly connected nodes? Only if waitus is too small.

4. If file size is very big and the Server is busy sending data, while the
   Client is failed and trying to sendto() the server, in this case, the Client will
   probably be BLOCKed longtime in sendto()! for the whole link is jammed
   with data from the Server?
   Need to increase send_waitus, or choose smaller packet size.

5. --- !!! WARNING !!! ---
   Select approriate waitus(spleep time) for different packsize. big waitus value
   for big packsize.
   Ajust send_waitus to change packet dispacthing speed at Server side, and
   to relax traffic pressure through the inet link. it helps to avoid packets drops(lost)
   and out of sequence.

6. Test results:  ( for a 14Mby test file )
   Select small packet: slow, stable, more probable to complete.
   Select big packet:   fast, unstable, more probable to break midway,
			and to jam the link.

TODO:
1. If the client detects lost/disordered packet, pause there and request
   the Server to re_send packets starting from the lost one. not just discard
   all packets and start a new round.
2. MMAP a null file and expand it to file size, then put received packet data
   to right position according to its seq number.

		( ---- WiFi --- )

 packsize: 25620Bs
   	Server set: snd_waitus=10ms		<<<------ Default ----
	Avg speed: ~2.0MBps

   	Server set: snd_waitus=5ms
	Avg speed: ~3.6MBps
	TRYs: 32,  FAILs: 2, TimeOuts: 0

   	Server set: snd_waitus=2ms
	Avg speed: 0-7MBps, chopping! sometime sustained. too many FAILs!
	TRYs: 56,  FAILs: 23, TimeOuts: 0

 packsize: 51220Bs
   	Server set: snd_waitus=20ms
	Avg speed: ~2.1MBps
	TRYs: 6539,  FAILs: 305, TimeOuts: 2  (FAIL: disorder/lost)

   	Server set: snd_waitus=10ms		<<<------- OK, cpu 70% idle. ----
	Avg speed: ~3.7MBps
	TRYs: 120,  FAILs: 2, TimeOuts: 0
	TRYs: 5259,  FAILs: 139, TimeOuts: 2

   	Server set: snd_waitus=5ms
	Avg speed:  0-6.5MBps, chopping! too many FAILs!
	TRYs: 49,  FAILs: 24, TimeOuts: 0

   	Server set: snd_waitus=2ms
	Avg speed:  0-11MBps, chopping!  Totally FAILs !!!
	TRYs: 102,  FAILs: 101, TimeOuts: 0

		( --- Eth0 --- : PC Server ---eth0---> widoraNEO -->widoraNEO  )

 packsize: 51220Bs
        Server set: snd_waitus=20ms
        Avg speed: ~2.4MBps
	TRYs: 42,  FAILs: 2, TimeOuts: 0

   	Server set: snd_waitus=10ms
	Avg speed: ~4.7MBps
	TRYs: 128,  FAILs: 0, TimeOuts: 0


Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
--------------------------------------------------------------------------*/
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


const char *fpath="udp_test.dat";

EGI_FILEMMAP *fmap_snd;   /* For server: send file. */
EGI_FILEMMAP *fmap_rcv;   /* For Client: received file. */
int fd;	      		  /* For client: save file. */

/* A private header for my IPACK */
typedef struct {
	int		code;
	unsigned int	seq;	/* Packet sequence number, from 0 */
	unsigned int	total;  /* Total packets */
	off_t		fsize;	/* Total size of the file */
	//unsigned long long all_size
#define CODE_NONE	0	/* none */
#define CODE_DATA	1	/* Data availabe, seq/total */
#define	CODE_REQUEST	2	/* Client ask for file stransfer */
#define CODE_FINISH	3	/* Client ends session */
#define CODE_FAIL	4	/* Client fails to receive packets */
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

/* privdata size in each IPACK. MUST <= EGI_MAX_UDP_PDATA_SIZE-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER)
 * For Server ONLY
 */
//static unsigned int datasize=1448-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER); // 1024*4;
static unsigned int datasize=1024*25;   /* Server: set avg privdata size.  Client: recevied ipack privdata size. */
static unsigned int tailsize;		/* For server: Defined as size of last pack, when pack_seq == pack_total-1 */
static unsigned int pack_seq;	   	/* For server: Sequence number for current IPACK */
static unsigned int pack_total;    	/* For server: Total number of IPACKs */
static unsigned int pack_count;		/* For client */

EGI_BITSTATUS *ebits;			/* To record  packet squence number */

static bool verbose_on;

int cnt_disorders;
int cnt_trys;
int cnt_tmouts;

/* Print help */
void print_help(const char* cmd)
{
	printf("Usage: %s [-hscva:p:m:d:] \n", cmd);
       	printf("        -h   help \n");
   	printf("        -s   work as UDP Server\n");
   	printf("        -c   work as UDP Client (default)\n");
	printf("	-v   verbose to print rcvSize\n");
	printf("	-a:  Server IP address, with or without port.\n");
	printf("	-y:  Client IP address, default NULL.\n");
	printf("	-p:  Port number, default 5678.\n");
	printf("	-d:  size of pridata packed in each IPACK, default 25kBs.\n");
	printf("	-w:  Server send_waitus in us. defautl 10ms.\n");
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
	unsigned short int port=5678;
	unsigned int send_waitus=10000;	/* in us, For server */
	char *pt;

#if  0 /* TEST: ------ EGI_BITSTAUTS ---------- */
	EGI_BITSTATUS *ebits=egi_bitstatus_create(1024);
	egi_bitstatus_print(ebits);
	printf("Total %d ZERO bits\n", egi_bitstatus_count_zeros(ebits));
	egi_bitstatus_setall(ebits);
	printf("Total %d ONE bits\n", egi_bitstatus_count_ones(ebits));
	egi_bitstatus_reset(ebits, 32);
	egi_bitstatus_reset(ebits, 555);
	printf("octbits[555]=%d\n",egi_bitstatus_getval(ebits,555));
	egi_bitstatus_print(ebits);

	printf("Zero bits index: ");
	ebits->pos=-1;
	while( egi_bitstatus_posnext_zero(ebits)>=0 )
		printf("[%d] ",ebits->pos);
	printf("\n");

	exit(0);
#endif

#if  0  /* TEST: ------ EGI_FILEMMAP ---------- */
	EGI_FILEMMAP *fmap_dat=egi_fmap_create("/tmp/fmap.dat", 9867);
	egi_fmap_free(&fmap_dat);
	exit(0);
#endif /* --------*/

	/* Parse input option */
	while( (opt=getopt(argc,argv,"hscva:y:p:m:d:w:"))!=-1 ) {
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
				/* If detects delimiter ':' */
				if((pt=strstr(optarg,":"))!=NULL) {
					port=atoi(pt+1);
					*pt='\0';
					svrAddr=strdup(optarg);
					printf("Input server address: '%s:%d'\n",svrAddr, port);
				}
				else {
					svrAddr=strdup(optarg);
					printf("Input server address='%s'\n",svrAddr);
				}
				break;
			case 'y':
				cltAddr=strdup(optarg);
				printf("Input client address='%s'\n",cltAddr);
				break;
			case 'p':
				port=atoi(optarg);
				break;
			case 'd':
				datasize=atoi(optarg);
				if(datasize<1448-40) datasize=1448-40;
				else if(datasize>EGI_MAX_UDP_PDATA_SIZE-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER))
					datasize=EGI_MAX_UDP_PDATA_SIZE-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER);
				break;
			case 'w':
				send_waitus=atoi(optarg);
				break;
			default:
				SetUDPServer=false;
				break;
		}
	}

	if(port==0) {
		printf("Please set the port number!\n");
		print_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("Set port=%d\n",port);
	printf("Set datasize=%d(Bytes)\n", datasize);
	printf("Set send_waitus=%d(us) for the Server.\n", send_waitus);

  /* A. -------- Work as a UDP Server --------- */
  if( SetUDPServer ) {
	printf("Set as UDP Server, set port=%d \n", port);

        /* Mmap file and get total_packs for transmitting. */
        fmap_snd=egi_fmap_create(fpath, 0);
        if(fmap_snd==NULL) {
		exit(EXIT_FAILURE);
        }
	pack_total=fmap_snd->fsize/datasize;
	tailsize=fmap_snd->fsize - pack_total*datasize;
	if(tailsize)
		pack_total +=1;
	else
		tailsize=datasize;

	/* Preset pack_seq=pack_total, as for holdon. */
	pack_seq=pack_total;

        /* Create UDP server */
        EGI_UDP_SERV *userv=inet_create_udpServer(svrAddr, port, 4);
        if(userv==NULL)
                exit(EXIT_FAILURE);

	/* Set waitus */
	userv->idle_waitus=100000; 	/* waitus */
	userv->send_waitus=send_waitus; /* waitus */

        /* Set callback */
        userv->callback=Server_Callback;

        /* Process UDP server routine */
        inet_udpServer_routine(userv);

        /* Free and destroy */
        inet_destroy_udpServer(&userv);
	egi_fmap_free(&fmap_snd);
  }


  /* B. -------- Work as a UDP Client --------- */
  else {
	printf("Set as UDP Client, targets to server at %s:%d. \n", svrAddr, port);

	if( svrAddr==NULL || port==0 ) {
		printf("Please provide Server IP address and port number!\n");
		print_help(argv[0]);
		exit(EXIT_FAILURE);
	}

     while(1) { ////////////////    LOOP  TEST    /////////////////

///////////////////////////* Open file for write *//////////
//      fd=open(fpath, O_RDWR|O_CREAT, 0666); /* BigFile- */
//	if(fd<0) {
//	printf("Fail to create '%s' for RDWR!", fpath);
//		exit(EXIT_FAILURE);
//	}
////////////////////////////////////////////////////////////

        /* Create UDP client */
        EGI_UDP_CLIT *uclit=inet_create_udpClient(svrAddr, port, cltAddr, 4);
        if(uclit==NULL)
                exit(EXIT_FAILURE);

	/* Set timeout, note UDP client works in BLOCKING mode */
	inet_sock_setTimeOut(uclit->sockfd, 3, 0, 3, 0);

	/* Set waitus */
	uclit->idle_waitus=10000; /* waitus */
	uclit->send_waitus=10000; /* waitus */

        /* Set callback */
        uclit->callback=Client_Callback;

        /* Process UDP server routine */
        inet_udpClient_routine(uclit);

        /* Free and destroy */
        inet_destroy_udpClient(&uclit);
	close(fd);
	egi_fmap_free(&fmap_rcv);
	egi_bitstatus_free(&ebits);

	/* sum up */
	cnt_trys++;
	printf("\n--- TRYs: %d,  DISORDERs: %d, TimeOuts: %d ---\n", cnt_trys, cnt_disorders, cnt_tmouts);

     }	  /////////////    END LOOP  TEST    //////////////

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
	=1	No client.
        0       OK
		If !=0, the server_routine() will NOT proceed to sendto().
        <0      Fails, the server_routine() will NOT proceed to sendto().
		!!! WARNING !!!
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
			case CODE_FAIL:
				printf("Client fail to recevie!\n");
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
				break;
		}
		/* Proceed on ...*/
	}
	else if(rcvSize==0) {
		/* A Client probe from EGI_UDP_CLIT means a client is created! */
		printf("Client Probe from: '%s:%d'.\n", inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port));

		/* DO NOT return, need to proceed on... */
	}
	else  {  /* rcvSize<0 */
		// printf("rcvSize<0\n");

		/* DO NOT return, need to proceed on... */
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
		header_snd->fsize=fmap_snd->fsize;

		/* Load data into ipack_snd */
		data_off=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);
		if( pack_seq != pack_total-1) {
			/* Set packsize before loadData */
			ipack_snd->packsize=data_off+datasize;
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap_snd->fp+pack_seq*datasize, datasize)!=0 )
				return -1;
		}
		else {
			/* Set packsize before loadData */
			ipack_snd->packsize=data_off+tailsize;
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap_snd->fp+pack_seq*datasize, tailsize)!=0 )
				return -1;
		}

		/* Set sndAddr and sndsize */
		*sndAddr=clientAddr;
		*sndSize=ipack_snd->packsize;

		/* Print */
		printf("Sent out pack[%d](%dBs), of total %d packs.\r", pack_seq, ipack_snd->packsize, pack_total);
		fflush(stdout);

		/* Increase pack_seq, as next sequence number. */
		pack_seq++;

	}
	/* 4. Finish, clear clientAddr */
	else if(pack_seq==pack_total) {
			printf("\nOk, all packs sent out!\n");
			bzero(&clientAddr,sizeof(clientAddr));
	                *sndAddr=clientAddr;
	}

	return 0;
}


/*-----------------------------------------------------------------------
A callback routine for UPD client.
To requeset and receive a file from the server.

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
        <0      Fails, the client_routine() will NOT proceed to sendto().
		!!! WARING !!!
-----------------------------------------------------------------------*/
int Client_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
	                                                              char *sndBuff, int *sndSize )
{
	int size;
	int nwrite;

	/* 1. Start request IPACK. Usually this will be sent twice before receive reply from the Server.  */
	if( pack_count==0 ) {
		/* Nest IPACK into sndBuff, fill in header info. */
		//void *ptmp=sndBuff;
		ipack_snd=(EGI_INETPACK *)sndBuff;
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
		header_snd=IPACK_HEADER(ipack_snd);
		header_snd->code=CODE_REQUEST;

		/* Update IPACK packsize, just a header! */
		ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);

		/* Set sndsize */
		*sndSize=ipack_snd->packsize;
		printf("Finish preping request ipack.\n");

		/* DO NOT return here, need to proceed on to receive reply/data next! */
	}

	/* 2. Receive file data */
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
				/* 1. Receive file data */
				printf("Receive pack[%d](%dBs), of total %d packs, ~%jd KBytes.\r",
						header_rcv->seq, ipack_rcv->packsize, header_rcv->total, header_rcv->fsize>>10);
				fflush(stdout);

				/* 2. Create fmap_rcv and ebits */
				if(fmap_rcv==NULL) {
					fmap_rcv=egi_fmap_create(fpath, header_rcv->fsize); /* create and/or resize */
					if(fmap_rcv==NULL) exit(EXIT_FAILURE);
					printf("Create fmap_rcv OK.\n");
				}
				if( ebits==NULL ) {
					ebits=egi_bitstatus_create(header_rcv->total);
					if(ebits==NULL) exit(EXIT_FAILURE);
					printf("Create ebits OK.\n");
				}

				/* 3. Error Checking: sequence number*/
				if(  header_rcv->seq != pack_count ) {
					printf("\nOut of sequence! pack_seq=%d, expect seq=%d.\n",
							header_rcv->seq, pack_count );
					/* count fails */
					cnt_disorders ++;

/////////////////////////////////////////////////* If sequence order wrong, then end routine */////////////
//					/* If fails, need to feed back to server, to stop Server sending,  */
//					/* Nest IPACK into sndBuff, fill in header info. */
//					ipack_snd=(EGI_INETPACK *)sndBuff;
//					header_snd=IPACK_HEADER(ipack_snd);
//					header_snd->code=CODE_FAIL;
//					/* Update IPACK packsize */
//					ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);
//
//					/* Set sndsize */
//					*sndSize=ipack_snd->packsize;
//
//					/* Reset pack_count and cmdcode to end routine */
//					pack_count=0;
//					*cmdcode=UDPCMD_END_ROUTINE;
//
//					return 0;
////////////////////////////////////////////////////////////////////////////////////////////////////////

				}

				/* 4. Check ebits for sequence number */
				if( egi_bitstatus_getval(ebits, header_rcv->seq) ) {
					printf("Ipack[%d] already received, skip it!\n",header_rcv->seq);
					break;
				}

				/* 5. Write to file MMAP, position as per the sequence number. */
				datasize=IPACK_PRIVDATASIZE(ipack_rcv);
				if(pack_count != header_rcv->total-1 ) {   /* Not tail, packsize is the same. */
					memcpy( fmap_rcv->fp + header_rcv->seq * datasize,
						IPACK_PRIVDATA(ipack_rcv),
						datasize
					      );
				}
				else {	/* Write the tail */
					memcpy( fmap_rcv->fp + header_rcv->fsize - datasize,
						IPACK_PRIVDATA(ipack_rcv),
						datasize
					      );
				}


//////////////////////////////////////*  Write to file *///////////////////////////////////////////////////////
//				size=IPACK_PRIVDATASIZE(ipack_rcv);
//		                nwrite=write(fd, IPACK_PRIVDATA(ipack_rcv), size);
//               		if(nwrite!=size) {
//		                        if(nwrite<0)
//                		                printf("write() ipack_rcv: Err'%s'", strerror(errno));
//                        		else
//                                		printf("Short write to file!, nwrite=%d of %d\n", nwrite, size);
//				}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


				/* 6. Recorder seq number as index to ebits */
				egi_bitstatus_set(ebits, header_rcv->seq);
				/* Increase pack_count, also as next ref. sequence number */
				pack_count++;

				/* 7. Check pack_count */
				if(pack_count==header_rcv->total) {
					printf("\n $$$ Successfully received the file with total %d packs!\n", pack_count);

					/* Feed back to server */
					/* Nest IPACK into sndBuff, fill in header info. */
					ipack_snd=(EGI_INETPACK *)sndBuff;
					header_snd=IPACK_HEADER(ipack_snd);
					header_snd->code=CODE_FINISH;
					/* Update IPACK packsize */
					ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);
					/* Set sndsize */
					*sndSize=ipack_snd->packsize;

					/* Reset pack_count amd cmdcode to end routine */
					pack_count=0;
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
		printf("rcvSize==0 from: '%s:%d'.\n", inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port));
	}
	else  { /* rcvSize <0, recvfrom() ERR! */

		/* DO NOT return -1 here; need to proceed to sendto()! */

		/*  Timeout: errno=EAGAIN or ECONNREFUSED */
		if( pack_count>0 ) {
			printf("Recv Timeout! abort session!\n");
			cnt_tmouts++;

			/* Print unreceived ipack seq number */
			egi_bitstatus_print(ebits);
			printf("Ipacks with follow seq numbers not received: ");
			ebits->pos=-1;
			while( egi_bitstatus_posnext_zero(ebits) ==0 )
				printf("[%d] ",ebits->pos);
			printf("\n press a key to continue.\n");
			getchar();

			/* Reset pack_total amd cmdcode to end routine */
			pack_count=0;
			*cmdcode=UDPCMD_END_ROUTINE;
		}
	}

	return 0;
}

