/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Reference: libmad-0.15.1b   minimad.c

Usage:
	 example:
		./test_miniStreamPlay -m -v -s sndcard http://kathy.torontocast.com:3530/stream
		( option: -m: to show StreamTitle, otherwise Untitled. )
        '+'     Increase volume +5%　　 增加音量
        '-'     Decrease volume -5%　　 减小音量
	'm'	Mute/Demute
	'q'	Quit

	--- A typical MPEG Audio Frame Header (32bits) ---

	Frame sync	(11bits)   All 1s
	MPEG Version	(2bits)
	Layer descript	(2bits)
	Protection bit	(1bit)
	Bitrate		(4bits)
	Sampling rate   (2bits)
	Padding bit	(1bit)
	Private bit	(1bit)
	Channel Mode	(2bits)
	Mode extension	(2bits)
	Copyright	(1bit)
	Original	(1bit)
	Emphasis	(2bits)


		----- AAC or MP3? -----

   1. Nealy same file size with same bitrate.
   2. MP3 performs a little better in high frequency when bitrate>=192kbps.
   3. With reducing of bitrate, MP3 drops more rapidly in high frequency.
   A recommendation:
           <=128kbps use AAC
           >=192kbps use MP3


Note:
1. Adjust http_stream timeout to fit with RingBuffer size. ....120s seems OK
   Set a big timeout value to avoid frequent breakout and clean_up of http_easy_perform,
   and a new round(new file) of easy_perform may receive repeated data, which
   makes RingBuffer overflow often!

   Key_point: Too often breakdown of http connection will introduce repeated data
	      and cause RingBuffer to overflow/overrunn!

2. A tail of 16bytes data (at least 8bytes, starting with an audio header)
   should be attached at feeding rawdata.
   OR it may produce blip noise between of calling of two mad_decode().
3. A HTTP header implys a new session/start of data transmission.

TODO:
1. XXX mp3, 22050 Hz, stereo, s16p, 96 kb/s
   decoding error 0x0235 (bad main_data_begin pointer) at byte offset 0
   Resample to 48000Hz  ---OK, see madplay_resample.h
2. Decode aac stream.
3. XXX Dump half content of RingBuffer if overflow occurs.
   ---OK, drop each 1/10 frame seems ok.
   3.1 ---> decoding error 0x0235 (bad main_data_begin pointer)


Journal:
2021-11-14:
	Create the file.
2021-11-16:
	Add miniMadplay for MPEG decoding.
2021-11-18:
	Preprocess for rawbuffer to find/mark complete frames.
2021-11-19/20:
	Request and extract icy-meta content of StreamTitle, in http_download_callback().
2021-11-22:
	Add madplay_resample.h. If samplerate not 48kHz/44.1kHz, then resample to 48kHz.
2021-11-23:
	1. Mute/Demute
	2. If ringbuff_almost_full, then trigger to drop every 1/10 frames.
2022-01-09:
	1. http_download_callback(): Check stream_type first.
	2. madplay_ringbuffer(): If stream_type is NOT audio/mpeg, DO NOT exit.
2022-03-27/28:
	1. static enum mad_flow output(): Try to detect if USB sndcard unplugged, and
	   then wait and try to reconnect/open.
2022-03-29:
	1. Add option -s for sound card selection.
2022-03-30:
	1. For a virt sncard, snd_pcm_state() CAN NOT detect HW disconnection!
	2. If USB sncard is unplugged during stream buffering, egi_pcmhnd_playBuff()
	   will return all 0s afterward. In this case, it needs to re_open the pcm handler.
2022-03-31:
	1. Test egi_getset_ctl_elemValue()
2023-02-23:
	1. Let go for stream_unknown ...

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
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
#include "madplay_resample.h"

#define MIXER_VOLUME_CONTROL  1   /* 1: By SND_Mixer, 0: By SND_CTL  */

#define RINGBUFFER_SIZE (256*1024)	/* Bytes, RingBuffer MAX space. */
#define INIT_BUFFER_SIZE (128*1025)	/* Buffer size as a threshold, before start decoding/playing buffered data. */
#define MAD_FEEDBUFFER_SIZE (32*1024)	/* Bytes, FeedBuffer, as MAX size data to feed into mad_decoder */
				 /* If received data_size > ringbuff->buffsize-ringbuff->datasize - MAD_FEEDBUFFER_SIZE
				    then set ringbuff_almost_full and to trigger dropping frames (every 1/10?)
				  */

bool verbose_ON;

void *indata;
EGI_RINGBUFFER *ringbuff;

size_t http_download_callback(void *ptr, size_t size, size_t nmemb, void *data);
size_t http_header_callback(void *ptr, size_t size, size_t nmemb, void *stream);

