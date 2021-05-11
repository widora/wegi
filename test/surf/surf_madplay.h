/* ----------------------------------------------------------------------------
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: minimad.c,v 1.4 2004/01/23 09:41:32 rob Exp $

 * This is perhaps the simplest example use of the MAD high-level API.
 * Standard input is mapped into memory via mmap(), then the high-level API
 * is invoked with three callbacks: input, output, and error. The output
 * callback converts MAD's high-resolution PCM samples to 16 bits, then
 * writes them to standard output in little-endian, stereo-interleaved
 * format.


 ----------------------------------------------------------
 Modified for ALSA PCM playback with SND_PCM_FORMAT_S16_LE.
 ----------------------------------------------------------

 Usage example:
	./minimad  /Music/ *.mp3

 KeyInput command:
	'q'	Quit			退出
	'n'	Next			下一首
	'p'	Prev			前一首
	'+'	Increase volume +5%　　	增加音量
	'-'	Decrease volume -5%　　	减小音量
	'>'	Fast forward		快进
	'<'	Fast backward		快退

 Midas Zhou
 midaszhou@yahoo.com
 -------------------------------------------------------------------------------

		---- Smples Per Frame ----

              MPEG-1      MPEG-2      MPEG-2.5(LSF)
 Layer_I       384         384           384
 Layer_II     1152        1152          1152
 Layer_III    1152         576           576

enum mad_mode {
  MAD_MODE_SINGLE_CHANNEL = 0,          // single channel
  MAD_MODE_DUAL_CHANNEL   = 1,          // dual channel
  MAD_MODE_JOINT_STEREO   = 2,          // joint (MS/intensity) stereo
  MAD_MODE_STEREO         = 3           /// normal LR stereo
};


struct mad_header {
  enum mad_layer layer;                 // audio layer (1, 2, or 3)
  enum mad_mode mode;                   // channel mode (see above)
  int mode_extension;                   // additional mode info
  enum mad_emphasis emphasis;           // de-emphasis to use see above)

  unsigned long bitrate;                // stream bitrate (bps)
  unsigned int samplerate;              // sampling frequency (Hz)

  unsigned short crc_check;             // frame CRC accumulator
  unsigned short crc_target;            // final target CRC checksum

  int flags;                            // flags (see below)
  int private_bits;                     // private bits (see below)

  mad_timer_t duration;                 // audio playing time of frame
};

typedef struct {
  signed long seconds;          //whole seconds
  unsigned long fraction;       // 1/MAD_TIMER_RESOLUTION seconds
} mad_timer_t;

  void mad_header_init(struct mad_header *);
  int mad_header_decode(struct mad_header *, struct mad_stream *);
  static int decode_header(struct mad_header *header, struct mad_stream *stream)
  int mad_header_decode(struct mad_header *header, struct mad_stream *stream)

------------------------------------------------------------------------------- */
#ifndef __SURF_MADPLAY_H__
#define __SURF_MADPLAY_H__

# include <stdio.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <egi_utils.h>
# include <egi_pcm.h>
# include <egi_input.h>
# include <egi_filo.h>
# include "mad.h"

extern FBDEV  *vfbdev;	/* Ref. to virtual FBDEV for the SURFUSER */

int        *pUserSig;       		/* Pointer to surfshmem->usersig, ==1 for quit. */
char	   strPassTime[256];		/* Passage of time */
//unsigned int nchannels, samplerate;	/* Channels and sample rate for decoded PCM data */

int	ffseek_val, fbseek_val;		/* Fast forward/backward speed: skip ffseek_val*5 sec */

const char *mad_mode_str[]={
	"Single channel",
	"Dual channel",
	"Joint Stereo",		// joint (MS/intensity) stereo */
	"Normal LR Stereo",
};

extern void draw_PassTime(long intus);

/* input回调函数.   在解码前执行．目的是将mp3数据喂给解码器．*/
static enum mad_flow input_all(void *data, struct mad_stream *stream);

/* header回调函数.  在每次读取帧头后执行．在这里读取键盘指令和mp3格式，播放时间等信息. */
static enum mad_flow header(void *data,  struct mad_header const *header);

/* header回调函数.  在每次读取帧头后执行．这里作为前处理，将帧头位置和时间推入FILO索引. */
static enum mad_flow header_preprocess(void *data,  struct mad_header const *header);

/* output回调函数.  在每次完成一帧解码后执行．将PCM数据转化成S16L格式后进行播放．*/
static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm);

/* error回调函数.   在每次完成mad_header(frame)_decode()后执行．主要用于打印错误信息. */
static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame);

