/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
1. Record sound and encode it to mp3 by shine. use Ctrl+C to interrupt.
2. Usage example:
			<--- LETS_NOTE --->
	./recmp3 -n 2 -r 44100 -b 64 -l 20000 xxx.mp3

				<--- USB SndCard --->
	./recmp3 -w plughw:1,0 -p -r 44100 -b 128 -c 128 -n 2 xxx.mp3  ---OK
	./recmp3 -w plughw:1,0 -n 1 -r 44100 -b 64 -l 20000 xxx.mp3

	./recmp3 -w plughw:1,0 -n 1 -r 16000 -b 32 -l 300 xxx.mp3
	./recmp3 -w plughw:1,0 -n 2 -r 16000 -b 64 -c 256 -l 100000 xxx.mp3

	./recmp3 -w plughw:1,0 -n 1 -r 16000 -b 32 -c 128 -l 10000 xxx.mp3	GOOD
	./recmp3 -w plughw:1,0 -n 1 -r 24000 -b 32 -c 128 -l 10000 xxx.mp3	GOOD
	./recmp3 -w plughw:1,0 -n 1 -r 32000 -b 64 -c 256 -l 10000 xxx.mp3	GOOD
	./recmp3 -w plughw:1,0 -n 1 -r 44100 -b 128 -c 256 -l 20000 xxx.mp3	GOOD
	./recmp3 -w plughw:1,0 -n 1 -r 48000 -b 128 -c 256 -l 10000 xxx.mp3	GOOD

         NOTE:
		1. For USB Mic, Big latency will cause overrun / short read! /
		   snd_pcm_readi error: Input/output error ...
		   Take latency=10000us seems OK.
		2. Take big chunk_frames: 64-256.
		3. USB MIC produces litte noise, comparing with the builtin sound card.
	 	4. ???If you set 2 channels for recording while the HW has only 1 channel,
   		   it makes lots of noise though it works! )
		   and, the Max. sample rate will be ONLY half of ALSA mixer srate. ---- NOPE! ok

			<--- NEO Builtin SndCard --->

	./recmp3 -n 1 -r 8000 -b 16 -l 100 xxx.mp3  	(NEO)	GOOD
		AUDIO: 8000 Hz, 1 ch, s16le, 16.0 kbit/12.50% (ratio: 2000->16000)

	./recmp3 -n 1 -r 12000 -b 16 xxx.mp3	    	(NEO)	well
		AUDIO: 12000 Hz, 1 ch, s16le, 16.0 kbit/8.33% (ratio: 2000->24000)

	./recmp3 -n 1 -r 16000 -b 32 xxx.mp3  	    	(NEO)	noisy
		AUDIO: 16000 Hz, 1 ch, s16le, 32.0 kbit/12.50% (ratio: 4000->32000)

	./recmp3 -n 1 -r 24000 -b 32 xxx.mp3	    	(NEO)	GOOD
		AUDIO: 24000 Hz, 1 ch, s16le, 32.0 kbit/8.33% (ratio: 4000->48000)

	./recmp3 -n 1 -r 32000 -b 64 xxx.mp3 		(NEO)	well
		AUDIO: 32000 Hz, 1 ch, s16le, 64.0 kbit/12.50% (ratio: 8000->64000)

	./recmp3 -n 1 -r 44100 -b 128 xxx.mp3 		(NEO)	noisy, chacha...
		AUDIO: 44100 Hz, 1 ch, s16le, 128.0 kbit/18.14% (ratio: 16000->88200)

	./recmp3 -n 1 -r 48000 -b 320 xxx.mp3 		(NEO)	GOOD
		AUDIO: 48000 Hz, 1 ch, s16le, 320.0 kbit/41.67% (ratio: 40000->96000)
   NOTE:
 	If you set 2 channels for recording while the HW has only 1 channel,
   	it makes lots of noise though it works! )
	Big bit_rate for big sample rate (and Max. latency is smaller).

