/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to analyze MIC captured audio and display its spectrum.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <alsa/asoundlib.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_math.h"

int main(void)
{
	int i,j;
	int ret;

	/* for FFT, nexp+aexp Max. 21 */
	unsigned int 	nexp=10;
	unsigned int 	np=1<<nexp;  	/* input element number for FFT */
	unsigned int 	aexp=11;   	/* 11 MAX Amp=2^12-1 */
	int		*nx;
	EGI_FCOMPLEX 	*ffx;  		/* FFT result */
        EGI_FCOMPLEX 	*wang; 		/* unit phase angle factor */
	unsigned int 	ns=1<<5;//6;   	/* Points for spectrum diagram */
	unsigned int    avg;
	int		ng=np/ns;	/* each ns covers ng numbers of np. ng=2^5, when fs=8k, sampling points=1024, fgap=8Hz*ng=256Hz */
	int		step;
	int 		sdx[64];//ns	/* displaying points  */
	int 		sdy[64];
	int		sx0;
	int		spwidth=240;    /* displaying widht for the spectrum diagram */

	sx0=(240-spwidth/(ns-1)*(ns-1))/2;
	for(i=0; i<ns; i++) {
		sdx[i]=sx0+(spwidth/(ns-1))*i;
	}

	/* prepare FFT */
	nx=calloc(np, sizeof(int));
	if(nx==NULL) {
		printf("Fail to calloc nx[].\n");
		return -1;
	}
	ffx=calloc(np, sizeof(EGI_FCOMPLEX));
	if(ffx==NULL) {
		printf("Fail to calloc ffx[]. \n");
		return -1;
	}
        wang=mat_CompFFTAng(np); /*  phase angle */

        /* For pcm capture */
        snd_pcm_t *play_handle;
	snd_pcm_t *rec_handle;
        int 	nchanl=1;   			/* number of channles */
        int 	format=SND_PCM_FORMAT_S16_LE;
        int 	bits_per_sample=16;
        int 	frame_size=2;               	/*bytes per frame, for 1 channel, format S16_LE */
        int 	rec_srate=8000; //44100;//8000 	/* HZ,  output sample rate from capture */

	/* If rec_srate != play_srate, crack... */
	int 	play_srate=8000;		/* HZ,  input sample rate for playback */
        bool 	enable_resample=true;      	/* whether to enable soft resample */

	/* Adjust latency to a suitable value,according to CAPTURE/PLAYBACK period_size */
        int 	rec_latency=5000; //500000;     /* required overall latency in us */
        int 	play_latency=150000; //500000;  /* required overall latency in us */
        //snd_pcm_uframes_t period_size;  	/* max numbers of frames that HW can hanle each time */

        /* For CAPTURE, period_size=1536 frames, while for PLAYBACK: 278 frames */
        snd_pcm_uframes_t chunk_frames=np; //1024; 	/* in frame, expected frames for readi/writei each time */
        int chunk_size;                 	/* =frame_size*chunk_frames */
	int16_t buff[1024*2];			/* 1024*2 frames, each frame 2 bytes, try to read out all! */

#if 0
	int16_t *buff;				/* chunk_frames, int16_t for S16 */
	buff=calloc(np, sizeof(int16_t));       /* for 1 channel,FORMAT S16 */
	if(buff==NULL) {
		printf("Fail to calloc buff[] \n");
		return -2;
	}
#endif


        /* <<<<<  EGI general init  >>>>>> */
#if 1
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_fb") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_sysfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,true);
        fb_position_rotate(&gv_fb_dev,1);

        /* open pcm captrue device */
        if( snd_pcm_open(&rec_handle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0) <0 ) {
                printf("Fail to open pcm captrue device!\n");
                return -1;
        }

        /* set params for pcm capture handle */
        if( snd_pcm_set_params( rec_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                                nchanl, rec_srate, enable_resample, rec_latency )  <0 ) {
                printf("Fail to set params for pcm capture handle.!\n");
                snd_pcm_close(rec_handle);
                return -2;
        }

#if 0
        /* open pcm play device */
        if( snd_pcm_open(&play_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {
                printf("Fail to open pcm play device!\n");
                return -1;
        }

        /* set params for pcm play handle */
        if( snd_pcm_set_params( play_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                                nchanl, play_srate, enable_resample, play_latency )  <0 ) {
                printf("Fail to set params for pcm play handle.!\n");
                snd_pcm_close(play_handle);
                return -2;
        }
#endif

        /* Adjust record volume, or to call ALSA API */
        system("amixer -D hw:0 set Capture 85%");
        system("amixer -D hw:0 set 'ADC PCM' 85%");

	/* Clear screen */
	clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);
	draw_line(&gv_fb_dev, 0, 240, 239, 240);
	fbset_color(WEGI_COLOR_CYAN);

	/* Put a tab */
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,  /* FBdev, fontface */
                                     25, 25, (UFT8_PCHAR)"FFT DEMO",             /* fw,fh, pstr */
                                     240, 1,  0,           	     /* pixpl, lines, gap */
                                     55, 240+30,                     /* x0,y0, */
                                     WEGI_COLOR_WHITE, -1, 255,   /* fontcolor, stranscolor,opaque */
				     NULL, NULL, NULL, NULL);

        printf("Start recording and playing ...\n");
        while(1) /* Let user to interrupt, or if(count<record_size) */
        {
                /* snd_pcm_sframes_t snd_pcm_readi (snd_pcm_t pcm, void buffer, snd_pcm_uframes_t size) */
		/* return number of frames, for 1 chan, 1 frame=size of a sample (S16_Le)  */
                ret=snd_pcm_readi(rec_handle, buff, 2048); //chunk_frames);
                if(ret == -EPIPE ) {
                        /* EPIPE for overrun */
                        printf("Overrun occurs during capture, try to recover...\n");
                    /* try to recover, to put the stream in PREPARED state so it can start again next time */
                        snd_pcm_prepare(rec_handle);
                        continue;
                }
                else if(ret<0) {
                        printf("snd_pcm_readi error: %s\n",snd_strerror(ret));
                        continue; /* carry on anyway */
                }
                /* CAUTION: short read may cause trouble! Let it carry on though. */
                //else if(ret != chunk_frames) {
                else if(ret < chunk_frames) {
                      printf("snd_pcm_readi: read end or short read ocuurs! get only %d of %ld expected frame",
                                                                                ret, chunk_frames);
                        /* pad 0 to chunk_frames */
                        if( ret<chunk_frames ) {  /* >chunk_frames NOT possible? */
                                memset( buff+ret*frame_size, 0, (chunk_frames-ret)*frame_size);
                        }
                }

#if 1	/* <<<<<<<<<<<<<<<<<<<   FFT  >>>>>>>>>>>>>>>>>>>> */
		/* Convert PCM buff[] to FFT nx[] */
		for(i=0; i<np; i++) {
			nx[i]=buff[i]>>4;  /* trim amplitude to Max 2^12-1 */
		}

		/* ---  Run FFT with INT nx[]  --- */
        	mat_egiFFFT(np, wang, NULL, nx, ffx);

		/* Update sdy */
#if 1  /* -----  1. Symmetric spectrum diagram ----- */
		for(i=0; i<ns; i++) {
			sdy[i]=240-( mat_uintCompAmp(ffx[i*ng])>>(nexp-1 -5) ); /* -5, shrink to 2^(-5) */

			/* trim sdy[] */
			if(sdy[i]<0)
				sdy[i]=0;
		}

#else  /* -----  2. Normal spectrum diagram ----- */
		step=ng>>1;
		for(i=0; i<ns; i++) {		     /* fs=8k, 1024 elements, resolution 8Hz, ng=2^5 */
		  #if 1 /* Direct calculation, Amp=FFT_A/(NN/2)  */
			sdy[i]=240-( mat_uintCompAmp( ffx[i*(ng>>1)])>>(nexp-1 -3) ); //(nexp-1) );
			//sdy[i]=240-( mat_uint32Log2( mat_uintCompAmp(ffx[i*(ng>>1)]) )<<3  );

		  #else  /* Average */
			/* get average Amp */
			avg=0;
			for( j=0; j< step; j++ ) {
				avg+=mat_uintCompAmp(ffx[i*step])>>(nexp-1 -3);
			}
			sdy[i]=240-avg/step;
		  #endif

			/* trim sdy[] */
			if(sdy[i]<0)
				sdy[i]=0;
		}
#endif

	        /* Apply 4 points average filter for sdy[], to smooth spectrum diagram, so it looks better! */
        	for(i=0; i<ns-(4-1); i++) {
                	for(j=0; j<(4-1); j++)
                        	sdy[i] += sdy[i+j];
		        sdy[i]>>=2;
        	}


		/* Draw spectrum */
	        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        	fb_filo_on(&gv_fb_dev);    /* start collecting old FB pixel data */

		for(i=1; i<ns-1; i++) {  /* i=0 is DC */
			//draw_dot(&gv_fb_dev,sdx[i],240-sdy[i]);
			draw_line(&gv_fb_dev,sdx[i], sdy[i], sdx[i+1],sdy[i+1]);
		}



	        fb_filo_off(&gv_fb_dev); /* turn off filo */

#endif 	/* <<<<<<<<<<<<<<<<<<<   FFT END  >>>>>>>>>>>>>>>>>>>> */


#if 0
                /* write back to pcm dev(interleaved) */
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
#endif


	}

        snd_pcm_close(rec_handle);
        snd_pcm_close(play_handle);

	free(ffx);
	free(wang);

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}