int stream_type;  /* 0--aac,  1--mpeg */
enum {
	STREAM_UNKNOWN		=0,
	STREAM_AUDIO_AAC	=1,
	STREAM_AUDIO_MPEG	=2,
	STREAM_TEXT_HTML	=3,
	STREAM_LIMIT		=4,
};
bool	Is_StreamHeader;	/* Indicate a new round of stream transission,
				 * set in http_header_callback(), reset in http_download_callback().
				 */
const char *strStreamType[STREAM_LIMIT]={"Unknown","audio/aac", "audio/mpeg", "text/html"};
char strStreamName[128]="Unknown";
char strStreamTitle[128]="Untitled";

/* For ICY meta_data.
 * 1. Extract metadata in http_download_callback()
 * 2. Reset params in http_header_callback()
 */
bool metadata_ON;
const char *icy_request[1]={"Icy-MetaData:1"};
int  metalen;		/* Length of metadata string */
int  meta_int;		/* the metadata interval icy-metaint:16000 */
int  intdata_count;  	/* interval data counter
		      	 * count all http download mpeg data(exluding metadata!), in order to get/check meta_int!
		      	 */
char metadata[4096];  /* 1byte(metalen/16)+metalen :
		       MAX. possible icy metatata string length = 255*16 =4096-16. */
int  mpos;	      /* Current metadata write_position */
bool refresh_display; /* If true, then refresh planel display at mad decoder processor */

/* ------- MPEG :: Madplay  -------- */

const unsigned char rawbuffer[MAD_FEEDBUFFER_SIZE+16]; /* 16bytes(at least 8bytes) as end of a MPEG feed chunk. */
unsigned int 	leftlen;			/* uncompelete frame data left in rawbuffer */
bool 		ringbuff_almost_full;		/* RingBuffer is almost full, trigger to drop frames. */
void 		*madplay_ringbuffer(void *arg);
pthread_t 	thread_madplay;


const char *mad_mode_str[]={
	"Single channel",
	"Dual channel",
	"Joint Stereo",		// joint (MS/intensity) stereo */
	"Normal LR Stereo",
};

const char *sndcard="default";
static snd_pcm_t *pcm_handle;      /* PCM播放设备句柄 */
static int16_t  *data_s16l=NULL;   /* FORMAT_S16L 数据 */
static bool 	pcmdev_ready;	   /* PCM 设备ready */
static bool 	pcmparam_ready;	   /* PCM 参数ready */
struct resample_state rstate;      /* Resample state, init at output() */
mad_fixed_t 	(*resampled)[2][MAX_NSAMPLES]; /* Resampled data, calloc at output() */
bool  		need_resample;
int vpercent;			  /* Volume in percent */

/* Frame counter 帧头计数 */
static unsigned long	  tmpHeadOff=0;
static unsigned long	  lastHeadOff=MAD_FEEDBUFFER_SIZE; /* The last MP3 header position offset to rawbuffer */
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
//static float		  durfSec;
//static int		  durHour, durMin;

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


void print_help(const char *name)
{
        printf("Usage: %s [-hms:v] URL\n", name);
        printf("-h     This help.\n");
        printf("-m     Request metadata.\n");
	printf("-s:    Plughw, or virt sound card as defined in asound.conf, Example 'plughw:1,0'\n");
	printf("-v     Verbose debug.\n");
}

