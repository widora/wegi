/*----------------------------------------------------------------------
Reference:
  超群天晴    https://www.cnblogs.com/surpassal/archive/2012/12/19/zed_webcam_lab1.html
  JWH SMITH   http://jwhsmith.net/2014/12/capturing-a-webcam-stream-using-v4l2

A USBCAM test program, support format MJPEG and YUYV.

Usage exmample:
	./test_usbcam -d /dev/video0 -s 320x240 -r 30
	./test_usbcam -d /dev/video0  -r 30 -f yuyv -v

Pan control:
	'a'  Pan left
	'd'  Pan right
	'w'  Pan up
	's'  Pan down
	'j'  Save current frame to a JPEG file.


Note:
1. Apply YUYV to convert RGB saves much CPU load than apply JPEG.
2. Command 'v4l2-ctl -d0 --list-ctrls' to see all supported contrls.
3. It supports unplug_and_plug operation of usbcams.
4. If you run 2 or 3 USBCAMs at the same time, it may appear Err'No space left on device',
   which indicates that current USB controller not have enough bandwidth.
   Try to use smaller image size and turn down FPS.

TODO:
1. Control quality of MJPEG.

Midas Zhou
------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

#include <egi_input.h>
#include <egi_fbdev.h>
#include <egi_image.h>
#include <egi_bjp.h>
#include <egi_FTsymbol.h>
#include <egi_timer.h>

/* 申请的帧缓存页数 */
#define REQ_BUFF_NUMBER	2	/* required buffers to be allocated. */

/* 用来记录帧缓存的地址和大小 */
struct buffer {
        void *                  start;
        size_t                  length;
};
struct buffer *buffers;
unsigned long bufindex;

/* 摄像头的控制变量和对应数值 */
struct v4l2_control vctrl_params[] =
{
  { V4L2_CID_BRIGHTNESS,	50 },
  { V4L2_CID_CONTRAST,		32 },
  { V4L2_CID_SATURATION,	60 }, //40
  { V4L2_CID_SHARPNESS,		3  },
  { V4L2_CID_HUE,		4  },
  { V4L2_CID_GAMMA,		150 },
  { V4L2_CID_EXPOSURE_AUTO_PRIORITY,	1 },
  { V4L2_CID_AUTO_WHITE_BALANCE,1  },
};


/* 显示标题和时间戳 */
void draw_marks(void);


/*---------------------------
        Print help
-----------------------------*/
void print_help(const char* cmd)
{
        printf("Usage: %s [-hvtd:s:r:f:o:b:] \n", cmd);
        printf("        -h   help \n");
	printf("	-v:  Reverse upside down, default: false. for YUYV only.\n");
	printf("	-t:  Display time, default: false.\n");
        printf("        -d:  Video device, default: '/dev/video0' \n");
        printf("        -s:  Image size, default: '320x240' \n");
        printf("        -r:  Frame rate, default: 10fps \n");
        printf("        -f:  Pixel format, default: V4L2_PIX_FMT_MJPEG \n");
        printf("             'yuyv' or 'mjpeg' \n");
	printf("	-o:  Origin position to the screen, default: 0,0 \n");
	printf("	-b:  Brightness in percentage, default: 80(%%) \n");
}


