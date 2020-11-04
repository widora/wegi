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


 ---------------------------------------------------------
 Modified for ALSA PCM playback for SND_PCM_FORMAT_S16_LE.
 ---------------------------------------------------------

 Usage:
	./minicad  /Music/*.mp3

 KeyInput command:
	'q'	Quit
	'n'	Next
	'p'	Prev
	'+'	Increase volume +5%
	'-'	Decrease volume -5%

 Midas Zhou
 midaszhou@yahoo.com
 -------------------------------------------------------------------------------


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
# include <stdio.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <egi_utils.h>
# include <egi_pcm.h>
# include <egi_input.h>
# include "mad.h"


/*
 * This is perhaps the simplest example use of the MAD high-level API.
 * Standard input is mapped into memory via mmap(), then the high-level API
 * is invoked with three callbacks: input, output, and error. The output
 * callback converts MAD's high-resolution PCM samples to 16 bits, then
 * writes them to standard output in little-endian, stereo-interleaved
 * format.
 */

static int decode(unsigned char const *, unsigned long);
static int16_t  *data_s16l=NULL;
static bool pcmdev_ready;

static int cmd_keyin;
enum {
	CMD_NONE=0,
	CMD_PREV,
	CMD_LOOP,
	CMD_PAUSE,
};

static unsigned long long duration_frac;
static unsigned int	  duration_sec;

static bool get_info;
static snd_pcm_t *pcm_handle;       /* for PCM playback */
snd_pcm_hw_params_t *pcm_params;



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

  /* Set termI/O */
  egi_set_termios();

  /* Prepare vol */
  egi_getset_pcm_volume(NULL,NULL);

  /* Open pcm captrue device */
  if( snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
  	printf("Fail to open PCM playback device!\n");
        return 1;
  }


/* Play all files */
for(i=0; i<files; i++) {

	/* Reset */
	duration_frac=0;
	duration_sec=0;
	get_info=false;
	pcmdev_ready=false;

	/* Command */
	switch(cmd_keyin) {
		case CMD_PREV:
			if(i==0) i=-1;
			else i-=2;
			break;
		case CMD_LOOP:
			break;

	}
	cmd_keyin=CMD_NONE;

  	/* Mmap input file */
	fmap=egi_fmap_create(argv[i+1]);
	if(fmap==NULL)
	return 1;

	/* Decoding ... */
  	printf(" Start playing...\n %s\n size=%lldk\n", argv[i+1], fmap->fsize>>10);
 	decode((const unsigned char *)fmap->fp, fmap->fsize);

  	/* Release source */
  	free(data_s16l); data_s16l=NULL;
  	egi_fmap_free(&fmap);
}

  /* Close pcm handle */
  snd_pcm_close(pcm_handle);

  /* Reset termI/O */
  egi_reset_termios();

  return 0;
}

/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
  unsigned char const *start;
  unsigned long length;
};

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input(void *data,
		    struct mad_stream *stream)
{
  struct buffer *buffer = data;

  if (!buffer->length)
    return MAD_FLOW_STOP;

  //printf("mad stream buffer...\n");
  /* set stream buffer pointers, pass user data to stream ... */

  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline
signed int scale(mad_fixed_t sample)
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

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */

static
enum mad_flow output(void *data,
		     struct mad_header const *header,
		     struct mad_pcm *pcm)
{
  unsigned int i,k;
  char ch;
  unsigned int nchannels, nsamples, samplerate;
  mad_fixed_t const *left_ch, *right_ch;
  int percent;

  /* Check input cmd */
  ch=0;
  read(STDIN_FILENO, &ch,1);
  switch(ch) {
	case 'q':	/* Quit */
		printf("\n\n");
		exit(0);
		break;
	case 'n':	/* Next */
		printf("\n\n");
	  	return MAD_FLOW_STOP;
	case 'p':	/* Next */
		printf("\n\n");
		cmd_keyin=CMD_PREV;
	  	return MAD_FLOW_STOP;
	case '+':
		egi_adjust_pcm_volume(5);
		egi_getset_pcm_volume(&percent,NULL);
		printf("\nVol: %d%%\n",percent);
		break;
	case '-':
		egi_adjust_pcm_volume(-5);
		egi_getset_pcm_volume(&percent,NULL);
		printf("\nVol: %d%%\n",percent);
		break;
  }

  /* Print MP3 file info */
  if(!get_info) {
	printf(" ----------------------\n");
	printf(" Simple is the Best!\n");
	printf(" miniMAD +Libmad +EGI\n");
  	printf(" Layer[%d] Mode[%d]\n bitrate:    %.1fkbits\n samplerate: %dHz\n",
			header->layer, header->mode, 1.0*header->bitrate/1000, header->samplerate);
	printf(" ----------------------\n");
	get_info=true;
  }
  duration_frac += header->duration.fraction;
  duration_sec  += header->duration.seconds;
  printf("\r %.1f seconds", (1.0*duration_frac/MAD_TIMER_RESOLUTION) + duration_sec);
  fflush(stdout);

  /* pcm->samplerate contains the sampling frequency */
  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];
  samplerate= pcm->samplerate;

  //printf("nchanls=%d, sample_rate=%d\n", nchannels, samplerate);
  /* Sep parameters for pcm_hanle */
  if(!pcmdev_ready) {
	//printf("nchanls=%d, sample_rate=%d\n", nchannels, samplerate);
	if( egi_pcmhnd_setParams(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, nchannels, samplerate) <0 ) {
                printf("Fail to set params for PCM playback handle.!\n");
	} else {
		//printf("EGI set pcm params successfully!\n");
		pcmdev_ready=true;
	}
  }

   /* Realloc data for S16 */
   data_s16l=(int16_t *)realloc(data_s16l, nchannels*nsamples*2);
   if(data_s16l==NULL)
	exit(1);

   /* scale */
   for(k=0,i=0; i<nsamples; i++) {
	data_s16l[k]=scale(*(left_ch+i));		/* Left */
	k++;
	if(nchannels==2) {
		data_s16l[k]=scale(*(right_ch+i));	/* Right */
		k++;
	}
   }

   /* playback */
   if(pcmdev_ready)
	    egi_pcmhnd_playBuff(pcm_handle, true, (void *)data_s16l, nsamples);

  return MAD_FLOW_CONTINUE;
}

/*
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
  struct buffer *buffer = data;

  #if 0
  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);
  #endif
  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

  return MAD_FLOW_CONTINUE;
}

/*
 * This is the function called by main() above to perform all the decoding.
 * It instantiates a decoder object and configures it with the input,
 * output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */

static
int decode(unsigned char const *start, unsigned long length)
{
  struct buffer buffer;
  struct mad_decoder decoder;
  int result;

  /* initialize our private message structure */
  buffer.start  = start;
  buffer.length = length;

  /* configure input, output, and error functions */
  //printf("decoder init...\n");
  mad_decoder_init(&decoder, &buffer,
		   input, 0 /* header */, 0 /* filter */, output,
		   error, 0 /* message */);

  /* start decoding */
  //printf("mad decoder run...\n");
  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  /* release the decoder */
  mad_decoder_finish(&decoder);

  return result;
}
