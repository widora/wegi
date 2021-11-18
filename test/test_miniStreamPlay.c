/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Reference: libmad-0.15.1b   minimad.c

Usage example:
	./test_miniStreamPlay http://kathy.torontocast.com:3530/stream

Note:
1. Adjust http_stream timeout to fit with RingBuffer size.

TODO:
1. XXX mp3, 22050 Hz, stereo, s16p, 96 kb/s
2. Decode aac stream.


Journal:
2021-11-14:
	Create the file.
2021-11-16:
	Add miniMadplay for MPEG decoding.
2021-11-18:
	Preprocess for rawbuffer to find/mark complete frames.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <egi_https.h>
#include <egi_timer.h>
#include <egi_log.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_debug.h>
#include <egi_ringbuffer.h>

#include <egi_input.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <egi_pcm.h>
#include "mad.h"


void *indata;
EGI_RINGBUFFER *ringbuff;

size_t http_download_callback(void *ptr, size_t size, size_t nmemb, void *data);
size_t http_header_callback(void *ptr, size_t size, size_t nmemb, void *stream);

int stream_type;  /* 0--aac,  1--mpeg */
enum {
	STREAM_UNKNOWN		=0,
	STREAM_AUDIO_AAC	=1,
	STREAM_AUDIO_MPEG	=2,
	STREAM_LIMIT		=3,
};
const char *strStreamType[STREAM_LIMIT]={"Unknown","audio/aac", "audio/mpeg"};
char strStreamName[128]="Unknown";

/* ------- MP3 :: Madplay  -------- */
#define RINGBUFFER_SIZE (512*1024)
#define RAWBUFFER_SIZE (16*1024)
const unsigned char rawbuffer[RAWBUFFER_SIZE+8]; /* frame raw data buffer, 8bits of 1s for end token for a frame data. */
unsigned int leftlen;			/* uncompelete frame data left in rawbuffer */
void *madplay_ringbuffer(void *arg);
pthread_t thread_madplay;


const char *mad_mode_str[]={
	"Single channel",
	"Dual channel",
	"Joint Stereo",		// joint (MS/intensity) stereo */
	"Normal LR Stereo",
};

static snd_pcm_t *pcm_handle;      /* PCM播放设备句柄 */
static int16_t  *data_s16l=NULL;   /* FORMAT_S16L 数据 */
static bool pcmdev_ready;	   /* PCM 设备ready */
static bool pcmparam_ready;	   /* PCM 参数ready */

/* Frame counter 帧头计数 */
static unsigned long	  tmpHeadOff=0;
static unsigned long	  lastHeadOff=16*1024; /* The last MP3 header position offset to rawbuffer */
static unsigned long   	  header_cnt;  /* Frame header counter */
static unsigned long	  frame_cnt;   /* Frames counter */
static unsigned long 	  feed_cnt;    /* Data feed session counter */
static unsigned long	  decode_cnt;  /* Decode session counter */

/* Time duration for a MP3 file 已播放时长 */
static unsigned long long timelapse_frac;
static unsigned int	  timelapse_sec;
static float		  timelapse;
static float		  lapfSec;
static int 		  lapHour, lapMin;

//static float		  preset_timelapse;

/* Time duration for a MP3 file 总时长 */
//static unsigned long long duration_frac;
//static unsigned int	  duration_sec;
//static float		  duration;
static float		  durfSec;
static int		  durHour, durMin;

//static unsigned long long poff;		/* Offset to buff of mp3 file */
//static unsigned long long fsize;	/* file size */

/* This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
  unsigned char const *start;
  unsigned long length;
  const struct mad_decoder *decoder;
};

/* Input callback: feed whole rawbuffer[] into stream */
static enum mad_flow input_rawbuffer(void *data, struct mad_stream *stream);
/* Header callback: to seek/get the last Header position in rawbuffer[] */
static enum mad_flow header_preprocess(void *data,  struct mad_header const *header );
/* Preprocess decoding: to count all Headers */
static int rawdata_preprocess(unsigned char const *rawdata, unsigned long length);

/* Input callback: Feed data into stream, usually not as whole rawbuffer[] */
static enum mad_flow input_data(void *data, struct mad_stream *stream);
/* Header callback: Process header data */
static enum mad_flow header(void *data,  struct mad_header const *header);
/* Output callback: Transform decoded data into S16L format */
static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm);
/* Error callback */
static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame);
/* Decode fucntion */
static int mad_decode(unsigned char const *rawdata, unsigned long length);


