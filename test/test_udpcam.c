/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program to transfer video through UDP.

Usage exmample:

	< CAM format YUYV, Trans_data format: YUYV >
	./test_udpcam -S -s 320x240   ( Default: -F yuyv -f yuyv )
	./test_udpcam -C -a 192.168.10.1:5678 -s 320x240 ( Default: -F yuyv )

	< CAM format YUYV, Trans_data format: MJPEG >
	./test_udpcam -S -F mjpeg -s 640x480
	./test_udpcam -C -a 192.168.10.1:5678 -F mjpeg -s 640x480

	< CAM format MJPEG, Trans_data format: MJPEG  >
	./test_udpcam -S -r 30 -f mjpeg -w 0 -t 52000 -s 1280x720  ( '-f mjpeg' also implys '-F mjpeg')
	./test_udpcam -C -a 192.168.10.1:5678 -F mjpeg -s 1280x720

Pan control:
	'a'  Pan left
	'd'  Pan right
	'w'  Pan up
	's'  Pan down


Note:
1. Support hot unplug and plug USBCAM.
2. ebits init once only: Assume that each image frame is divided into
   just same numbers of ipacks!
3. It supports YUYV and MJEPG data transmission,
   If input options:  '-f yuyv -F mjpeg',  then mjpeg data is converted
   from YUYV, not from CAM original source.
4. The Client MUST set same option args for -F (tran_format)
   and -s (image size) as the Server.
5. Set REQ_BUFF_NUMBER as 1 seems better than other values?!
   To many vidoe buffers maybe NOT good.
6. Zlib compress test:
	320x240 jpeg data, ~150kBs, compress time ~30ms  ( --- Applicable --- )
        640x480 jpeg data, ~600kBs, compress time ~150ms
	1280x720 jpeg data, ~1800kBs, compress time ~500ms


TODO:
1. Mutual authentication.
2. Server stop transfering when client quits unexpectedly.
3. If the UDP_server quits, the UDP_client can detect and ....clock timer?
   If the UDP_client quits, the UDP_server will be informed...
   Or a rather reliable way to communicate/confirm with eatch other.

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
//#include <egi_input.h>
#include <egi_fbdev.h>
#include <egi_image.h>
#include <egi_bjp.h>
//#include <egi_FTsymbol.h>
#include <egi_timer.h>
#include <egi_inet.h>
#include <egi_utils.h>
#include <egi_debug.h>

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h> /* include <linux/v4l2-controls.h>, <linux/v4l2-common.h> */

#include <zlib.h>

#undef  DEFAULT_DBG_FLAGS
#define DEFAULT_DBG_FLAGS   DBG_TEST


/* Video frame buffers */
#define REQ_BUFF_NUMBER	1	/* Buffers to be allocated for the CAM */

/* Arguments for usbcam_routine() */
struct thread_cam_args {
	char	*dev_name;
	int 	pixelformat;
	int 	width;
	int	height;
	int	fps;
	bool	reverse;
	bool	display;	/* Display on the host */
};
struct thread_cam_args cam_args;

//int usbcam_routine(char * dev_name, int width, int height, int pixelformat, int fps );
void* usbcam_routine(void *args);


/* Data for UDP transmission */
struct  udp_trans_data {
	void 	*src_data;	/* Source data, Only referece */
	size_t 	 src_size;
	bool	 ready;		/* Ready for transfer */
	//pthread_mutex_t  mutex_lock;
};
struct udp_trans_data	trans_data;
int trans_format=V4L2_PIX_FMT_YUYV;	/* Default is YUYUV,  may set to be V4L2_PIX_FMT_MJPEG */

void *dest_data;  		/* Received data, from src_data */
unsigned long dest_size;	/* Size of dest_data */

unsigned char *rgb24;		/* For RGB 24bits color data */
unsigned char *jpgdata;		/* For mjpeg data */
unsigned long jpgdata_size=0;	/* Size of jpgdata */
unsigned int  jpg_quality=80;

/* A private header for my IPACK */
typedef struct {

#define CODE_NONE		0	/* none */
#define CODE_DATA		1	/* Data availabe, seq/total, data in privdata */
#define	CODE_REQUEST_VIDEO	2	/* Client ask for file stransfer */
#define CODE_REQUEST_PACK	3	/* Client ask for lost packs, ipack seq number in privdata */
#define CODE_FINISH		4	/* Client ends session */
#define CODE_FAIL		5	/* Client fails to receive packets */

	int		code;   /* MSG code */
	unsigned int	seq;	/* Packet sequence number, from 0 */
	unsigned int	total;  /* Total packets */
	off_t		fsize;	/* Total size of a file/buffer divided into packs for transmission */
//	int		width;
//	int		height;	/* For image size */
	int		stamp;  /* signature, stamp */

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

/* 1. UDP packet information
 * privdata size in each IPACK. MUST <= EGI_MAX_UDP_PDATA_SIZE-sizeof(EGI_INETPACK)-sizeof(PRIV_HEADER)
 * For Server ONLY
 * 2. pack_total, tailsize, pack_seq=0 to be calculated and set in/by usbcam_routine()
 *
 */
static unsigned int datasize=1024*25;   /* !!!Server: set avg privdata size.  Client: recevied ipack privdata size(avg and tail). */
static unsigned int tailsize;		/* For server: Defined as size of last pack, when pack_seq == pack_total-1 */
static unsigned int pack_seq;	   	/* For server: Sequence number for current IPACK */
static unsigned int pack_total;    	/* For server/client: Total number of IPACKs for transfering a file */
static unsigned int pack_count;		/* For client */
static unsigned int lost_seq;		/* Lost pack seq number */

EGI_BITSTATUS *ebits;			/* To record/check packet sequence number */

/* Callback functions, define routine jobs. */
int Client_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
								 	   char *sndBuff, int *sndSize );
int Server_Callback( int *cmdcode, const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
					 struct sockaddr_in *sndAddr, char *sndBuff, int *sndSize);

