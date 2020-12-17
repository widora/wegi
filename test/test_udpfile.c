/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program of tranferring a file under UDP protocol.
For one Server one Client scenario.

Usage Example:
         < Default test file: udp_test.dat >
	./test_udpfile -s -p 8765		( As server )
	./test_udp -c -a 192.168.9.9:8765	( As client )

	( Use test_graph.c to show net interface traffic speed )

File size limit: < 2GBs ( File MMAP Limit )

Note:
0. Packet size is controlled by the UDP Server.

1. The UDP Server routine just dump all data into knernel to send, and
   will NOT check/confirm whether they finally reach to the Client or lost.

2. Once receives Client's request, the UDP server send out all packs in sequence,
   while the UDP client may fail and break if any pack is lost, then a
   round of request/receive process will start again.  ----- OK, lost_pack retransmission.

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

			--- Lost Pack Retransmission ---

6. The client will request for any lost packs before complete the transfer session.
   In this case, a rather smaller snd_waitus maybe set for Server,  and a smaller rcv_TIMEOUT
   value for Client (for pack rcvfrom() timeout).
   However, you have to balance between transfer speed and error rate.

7. Transmitting speed is restricted by the weakest (most disadvantageous/limited)
   node/element/factor/ considered in the whole data link,
   Example:
   1. For micrSD IO(write/read) speed limit, you may set a larger buffer to improve
   inet send/recv speed.
   2. Any glitch of the system(temp. breakout of SD IO operation etc.) may cause losts of lost packs.

TODO:
   1. Mutual authentication.
   2. Lost_pack/Speed tune.
   3. For a big file, egi_fmap_create() may consume lots of time??! --- Because of a bad or low_qulity miniSD card!!!
      During that period, the server will have thrown out lots of packs, which may block the inet link.
      use write()+buffer instead.
      Or let the Server wait for the Client to finish it first.

 Test results:  ( for a 14Mbytes test file )
   Select small packet and big waitus:   slow, stable, more probable to complete.
   Select big packet and samll waitus:   fast, unstable, more probable to break midway,
					 and more likely to jam the link.

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


 ./test_udpfile -s -d 52000 -w 10000
	UDP transfer: Round 1,  Retrys: 4/284,  TimeOuts: 0, Time: 4409ms
	UDP transfer: Round 1,  Retrys: 4/284,  TimeOuts: 0, Time: 4376ms
	UDP transfer: Round 1,  Retrys: 4/284,  TimeOuts: 0, Time: 4371ms

 ./test_udpfile -s -d 52000 -w 5000	<-----  OK ------
	UDP transfer: Round 1,  Retrys: 3/284,  TimeOuts: 0, Time: 2937ms
	UDP transfer: Round 1,  Retrys: 0/284,  TimeOuts: 0, Time: 2929ms
	UDP transfer: Round 1,  Retrys: 2/284,  TimeOuts: 0, Time: 2957ms

 ./test_udpfile -s -d 52000 -w 3000  ( Too many lost packs)
	UDP transfer: Round 1,  Retrys: 73/284,  TimeOuts: 0, Time: 2925ms
	UDP transfer: Round 1,  Retrys: 34/284,  TimeOuts: 0, Time: 2635ms
	UDP transfer: Round 1,  Retrys: 259/284,  TimeOuts: 0, Time: 5509ms

 ./test_udpfile -s -d 52000 -w 2000  ( Too many lost packs)
	UDP transfer: Round 1,  Retrys: 276/284,  TimeOuts: 0, Time: 5653ms
	UDP transfer: Round 1,  Retrys: 1/284,  TimeOuts: 0, Time: 2001ms
	UDP transfer: Round 1,  Retrys: 274/284,  TimeOuts: 0, Time: 5843ms
	UDP transfer: Round 1,  Retrys: 186/284,  TimeOuts: 0, Time: 4359ms
	UDP transfer: Round 1,  Retrys: 181/284,  TimeOuts: 0, Time: 4394ms


		( --- Eth0 --- : PC Server ---eth0---> widoraNEO -->widoraNEO  )

 packsize: 51220Bs
        Server set: snd_waitus=20ms
        Avg speed: ~2.4MBps
	TRYs: 42,  FAILs: 2, TimeOuts: 0

   	Server set: snd_waitus=10ms
	Avg speed: ~4.7MBps
	TRYs: 128,  FAILs: 0, TimeOuts: 0

		( --- Eth0 --- : PC Client <---eth0--- widoraNEO   )

 ./test_udpfile -s -d 52000 -w 5000
	UDP transfer: Round 1,  Retrys: 3/284,  TimeOuts: 0, Time: 2612ms
	UDP transfer: Round 1,  Retrys: 3/284,  TimeOuts: 0, Time: 2618ms
	UDP transfer: Round 1,  Retrys: 2/284,  TimeOuts: 0, Time: 2511ms

 ./test_udpfile -s -d 52000 -w 2000
	UDP transfer: Round 1,  Retrys: 2/284,  TimeOuts: 0, Time: 1730ms
	UDP transfer: Round 1,  Retrys: 1/284,  TimeOuts: 0, Time: 1611ms
	UDP transfer: Round 1,  Retrys: 1/284,  TimeOuts: 0, Time: 1628ms

 ./test_udpfile -s -d 52000 -w 1000    <-------- OK -----
	UDP transfer: Round 1,  Retrys: 3/284,  TimeOuts: 0, Time: 1350ms
	UDP transfer: Round 1,  Retrys: 1/284,  TimeOuts: 0, Time: 1375ms
	UDP transfer: Round 1,  Retrys: 1/284,  TimeOuts: 0, Time: 1339ms

 ./test_udpfile -s -d 52000 -w 500    ( Too many lost packs)
	UDP transfer: Round 1,  Retrys: 278/284,  TimeOuts: 0, Time: 4102ms
	UDP transfer: Round 1,  Retrys: 277/284,  TimeOuts: 0, Time: 4086ms
	UDP transfer: Round 1,  Retrys: 21/284,  TimeOuts: 0, Time: 1510ms

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
#include <egi_debug.h>