/* 前期解码函数，读取所有帧头，并将位置和时间推入FILO索引. */
static int decode_preprocess(unsigned char const *start, unsigned long length);

/* 解码函数MP3 */
static int mp3_decode(unsigned char const *, unsigned long);


static snd_pcm_t *pcm_handle;      /* PCM播放设备句柄 */
static int16_t  *data_s16l=NULL;   /* FORMAT_S16L 数据 */
static bool pcmdev_ready;	   /* PCM 设备ready */
static bool pcmparam_ready;	   /* PCM 参数ready */

/* KeyIn command 定义键盘输入命令 */
//static int cmd_keyin;
enum {
	CMD_NONE=0,
	CMD_NEXT,
	CMD_PREV,
	CMD_FFSEEK,	/* Fast forward seek, command in BTN_NEXT, react in mad_flow header() */
	CMD_FBSEEK,	/* Fast backward seek */
	CMD_LOOP,
	CMD_PAUSE,
	CMD_PLAY,
};
static int madCMD=CMD_PAUSE;

/* Status */
enum {
	STAT_PLAY,
	STAT_PAUSE,
	STAT_FFSEEK,
	STAT_FBSEEK,
	STAT_LOAD, /* Indicate that MP3 is just being loaded.  set/reset in decode_preprocess() */
};
static int madSTAT=STAT_PAUSE;

/* Frame counter 帧头计数 */
static unsigned long   	  header_cnt;

/* Time duration for a MP3 file 已播放时长 */
static unsigned long long timelapse_frac;
static unsigned int	  timelapse_sec;
static float		  timelapse;
static float		  lapfSec;
static int 		  lapHour, lapMin;

static float		  preset_timelapse;

/* Time duration for a MP3 file 总时长 */
static unsigned long long duration_frac;
static unsigned int	  duration_sec;
static float		  duration;
static float		  durfSec;
static int		  durHour, durMin;

static unsigned long long poff;		/* Offset to buff of mp3 file */
static unsigned long long fsize;	/* file size */

/*  Frame header index 帧头位置和时间，推入FILO作为mp3帧头位置和时间索引 */
typedef struct mp3_header_pos
{
	off_t poff;		/* Offset */
	float timelapse;	/* Timelapse at start of the frame */
} MP3_HEADER_POS;
EGI_FILO *filo_headpos;		/* Array/index of frame header pos */


EGI_FILEMMAP *mp3_fmap=NULL;    /* MP3 file fmap */

#if 0 ///////////////////////////////////////////
/*-----------------------------
	     MAIN
------------------------------*/
int main(int argc, char *argv[])
{
  EGI_FILEMMAP *fmap=NULL;
  int i;
  int files;	/* Total inpu files */

  if (argc <2 )
  	return 1;
  files=argc-1;

  /* Set termI/O 设置终端为直接读取单个字符方式 */
  egi_set_termios();

  /* Prepare vol 启动系统声音调整设备 */
  egi_getset_pcm_volume(NULL,NULL);

  /* Open pcm playback device 打开PCM播放设备 */
  if( snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
  	printf("Fail to open PCM playback device!\n");
        return 1;
  }

/* Play all files 逐个播放MP3文件 */
for(i=0; i<files; i++) {

	/* Init filo 建立一个FILO用于存放　*/
  	filo_headpos=egi_malloc_filo(1024, sizeof(MP3_HEADER_POS), FILOMEM_AUTO_DOUBLE);
  	if(filo_headpos==NULL) {
		printf("Fail to malloc headpos FILO!\n");
		return 2;
  	}

	/* Reset 重置参数 */
	duration_frac=0;
	duration_sec=0;
	timelapse_frac=0;
	timelapse_sec=0;
	header_cnt=0;
	pcmparam_ready=false;
	pcmdev_ready=false;
	poff=0;

  	/* Mmap input file 映射当前文件 */
	fmap=egi_fmap_create(argv[i+1]);
	if(fmap==NULL) return 1;
	fsize=fmap->fsize;

	/* To fill filo_headpos and calculation time duration. */
	/* 前期解码处理，仅快速读取所有帧头，并建立索引 */
 	decode_preprocess((const unsigned char *)fmap->fp, fmap->fsize);

	/* Calculate duration 计算总时长 */
	duration=(1.0*duration_frac/MAD_TIMER_RESOLUTION) + duration_sec;
  	printf("\rDuration: %.1f seconds\n", duration);
	durHour=duration/3600; durMin=(duration-durHour*3600)/60;
	durfSec=duration-durHour*3600-durMin*60;

	/* Decoding 解码播放 */
  	printf(" Start playing...\n %s\n size=%lldk\n", argv[i+1], fmap->fsize>>10);
 	decode((const unsigned char *)fmap->fp, fmap->fsize);
//	poff=0;

	/* Command parse, after jumping out of decode. 键盘输入命令解释 */
	switch(cmd_keyin) {
		case CMD_PREV:
			if(i==0) i=-1;
			else i-=2;
			break;
		case CMD_LOOP:
			break;

	}
	cmd_keyin=CMD_NONE;

  	/* Release source 释放相关资源　*/
  	free(data_s16l); data_s16l=NULL;
  	egi_fmap_free(&fmap);
	egi_filo_free(&filo_headpos);
}

  /* Close pcm handle 关闭PCM播放设备 */
  snd_pcm_close(pcm_handle);

  /* Reset termI/O 设置终端 */
  egi_reset_termios();

  return 0;
}