/*-----------------------------
	      MAIN
------------------------------*/
int main (int argc,char ** argv)
{
	char *dev_name = "/dev/video0";
	int fd_dev;	/* Video device */
	int fd_jpg;	/* Saved JPG file */
   	fd_set fds;
	char strtmp[256];
	//unsigned char *ptr=NULL;

   	//char fname[64]="usbcam.jpg";
	unsigned char *rgb24=NULL;
	unsigned char *jpgdata=NULL;
	unsigned long jpgdata_size;

	struct v4l2_queryctrl		queryctrl={0};
	struct v4l2_control		vctrl;
     	struct v4l2_capability 		cap;
     	struct v4l2_format 		fmt;
     	struct v4l2_fmtdesc 		fmtdesc;
     	struct v4l2_requestbuffers 	req;
     	struct v4l2_buffer 		bufferinfo;
     	struct v4l2_streamparm 		streamparm;
	struct v4l2_jpegcompression	jpgcmp;
	struct v4l2_cropcap		cropcap;
	struct v4l2_crop		crop;
     	enum v4l2_buf_type type;

	struct timeval			tv;
	struct timeval          	timestamp;
	struct v4l2_timecode    	timecode;
#if 0
struct v4l2_timecode {
        __u32   type;
        __u32   flags;
        __u8    frames;
        __u8    seconds;
        __u8    minutes;
        __u8    hours;
        __u8    userbits[4];
};
#endif
	unsigned int 	i;
	int	opt;
	char	*pt;
	char	ch;

	/* 默认参数 */
	/* Defaut params: pxiformat, image size and frame rate */
	int 	pixelformat=V4L2_PIX_FMT_MJPEG;
	int	width=320;
	int	height=240;
	int	fps=10;
	int	brightpcnt=80;	/* brightness in percentage */
	bool	reverse=false;
	bool	marktime=false;
	int	x0=0,y0=0;


	/* 程序选项 */
        /* Parse input option */
        while( (opt=getopt(argc,argv,"hvtd:s:r:f:o:b:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
			case 'v':
				reverse=true;
				break;
			case 't':
				marktime=true;
				break;
                        case 'd':	/* Video device name */
				dev_name=optarg;
				break;
			case 's':	/* Image size */
				width=atoi(optarg);
				if((pt=strstr(optarg,"x"))!=NULL)
					height=atoi(pt+1);
				else
					height=atoi(strstr(optarg,"X"));
				break;
			case 'r':	/* Frame rate */
				fps=atoi(optarg);
				printf("Input fps=%d\n", fps);
				break;
			case 'f':
				if(strstr(optarg, "yuyv"))
					pixelformat=V4L2_PIX_FMT_YUYV;
				break;
			case 'o':
				x0=atoi(optarg);
				y0=atoi(strstr(optarg, ",")+1);
				break;
			case 'b':
				brightpcnt=atoi(optarg);
				if(brightpcnt<30)brightpcnt=30;
				if(brightpcnt>100)brightpcnt=100;
				break;
		}
	}

  	/* Set termI/O 设置终端为直接读取单个字符方式 */
  	egi_set_termios();

        /* Init sys FBDEV  初始化FB显示设备 */
        if( init_fbdev(&gv_fb_dev) )
                return -1;

	/* Load FT derived sympg_ascii 加载ASCII字体 */
	if(FTsymbol_load_allpages() !=0) {
		//go on anyway..., return -1;
	}

	/* 设置显示模式: 是否直接操作FB映像数据， 设置横竖屏 */
        /* Set FB mode */
	fb_set_directFB(&gv_fb_dev, true);
      	fb_position_rotate(&gv_fb_dev, 0);

OPEN_DEV:
	/* 1. 打开摄像头设备文件 */
	/* Open video device */
     	while ( (fd_dev = open(dev_name, O_RDWR)) <0 ) {
      	  	printf("Fail to open '%s', Err'%s'.\n", dev_name, strerror(errno));
		usleep(200000);
		//return -1;
	}

	/* 2. 获取设备驱动支持的操作 */
	/* Get video driver capacity */
      	if( ioctl (fd_dev, VIDIOC_QUERYCAP, &cap) !=0) {
      	  	printf("Fail to ioctl VIDIOC_QUERYCAP, Err'%s'\n", strerror(errno));
	  	return -1;
      	}
	printf("\n\t--- Driver info ---\n");
  	printf("Driver Name:\t%s\nCard Name:\t%s\nBus info:\t%s\nCapibilities:\t0x%04X\n",cap.driver,cap.card,cap.bus_info, cap.capabilities);
      	if( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) {  /* 是否支持视频捕获 */
	      	printf("The video device does NOT supports capture.\n");
		  	return -1;
      	}
      	if( !(cap.capabilities & V4L2_CAP_STREAMING) ) {      /* 是否支持数据流控 streaming I/O ioctls */
	      	printf("The video device does NOT support streaming!\n");
		return -1;
	}

	/*　3. 在这里查询/设置摄像头的各项控制变量: 亮度，对比度，饱和度，色度等等． */
#if 1	/* Set contrast/brightness/saturation/hue/sharpness/gamma/...
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
			printf("%s:	min=%d	max=%d	default=%d  value=%d\n",
					queryctrl.name, queryctrl.minimum, queryctrl.maximum, queryctrl.default_value,
					vctrl.value);
		}

	#if 1	/* 将亮度设置到其值域的80% */
		/* Change brightness to 80% of its range. */
		if(vctrl.id==V4L2_CID_BRIGHTNESS) {
			vctrl.value=queryctrl.minimum+(queryctrl.maximum-queryctrl.minimum)*brightpcnt/100;
			ioctl( fd_dev, VIDIOC_S_CTRL, &vctrl);
		}
	#endif

	#if 0	/* Set controls 设置其他控制变量的值 */
		vctrl.id=vctrl_params[i].id;
	        vctrl.value=vctrl_params[i].value;
        	if( ioctl( fd_dev, VIDIOC_S_CTRL, &vctrl) !=0 ) {
	      	  	printf("Fail to ioctl VIDIOC_S_CTRL, Err'%s'\n", strerror(errno));
		}
	#endif
	}
