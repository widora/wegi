/*------------------------------------------------------------------
Reference:
  JWH SMITH   http://jwhsmith.net/2014/12/capturing-a-webcam-stream-using-v4l2
  超群天晴    https://www.cnblogs.com/surpassal/archive/2012/12/19/zed_webcam_lab1.html


A USBCAM test program.

Usage exmample:
	./test_usbcamjpg -d /dev/video0 -s 320x240 -r 30

TODO:
1. YUYV to RGB.
2. Stream jpg decoding.


Midas Zhou
------------------------------------------------------------------*/
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


#define REQ_BUFF_NUMBER	5	/* required buffers to be allocated. */

struct buffer {
        void *                  start;
        size_t                  length;
};
struct buffer *buffers;
unsigned long bufindex;

struct vctrl_id_value
{
	int id;
	int value;
};
struct vctrl_id_value vctrl_params[] =
{
  { V4L2_CID_BRIGHTNESS,	50 },
  { V4L2_CID_CONTRAST,		32 },
  { V4L2_CID_SATURATION,	60 }, //40
  { V4L2_CID_SHARPNESS,		3  },
  { V4L2_CID_HUE,		4  },
  { V4L2_CID_AUTO_WHITE_BALANCE,1  },
  { V4L2_CID_GAMMA,		150 },
  { V4L2_CID_EXPOSURE_AUTO_PRIORITY,	1 },
};


/*---------------------------
        Print help
-----------------------------*/
void print_help(const char* cmd)
{
        printf("Usage: %s [-hd:s:r:] \n", cmd);
        printf("        -h   help \n");
        printf("        -d:  Video device, default: '/dev/video0' \n");
        printf("        -s:  Image size, default: '320x240' \n");
        printf("        -r:  Frame rate, default: 10fps \n");
}


/*-----------------------------
	      MAIN
------------------------------*/
int main (int argc,char ** argv)
{
	char *dev_name = "/dev/video0";
	int fd_dev;	/* Video device */
	int fd_jpg;	/* Saved JPG file */

	struct v4l2_control		vctrl;
     	struct v4l2_capability 		cap;
     	struct v4l2_format 		fmt;
     	struct v4l2_fmtdesc 		fmtdesc;
     	struct v4l2_requestbuffers 	req;
     	struct v4l2_buffer 		bufferinfo;
     	struct v4l2_streamparm 		streamparm;
     	enum v4l2_buf_type type;

	unsigned int 	i;
	int	opt;
	char	*pt;
	char	ch;

	/* Image size and frame rate */
	int	width=320;
	int	height=240;
	int	fps=10;

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hd:s:r:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
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
		}
	}

  	/* Set termI/O 设置终端为直接读取单个字符方式 */
  	egi_set_termios();

        /* Init sys FBDEV  */
        if( init_fbdev(&gv_fb_dev) )
                return -1;

        /* Set FB position mode: LANDSCAPE  or PORTRAIT */
      	fb_position_rotate(&gv_fb_dev, 0);
	fb_set_directFB(&gv_fb_dev, true);

	/* Open video device */
     	fd_dev = open (dev_name, O_RDWR);
	if(fd_dev<0) {
      	  	printf("Fail to open '%s', Err'%s'\n", dev_name, strerror(errno));
		return -1;
	}

	/* Get video driver capacity */
      	if( ioctl (fd_dev, VIDIOC_QUERYCAP, &cap) !=0) {
      	  	printf("Fail to ioctl VIDIOC_QUERYCAP, Err'%s'\n", strerror(errno));
	  	return -1;
      	}
  	printf("Driver Name:%s\n Card Name:%s\n Bus info:%s\n\n",cap.driver,cap.card,cap.bus_info);
      	if( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) {
	      	printf("The video device does NOT supports capture.\n");
		  	return -1;
      	}
      	if( !(cap.capabilities & V4L2_CAP_STREAMING) ) {
	      	printf("The video device does NOT support streaming!\n");
		return -1;
	}

#if 1	/* Set contrast/brightness/saturation/hue/sharpness/gamma/...
	 * v4l2-ctl -d0 --list-ctrls  to see all params.
	 */
	for(i=0; i<sizeof(vctrl_params)/sizeof(struct vctrl_id_value); i++) {
		vctrl.id=vctrl_params[i].id;
	        vctrl.value=vctrl_params[i].value;
        	if( ioctl( fd_dev, VIDIOC_S_CTRL, &vctrl) !=0 ) {
	      	  	printf("Fail to ioctl VIDIOC_S_CTRL, Err'%s'\n", strerror(errno));
		}
	}
