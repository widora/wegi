/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of decoding H264 NALU stream data with libffmpeg.

Example:
If there are 10 Key frames in each 10s segment file:
    ./test_decode264X -m 10 -d 1.0

Reference:
1. https://videoexpert.tech/ffmpeg-libav-example-h-264-decoder/
2. https://blog.csdn.net/zhaoyun_zzz/article/details/87302600

Note:
1. H264 Annexb byte-stream NALU:
   A H264 NALU unit(A H264_Frame) = [Start_Code]+[NALU Header]+[NALU Payload]

   1.1 NALU: Network Abstraction Layer Unit
   1.2 Start_Code:
        If starts of GOP: 0x00000001, with 3bytes zeros
        Else 0x000001, with 2bytes zeros (NOT necessary?)
   1.3 NALU_Header(1Byte):
	bits[7]:    F    0-OK, 1-Error
	bits[5-6]:  NRI
	bits[0-4]:  Type  5-IDR, 6-SEI, 7-SPS ,8-PPS

2. Time cost for 1920x1080x25fps I_frames, format h264 (High):
	Key frame decoding  0.145 seconds.
	Scaling             0.090 seconds.
	Displaying          0.039 seconds.
	Ttotal 		    0.274 seconds.

   Number of key frames in each segement varys, it depends on original
   video setting.

TODO:
1. IDR frame is NOT the only I_frame, to find other I_Frames in mid of GOP.

Journal:
2022-07-29: Create the file.
2022-07-31: usleep(duspf) for each frame.
2022-08-31: Monitoring test.

Midas Zhou
------------------------------------------------------------------*/
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
//#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
//#include "libavfilter/avfiltergraph.h"
//#include "libavfilter/buffersink.h"
//#include "libavfilter/buffersrc.h"
//#include "libavutil/opt.h"
#include <egi_color.h>
#include <egi_debug.h>
#include <egi_image.h>
#include <egi_timer.h>

#define LOOP_DECODING   1    /* Loop accessing a.h264/b.h264 and decoding */
#define NALU_MAX_DATASIZE  (1024*1024)

/* TEST: ------------- */
int startpos;

AVFormatContext *pFormatCtx;

