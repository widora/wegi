/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of decoding H264 NALU stream data with libffmpeg,
only I_frame is decoded here.

Reference:
1. https://videoexpert.tech/ffmpeg-libav-example-h-264-decoder/
2. https://blog.csdn.net/zhaoyun_zzz/article/details/87302600

Note:
1. H264 (AnnexB mode) byte-stream NALU(Network Abstract Layer Unit):
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
   Suggest for 10s Duration ts: -m 10 -d 0 ... if 10 I_frames per segment.


TODO:
XXX 1. For some h264 stream data, decoding error:
     ----- nalType: 5 ----
[h264 @ 0x840070] top block unavailable for requested intra4x4 mode -1 at 0 0
[h264 @ 0x840070] error while decoding MB 0 0, bytestream 9425
[h264 @ 0x840070] concealing 240 DC, 240 AC, 240 MV errors in I frame

2. IDR frame is NOT the only I_frame, to find other I_Frames in mid of GOP.


Journal:
2022-07-07: Create the file.
2022-07-08:
	1. SWS scale frame image and convert to RGB16LE
	2. Pick out IDR frames.
2022-07-10:
	1.  Add ff_readNALU()
2022-07-12:
	1. Add input option
	2. Add imgbuf for fb_render.
2022-07-26:
	1. Time cost analysis.
2022-08-31:
	1. Monitoring test.


Midas Zhou
------------------------------------------------------------------*/
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
//#include "libavutil/timestamp.h"
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
#include <egi_bjp.h>

#define LOOP_DECODING   1    /* Loop accessing a.h264/b.h264 and decoding */
#define NALU_MAX_DATASIZE  (1024*1024)

/* TEST: ------------- */
int startpos;

/*----------------------------------------------------------------------
Read out a NALU from a H264 data file, then put it to buff.

Reference:
	https://videoexpert.tech/ffmpeg-libav-example-h-264-decoder/

Note:
1. A H264 NALU unit = [Start_Code]+[NALU Header]+[NALU Payload]
   Start_Code(3or4Bytes):
        If starts of GOP: 0x00000001, with 3bytes zeros
        Else 0x000001, with 2bytes zeros (OR maybe 3bytes zeros?)
   NALU_Header(1Byte):
	bits[7]:    F    0-OK, 1-Error
	bits[5-6]:  NRI
	bits[0-4]:  Type  5-IDR, 6-SEI, 7-SPS ,8-PPS

@fin:		FILE that holds h264 data.
@out:   	Buffer to hold a NALU unit read out from fin.
		NALU_Header + NALU_Payload
		If fails, it keeps old data.
@HeadByte:	To hold the NALU_Header.
		If fails, it returns value 0x00.

Return:
	<0	Fails, or NOT found, or EOF.
		The caller MUST call feof(fin) to check which is the case.
	>0	Ok, Length of this NALU,excluding startcode.
-----------------------------------------------------------------------*/
static int ff_readNALU(FILE* fin, unsigned char* out, unsigned char* HeadByte)
{
	int zbcnt=0;  /* To conunter zero bytes  */
	int pos=0;
	bool FoundNextStartCode=false;

    	/* 1. Reset HeadByte first */
    	*HeadByte=0;

    	/* 2. Case DATA ERROR: Skip leading nonzero bits and get to start of start_code..
	 *    Leading nonzero bits SHOULD NOT happen in normal cases,
	 *    instead it should begin with zeros as NALU start_code.
	 */
     	while(!feof(fin) && fgetc(fin)!=0);
	if(feof(fin)) return -1;
     	fseek (fin, -1, SEEK_CUR); /* rewind the '0' */

        /* Read start_code of this NALU  */
        while( !feof(fin) && (out[pos++]=fgetc(fin))==0 );
	if(feof(fin)) return -1;

    	/* Read HeadByte of this NALU */
    	out[pos++]=fgetc(fin);
    	*HeadByte = out[pos-1];

	/* Read payload data for this NALU */
	while(!FoundNextStartCode) {
		/* EOF, no next NALU header */
        	if (feof(fin))
            		return pos; ///Excluding next NAUL_Header //pos-4; //return -1;

		/* Read one byte */
		out[pos++]=fgetc(fin);

		/* Check start code */
		if(out[pos-1]>1) /* For most cases: NOT 0x00 OR 0x01 */
			zbcnt=0; /* Reset zbscnt */
		else if(out[pos-1]==0)
			zbcnt++;
		else { /* out[pos-1]==1 */
			if(zbcnt>1) /* 2 OR 3 BYTES zeros alread */
				FoundNextStartCode=true;
			else
				zbcnt=0; /* Reset zbscnt */
		}
	}
	/* NOW: It reads all NEXT Start_Code */

    	/* Rewind to the start position of next NAUL_Header */
    	fseek (fin, -(zbcnt+1), SEEK_CUR);

	/* Length of this NALU */
    	return pos - (zbcnt+1); /* deduct start_code bytes */
}

