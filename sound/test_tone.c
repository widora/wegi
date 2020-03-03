/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of generating pure sine wave sound data, in format of S16.
sample rate set to be 44.1KHz.

Note:
1. Assume left_shifting and right_shifting are both arithmatical!

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"

int main(void)
{
	int ret;
	int i,j,k;

        /* for pcm PLAYBACK */
        snd_pcm_t *play_handle;
        int nchanl=1;   			/* number of channles */
        int format=SND_PCM_FORMAT_S16_LE;
        int bits_per_sample=16;
        int frame_size=2;               	/*bytes per frame, for 1 channel, format S16_LE */
        int srate=44100;           		/* HZ,  sample rate for capture */
        bool enable_resample=true;      	/* whether to enable resample */
        int latency=100000; //500000;             	/* required overall latency in us */
        //snd_pcm_uframes_t period_size;  	/* max numbers of frames that HW can hanle each time */
        snd_pcm_uframes_t chunk_frames=1024; 	/*in frame, expected frames for readi/writei each time */
        int chunk_size;                 	/* =frame_size*chunk_frames */
	int16_t buff[1024*1];			/* chunk_frames, int16_t for S16 */
	int16_t Mamp=(1U<<15)-1;		/* Max. amplitude for format S16 */
	int32_t fp16_sin[1024];			/* Fix point sine value lookup table */

        /* open pcm play device */
        if( snd_pcm_open(&play_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {
                printf("Fail to open pcm play device!\n");
                return -1;
        }

        /* set params for pcm play handle */
        if( snd_pcm_set_params( play_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                                nchanl, srate, enable_resample, latency )  <0 ) {
                printf("Fail to set params for pcm play handle.!\n");
                snd_pcm_close(play_handle);
                return -2;
        }

        /* adjust record volume, or to call ALSA API */
        system("amixer -D hw:0 set Capture 85%");
        system("amixer -D hw:0 set 'ADC PCM' 85%");

	/* generate fixed sine value, 1024 points in 2*PI */
	printf("Generate fp16_sin[]...");
	for(k=0; k<1024; k++)
		fp16_sin[k]=sin(2*MATH_PI/1024*k + MATH_PI/2)*(1<<16);
	printf("\n");

        printf("Start playing ...\n");
	j=164; //512;
        while(1) /* let user to interrupt, or if(count<record_size) */
        {
		/* j number of samples to form a sine circle,
		 * fs=44.1k, f=fs/j;  j=164 f=268Hz; j=256 f=44100/256=172Hz
		 * j=4; f=44.1k/4=11kHz
		 */
		//j-=1;
		//if(j<2)j=164;
		j=9;//5KHz
		printf("j=%d  F=%dHz\n",j,44100/j);

		/* generate a tone, a pure sine wave */
		for(k=0; k<1024; k++)
			buff[k]= ( (int)(fp16_sin[ (1024/j*k) &(1024-1) ]*(Mamp>>2)) )>>16; /* Suppose right_shift is arithmatical! */

                /* write back to pcm dev(interleaved) */
	   	for(i=0; i<164/j; i++) {
			ret=snd_pcm_writei(play_handle, (void**)&buff, chunk_frames);
		        if (ret == -EPIPE) {
        	    		/* EPIPE means underrun */
            			fprintf(stderr,"snd_pcm_writei() underrun occurred\n");
            			snd_pcm_prepare(play_handle);
	        	}
		        else if(ret<0) {
          		        fprintf(stderr,"snd_pcm_writei():%s\n",snd_strerror(ret));
		        }
		        else if (ret!=chunk_frames) {
                		fprintf(stderr,"snd_pcm_writei(): short write, write %d of total %ld frames\n",
										ret, chunk_frames);
	        }
	  }

	}
        snd_pcm_close(play_handle);

return 0;
}
