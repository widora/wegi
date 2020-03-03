/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to analyze MIC captured audio and display its spectrum.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"

int main(void)
{
	int i,j;
	int ret;

	/* for FFT, nexp+aexp Max. 21 */
	unsigned int 	nexp=10; 	// 10
	unsigned int 	np=1<<nexp;  	/* input element number for FFT */
	unsigned int 	aexp=11;   	/* 11 MAX Amp=1<<aexp */
	int		*nx;
	EGI_FCOMPLEX 	*ffx;  		/* FFT result */
        EGI_FCOMPLEX 	*wang; 		/* unit phase angle factor */
	unsigned int 	ns=1<<5;//6;   	/* points for spectrum diagram */
	unsigned int    avg;
	int		ng=np/ns;	/* each ns covers ng numbers of np */
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

        /* for pcm capture */
        snd_pcm_t *play_handle;
	snd_pcm_t *rec_handle;
        int 	nchanl=1;   			/* number of channles */
        int 	format=SND_PCM_FORMAT_S16_LE;
        int 	bits_per_sample=16;
        int 	frame_size=2;               	/*bytes per frame, for 1 channel, format S16_LE */
        int 	rec_srate=8000; //44100;//8000 	/* HZ,  output sample rate from capture */
	/* if rec_srate != play_srate, crack... */
	int 	play_srate=8000;		/* HZ,  input sample rate for playback */
        bool 	enable_resample=true;      	/* whether to enable soft resample */
	/* adjust latency to a suitable value,according to CAPTURE/PLAYBACK period_size */
        int 	rec_latency=5000; //500000;    /* required overall latency in us */
        int 	play_latency=150000; //500000;   /* required overall latency in us */
        //snd_pcm_uframes_t period_size;  	/* max numbers of frames that HW can hanle each time */
        /* for CAPTURE, period_size=1536 frames, while for PLAYBACK: 278 frames */
        snd_pcm_uframes_t chunk_frames=np; //1024; 	/*in frame, expected frames for readi/writei each time */
        int chunk_size;                 	/* =frame_size*chunk_frames */


	int16_t buff[1024];
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
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;


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

        /* adjust record volume, or to call ALSA API */
        system("amixer -D hw:0 set Capture 85%");
        system("amixer -D hw:0 set 'ADC PCM' 85%");

	/* clear screen */
	clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);
	draw_line(&gv_fb_dev, 0, 240, 239, 240);
	fbset_color(WEGI_COLOR_CYAN);

	/* put a tab */
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.bold,  /* FBdev, fontface */
                                     25, 25, "FFT DEMO",               /* fw,fh, pstr */
                                     240, 1,  0,           /* pixpl, lines, gap */
                                     55, 240+30,                      /* x0,y0, */
                                     WEGI_COLOR_WHITE, -1, -1);   /* fontcolor, stranscolor,opaque */

        printf("Start recording and playing ...\n");
        while(1) /* let user to interrupt, or if(count<record_size) */
        {
//		printf(" --- \n");

                /* snd_pcm_sframes_t snd_pcm_readi (snd_pcm_t pcm, void buffer, snd_pcm_uframes_t size) */
		/* return number of frames, for 1 chan, 1 frame=size of a sample (S16_Le)  */
                ret=snd_pcm_readi(rec_handle, buff, chunk_frames);
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
                else if(ret != chunk_frames) {
                      printf("snd_pcm_readi: read end or short read ocuurs! get only %d of %ld expected frame",
                                                                                ret, chunk_frames);
                        /* pad 0 to chunk_frames */
                        if( ret<chunk_frames ) {  /* >chunk_frames NOT possible? */
                                memset( buff+ret*frame_size, 0, (chunk_frames-ret)*frame_size);
                        }
                }


#if 1	/* <<<<<<<<<<<<<<<<<<<   FFT  >>>>>>>>>>>>>>>>>>>> */
		/* convert PCM buff[] to FFT nx[] */
		for(i=0; i<np; i++) {
			nx[i]=buff[i]>>4;  /* trim amplitude to Max 2^11 */
		}

		/* ---  Run FFT with INT nx[]  --- */
        	mat_egiFFFT(np, wang, NULL, nx, ffx);

		/* update sdy */
#if 0  /* -----  1. Symmetric spectrum diagram ----- */
		for(i=0; i<ns; i++) {
			sdy[i]=240-( mat_uintCompAmp( ffx[i*ng])>>(nexp-1 -5) ); //(nexp-1) );
			/* trim sdy[] */
			if(sdy[i]<0)
				sdy[i]=0;
		}

#else  /* -----  2. Normal spectrum diagram ----- */
		step=ng>>1;
		for(i=0; i<ns; i++) {		     /* fs=8k, 1024 elements, resolution 8Hz, ng=4 */
		  #if 1 /* direct calculation */
			sdy[i]=240-( mat_uintCompAmp( ffx[i*(ng>>1)])>>(nexp-1 -3) ); //(nexp-1) );
			//sdy[i]=240-( mat_uint32Log2( mat_uintCompAmp(ffx[i*(ng>>1)]) )<<3  );

		  #else  /* average */
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

		/* draw spectrum */
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