#if 0 //////// Original codes at https://videoexpert.tech/ffmpeg-libav-example-h-264-decoder ///////
inline int FindStartCode (unsigned char *Buf, int zeros_in_startcode)
{
  int info;
  int i;

  info = 1;
  for (i = 0; i < zeros_in_startcode; i++)
    if(Buf[i] != 0)
      info = 0;

  if(Buf[i] != 1)
    info = 0;
  return info;
}

inline int getNextNal(FILE* fin, unsigned char* Buf, unsigned char* HeadByte)
{
    int pos = 0;

    /* Reset HeadByte */
    *HeadByte=0;

    /* >>>> Skip nonzero bits and get to start of start_code */
     while(!feof(fin) && fgetc(fin)!=0);
     if(feof(fin)) return 0;
     fseek (fin, -1, SEEK_CUR);

    /* Read start_code of this NALU  */
    while(!feof(fin) && (Buf[pos++]=fgetc(fin))==0);

    int StartCodeFound = 0;
    int info2 = 0;
    int info3 = 0;

    /* Read HeadByte of this NALU */
    if(feof(fin))
	return -1;
    Buf[pos++]=fgetc(fin);
    *HeadByte = Buf[pos-1];

    while (!StartCodeFound)
    {
	/* Get EOF */
        if (feof(fin)) {
	    return pos; //Excluding next NAUL_Header, return pos-4;  return -1;
	}
        Buf[pos++] = fgetc (fin);
	/* Satrtcode: 0x00000001, with 3bytes zeros.*/
        info3 = FindStartCode(&Buf[pos-4], 3);
	/* Satrtcode: 0x000001, with 2bytes zeros.*/
        if(info3 != 1)
            info2 = FindStartCode(&Buf[pos-3], 2);

        StartCodeFound = (info2 == 1 || info3 == 1);
    }

    /* Rewind back to start of next NAUL_Header */
    //XXX fseek(fin, -4, SEEK_CUR);
    fseek(fin, info2==1 ? -3 : -4, SEEK_CUR);

    //startpos += pos-3 or -4;

    return pos - (info2?3:4);
}
#endif ///////////////////////////////////////////////////////////////////////