bool verbose_on;
bool run_session;	/* predicate: transmission session is running */

int px0;		/* Display origin */
int py0;

/*---------------------------
        Print help
-----------------------------*/
void print_help(const char* cmd)
{
        printf("Usage: %s [-hvPRd:s:r:f:b:SCF:q:a:y:p:t:w:] \n", cmd);
        printf("        -h   help \n");
        printf("        -v   print verbose\n");
	/* For USB CAM */
	printf("\n	--- USB CAM ---\n");
	printf("	-P   Display CAM video on the host.\n");
	printf("	-R   Reverse upside down, default: false. for YUYV only.\n");
        printf("        -d:  Video device, default: '/dev/video0' \n");
        printf("        -s:  Image size, default: '320x240' \n");
        printf("        -r:  Frame rate, default: 10fps \n");
        printf("        -f:  CAM pixel data format, default: 'yuyv'(V4L2_PIX_FMT_YUYV) \n");
        printf("             or 'mjpeg'(V4L2_PIX_FMT_MJPEG), then trans_data format is also 'mjpeg'. \n");
	printf("	-b:  Brightness in percentage(for host display), default: 80(%%) \n");

	/* For UDP transmission */
	printf("\n	--- UDP transmission ---\n");
        printf("        -S   work as UDP Server\n");
        printf("        -C   work as UDP Client (default)\n");
	printf("	-F:  Trans_Data format, default 'yuyv', (or 'mjpeg'(converted from yuyv))\n");
	printf("	     when set option '-f mjpeg', then trans_data format is also 'mjpeg'.\n");
	printf("	-q:  JPEG quality, default: 80\n");
        printf("        -a:  Server IP address, with or without port.\n");
        printf("        -y:  Client IP address, default NULL.\n");
        printf("        -p:  Port number, default 5678.\n");
        printf("        -t:  Size of pridata packed in each IPACK, default 25kBs.\n");
        printf("        -w:  Server send_waitus in us. defautl 10ms.\n");

}


/*-----------------------------
	      MAIN
------------------------------*/
int main (int argc,char ** argv)
{
	int	opt;
	char	*pt=NULL;

	/* Thread CAM and default args */
	pthread_t	thread_cam;

	/* Defaut params: pxiformat, image size and frame rate */
	cam_args.dev_name="/dev/video0";
	cam_args.pixelformat=V4L2_PIX_FMT_YUYV; //MJPEG;
	cam_args.width=320;
	cam_args.height=240;
	cam_args.fps=10;

        int     brightpcnt=80;  /* brightness in percentage */

	/* For UDP server/client transfer */
        bool SetUDPServer=false;
        char *svrAddr=NULL;             /* Default NULL, to be decided by system */
        char *cltAddr=NULL;
        unsigned short int port=5678;
        unsigned int pack_shellsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);

	/* Routine waitus */
        unsigned int send_waitus=5000; /* in us */
	unsigned int idle_waitus=5000;


#if 1 /* TEST: zlib ------ argv[1]: filepath, argv[2]: compress level */
	/* NOTE:
	 * 320x240 jpeg data, ~150kBs, compress time ~30ms
         * 640x480 jpeg data, ~600kBs, compress time ~150ms
	 * 1280x720 jpeg data, ~1800kBs, compress time ~500ms
	 */
	int level;
	unsigned long len;
	EGI_CLOCK eclock={0};

	if(argc>2)
		level=atoi(argv[2]);
	if(level<0) level=0;
	if(level>9) level=9;

	EGI_FILEMMAP *fin=egi_fmap_create(argv[1], 0, PROT_READ, MAP_PRIVATE);
	if(fin==NULL) exit(1);

	unsigned long bsize=compressBound(fin->fsize);
	printf("bsize=%lu\n", bsize);

	/* 1. Compress to test.z */
	EGI_FILEMMAP *fout=egi_fmap_create("test.z", bsize, PROT_WRITE, MAP_SHARED);
	if(fout==NULL) exit(1);

	len=bsize;
	egi_clock_start(&eclock);
	printf("start compress..."); fflush(stdout);
	if(compress2((Bytef *)fout->fp, &len, (Bytef *)fin->fp, fin->fsize, level) !=Z_OK) {  /* LEVEL 0~9 */
		printf("Zlib compress2 error!\n");
		exit(1);
	}
	printf("OK!\n");
	egi_clock_stop(&eclock);
	printf("Input size=%jdBs, output size=%luBs, compress ratio=%.1f:1\n", fin->fsize, len, 1.0*fin->fsize/len);
	printf("Cost tm=%ld us\n", egi_clock_readCostUsec(&eclock));

	/* Resize fout */
	if( egi_fmap_resize(fout, len)!=0 ) {
		printf("Fout resize error!\n");
	}

	/* 2. Uncompress again to zlib.jpg */
	EGI_FILEMMAP *fbk=egi_fmap_create("zlib.jpg",fin->fsize, PROT_WRITE, MAP_SHARED);
	if(fbk==NULL) exit(1);

	printf("start uncompress...");fflush(stdout);
	len=fbk->fsize;
	if(uncompress((Bytef *)fbk->fp, &len, (Bytef *)fout->fp, fout->fsize)!=Z_OK) {
		printf("Zlib uncompress error!\n");
		exit(1);
	}
	printf("Ok!\n");

	egi_fmap_free(&fin);
	egi_fmap_free(&fout);
	egi_fmap_free(&fbk);
	exit(0);