#endif //////////////////////////////////////////



/* 私有数据结构，用于在各callback函数间共享数据
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
  unsigned char const *start;
  unsigned long length;
  const struct mad_decoder *decoder;
};

#if 0 ///////////////////////////////////////////////////////////////
/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input_blocks(void *data,  struct mad_stream *stream)
{
  struct buffer *buffer = data;
  int feed_size=4096*2;

  /* NOTE: Each stream will has to be decoded from the start, old tail has been discarded!!! */
  if(!buffer->length)
	return MAD_FLOW_STOP;

  if( buffer->length > feed_size ) {
	  mad_stream_buffer(stream, buffer->start+poff, feed_size);
	  poff += feed_size;
	  buffer->length -= feed_size;
  }
  else {
	  mad_stream_buffer(stream, buffer->start+poff, buffer->length );
	  poff = buffer->length;
	  buffer->length=0;
  }

  return MAD_FLOW_CONTINUE;
}
#endif ///////////////////////////////////////////////////////////////

/* 回调函数，在解码前执行．目的是将mp3数据喂给解码器．
 * 　To feed whole mp3 file to the stream
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */
static enum mad_flow input_all(void *data, struct mad_stream *stream)
{
  struct buffer *buffer = data;

  if (!buffer->length)
    	return MAD_FLOW_STOP;

  /* set stream buffer pointers, pass user data to stream ... */
  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}

/*　将解码后得到的PCM数据转化为FORMAT_S16L格式
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/*　回调函数，在每次完成一帧解码后执行．将PCM数据转化成S16L格式后进行播放．
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 *
 *** NOTE:
 *	1. The first header_cnt MAY NOT be 1, as it's NOT compelete then!
 *	2. nchannels and samplerate MAY chages for the first serveral frames!! WHY?

header_cnt:4, flags:0x50C0, mode: Single channel, nchanls=1, sample_rate=11025, nsamples=384
header_cnt:7, flags:0x0043, mode: Joint Stereo, nchanls=2, sample_rate=44100, nsamples=1152
header_cnt:8, flags:0x0043, mode: Joint Stereo, nchanls=2, sample_rate=44100, nsamples=1152

	3. header->flags is NOT correct here!, assume (header->flags&0x1)==1 as complete frame.
 --- correct in header() ---
Header flags: 0x50C8
Header flags: 0x0048
Header flags: 0x00C8
Header flags: 0x0048
 --- wrong here in output() ---
header_cnt:4, flags:0x50C0, mode: Single channel, nchanls=1, sample_rate=11025, nsamples=384
header_cnt:7, flags:0x0043, mode: Joint Stereo, nchanls=2, sample_rate=44100, nsamples=1152
header_cnt:9, flags:0x00C3, mode: Joint Stereo, nchanls=2, sample_rate=44100, nsamples=1152
header_cnt:10, flags:0x0043, mode: Joint Stereo, nchanls=2, sample_rate=44100, nsamples=1152

 */