/*----------------------------
	   MAIN
-----------------------------*/
int main(int argc, char **argv)
{
	char *strURL=NULL;

        /* Parse input option */
        int opt;
        while( (opt=getopt(argc,argv,"hms:v"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
				exit(0);
                                break;
			case 'm':
				metadata_ON=true;
				break;
			case 's':
				sndcard=optarg;
				printf("sndcar: %s\n",sndcard);
				break;
			case 'v':
				verbose_ON=true;
				break;
			default:
				break;
		}
	}
        strURL=argv[optind];
        if(strURL==NULL) {
                print_help(argv[0]);
                exit(-1);
        }

#if 0	/* Start egi log */
  	if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  	}
	EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif
	if(!verbose_ON)
		egi_log_silent(true);

	/* For http, conflict with curl?? */
	printf("start egitick...\n");
	tm_start_egitick();

	/* Create ring buffer */
	ringbuff=egi_ringbuffer_create(RINGBUFFER_SIZE);
	if(ringbuff==NULL) exit(-1);

  	/* Set termI/O 设置终端为直接读取单个字符方式 */
  	egi_set_termios();

  	/* Prepare vol 启动系统声音调整设备, before snd_pcm_open() OK. */
	#if 1
  	if(egi_getset_pcm_volume(sndcard, &vpercent, NULL)!=0)
		egi_dpstd(DBG_RED"Fail to init volume mixer!\n"DBG_RESET);
	#else
	egi_getset_ctl_elemValue(sndcard, "Speaker Playback Volume", &vpercent, NULL, 0);
	#endif
	egi_dpstd(DBG_GREEN"Volume: %d%%\n"DBG_RESET, vpercent);

  	/* Open pcm playback device 打开PCM播放设备 */
  	if( snd_pcm_open(&pcm_handle, sndcard, SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
//  	if( snd_pcm_open(&pcm_handle, "plughw:1,0", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
  		egi_dpstd(DBG_RED"Fail to open PCM playback device!\n"DBG_RESET);
        	exit(-1);
  	}

	/* Allocate space for resampled data */
	resampled=(typeof(resampled))malloc(sizeof(*resampled));
	if(resampled==NULL) {
	      egi_dpstd(DBG_RED"Fail to malloc resampled!\n"DBG_RESET);
	      exit(-1);
	}

	/* Start madplay thread */
       if( pthread_create(&thread_madplay, NULL, madplay_ringbuffer, NULL)!=0 ) {
		printf("Fail to start thread_madplay!\n");
		exit(-1);
	}

while(1) {

	/* For each https stream session */
  	//Is_StreamHeader=true; ----> http_header_callback()
	meta_int=0; /* 0 to ignore icy-metadata */

        if( https_easy_stream( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION, 0, 120, strURL,
			  	metadata_ON?1:0, icy_request, indata, http_download_callback, http_header_callback) !=0 )
	{
       		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download_stream! try again...\n");
	} else {
		EGI_PLOG(LOGLV_INFO, "Ok, finish downloading stream!\n");
	}

  } /* while */


	/* Free RingBuffer */
	egi_ringbuffer_free(&ringbuff);
	/* Free sapce for resampled data */
	free(resampled);

	exit(EXIT_SUCCESS);
}


///////////////////////  1. Functions for https_easy_stream  ///////////////////////

/*------------------------------------------------
A callback function when receive a header data

Note:
 1. Each head line will trigger this callback once!
 2. Reset params for meta_data extraction.
-------------------------------------------------*/
size_t http_header_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	char *ps;
        size_t data_size=size*nmemb;

	/* Reset intdata_count when HTTP header starts. */
//	intdata_count=0; see following P3.

	/* The callback fetch http header line by line! */
	printf("%s\n",(char *)ptr);

	/* P1. Check stream type */
	if(strstr(ptr, "Content-Type:")==ptr) {
		//printf("---> %s", (char *)ptr);
		if( strstr(ptr, "audio/mpeg") ) {
			stream_type=STREAM_AUDIO_MPEG;

			/* For each https stream session */
  			Is_StreamHeader=true;
		}
		else if( strstr(ptr, "audio/aac") )
			stream_type=STREAM_AUDIO_AAC;
		else if( strstr(ptr, "text/html") )
			stream_type=STREAM_TEXT_HTML;
		else {
			printf("Unknown stream type: %s\n", (char *)ptr);
			stream_type=STREAM_UNKNOWN;
		}
	}

	/* P2. Get stream name */
	if( (ps=strstr(ptr,"-name:")) ) {
		ps +=strlen("-name:");
		memset(strStreamName,0,sizeof(strStreamName));
		strncpy(strStreamName, ps, data_size-(ps-(char*)ptr));
		cstr_trim_space(strStreamName);
	}

	/* P3. Get icy metadata interval param. */
	if( (ps=strstr(ptr,"icy-metaint:")) ) {
		/* Reset meta_int */
		meta_int=atoi(ps+strlen("icy-metaint:"));
		printf(" ------ meta_int=%d \n", meta_int);

		/* Reset meta data params */
		mpos=0;
		metalen=0;
		intdata_count=0;
		// meta_int =0; TODO where to init.
	}

	return data_size;
}

/*------------------------------------------------------------
 A callback function for http_easy_stream():
 1. Extract meta_data from stream data.
 2. Write clear data(without meta_data) into RingBuffer.
-------------------------------------------------------------*/
size_t http_download_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
        size_t written=0;
	size_t chunk_size=size*nmemb;