3. For SND_PCM_FORMAT_S16_LE only.
4. Adjust asound.conf accordingly, and test your MIC first.
   Example:
	arecord -D plughw:'Camera' -f dat xxx.wav

   /////////////  Example: /etc/asound.conf  (WidoraNEO) //////////////
	defaults.ctl.card 0
	pcm.!default{
		type plug
		slave.pcm "asymed"
	}
	pcm.asymed {
		type asym
		playback.pcm  "mymixer"
		capture.pcm   "hw:0,0"
	}
	pcm.mymixer{
		type dmix
		ipc_key 1024
		slave {
			pcm "hw:0,0"
			format S16_LE
			period_time 0
			period_size 1024
			buffer_size 8192
			rate 48000
		}
		bindings {
			0 0
			1 1
		}
	}
   /////////////////////////////////////////////////////////////////////

5. (Obsolete) For USB MIC, adjust latency to a rather smaller value.

TODO:
1. Get actual number of capture channels.
2. Config 2 channels for Shine.

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

	---  Shine Sturcs ---
typedef struct {
  shine_wave_t wave;
  shine_mpeg_t mpeg;
} shine_config_t;

typedef struct {
    enum channels channels;
    int           samplerate;
} shine_wave_t;

typedef struct {
    enum modes mode;      // Stereo mode
    int        bitr;      // Must conform to known bitrate
    enum emph  emph;      // De-emphasis
    int        copyright;
    int        original;
} shine_mpeg_t;


 Tables of supported audio parameters & format.

 Valid samplerates and bitrates.
 const int samplerates[9] = {
   44100, 48000, 32000, // MPEG-I
   22050, 24000, 16000, // MPEG-II
   11025, 12000, 8000   // MPEG-2.5
 };

 const int bitrates[16][4] = {
 //  MPEG version:
 //  2.5, reserved,  II,  I
   { -1,  -1,        -1,  -1},
   { 8,   -1,         8,  32},
   { 16,  -1,        16,  40},
   { 24,  -1,        24,  48},
   { 32,  -1,        32,  56},
   { 40,  -1,        40,  64},
   { 48,  -1,        48,  80},
   { 56,  -1,        56,  96},
   { 64,  -1,        64, 112},
   { 80,  -1,        80, 128},
   { 96,  -1,        96, 160},
   {112,  -1,       112, 192},
   {128,  -1,       128, 224},
   {144,  -1,       144, 256},
   {160,  -1,       160, 320},
   { -1,  -1,        -1,  -1}
  };


Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------*/
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <shine/layer3.h>
#include <signal.h>
#include <egi_pcm.h>
//#include "pcm2wav.h"


bool sig_stop=false;
void new_sighandler(int signum, siginfo_t *info, void *myact);
int init_shine(shine_t *pshine, shine_config_t *psh_config, int nchanl, int sample_rate,int bitrate);


/*---------------------------
        Print help
-----------------------------*/
void print_help(const char* cmd)
{
        printf("Usage: %s [-hpw:r:b:c:l:n:]  fmp3_name\n", cmd);
        printf("	-h   This help \n");
	printf("	-p   Enable resample, default: false.\n");
        printf("	-w:  Name of alsa capture HW, default: 'plughw:0,0'. \n");
        printf("	-r:  HZ, Input sample rate. default: 8000. \n");
        printf("	-b:  Kbits, Shine bit rate. default: 16kbits. \n");
        printf("	-c:  Chunk frames. default: 32. \n");
	printf("	-l:  (Obsolete) us, latency. default 500000us\n");
	printf("	-n:  number of channels, 1-MONO, 2-STEREO. default 1.\n");
}