static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
  unsigned int i,k;
  static unsigned int nsamples, nchannels, samplerate;
  mad_fixed_t const *left_ch, *right_ch;

  /*　声道数　采样率　PCM数据 */
  /* Get PCM data */
  /* NOTE: nchannels and samplerate MAY chages for the first serveral frames, see below */
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];
  nsamples  = pcm->length;	/* number of samples per channel, MAD_NSBSAMPLES(header) */

  /* 为PCM播放设备设置相应参数 */
  /* Sep parameters for pcm_hanle */
  if( !pcmdev_ready && header->flags&0x1 ) {  /* assume (header->flags&0x1)==1 as start of complete frame */
	/* Update nchannles and samplerate */
	nchannels=pcm->channels; //MAD_NCHANNELS(header);
	samplerate= pcm->samplerate;

	/* NOTE: nchannels/samplerate from the first frame header MAY BE wrong!!! */
	printf("header_cnt:%lu, flags:0x%04X, mode: %s, nchanls=%d, sample_rate=%d, nsamples=%d\n",
			header_cnt, header->flags, mad_mode_str[header->mode], nchannels, samplerate, nsamples);
	if( egi_pcmhnd_setParams(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, nchannels, samplerate) <0 ) {
               	printf("Fail to set params for PCM playback handle.!\n");
	} else {
		//printf("EGI set pcm params successfully!\n");
		pcmdev_ready=true;
	}
  }

  if(pcmdev_ready) {
	/* 转换成S16L格式 */
  	/* Realloc data for S16 */
 		data_s16l=(int16_t *)realloc(data_s16l, nchannels*nsamples*2);
  	if(data_s16l==NULL) {
		return MAD_FLOW_CONTINUE;
		//exit(1);
	}
 		/* Scale to S16L */
  	for(k=0,i=0; i<nsamples; i++) {
		data_s16l[k++]=scale(*(left_ch+i));		/* Left */
		if(nchannels==2)
			data_s16l[k++]=scale(*(right_ch+i));	/* Right */
  	}
	/* Playback 播放PCM数据 */
  	egi_pcmhnd_playBuff(pcm_handle, true, (void *)data_s16l, nsamples);
  }

  return MAD_FLOW_CONTINUE;
}


/* 回调函数，在每次读取帧头后执行．在这里读取指令和mp3格式，播放时间等信息．
 * Parse keyinput and frame header
 *
 */
static enum mad_flow header(void *data,  struct mad_header const *header )
{
//  int percent;
//  char ch;
//  ch=0;
  int mad_ret=MAD_FLOW_CONTINUE;
  struct buffer *buffer = data;
  MP3_HEADER_POS headpos;

  /* If usersig to quit */
  if(*pUserSig==1)
	return MAD_FLOW_STOP;

  /* If go PREV or NEXT */
  if(madCMD==CMD_PREV || madCMD==CMD_NEXT)
	return MAD_FLOW_STOP;

  /* If pause */
  if(madCMD==CMD_PAUSE) {
	  while(madCMD==CMD_PAUSE) {
		if(*pUserSig==1)
			return MAD_FLOW_STOP;
		madSTAT=STAT_PAUSE;
	  	usleep(100000);
	  };

	  madCMD=CMD_NONE;
  }
  madSTAT=STAT_PLAY;

  /* If fast seek */
  if(madCMD==CMD_FFSEEK) {
	if(preset_timelapse < timelapse) /* To avoid repeat updating preset_timelapse. */
		preset_timelapse=timelapse+5*ffseek_val;  /* 5s */
	/* To clear madCMD later */
  }
  else if(madCMD==CMD_FBSEEK) {
                if( egi_filo_read(filo_headpos, header_cnt-50*ffseek_val, &headpos) ==0 )
		{
                	header_cnt-=50*ffseek_val; /* go back 10*ffseek_val Frameheaders */

                	/* Reload stream */
                	mad_stream_buffer(&(buffer->decoder->sync->stream), buffer->start+headpos.poff, fsize-headpos.poff);
                	timelapse_sec=headpos.timelapse;
                	timelapse_frac=headpos.timelapse-timelapse_sec;

			/* Update passtime label */
  			timelapse=(1.0*timelapse_frac/MAD_TIMER_RESOLUTION) + timelapse_sec;
  			lapHour=timelapse/3600;
  			lapMin=(timelapse-lapHour*3600)/60;
  			lapfSec=timelapse-3600*lapHour-60*lapMin;

  			snprintf(strPassTime, sizeof(strPassTime), "%02d:%02d:%02d - [%02d:%02d:%02d]",
						lapHour, lapMin, (int)lapfSec, durHour, durMin, (int)durfSec);
			draw_PassTime(0);

			madCMD=CMD_NONE;
			return MAD_FLOW_IGNORE;
		}
  }

  /* Count header frame */
  header_cnt++;