#endif /* END:TEST */



        /* Parse input option */
        while( (opt=getopt(argc,argv,"hvPRd:s:r:f:b:SCF:q:a:y:p:t:w:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
                        case 'v':
                                verbose_on=true;
                                break;

			/* Four USB CAM */
			case 'P':
				cam_args.display=true;
				break;
			case 'R':
				cam_args.reverse=true;
				break;
                        case 'd':       /* Video device name */
                                cam_args.dev_name=optarg;
                                break;
                        case 's':       /* Image size */
                                cam_args.width=atoi(optarg);
                                if((pt=strstr(optarg,"x"))!=NULL)
                                        cam_args.height=atoi(pt+1);
                                else
                                        cam_args.height=atoi(strstr(optarg,"X"));
                                break;
                        case 'r':       /* Frame rate */
                                cam_args.fps=atoi(optarg);
                                printf("Input fps=%d\n", cam_args.fps);
                                break;
                        case 'f':
                                if(strstr(optarg, "mjpeg"))
                                        cam_args.pixelformat=V4L2_PIX_FMT_MJPEG;
                                break;
                        case 'b':
                                brightpcnt=atoi(optarg);
                                if(brightpcnt<30)brightpcnt=30;
                                if(brightpcnt>100)brightpcnt=100;
                                break;

			/* For UDP transmission */
                        case 'S':
                                SetUDPServer=true;
                                break;
                        case 'C':
                                SetUDPServer=false;
                                break;
			case 'F':
                                if(strstr(optarg, "jpeg"))
                                        trans_format=V4L2_PIX_FMT_MJPEG;
                                break;
			case 'q':
				jpg_quality=atoi(optarg);
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
                        case 't':
                                datasize=atoi(optarg);
                                if(datasize<1448-pack_shellsize)
                                        datasize=1448-pack_shellsize;
                                else if(datasize>EGI_MAX_UDP_PDATA_SIZE-pack_shellsize) {
                                        datasize=EGI_MAX_UDP_PDATA_SIZE-pack_shellsize;
                                        printf("UDP packet size exceeds limit of %dBs, reset datasize=%d-%d(pack_shellsize)=%dBs.\n",
                                                EGI_MAX_UDP_PDATA_SIZE, EGI_MAX_UDP_PDATA_SIZE, pack_shellsize, datasize);
                                }
                                break;
                        case 'w':
                                send_waitus=atoi(optarg);
                                break;

                }
        }

        /* Init sys FBDEV */
        if( init_fbdev(&gv_fb_dev) )
                return -1;

        /* Set FB mode */
        fb_set_directFB(&gv_fb_dev, true);
        fb_position_rotate(&gv_fb_dev, 0);

   printf("\n\t--- UDP Transmission params ---\n");
   printf("Format: %s\n", trans_format==V4L2_PIX_FMT_YUYV?"YUYV":"MJPEG");
   printf("Packsize: %dBs\n", datasize + pack_shellsize);
   printf("Pack privdata size: %dBs\n", datasize);

   /* A. -------- Work as a UDP Server --------- */
  if( SetUDPServer ) {
        printf("Set as UDP Server, set port=%d \n", port);

	/* Start thread CAM */
	printf("Start CAM routine thread...\n");
	if( pthread_create(&thread_cam, NULL, usbcam_routine, &cam_args) !=0 ) {
		printf("Fail to create thread_cam, Err'%s'\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Create a UDP server */
 	EGI_UDP_SERV  *userv=inet_create_udpServer(NULL, port, 4);
	if(userv==NULL)
		exit(EXIT_FAILURE);

        /* Set waitus */
        userv->idle_waitus=2000;
        userv->send_waitus=send_waitus;

	/* Set callback */
	userv->callback=Server_Callback;

	/* Start routine */
	printf("Start UDP server routine...\n");
	inet_udpServer_routine(userv);

	/* Joint thread CAM */
	void *ret=NULL;
	if( pthread_join(thread_cam, &ret)!=0 ) {
		printf("Fail pthread_join, ret=%d, Err'%s'\n", *(int *)ret, strerror(errno));
	}

        /* Free and destroy */
        inet_destroy_udpServer(&userv);
  }

  /* B. -------- Work as a UDP Client --------- */
  else {
        printf("Set as UDP Client, targets to server at %s:%d. \n", svrAddr, port);

        if( svrAddr==NULL || port==0 ) {
                printf("Please provide Server IP address and port number!\n");
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }

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
        uclit->idle_waitus=2000;
        uclit->send_waitus=10000;

        /* Set callback */
        uclit->callback=Client_Callback;

        /* Process UDP server routine */
        inet_udpClient_routine(uclit);

        /* Free and destroy */
        inet_destroy_udpClient(&uclit);
        egi_bitstatus_free(&ebits);
	free(dest_data);

  }

	/* Common:  Release FBDEV */
	fb_filo_flush(&gv_fb_dev);
   	release_fbdev(&gv_fb_dev);

	return 0;
}


/*-----------------------------------------------------------
		USB CAM rountine jobs
Open video device and the set with proper parameters, allocate
video buffers and start capturing, the it loops reading video
stream data and processing/dispaching.

args:
@dev_name:	Video capture device path
@width,height:	Expected video size.
@pixelformat:   Pixel format for video stream. Examples:
		V4L2_PIX_FMT_MJPEG
		V4L2_PIX_FMT_YUYV
@fps:		Expected frame rate.


----------------------------------------------------------*/
//int usbcam_routine(char * dev_name, int width, int height, int pixelformat, int fps )
void* usbcam_routine(void *args)
{
	struct thread_cam_args *camargs=NULL;
	char	*dev_name;
	int     width;
	int     height;
	int     pixelformat;
	int     fps;

	int 	fd_dev;
   	fd_set 	fds;
	unsigned int i;

	int	brightpcnt=80;	/* brightness in percentage */
	bool	reverse=false;
//	int	x0=0,y0=0;

	/* Get CAM args */
	if(args==NULL)
		return (void *)-1;
	camargs=args;
	dev_name=camargs->dev_name;
	width=camargs->width;
	height=camargs->height;
	pixelformat=camargs->pixelformat;
	fps=camargs->fps;

	/* Check input args */
	if(dev_name==NULL)
		dev_name = "/dev/video0";
	if(width<0 || height<0) {
		width=320;
		height=240;
	}
	if(pixelformat!=V4L2_PIX_FMT_MJPEG && pixelformat!=V4L2_PIX_FMT_YUYV)
		pixelformat=V4L2_PIX_FMT_MJPEG;
	if(fps<0)
		fps=10;

	/* v4l2 STRUTs */
	struct v4l2_queryctrl		queryctrl={0};
	struct v4l2_control		vctrl;
     	struct v4l2_capability 		cap;
     	struct v4l2_format 		fmt;
     	struct v4l2_fmtdesc 		fmtdesc;
     	struct v4l2_requestbuffers 	req;
     	struct v4l2_buffer 		bufferinfo;
     	struct v4l2_streamparm 		streamparm;
     	enum v4l2_buf_type type;

	/* Time vars */
	struct timeval			tv;
	struct timeval          	timestamp;
	struct v4l2_timecode    	timecode;

	/* To store allocated video buffer address and size */
	struct vbuffer {
        	void *                  start;
        	size_t                  length;
	};
	struct vbuffer *buffers;
	unsigned long bufindex;

	/* To store controll values for specific CAM only */
	struct v4l2_control vctrl_params[] =
	{
	  { V4L2_CID_BRIGHTNESS,		50 	},
	  { V4L2_CID_CONTRAST,			32 	},
	  { V4L2_CID_SATURATION,		60 	},
	  { V4L2_CID_SHARPNESS,			3  	},
	  { V4L2_CID_HUE,			4  	},
	  { V4L2_CID_GAMMA,			150 	},
	  { V4L2_CID_EXPOSURE_AUTO_PRIORITY,	1 	},
	  { V4L2_CID_AUTO_WHITE_BALANCE,	1 	},
	};


OPEN_DEV:

	/* 1. Open video device */
     	while ( (fd_dev = open(dev_name, O_RDWR)) <0 ) {
      	  	printf("Fail to open '%s', Err'%s'.\n", dev_name, strerror(errno));
		usleep(200000);
		//return -1;
	}

	/* 2. Get video driver capacity */
      	if( ioctl (fd_dev, VIDIOC_QUERYCAP, &cap) !=0) {
      	  	printf("Fail to ioctl VIDIOC_QUERYCAP, Err'%s'\n", strerror(errno));
	  	return (void *)-1;
      	}
	printf("\n\t--- Driver info ---\n");
  	printf("Driver Name:\t%s\nCard Name:\t%s\nBus info:\t%s\nCapibilities:\t0x%04X\n",cap.driver,cap.card,cap.bus_info, cap.capabilities);
      	if( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) {
	      	printf("The video device does NOT supports capture.\n");
		  	return (void *)-1;
      	}
      	if( !(cap.capabilities & V4L2_CAP_STREAMING) ) {
	      	printf("The video device does NOT support streaming!\n");
		return (void *)-1;
	}

	/* 3. Adjust CAM controls
	 * Set contrast/brightness/saturation/hue/sharpness/gamma/...
	 * Default usually is OK.
	 */
	printf("\n\t--- Control params ---\n");
	for(i=0; i<sizeof(vctrl_params)/sizeof(struct v4l2_control); i++) {
		/* Query Contro　获取设备支持的各项控制变量及其数值 */
		vctrl.id=vctrl_params[i].id;
		queryctrl.id=vctrl_params[i].id;
		if( ioctl( fd_dev, VIDIOC_QUERYCTRL, &queryctrl) !=0 ) {
	      	  	printf("Fail to ioctl VIDIOC_QUERYCTRL for control ID=%d, Err'%s'\n", i, strerror(errno));
			continue;
		}
        	if( ioctl( fd_dev, VIDIOC_G_CTRL, &vctrl) !=0 ) {
	      	  	printf("Fail to ioctl VIDIOC_G_CTRL for control ID=%d, Err'%s'\n", i, strerror(errno));
			continue;
		}
		else {
			printf("%s:	min=%d	max=%d	default=%d	value=%d\n",
					queryctrl.name, queryctrl.minimum, queryctrl.maximum, queryctrl.default_value,
					vctrl.value);
		}

		/* Change brightness to 80% of its range. */
		if(vctrl.id==V4L2_CID_BRIGHTNESS) {
			vctrl.value=queryctrl.minimum+(queryctrl.maximum-queryctrl.minimum)*brightpcnt/100;
			ioctl( fd_dev, VIDIOC_S_CTRL, &vctrl);
		}
	}

	/* 4. Set data format */
     	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	fmt.fmt.pix.width       = width;
     	fmt.fmt.pix.height      = height;
     	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;  /* Usually no effective */
     	fmt.fmt.pix.pixelformat = pixelformat;			/*  MJPEG or YUYV */
     	if( ioctl (fd_dev, VIDIOC_S_FMT, &fmt) !=0) {
    		printf("Fail to ioctl VIDIOC_S_FMT, Err'%s'\n", strerror(errno));
		//return (void *)-2;
		goto END_FUNC;
	}
     	if( ioctl( fd_dev, VIDIOC_G_FMT, &fmt) !=0 ) {
    		printf("Fail to ioctl VIDIOC_G_FMT, Err'%s'\n", strerror(errno));
		//return (void *)-2;
		goto END_FUNC;
	}
	else {
		printf("\n\t--- Effective params ---\n");
	     	printf("fmt.type: 	%d\n", fmt.type);
     		printf("pixelformat:	%c%c%c%c\n",fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat>>8) & 0xFF,
						(fmt.fmt.pix.pixelformat>>16) & 0xFF, (fmt.fmt.pix.pixelformat>>24) & 0xFF);
	     	printf("WidthxHeight: 	%dx%d\n",fmt.fmt.pix.width, fmt.fmt.pix.height);
     		printf("Bytesperline: 	%d\n", fmt.fmt.pix.bytesperline);
     		printf("Pixfield: 	%d (1-no field)\n", fmt.fmt.pix.field);
		printf("Colorspace:	%d\n", fmt.fmt.pix.colorspace);
		printf("Display origin:	(%d,%d)\n", px0,py0);
	}
	/* Confirm working width and height */
	width=fmt.fmt.pix.width;
	height=fmt.fmt.pix.height;

     	/* 6. Set video stream params: frame rate */
     	memset(&streamparm, 0, sizeof(streamparm));
     	streamparm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	streamparm.parm.capture.timeperframe.denominator=fps;
     	streamparm.parm.capture.timeperframe.numerator=1;
     	if( ioctl( fd_dev, VIDIOC_S_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_S_PARM, Err'%s'\n", strerror(errno));
		//return (void *)-3;
		goto END_FUNC;
	}
	/* Confirm working FPS */
	if( ioctl( fd_dev, VIDIOC_G_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_G_PARM, Err'%s'\n", strerror(errno));
		//return (void *)-3;
		goto END_FUNC;
	}
	else {
		printf("Frame rate:\t%d/%d fps\n", streamparm.parm.capture.timeperframe.denominator,
							streamparm.parm.capture.timeperframe.numerator);
	}

     	/* 7. Enumerate formats supported by the vide device */
     	fmtdesc.index=0;
     	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	printf("The device supports following formats for video captrue:\n");
     	while( ioctl( fd_dev, VIDIOC_ENUM_FMT, &fmtdesc)==0 ) {
		printf("\t%d.%s\n", fmtdesc.index+1, fmtdesc.description);
		fmtdesc.index++;
     	}

	/* 8. Allocate video frame buffers */
     	req.count               = REQ_BUFF_NUMBER;
     	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	req.memory              = V4L2_MEMORY_MMAP;
     	if( ioctl (fd_dev, VIDIOC_REQBUFS, &req) !=0) {
    		printf("Fail to ioctl VIDIOC_REQBUFS, Err'%s'\n", strerror(errno));
		//return (void *)-4;
		goto END_FUNC;
	}
	/* Confirm actual buffers allocated */
	printf("Req. buffer ret/req:\t%d/%d \n", req.count, REQ_BUFF_NUMBER);

	/* Save allocated buffer info */
     	buffers = calloc (req.count, sizeof (*buffers));
	memset(&bufferinfo, 0, sizeof(bufferinfo));
     	for (bufindex = 0; bufindex < req.count; ++bufindex)
     	{
           	bufferinfo.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
           	bufferinfo.memory      = V4L2_MEMORY_MMAP;
           	bufferinfo.index       = bufindex;

           	if( ioctl (fd_dev, VIDIOC_QUERYBUF, &bufferinfo) !=0) {
	    		printf("Fail to ioctl VIDIOC_QUERYBUF, Err'%s'\n", strerror(errno));
			//return (void *)-4;
			goto END_FUNC;
		}
           	buffers[bufindex].length = bufferinfo.length;

		/* Mmap to user space. the device MUST support V4L2_CAP_STREAMING mode! */
		buffers[bufindex].start = mmap (NULL, bufferinfo.length, PROT_READ | PROT_WRITE,
			                        	                MAP_SHARED, fd_dev, bufferinfo.m.offset);
		if(buffers[bufindex].start == MAP_FAILED) {
	    		printf("Fail to mmap buffer, Err'%s'\n", strerror(errno));
			return (void *)-4;
		}

		/* Clear it */
		memset(buffers[bufindex].start, 0, bufferinfo.length);
     	}
        /* Size of allocated video buffer */
        printf("Video buffer size:\t%d(Bytes)\n", bufferinfo.length);

	/* 9. Start to capture video.  some devices may need to queue the buffer at first! */
    	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	if( ioctl (fd_dev, VIDIOC_STREAMON, &type) !=0) {
    		printf("Fail to ioctl VIDIOC_STREAMON, Err'%s'\n", strerror(errno));
		//return (void *)-5;
		goto END_FUNC;
	}

	/* 10. Queue buffer after turn on video capture! */
       	for (i = 0; i < REQ_BUFF_NUMBER; ++i) {
               bufferinfo.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               bufferinfo.memory      = V4L2_MEMORY_MMAP;
               bufferinfo.index       = i;
               if( ioctl (fd_dev, VIDIOC_QBUF, &bufferinfo) !=0) {
		    	printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));
			//return (void *)-4;
			goto END_FUNC;
		}
	}

   	/* Reset index, from which it starts the loop. */
   	bufferinfo.index=0;

        	/* ---------- Loop reading video frames and display --------*/

/* 1. --- pixelformat: V4L2_PIX_FMT_YUYV --- */
if( pixelformat==V4L2_PIX_FMT_YUYV )
{
   printf("Output format: YUYV, reverse=%s\n", reverse?"Yes":"No");


   /* Allocate mem for rgb24 data */
   rgb24=calloc(1, width*height*3);
   if(rgb24==NULL) {
	printf("Fail to calloc rgb24!\n");
	goto END_FUNC;
   }

   /* Allocate mem for jpgdata, pre.. */
   jpgdata_size=width*height*3;
   jpgdata=calloc(1, jpgdata_size);
   if(jpgdata==NULL) {
	printf("Fail to calloc jpgdata!\n");
	goto END_FUNC;
   }


   /* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
   for(;;) {
   	FD_ZERO (&fds);
   	FD_SET (fd_dev, &fds);

	/* Select() will do  nothing if fd_dev is closed by other thread! */
	tv.tv_sec=1; tv.tv_usec=0;
   	if( select(fd_dev + 1, &fds, NULL, NULL, &tv) <0) {
		fprintf(stderr, "Device file abnormal, Err'%s'.\n", strerror(errno));
	}

    	/* Dequeue the buffer */
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index++;
	if(bufferinfo.index==REQ_BUFF_NUMBER)
		bufferinfo.index=0;
        if( ioctl(fd_dev, VIDIOC_DQBUF, &bufferinfo) !=0) {
                printf("Fail to ioctl VIDIOC_DQBUF, Err'%s'\n", strerror(errno));

		/* If device is closed, then try to re_open it. this shall follow after select() operation. */
		if(errno==ENODEV) {
			goto END_FUNC;

			/* Unmap and close file */
   			//for (i = 0; i < REQ_BUFF_NUMBER; ++i)
      			//	munmap (buffers[i].start, buffers[i].length);
			//close(fd_dev);
			//goto OPEN_DEV;
		}
	}
	else {
		/* Get frame timestamp. it's uptime!  */
		//printf("VBUF len: %dBs,  Ts: %ld.%01ld \r",
		//		buffers[bufferinfo.index].length,  bufferinfo.timestamp.tv_sec, bufferinfo.timestamp.tv_usec/100000 );
		//printf("Tc: %02d:%02d:%02d \r");
		//		bufferinfo.timecode.hours, bufferinfo.timecode.minutes, bufferinfo.timecode.seconds);
		// fflush(stdout);

 	   	/* 1. trans_data format: YUYV -------- */
		if(trans_format==V4L2_PIX_FMT_YUYV) {
			/* -------> Prepare buffer for UDP transfer */
			trans_data.src_data=buffers[bufferinfo.index].start;
			trans_data.src_size=buffers[bufferinfo.index].length;
	   	}
	   	/* 2. trans_data format: MJPEG -------- */
 	   	else {
		   #if 0 /* METHOD 1: YUYV --> RGB24 --> JPEG */
			egi_color_YUYV2RGB888(buffers[bufferinfo.index].start, rgb24, width, height, reverse);
			/* Alway reset jpgdata_size to original value, otherwise the function will realloc if NOT enough! */
			jpgdata_size=3*width*height;
			//free(jpgdata); jpgdata=NULL; /* OR let function to allocate it each time! */
			compress_to_jpgBuffer(&jpgdata, &jpgdata_size, 80, width, height, rgb24, JCS_RGB);

		   #else /* METHOD 2: YUYV --> YUV --> JPEG  */
                        /* Convert YUYV to YUV, then to JPEG */
                        egi_color_YUYV2YUV(buffers[bufferinfo.index].start, rgb24, width, height, reverse); /* rbg24 for dest YUV data */
			compress_to_jpgBuffer(&jpgdata, &jpgdata_size, 80, width, height, rgb24, JCS_YCbCr);

		   #endif
			printf("WxH: %dx%d, jpadata_size: %luBs\n", width,height, jpgdata_size);

		    #if 0 /* TEST: show jpgdata ----------- */
			show_jpg(NULL, jpgdata, jpgdata_size, &gv_fb_dev, 0, px0, py0);
			fb_render(&gv_fb_dev);

			//usleep(500000);
	        	if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0)
		               printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));
			continue;
		    #endif

			/* -------> Prepare buffer for UDP transfer */
			trans_data.src_data=jpgdata;
			trans_data.src_size=jpgdata_size;
	   	}

		/* Cal. pack total */
		pack_total=trans_data.src_size/datasize;
		tailsize=trans_data.src_size - pack_total*datasize;
		printf("trans_data.src_size=%d, datasize=%d, tailsize=%d \n", trans_data.src_size, datasize, tailsize);
		if(tailsize)
			pack_total +=1;
		else	/* All pack same size */
			tailsize=datasize;

		/* Let go */
		pack_seq=0;
		trans_data.ready=true;

		/* Display Video */
		if( camargs->display ) {
			egi_color_YUYV2RGB888(buffers[bufferinfo.index].start, rgb24, width, height, reverse);
			egi_imgbuf_showRBG888(rgb24, width, height, &gv_fb_dev, px0, py0);  /* Direct FBwrite */
			fb_render(&gv_fb_dev);
		}


	        /* If NO client */
        	if( clientAddr.sin_addr.s_addr ==0 )
			trans_data.ready=false;

/* <-------- Wait for complete of transfer  */
		while( trans_data.ready ) usleep(5000);

        	/* Queue the buffer */
        	if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0)
	                printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));

		/* <-------- Wait for complete of transfer  */
	}

   }

} /* End  pixelformat==V4L2_PIX_FMT_YUYV */

