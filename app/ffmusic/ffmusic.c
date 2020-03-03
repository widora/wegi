/*----------------------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Based on:
          FFmpeg examples in code sources.
				       --- FFmpeg ORG
          dranger.com/ffmpeg/tutorialxx.c
 				       ---  by Martin Bohme
	  muroa.org/?q=node/11
				       ---  by Martin Runge
	  www.xuebuyuan.com/1624253.html
				       ---  by niwenxian


TODO:
0. Only for 1 or 2 channels audio data.
1. Convert audio format AV_SAMPLE_FMT_FLTP(fltp) to AV_SAMPLE_FMT_S16.
   It dosen't work for ACC_LC decoding, lots of float point operations.

2. Can not decode png picture in a mp3 file. ...OK, with some tricks:
   !!! NOTE: Use libs of ffmpeg-2.8.15, except libavcodec.so.56.26.100 of ffmpeg-2.6.9 from openwrt !!!
   However, fail to put logo on mp3 picture by AVFilter descr.

3. FFmpeg PCM 'planar' format means noninterleaved? while 'packed' mean interleaved?
	XXX_FMT_S16P --- packed	???  interleaved
	XXX_FMT_S16  --- planar ???  noninterleaved

4. ffmusic.c and egi_pcm.c only support AV_SAMPLE_FMT_S16(P) now:
	enum AVSampleFormat {     ---  FFMPEG SAMPLE FORMAT  ---
	    AV_SAMPLE_FMT_NONE = -1,
	    AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
	    AV_SAMPLE_FMT_S16,         ///< signed 16 bits
	    AV_SAMPLE_FMT_S32,         ///< signed 32 bits
	    AV_SAMPLE_FMT_FLT,         ///< float
	    AV_SAMPLE_FMT_DBL,         ///< double

	    AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
	    AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
	    AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
	    AV_SAMPLE_FMT_FLTP,        ///< float, planar
	    AV_SAMPLE_FMT_DBLP,        ///< double, planar

	    AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
	};


NOTE:
1.  A simpley example of opening a media file/stream then decode frames and send RGB data to LCD for display.
    Files without audio stream can also be played, or audio files.
A.  IF input audio(mp3) sample rate is not 44100, then use SWR to covert it.
B.  Assume all output pcm data is nonintervealed type. (except FLTP format ?).
2.  Decode audio frames and playback by alsa PCM.
3.  DivX(DV50) is better than Xdiv. Especially in respect to decoded audio/video synchronization,
    DivX is nearly perfect.
4.  It plays video files smoothly with format of 480*320*24bit FP20, encoded by DivX.
    Decoding speed also depends on AVstream data rate of the file. ( for USB Bridged )
5.  The speed of whole procedure depends on ffmpeg decoding speed, FB wirte speed and LCD display speed.
6.  Please also notice the speed limit of your LCD controller, It's 500M bps for ILI9488???
7.  Cost_time test codes will slow down processing and cause choppy.
8.  Use unstripped ffmpeg libs.
9.  Try to play mp3 with album picture. :)
10. For stream media, avformat_open_input() will fail if key_frame is corrupted.
11. Change AVFilter descr to set clock or cclock, var. transpos_colock is vague.
12. If logo.png is too big....?!!!!
13. The final display windown H/W(row/column pixel numbers) must be multiples of 16 if AVFilter applied,
    and multiples of 2 while software scaler applied.
14. When display a picture, you shall wait for a while after finish
15. Put the subtitle file at the same directory as of the media file, the program will automatically get it
    if it exists, Only 'srt' type subtitle file is supported.



		 	(((  --------  PAGE DIVISION  --------  )))
[Y0-Y29]
{0,0},{240-1, 29}  		--- Head titlebar

[Y30-Y260]
{0,30}, {240-1, 260}  		--- Image/subtitle Displaying Zone
[Y30-Y149]  Image
[Y150-Y260] Sub_displaying

[Y150-Y265]
{0,150}, {240-1, 260} 		--- box area for subtitle display

[Y266-Y319]
{0,266}, {240-1, 320-1}  	--- Buttons


			 (((  --------  Glossary  --------  )))

FPS:		Frame Per Second
PAR:		Pixel Aspect Ratio
SAR:		Sample Aspect Ratio
	     	!!!!BUT, in avcodec.h,  struct AVCodecContext->sample_aspect_ratio is "width of a pixel divided by the height of the pixel"
DAR:		Display Aspect Ratio
PIX_FMT:	pixel format defined in libavutil/pixfmt.h
FFmpeg transpose:
		1. Map W(row) pixels to H(column) pixels , or vise versa.
           	2. For avfilter descr normal 'scale=WxH' (image upright size!!! NOT LCD WxH!!!)
	   	   !!! normal scale= image_W x image_H (image upright size!!!),
 	   	3. If transpose clock or cclock, it shall set to be 'scale=display_H x display_W',
	   	   LCD always maps display_W to row, and display_H to column.
	   	4. Finally, image_H map to display_W if clock/cclock transpose applied.

display_height,display_width:

	   	Mapped to final display WxH as of LCD row_pixel_number x column_pixel_number
	   	(NOT image upright size!!!)


		 	 (((  --------  Data Flow  --------  )))

The data flow of a movie is like this:
  (main)    	FFmpeg video decoding (~10-15ms per frame) ----> pPICBuff
  (thread)  	pPICBuff ----> FB (~xxxms per frame) ---> Display
  (thread)  	display subtitle
  (main)    	FFmpeg audio decoding ---> write to PCM ( ~2-4ms per packet?)


Usage:
	ffplay  media_file or media_address
	ffplay /music/ *.mp3
	ffplay /music/ *.mp3  /video/ *.avi
	ffplay /media/ *

	Well, try this with AUDIO off:
	ffplay http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8


Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------------------------------------------*/
#include <signal.h>
#include <math.h>
#include <libgen.h>
#include <string.h>

#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "sound/egi_pcm.h"
#include "utils/egi_cstring.h"
#include "utils/egi_utils.h"
#include "page_ffmusic.h"
#include "ffmusic_utils.h"
#include "ffmusic.h"

#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"

#define FFMUZ_LOOP_TIMEGAP  1   /*  in second, hold_on time after ffplaying a file, especially for a picture.
			         *  set before FAIL_OR_TERM.
			         */
#define FFMUZ_CLIP_PLAYTIME 3   /* in second, set clip play time */
#define FFMUZ_MEDIA_LOOP
#define END_PAUSE_TIME   0	 /* end pause time */


/*-----------  FFPLAY ENVIRONTMENT SET  --------------
 To set up first before you call thread_ffplay_music()
----------------------------------------------------*/
FFMUSIC_CONTEXT *FFmuz_Ctx=NULL;

/* FB DEV for picture displaying */
FBDEV ff_fb_dev;

int ff_sec_Vduration=0; /* in seconds, multimedia file Video duration */
int ff_sec_Aduration=0; /* in seconds, multimedia file Audio duration */
int ff_sec_Velapsed=0;  /* in seconds, playing time elapsed for Video */
int ff_sec_Aelapsed=0;  /* in seconds, playing time elapsed for Audio */

/* expected display window size, LCD will be adjusted in the function */
//static int show_w= 240; //185; /* LCD row pixels */
//static int show_h= 200; //144; //240; //185;/* LCD column pixels */

/* offset of the show window relating to LCD origin */
static int offx;
static int offy;

/* param: ( enable_audio_spectrum ) ( precondition: audio is ON and available  )
 *   if True:	run thread ff_display_spectrum() to display audio spectrum
 *   if False:	disable it.
 */