/*----------------------------
	   MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	char strRequest[256+64];

#if 0	/* Start egi log */
  	if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  	}
	EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif
	egi_log_silent(true);

	/* For http, conflict with curl?? */
	printf("start egitick...\n");
	tm_start_egitick();

	/* prepare GET request string */
        memset(strRequest,0,sizeof(strRequest));

	/* Requset string from user input */
        strcat(strRequest,argv[1]);
        EGI_PLOG(LOGLV_CRITICAL,"Http request head:\n %s\n", strRequest);

	/* Create ring buffer */
	ringbuff=egi_ringbuffer_create(RINGBUFFER_SIZE);
	if(ringbuff==NULL) exit(-1);

  	/* Set termI/O 设置终端为直接读取单个字符方式 */
  	egi_set_termios();

  	/* Prepare vol 启动系统声音调整设备 */
  	egi_getset_pcm_volume(NULL,NULL);

  	/* Open pcm playback device 打开PCM播放设备 */
  	if( snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
  		printf("Fail to open PCM playback device!\n");
        	exit(-1);
  	}

	/* Start madplay thread */
       if( pthread_create(&thread_madplay, NULL, madplay_ringbuffer, NULL)!=0 ) {
		printf("Fail to start thread_madplay!\n");
		exit(-1);
	}

while(1) {

        if( https_easy_stream( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 0, 30,
				 argv[1], indata, http_download_callback, http_header_callback) !=0 )
	{
       		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download_stream! try again...\n");
	} else {
		EGI_PLOG(LOGLV_INFO, "Ok, finish downloading stream!\n");
	}

  } /* while */

	egi_ringbuffer_free(&ringbuff);

	exit(EXIT_SUCCESS);
}


///////////////////////  1. Functions for https_easy_stream  ///////////////////////

/*-----------------------------------------------
 A callback function for write data.
------------------------------------------------*/
size_t http_download_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
        size_t written=0;
	size_t data_size=size*nmemb;
//	printf("Download size*nmemb = %zu\n", chunksize);

	//if(chunksize>0)
	// mad_decode(ptr, chunksize);

	/* Write into ringbuffer */
	written=egi_ringbuffer_write(ringbuff, ptr, data_size);

	return data_size;
}

/*-----------------------------------------------
A callback function when receive a header data
Each head line triggers a callback!
------------------------------------------------*/
size_t http_header_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
        size_t data_size=size*nmemb;
	char *ps;

	/* Http header comes line by line! */
//	printf("%s",(char *)ptr);

	/* Check stream type */
	if(strstr(ptr, "Content-Type:")==ptr) {
		//printf("---> %s", (char *)ptr);
		if( strstr(ptr, "audio/mpeg") )
			stream_type=STREAM_AUDIO_MPEG;
		else if( strstr(ptr, "audio/aac") )
			stream_type=STREAM_AUDIO_AAC;
		else
			stream_type=STREAM_UNKNOWN;

//		egi_dpstd("Start to receive stream '%s'.\n", strStreamType[stream_type]);
	}

	/* Get stream name */
	if( (ps=strstr(ptr,"-name:")) ) {
		ps +=strlen("-name:");
		memset(strStreamName,0,sizeof(strStreamName));
		strncpy(strStreamName, ps, data_size-(ps-(char*)ptr));
		cstr_trim_space(strStreamName);
	}
	//else
	//	strcpy(strStreamName,"Unknown");

	return data_size;
}

///////////////////////  2. Functions for MPEG stream preprocessing  /////////////////////////

/* Input rawbuffer to stream */
static enum mad_flow input_rawbuffer(void *data, struct mad_stream *stream)
{
  struct buffer *buffer = data;

  if (!buffer->length)
    	return MAD_FLOW_STOP;

  /* set stream buffer pointers, pass user data to stream ... */
  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}

 /* To count frames in the current buffer */
static enum mad_flow header_preprocess(void *data,  struct mad_header const *header )
{
  struct buffer *buffer = data;

  /* Get header offset, the frame MAYBE NOT complete?!! */
  if(tmpHeadOff!=0) lastHeadOff=tmpHeadOff;
  tmpHeadOff=(buffer->decoder->sync->stream.this_frame)-(buffer->decoder->sync->stream.buffer);

#if 1 /* TEST Header :----------- */
  //char *headbytes=(char*)(buffer->decoder->sync->stream.this_frame);
  char *headbytes=(char*)(rawbuffer+tmpHeadOff);
  //printf(".....Head bytes: 0x%02x, 0x%02x \n", (uint8_t)headbytes[0], (uint8_t)headbytes[1]);
  if((uint8_t)(headbytes[0])!=0xFF)printf("xxxxx Head Error xxxxx! \n");
#endif