//	egi_dpstd("<<<  Stream downloaded chun_size=%zu  >>>\n",chunk_size);

	/* Ptr data bufferd here, and after meta processing all metadata
	   in tmpbuff[] will be cleared! */
	static unsigned char tmpbuff[32*1024]={0};
	/* Left data size in tmpbuff, changing during meta processing as metadata being cleared out. */
	size_t data_size=size*nmemb;
	unsigned char *ptm=tmpbuff; /* Pointer to the END of the clear data in tmpbuff,
				     * and HEAD of unprocessed data.
				     */
	int xlen;
	char *pt;

	/* Check stream type */
	if(stream_type==STREAM_TEXT_HTML) {
		printf("TEXT_HTML: %s\n", (char *)ptr);
		return chunk_size;
	}
	//else if(stream_type!=STREAM_AUDIO_MPEG) {
	else if(stream_type!=STREAM_AUDIO_MPEG && stream_type!=STREAM_UNKNOWN) {  //HK2023-02-23
		egi_dpstd(DBG_YELLOW"stream_type is '%s', NOT audio_mpeg, ignore it!"DBG_RESET, strStreamType[stream_type]);
		return chunk_size;
	}

	/* If start of a new round stream session, update display. */
	if(Is_StreamHeader) {
		refresh_display=true;
		Is_StreamHeader=false;
	}

	/* Copy  ptr data to tmpbuff */
	if(data_size>sizeof(tmpbuff)) {
		printf("\n  ==========  data_size=%zu, tmpbuff(%zu) overflow =========\n", data_size, sizeof(tmpbuff) );
		exit(1); //HK2022-11-28
		return 0;
	}
	memcpy((void *)tmpbuff, ptr, chunk_size);


/*
  Possible icy-metint values: 16000(most), 2040, 1024
*/

  /* Check icy-metadata:  metadata interval >0 */
  if( metadata_ON && meta_int>0) {
	//unsigned char *pstr=NULL;
	//printf(" >>>> ======== Process META ========== <<<<<\n");

	/* 1. If last chunk data has incomplete metadata leave to this chunk. */
	if(mpos>0) {  /* mpos == 1+metalen and mod to 0, if compelete */
		printf(" >>>> =============== case 1 =============== <<<<<\n");
		/* With all remaining metadata in this chunk,  metalen the is SAME as last time! */
		if(chunk_size >= (1+metalen)-mpos) {
			xlen=(1+metalen)-mpos;  /* Remaining metdata length  */
			memcpy(metadata+mpos, tmpbuff, xlen);
			metadata[1+metalen]=0;

			intdata_count=0;  /* unchanged, mod of meta_int */
			mpos=0; /* reset it */
			data_size -=xlen; /* Adjust */

			/* To trim those metadata in the tmpbuff */
			memmove(tmpbuff, tmpbuff+xlen, chunk_size-xlen);

			/* ptm unchanged */

			//ptm NOT changed!
		}
		/* A short chunk data, metadata NOT complete yet. */
		else {
			memcpy(metadata+mpos, tmpbuff, chunk_size);

			intdata_count=0; //intdata_count unchanged! DO NOT count include metadat!
			mpos +=chunk_size;
			data_size=0;
			//pt NOT changed!

			goto END_META;
		}
	}

	/* Processing on.... */

	/* NOW: mpos == 0;
           and intdata_count & data_size updated.
	 */

	/* 2. At least partial metadata contained in tmpbuff[], MAYBE more than 1 metdata occurrence!
	 *    Loop to clear all metadata in tmpbuff[]
         */
	while( intdata_count + data_size-(ptm-tmpbuff) > meta_int ) { /* ptm-tmpbuff: processed/cleared data */

		/* First 1bytes as metalen/16, get it immediately! */
		metalen= 16*ptm[meta_int-intdata_count];

#if 0 /* TEST: ------------ */
		printf("meta_int=%d: intdata_count=%d, data_size=%d, metalen=%d, ptm-tmpbuff=%d\n",
				meta_int, intdata_count, data_size, metalen, ptm-tmpbuff);
#endif
		/* 2.1 If a complete metadata in ptm[], clear the first ONE metadata.  */
		if( data_size-(ptm-tmpbuff)-(meta_int-intdata_count) >= 1+metalen ) {  /* ptm-tmpbuff: cleared data */
			/* Save 1+metadata to metadata[] */
			memcpy(metadata, (char *)ptm+(meta_int-intdata_count), 1+metalen); /* including the 1byte for length */
			metadata[1+metalen]='\0';

			/* Trim metadata in the tmpbuff */
			xlen=meta_int-intdata_count;
			memmove( ptm+xlen,  ptm+xlen+(1+metalen),  data_size-(ptm-tmpbuff)-xlen-(1+metalen) );

			/* Update mpos, data_size for tmpbuff */
			intdata_count=0; /* metadata NOT included, mod of meta_int. */
			ptm += xlen;
			mpos=0;
			data_size -=1+metalen;
		}
		/* 2.2 An incomplete metadata in ptm[] */
		else if( data_size-(ptm-tmpbuff)-(meta_int-intdata_count) >0 ) {
//			printf(" >>>>> Incomplete metadata <<<<<\n");
			xlen=data_size-(ptm-tmpbuff)-(meta_int-intdata_count);

			/* Save to metadat[] */
			memcpy(metadata, (char *)ptm+(meta_int-intdata_count), xlen);

			/* NO necessary to trim tmpbuff[], just update ptm and data_size */

			/* Update ptm */
			ptm += meta_int-intdata_count;

			/* Update mpos, data_size for tmpbuff */
			intdata_count=0;
			mpos=xlen;
			data_size -=xlen;
		}
	} /* End while() */

	/* 3. After while() : then all data left in ptm[] is all cleared data, without metadata.
	 */
	intdata_count += data_size-(ptm-tmpbuff);
	//data_size unchanged.
	//mpos unchanged
	ptm = tmpbuff+data_size;

#if 0	/* Check */
	printf("CHECK meta_int=%d: intdata_count=%d, metalen=%d, data_size=%d, ptm-tmpbuff=%d\n",
				meta_int, intdata_count, metalen, data_size, ptm-tmpbuff);
#endif
  }