#undef  DEFAULT_DBG_FLAGS
#define DEFAULT_DBG_FLAGS   DBG_NONE //DBG_TEST

const char *fpath="udp_test.dat";

EGI_FILEMMAP *fmap_snd;   /* For server: send file. */
EGI_FILEMMAP *fmap_rcv;   /* For Client: received file. */

/* A private header for my IPACK */
typedef struct {
	int		code;
	unsigned int	seq;	/* Packet sequence number, from 0 */
	unsigned int	total;  /* Total packets */
	off_t		fsize;	/* Total size of the file, which is divided into packs for transmission */
	int		stamp;  /* signature, stamp */
	//unsigned long long all_size
#define CODE_NONE		0	/* none */
#define CODE_DATA		1	/* Data availabe, seq/total, data in privdata */
#define	CODE_REQUEST_FILE	2	/* Client ask for file stransfer */
#define CODE_REQUEST_PACK	3	/* Client ask for lost packs, ipack seq number in privdata */
#define CODE_FINISH		4	/* Client ends session */
#define CODE_FAIL		5	/* Client fails to receive packets */
} PRIV_HEADER;

/* Following pointers to be used as reference only!
 * They'll be nested into allocated send/recv buffer.
 */
PRIV_HEADER  *header_rcv;
PRIV_HEADER  *header_snd;
EGI_INETPACK *ipack_rcv;
EGI_INETPACK *ipack_snd;

struct sockaddr_in  clientAddr;
struct sockaddr_in  serverAddr;

/* UDP packet information */
//UNUSED: static unsigned int packsize;    	/* 1448,  packet size */