/*---------------------------
	 Main
---------------------------*/
int main(int argc, char **argv)
{
    FILE * fin;
    FILE * fout;

    unsigned char nalHeadByte;  /* NALU Header Byte */
    uint8_t nalType;   /* nal_unit_type */
    int nalLen = 0;    /* NALU lenght, including Header */
    int nfs=0;   /* Frame counter */
    float tscost=0.0; /* Time for decoding/scale/display a Key Frame, in seconds. */
    float tsall=0.0; /* Total time cost for proceeding one Key Frame, in seconds. */
    struct timeval tms, tme;


    int rawBuffSize=NALU_MAX_DATASIZE; /*  Max. h264 data size for 1 frame. 1920x1080  */
    unsigned char *rawbuff=NULL;  /* To hold raw h264 frame data */
    unsigned char* outbuff=NULL;  /* To hold pixel data, to copy from an AVPicture */

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

    AVPacket packet;
    int  gotFrames=0;  /* Frames decoded */
    int  ret;
    int  fcnt=0;  /* decoding frame counter */

    EGI_IMGBUF *imgbuf=NULL;
    int  maxdf=1; /* Max IRD frames to display */
    int  ds=0.0;    /* Delay is second, default 0s */

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
			printf("  m:  Max. IRD frames to display for a segment\n");
			printf("  d:  Delay in seconds between each displaying\n");
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

    /* 4. Allocate raw h264 frame data buffer */
    rawbuff  = (unsigned char*)calloc (rawBuffSize, sizeof(char));
    if(rawbuff==NULL) exit(1);

#if (!LOOP_DECODING)   /* Open h246 data file */
    fin = fopen(argv[1], "rb");
    if(fin==NULL)
	exit(1);
#endif

    /* 5. Open file for output  */
    fout = fopen("test.rgb", "wb");
    if(fout==NULL)
	exit(1);


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
    fin = fopen(fpath, "rb");
    if(fin==NULL) {
	remove(fpath);
	usleep(250000);
	goto LOOP_ACCESS;
    }
#endif

    /* 13. Read h264 packets and decode it */
    while(!feof(fin))
    {
	/* 13.1 Check if the corresponding audio a.aac/b.aac already played out.
	 * Try to keep up (sync) with audio ... HK2022-07-25
	 */
	if( strcmp(fpath,"/tmp/a.h264")==0 ) {
		if( access("/tmp/a.aac", R_OK)!=0 )
			break;
	} else if ( access("/tmp/b.aac", R_OK)!=0 ) {
		break;
	}

	/* 13.2 Clear rawbuff */
	bzero(rawbuff, rawBuffSize);

READ_NALU:
	/* 13.3 Read packet data into rawbuff */
//        printf("Get next Nal...\n");
        //nalLen = getNextNal(fin, rawbuff, &nalHeadByte); /* See ref. */
	nalLen = ff_readNALU(fin, rawbuff, &nalHeadByte);
//	printf("NALU length =%dBytes, HeadByte: 0x%02X, startpos=%d\n", nalLen, nalHeadByte, startpos);
	if(nalLen<0) {
	    if(feof(fin))
		printf("Finish NALU data!\n");
	    else
		printf("NALU data error!\n");
	    goto END_FUNC;
	}

	/* 13.4 Pick out IRD(Instanenous Decoding Refresh) frames, and SPS/PPS params. IDR frame needs SPS/PPS to reset decoding parameters... */
	nalType = nalHeadByte&0b00011111;
	printf(" ----- nalType: %d ----\n", nalType);
	//if( nalType==1 || nalType==5 || nalType==6 || nalType==7 || nalType==8  ) {  /* 1-XXX, 5-IDR,6-SEI,7-SPS,8-PPS */
	//if( nalType==1 || nalType==5 || nalType==7 || nalType==8  ) {  /* 1-XXX, 5-IDR,6-SEI,7-SPS,8-PPS */

#if LOOP_DECODING
	if( nalType==5 || nalType==7 || nalType==8  ) {  /* 1-XXX, 5-IDR,6-SEI,7-SPS,8-PPS */
		//printf(" ----- nalType: %d ----\n", nalType);
		//Go on...
	}
	else {
	    	if(nalType==1) { /* Non IDR frame or I_frame */
			usleep(25000);  //1s/25=40ms=40000us, left time for decoding
			fcnt++;
	    	}
		goto READ_NALU;  /* To skip mem clear operation. */
	}

	/* 13.5 Delay count down */
	if( nalType==5 ) {
	#if 1   /* Deduct time delay */
		/* fcnt==0 first IDR frame  */
		if( fcnt>0 && fcnt/25.0 < ds+tsall)  /* Suppose frame rate 25fps + Decoding_IDRFrame_time */
			goto READ_NALU; /* To skip mem clear operation. */
		else { /* Ok, go on to play this key frame */
			fcnt=0;
		}
	#else  /* Display each IDR  */
		if(fcnt>0) {  /* fcnt==0 first IDR frame  */
			//goto READ_NALU;
			fcnt=0;
		}
		//else fcnt==0, goon..
	#endif
	}
#else
	/* Pass down all NALUs */
#endif

	/* Every 30 packets --- NOPE!!! Must decode in sequence! */

	/* 13.6 Init packet data */
        av_init_packet(&packet);
        packet.data = rawbuff;
        packet.size = nalLen;

#if 0  ////////// TEST:  Decoding IDR Frame with libjpeg, Not possible, unless for private I_frame slice with jpeg data  //////////////////
if(nalType==5) {
	int width,height;
	int components;
	unsigned char *imgdat=NULL;
	int k;

	for(k=1; k<nalLen; k++) {
		if( rawbuff[k]==(unsigned char)0xFF && rawbuff[k+1]==(unsigned char)0xD8 ) {
			printf("JPEG SOI at k=%d\n", k);
		}
	}
	printf("JPEG SOI NOT found!\n");
	continue;


	/* nalLen including 1Byte Header, memmove to make 2Bytes for JPEG SOI (FF D8); */
	memmove(rawbuff+2, rawbuff+1, nalLen-1);
	rawbuff[0]=0xFF; rawbuff[1]=0xD8;

        imgdat=open_buffer_jpgImg(rawbuff, nalLen-1, &width, &height, &components );
	if(imgdat) {
		printf("Picture width=%d, heigh=%d, components=%d\n", width, height, components);
	} else {
		printf("open_buffer_jpgImg fails!\n");
	}

	free(imgdat);
	continue;
}
#endif ////////////////////////////////////////////////////////////

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

#if 0	/* Copy picture data to outbuff */
	    if(outbuff==NULL) {
		yuvBuffSize=pCodecCtx->width*pCodecCtx->height*1.5;
		outbuff = (uint8_t *)av_malloc(yuvBuffSize);
	    }

	    /* NOTE: avpicture_layout() can NOT change pic format and size!! */
            avpicture_layout((AVPicture *)pFrameYUV, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, outbuff,
     					pCodecCtx->width*pCodecCtx->height*1.5);

	    /* Write to file */
//            fwrite(outbuff, pCodecCtx->width*pCodecCtx->height*1.5, 1, xxfout);
#endif

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

		/* Save RGB565LE raw data */
//	        fwrite(rgbBuffer, pFrameRGB->width*pFrameRGB->height*sizeof(EGI_16BIT_COLOR), 1, fout);

		gettimeofday(&tms, NULL);

		/* Display imgbuf */
		egi_subimg_writeFB(imgbuf, &gv_fb_dev, 0, -1, 0,0);
		fb_render(&gv_fb_dev);

	        gettimeofday(&tme, NULL);
		tscost=tm_signed_diffms(tms, tme)/1000.0;
		tsall += tscost;
		egi_dpstd(DBG_YELLOW"Display cost %.3f seconds.\n"DBG_RESET, tscost);

		egi_dpstd(DBG_YELLOW"tsall = %.3f seconds.\n"DBG_RESET, tsall);

	    }

            printf("nfs: %d\n", ++nfs);
//	    system("cat /tmp/test.rgb >/dev/fb0");

	    /* Rewind fout to start pos */
	    rewind(fout);

#if LOOP_DECODING /* TEST: 1 Frame only -----------------------*/
	   //sleep(3); // 10s per segment
//	   sleep(ds);
           if(nfs>=maxdf) goto END_FUNC;
#endif

        }
	else { /* decode error */
		printf(DBG_YELLOW"ret=%d, %s \n"DBG_RESET, ret, pFrameYUV->linesize[0]==0 ? "linesize[0]==0" : "" );
	}
    }


END_FUNC: /////////////////////////////////////////////////////

#if LOOP_DECODING  /* LOOP_ACCESS: ------------------- */
	/* Reset nfs */
	nfs=0;

	/* Remove a.h264/b.h264 and close fin */
	remove(fpath);
	fclose(fin);

	goto LOOP_ACCESS;

#endif

    /* Colse file */
    fclose(fin);
    fclose(fout);

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