END_META:

	/* Get strStreamTitle */
	pt=strstr(metadata+1,"StreamTitle="); /* +1 pass metlenByte */
   	if(pt) {
		int m=0;

		pt +=strlen("StreamTitle=");
		while( pt[m] && pt[m]!=';') {
			strStreamTitle[m]=pt[m];
			m++;
		}
		strStreamTitle[m]='\0';
		refresh_display=true;
	}


	/* NOW: data_size is the size of cleared data in tmpbuff, without metadata!!! data_size MAY BE zero! */

	/* To trigger drop frames */
	if( data_size > ringbuff->buffsize-ringbuff->datasize - MAD_FEEDBUFFER_SIZE ) {
		ringbuff_almost_full=true;
	}
	else
		ringbuff_almost_full=false;

	/* Wait and hope mad decoder header processor will drop frames ... */
	while( data_size > ringbuff->buffsize-ringbuff->datasize )
		usleep(10000);

	/* Write into ringbuffer */
	written=egi_ringbuffer_write(ringbuff, tmpbuff, data_size);
	if(written<data_size) {
		printf("	>>>> RingBuffer overruns! <<<<<\n");
	}

	return chunk_size; //data_size; as changed excluding metadata
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

#if 1 /* TEST FRAME Header :----------- */
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

//////////////////////  3. Functions for MPEG stream decoding  /////////////////////////

/* To feed mpeg raw data  */
static enum mad_flow input_data(void *data, struct mad_stream *stream)
{
  int nread;

  /* Read data from ringbuffer to rawbuffer, and feed to stream */
  if(ringbuff->datasize > MAD_FEEDBUFFER_SIZE) {
	nread=0;

	/* Shift remaining data forward of the rawbuffer */
	if(lastHeadOff!=MAD_FEEDBUFFER_SIZE) /* Initl lastHeadOff =MAD_FEEDBUFFER_SIZE */
		memcpy((void*)rawbuffer, (void*)rawbuffer+lastHeadOff, MAD_FEEDBUFFER_SIZE-lastHeadOff);
	else
		printf("\n -------- lastHeadOff==MAD_FEEDBUFFER_SIZE --------\n"); /* As init  */

	/* Read out data from RingBuffer to fill up the rawbuffer[] */
  	nread=egi_ringbuffer_read(ringbuff, (void*)rawbuffer+(MAD_FEEDBUFFER_SIZE-lastHeadOff), lastHeadOff);
//	printf("lastHeadOff=%lu, nread=%zu, ringbuff->datasize=%zu (Bs)\n", lastHeadOff, nread,ringbuff->datasize);

	/* Preprocess and decode */
	if(nread>0) {
		/* Preprocess to count available headers and get lastHeadOff */
		rawdata_preprocess(rawbuffer, MAD_FEEDBUFFER_SIZE);

		/* NOW: ---->lastHeadOff updated! */

		/* Feed data of complete frames into stream  */
		mad_stream_buffer(stream, rawbuffer, lastHeadOff+16); /* 16bytes(at least 8bytes) as end of a MPEG decoder feed chunk. */
		feed_cnt++;
	}
	else {
	  	printf("\rBuffering...  "); fflush(stdout);
		usleep(10000);
	}

	  return MAD_FLOW_CONTINUE;
  }
  else {
	  printf("\nringbuff->datasize(%zuBs) <= MAD_FEEDBUFFER_SIZE ---- mad flow stops ---\n", ringbuff->datasize);
	  return MAD_FLOW_STOP;
  }
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
int nchannels;
static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
  unsigned int i,k;
  unsigned int nsamples, samplerate;
  static int new_samplerate=0;
  mad_fixed_t const *left_ch, *right_ch;
  int ret;

  /*　声道数　采样率　PCM数据 */
  /* pcm->samplerate contains the sampling frequency */
  /* NOTE: nchannels and samplerate MAY changes for the first serveral frames. see below. */
  if(nchannels!=pcm->channels) {
	  nchannels = pcm->channels;
	  refresh_display=true;
  }

  //samplerate= pcm->samplerate;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];
  nsamples  = pcm->length;
  samplerate= pcm->samplerate;

