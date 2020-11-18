/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program to through UDP

Usage exmample:
	./test_usbcam -d /dev/video0 -s 320x240 -r 30
	./test_usbcam -d /dev/video0  -r 30 -f yuyv -v

Pan control:
	'a'  Pan left
	'd'  Pan right
	'w'  Pan up
	's'  Pan down


Note:

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


/* 申请的帧缓存页数 */
#define REQ_BUFF_NUMBER	4	/* Buffers to be allocated for the CAM */

struct thread_cam_args {
	char	*dev_name;
	int 	pixelformat;
	int 	width;
	int	height;
	int	fps;
};

//int usbcam_routine(char * dev_name, int width, int height, int pixelformat, int fps );
void* usbcam_routine(void *args);

int Server_Callback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                           struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize);

int Client_Callback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                                              char *sndBuff, int *sndSize );


/*---------------------------
        Print help
-----------------------------*/
void print_help(const char* cmd)
{
        printf("Usage: %s [-hvtd:s:r:f:o:b:] \n", cmd);
        printf("        -h   help \n");
	printf("	-v:  Reverse upside down, default: false. for YUYV only.\n");
        printf("        -d:  Video device, default: '/dev/video0' \n");
        printf("        -s:  Image size, default: '320x240' \n");
        printf("        -r:  Frame rate, default: 10fps \n");
        printf("        -f:  Pixel format, default: V4L2_PIX_FMT_MJPEG \n");
        printf("             'yuyv' or 'mjpeg' \n");
	printf("	-b:  Brightness in percentage, default: 80(%%) \n");
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
	struct thread_cam_args cam_args;
	cam_args.dev_name="/dev/video0";
	cam_args.pixelformat=V4L2_PIX_FMT_YUYV;//MJPEG;
	cam_args.width=320;
	cam_args.height=240;
	cam_args.fps=10;

	/* UDP server */
	EGI_UDP_SERV 	*userv=NULL;

        int     brightpcnt=80;  /* brightness in percentage */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hvtd:s:r:f:o:b:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
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
                                if(strstr(optarg, "yuyv"))
                                        cam_args.pixelformat=V4L2_PIX_FMT_YUYV;
                                break;
                        case 'b':
                                brightpcnt=atoi(optarg);
                                if(brightpcnt<30)brightpcnt=30;
                                if(brightpcnt>100)brightpcnt=100;
                                break;
                }
        }

        /* Init sys FBDEV */
        if( init_fbdev(&gv_fb_dev) )
                return -1;

        /* Set FB mode */
        fb_set_directFB(&gv_fb_dev, true);
        fb_position_rotate(&gv_fb_dev, 0);

	/* Start thread CAM */
	printf("Start CAM routine thread...\n");
	if( pthread_create(&thread_cam, NULL, usbcam_routine, &cam_args) !=0 ) {
		printf("Fail to create thread_cam, Err'%s'\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Create a UDP server */
	userv=inet_create_udpServer(NULL, 5678, 4);
	if(userv==NULL)
		exit(EXIT_FAILURE);

	/* Set callback */
	userv->callback=Server_Callback;

	/* Start routine */
	printf("Start UDP server routine...\n");
	inet_udpServer_routine(userv);

	/* CAM routine */
	printf("Start CAM routine job...\n");
//	usbcam_routine(dev_name, width, height, pixelformat, fps );

	/* Joint thread */
	void *ret=NULL;
	if( pthread_join(thread_cam, &ret)!=0 ) {
		printf("Fail pthread_join, ret=%d, Err'%s'\n", *(int *)ret, strerror(errno));
	}

	/* Release FBDEV */
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
	unsigned char *rgb24=NULL;
	unsigned int i;


	int	brightpcnt=80;	/* brightness in percentage */
	bool	reverse=false;
	bool	marktime=false;
	int	x0=0,y0=0;

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

	/* To store allocated video buffer address/size */
	struct buffer {
        	void *                  start;
        	size_t                  length;
	};
	struct buffer *buffers;
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
		return -1;
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
		return (void *)-2;
	}
     	if( ioctl( fd_dev, VIDIOC_G_FMT, &fmt) !=0 ) {
    		printf("Fail to ioctl VIDIOC_G_FMT, Err'%s'\n", strerror(errno));
		return (void *)-2;
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
		printf("Display origin:	(%d,%d)\n", x0,y0);
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
		return (void *)-3;
	}
	/* Confirm working FPS */
	if( ioctl( fd_dev, VIDIOC_G_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_G_PARM, Err'%s'\n", strerror(errno));
		return (void *)-3;
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
		return (void *)-4;
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
			return (void *)-4;
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

	/* 9. Start to capture video.  some devices may need to queue the buffer at first! */
    	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	if( ioctl (fd_dev, VIDIOC_STREAMON, &type) !=0) {
    		printf("Fail to ioctl VIDIOC_STREAMON, Err'%s'\n", strerror(errno));
		return (void *)-5;
	}

	/* 10. Queue buffer after turn on video capture! */
       	for (i = 0; i < REQ_BUFF_NUMBER; ++i) {
               bufferinfo.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               bufferinfo.memory      = V4L2_MEMORY_MMAP;
               bufferinfo.index       = i;
               if( ioctl (fd_dev, VIDIOC_QBUF, &bufferinfo) !=0) {
		    	printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));
			return (void *)-4;
		}
	}

   	/* Reset index, from which it starts the loop. */
   	bufferinfo.index=0;

        	/* ---------- Loop reading video frames and display --------*/

/* 1. --- pixelformat: V4L2_PIX_FMT_YUYV --- */
if( pixelformat==V4L2_PIX_FMT_YUYV )
{
   printf("Output format: YUYV, reverse=%s\n", reverse?"Yes":"No");

   /* 为RBG888数据申请内存 */
   rgb24=calloc(1, width*height*3);
   if(rgb24==NULL) {
	printf("Fail to calloc rgb24!\n");
	goto END_FUNC;
   }

   /* 循环: 帧缓存出列-->读数据转码成RGB888--->帧缓存入列--->显示图像 */
   /* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
   for(;;) {

	/* 等待数据 */
   	FD_ZERO (&fds);
   	FD_SET (fd_dev, &fds);

	/* Select() will do  nothing if fd_dev is closed by other thread! */
	tv.tv_sec=1; tv.tv_usec=0;
   	if( select(fd_dev + 1, &fds, NULL, NULL, &tv) <0) {
		fprintf(stderr, "Device file abnormal, Err'%s'.\n", strerror(errno));
	}

	/* 将当前帧缓存从工作队列出取出 */
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
			/* Unmap and close file */
   			for (i = 0; i < REQ_BUFF_NUMBER; ++i)
      				munmap (buffers[i].start, buffers[i].length);
			close(fd_dev);
			goto OPEN_DEV;
		}
	}
	else {
		/* 获取帧时间戳 */
		/* Get frame timestamp. it's uptime!  */
		printf("Ts: %ld.%01ld \r", bufferinfo.timestamp.tv_sec, bufferinfo.timestamp.tv_usec/100000 );
		//printf("Tc: %02d:%02d:%02d \r");
		//		bufferinfo.timecode.hours, bufferinfo.timecode.minutes, bufferinfo.timecode.seconds);
		fflush(stdout);

		/* 将当前缓存里的数据转化成RGB888格式放入到rbg24中 */
		/* Convert YUYV to RGB888 */
		egi_color_YUYV2RGB888(buffers[bufferinfo.index].start, rgb24, width, height, reverse);

		/* 将当前帧缓存放入工作队列，以供摄像头写入数据．*/
        	/* Queue the buffer */
        	if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0)
	                printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));

		/* 显示图像 */
		/* DirectFB write */
		egi_imgbuf_showRBG888(rgb24, width, height, &gv_fb_dev, x0, y0);
		fb_render(&gv_fb_dev);  	/* 刷新Framebuffer */
	}

   }

} /* End  pixelformat==V4L2_PIX_FMT_YUYV */