#endif

	/* Get JPEG compression params */
	if( ioctl( fd_dev, VIDIOC_G_JPEGCOMP, &jpgcmp ) !=0 ) {
      	  	printf("Fail to ioctl VIDIOC_G_JPEGCOMP, Err'%s'\n", strerror(errno));
	}
	else
		printf("JPEG quality: %d.\n", jpgcmp.quality);


	/* 4. 设置视频数据流格式: 模式及相关参数 */
	/* Set data format */
     	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;	/* 视频捕捉模式 */
     	fmt.fmt.pix.width       = width;			/* 视频尺寸 */
     	fmt.fmt.pix.height      = height;
     	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;	/* 隔行交替 */
     	fmt.fmt.pix.pixelformat = pixelformat;			/*  MJPEG or YUYV */
     	if( ioctl (fd_dev, VIDIOC_S_FMT, &fmt) !=0) {
    		printf("Fail to ioctl VIDIOC_S_FMT, Err'%s'\n", strerror(errno));
		goto END_FUNC;
		//return -2;
	}
     	if( ioctl( fd_dev, VIDIOC_G_FMT, &fmt) !=0 ) {
    		printf("Fail to ioctl VIDIOC_G_FMT, Err'%s'\n", strerror(errno));
		goto END_FUNC;
		//return -2;
	}
	else {
		/* 打印实际设置生效的参数 */
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

	/* 5. 获取生效的实际画面尺寸 */
	/* Confirm working width and height */
	width=fmt.fmt.pix.width;
	height=fmt.fmt.pix.height;

	#if 0 /* TODO: 设置取景范围　Crop capture video */
	memset(&cropcap,0,sizeof(typeof(cropcap)));
	cropcap.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if( ioctl( fd_dev, VIDIOC_CROPCAP, &cropcap) !=0) {
		printf("Fail to ioctl VIDIOC_CROPCAP, Err'%s'\n", strerror(errno));
	} else
		printf("Crop bounds: (%d,%d) %dx%d\n",
			cropcap.bounds.left,cropcap.bounds.top,cropcap.bounds.width,cropcap.bounds.height);
	memset(&crop,0,sizeof(typeof(crop)));
	crop.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c.left=20;
	crop.c.top=20;
	crop.c.width=200;
	crop.c.height=200;
	if( ioctl( fd_dev, VIDIOC_S_CROP, &crop) !=0) {
    		printf("Fail to ioctl VIDIOC_S_CROP, Err'%s'\n", strerror(errno));
	}
	#endif

	/* 6. 设置流控参数FPS */
     	/* Set stream params: frame rate */
     	memset(&streamparm, 0, sizeof(streamparm));
     	streamparm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	streamparm.parm.capture.timeperframe.denominator=fps;
     	streamparm.parm.capture.timeperframe.numerator=1;
     	if( ioctl( fd_dev, VIDIOC_S_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_S_PARM, Err'%s'\n", strerror(errno));
		goto END_FUNC;
		//return -3;
	}
	if( ioctl( fd_dev, VIDIOC_G_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_G_PARM, Err'%s'\n", strerror(errno));
		goto END_FUNC;
		//return -3;
	}
	else {
		printf("Frame rate:\t%d/%d fps\n", streamparm.parm.capture.timeperframe.denominator,
							streamparm.parm.capture.timeperframe.numerator);
	}

	/* 7. 枚举支持的视频格式 */
     	/* Enumerate formats supported by the vide device */
     	fmtdesc.index=0;
     	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	printf("The device supports following formats for video captrue:\n");
     	while( ioctl( fd_dev, VIDIOC_ENUM_FMT, &fmtdesc)==0 ) {
		printf("\t%d.%s\n", fmtdesc.index+1, fmtdesc.description);
		fmtdesc.index++;
     	}

	/* 8. 申请帧缓存 */
	/* Allocate buffers */
     	req.count               = REQ_BUFF_NUMBER;
     	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	req.memory              = V4L2_MEMORY_MMAP;
     	if( ioctl (fd_dev, VIDIOC_REQBUFS, &req) !=0) {
    		printf("Fail to ioctl VIDIOC_REQBUFS, Err'%s'\n", strerror(errno));
		//return -4;
		goto END_FUNC;
	}
	/* Actual buffer number allocated */
	printf("Req. buffer ret/req:\t%d/%d \n", req.count, REQ_BUFF_NUMBER);


	/* 获取帧缓存的大小和地址，并将其保存到buffers结构中 */
	/* Allocate buffer info */
     	buffers = calloc (req.count, sizeof (*buffers));
	/* Get buffer info */
	memset(&bufferinfo, 0, sizeof(bufferinfo));
     	for (bufindex = 0; bufindex < req.count; ++bufindex)
     	{
           	bufferinfo.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
           	bufferinfo.memory      = V4L2_MEMORY_MMAP;
           	bufferinfo.index       = bufindex;

           	if( ioctl (fd_dev, VIDIOC_QUERYBUF, &bufferinfo) !=0) {
	    		printf("Fail to ioctl VIDIOC_QUERYBUF, Err'%s'\n", strerror(errno));
			return -4;
		}
           	buffers[bufindex].length = bufferinfo.length;

		/* Mmap to user space. the device MUST support V4L2_CAP_STREAMING mode! */
		buffers[bufindex].start = mmap (NULL, bufferinfo.length, PROT_READ | PROT_WRITE,
			                        	                MAP_SHARED, fd_dev, bufferinfo.m.offset);
		if(buffers[bufindex].start == MAP_FAILED) {
	    		printf("Fail to mmap buffer, Err'%s'\n", strerror(errno));
			return -4;
		}

		/* Clear it */
		memset(buffers[bufindex].start, 0, bufferinfo.length);
     	}
	/* Size of allocated video buffer */
	printf("Video buffer size:\t%d(Bytes)\n", bufferinfo.length);

	/* 9. 启动捕获视频，注意:有些设备必须将帧缓存先放入队列后才能启动！ */
	/* Start to capture video.  some devices may need to queue the buffer at first! */
    	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	if( ioctl (fd_dev, VIDIOC_STREAMON, &type) !=0) {
    		printf("Fail to ioctl VIDIOC_STREAMON, Err'%s'\n", strerror(errno));
		//return -5;
		goto END_FUNC;
	}

	/* 10. 将帧缓存放入工作队列，以供摄像头写入数据． */
	/* Queue buffer after turn on video capture! */
       	for (i = 0; i < REQ_BUFF_NUMBER; ++i) {
               bufferinfo.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               bufferinfo.memory      = V4L2_MEMORY_MMAP;
               bufferinfo.index       = i;
               if( ioctl (fd_dev, VIDIOC_QBUF, &bufferinfo) !=0) {
		    	printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));
			goto END_FUNC;
			//return -4;
		}
	}

   	/* Reset index, from which it starts the loop. */
   	bufferinfo.index=0;


		/* 循环例程: 从摄像头读取视频数据，将其输出到ＦＢ显示出来． */
        	/* ---------- Loop reading video frames and display --------*/

/* 1. V4L2_PIX_FMT_YUYV格式读取和显示循环例程序 */
/* 1. --- pixelformat: V4L2_PIX_FMT_YUYV --- */
if( pixelformat==V4L2_PIX_FMT_YUYV )
{
   printf("Output format:\tYUYV, reverse=%s\n", reverse?"Yes":"No");

   /* 为RBG888数据申请内存 */
   rgb24=calloc(1, width*height*3);
   if(rgb24==NULL) {
	printf("Fail to calloc rgb24!\n");
	goto END_FUNC;
   }

   jpgdata_size=width*height*3;
   jpgdata=calloc(1, jpgdata_size);
   if(jpgdata==NULL) {
	printf("Fail to calloc jpgdata!\n");
	goto END_FUNC;
   }


   /* 循环: 帧缓存出列-->读数据转码成RGB888--->帧缓存入列--->显示图像 */
   /* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
   for(;;) {
	/* Parse keyinput 键盘输入 */
	ch=0;
  	read(STDIN_FILENO, &ch,1);
  	switch(ch) {
		case 'a':	/* Shift left */
			x0 +=20;
			break;
		case 'd':	/* Shift right */
			x0 -=20;
			break;
		case 'w':	/* Shift up */
			y0 +=20;
			break;
		case 's':	/* Shift down */
			y0 -=20;
			break;
		case 'j':	/* Save the image to jpeg file */
			tm_get_strtime2(strtmp, ".jpg");
			//compress_to_jpgFile(strtmp, 80, width, height, rgb24, JCS_RGB);
			/* Convert YUYV to YUV, then to JPEG */
			egi_color_YUYV2YUV(buffers[bufferinfo.index].start, rgb24, width, height, false); /* rbg24 for dest YUV data */
			compress_to_jpgFile(strtmp, 80, width, height, rgb24, JCS_YCbCr);
			break;
		case 'm':	/* TEST: Compress image to jpg data */
			compress_to_jpgBuffer(&jpgdata, &jpgdata_size,
							100, width, height, rgb24, JCS_RGB);
			printf("jpgdata_size: %ldk\n", jpgdata_size>>10);
			sleep(1);
			break;
        	case 'q':       /* Quit */
        	case 'Q':
			exit(0);
                	break;
	}

	/* Check fd_dev */

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
		if(marktime) draw_marks();	/* 加上标题时间戳 */
		fb_render(&gv_fb_dev);  	/* 刷新Framebuffer */
	}

   }

} /* End  pixelformat==V4L2_PIX_FMT_YUYV */