static bool enable_audio_spectrum=false;

/* seek position */
//long start_tmsecs=60*55; /*in sec, starting position */

/* param: ( enable_seek_loop )
 *   True:	loop one single file/stream forever.
 *   False:	play one time only.
 *   NOTE: 1. if TRUE, then curretn input seek postin will be ignored.!!! It always start from the
 *     	      very beginning of the file.
 *	   2. if enable_shuffle set to be true, then enable_seekloop will be false, and viceversa.
 */
/* loop seeking and playing from the start of the same file */
static bool enable_seekloop=false;

/* param: ( enable_shuffle )
 *   True:	play files in the list randomly.
 *   False:	play files in order.
 *   Note:	if enable_shuffle set to be true, then enable_seekloop will be false, and viceversa.
 */
static bool enable_shuffle=false;

/* param: ( enable_filesloop )
 *   True:      Loop playing file(s) in playlist forever, if the file is unrecognizable then skip it
 *		and continue to try next one.
 *   False:	play one time for every file in playlist.
 */
static bool enable_filesloop=true;

/* param: ( enable_audio )
 *   if True:	disbale audio/video playback.
 *   if False:	enable audio/video playback.
 */
static bool disable_audio=false;
static bool disable_video=true;

/* param: ( enable_clip_test )
 * NOTE:
 *   Clip test only for media file with audioSrteams, if no audioStream or audioStream is disabled,
 *   then is will only display the first picture from the videoStream and hold on for CLIP_PLAYTIME,
 *   then skip to next file.
 *   if True:	play the beginning of a file for FFMUZ_CLIP_PLAYTIME seconds, then skip.
 *   if False:	disable clip test.
 */
static bool enable_clip_test=false; //true;

/* Resample ON/OFF
 * True: Resample to 44.1k
 * False: Do not resample.
 * Ineffective for AV_SAMPLE_FMT_FLTP
 */
static bool enable_audio_resample=false;

/* param: ( play mode )
 *   mode_loop_all:	loop all files in the list
 *   mode_repeat_one:	repeat current file
 *   mode_shuffle:	pick next file randomly
 */
static enum ffmuz_mode playmode=mode_loop_all;


/*-----------------------------------------------------
FFplay for most types of media files:
	.mp3, .mp4, .avi, .jpg, .png, .gif, ...

thread_ffplay_music() is only for audio files.
-----------------------------------------------------*/
void * thread_ffplay_music(EGI_PAGE *page)
{

#if 0	/*------------- TEST ------------------*/
	int m=0;
	while(1) {
		m++;
		muzpage_update_timingBar(m, m);
        	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                	                        18, 18, m&0x1?"奇ODD":"偶EVEN",            /* fw,fh, pstr */
                        	                240, 2, 0,                      /* pixpl, lines, gap */
                                	        5, 10,                          /* x0,y0, */
                                        	WEGI_COLOR_GRAYC, -1, -1 );     /* fontcolor, transcolor, opaque */
		sleep(1);
		egi_page_needrefresh(page);
		sleep(1);
	}
#endif ///////////////////////////////////////////////


	/* Check ffplay context */
	if(FFmuz_Ctx==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Context struct FFmuz_Ctx is NULL!", __func__);
		return (void *)-1;
	}
	else if ( FFmuz_Ctx->fpath==NULL || FFmuz_Ctx->fpath[0]==NULL )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Context struct FFmuz_Ctx has NULL fpath!", __func__);
		return (void *)-1;
	}
	else if (FFmuz_Ctx->ftotal<=0 )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: No media file in FFmz_Ctx!", __func__);
		return (void *)-1;
	}

	/* parse mode */
	if(FFmuz_Ctx->ffmode==mode_play_once) {
		enable_seekloop=false;
		enable_filesloop=false;
	}


	printf("--------- media playing start at: %ldms ---------\n", FFmuz_Ctx->start_tmsecs);

	int ftotal=FFmuz_Ctx->ftotal; /* number of multimedia files input from shell */
	int fnum;		/* Index number of files in array FFmuz_Ctx->fpath */
	int fnum_playing;	/* Current playing fpath index, fnum may be changed by command PRE/NEXT */

	char **fpath=NULL; //FFmuz_Ctx->fpath;  /* array of media file path */
	char *fname=NULL;
	char *fbsname=NULL;

	/* for VIDEO and AUDIO  ::  Initializing these to NULL prevents segfaults! */
	AVFormatContext		*pFormatCtx=NULL;
	AVDictionaryEntry 	*tag=NULL;

	/* for VIDEO  */
	int			i;
	int			videoStream=-1; /* >=0, if stream exists */
	AVCodecContext		*pCodecCtxOrig=NULL;
	AVCodecContext		*pCodecCtx=NULL;
	AVCodec			*pCodec=NULL;
	AVFrame			*pFrame=NULL;
	AVFrame			*pFrameRGB=NULL;
	AVPacket		packet;
	int			frameFinished;
	int			numBytes;
	uint8_t			*buffer=NULL;
	struct SwsContext	*sws_ctx=NULL;
	AVRational 		time_base; /*get from video stream, pFormatCtx->streams[videoStream]->time_base*/

	/* for Pic Info. */
	struct PicInfo pic_info;
	pic_info.app_page=page; /* for PAGE wallpaper */

	/*  for AUDIO  ::  for audio   */
	int			audioStream=-1;/* >=0, if stream exists */
	AVCodecContext		*aCodecCtxOrig=NULL;
	AVCodecContext		*aCodecCtx=NULL;
	AVCodec			*aCodec=NULL;
	AVFrame			*pAudioFrame=NULL;

	/* for AVCodecDescriptor */
	enum AVCodecID		 vcodecID,acodecID;
	const AVCodecDescriptor	 *vcodecDespt=NULL;
	const AVCodecDescriptor	 *acodecDespt=NULL;

	int			frame_size;	/* number of samples in one frame */
	int 			sample_rate;
	int			out_sample_rate; /* after conversion, for ffplaypcm */
	enum AVSampleFormat	sample_fmt;
	int			nb_channels;
	int			bytes_per_sample;
	int64_t			channel_layout;
	int			nchanstr=256;
	char			chanlayout_string[256];
	int 			bytes_used;
	int			got_frame;
	struct SwrContext		*swr=NULL; /* AV_SAMPLE_FMT_FLTP format conversion */
	uint8_t			*outputBuffer=NULL;/* for converted data */
	int			dst_nb_samples;    /* destination number of samples for SWR
						    * Here it means number of frame samples??
						    */
	int 			outsamples;

	/* display window, the final window_size that will be applied to AVFilter or SWS.
	   0. Display width/height are NOT image upright width/height, their are related to LCD coordinate!
	      If auto_rotation enabled, final display WxH are mapped to LCD row_pixel_number x column_pixel_number .
	   1. Display_width/height are limited by scwidth/scheight, which are decided by original movie size
	      and LCD size limits.
	   2. When disable AVFILTER, display_width/disaply_height are just same as scwidth/scheight.
        */
	int display_width;
	int display_height;

	/* time structe */
	struct timeval tm_start, tm_end;

	/* thread for displaying RGB data */
	pthread_t pthd_displayPic;
	pthread_t pthd_displaySub;
	pthread_t pthd_audioSpectrum;
	bool pthd_displayPic_running=false;
	bool pthd_subtitle_running=false;
	bool pthd_audioSpectrum_running=false;

	char *pfsub=NULL; /* subtitle path */
	int ret;

	EGI_PDEBUG(DBG_FFPLAY," start ffplay with input ftotal=%d.\n", ftotal);

        /* prepare fb device just for FFPLAY */
        init_fbdev(&ff_fb_dev);

   	/* Register all formats and codecs, before loop for() is OK!! ??? */
	EGI_PLOG(LOGLV_INFO,"%s: Init and register codecs ...",__func__);
   	av_register_all();
   	avformat_network_init();