/* 2. V4L2_PIX_FMT_MJPEG格式读取和显示循环例程序 */
/* 2. --- pixelformat: V4L2_PIX_FMT_MJPEG --- */
else if( pixelformat==V4L2_PIX_FMT_MJPEG )
{
   printf("Output format: MJPEG\n");

   /* 循环: 帧缓存出列-->解码显示JPG图像--->帧缓存入列 */
   /* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
   for(;;) {

	/* 等待数据 */
   	FD_ZERO (&fds);
   	FD_SET (fd_dev, &fds);
   	select(fd_dev + 1, &fds, NULL, NULL, NULL);

	/* 将当前帧缓存从工作队列出取出 */
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
			/* Unmap and close file */
   			for (i = 0; i < REQ_BUFF_NUMBER; ++i)
      				munmap (buffers[i].start, buffers[i].length);
			close(fd_dev);
			goto OPEN_DEV;
		}
	}
	else {
		/* 直接解码显示缓存中的JPG图像数据 */
		show_jpg(NULL, buffers[bufferinfo.index].start, buffers[bufferinfo.index].length, &gv_fb_dev, 0, x0, y0);
		fb_render(&gv_fb_dev); 		/* 刷新Framebuffer */
	}

	/* 将当前帧缓存放入工作队列，以供摄像头写入数据．*/
        /* Queue the buffer */
        if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0)
                printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));
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


   return 0;
}