  /* Do not decode this frame */
  return MAD_FLOW_IGNORE;
}

/*  Preprocess decoding. To search through all headers and get the last Header offset position */
static int rawdata_preprocess(unsigned char const *start, unsigned long length)
{
  struct buffer buffer;
  struct mad_decoder decoder;
  int result;

  /* initialize our private message structure */
  buffer.start  = start;
  buffer.length = length;
  buffer.decoder = &decoder;

  /* Reset tmpHeadOff */
  tmpHeadOff=0;

  /* configure input, output, and error functions */
  mad_decoder_init(&decoder, &buffer,
                   input_rawbuffer, header_preprocess /* header */, 0 /* filter */, 0 /* output */,
                   0 /* error */, 0 /* message */);

  /* start decoding */
  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  //printf("Last header offset: %lu\n", lastHeadOff);

  /* release the decoder */
  mad_decoder_finish(&decoder);

  return result;
}

///////////////////////  3. Functions for MPEG stream decoding  /////////////////////////

/* To feed mpeg raw data  */
static enum mad_flow input_data(void *data, struct mad_stream *stream)
{
  int nread;

  /* Read data from ringbuffer to rawbuffer, and feed to stream */
  if(ringbuff->datasize > RAWBUFFER_SIZE) {
	nread=0;

	/* Shift remaining data forward of the rawbuffer */
	if(lastHeadOff!=16*1024)
		memcpy((void*)rawbuffer, (void*)rawbuffer+lastHeadOff, 16*1024-lastHeadOff);
	else
		printf(" -------- lastHeadOff==16*1024 --------\n"); /* Impossibe. */

	/* Read data from RingBuffer to fill up the rawbuffer[] */
  	nread=egi_ringbuffer_read(ringbuff, (void*)rawbuffer+(16*1024-lastHeadOff), lastHeadOff);
//	printf("lastHeadOff=%lu, nread=%zu, ringbuff->datasize=%zu (Bs)\n", lastHeadOff, nread,ringbuff->datasize);

	/* Preprocess and decode */
	if(nread>0) {
		/* Preprocess to count available headers and get lastHeadOff */
		rawdata_preprocess(rawbuffer, RAWBUFFER_SIZE);

		/* NOW: ---->lastHeadOff updated! */

		/* Feed data of complete frames into stream  */
		mad_stream_buffer(stream, rawbuffer, lastHeadOff+8); /* 8bits of 1s as end token of a frame data! */
		feed_cnt++;
	}
	else {
	  	printf("\rBuffering...  "); fflush(stdout);
		usleep(10000);
	}

	  return MAD_FLOW_CONTINUE;
  }
  else
	  return MAD_FLOW_STOP;
}

/* Transform MAD's high-resolution samples down to 16 bits */
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

 /* This is the output callback function. It is called after each frame of
 *  MPEG audio data has been completely decoded. The purpose of this callback
 *  is to output (or play) the decoded PCM audio.
 */