/*<<<<<<<<<<<<<<<<<<<<<<<< 	 LOOP PLAYING LIST    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/* loop playing all files, check if enable_filesloop==true at the end of while(1) */
while(1) {

	/* For Each Loop:  Check ffplay context and re_gain fpath */
	if(FFmuz_Ctx==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Context struct FFmuz_Ctx is NULL!", __func__);
		return (void *)-1;
	}
	else if ( FFmuz_Ctx->fpath==NULL || FFmuz_Ctx->fpath[0]==NULL )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Context struct FFmuz_Ctx has NULL fpath!", __func__);
		return (void *)-1;
	}
	else if (FFmuz_Ctx->ftotal<=0 )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: No media file in FFmz_Ctx!", __func__);
		return (void *)-1;
	}

	fpath=FFmuz_Ctx->fpath;  /* array of media file path */



   /* play all input files, one by one. */
   for(fnum=0; fnum < ftotal; fnum++)
   {
	/* check if enable_shuffle */
	if(enable_shuffle) {
		fnum=egi_random_max(ftotal)-1;
	}

	/* reset display window size */
//	display_height=show_h;
//	display_width=show_w;

	/* reset time indicators */
	ff_sec_Aelapsed=0;
	ff_sec_Velapsed=0;
	ff_sec_Aduration=0;
	ff_sec_Vduration=0;

	/* reset VCODECID */
	vcodecID=AV_CODEC_ID_NONE;

	/* Open media stream or file */
	EGI_PLOG(LOGLV_INFO,"%s: [fnum=%d] Start to open file %s...", __func__, fnum, fpath[fnum]);
	if(avformat_open_input(&pFormatCtx, fpath[fnum], NULL, NULL)!=0)
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to open the file, or file type is not recognizable.",__func__);

	   if(enable_filesloop) {	/* if loop, skip to next one */
		avformat_close_input(&pFormatCtx);
        	pFormatCtx=NULL;
		tm_delayms(10); /* sleep less time if try to catch a key_frame for a media stream */
		continue;
	   }
	   else {
		avformat_close_input(&pFormatCtx);
		return (void *)-1;
	   }
	}

	/* Retrieve stream information, !!!!!  time consuming  !!!! */
	EGI_PDEBUG(DBG_FFPLAY,"%lld(ms):  Retrieving stream information... \n", tm_get_tmstampms());
	/* ---- seems no use! ---- */
pFormatCtx->probesize2=128*1024;
	//pFormatCtx->max_analyze_duration2=8*AV_TIME_BASE;
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)  {
		EGI_PLOG(LOGLV_ERROR,"Fail to find stream information!");

	/* No media stream found */
#ifdef FFMUZ_MEDIA_LOOP
		avformat_close_input(&pFormatCtx);
        	pFormatCtx=NULL;
		continue;
#else
		avformat_close_input(&pFormatCtx);
		return (void *)-1;
#endif
	}


	/* Dump information about file onto standard error */
	EGI_PDEBUG(DBG_FFPLAY,"%lld(ms): Try to dump file information... \n",tm_get_tmstampms());
	av_dump_format(pFormatCtx, 0, fpath[fnum], 0);

	/* OR to read dictionary entries one by one */
	while( tag=av_dict_get(pFormatCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX) ) {
		EGI_PDEBUG(DBG_FFPLAY,"metadata: key [%s], value [%s] \n", tag->key, tag->value);

		/* Get Title name */
		if( strcmp( tag->key, "title")==0 ) {
			muzpage_update_title(tag->value);
		}
	}

	/* If no Title, then use base file name, and put to pic_info for display */
	if( fname==NULL ) {
		muzpage_update_title(fpath[fnum]);
	}

	/* Find the first video stream and audio stream */
	EGI_PDEBUG(DBG_FFPLAY,"%lld(ms):	Try to find the first video stream... \n",tm_get_tmstampms());
	/* reset stream index first */
	videoStream=-1;
	audioStream=-1;

	//printf("%d streams found.\n",pFormatCtx->nb_streams);
	/* find the first available stream of VIDEO and AUDIO */
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		/* For VIDEO streams */
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO ) {
		   if( videoStream < 0) {
			videoStream=i;
			EGI_PDEBUG(DBG_FFPLAY,"Video is found in stream[%d], to be accepted.\n",i);
			vcodecID=pFormatCtx->streams[i]->codec->codec_id;
			/*----------- Picture ----------
			 	AV_CODEC_ID_MJPEG
			 	AV_CODEC_ID_MJPEGB
			 	AV_CODEC_ID_LJPEG
				AV_CODEC_ID_JPEGLS
				AV_CODEC_ID_BMP
				AV_CODEC_ID_PNG
				... ...
			-------------------------------*/
		        vcodecDespt=avcodec_descriptor_get(vcodecID);
			if(vcodecDespt != NULL) {
				EGI_PLOG(LOGLV_INFO,"%s: Video codec name: %s, %s",
							__func__, vcodecDespt->name, vcodecDespt->long_name);
			}
		   }
		   else
			EGI_PDEBUG(DBG_FFPLAY,"Video is also found in stream[%d], to be ignored.\n",i);
		}
		/* For AUDIO streams */
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO ) {
		   if( audioStream < 0) {
			audioStream=i;
			EGI_PDEBUG(DBG_FFPLAY,"Audio is found in stream[%d], to be accepted.\n",i);
			acodecID=pFormatCtx->streams[i]->codec->codec_id;
			/*----------- AUDIO  ----------
				AV_CODEC_ID_MP2
				AV_CODEC_ID_MP3
				AV_CODEC_ID_AAC
				AV_CODEC_ID_AC3
				AV_CODEC_ID_FLAC
				AV_CODEC_ID_MP3ADU
				AV_CODEC_ID_MP3ON4
				AV_CODEC_ID_SPEEX
				... ...
			-------------------------------*/
		        acodecDespt=avcodec_descriptor_get(acodecID);
			if(acodecDespt != NULL) {
				EGI_PLOG(LOGLV_INFO,"%s: Audio codec name: %s, %s",
							__func__, acodecDespt->name, acodecDespt->long_name);
			}
		   }
		   else
			EGI_PDEBUG(DBG_FFPLAY,"Audio is also found in stream[%d], to be ignored.\n",i);
		}
	}
	EGI_PLOG(LOGLV_INFO,"%s: get videoStream [%d], audioStream [%d] ", __func__,
				 videoStream, audioStream);

	if(videoStream == -1 && audioStream == -1) {
		EGI_PLOG(LOGLV_ERROR,"No stream found for video or audio! quit ffplay now...");
		return (void *)-1;
	}
	else if(videoStream == -1) {
		EGI_PLOG(LOGLV_INFO,"Didn't find a video stream!");
//go on		return (void *)-1;
	}
	else if(audioStream == -1) {
		EGI_PLOG(LOGLV_WARN,"Didn't find an audio stream!");
//go on   	return (void *)-1;
	}