/* 2. --- pixelformat: V4L2_PIX_FMT_MJPEG --- */
else if( pixelformat==V4L2_PIX_FMT_MJPEG )
{
   printf("Output format: MJPEG\n");

   /* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
   for(;;) {
   	FD_ZERO (&fds);
   	FD_SET (fd_dev, &fds);
   	select(fd_dev + 1, &fds, NULL, NULL, NULL);

    	/* Dequeue the buffer */
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index++;
	if(bufferinfo.index==REQ_BUFF_NUMBER)
		bufferinfo.index=0;
        if( ioctl(fd_dev, VIDIOC_DQBUF, &bufferinfo) !=0) {
                printf("Fail to ioctl VIDIOC_DQBUF, Err'%s'\n", strerror(errno));

		/* If device is closed, then try to re_open it. this shall follow after select() operation. */
		if(errno==ENODEV) {
			goto END_FUNC;

			/* Unmap and close file */
   			//for (i = 0; i < REQ_BUFF_NUMBER; ++i)
      			//	munmap (buffers[i].start, buffers[i].length);
			//close(fd_dev);
			//goto OPEN_DEV;
		}
	}
	else {
		/* Print size and timestamp */
		//printf("VBUF len: %dBs,  Ts: %ld.%01ld \r",
		//		buffers[bufferinfo.index].length,  bufferinfo.timestamp.tv_sec, bufferinfo.timestamp.tv_usec/100000 );
		//fflush(stdout);

		/* -------> Prepare buffer for UDP transfer */
		trans_data.src_data=buffers[bufferinfo.index].start;
		trans_data.src_size=buffers[bufferinfo.index].length;

		/* Cal. pack total */
		pack_total=trans_data.src_size/datasize;
		tailsize=trans_data.src_size - pack_total*datasize;
		printf("trans_data.src_size=%d, datasize=%d, tailsize=%d \n", trans_data.src_size, datasize, tailsize);
		if(tailsize)
			pack_total +=1;
		else	/* All pack same size */
			tailsize=datasize;

		/* Let go */
		pack_seq=0;
		trans_data.ready=true;

		/* Display Video */
		if( camargs->display ) {
			show_jpg(NULL, buffers[bufferinfo.index].start, buffers[bufferinfo.index].length, &gv_fb_dev, 0, px0, py0);
			fb_render(&gv_fb_dev); 		/* 刷新Framebuffer */
		}

	        /* If NO client */
        	if( clientAddr.sin_addr.s_addr ==0 )
			trans_data.ready=false;

/* <-------- Wait for complete of transfer  */
		while( trans_data.ready ) usleep(5000);

        	/* Queue the buffer */
        	if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0)
	                printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));

	}

   }

} /* End  pixelformat==V4L2_PIX_FMT_MJPEG */


