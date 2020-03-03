/*-------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. Record sound and encode it to mp3 by shine. use Ctrl+C to interrupt.
2. Usage: recmp3 save_path


			<<   Glossary  >>

Sample:		A digital value representing a sound amplitude,
		sample data width usually to be 8bits or 16bits.
Channels: 	1-Mono. 2-Stereo, ...
Frame_Size:	Sizeof(one sample)*channels, in bytes.
Rate: 		Frames per second played/captured. in HZ.
Data_Rate:	Frame_size * Rate, usually in kBytes/s(kb/s), or kbits/s.
Period:		Period of time that a hard ware spends in processing one pass data/
		certain numbers of frames, as capable of the HW.
Period Size:	Representing Max. frame numbers a hard ware can be handled for each
		write/read(playe/capture), in HZ, NOT in bytes??
		(different value for PLAYBACK and CAPTURE!!)
Running show:   1536 frames for CAPTURE; 278 frames for PLAYBACK, depends on HW?
Chunk(period):	frames from snd_pcm_readi() each time for shine enconder.
Buffer: 	Usually in N*periods

Interleaved mode:	record period data frame by frame, such as
	 		frame1(Left sample,Right sample), frame2(), ......
Uninterleaved mode:	record period data channel by channel, such as
			period(Left sample,Left ,left...), period(right,right...),
			period()...
snd_pcm_uframes_t	unsigned long
snd_pcm_sframes_t	signed long


Midas Zhou
----------------------------------------------------------------------------------*/
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <shine/layer3.h>
#include <signal.h>
//#include "pcm2wav.h"


bool sig_stop=false;

void new_sighandler(int signum, siginfo_t *info, void *myact);
int init_shine_mono(shine_t *pshine, shine_config_t *psh_config, int sample_rate,int bitrate);


int main(int argc, char** argv)
{
	int i;
	int ret;

	/* for pcm capture */
	snd_pcm_t *pcm_handle;
	int nchanl=1;	/* number of channles */
	int format=SND_PCM_FORMAT_S16_LE;
	int bits_per_sample=16;
	int frame_size=2;		/*bytes per frame, for 1 channel, format S16_LE */
	int sample_rate=8000; 		/* HZ,sample rate */
	bool enable_resample=true;	/* whether to enable resample */
	int latency=500000;		/* required overall latency in us */
	//snd_pcm_uframes_t period_size;  /* max numbers of frames that HW can hanle each time */

	/* for CAPTURE, period_size=1536 frames, while for PLAYBACK: 278 frames */
	snd_pcm_uframes_t chunk_frames=32; /*in frame, expected frames for readi/writei each time, to be modified by ... */
	int chunk_size;			/* =frame_size*chunk_frames */
	int record_size=64*1024;	/* total pcm raw size for recording/encoding */

	/* chunk_frames=576, */
	int16_t buff[SHINE_MAX_SAMPLES]; /* enough size of buff to hold pcm raw data AND encoding results */
	int16_t *pbuf=buff;
	unsigned long count;

	/* for shine encoder */
	shine_t	sh_shine;		/* handle to shine encoder */
	shine_config_t	sh_config; 	/* config structure for shine */
	int sh_bitrate=16; 		/* kbit */
	int ssamples_per_chanl; /* expected shine_samples (16bit depth) per channel to feed to the shine encoder each session */
//	int ssamples_per_pass;	/* nchanl*ssamples_per_chanl, for each encoding pass/session */
	unsigned char *mp3_pout; /* pointer to mp3 data after each encoding pass */
	int mp3_count;		/* byte counter for shine mp3 output per pass */

	FILE *fmp3;

	struct sigaction sigact;

	/* set signal handler for ctrl+c */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags=SA_SIGINFO;
	sigact.sa_sigaction=new_sighandler;
	if(sigaction(SIGINT,&sigact, NULL) <0 ){
		perror("Set sigation error");
		return -1;
	}

	/* check input param */
	if(argc<2) {
		printf("Please input mp3 file path for saving.\n");
		return -1;
	}


	/* open pcm captrue device */
	if( snd_pcm_open(&pcm_handle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0) <0 ) {
		printf("Fail to open pcm captrue device!\n");
		return -1;
	}

	/* set params for pcm capture handle */
	if( snd_pcm_set_params( pcm_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED,
				nchanl, sample_rate, enable_resample, latency )  <0 ) {
		printf("Fail to set params for pcm capture handle.!\n");
		snd_pcm_close(pcm_handle);
		return -2;
	}

	/* open file for mp3 input */
	fmp3=fopen(argv[1],"wb");
	if( fmp3==NULL ) {
		printf("Fail to open file for mp3 input.\n");
		snd_pcm_close(pcm_handle);
		return -3;
	}


	/* adjust record volume */
	system("amixer -D hw:0 set Capture 90%");
	system("amixer -D hw:0 set 'ADC PCM' 85%");

	/* init shine */
	if( init_shine_mono(&sh_shine, &sh_config, sample_rate, sh_bitrate) <0 ) {
		snd_pcm_close(pcm_handle);
		return -4;
	}

	/* get number of ssamples per channel, to feed to the encoder */
	ssamples_per_chanl=shine_samples_per_pass(sh_shine); /* return audio ssamples expected  */
	printf("%s: MONO ssamples_per_chanl=%d frames for Shine encoder.\n",__func__, ssamples_per_chanl);
	/* change to frames for snd_pcm_readi() */
	chunk_frames=ssamples_per_chanl*16/bits_per_sample*nchanl; /* 16bits for a shine input sample */
	chunk_size=chunk_frames*frame_size;
	printf("Expected pcm readi chunk_frames for the shine encoder is %ld frames per pass.\n",chunk_frames);
	printf("SHINE_MAX_SAMPLES = %d \n",SHINE_MAX_SAMPLES);
	printf("Shine bitrate:%d kbps\n",sh_config.mpeg.bitr);


	/* capture pcm */
	count=0;
	printf("Start recording and encoding ...\n");
	while(1) /* let user to interrupt, or if(count<record_size) */
	{
		/* snd_pcm_sframes_t snd_pcm_readi (snd_pcm_t pcm, void buffer, snd_pcm_uframes_t size) */
		/* return number of frames, 1 chan, 1 frame=size of a sample (S16_Le)  */
		ret=snd_pcm_readi(pcm_handle, buff, chunk_frames);
		if(ret == -EPIPE ) {
			/* EPIPE for overrun */
			printf("Overrun occurs during capture, try to recover...\n");
			/* try to recover, to put the stream in PREPARED state so it can start again next time */
			snd_pcm_prepare(pcm_handle);
			continue;
		}
		else if(ret<0) {
			printf("snd_pcm_readi error: %s\n",snd_strerror(ret));
			continue; /* carry on anyway */
		}
		/* CAUTION: short read may cause trouble! Let it carry on though. */
		else if(ret != chunk_frames) {
			printf("snd_pcm_readi: read end or short read ocuurs! get only %d of %ld expected frame data.\n",
										ret, chunk_frames);
			/* pad 0 to chunk_frames */
			if( ret<chunk_frames ) {  /* >chunk_frames NOT possible? */
				memset( buff+ret*frame_size, 0, (chunk_frames-ret)*frame_size);
			}
		}

		/* pcm raw data count */
		count += chunk_size;

                 /* 	------	Shine Start -----
		  * unsigned char* shine_encode_buffer(shine_t s, int16_t **data, int *written);
                  * unsigned char* shine_encode_buffer_interleaved(shinet_t s, int16_t *data, int *written);
                  * ONLY 16bit depth sample is accepted by shine_encoder
                  * chanl_samples_per_pass*chanl_val samples encoded
		  */

                  mp3_pout=shine_encode_buffer(sh_shine, &pbuf, &mp3_count);

		/* 	----- Shine End -----  	*/

		 /* write to mp3 file */
		 fwrite(mp3_pout, mp3_count, sizeof(unsigned char), fmp3);

		 /* check signal */
	         if(sig_stop) {
			printf("User interrupt.\n");
			break;
		}

	} /* end of capture while() */

	/* flush out remain mp3 */
	mp3_pout=shine_flush(sh_shine,&mp3_count);

	/* encode more chunks to sync for useful data */
	memset(buff,0, chunk_size);
	for(i=0;i<32;i++) {
	        mp3_pout=shine_encode_buffer(sh_shine, &pbuf, &mp3_count);
		fwrite(mp3_pout, mp3_count, sizeof(unsigned char), fmp3);
	}

	printf(" %ld bytes raw pcm data recorded and enconded, plus %d more bytes for sync.\n",
										count, 32*chunk_size);

	/* close files and release soruces */
	fclose(fmp3);
	snd_pcm_close(pcm_handle);
	shine_close(sh_shine);

	return 0;
}