/* Disable audio */
if(disable_audio && audioStream>=0 )
{
	EGI_PDEBUG(DBG_FFPLAY,"Audio is disabled by the user! \n");
	audioStream=-1;
}

	/* proceed --- audio --- stream */
	if(audioStream >= 0) /* only if audioStream exists */
  	{
		EGI_PLOG(LOGLV_INFO,"%s: Prepare for audio stream processing ...",__func__);
		/* Get a pointer to the codec context for the audio stream */
		aCodecCtxOrig=pFormatCtx->streams[audioStream]->codec;
		/* Find the decoder for the audio stream */
		EGI_PLOG(LOGLV_INFO,"%s: Try to find the decoder for the audio stream... ",__func__);
		aCodec=avcodec_find_decoder(aCodecCtxOrig->codec_id);
		if(aCodec == NULL) {
			EGI_PLOG(LOGLV_ERROR, "%s: Unsupported audio codec! quit now..." ,__func__);
			return (void *)-1;
		}
		/* copy audio codec context */
		aCodecCtx=avcodec_alloc_context3(aCodec);
		if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0) {
			EGI_PLOG(LOGLV_ERROR, "%s: Couldn't copy audio code context! quit now...", __func__);
			return (void *)-1;
		}
		/* open audio codec */
		EGI_PLOG(LOGLV_INFO,"%s: Try to open audio stream with avcodec_open2()...",__func__);
		if(avcodec_open2(aCodecCtx, aCodec, NULL) <0 ) {
			EGI_PLOG(LOGLV_ERROR, "Could not open audio codec with avcodec_open2(), quit now...!");
			return (void *)-1;
		}
		/* get audio stream parameters */
		frame_size = aCodecCtx->frame_size;
		sample_rate = aCodecCtx->sample_rate;
		sample_fmt  = aCodecCtx->sample_fmt;
		bytes_per_sample = av_get_bytes_per_sample(sample_fmt);
		nb_channels = aCodecCtx->channels;
		channel_layout = aCodecCtx->channel_layout;
		av_get_channel_layout_string(chanlayout_string, nchanstr, nb_channels, channel_layout);
		EGI_PDEBUG(DBG_FFPLAY,"		frame_size=%d\n",frame_size);//=nb_samples, nb of samples per frame.
		EGI_PDEBUG(DBG_FFPLAY,"		channel_layout=%lld, as for: %s\n",
								 channel_layout, chanlayout_string);
		EGI_PDEBUG(DBG_FFPLAY,"			   %s\n", chanlayout_string);
		EGI_PDEBUG(DBG_FFPLAY,"		nb_channels=%d\n",nb_channels);
		EGI_PDEBUG(DBG_FFPLAY,"		sample format=%d as for: %s\n", sample_fmt,
							av_get_sample_fmt_name(sample_fmt) );
		EGI_PDEBUG(DBG_FFPLAY,"		bytes_per_sample: %d\n",bytes_per_sample);
		EGI_PDEBUG(DBG_FFPLAY,"		sample_rate=%d\n",sample_rate);


		/* Now only support AV_SAMPLE_FMT_S16(P) , SND_PCM_FORMAT_S16_LE */
		if(sample_fmt!=AV_SAMPLE_FMT_S16P && sample_fmt!=AV_SAMPLE_FMT_S16 ) {
			EGI_PLOG(LOGLV_CRITICAL,"%s: Sample formt is '%s', NOT supported?",
							__func__, av_get_sample_fmt_name(sample_fmt));
		}
		/* Now only support Max. channel number =2 */
		if( nb_channels > 2 ) {
			EGI_PLOG(LOGLV_CRITICAL,"%s: Number of audio channels is %d >2, not supported!",
										__func__, nb_channels);
			goto FAIL_OR_TERM;
		}

		/* prepare SWR context for FLTP format conversion, WARN: float points operations!!!*/
		if(sample_fmt == AV_SAMPLE_FMT_FLTP ) {
			/* set out sample rate for ffplaypcm */
			out_sample_rate=44100;

#if 1 /* -----METHOD (1)-----:  or to be replaced by swr_alloc_set_opts() */
			EGI_PLOG(LOGLV_INFO,"%s: alloc swr and set_opts for converting AV_SAMPLE_FMT_FLTP to S16 ...",
									__func__);
			swr=swr_alloc();
			av_opt_set_channel_layout(swr, "in_channel_layout",  channel_layout, 0);
			av_opt_set_channel_layout(swr, "out_channel_layout", channel_layout, 0);
			av_opt_set_int(swr, "in_sample_rate", 	sample_rate, 0); // for FLTP sample_rate = 24000
			av_opt_set_int(swr, "out_sample_rate", 	out_sample_rate, 0);
			av_opt_set_sample_fmt(swr, "in_sample_fmt",   AV_SAMPLE_FMT_FLTP, 0);
			av_opt_set_sample_fmt(swr, "out_sample_fmt",   AV_SAMPLE_FMT_S16P, 0);

#else  /* -----METHOD (2)-----:  call swr_alloc_set_opts() */
	/* Function definition:
	struct SwrContext *swr_alloc_set_opts( swr ,
                           int64_t out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
                           int64_t  in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate,
                           int log_offset, void *log_ctx);		 */

			/* allocate and set opts for swr */
			EGI_PLOG(LOGLV_INFO,"%s: swr_alloc_set_opts()...",__func__);
			swr=swr_alloc_set_opts( swr,
						channel_layout,AV_SAMPLE_FMT_S16P, out_sample_rate,
						channel_layout,AV_SAMPLE_FMT_FLTP, sample_rate,
						0, NULL );

			/* how to dither ...?? */
			//av_opt_set(swr,"dither_method",SWR_DITHER_RECTANGULAR,0);
#endif

			EGI_PLOG(LOGLV_INFO,"%s: start swr_init() ...", __func__);
			swr_init(swr);

			/* alloc outputBuffer */
			EGI_PDEBUG(DBG_FFPLAY,"%s: malloc outputBuffer ...\n",__func__);
			outputBuffer=malloc(2*frame_size * bytes_per_sample);
			if(outputBuffer == NULL)
	       	 	{
				EGI_PLOG(LOGLV_ERROR,"%s: malloc() outputBuffer failed!",__func__);
				return (void *)-1;
			}

			/* open pcm play device and set parameters */
 			if( egi_prepare_pcm_device(nb_channels,out_sample_rate,true) !=0 ) /* true for interleaved access */
			{
				EGI_PLOG(LOGLV_ERROR,"%s: fail to prepare pcm device for interleaved access.",
											__func__);
				goto FAIL_OR_TERM;
			}
		}
		else /* sample_fmt other than AV_SAMPLE_FMT_FLTP */
		{
			/*  Use SWR to convert out sample rate to 44100  ...*/
			if( sample_rate != 44100 && enable_audio_resample ) {
				EGI_PDEBUG(DBG_FFPLAY,"Sample rate is %d, NOT 44.1k, try to convert...\n",sample_rate);
				out_sample_rate=44100;

			  #if 1  /* ----- METHOD(1) ----- */
				swr=swr_alloc();
				av_opt_set_channel_layout(swr, "in_channel_layout",  channel_layout, 0);
				av_opt_set_channel_layout(swr, "out_channel_layout", channel_layout, 0);
				av_opt_set_int(swr, "in_sample_rate", 	sample_rate, 0); // for FLTP sample_rate = 24000
				av_opt_set_int(swr, "out_sample_rate", 	out_sample_rate, 0);
				av_opt_set_sample_fmt(swr, "in_sample_fmt",   sample_fmt, 0);
				av_opt_set_sample_fmt(swr, "out_sample_fmt",   AV_SAMPLE_FMT_S16, 0);

			  #else /* ----- METHOD(2) ----- */
				/* allocate and set opts for swr
 				 * out and in fmt both to be AV_SAMPLE_FMT_S16!
				 * 'in' data referes to decoded audio pcm data, which is noninterleated.
				 */
				EGI_PLOG(LOGLV_INFO,"%s: swr_alloc_set_opts()...",__func__);
				swr=swr_alloc_set_opts( swr,
						channel_layout, AV_SAMPLE_FMT_S16, out_sample_rate,
						channel_layout, AV_SAMPLE_FMT_S16, sample_rate,
						0, NULL );
 			  #endif

				EGI_PDEBUG(DBG_FFPLAY,"Start swr_init() ...\n");
				swr_init(swr);

				/* alloc outputBuffer for converted audio data
				 * Suppose that we only change sample rate.
				 * Allocate more mem. for outputBuffer as we not sure output frame size.
				 */
				EGI_PDEBUG(DBG_FFPLAY,"malloc outputBuffer ...\n");
				outputBuffer=malloc( nb_channels*frame_size*bytes_per_sample*2);
				if(outputBuffer == NULL)
		       	 	{
					EGI_PLOG(LOGLV_ERROR,"%s: malloc() outputBuffer failed!",__func__);
					return (void *)-1;
				}

				/* open pcm play device and set parameters */
				printf("-------- egi_prepare_pcm_device() ....\n");
 				if( egi_prepare_pcm_device(nb_channels,out_sample_rate, false) !=0 ) /* 'true' for interleaved access */
				{
					EGI_PLOG(LOGLV_ERROR,"%s: fail to prepare pcm device for interleaved access.",
											__func__);
					goto FAIL_OR_TERM;
				}

			}
			/* ---END sample rate convert to 44100--- */

			/* Directly open pcm play device and set parameters */
 			else if ( egi_prepare_pcm_device(nb_channels,sample_rate, false) !=0 ) /* 'false' as for 'noninterleaved access' */
			{
				EGI_PLOG(LOGLV_ERROR,"%s: fail to prepare pcm device for noninterleaved access.",
											__func__);
				goto FAIL_OR_TERM;
			}
			printf("-------- egi_prepare_pcm_device() finish ....\n");

		}

		/* allocate frame for audio */
		EGI_PDEBUG(DBG_FFPLAY,"%s: av_frame_alloc() for Audio...\n",__func__);
		pAudioFrame=av_frame_alloc();
		if(pAudioFrame==NULL) {
			EGI_PLOG(LOGLV_ERROR, "Fail to allocate pAudioFrame!");
			return (void *)-1;
		}

		/* <<<<<<<<<<<<     create a thread to display audio spectrum    >>>>>>>>>>>>>>> */
	       if(enable_audio_spectrum)
               {
	           if(pthread_create(&pthd_audioSpectrum, NULL, ff_display_spectrum, NULL ) != 0) {
        	        EGI_PLOG(LOGLV_ERROR, "Fails to create thread for displaying audio spectrum!");
                	return (void *)-1;
	           }
        	   else
                	EGI_PDEBUG(DBG_FFPLAY,"\033[32mFinish creating thread to display audio spectrum.\033[0m\n");

	           /* set running token */
		   pthd_audioSpectrum_running=true;
	       }

	} /* end of if(audioStream =! -1) */