  /* 为PCM播放设备设置相应参数 */
  if( !pcmdev_ready ) {  //&& header->flags&0x1 ) {  /* assume (header->flags&0x1)==1 as start of complete frame */
        /* Update nchannles and samplerate */
        nchannels=pcm->channels; //MAD_NCHANNELS(header);
        //samplerate= pcm->samplerate;

        /* NOTE: nchannels/samplerate from the first frame header MAY BE wrong!!! */
//        printf("header_cnt:%lu, flags:0x%04X, mode: %s, nchanls=%d, sample_rate=%d, nsamples=%d\n",
//                        header_cnt, header->flags, mad_mode_str[header->mode], nchannels, samplerate, nsamples);

        /* Init resampling */
        if( samplerate!=48000 && samplerate!=44100 ) {
	   #if 1
		float dd;
		new_samplerate=modff(1.0*samplerate/11025, &dd)==0.0 ? 44100:48000;
	   #else
		new_samplerate=48000;
	   #endif
                printf("Samplerate=%dHz, init resampling to %dHz...\n", samplerate, new_samplerate );
                resample_init(&rstate, samplerate, new_samplerate);

  		#if 0 /* XXX Recalloc space for resampled data XXX NOPE, fixed! */
		resampled=(typeof(resampled))realloc((void *)resampled, sizeof(*resampled));
		if(resampled==NULL) {
		      printf("Fail to malloc resampled!\n");
		      exit(1);
		}
		#endif

                need_resample=true;
        }
        else {
                need_resample=false;
		new_samplerate=samplerate;
        }

	/* Set params */
        if( egi_pcmhnd_setParams(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, nchannels,
//	    need_resample?48000:samplerate) <0 ) {
	    need_resample?new_samplerate:samplerate) <0 ) {
                egi_dpstd(DBG_RED"Fail to set params for PCM playback handle.!\n"DBG_RESET);
        } else {
                //printf("EGI set pcm params successfully!\n");
                pcmdev_ready=true;
        }
  }