END_FUNC:

   /* 关闭视频流 */
   /* Video stream off */
   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   if( ioctl(fd_dev, VIDIOC_STREAMOFF, &type) !=0)
   	printf("Fail to ioctl VIDIOC_DQBUF, Err'%s'\n", strerror(errno));

   /* 解除内存映射 */
   /* Unmap and close file */
   for (i = 0; i < REQ_BUFF_NUMBER; ++i)
      munmap (buffers[i].start, buffers[i].length);

   /* 释放申请的内存 */
   free(buffers);
   free(rgb24);

   /* 关闭设备文件 */
   close (fd_dev);

   usleep(500000);
goto OPEN_DEV;

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
			case CODE_REQUEST_VIDEO:
				/* Get/save clientAddr, and reset pack_seq */
				printf("Client from '%s:%d' requests for video transfer!\n",
							inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port) );

			#if 0   /* Get clientAddr */
				if( clientAddr.sin_addr.s_addr !=0 ) {
					/* Consider only one client! */
					printf("Transmission already started!\n");
				}
				else
					clientAddr=*rcvAddr;
			#else  /* Just Update clientAddr! */
				clientAddr=*rcvAddr;
			#endif

				break;

#if 0 //////////////////// request lost_pack //////////////////////
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
#endif ///////////////////////////////////////////////////////

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
	if( clientAddr.sin_addr.s_addr ==0 ) {
		return 1;
	}

	/* 3. Continue to send packs */
	if( trans_data.ready && pack_seq < pack_total ) {
		//EGI_PDEBUG(DBG_TEST,"Start send packs...\n");

		/* Nest IPACK into sndBuff, fill in header info. */
		ipack_snd=(EGI_INETPACK *)sndBuff;
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* set headsize, Or IPACK_HEADER() may return NULL */

		header_snd=IPACK_HEADER(ipack_snd);
		header_snd->code=CODE_DATA;
		header_snd->seq=pack_seq;
		header_snd->total=pack_total;
		header_snd->fsize=trans_data.src_size;

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
			header_snd->stamp=egi_bitstatus_checksum(trans_data.src_data+pack_seq*datasize, datasize);
			/* Load privdata */
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), trans_data.src_data+pack_seq*datasize, datasize)!=0 )
				return -1;
		}
		else {  /* Tail, pack_seq == pack_total-1 */
			/* Set packsize before loadData */
			ipack_snd->packsize=privdata_off+tailsize;
			/* Put stamp */
			header_snd->stamp=egi_bitstatus_checksum(trans_data.src_data+pack_seq*datasize, tailsize);
			/* Load privdata */
			if( inet_ipacket_loadData(ipack_snd, sizeof(PRIV_HEADER), trans_data.src_data+pack_seq*datasize, tailsize)!=0 )
				return -1;
		}

		/* Set sndAddr and sndsize */
		*sndAddr=clientAddr;
		*sndSize=ipack_snd->packsize;

		/* Print */
		EGI_PDEBUG(DBG_TEST,"Sent out pack[%d](%dBs), of total %d packs.\n", pack_seq, ipack_snd->packsize, pack_total);
		fflush(stdout);

		/* Increase pack_seq, as next sequence number. */
		pack_seq++;

	}
	/* 4. Finish sending, clear clientAddr */
	else if(pack_seq==pack_total) {
			//printf("\nFinish one frame!\n");
			trans_data.ready=false;
	}
	else {
		EGI_PDEBUG(DBG_TEST, " trans_data.ready:%s, pack_seq:%d, pack_total:%d\n",
					trans_data.ready ? "true":"false", pack_seq, pack_total );
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
	void *ptr=NULL;

	/* 1. Start request IPACK. Usually this will be sent twice before receive reply from the Server.  */
	if( !run_session ) {
		pack_count=0;

		/* Nest IPACK into sndBuff, fill in header info. */
		//void *ptmp=sndBuff;
		ipack_snd=(EGI_INETPACK *)sndBuff;
		ipack_snd->headsize=sizeof(PRIV_HEADER); /* Or IPACK_HEADER() may return NULL */
		header_snd=IPACK_HEADER(ipack_snd);
		header_snd->code=CODE_REQUEST_VIDEO;

		/* Update IPACK packsize, just a header! */
		ipack_snd->packsize=sizeof(EGI_INETPACK)+sizeof(PRIV_HEADER);

		/* Set sndsize */
		*sndSize=ipack_snd->packsize;
		if(verbose_on) printf("Finish preping request ipack.\n");

		/* DO NOT return here, need to proceed on to receive reply/data next! */
	}

	/* 2. Receive file data */
	if( rcvSize > 0 ) {
		//EGI_PDEBUG(DBG_TEST,"Received %d bytes data.\n", rcvSize);

		/* No authenication */
		run_session=true;

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
					printf("pack_count=%d: Receive pack[%d](%dBs), of total %d packs, ~%jd KBytes.\n",
					    	pack_count, header_rcv->seq, ipack_rcv->packsize, header_rcv->total, header_rcv->fsize>>10);
					fflush(stdout);
				}

				/* 2. Create ebits for the first time. */
				/* WARNING!!! Assume that number of packs for each image frame is the SAME! */
				if( ebits==NULL ) {
					ebits=egi_bitstatus_create(header_rcv->total);
					if(ebits==NULL) exit(EXIT_FAILURE);
					if(verbose_on) printf("Create ebits OK.\n");
				}

				/* 2. Check and allocate dest_data each time, since fsize MAY be differenct for each ipack! */
				if( dest_size < header_rcv->fsize ) {
					ptr=realloc(dest_data, header_rcv->fsize);
					if(ptr==NULL) {
						printf("Fail to realloc dest_data!\n");
						exit(EXIT_FAILURE);
					}
					else {
						dest_data=ptr;
						dest_size=header_rcv->fsize;
					}
				}
				/* 2. Allocate rgb24 once! */
				if( rgb24==NULL ) {
					rgb24=calloc(1, cam_args.width*cam_args.height*3);
					if(rgb24==NULL) {
					        printf("Fail to calloc rgb24!\n");
						exit(EXIT_FAILURE);
					}
				}

				/* 3. Checking seq number, only to see if it's out of sequence, proceed on... */
				if(  header_rcv->seq != pack_count ) {
					//printf("\nOut of sequence! pack_seq=%d, expect seq=%d.\n",
					//		header_rcv->seq, pack_count );
					/* count fails */
					//cnt_disorders ++;
				}

		#if 0//////* 4. Check ebits to see if corresponding pack already received. */
				if( egi_bitstatus_getval(ebits, header_rcv->seq)==1 ) {
					printf("Ipack[%d] already received, skip it!\n",header_rcv->seq);
					break;
				}
		#endif
				/* Get pack privdata size */
				datasize=IPACK_PRIVDATASIZE(ipack_rcv);
				//printf("datasize=%d, fsize=%jd\n", datasize, header_rcv->fsize);

				/* 5. Check stamp */
				if( header_rcv->stamp != egi_bitstatus_checksum(IPACK_PRIVDATA(ipack_rcv),datasize) )
				{
					printf("	xxxxx Ipack[%d] stamp/checksum error! xxxxx\n", header_rcv->seq);
					//exit(EXIT_FAILURE);
				}

				if( IPACK_PRIVDATA(ipack_rcv)==NULL ) {
					printf("NO data in ipack_rcv->privdata\n");
					exit(EXIT_FAILURE);
				}

				/* 6. Write to dest_data, offset as per the sequence number. */
				if(header_rcv->seq > header_rcv->total-1) {
					printf("Seq > header_rcv->total-1! Error.\n");
					break;
				}
				else if(header_rcv->seq != header_rcv->total-1 ) {   /* Not tail, packsize is the same. */
					memcpy( dest_data + header_rcv->seq * datasize,
						IPACK_PRIVDATA(ipack_rcv),
						datasize
					      );
				}
				else {	/* Write the tail */
					memcpy( dest_data + header_rcv->fsize - datasize,  /* now, datasize is tailsize */
						IPACK_PRIVDATA(ipack_rcv),
						datasize
					      );
				}

				/* 7. Take seq number as index of ebits, and set 1 to the bit . */
				if( egi_bitstatus_set(ebits, header_rcv->seq)!=0 ) {
					printf("Bitstatus set index=%d fails!\n", header_rcv->seq);
				}
				else {
					//EGI_PDEBUG(DBG_TEST,"Bitstatus set seq=%d\n",header_rcv->seq);
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
			    	ebits->pos=-1;
				if( egi_bitstatus_posnext_zero(ebits) ==0 && ebits->pos != pack_count )
			    	{
					lost_seq = ebits->pos;
					printf("Pack[%d] is lost!\n",lost_seq);

#if 0 //////////////////////////////// Retry lost_pack, Nest IPACK into sndBuff, fill in header info. */
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
					//cnt_retrys++;

					/* Return to routine to sendto() immediately. */
					return 0;
#endif /////////////////////////////////////////////////

				}

				/* 10. If get last pack_seq, then complete one frame, convert data to rgb24 and display it. */
				if(header_rcv->seq==header_rcv->total-1) {
					/* Check ebits again! */
				    	ebits->pos=-1;
				    	if( egi_bitstatus_posnext_zero(ebits) ==0 ) {
						lost_seq = ebits->pos;
						printf("Pack[%d] is lost!\n",lost_seq);
					}
					else {
						//EGI_PDEBUG(DBG_TEST,"OK, ebits all ONEs!\n");
					}

 				   /* dest_data format: YUYV */
				   if(trans_format==V4L2_PIX_FMT_YUYV) {
					/* Display Video */
					egi_color_YUYV2RGB888(dest_data, rgb24, cam_args.width, cam_args.height, false);
					egi_imgbuf_showRBG888(rgb24, cam_args.width, cam_args.height, &gv_fb_dev, px0, py0);  /* Direct FBwrite */
					fb_render(&gv_fb_dev);
				   }
				   /* dest_data format: V4L2_PIX_FMT_MJPEG  */
				   else {
					show_jpg(NULL, dest_data, header_rcv->fsize, &gv_fb_dev, 0, px0, py0);
					fb_render(&gv_fb_dev);

					/* Reset pack_count */
					pack_count=0;
				   }
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

#if 0   ////////// request lost_pack /////////////////////////////////
		/* WARNING!!! If the Server finish sending out all packs at this point, then we have
		 * to check any lost pack here, NOT in if(rcvSize>0){... } part!
		 * Pick out first ZERO bit in the ebits, ebits->pos is the missed seq number.
		 * Note: This may be NOT a lost ipack, but just the next ipack in sequence.
		 */
		    	ebits->pos=-1;
		    	if( egi_bitstatus_posnext_zero(ebits) ==0 ) {
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
#endif ///////////////////////////////////////////

		}  /* END if(pack_count>0) */

	}

	return 0;
}