/* disable video */
if(disable_video && videoStream>=0 )
{
	EGI_PDEBUG(DBG_FFPLAY,"Video is disabled by the user! \n");
	videoStream=-1;
}

     /* proceed --- video --- stream */
    if(videoStream >=0 ) /* only if videoStream exists */
    {
	EGI_PDEBUG(DBG_FFPLAY,"Prepare for video stream processing ...\n");

	/* get time_base */
	time_base = pFormatCtx->streams[videoStream]->time_base;

	/* Get a pointer to the codec context for the video stream */
	pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;

	/* Find the decoder for the video stream */
	EGI_PDEBUG(DBG_FFPLAY,"Try to find the decoder for the video stream... \n");
	pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if(pCodec == NULL) {
		EGI_PLOG(LOGLV_WARN,"Unsupported video codec! try to continue to decode audio...");
		videoStream=-1;
		//return (void *)-1;
	}
	/* if video size is illegal, then reset videoStream to -1, to ignore video processing */
	if(pCodecCtxOrig->width <= 0 || pCodecCtxOrig->height <= 0) {
		EGI_PLOG(LOGLV_WARN,"Video width=%d or height=%d illegal! try to continue to decode audio...",
				   pCodecCtxOrig->width, pCodecCtxOrig->height);
		videoStream=-1;
	}

    }

    /* videoStream may be reset to -1, so confirm again */
    if(videoStream >=0 && pCodec != NULL)
    {

	/* copy video codec context */
	pCodecCtx=avcodec_alloc_context3(pCodec);
	if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		EGI_PLOG(LOGLV_ERROR,"Couldn't copy video code context!");
		return (void *)-1;
	}

	/* open video codec */
	if(avcodec_open2(pCodecCtx, pCodec, NULL) <0 ) {
		EGI_PLOG(LOGLV_ERROR, "Cound not open video codec!");
		return (void *)-1;
	}

	/* Allocate video frame */
	pFrame=av_frame_alloc();
	if(pFrame==NULL) {
		EGI_PLOG(LOGLV_ERROR,"Fail to allocate pFrame!");
		return (void *)-1;
	}
	/* allocate an AVFrame structure */
	EGI_PDEBUG(DBG_FFPLAY,"Try to allocate an AVFrame structure for RGB data...\n");
	pFrameRGB=av_frame_alloc();
	if(pFrameRGB==NULL) {
		EGI_PLOG(LOGLV_ERROR, "Fail to allocate a AVFrame struct for RGB data !");
		return (void *)-1;
	}
	/* get original video size */
	EGI_PDEBUG(DBG_FFPLAY,"original video image size: widthOrig=%d, heightOrig=%d\n",
					 		pCodecCtx->width,pCodecCtx->height);

	/* use Orig size */
	display_width=pCodecCtx->width;
	display_height=pCodecCtx->height;
	/* normalize the size */
	display_width=(display_width>>1)<<1;
	display_height=(display_height>>1)<<1;

	EGI_PDEBUG(DBG_FFPLAY,"Finally adjusted display window size: display_width=%d, display_height=%d\n",
					 				  display_width,  display_height);

#if 0 ////////////////    NO need for FFMUZ    ///////////////
	/* Addjust displaying window position */
	offx=(LCD_MAX_WIDTH-display_width)>>1; /* put display window in mid. of width */
	if(IS_IMAGE_CODEC(vcodecID))		/* for IMAGE */
		offy=((265-29-display_height)>>1) +30;
	else					/* for MOTION PIC */
		offy=50;