/*---------------------------
	 Main
---------------------------*/
int main(int argc, char **argv)
{
    FILE * fin;

    int nfs=0;   /* Frame counter */
    float tscost=0.0; /* Time for decoding/scale/display a Key Frame, in seconds. */
    float tsopen=0.0; /* Time for open the buffer file */
    float tsall=0.0; /* Total time cost for proceeding one Key Frame, in seconds. */
    struct timeval tms, tme;
    int  duspf=0; /* duration in us, per frame */

    AVCodec *codec=NULL;
    AVCodecContext *pCodecCtx=NULL;
    AVFrame *pFrameYUV=NULL;
    AVFrame *pFrameRGB=NULL;
    AVDictionary *opts=NULL;

    uint8_t *yuvBuffer=NULL;
    uint8_t *rgbBuffer=NULL;
    int rgbBuffSize;
    int yuvBuffSize;
    struct SwsContext  *SwsCtx=NULL;
    float framerate=0.0;

    AVPacket packet;
    int  gotFrames=0;  /* Frames decoded */
    int  ret;
    int  fcnt=0;  /* decoding frame counter */

    EGI_IMGBUF *imgbuf=NULL;
    int  maxdf=20; /* Max IRD frames to display */
    float  ds=0.0;    /* Delay is second, default 0s */

/* --------- Monitoring test ----------------- */
    #include <egi_utils.h>
    #include <sys/wait.h>
    char strbuff[256];
    pid_t fpid;
    int status;
    int retpid;

    fpid=fork();
    if(fpid<0) {
	printf("Fail to fork!\n");
	exit(1);
    }
    else if(fpid>0) {
	printf("Start monitoring child process...\n");
	egi_append_file("/tmp/test.log", "Startlog...\n", strlen("Startlog...\n"));

	while(1) {
		retpid=waitpid(fpid, &status, 0);
                /* Exit normally */
                if(WIFEXITED(status)){
			system("killall radio_aacdecode"); //it rename a._h264 to a.h264, so to keep a.h264
			sprintf(strbuff,"Child %d Exit normally!\n", retpid);
			egi_append_file("/tmp/test.log", strbuff, strlen(strbuff));
			exit(1);
                }
                /* Terminated by signal, in case APP already terminated. */
                else if( WIFSIGNALED(status) ) {
			system("killall radio_aacdecode"); //it rename a._h264 to a.h264, so to keep a.h264
			sprintf(strbuff,"Child %d is terminated by signal!\n", retpid);
			egi_append_file("/tmp/test.log", strbuff, strlen(strbuff));
			exit(1);
                }
		else {
			sprintf(strbuff,"Child %d Status=%d\n", retpid, status);
			egi_append_file("/tmp/test.log", strbuff, strlen(strbuff));
			//exit(1);
		}
	}

    }
    //else : Child process

/* ------ END TEST ------ */


    /* 1. Input option */
    int opt;
    while( (opt=getopt(argc,argv,"hm:d:"))!=-1 ) {
     	switch(opt) {
        	case 'h':
			printf("%s hm:d: \n", argv[0]);
			printf("  m:  Max. IRD frames to display for a segment. default 20.\n");
			printf("  d:  Delay in seconds between each displaying, default 0.\n");
			exit(0);
                	break;
                case 'm':  /* Max. IRD frames to display */
                         maxdf=atoi(optarg);
                        break;
		case 'd': /* Delay in second */
			ds=atof(optarg);
			break;
        }
    }

    /* 2. Allocate imgbuf */
    imgbuf=egi_imgbuf_alloc();
    if(imgbuf==NULL) exit(1);

    /* 3. Init FBDEV */
    if( init_fbdev(&gv_fb_dev) )
            return -1;
    fb_set_directFB(&gv_fb_dev, false);
    fb_position_rotate(&gv_fb_dev, 0);

    /* 4. Register all formats and codecs, before calling avformat_open_input() */
    av_register_all();
    avformat_network_init();

    /*--------- avcodec register and open routine -------
     * avcodec_register_all();
     * av_dict_set(&opts, "b", "2.5M", 0);
     * codec = avcodec_find_decoder(AV_CODEC_ID_H264);
     * if (!codec)
     *     exit(1);
     *
     * context = avcodec_alloc_context3(codec);
     *
     * if (avcodec_open2(context, codec, opts) < 0)
     *     exit(1);
     *--------------------------------------------------*/

    /* 5. Avcodec register */
    printf("avcodec register ...\n");
    avcodec_register_all();
    //av_dict_set(&opts, "b", "2.5M", 0);
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)  {
        return 0;
    }

    /* 6. Allocate codec context */
    printf("Allocate codec context...\n");
    pCodecCtx = avcodec_alloc_context3(codec);
    if(!pCodecCtx){
        return 0;
    }

    /* 7. Open codec */
    printf("avcodec open2...\n");
    //if (avcodec_open2(pCodecCtx, codec, &opts) < 0) {
    if (avcodec_open2(pCodecCtx, codec, NULL) < 0) {
        return 0;
    }

    /* 8. Allocate frame */
    printf("av_frame_alloc() pFrameYUV and pFrameRGB...\n");
    pFrameYUV = av_frame_alloc();
    pFrameRGB = av_frame_alloc();
    if(!pFrameYUV || !pFrameRGB){
        return 0;
    }

    /* 9. Assign width/height for pFrameRGB, for further SWS. */
    pFrameRGB->width=320; pFrameRGB->height=240;

    /* 10. Malloc rgbBuffer */
    rgbBuffSize=avpicture_get_size(AV_PIX_FMT_RGB565LE, pFrameRGB->width, pFrameRGB->height);
    rgbBuffer=(uint8_t *)av_malloc(rgbBuffSize);

    /* 11. Assign imgbuf */
    imgbuf->width=pFrameRGB->width;
    imgbuf->height=pFrameRGB->height;
    imgbuf->imgbuf=(EGI_16BIT_COLOR *)rgbBuffer;

#if LOOP_DECODING   /* ------ For test_http: h246 data file ------ */
    const char *fpath;