/*-----------------------------------------------------------------
A TEST callback routine for UPD server.

@rcvAddr:       Client address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndAddr:       Client address, to which sndBuff will be sent.
@sndBuff:       Data to send to sndAddr
@sndSize:       Data size of sndBuff

Return:
        0       OK
        <0      Fails
------------------------------------------------------------------*/
int Server_Callback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                           struct sockaddr_in *sndAddr,       char *sndBuff, int *sndSize)
{
	bool verbose_on=false;

	#if 0	/* NOT necessary NOW!  	 If a new client */
	static int toksize=0;
	if( toksize==0 && rcvSize>0 ) {
		printf("New client from: '%s:%d'.\n", inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port));
		toksize=1;
	}
	#endif

	/* Check rcvSize */
	if(rcvSize>0) {
		if(verbose_on)
			printf("Received %d bytes data.\n", rcvSize);
	}
	else if(rcvSize==0) {
		/* A Client probe from EGI_UDP_CLIT means a client is created! */
		printf("Client Probe from: '%s:%d'.\n", inet_ntoa(rcvAddr->sin_addr), ntohs(rcvAddr->sin_port));
	}

#if 0	/* If Client_to_Server only, reply a small packet to keep alive. */
	if(trans_mode==mode_C2S) {
		*sndSize=4;
		*sndAddr=*rcvAddr;
		return 0;
	}
#endif

	/* TEST FOR SPEED: whatever, just request to send back ... */
	*sndSize=EGI_MAX_UDP_PDATA_SIZE;
	/* Use default rcvAddr data in EGI UDP SERV buffer */
	*sndAddr=*rcvAddr;
	/* If rcvSize<0, then rcvAddr would be meaningless, whaterver, use default value in EGI UDP SERV.
	 * However, if *rcvAddr is meaningless, EGI_UDP_SERV will NOT send! */

	return 0;
}

/*-------------------------------------------------------------------
A TEST callback routine for UPD client.
@rcvAddr:       Server/Sender address, from which rcvData was received.
@rcvData:       Data received from rcvAddr
@rcvSize:       Data size of rcvData.

@sndBuff:       Data to send to UDP server
@sndSize:       Data size of sndBuff

Return:
        0       OK
        <0      Fails
--------------------------------------------------------------------*/
int Client_Callback( const struct sockaddr_in *rcvAddr, const char *rcvData, int rcvSize,
                                                              char *sndBuff, int *sndSize )
{
	bool verbose_on=false;

	/* Check rcvSize */
	if(rcvSize>0) {
		if(verbose_on)
			printf("Received %d bytes data.\n", rcvSize);
	}

#if 0	/* If Server_to_Client only. Reply a small packet to keep alive. */
	if(trans_mode==mode_S2C) {
		*sndSize=4;
		return 0;
	}
#endif

	/* TEST FOR SPEED: whatever, just request to send back ... */
	*sndSize=EGI_MAX_UDP_PDATA_SIZE;
	/* Use default data in EGI UDP CLIT buffer */

	return 0;
}