#endif

	/* Determine required buffer size and allocate buffer for scaled picture size */
	numBytes=avpicture_get_size(PIX_FMT_RGB565LE, display_width, display_height);//pCodecCtx->width, pCodecCtx->height);
	pic_info.numBytes=numBytes;
	/* malloc buffer for pFrameRGB */
	buffer=(uint8_t *)av_malloc(numBytes);

	/* <<<<<<<<    allocate mem. for PIC buffers   >>>>>>>> */
	printf("%s: start to ff_malloc_PICbuffs()...\n", __func__);
	if(ff_malloc_PICbuffs(display_width,display_height, 2) == NULL) { /* pixel_size=2bytes for PIX_FMT_RGB565LE */
		EGI_PLOG(LOGLV_ERROR,"Fail to allocate memory for PICbuffs!");
		return (void *)-1;
	}
	else
		EGI_PDEBUG(DBG_FFPLAY, "Finish allocate memory for uint8_t *PICbuffs[%d]\n",PIC_BUFF_NUM);

	 /* We dont need offx/offy anymore for FFmuz, all to be handled by display_MusicPic() */
	 pic_info.vcodecID=vcodecID;
	 pic_info.height=display_height;
	 pic_info.width=display_width;

	 /* Assign appropriate parts of buffer to image planes in pFrameRGB
	 Note that pFrameRGB is an AVFrame, but AVFrame is a superset of AVPicture */
	 avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB565LE, display_width, display_height); //pCodecCtx->width, pCodecCtx->height);

	/* Initialize SWS context for software scaling, allocate and return a SwsContext */
	EGI_PDEBUG(DBG_FFPLAY, "Initialize SWS context for software scaling... \n");
	sws_ctx = sws_getContext( pCodecCtx->width,
				  pCodecCtx->height,
				  pCodecCtx->pix_fmt,
				  display_width, //scwidth,/* scaled output size */
				  display_height, //scheight,
				  AV_PIX_FMT_RGB565LE,
				  SWS_BILINEAR,
				  NULL,
				  NULL,
				  NULL
				);


/* <<<<<<<<<<<<     create a thread to display subtitles     >>>>>>>>>>>>>>> */
	/* check if substitle file *.srt exists in the same path of the media file */
	if(pfsub!=NULL){
		free(pfsub);
		pfsub=NULL;
	}
	pfsub=cstr_dup_repextname(fpath[fnum],".srt");
	EGI_PLOG(LOGLV_CRITICAL,"Subtitle file: %s,  Access: %s",pfsub, ( access(pfsub,F_OK)==0 ) ? "OK" : "FAIL" );
	if( pfsub != NULL && access(pfsub,F_OK)==0 ) {
		if( pthread_create(&pthd_displaySub,NULL,thdf_Display_Subtitle,(void *)pfsub ) != 0) {
			EGI_PLOG(LOGLV_ERROR, "Fails to create thread for displaying subtitles!");
			pthd_subtitle_running=false;
			free(pfsub); pfsub=NULL;
			//Go on anyway. //return (void *)-1;
		}
		else {
			EGI_PDEBUG(DBG_FFPLAY,"Finish creating thread for displaying subtitles.\n");
			pthd_subtitle_running=true;
		}
	}
	else {
		pthd_subtitle_running=false;
		free(pfsub); pfsub=NULL;
	}

  }/* end of (videoStream >=0 && pCodec != NULL) */


/* <<<<<<<<<<<<     create a thread to display picture to LCD    >>>>>>>>>>>>>>> */

	/* Even if no video stream */

	if( videoStream <0 )
		pic_info.PicOff=true;
	else
		pic_info.PicOff=false;

#if 0
	if(pthread_create(&pthd_displayPic,NULL,display_MusicPic,(void *)&pic_info) != 0) {
		EGI_PLOG(LOGLV_ERROR, "Fails to create thread for displaying pictures!");
		return (void *)-1;
	}
	else
		EGI_PDEBUG(DBG_FFPLAY,"Finish creating thread for displaying pictures.\n");
	/* set running token */
	pthd_displayPic_running=true;
#endif


/* if loop playing for only ONE file ..... */
SEEK_LOOP_START:
if(enable_seekloop)
{
	/* seek starting point */
	EGI_PDEBUG(DBG_FFPLAY,"av_seek_frame() to the starting point...\n");
        av_seek_frame(pFormatCtx, 0, 0, AVSEEK_FLAG_ANY);
}
else
{	//pFormatCtx->streams[videoStream]->time_base
	/* Seek to the Video position,
	 * Note:
	 * 1. For MP3, to call av_seek_frame() will fail!
	 */
	if(FFmuz_Ctx->start_tmsecs !=0 ) {
	  av_seek_frame(pFormatCtx, videoStream,(FFmuz_Ctx->start_tmsecs)*time_base.den/time_base.num,
											 AVSEEK_FLAG_ANY);
	}
}

	/* Get playing time (duration) */
	if(audioStream>=0) {
		ff_sec_Aduration=atoi( av_ts2timestr(pFormatCtx->streams[audioStream]->duration,
							&pFormatCtx->streams[audioStream]->time_base) );
	}
	if(videoStream>=0) {
		ff_sec_Vduration=atoi( av_ts2timestr(pFormatCtx->streams[videoStream]->duration,
							&pFormatCtx->streams[videoStream]->time_base) );
		EGI_PLOG(LOGLV_TEST,"%s: Video duration is %lds.",__func__,ff_sec_Vduration);
	}