/* privdata size in each IPACK. MUST <= EGI_MAX_UDP_PDATA_SIZE-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER)
 * For Server ONLY
 */
//static unsigned int datasize=1448-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER); // 1024*4;
static unsigned int datasize=1024*25;   /* Server: set avg privdata size.  Client: recevied ipack privdata size. */
static unsigned int tailsize;		/* For server: Defined as size of last pack, when pack_seq == pack_total-1 */
static unsigned int pack_seq;	   	/* For server: Sequence number for current IPACK */
static unsigned int pack_total;    	/* For server/client: Total number of IPACKs for transfering a file */
static unsigned int pack_count;		/* For client */
static unsigned int lost_seq;		/* Lost pack seq number */

EGI_BITSTATUS *ebits;			/* To record  packet squence number */

static bool verbose_on;

int cnt_disorders;			/* disorder packs */
int cnt_rounds;				/* Rounds of transmission/session. */
int cnt_retrys;				/* Retrys for lost packs */
int cnt_tmouts;				/* Client recvfrom() Timeout */
long cnt_tmms;				/* Time cost, in ms */

/* Print help */
void print_help(const char* cmd)
{
	printf("Usage: %s [-hSCva:p:m:d:s:l:] \n", cmd);
       	printf("        -h   help, default file is '%s'. \n", fpath);
   	printf("        -S   work as UDP Server\n");
   	printf("        -C   work as UDP Client (default)\n");
	printf("	-v   verbose\n");
	printf("	-a:  Server IP address, with or without port.\n");
	printf("	-y:  Client IP address, default NULL.\n");
	printf("	-p:  Port number, default 5678.\n");
	printf("	-d:  Size of pridata packed in each IPACK, default 25kBs.\n");
	printf("	-s:  send_waitus in us. defautl 10ms.\n");
	printf("	-r:  recv_waitus in us. defautl 0ms.\n");
	printf("	-l:  Test loop times, default 1. If <=0, nonstop. \n");
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
	bool ever_loop=false;
	int  nloops=1;
	bool SetUDPServer=false;
	char *svrAddr=NULL;		/* Default NULL, to be decided by system */
	char *cltAddr=NULL;
	unsigned short int port=5678;
	unsigned int send_waitus=10000;	/* in us */
	unsigned int recv_waitus=0;	/* in us */
	char *pt;
	EGI_CLOCK eclock={0};
	unsigned int pack_shellsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);




#if 0 /* TEST: ------ EGI_BITSTAUTS ---------- */
	int p;

	EGI_BITSTATUS *ebits=egi_bitstatus_create(1024);
	egi_bitstatus_print(ebits);
	printf("Total %d ZERO bits\n", egi_bitstatus_count_zeros(ebits));

	printf("Set ONE at pos=555\n");
	egi_bitstatus_set(ebits, 555);
	egi_bitstatus_posfirst_one(ebits, -10);
	printf("First ONE at pos=%d\n", ebits->pos);

	egi_bitstatus_setall(ebits);

	printf("Total %d ONE bits\n", egi_bitstatus_count_ones(ebits));

	egi_bitstatus_reset(ebits, 32);


	egi_bitstatus_posfirst_zero(ebits, 0);
	printf("From pos=0, First ZERO at pos=%d\n", ebits->pos);

	egi_bitstatus_reset(ebits, 555);
	printf("octbits[555]=%d\n",egi_bitstatus_getval(ebits,555));
	egi_bitstatus_print(ebits);

	egi_bitstatus_posfirst_zero(ebits, 32+1);
	printf("From pos=33, First ZERO at pos=%d\n", ebits->pos);

	printf("Zero bits index: ");
	p=0;
	while( egi_bitstatus_posfirst_zero(ebits,p)==0 ) {
		printf("[%d]  ",ebits->pos);
		p=ebits->pos+1;
	}
	printf("\n");

	exit(0);