  if(pcmdev_ready) {
        /* A. Need to resample */
        if( need_resample ) {
                /* Resample, with new nsamples! */
		//printf("Start resampling...\n");
                nsamples = resample_block( &rstate, pcm->length, left_ch, (*resampled)[0]);
                if(nchannels==2)
                        resample_block( &rstate, pcm->length, right_ch, (*resampled)[1]);
		//printf("New nsamples=%d\n",nsamples);

                /* 转换成S16L格式 */
                /* Realloc data for S16 */
                data_s16l=(int16_t *)realloc(data_s16l, nchannels*nsamples*2);
                if(data_s16l==NULL)
                        exit(1);
                /* Scale to S16L */
                for(k=0,i=0; i<nsamples; i++) {
                	data_s16l[k++]=scale((*resampled)[0][i]);               /* Left */
                        if(nchannels==2)
                                data_s16l[k++]=scale((*resampled)[1][i]);       /* Right */
                }
        }
        /*  B. No need to resample */
        else {
                /* 转换成S16L格式 */
                /* Realloc data for S16 */
                data_s16l=(int16_t *)realloc(data_s16l, nchannels*nsamples*2);
                if(data_s16l==NULL)
                        exit(1);
                /* Scale to S16L */
                for(k=0,i=0; i<nsamples; i++) {
                        data_s16l[k++]=scale(*(left_ch+i));             /* Left */
                        if(nchannels==2)
                                data_s16l[k++]=scale(*(right_ch+i));    /* Right */
                }

        }

#if 0 /* TEST: ---------------- */
egi_dpstd(DBG_GREEN"Samplerate: %dHz --> %dHz, Samples: %d --> %d\n"DBG_RESET,
				samplerate, new_samplerate, pcm->length, nsamples);
#endif


#if 0	/* Check available space for plaback data
	 * !!! It returns ok, even though the USB Speaker is unplugged.
	 */
	ret=snd_pcm_avail_update(pcm_handle);
	if( ret <0 ) {
	   if(ret==-EPIPE)
		egi_dpstd(DBG_YELLOW"Xrun occured!\n"DBG_RESET);
	   else
		egi_dpstd(DBG_RED"ALSA avail update ret=%d\n"DBG_RESET,ret);
	}
	else
		egi_dpstd("ALSA avail update ret=%d\n",ret);
#endif

    /* For computing average iwrite speed */
	struct timeval tms, tme;
	int tdm;
	static int cnt=0;
	static int cntSamples=0;
	static int cntMs=0;

	gettimeofday(&tms, NULL);

        /* C. Playback 播放PCM数据 */
	//printf("--->\n");
        ret=egi_pcmhnd_playBuff(pcm_handle, true, (void *)data_s16l, nsamples);
	/* TODO: To detect audio device stuck/error, Example: USB speaker unplugged.
	   Then try to reopen and reset pcmdev_ready
	   Possible return status when the audio device disconnected.
	   1. It returns -EIO, it happens when USB sncard is unplugged during playback.
	   2. OR It ALWAYS returns ret=0, it happens if USB sncard is unplugged during stream buffering.
	 */

/* TEST:-------------- if sound card lost, espcially if USB card unplugged */
#if 1  /* NOT call snd_pcm_state(), For a virt sncard, snd_pcm_state() CAN NOT detect diconnection!!! */
   #if 0  /* 1. Compute average iwrite speed, if too fast, the USB may be unplugged?? */
  	//printf("nch=%d, newsrate=%d, nsamples=%d, tdm=%d\n", pcm->channels, new_samplerate, nsamples, tdm);
	if(cnt<100) {
		gettimeofday(&tme, NULL);
		tdm=tm_signed_diffms(tms, tme);
		cntSamples += nsamples;
		cntMs += tdm;
		cnt++;
	}
	else {
		if( (cntMs==0 || cntSamples/cntMs >1000) && lastHeadOff!=MAD_FEEDBUFFER_SIZE ) {  /* lastHeadOff==MAD_FEEDBUFFER_SIZE as init */
			printf(DBG_RED"\nSound card lost? state=%d. try to reopen it...\n"DBG_RESET, snd_pcm_state(pcm_handle));

   #else  /* 2. Check return value of egi_pcmhnd_playBuff() */
	if(ret==-EIO || ret==-EBADFD || ret==0 ) {  /* IO error, bad FD, as sound card disconnected. */
		if( lastHeadOff!=MAD_FEEDBUFFER_SIZE ) {  /* lastHeadOff==MAD_FEEDBUFFER_SIZE as init */
			printf(DBG_RED"\nSound card lost? state=%d. try to reopen it...\n"DBG_RESET, snd_pcm_state(pcm_handle));
   #endif

			#if 1
			egi_close_gVolMixer(sndcard); /* after close pcm_handle?*/

			//snd_pcm_drain(pcm_handle); /* stuck, as it disconneted already! */
			//snd_pcm_drop(pcm_handle);  /* No effect,stops the PCM immediately. The pending samples on the buffer are ignored. */
			//snd_pcm_hw_free(pcm_handle); /* Remove PCM hardware configuration and free associated resources. even slower to restore.*/
			snd_pcm_close(pcm_handle);
			pcm_handle=NULL;

			/* Open pcm_handle */
	        	while( snd_pcm_open(&pcm_handle, sndcard, SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
                		printf(DBG_YELLOW"Try to find sound card...\n"DBG_RESET);
				sleep(1);
			}
			/* Init volume mixer */
			//vpercent=30;
			egi_getset_pcm_volume(sndcard, NULL, &vpercent); /* TODO vpercent to be 0 again! */

			#else /* !!!! snd_pcm_recover() NO use for USB unplug !!! */
			/* This functions handles -EINTR (interrupted system call), -EPIPE (overrun or underrun)
			 and -ESTRPIPE (stream is suspended) error codes trying to prepare given stream for next I/O. */
			while(snd_pcm_recover(pcm_handle, -ESTRPIPE, 0)!=0) {
                		printf(DBG_YELLOW"Try to find sound card...\n"DBG_RESET);
				sleep(1);
			}

			#endif

			/*  XXX NOPE, fixed. */
			//free(resampled); resampled=NULL;

			/* Reset pcmdev_ready */
			pcmdev_ready=false;
			refresh_display=true;
		}
		cnt =0;
		cntMs=0;
		cntSamples=0;
	}
#else	/* snd_pcm_state() CAN NOT detect virtual sound card unplugged! OK for plughw. */
	//if( snd_pcm_state(pcm_handle)==SND_PCM_STATE_DISCONNECTED ) {
	ret=snd_pcm_state(pcm_handle);
	if( ret!=SND_PCM_STATE_RUNNING && ret!=SND_PCM_STATE_PREPARED ) {
		printf(DBG_RED"\nstate=%d, Sound card lost? try to reopen it...\n"DBG_RESET,ret);

	}

#endif

   }  /* End if(pcmdev_ready) */

  return MAD_FLOW_CONTINUE;
}


/* Parse keyinput and frame header */
static enum mad_flow header(void *data,  struct mad_header const *header )
{
  char ch;
  int mad_ret=MAD_FLOW_CONTINUE;

  //  struct buffer *buffer = data;
  /* Count header frame */
  header_cnt++;
  frame_cnt++;

  /* If RingBuffer is overflow, skip decoding every 1/10 frame */
  //if(egi_ringbuffer_IsFull(ringbuff)) {
  if(ringbuff_almost_full && frame_cnt%10==0 ) {
	egi_dpstd("Drop frame...\n");
	refresh_display=true;
	return MAD_FLOW_IGNORE;
  }

  /* Parse keyinput...*/
  read(STDIN_FILENO, &ch,1);
  switch(ch) {
	case 'q': 	/* Quit */
	case 'Q':
		printf("\n\n");
		exit(0);
		break;
	case 'm':	/* Mute/Demute */
	case 'M':
		egi_pcm_mute(sndcard);
		break;
	case '+':	/* Volume up */
		#if MIXER_VOLUME_CONTROL
		egi_adjust_pcm_volume(sndcard, 2);
		egi_getset_pcm_volume(sndcard, &vpercent,NULL);
		printf(DBG_GREEN"\r Vol: %d%% "DBG_RESET, vpercent); fflush(stdout);
		#else
		//egi_adjust_ctl_elemValue(sndcard, "Speaker Playback Volume", 5);
		egi_getset_ctl_elemValue(sndcard, "Speaker Playback Volume", &vpercent, NULL, 5);
		printf("\r Vol: %d%%",vpercent>100?100:vpercent+5); fflush(stdout);
		#endif

		usleep(100000);
		refresh_display=true;
		break;
	case '-':	/* Volume down */
		#if MIXER_VOLUME_CONTROL
		egi_adjust_pcm_volume(sndcard, -4);
		egi_getset_pcm_volume(sndcard, &vpercent,NULL);
		printf(DBG_GREEN"\r Vol: %d%% "DBG_RESET, vpercent); fflush(stdout);
		#else
		//egi_adjust_ctl_elemValue(sndcard, "Speaker Playback Volume", -5);
		egi_getset_ctl_elemValue(sndcard, "Speaker Playback Volume", &vpercent, NULL, -5);
		printf("\r Vol: %d%%", vpercent-5 <0 ? 0:vpercent-5 ); fflush(stdout);
		#endif

		usleep(100000);
		refresh_display=true;
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
  if(!pcmparam_ready || refresh_display ) {
	if( header_cnt>2 ) {

#if 1		/* Display panel */
		printf("\n\n\n");
		printf(" ----------------------------\n");
		printf("      A miniStreamPlayer\n");
		printf(" ----------------------------\n");
		//printf(" Libcurl+Libmad+EGI\n");
  		printf(" MPEG Layer_%d\n %s chan=%d\n bitrate:    %.1fkbits\n samplerate: %dHz\n",
			header->layer, mad_mode_str[header->mode],nchannels,1.0*header->bitrate/1000, header->samplerate);
		printf("            * * *\n");
		printf("Icecast: %s\n", strStreamName);
		printf("Stream: %s\n", strStreamTitle);
		printf(" ----------------------------\n");
#endif
		pcmparam_ready=true;   /* Set */
		refresh_display=false; /* Reset */
	}
	else
		return MAD_FLOW_CONTINUE;
  }

   //printf(" Buffer data: %dBs\n", ringbuff->datasize);
  lapHour=timelapse/3600;
  lapMin=(timelapse-lapHour*3600)/60;
  lapfSec=timelapse-3600*lapHour-60*lapMin;
   //  printf("\r %02d:%02d:%.1f of [%02d:%02d:%.1f]   ", lapHour, lapMin, lapfSec, durHour, durMin, durfSec);
  printf("\r %02d:%02d:%.1f [Buffer: %.1fkBs]", lapHour, lapMin, lapfSec, 1.0*(ringbuff->datasize)/1024);
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

  fprintf(stderr, "\nDecodes[%lu] feed[%lu] frame_cnt=%lu: decoding error 0x%04x (%s) at byte offset %u.\n",
	  decode_cnt, feed_cnt,frame_cnt,
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);

  refresh_display=true;
  #endif

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
  //return MAD_FLOW_CONTINUE;
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

INIT_BUFFER:
  /* Init buffer ... */
  printf("Buffering stream...  \n");
  /* Note:
   * 1. Init buffer should NOT be the same as RINGBUFFER_SIZE,
   *    OR it conflicts with http_download_callback() size check!
   */
  while( ringbuff->datasize < INIT_BUFFER_SIZE ) {
	//if( ringbuff->datasize>0 && stream_type!=STREAM_AUDIO_MPEG) {
	if( ringbuff->datasize>0 && (stream_type!=STREAM_AUDIO_MPEG && stream_type !=STREAM_UNKNOWN) ) { // HK2023-02-23
		printf("Stream type %s, NOT supported yet!\n", strStreamType[stream_type]);
//		exit(1);
	}
	printf("\rBuffering %dBs ", ringbuff->datasize); fflush(stdout);

 	usleep(50000);
  };

	/* Loop feed data in input_data() */
	mad_decode((unsigned char const *)rawbuffer, MAD_FEEDBUFFER_SIZE);
	goto INIT_BUFFER;

	return (void *)0;
}