/*  --------  LOOP  ::  Read packets and process data  --------   */

	gettimeofday(&tm_start,NULL);
	EGI_PDEBUG(DBG_FFPLAY,"<<<< ----- FFPLAY START PLAYING STREAMS ----- >>>\n");
	//printf("	 converting video frame to RGB and then send to display...\n");
	//printf("	 sending audio frame data to playback ... \n");
	i=0;

	EGI_PDEBUG(DBG_FFPLAY,"Start while() for loop reading, decoding and playing frames ...\n");
	while( av_read_frame(pFormatCtx, &packet) >= 0) {
		/* -----   process Video Stream   ----- */
		if( videoStream >=0 && packet.stream_index==videoStream)
		{
			//printf("...decoding video frame\n");
			//gettimeofday(&tm_start,NULL);
			if( avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet)<0 )
				EGI_PLOG(LOGLV_ERROR,"Error decoding video, try to carry on...");
			//gettimeofday(&tm_end,NULL);
			//printf(" avcode_decode_video2() cost time: %d ms\n",get_costtime(tm_start,tm_end) );

			/* if we get complete video frame(s) */
			if(frameFinished) {

				/* apply SWS and send scaled RGB data to pic buff for display */
				/* convert the image from its native format to RGB */
				//printf("%s: sws_scale converting ...\n",__func__);
				sws_scale( sws_ctx,
					   (uint8_t const * const *)pFrame->data,
					   pFrame->linesize, 0, pCodecCtx->height,
					   pFrameRGB->data, pFrameRGB->linesize
					);

				/* push data to pic buff for SPI LCD displaying */
				//printf("%s: start Load_Pic2Buff()....\n",__func__);
				if( ff_load_Pic2Buff(&pic_info,pFrameRGB->data[0],numBytes) <0 )
					EGI_PDEBUG(DBG_FFPLAY,"[%lld] PICBuffs are full! video frame is dropped!\n",
								tm_get_tmstampms());

				/* ---print playing time--- */
				ff_sec_Velapsed=atoi( av_ts2timestr(packet.pts,
				     			&pFormatCtx->streams[videoStream]->time_base) );
				//ff_sec_Vduration=atoi( av_ts2timestr(pFormatCtx->streams[videoStream]->duration,
				//			&pFormatCtx->streams[videoStream]->time_base) );

/*
				printf("\r	     video Elapsed time: %ds  ---  Duration: %ds  ",
									ff_sec_Velapsed, ff_sec_Vduration );
*/
				//printf("\r ------ Time stamp: %llds  ------", packet.pts/ );
				fflush(stdout);

				//gettimeofday(&tm_end,NULL);
				//printf(" LCD_Write_Block() for one frame cost time: %d ms\n",get_costtime(tm_start,tm_end) );				//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			} /* end of if(FrameFinished) */

		}/*  end  of vidoStream process  */


	/*----------------//////   process audio stream   \\\\\\\-----------------*/
		else if( audioStream >=0 && packet.stream_index==audioStream) { //only if audioStream exists
			//printf("processing audio stream...\n");
			/* bytes_used: indicates how many bytes of the data was consumed for decoding.
			         when provided with a self contained packet, it should be used completely.
			*  sb_size: hold the sample buffer size, on return the number of produced samples is stored.
			*/
			while(packet.size > 0) {
				//gettimeofday(&tm_start,NULL);
				bytes_used=avcodec_decode_audio4(aCodecCtx, pAudioFrame, &got_frame, &packet);
				//gettimeofday(&tm_end,NULL);
				//printf(" avcode_decode_audio4() cost time: %d ms\n",get_costtime(tm_start,tm_end) );
				if(bytes_used<0)
				{
					EGI_PDEBUG(DBG_FFPLAY,"Error while decoding audio! try to continue...\n");
					packet.size=0;
					packet.data=NULL;
					//av_free_packet(&packet);
					//break;
					continue;
				}
				/* if decoded data size >0 */
				if(got_frame)
				{
					//gettimeofday(&tm_start,NULL);

					/* 1. Playback 2 channel audio data */
					if(pAudioFrame->data[0] && pAudioFrame->data[1]) {
						// pAuioFrame->nb_sample = aCodecCtx->frame_size !!!!
						// Number of samples per channel in an audio frame
					/* 1.1 SWR ON, if sample_fmt == AV_SAMPLE_FMT_FLTP */
						if(sample_fmt == AV_SAMPLE_FMT_FLTP) {
							outsamples=swr_convert(swr,&outputBuffer, pAudioFrame->nb_samples, (const uint8_t **)pAudioFrame->extended_data, aCodecCtx->frame_size);
							EGI_PDEBUG(DBG_FFPLAY,"FLTP outsamples=%d, frame_size=%d \n",outsamples,aCodecCtx->frame_size);
							egi_play_pcm_buff( (void **)&outputBuffer,outsamples);
						}

					/* 1.2 SWR ON,  if sample_rate != 44100 */
						else if( enable_audio_resample )  {
		        	/* compute destination(converted) number of samples */
			        dst_nb_samples = av_rescale_rnd( swr_get_delay(swr, sample_rate) +
                                       	pAudioFrame->nb_samples, out_sample_rate, sample_rate, AV_ROUND_UP);
					/* swr convert */
					//outsamples=swr_convert(swr,&outputBuffer, pAudioFrame->nb_samples,
					//	  (const uint8_t **)pAudioFrame->data, aCodecCtx->frame_size);
					ret=swr_convert(swr,&outputBuffer, dst_nb_samples,
						  (const uint8_t **)pAudioFrame->data, aCodecCtx->frame_size);
                 EGI_PDEBUG(DBG_FFPLAY,"converted nb of samples=%d, dst_nb_samples=%d input frame_size=%d \n",
						                ret, dst_nb_samples, aCodecCtx->frame_size);
						      if(ret>0)
							  egi_play_pcm_buff( (void **)&outputBuffer, ret);
						}
					/* 1.3 SWR OFF */
						else {
							 egi_play_pcm_buff( (void **)pAudioFrame->data, aCodecCtx->frame_size);// 1 frame each time
						}
					}
					/* 2. Playback one channel */
					else if(pAudioFrame->data[0]) {
						 //printf("One channel only\n");
						if( enable_audio_resample )  { /* sample_rate != 44100 */
							outsamples=swr_convert(swr,&outputBuffer, pAudioFrame->nb_samples, (const uint8_t **)pAudioFrame->data, aCodecCtx->frame_size);
							EGI_PDEBUG(DBG_FFPLAY,"outsamples=%d, frame_size=%d \n",outsamples,aCodecCtx->frame_size);
							egi_play_pcm_buff( (void **)&outputBuffer,aCodecCtx->frame_size);
						}
						/* direct output */
						else {
						        egi_play_pcm_buff( (void **)(&pAudioFrame->data[0]), aCodecCtx->frame_size);// 1 frame each time
						}

					}

					/*    ---- 1024 points FFT displaying handling ----
					 *   Note:
					 *     1. For sample rate 44100 only, noninterleaved.
					 *     2. If channle >=2, framesize must > 1024, or two channel PCM
					 *	  data will be interfered.
					 */
					if( pthd_audioSpectrum_running ) {
						//printf("Load FFT data...\n");
					        if( enable_audio_resample )  {     /* SWR ON, if sample_rate != 44100 */
							ff_load_FFTdata(  (void **)&outputBuffer,
									  aCodecCtx->frame_size   );
						}
						else {			  /* direct data */
							ff_load_FFTdata(  (void **)pAudioFrame->data,
									  aCodecCtx->frame_size   );
						}
					}

					/* print audio playing time, only if no video stream */
					ff_sec_Aelapsed=atoi( av_ts2timestr(packet.pts,
				     			&pFormatCtx->streams[audioStream]->time_base) );
					//printf("\r	     audio Elapsed time: %ds  ---  Duration: %ds  ",
					//			ff_sec_Aelapsed, ff_sec_Aduration );

					//gettimeofday(&tm_end,NULL);
					//printf(" egi_play_pcm_buff() cost time: %d ms\n",get_costtime(tm_start,tm_end) );

					/* --- Reset timing slider ---- */
					muzpage_update_timingBar(ff_sec_Aelapsed, ff_sec_Aduration);

				}
				packet.size -= bytes_used;
				packet.data += bytes_used;
			}/* end of while(packet.size>0) */


		}/*   end of audioStream process  */

                /* free OLD packet each time, that was allocated by av_read_frame */
                av_free_packet(&packet);


/* For clip test, just ffplay a short time then break */
if(enable_clip_test)
{
		if( (audioStream >= 0) && (ff_sec_Aelapsed >= FFMUZ_CLIP_PLAYTIME) )
		{
			/* reset elapsed/duratoin time */
			ff_sec_Aelapsed=0;
			ff_sec_Aduration=0;
			break;
		}
		/* if a picture without audio */
		else if( audioStream<0 )
		{
			/* reset elapsed/duratoin time */
			ff_sec_Aelapsed=0;
			ff_sec_Aduration=0;

			tm_delayms(FFMUZ_CLIP_PLAYTIME*1000);
			break;
		}
}

	/*----------------<<<<< Check and parse commands >>>>>-----------------*/
		if( FFmuz_Ctx->ffcmd != cmd_none )
		{
		    /* 1. parse PAUSE/PLAY first */
		    if(FFmuz_Ctx->ffcmd==cmd_pause) {
			do {
				egi_sleep(0,0,100);
			} while(FFmuz_Ctx->ffcmd==cmd_pause); // !=cmd_play;

			/* Don not reset, pass down curretn cmd */

		    }
	  	    /* 2. shift mode */
		    else if( FFmuz_Ctx->ffcmd==cmd_mode) {
			    /* Note:
			     *      1. Default enable_filesloop is true.
			     *	    2. mode_loop_all means loop in order.
			     */
			    if(FFmuz_Ctx->ffmode==mode_repeat_one) {
				enable_seekloop=true;
				enable_shuffle=false;
			    }
			    else if(FFmuz_Ctx->ffmode==mode_shuffle) {
				enable_shuffle=true;
				enable_seekloop=false;
			    }
			    else if(FFmuz_Ctx->ffmode==mode_loop_all) {
				enable_seekloop=false;
				enable_shuffle=false;
			    }
			    else if(FFmuz_Ctx->ffmode==mode_play_once) {
				enable_seekloop=false;
				enable_filesloop=false;
			    }
			    /* reset */
	 		    FFmuz_Ctx->ffcmd=cmd_none;
		    }

		    /* 3. parse PREV/NEXT  */
		    else if(FFmuz_Ctx->ffcmd==cmd_next) {
		    	FFmuz_Ctx->ffcmd=cmd_none;
			if(enable_seekloop==true) { /* If REPEAT ONE */
				if( fnum > 0 ) 	fnum-=1;
				else		fnum=-1;
			}
			//break;
			goto FAIL_OR_TERM;
		    }
		    else if(FFmuz_Ctx->ffcmd==cmd_prev) {
			FFmuz_Ctx->ffcmd=cmd_none;
			//printf("xxxxxxxxx  fnum=%d  xxxxxxx", fnum );
			if(enable_seekloop==true) { /* If REPEAT ONE */
				if( fnum > 0 ) 	fnum-=1;
				else		fnum=-1;
			}
			else {
				if( fnum > 0 ) 	fnum-=2;
				else		fnum=-1;
			}
			//break;
			goto FAIL_OR_TERM;
		    }

		    /* reset as cmd_none at last */
		    else
			FFmuz_Ctx->ffcmd=cmd_none;
		}

	}/*  end of while()  <<--- end of one file playing --->> */


	/* hold on for a while, also let pic buff to be cleared before fbset_color!!! */
	if(FFMUZ_LOOP_TIMEGAP>0)
	{
		/* NOTE: fnum may be illegal, as modified in ffcmd parsing, so skip to FAIL_OR_TERM! */
		EGI_PDEBUG(DBG_FFPLAY,"End playing %s, hold on for a while...\n",fpath[fnum]);
		tm_delayms(FFMUZ_LOOP_TIMEGAP*1000);
	}