/* 2. V4L2_PIX_FMT_MJPEG格式读取和显示循环例程序 */
/* 2. --- pixelformat: V4L2_PIX_FMT_MJPEG --- */
else
{
   printf("Output format:\tMJPEG\n");

   /* 循环: 帧缓存出列-->解码显示JPG图像--->帧缓存入列 */
   /* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
   for(;;) {

	/* Parse keyinput 键盘输入 */
	/* Parse keyinput */
	ch=0;
  	read(STDIN_FILENO, &ch,1);
  	switch(ch) {
		case 'a':	/* Shift left */
			x0 +=20;
			break;
		case 'd':	/* Shift right */
			x0 -=20;
			break;
		case 'w':	/* Shift up */
			y0 +=20;
			break;
		case 's':	/* Shift down */
			y0 -=20;
			break;
		case 'j':	/* Save the image to jpeg file */
			tm_get_strtime2(strtmp, ".jpg");
		   	fd_jpg = open(strtmp, O_RDWR | O_CREAT, 0777);
			#if 0 /* End of jpeg, buffers[].length NOT include 0xFF and 0xD9 */
			for(pt=buffers[bufferinfo.index].start; pt-(char *)buffers[bufferinfo.index].start<buffers[bufferinfo.index].length; pt++) {
				if(*pt==0xFF && *(pt+1)==0xD9) break;
			}
			printf("buffer len=%d, Jpg file len=%d Bytes\n",
					buffers[bufferinfo.index].length, pt-(char *)buffers[bufferinfo.index].start);
			#endif
        		write(fd_jpg, buffers[bufferinfo.index].start, buffers[bufferinfo.index].length);
   			close(fd_jpg);
			//exit(0);
			break;
		case 'm':	/* TEST: Compress image to jpg data */

			break;
        	case 'q':       /* Quit */
        	case 'Q':
                	exit(0);
                	break;
	}


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
		if(marktime) draw_marks();	/* 加上标题时间戳 */
		fb_render(&gv_fb_dev); 		/* 刷新Framebuffer */
	}

	/* 将当前帧缓存放入工作队列，以供摄像头写入数据．*/
        /* Queue the buffer */
        if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0) {
                printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));

	}


   #if 0  /* Display image */
	#if 1
   	show_jpg(fname, NULL, 0, &gv_fb_dev, 0, 0, 0);
	//usleep(5000);
	#else
	imgbuf=egi_imgbuf_readfile(fname);
	//egi_imgbuf_windisplay2( imgbuf, &gv_fb_dev, 0, 0, 0, 0, imgbuf->width, imgbuf->height);
	egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0, 0);
   	fb_render(&gv_fb_dev);
	egi_imgbuf_free2(&imgbuf);
	#endif
   #endif
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
   close (fd_jpg);
   close (fd_dev);

   usleep(500000);
goto OPEN_DEV;

   fb_filo_flush(&gv_fb_dev);
   release_fbdev(&gv_fb_dev);

   /* 释放字符映像 */
   symbol_release_allpages();

   egi_reset_termios();

   return 0;
}


/*------------------------------------------
显示标题和时间戳
Put titles and time stamp on the FB image.
------------------------------------------*/
void draw_marks(void)
{
	char strtmp[32];

	/* 标题 */
        symbol_string_writeFB(&gv_fb_dev, &sympg_ascii,
                               WEGI_COLOR_BLUE, -1,     		/* fontcolor, int transpcolor */
                               10, 5,          			/* int x0, int y0 */
                               "EGi:  A Linux Player", 255);      /* string, opaque */
	/* 时间戳 */
	tm_get_strdaytime(strtmp);
        symbol_string_writeFB(&gv_fb_dev, &sympg_ascii,
                                WEGI_COLOR_ORANGE, -1,     /* fontcolor, int transpcolor */
                                320/2-20, 240-20,          /* int x0, int y0 */
                                strtmp, 255);           /* string, opaque */
}