/*----------------------------
	     MAIN
-----------------------------*/
int main(int argc, char** argv)
{
	int ret;
	int opt;
	const char *strHW="plughw:0,0";

	/* For PCM capture */
	snd_pcm_t *pcm_handle	=NULL;
unsigned int 	nchanl		=1;		/* number of channles, 1 MONO, 2 STEREO */
	int 	pcm_format	=SND_PCM_FORMAT_S16_LE;
	int 	bits_per_sample	=16;
	int 	frame_size	=2;		/* bytes per frame, for 1 channel, format S16_LE */
unsigned int 	sample_rate	=8000; 		/* HZ,sample rate */
	bool	enable_resample	=false;		/* whether to enable resample */
	int 	latency		=500000;	/* required overall latency in us */
	//snd_pcm_uframes_t period_size;  	/* max numbers of frames that HW can hanle each time */

	/* For CAPTURE, period_size=1536 frames, while for PLAYBACK: 278 frames */
	snd_pcm_uframes_t chunk_frames=32; 	/* in frame, expected frames for readi/writei each time, to be modified by ... */
	int 	chunk_size;			/* =frame_size*chunk_frames */
	//int 	record_size=64*1024;		/* total pcm raw size for recording/encoding */

	/* buffer for chunk size raw data, Max. 2 channels  */
	int16_t buff[SHINE_MAX_SAMPLES*2]; 	/* enough size of buff to hold pcm raw data AND encoding results */
	int16_t *pbuf=buff;
	unsigned long 	count;

	/* For shine encoder */
	shine_t		sh_shine;		/* handle to shine encoder */
	shine_config_t	sh_config; 		/* config structure for shine */
	int 		sh_bitrate=16; 		/* kbit */
	int 		ssamples_per_chanl; 	/* expected shine_samples (16bit depth) per channel to feed to the shine encoder each session */
	//int 		ssamples_per_pass;	/* nchanl*ssamples_per_chanl, for each encoding pass/session */
	unsigned char 	*mp3_pout; 		/* pointer to mp3 data after each encoding pass */
	int 		mp3_count;		/* byte counter for shine mp3 output per pass */
	int 		nwrite;

	const char *fpath;
	FILE *fmp3;
	struct sigaction sigact;

	/* Set signal handler for ctrl+c */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags=SA_SIGINFO;
	sigact.sa_sigaction=new_sighandler;
	if(sigaction(SIGINT,&sigact, NULL) <0 ){
		perror("Set sigation error");
		return -1;
	}

	/* Check input param */
	if(argc<2) {
		print_help(argv[0]);
		return -1;
	}

        /* Parse options */
        while( (opt=getopt(argc,argv,"hpw:r:b:c:l:n:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
			case 'p':
				enable_resample=true;
				break;
                        case 'w':
                                strHW=optarg;
                                break;
                        case 'r':
                                sample_rate=atoi(optarg);
                                break;
			case 'b':
				sh_bitrate=atoi(optarg);
				break;
			case 'c':
				chunk_frames=(snd_pcm_uframes_t)atoi(optarg);
				break;
			case 'l':
				latency=atoi(optarg);
				break;
			case 'n':
				nchanl=atoi(optarg);
				if(nchanl<1 || nchanl>2)
					nchanl=1;
				/* update frame_size */
				frame_size=2*nchanl;
				break;
		}
	}

        /* Check output mp3 file name  */
        if( optind < argc ) {
		fpath=argv[optind++];
        } else {
                printf("No input file!\n");
		print_help(argv[0]);
		return -1;
        }

	/* Open pcm captrue device */
	if( snd_pcm_open(&pcm_handle, strHW, SND_PCM_STREAM_CAPTURE, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
		printf("Fail to open pcm capture device!\n");
		return -1;
	}
	printf("Capture device '%s' opened successfully!\n",strHW);

	/* Set params for pcm capture handle */

#if 1 ///////////////////////
      #if 1  /* --- GOOD! No latency needed! --- */
        if( egi_pcmhnd_setParams( pcm_handle, pcm_format, SND_PCM_ACCESS_RW_INTERLEAVED, nchanl, sample_rate) <0 ) {
		printf("Fail to set params for pcm capture handle.!\n");
		snd_pcm_close(pcm_handle);
		return -2;
	}
      #else  /* OBSOLETE */
	if( snd_pcm_set_params( pcm_handle, pcm_format, SND_PCM_ACCESS_RW_INTERLEAVED, // SND_PCM_ACCESS_RW_NONINTERLEAVED,
				nchanl, sample_rate, enable_resample, latency )  <0 ) {
		printf("Fail to set params for pcm capture handle.!\n");
		snd_pcm_close(pcm_handle);
		return -2;
	}
     #endif
#else //////////////////////////
	snd_pcm_hw_params_t *params;
	int dir=0;
	int err;
	snd_pcm_uframes_t frames;
	unsigned int chmax, chmin;

        /* allocate a hardware parameters object */
        snd_pcm_hw_params_alloca(&params);
        /* fill it in with default values */
        snd_pcm_hw_params_any(pcm_handle, params);
	/* TODO: Get actual number of Capture channels */
	snd_pcm_hw_params_get_channels_min(params, &chmin);
	snd_pcm_hw_params_get_channels_max(params, &chmax);
	printf("chmin=%d, chmax=%d\n",chmin, chmax);
	err=snd_pcm_hw_params_get_channels(params, &nchanl);
	if(err!=0) {
                printf("Unable to get channels: Err'%s'.\n", snd_strerror(err));
		return -3;
	} else
		printf("System default nchanl=%d\n", nchanl);

        /* set access type  */
        snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        /* HW signed 16-bit little-endian format */
        snd_pcm_hw_params_set_format(pcm_handle, params, pcm_format);
        /* set channel  */
        err=snd_pcm_hw_params_set_channels(pcm_handle, params, nchanl);
	if(err!=0) {
                printf("Unable to set hw parameter: Err'%s'.\n", snd_strerror(err));
                snd_pcm_close(pcm_handle);
                return -3;
	}
        /* sampling rate */
        snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, &dir);
        if(dir != 0)
                printf("Actual sampling rate is set to %d HZ!\n", sample_rate);

        /* set HW params */
        if ( (err=snd_pcm_hw_params(pcm_handle,params)) != 0 ) {
                printf("Unable to set hw parameter: Err'%s'.\n", snd_strerror(err));
                snd_pcm_close(pcm_handle);
                return -3;
        }
        /* get period size */
        snd_pcm_hw_params_get_period_size(params, &frames, &dir);
        printf("snd pcm period size is %d frames.\n", (int)frames);

#endif /////////////////////////////

	/* Open file for mp3 saving */
	fmp3=fopen(fpath,"wb");
	if( fmp3==NULL ) {
		printf("Fail to open file '%s' for mp3 input.\n", fpath);
		snd_pcm_close(pcm_handle);
		return -3;
	}

	/* Adjust record volume */
	system("amixer -D hw:0 set Capture 90% >/dev/null");
	system("amixer -D hw:0 set 'ADC PCM' 90% >/dev/null");

	/* Init shine */
	if( init_shine(&sh_shine, &sh_config, nchanl, sample_rate, sh_bitrate) <0 ) {
		snd_pcm_close(pcm_handle);
		return -4;
	}

	/* Get number of ssamples per channel, to feed to the encoder per pass */
	ssamples_per_chanl=shine_samples_per_pass(sh_shine); /* return audio ssamples expected  */
	printf("nchanl=%d, ssamples_per_chanl=%d frames for Shine encoder.\n",nchanl, ssamples_per_chanl);

	/* change to frames for snd_pcm_readi() */
	//chunk_frames=ssamples_per_chanl*(16/bits_per_sample)*nchanl; /* 16bits for a shine input sample */
	chunk_frames=ssamples_per_chanl;
	chunk_size=chunk_frames*frame_size;
	printf("Expected pcm readi chunk_frames for the shine encoder is %ld frames per pass.\n",chunk_frames);
	printf("chunck_size=chunk_frames*frame_size=%d*%d=%d\n", (int)chunk_frames, frame_size, (int)chunk_frames*frame_size);
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
			printf("Overrun occurs during capture, try to recover. Maybe latency it too small...\n");
			/* try to recover, to put the stream in PREPARED state so it can start again next time */
			snd_pcm_prepare(pcm_handle);
			continue;
		}
		else if(ret<0) {
			printf("snd_pcm_readi error: %s\n",snd_strerror(ret));
			continue; /* carry on anyway */
		}
		/* CAUTION: short read may cause trouble! Let it carry on though. */
		else if(ret != chunk_frames ) {
			printf("snd_pcm_readi: read end or short read occurs! get only %d of %ld expected frame data.\n",
										ret, chunk_frames);
			#if 0 /* pad 0 to chunk_frames */
			if( ret<chunk_frames ) {  /* >chunk_frames NOT possible? */
				memset( buff+ret*frame_size, 0, (chunk_frames-ret)*frame_size);
			}
			#endif
		}

		/* pcm raw data count */
		count += chunk_size;

                /* 	------	Shine Start -----
		 * unsigned char* shine_encode_buffer(shine_t s, int16_t **data, int *written);
                 * unsigned char* shine_encode_buffer_interleaved(shinet_t s, int16_t *data, int *written);
                 * ONLY 16bit depth sample is accepted by shine_encoder
                 * chanl_samples_per_pass*chanl_val samples encoded
		 */
		if(nchanl==1)
                	mp3_pout=shine_encode_buffer(sh_shine, &pbuf, &mp3_count);
		else //nchanl==2
                	mp3_pout=shine_encode_buffer_interleaved(sh_shine, pbuf, &mp3_count);

		/* 	----- Shine End -----  	*/

		/* write to mp3 file */
		//nwrite=fwrite(mp3_pout, mp3_count, sizeof(unsigned char), fmp3);
		nwrite=fwrite(mp3_pout, sizeof(unsigned char), mp3_count, fmp3);
		if(nwrite != mp3_count) {
			printf("Short write occurs when fwrite mp3 data to file!\n");
		}

		/* check signal */
	        if(sig_stop) {
			printf("User interrupt.\n");
			break;
		}

	} /* end of capture while() */

	/* flush out remain mp3 and write to mp3 file */
	mp3_pout=shine_flush(sh_shine, &mp3_count);
	nwrite=fwrite(mp3_pout, sizeof(unsigned char), mp3_count, fmp3);
	if(nwrite != mp3_count) {
		printf("Short write occurs when fwrite mp3 data to file!\n");
	}

	printf(" %ld bytes raw pcm data recorded and enconded\n",count);