/* if loop playing one file, then got to seek start  */
if(enable_seekloop)
{
	EGI_PDEBUG(DBG_FFPLAY,"enable_seekloop=true, go to seek the start of the same file and replay. \n");
	goto SEEK_LOOP_START;
}


FAIL_OR_TERM:	/*  <<<<<<<<<<  start to release all resources  >>>>>>>>>>  */

	/* Exit Pic displaying thread, just before exiting subtitle_thread!! */
	if( pthd_displayPic_running ) {
		/* wait for display_thread to join */
		EGI_PDEBUG(DBG_FFPLAY,"Try to join picture displaying thread ...\n");
		control_cmd = cmd_exit_display_thread;
		pthread_join(pthd_displayPic,NULL);
		pthd_displayPic_running=false; /* reset token */
		control_cmd = cmd_none;/* call off command */
	}

	/* Exit subtitle displaying thread, just after picture displaying thread joined. */
	if( pthd_subtitle_running ) {
		EGI_PDEBUG(DBG_FFPLAY,"Try to join subtitle displaying thread ...\n");
                control_cmd = cmd_exit_subtitle_thread;
		pthread_join(pthd_displaySub,NULL);  /* Though it will exit when reaches end of srt file. */
		pthd_subtitle_running=false; /* reset token */
		control_cmd = cmd_none;/* call off command */
	}


	/* free strdupped file name, after pthd_displayPic()  */
	free(fname); fname=NULL;

	/* Free the YUV frame */
	EGI_PDEBUG(DBG_FFPLAY,"Free pFrame...\n");
	if(pFrame != NULL) {
		av_frame_free(&pFrame);
		pFrame=NULL;
		EGI_PDEBUG(DBG_FFPLAY,"	...pFrame freed.\n");
	}

        /* Free pAudioFrame */
        if(pAudioFrame != NULL) {
                av_frame_free(&pAudioFrame);
                pAudioFrame=NULL;
                EGI_PDEBUG(DBG_FFPLAY," ...pAudioFrame freed.\n");
        }

	/* Free the RGB image */
	EGI_PDEBUG(DBG_FFPLAY,"free buffer...\n");
	if(buffer != NULL) {
		av_free(buffer);
		buffer=NULL;
		EGI_PDEBUG(DBG_FFPLAY,"	...buffer freed.\n");
	}

	EGI_PDEBUG(DBG_FFPLAY,"Free pFrameRGB...\n");
	if(pFrameRGB != NULL) {
		av_frame_free(&pFrameRGB);
		pFrameRGB=NULL;
		EGI_PDEBUG(DBG_FFPLAY,"	...pFrameRGB freed.\n");
	}

	/* close pcm device and audioSpectrum */
//	if(audioStream >= 0) {
		EGI_PDEBUG(DBG_FFPLAY,"Close PCM device...\n");
		egi_close_pcm_device();

		/* exit audioSpectrum thread */
		if( pthd_audioSpectrum_running ) {
			/* wait for thread to join */
			EGI_PDEBUG(DBG_FFPLAY,"Try to join audioSpectrum thread ...\n");
			/* give a command to exit audioSpectrum thread */
			control_cmd = cmd_exit_audioSpectrum_thread;
			pthread_join(pthd_audioSpectrum,NULL);
			control_cmd = cmd_none;/* call off command */
			pthd_audioSpectrum_running=false; /* reset token */
		}
//	}

	/* free outputBuffer */
	if(outputBuffer != NULL)
	{
		EGI_PDEBUG(DBG_FFPLAY,"Free outputBuffer for pcm...\n");
		free(outputBuffer);
		outputBuffer=NULL;
	}

	/* Close the codecs */
	EGI_PDEBUG(DBG_FFPLAY,"Closee the codecs...\n");
	avcodec_close(pCodecCtx);
	pCodecCtx=NULL;
	avcodec_close(pCodecCtxOrig);
	pCodecCtxOrig=NULL;
	avcodec_close(aCodecCtx);
	aCodecCtx=NULL;
	avcodec_close(aCodecCtxOrig);
	aCodecCtxOrig=NULL;

	/* Close the video file */
	EGI_PDEBUG(DBG_FFPLAY,"avformat_close_input()...\n");
	if(pFormatCtx != NULL) {
		avformat_close_input(&pFormatCtx);
		pFormatCtx=NULL;
	}

	/* Free SWR and SWS */
//	if(audioStream >= 0)
//	{
		EGI_PDEBUG(DBG_FFPLAY,"Free swr at last...\n");
		swr_free(&swr);
		swr=NULL;
//	}
//	if(videoStream >= 0)
//	{
		EGI_PDEBUG(DBG_FFPLAY,"Free sws_ctx at last...\n");
		sws_freeContext(sws_ctx);
		sws_ctx=NULL;
//	}

	/* free buff for subtitle fpath */
	if( pfsub != NULL ) {
		free(pfsub);
		pfsub=NULL;
	}

	/* print total playing time for the file */
	gettimeofday(&tm_end,NULL);
	EGI_PDEBUG(DBG_FFPLAY,"Playing %s cost time: %d ms\n",fpath[fnum_playing],
									tm_signed_diffms(tm_start,tm_end) );

	EGI_PDEBUG(DBG_FFPLAY," \n ---( End of playing file %s )--- \n",fpath[fnum_playing]);

	/* sleep, to let sys release cache ....???? */
	tm_delayms(END_PAUSE_TIME);

   } /* end of for(...), loop playing input files*/

   //tm_delayms(500);

  /* --- PLAY ALL FILES END --- */
  if(!enable_filesloop) /* if disable loop playing all files, then break here */
  {
	break;
  }
  EGI_PDEBUG(DBG_FFPLAY," \n----( End of Playing One Complete Round )--- \n\n\n\n\n");

} /* end of while(1), eternal loop. */


	/* close fb dev */
	release_fbdev(&ff_fb_dev);

        return 0;
}