/*---------------------------------------------
Initiliaze shine for mono channel
@sample_rate:	input wav sample rate
@bitrate:	bitrate for mp3 file
Return:
	0	oK
	<0 	fails
--------------------------------------------*/
int init_shine_mono(shine_t *pshine, shine_config_t *psh_config, int sample_rate,int bitrate)
{
	int samplerate_index;
	int mpegver_index; 	    /* mpeg verion index */
	int bitrate_index;	    /* bitrate index */

	samplerate_index=shine_find_samplerate_index(sample_rate);
	if(samplerate_index < 0) {
		printf("Index for sample rate %dHZ is NOT available.\n",sample_rate);
		return -1;
	}

	mpegver_index=shine_mpeg_version(samplerate_index);
	if(mpegver_index < 0) {
		printf("Mpeg version Index for sample rate %dHZ is NOT available.\n",samplerate_index);
		return -1;
	}

	/* check whether bitrate is available */
	bitrate_index=shine_find_bitrate_index(bitrate,mpegver_index);
	if(bitrate_index < 0) {
		printf("Bit rate %dk is NOT supported for mpeg verions index=%d.\n",bitrate, mpegver_index);
		return -1;
	}

	printf("Sample rate index:%d,	Mpeg Version index:%d,	Bit rate index:%d.\n",
								samplerate_index,mpegver_index,bitrate_index);

	shine_set_config_mpeg_defaults(&(psh_config->mpeg));
	psh_config->wave.channels=PCM_MONO;/* PCM_STEREO=2,PCM_MONO=1 */
	/*valid samplerates: 44100,48000,32000,22050,24000,16000,11025,12000,8000*/
	psh_config->wave.samplerate=sample_rate;
	psh_config->mpeg.copyright=1;
	psh_config->mpeg.original=1;
	psh_config->mpeg.mode=MONO;/* STEREO=0,MONO=3 */
	psh_config->mpeg.bitr=bitrate;

	if(shine_check_config(psh_config->wave.samplerate,psh_config->mpeg.bitr)<0) {
        	printf("%s: unsupported sample rate and bit rate configuration.\n",__func__);
	}

	*pshine=shine_initialise(psh_config);
	if(*pshine == NULL){
        	printf("%s: Fail to initialize Shine!\n",__func__);
	        return -1;
	 }

	return 0;
}

/*---------------------------
signal SIGINT handler
---------------------------*/
void new_sighandler(int signum, siginfo_t *info, void *myact)
{
	sig_stop=true;
}