#endif

	/* Parse input option */
	while( (opt=getopt(argc,argv,"hSCva:y:p:m:d:s:r:l:"))!=-1 ) {
		switch(opt) {
			case 'h':
				print_help(argv[0]);
				exit(0);
				break;
			case 'S':
				SetUDPServer=true;
				break;
			case 'C':
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
				if(datasize<1448-pack_shellsize)
					datasize=1448-pack_shellsize;
				else if(datasize>EGI_MAX_UDP_PDATA_SIZE-pack_shellsize) {
					datasize=EGI_MAX_UDP_PDATA_SIZE-pack_shellsize;
					printf("UDP packet size exceeds limit of %dBs, reset datasize=%d-%d(pack_shellsize)=%dBs.\n",
						EGI_MAX_UDP_PDATA_SIZE, EGI_MAX_UDP_PDATA_SIZE, pack_shellsize, datasize);
				}
				break;
			case 's':
				send_waitus=atoi(optarg);
				break;
			case 'r':
				recv_waitus=atoi(optarg);
				break;
			case 'l':
				nloops=atoi(optarg);
				if(nloops<=0)
					ever_loop=true;
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
	if(SetUDPServer)
		printf("Set datasize=%d(Bytes)\n", datasize);
	printf("Set send_waitus=%d(us).\n", send_waitus);
	printf("Set recv_waitus=%d(us).\n", recv_waitus);
	printf("Size of empty EGI_INETPACK+PRIV_HEADER: %d Bytes\n",sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER));

  /* A. -------- Work as a UDP Server --------- */
  if( SetUDPServer ) {
	printf("Set as UDP Server, set port=%d \n", port);

        /* Mmap file and get total_packs for transmitting. */
        fmap_snd=egi_fmap_create(fpath, 0, PROT_READ, MAP_PRIVATE);
        if(fmap_snd==NULL) {
		exit(EXIT_FAILURE);
        }
	pack_total=fmap_snd->fsize/datasize;
	tailsize=fmap_snd->fsize - pack_total*datasize;
	if(tailsize)
		pack_total +=1;
	else	/* All pack same size */
		tailsize=datasize;
	printf("tailsize=%dBs\n", tailsize);

	/* Preset pack_seq=pack_total, as for holdon. */
	pack_seq=pack_total;

        /* Create UDP server */
        EGI_UDP_SERV *userv=inet_create_udpServer(svrAddr, port, 4);
        if(userv==NULL)
                exit(EXIT_FAILURE);

	/* Set waitus */
	userv->idle_waitus=100000; 	/* waitus */
	userv->send_waitus=send_waitus; /* waitus */
	userv->recv_waitus=recv_waitus;

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

     do { ////////////////    LOOP  TEST    /////////////////

        /* Create UDP client */
        EGI_UDP_CLIT *uclit=inet_create_udpClient(svrAddr, port, cltAddr, 4);
        if(uclit==NULL)
                exit(EXIT_FAILURE);

	/* Set timeout, note UDP client works in BLOCKING mode
	 * Set a small rcv_timeout for the case: the Client waits in recvfrom(),
         * while the Server has already sent out all packs. it takes rcv_timout
	 * time for the Client to realize that some packs are lost. If rcv_timeout
	 * is big, it just waste time waiting.... OK, check lost packs in other places.
	 */
	inet_sock_setTimeOut(uclit->sockfd, 3, 0, 0, 50000);  /* snd, rcv */

	/* Set waitus */
	uclit->idle_waitus=10000;
	uclit->send_waitus=send_waitus;
	uclit->recv_waitus=recv_waitus;

        /* Set callback */
        uclit->callback=Client_Callback;

        /* Process UDP server routine */
   	egi_clock_start(&eclock);
        inet_udpClient_routine(uclit);
   	egi_clock_stop(&eclock);

        /* Free and destroy */
        inet_destroy_udpClient(&uclit);
	egi_fmap_free(&fmap_rcv);
	egi_bitstatus_free(&ebits);

	/* Sum up */
	cnt_rounds++;
	cnt_tmms+=egi_clock_readCostUsec(&eclock)/1000;
	printf("\n--- UDP transfer Rounds: %d,  Retrys: %d/(%d*%d), TimeOuts: %d, CostTime: %ldms ---\n",
		cnt_rounds, cnt_retrys, pack_total, cnt_rounds, cnt_tmouts, cnt_tmms );

	if( !ever_loop ) {
		printf("Press 'q' to quit, or any other key to run again!\n");
		char ch=getchar();
		if(ch=='q')
			exit(0);
	}

     } while( --nloops || ever_loop); /////////////    END LOOP  TEST    //////////////

  }

	free(svrAddr);
	free(cltAddr);
        return 0;
}