static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
  unsigned int i,k;
  unsigned int nchannels=2, nsamples, samplerate;
  mad_fixed_t const *left_ch, *right_ch;

  /*　声道数　采样率　PCM数据 */
  /* pcm->samplerate contains the sampling frequency */
  /* NOTE: nchannels and samplerate MAY chages for the first serveral frames. see below. */
  //nchannels = pcm->channels;
  //samplerate= pcm->samplerate;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];
  nsamples  = pcm->length;

  /* 为PCM播放设备设置相应参数 */
  if( !pcmdev_ready && header->flags&0x1 ) {  /* assume (header->flags&0x1)==1 as start of complete frame */
        /* Update nchannles and samplerate */
        nchannels=pcm->channels; //MAD_NCHANNELS(header);
        samplerate= pcm->samplerate;

        /* NOTE: nchannels/samplerate from the first frame header MAY BE wrong!!! */
//        printf("header_cnt:%lu, flags:0x%04X, mode: %s, nchanls=%d, sample_rate=%d, nsamples=%d\n",
//                        header_cnt, header->flags, mad_mode_str[header->mode], nchannels, samplerate, nsamples);
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
	  if(data_s16l==NULL)
		exit(1);

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


/* Parse keyinput and frame header */
static enum mad_flow header(void *data,  struct mad_header const *header )
{
  int percent;
  char ch;
  int mad_ret=MAD_FLOW_CONTINUE;

//  struct buffer *buffer = data;

  /* Count header frame */
  header_cnt++;
  frame_cnt++;

  /* Parse keyinput...*/
  read(STDIN_FILENO, &ch,1);
  switch(ch) {
	case 'q': 	/* Quit */
	case 'Q':
		printf("\n\n");
		exit(0);
		break;
	case '+':	/* Volume up */
		egi_adjust_pcm_volume(5);
		egi_getset_pcm_volume(&percent,NULL);
		printf("\r Vol: %d%%",percent); fflush(stdout);
		usleep(250000);
		break;
	case '-':	/* Volume down */
		egi_adjust_pcm_volume(-5);
		egi_getset_pcm_volume(&percent,NULL);
		printf("\r Vol: %d%%",percent); fflush(stdout);
		usleep(250000);
		break;
  }

  /* Time elapsed 计算当前播放时间 ---or read FILO. */
  timelapse_frac += header->duration.fraction;
  timelapse_sec  += header->duration.seconds;
  timelapse=(1.0*timelapse_frac/MAD_TIMER_RESOLUTION) + timelapse_sec;

  /*** Print MP3 file info 输出播放信息到终端
   * Note:
   *	1. For some MP3 file, it should wait until the third header frame comes that it contains right layer and samplerate!
   */
  if(!pcmparam_ready ) {
	if( header_cnt>2 ) {
		printf("\n");
		printf(" ------------------------\n");
		printf("    A miniStreamPlayer\n");
		printf(" ------------------------\n");
		printf(" Libcurl+Libmad+EGI\n");
  		printf(" Layer_%d\n %s\n bitrate:    %.1fkbits\n samplerate: %dHz\n",
			header->layer, mad_mode_str[header->mode], 1.0*header->bitrate/1000, header->samplerate);
		printf(" Name: %s\n", strStreamName);
		printf(" ------------------------\n");
		pcmparam_ready=true; /* Set */
	}
	else
		return MAD_FLOW_CONTINUE;
  }

//printf(" Buffer data: %dBs\n", ringbuff->datasize);
  lapHour=timelapse/3600;
  lapMin=(timelapse-lapHour*3600)/60;
  lapfSec=timelapse-3600*lapHour-60*lapMin;
  printf("\r %02d:%02d:%.1f of [%02d:%02d:%.1f]   ", lapHour, lapMin, lapfSec, durHour, durMin, durfSec);
  fflush(stdout);

  return mad_ret;
}

/*  It is called whenever a decoding error occurs. */
static
enum mad_flow error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame)
{
  #if 1
  struct buffer *buffer = data;

  //leftlen=

  fprintf(stderr, "\nDecodes[%lu] feed[%lu] frame_cnt=%lu: decoding error 0x%04x (%s) at byte offset %u.\n",
	  decode_cnt, feed_cnt,frame_cnt,
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);
  #endif

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
  // return MAD_FLOW_CONTINUE;
  return MAD_FLOW_IGNORE;
}



/* Perform mpeg decoding. */
static int mad_decode(unsigned char const *start, unsigned long length)
{
  struct buffer buffer;
  struct mad_decoder decoder;
  int result;

  /* initialize our private message structure */
  buffer.start  = start;
  buffer.length = length;
  buffer.decoder = &decoder;

  /* init frame_cnt */
  frame_cnt=0;

 /* configure input, output, and error functions */
  mad_decoder_init(&decoder, &buffer,
		   input_data, header /* header */, 0 /* filter */, output,
		   error, 0 /* message */);

  /* Start decoding */
  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  printf("\n ---> frame_cnt=%lu \n", frame_cnt);

  /* release the decoder */
  mad_decoder_finish(&decoder);

  decode_cnt++;

  return result;
}


///////////////////////  4. Thread function  ///////////////////


/*  Read data from ringbuffer and madplay it. */
void *madplay_ringbuffer(void *arg)
{
	size_t nread;

INIT_BUFFER:
  /* Init buffer ... */
  printf("Buffering stream...  \n");
  while( ringbuff->datasize < 128*1024 ) {
	if( ringbuff->datasize>0 && stream_type!=STREAM_AUDIO_MPEG) {
		printf("Stream type '%s;, NOT supported yet!\n", strStreamType[stream_type]);
		exit(1);
	}
	printf("\rBuffering %dBs ", ringbuff->datasize); fflush(stdout);

 	usleep(50000);
  };

	/* Loop feed data in input_data() */
	mad_decode((unsigned char const *)rawbuffer, RAWBUFFER_SIZE);
	goto INIT_BUFFER;

	return (void *)0;
}