#endif

	/* Set frame format */
     	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	fmt.fmt.pix.width       = width;
     	fmt.fmt.pix.height      = height;
     	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
     	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
     	if( ioctl (fd_dev, VIDIOC_S_FMT, &fmt) !=0) {
    		printf("Fail to ioctl VIDIOC_S_FMT, Err'%s'\n", strerror(errno));
		return -2;
	}
     	if( ioctl( fd_dev, VIDIOC_G_FMT, &fmt) !=0 ) {
    		printf("Fail to ioctl VIDIOC_G_FMT, Err'%s'\n", strerror(errno));
		return -2;
	}
	else {
	     	printf("fmt.type: %d\n", fmt.type);
     		printf("pixelformat: %c%c%c%c\n",fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat>>8) & 0xFF,
						(fmt.fmt.pix.pixelformat>>16) & 0xFF, (fmt.fmt.pix.pixelformat>>24) & 0xFF);
	     	printf("Width&Height: %d&%d\n",fmt.fmt.pix.width, fmt.fmt.pix.height);
     		printf("Bytesperline: %d\n", fmt.fmt.pix.bytesperline);
     		printf("Pix field: %d\n", fmt.fmt.pix.field);
	}

     	/* Set stream params: frame rate */
     	memset(&streamparm, 0, sizeof(streamparm));
     	streamparm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	streamparm.parm.capture.timeperframe.denominator=fps;
     	streamparm.parm.capture.timeperframe.numerator=1;
     	if( ioctl( fd_dev, VIDIOC_S_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_S_PARM, Err'%s'\n", strerror(errno));
		return -3;
	}
	if( ioctl( fd_dev, VIDIOC_G_PARM, &streamparm) !=0) {
    		printf("Fail to ioctl VIDIOC_G_PARM, Err'%s'\n", strerror(errno));
		return -3;
	}
	else {
		printf("Frame rate: %d/%d fps\n", streamparm.parm.capture.timeperframe.denominator,
							streamparm.parm.capture.timeperframe.numerator);
	}

     	/* Enumerate formats supported by the vide device */
     	fmtdesc.index=0;
     	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	printf("The device supports following formats:\n");
     	while( ioctl( fd_dev, VIDIOC_ENUM_FMT, &fmtdesc)==0 ) {
		printf("\t%d.%s\n", fmtdesc.index+1, fmtdesc.description);
		fmtdesc.index++;
     	}

	/* Allocate buffers */
     	req.count               = REQ_BUFF_NUMBER;
     	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     	req.memory              = V4L2_MEMORY_MMAP;
     	if( ioctl (fd_dev, VIDIOC_REQBUFS, &req) !=0) {
    		printf("Fail to ioctl VIDIOC_REQBUFS, Err'%s'\n", strerror(errno));
		return -4;
	}

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

		/* Mmap to user space */
		buffers[bufindex].start = mmap (NULL, bufferinfo.length, PROT_READ | PROT_WRITE,
			                        	                MAP_SHARED, fd_dev, bufferinfo.m.offset);
		if(buffers[bufindex].start == MAP_FAILED) {
	    		printf("Fail to mmap buffer, Err'%s'\n", strerror(errno));
			return -4;
		}

		/* Clear it */
		memset(buffers[bufindex].start, 0, bufferinfo.length);
     	}

	/* Start to capture video.  some devices may need to queue the buffer at first! */
    	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	if( ioctl (fd_dev, VIDIOC_STREAMON, &type) !=0) {
    		printf("Fail to ioctl VIDIOC_STREAMON, Err'%s'\n", strerror(errno));
		return -5;
	}

	/* UVC queue buffer. After turn on video capture! */
       	for (i = 0; i < REQ_BUFF_NUMBER; ++i) {
               bufferinfo.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               bufferinfo.memory      = V4L2_MEMORY_MMAP;
               bufferinfo.index       = i;
               if( ioctl (fd_dev, VIDIOC_QBUF, &bufferinfo) !=0) {
		    	printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));
			return -4;
		}
	}


/*----------------- Loop reading video frames and display --------------*/
fd_set fds;
char fname[64]="usbcam.jpg";
EGI_IMGBUF *imgbuf=NULL;

bufferinfo.index=0;

/* Loop: dequeue the buffer, read out video, then queque the buffer, ....*/
for(;;) {
	/* Parse keyinput */
  	read(STDIN_FILENO, &ch,1);
  	switch(ch) {
        	case 'q':       /* Quit */
        	case 'Q':
			goto FUNC_END;
                	exit(0);
                	break;
	}

   	fd_jpg = open(fname, O_RDWR | O_CREAT, 0777);

   	FD_ZERO (&fds);
   	FD_SET (fd_dev, &fds);
   	select(fd_dev + 1, &fds, NULL, NULL, NULL);

    	/* Dequeue the buffer */
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index++;
	if(bufferinfo.index==REQ_BUFF_NUMBER)
		bufferinfo.index=0;
        if( ioctl(fd_dev, VIDIOC_DQBUF, &bufferinfo) !=0)
                printf("Fail to ioctl VIDIOC_DQBUF, Err'%s'\n", strerror(errno));
	else
        	write(fd_jpg, buffers[bufferinfo.index].start, buffers[bufferinfo.index].length);

        /* Queue the buffer */
        if( ioctl(fd_dev, VIDIOC_QBUF, &bufferinfo) !=0)
                printf("Fail to ioctl VIDIOC_QBUF, Err'%s'\n", strerror(errno));


   	close(fd_jpg);

   	/* Display image */
	#if 1
   	show_jpg(fname, &gv_fb_dev, 1, 0, 0);
	//usleep(5000);
	#else
	imgbuf=egi_imgbuf_readfile(fname);
	//egi_imgbuf_windisplay2( imgbuf, &gv_fb_dev, 0, 0, 0, 0, imgbuf->width, imgbuf->height);
	egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0, 0);
   	fb_render(&gv_fb_dev);
	egi_imgbuf_free2(&imgbuf);
	#endif
}


FUNC_END:

   /* Video stream off */
   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   if( ioctl(fd_dev, VIDIOC_STREAMOFF, &type) !=0)
   	printf("Fail to ioctl VIDIOC_DQBUF, Err'%s'\n", strerror(errno));

   /* Unmap and close file */
   for (i = 0; i < REQ_BUFF_NUMBER; ++i)
      munmap (buffers[i].start, buffers[i].length);
   free(buffers);

   close (fd_jpg);
   close (fd_dev);

   egi_reset_termios();


   return 0;
}