/*----------------------------------------------------------------------
A callback routine for UPD server.

Note:
1. If *sndSize NOT re_assigned to a positive value, server rountine will NOT
proceed to sendto().
2. Server routine shall never exit.

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
	int privdata_off=0;

	/* Note: *sndSize preset to -1 in routine() */

	/* 1. Check and parse receive data  */
	if(rcvSize>0) {
		//EGI_PDEBUG(DBG_TEST,"Received %d bytes data.\n", rcvSize);

		/* 1. Convert rcvData to an IPACK, which is just a reference */
		ipack_rcv=(EGI_INETPACK *)rcvData;
		header_rcv=IPACK_HEADER(ipack_rcv);
		if(header_rcv==NULL) {
			printf("header_rcv is NULL!\n");
			return 0;
		}

		/* 2. Parse CODE */
		switch(header_rcv->code) {
			case CODE_REQUEST_FILE:
				/* Get/save clientAddr, and reset pack_seq */
				printf("Client from '%s:%d' requests for file transfer!\n",
							inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port) );
				if( pack_seq==pack_total ) {  /* pack_seq initialized as pack_total */
					/* To start tranfser */
					pack_seq=0;
					clientAddr=*rcvAddr;
				}
				else {
					/* Consider only one client! */
					printf("Transmission already started!\n");
				}
				break;

			case CODE_REQUEST_PACK:
				/* Note:
				 *	1. *rcvAddr will be used for *sndAddr. It MAY NOT be same as clientAddr.
				 *	2. The lost pack will be sent out immediately, rather than the next pack_seq.
				 */

				/* Get lost_seq number from received ipack_rcv */
				lost_seq= *(unsigned int *)IPACK_PRIVDATA(ipack_rcv);
				printf("Client requests for pack[%d]!\n", lost_seq);

				/* Nest IPACK into sndBuff, fill in header info. */
				ipack_snd=(EGI_INETPACK *)sndBuff;
				ipack_snd->headsize=sizeof(PRIV_HEADER); /* set headsize, Or IPACK_HEADER() may return NULL */

				header_snd=IPACK_HEADER(ipack_snd);
				header_snd->code=CODE_DATA;
				header_snd->seq=lost_seq;
				header_snd->total=pack_total;
				header_snd->fsize=fmap_snd->fsize;

				/* Load data into ipack_snd */
				privdata_off=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);
				if( lost_seq < pack_total-1) {
					/* Set packsize before loadData */
					ipack_snd->packsize=privdata_off+datasize;
					/* Put stamp */
					header_snd->stamp=egi_bitstatus_checksum(fmap_snd->fp+lost_seq*datasize, datasize);
					/* Load privdata */
					if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap_snd->fp+lost_seq*datasize, datasize)!=0 ) {
						printf("Fail to inet_ipacket_loadData() datasize!\n");
						return -1;
					}
				}
				else if ( lost_seq == pack_total-1) {	/* the tail  */
					/* Set packsize before loadData */
					ipack_snd->packsize=privdata_off+tailsize;
					/* Put stamp */
					header_snd->stamp=egi_bitstatus_checksum(fmap_snd->fp+lost_seq*datasize, tailsize);
					/* Load privdata */
					if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap_snd->fp+lost_seq*datasize, tailsize)!=0 ) {
						printf("Fail to inet_ipacket_loadData() tailsize!\n");
						return -1;
					}
				}
				else  { /* Invalid lost_seq number */
					printf("Invalid lost_seq number!\n");
					return -1;
				}

				/* Set sndAddr and sndsize */
				*sndAddr=*rcvAddr;
				*sndSize=ipack_snd->packsize;

				/* Print */
				printf("Sent out lost pack[%d](%dBs).\n", lost_seq, ipack_snd->packsize);
				fflush(stdout);

				/* Return to routine to sendto() it immediately! Any other sendto() jobs are ignored in this round. */
				return 0;

				break;
			case CODE_FAIL:
				printf("Client fails! End session now.\n");
				/* Clear clientAddr and reset pack_seq */
				bzero(&clientAddr,sizeof(clientAddr));
				*sndAddr=clientAddr;
				pack_seq=pack_total;

				break;
			case CODE_FINISH:   /* Finish, clear clientAddr */
				printf("/t--- OK, Client finish receiving and close the session! ---\n");

				/* Clear clientAddr and reset pack_seq */
				bzero(&clientAddr,sizeof(clientAddr));
				*sndAddr=clientAddr;
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

	/* 2. If NO client specified */
	if( clientAddr.sin_addr.s_addr ==0 )
		return 1;

	/* 3. Continue to send packs */
	if( pack_seq < pack_total ) {

		/* Nest IPACK into sndBuff, fill in header info. */
		ipack_snd=(EGI_INETPACK *)sndBuff;
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* set headsize, Or IPACK_HEADER() may return NULL */

		header_snd=IPACK_HEADER(ipack_snd);
		header_snd->code=CODE_DATA;
		header_snd->seq=pack_seq;
		header_snd->total=pack_total;
		header_snd->fsize=fmap_snd->fsize;

		/* Load data into ipack_snd */
		privdata_off=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);
		if( pack_seq > pack_total-1 ) {
			printf("ERR: pack_seq > pack_total-1 \n");
			exit(EXIT_FAILURE);
		}
		else if( pack_seq < pack_total-1) {
			/* Set packsize before loadData */
			ipack_snd->packsize=privdata_off+datasize;
			/* Put stamp */
			header_snd->stamp=egi_bitstatus_checksum(fmap_snd->fp+pack_seq*datasize, datasize);
			/* Load privdata */
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), fmap_snd->fp+pack_seq*datasize, datasize)!=0 )
				return -1;
		}
		else {  /* Tail, pack_seq == pack_total-1 */
			/* Set packsize before loadData */
			ipack_snd->packsize=privdata_off+tailsize;
			/* Put stamp */
			header_snd->stamp=egi_bitstatus_checksum(fmap_snd->fp+pack_seq*datasize, tailsize);
			/* Load privdata */
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
	/* 4. Finish sending, clear clientAddr */
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
	/* 1. Start request IPACK. This Maybe send out more than once before it receives reply from the Server.  */
	if( pack_count==0 ) {
		/* Nest IPACK into sndBuff, fill in header info. */
		ipack_snd=(EGI_INETPACK *)sndBuff;  /* Ref. */
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
		header_snd=IPACK_HEADER(ipack_snd); /* Ref. */
		header_snd->code=CODE_REQUEST_FILE;

		/* Update IPACK packsize, just a header, no privdata! */
		ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);

		/* Set sndsize */
		*sndSize=ipack_snd->packsize;
		if(verbose_on) printf("Finish preping request ipack.\n");

		/* DO NOT return here, need to proceed on to receive reply/data next! */

		//usleep(200000); /* Lingering for a while... just for the Server to response, check server idle_waitus. */
		/* DO NOT wait! in case the server is too fast to send out, then the client will be ! */
	}

	/* 2. Receive file data */
	if( rcvSize > 0 ) {
		EGI_PDEBUG(DBG_TEST,"Received %d bytes data.\n", rcvSize);

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
				if(1) {
					/* Here pack_count is NOT necessary the seq number! */
					//printf("pack_count=%d: Receive pack[%d](%dBs), of total %d packs, ~%jd KBytes.\n",
					//    	pack_count, header_rcv->seq, ipack_rcv->packsize, header_rcv->total, header_rcv->fsize>>10);
					printf("pack_count=%d: Receive pack[%d/(0~%d)](%dBs).\n",
					    	pack_count, header_rcv->seq, header_rcv->total-1, ipack_rcv->packsize);
					fflush(stdout);
				}

				/* 2. Create fmap_rcv and ebits for the first time. */
				if(fmap_rcv==NULL) {
					pack_total=header_rcv->total;

					/* fmap create a file, this will cause time if file is very big! */
					printf("Start create fmap for '%s'...", fpath); fflush(stdout);
					fmap_rcv=egi_fmap_create(fpath, header_rcv->fsize, PROT_READ|PROT_WRITE, MAP_SHARED);
					if(fmap_rcv==NULL) exit(EXIT_FAILURE);
					printf("...OK.\n");
				}
				if( ebits==NULL ) {
					printf("Start create bitstatus..."); fflush(stdout);
					ebits=egi_bitstatus_create(header_rcv->total);
					if(ebits==NULL) exit(EXIT_FAILURE);
					printf("...OK.\n");
				}

				/* 3. Checking seq number, only to see if it's out of sequence, proceed on... */
				if(  header_rcv->seq != pack_count ) {
					//printf("\nOut of sequence! pack_seq=%d, expect seq=%d.\n",
					//		header_rcv->seq, pack_count );
					/* count fails */
					cnt_disorders ++;
				}

				/* 4. Check ebits to see if corresponding pack already received. */
				if( egi_bitstatus_getval(ebits, header_rcv->seq)==1 ) {
					printf("Ipack[%d] already received, skip it!\n",header_rcv->seq);
					break;
				}

				/* Get pack privdata size */
				datasize=IPACK_PRIVDATASIZE(ipack_rcv);

				/* 5. Check stamp */
				if( header_rcv->stamp != egi_bitstatus_checksum(IPACK_PRIVDATA(ipack_rcv),datasize) )
				{
					printf("	xxxxx Ipack[%d] stamp/checksum error! xxxxx\n", header_rcv->seq);
					exit(EXIT_FAILURE);
				}

				if( IPACK_PRIVDATA(ipack_rcv)==NULL ) {
					printf("NO data in ipack_rcv->privdata\n");
					exit(EXIT_FAILURE);
				}

				/* 6. Write to file MMAP, offset as per the sequence number. */
				if(header_rcv->seq > header_rcv->total-1) {
					printf("Seq > header_rcv->total-1! Error.\n");
					exit(EXIT_FAILURE);
					break;
				}
				else if(header_rcv->seq != header_rcv->total-1 ) {   /* Not tail, packsize is the same. */
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

				/* 7. Take seq number as index of ebits, and set 1 to the bit . */
				if( egi_bitstatus_set(ebits, header_rcv->seq)!=0 ) {
					printf("Bitstatus set index=%d fails!\n", header_rcv->seq);
					exit(EXIT_FAILURE);
				}
				else {
					EGI_PDEBUG(DBG_TEST,"Bitstatus set seq=%d\n",header_rcv->seq);
				}

				/* 8. Increase pack_count, also as next ref. sequence number */
				pack_count++;

			    	/* 9. Retry request for lost_pack
				* 9.1 Pick out first ZERO bit in the ebits, ebits->pos is the missed seq number.
				* 9.2 This may be NOT a lost ipack, but just the next ipack in sequence.
				*     so to rule out ebits->pos==pack_count
				* 9.3 TODO: next received pack may NOT be the requested lost ipack, so the same request
				*     may send more than once!
				*/
			    	//ebits->pos=-1;
				if( egi_bitstatus_posfirst_zero(ebits, 0) ==0 && ebits->pos != pack_count )
			    	{
					lost_seq = ebits->pos;
					printf("Pack[%d] is lost!\n",lost_seq);

					/* Nest IPACK into sndBuff, fill in header info. */
					ipack_snd=(EGI_INETPACK *)sndBuff;
					ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
					header_snd=IPACK_HEADER(ipack_snd);
					header_snd->code=CODE_REQUEST_PACK;

					/* Update IPACK packsize first! or IPACK_PRIVDATA() will fail! */
					ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER)+sizeof(lost_seq);

					/* Push lost seq number into privdata */
					*(unsigned int *)IPACK_PRIVDATA(ipack_snd)=lost_seq;

					/* Set sndsize */
					*sndSize=ipack_snd->packsize;
					if(verbose_on)
						printf("Finish preping request for lost pack[%d].\n", lost_seq);

					/* Count retry for lost packs */
					cnt_retrys++;

					/* Return to routine to sendto() immediately. */
					return 0;
				}

				/* 10. Check pack_count */
				if(pack_count==header_rcv->total) {

					printf("\n $$$ Successfully received the file with total %d packs!\n", pack_count);
					//getchar();

					/* Check ebits again! */
				    	//ebits->pos=-1;
				    	if( egi_bitstatus_posfirst_zero(ebits,0) ==0 ) {
						lost_seq = ebits->pos;
						printf("Pack[%d] is lost!\n",lost_seq);
					}
					else
						EGI_PDEBUG(DBG_TEST,"OK, ebits all ONEs!\n");

					/* Feed back to server and end this round of routine process. */
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
	/* recfrome() TIMEOUT : errno=EAGAIN or ECONNREFUSED */
	else  { /* rcvSize <0, recvfrom() ERR! */

		/* DO NOT return -1 here; need to proceed to sendto()! */

		/* Check if any unreceived packs, prepare request ipack. */
		if( pack_count>0 ) {  /* Only if transfer started  */
			EGI_PDEBUG(DBG_TEST,"Recvfrom() timeout!\n");

		/* WARNING!!! If the Server finish sending out all packs at this point, then we have
		 * to check any lost pack here, NOT in if(rcvSize>0){... } part!
		 * Pick out first ZERO bit in the ebits, ebits->pos is the missed seq number.
		 * Note: This may be NOT a lost ipack, but just the next ipack in sequence.
		 */
		    	//ebits->pos=-1;
		    	if( egi_bitstatus_posfirst_zero(ebits,0) ==0 ) {
				lost_seq = ebits->pos;
				printf("Pack[%d] is lost!\n",lost_seq);

				/* Nest IPACK into sndBuff, fill in header info. */
				ipack_snd=(EGI_INETPACK *)sndBuff;
				ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
				header_snd=IPACK_HEADER(ipack_snd);
				header_snd->code=CODE_REQUEST_PACK;

				/* Update IPACK packsize first! or IPACK_PRIVDATA() will fail! */
				ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER)+sizeof(lost_seq);

				/* Push lost seq number into privdata */
				*(unsigned int *)IPACK_PRIVDATA(ipack_snd)=lost_seq;

				/* Set sndsize */
				*sndSize=ipack_snd->packsize;
				EGI_PDEBUG(DBG_TEST,"Finish preping request for lost pack[%d].\n", lost_seq);

				/* Count retry for lost packs */
				cnt_retrys++;

				/* Return to routine to sendto() immediately. */
				return 0;
		     	}

		}  /* END if(pack_count>0) */

	}

	return 0;
}