#if 0	/* encode more chunks to sync for useful data */
	int i;
	memset(buff,0, chunk_size);
	for(i=0;i<32;i++) {
	        mp3_pout=shine_encode_buffer(sh_shine, &pbuf, &mp3_count);
		fwrite(mp3_pout, mp3_count, sizeof(unsigned char), fmp3);
	}

	printf("Add additional  %d more bytes for sync.\n", 32*chunk_size);
#endif

	/* close files and release soruces */
printf("fclose(fmp3)...\n");
	fclose(fmp3);
printf("snd_pcm_close(pmc_handle)...\n");
	snd_pcm_close(pcm_handle);
printf("shine_close(sh_shine)...\n");
	shine_close(sh_shine);

	return 0;
}


/*-------------------------------------------------
Initiliaze shine for mono channel

@pshine:	shine encoder handler
@psh_config:	shine config
@nchanl:	number of channels, 1-MONO,2-STERO
@sample_rate:	input wav sample rate
@bitrate:	bitrate for mp3 file
Return:
	0	oK
	<0 	fails
-------------------------------------------------*/
int init_shine(shine_t *pshine, shine_config_t *psh_config, int nchanl, int sample_rate,int bitrate)
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
		printf("Mpeg version index for sample rate %dHZ is NOT available.\n",samplerate_index);
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
	psh_config->wave.channels=nchanl; //PCM_MONO;/* PCM_STEREO=2,PCM_MONO=1 */
	/*valid samplerates: 44100,48000,32000,22050,24000,16000,11025,12000,8000*/
	psh_config->wave.samplerate=sample_rate;
	psh_config->mpeg.copyright=1;
	psh_config->mpeg.original=1;
	psh_config->mpeg.mode=(nchanl==1?MONO:STEREO);		/* shine: STEREO=0, MONO=3 */
	psh_config->mpeg.bitr=bitrate;

	if(shine_check_config(psh_config->wave.samplerate,psh_config->mpeg.bitr)<0) {
        	printf("%s: unsupported sample rate and bit rate configuration.\n",__func__);
		//Go on... return -1;
	} else
		printf("%s: Shine_check_config Ok!\n",__func__);

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