LOOP_ACCESS:
    /* 12. Check buffer files */
    if(access("/tmp/a.h264",R_OK)==0)
        fpath="/tmp/a.h264";
    else if(access("/tmp/b.h264",R_OK)==0)
	fpath="/tmp/b.h264";
    else {
        printf("No h264 file, wait...\n");
        usleep(250000);
        goto LOOP_ACCESS;
    }

    /* 12a. Open buffer file */
    printf("avformat_open_input...\n");
    gettimeofday(&tms, NULL);
    if( avformat_open_input(&pFormatCtx, fpath, NULL, NULL)!=0 ) {
	egi_dperr(DBG_RED"Fail to open input file, or file unrecognizable!"DBG_RESET);
	avformat_close_input(&pFormatCtx);
        sleep(1);
	goto LOOP_ACCESS;
    }
    gettimeofday(&tme, NULL);
    tsopen=tm_signed_diffms(tms, tme)/1000.0;
    tsopen +=0.5; // To catch up...
    printf("tsopen=%.3fs\n", tsopen);

#endif

    /* 13. Read h264 packets and decode it. ONLY 1 stream here. */
    while( av_read_frame(pFormatCtx, &packet)==0 ) {

	/* 13.1 Check if the corresponding audio a.aac/b.aac already played out.
	 * Try to keep up (sync) with audio ... HK2022-07-25
	 */
	if( strcmp(fpath,"/tmp/a.h264")==0 ) {
		if( access("/tmp/a.aac", R_OK)!=0 )
			break;
	} else if ( access("/tmp/b.aac", R_OK)!=0 ) {
		break;
	}

	/* Get frame rate */
	framerate= av_q2d(pCodecCtx->framerate);
	//printf("Framerate: %f \n", framerate);
	if(framerate>1.0) /* Initial framerate=0 */
		duspf=1000000/framerate;
	else
		duspf=0;

	/* 13.2 Check packet type */
	if( packet.flags & AV_PKT_FLAG_KEY ) {  /* TODO: NOT including all I_frames! */
		/* Following is obsolete, use pCodecCtx->framerate instead */
		/* Duration of this packet, in AVStream->time_base units. TODO: strange, 25fps, duspf=0.08s 
		 */
		//duspf=1000000*av_q2d(pFormatCtx->streams[0]->time_base)*packet.duration; /* in us */
		//duspf=500000*av_q2d(pFormatCtx->streams[0]->time_base)*packet.duration; /* in us */

		printf("Key frame\n");

		if(tsopen>0.0) {  /* Deduct open time */
			tsopen -= 1.0*duspf/1000000;;
		}
		else {  /* Deduct decoding time */
			if(tsall>0.0)  /* To catch up, initial tsall=0.0 */
				tsall -= 1.0*duspf/1000000;
			else /* Hold on */
				usleep(duspf);
		}

		//NOTE: av_q2d():  static inline double av_q2d(AVRational a){ return a.num / (double) a.den; }
	}
	else {  /* Other pciture frames. */
		/* OBSOLETE, use pCodecCtx->framerate instead */
		//duspf=1000000*av_q2d(pFormatCtx->streams[0]->time_base)*packet.duration; /* in us */
		//duspf=500000*av_q2d(pFormatCtx->streams[0]->time_base)*packet.duration; /* in us */

		if(tsopen>0.0) {  /* Deduct open time */
			tsopen -= 1.0*duspf/1000000;;
		}
		else {  /* Deduct decoding time */
			if(tsall>0.0)  /* To catch up, initial tsall=0.0 */
				tsall -= 1.0*duspf/1000000;
			else /* Hold on */
				usleep(duspf);
		}

//		printf("NonKey frame\n");

		/* Free packet and continue */
	        av_free_packet(&packet);

		continue;
	}

	/* 13.7 Init tsall, approx. time cost for decoding a frame. */
	tsall=0.0;

	/* 13.8 Decode h264 rawbuff frame */
	gettimeofday(&tms, NULL);
        ret= avcodec_decode_video2(pCodecCtx, pFrameYUV, &gotFrames, &packet);
	gettimeofday(&tme, NULL);
	tscost=tm_signed_diffms(tms, tme)/1000.0;
	tsall += tscost;
	egi_dpstd(DBG_YELLOW"Decoding cost %.3f seconds.\n"DBG_RESET, tscost);
        if(ret>0 && pFrameYUV->linesize[0]!=0)
        {
	    printf("gotFrames=%d, avpicture_layout...\n", gotFrames);

	    /* Init SwsCtx */
	    if(SwsCtx==NULL) {
		printf("pCodecCtx WxH: %dx%d\n",pCodecCtx->width, pCodecCtx->height);
            	SwsCtx = sws_getContext( pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                        	          320, 240, AV_PIX_FMT_RGB565LE,
                                          SWS_BILINEAR,
                            	          NULL, NULL, NULL);
	    }

	    /* SWS scale image */
	    if(SwsCtx) {
		gettimeofday(&tms, NULL);
		/* Assign rgbBuffer to pFrmaeRGB, !!! pFrameRGB->width/height MUST be assigned manually! */
		avpicture_fill((AVPicture *)pFrameRGB, rgbBuffer, AV_PIX_FMT_RGB565LE, pFrameRGB->width, pFrameRGB->height); //320,240
		/* Scale picture */
		sws_scale(SwsCtx, (uint8_t const * const *)pFrameYUV->data, pFrameYUV->linesize, 0, pFrameYUV->height,
				pFrameRGB->data, pFrameRGB->linesize);
	        gettimeofday(&tme, NULL);
		tscost=tm_signed_diffms(tms, tme)/1000.0;
		tsall += tscost;
		egi_dpstd(DBG_YELLOW"Scale cost %.3f seconds.\n"DBG_RESET, tscost);

		/* !!! pFrameRGB->width/height MUST have been assigned manually before! */
		printf("pFrameRGB WxH: %dx%d\n", (*pFrameRGB->linesize)/sizeof(EGI_16BIT_COLOR), pFrameRGB->height);

		/* Display imgbuf */
		gettimeofday(&tms, NULL);
		egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);
		fb_render(&gv_fb_dev);
	        gettimeofday(&tme, NULL);
		tscost=tm_signed_diffms(tms, tme)/1000.0;
		tsall += tscost;
		egi_dpstd(DBG_YELLOW"Display cost %.3f seconds.\n"DBG_RESET, tscost);

		egi_dpstd(DBG_YELLOW"tsall = %.3f seconds.\n"DBG_RESET, tsall);
	    }

            printf("nfs: %d\n", ++nfs);


#if LOOP_DECODING /* TEST: 1 Frame only -----------------------*/
    #if 0 //////////// NO Need, to usleep(duspf) for each frame ////////
	   float dsec;
	   float fptr;
	   int dus;

	   dsec=ds-tsall; /* deduct tsall */
	   if(dsec<0.0)
		dsec=0.0;
	   dus=1000000.0*modff(dsec, &fptr);
	   egi_dpstd(DBG_MAGENTA"<<< Hold on: dsec=%ds, dus=%dus >>>\n"DBG_RESET, (int)dsec, (int)dus);

	   sleep(dsec);
	   usleep(dus);
    #endif ////////////////////////////////////////////////////////////////

           if(nfs>=maxdf) goto END_FUNC;
#endif

        }
	else { /* decode error */
		printf(DBG_YELLOW"ret=%d, %s \n"DBG_RESET, ret, pFrameYUV->linesize[0]==0 ? "linesize[0]==0" : "" );
	}

        /* Free OLD packet each time */
	printf("av_free_packet...\n");
        av_free_packet(&packet);

    }


END_FUNC: /////////////////////////////////////////////////////

#if LOOP_DECODING  /* LOOP_ACCESS: ------------------- */
	/* Reset nfs */
	nfs=0;

	/* Close pFormatCtx */
	avformat_close_input(&pFormatCtx);

	/* Remove a.h264/b.h264 and close fin */
	remove(fpath);

	goto LOOP_ACCESS;
#endif

    /* Colse file */
    //fclose(fin);

    /* Free frame and buffer */
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrameRGB);
    av_free(rgbBuffer);
    av_free(yuvBuffer);

    /* Free SWS Context */
    sws_freeContext(SwsCtx);

    /* Close Codec Context */
    avcodec_close(pCodecCtx);

    return 1;
}