  /* Time elapsed 计算当前播放时间 ---or read FILO. */
  timelapse_frac += header->duration.fraction;
  timelapse_sec  += header->duration.seconds;
  timelapse=(1.0*timelapse_frac/MAD_TIMER_RESOLUTION) + timelapse_sec;

#if 1 //////////////////////////////////////////////
  /*** Print MP3 file info 输出播放信息到终端
   * Note:
   *	1. For some MP3 file, it should wait until the third header frame comes that it contains right layer and samplerate!
   */
  if( !pcmparam_ready ) {
	if( pcmdev_ready ) {
		printf(" -----------------------\n");
		printf(" Simple is the Best!\n");
		printf(" miniMAD +Libmad +EGI\n");
  		printf(" Layer_%d\n %s\n bitrate:    %.1fkbits\n samplerate: %dHz\n",
			header->layer, mad_mode_str[header->mode], 1.0*header->bitrate/1000, header->samplerate);
		printf(" -----------------------\n");

		//pcmparam_ready=true; /* Set, see below. after draw_PassTime(0). */
	}
	else
		return MAD_FLOW_CONTINUE;
  }
#endif ////////////////////////////////////////////

  lapHour=timelapse/3600;
  lapMin=(timelapse-lapHour*3600)/60;
  lapfSec=timelapse-3600*lapHour-60*lapMin;
//  printf(" %02d:%02d:%.1f of [%02d:%02d:%.1f]   \r", lapHour, lapMin, lapfSec, durHour, durMin, durfSec);
//  fflush(stdout);

  //printf("Header flags: 0x%04X\n",header->flags);

  snprintf(strPassTime, sizeof(strPassTime), "%02d:%02d:%02d - [%02d:%02d:%02d]",
					lapHour, lapMin, (int)lapfSec, durHour, durMin, (int)durfSec);

  if( !pcmparam_ready && pcmdev_ready ) {  /* For the first complete frame */
	draw_PassTime(0);
	pcmparam_ready=true;
  }
  else
	draw_PassTime(1000000);

  /* 当按下快进时，跳过中间帧 */
  /* Check timelapse, to skip/ignore if <preset_timelapse. */
  if(madCMD==CMD_FFSEEK) {
	if(timelapse < preset_timelapse) {
		return MAD_FLOW_IGNORE;
	}
	else {
		madCMD=CMD_NONE;
		draw_PassTime(0); /* Update passtime label */
	}
  }



  return mad_ret;
}


/* 回调函数，在每次读取帧头后执行．这里作为前处理，将帧头位置和时间推入FILO索引
 * To calculate time duration only.
 */
static enum mad_flow header_preprocess(void *data,  struct mad_header const *header )
{
  MP3_HEADER_POS head_pos;
  struct buffer *buffer = data;

  /* Count duration */
  duration_frac += header->duration.fraction;
  duration_sec  += header->duration.seconds;

  /* Fill in filo_headpos */
  head_pos.poff= (buffer->decoder->sync->stream.this_frame)-(buffer->decoder->sync->stream.buffer);
  head_pos.timelapse=(1.0*duration_frac/MAD_TIMER_RESOLUTION) + duration_sec;
  egi_filo_push(filo_headpos, (void *)&head_pos);

  /* Do not decode */
  return MAD_FLOW_IGNORE;
}


/*　回调函数, 在每次完成mad_header(frame)_decode()后执行．
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */

static
enum mad_flow error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame)
{
  #if 0
  struct buffer *buffer = data;

  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);
  #endif

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
  return MAD_FLOW_CONTINUE;
}

/* MP3解码函数:	初开始化解码器,进行MP3解码.
 * This is the function called by main() above to perform all the decoding.
 * It instantiates a decoder object and configures it with the input,
 * output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */

static
int mp3_decode(unsigned char const *start, unsigned long length)
{
  struct buffer buffer;
  struct mad_decoder decoder;
  int result;

  /* initialize our private message structure */
  buffer.start  = start;
  buffer.length = length;

  /* configure input, output, and error functions */
  mad_decoder_init(&decoder, &buffer,
		   input_all, header /* header */, 0 /* filter */, output,
		   error, 0 /* message */);

  /* start decoding */
  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  /* release the decoder */
  mad_decoder_finish(&decoder);

  return result;
}

/* MP3前处理解码函数:	初开始化解码器,读取所有帧头．
   Preprocess decoding. Push all header info.  No output.
 */
static int decode_preprocess(unsigned char const *start, unsigned long length)
{
  struct buffer buffer;
  struct mad_decoder decoder;
  int result;

  /* initialize our private message structure */
  buffer.start  = start;
  buffer.length = length;
  buffer.decoder = &decoder;

  /* configure input, output, and error functions */
  mad_decoder_init(&decoder, &buffer,
                   input_all, header_preprocess /* header */, 0 /* filter */, 0 /* output */,
                   0 /* error */, 0 /* message */);

  /* start decoding */
  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  /* release the decoder */
  mad_decoder_finish(&decoder);

  return result;
}


#endif
