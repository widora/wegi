/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example of generating sine wave sound data and displaying FFT result.
PCM FORMATE S16, sample rate 44.1KHz.

Note:
1. Assume left_shifting and right_shifting are both arithmatical!


Midas Zhou
-----------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"


/* for pcm PLAYBACK */
snd_pcm_t *play_handle;
static int 	nchanl=1;                       /* number of channles */
static int 	format=SND_PCM_FORMAT_S16_LE;
static int 	bits_per_sample=16;
static int 	frame_size=2;                   /*bytes per frame, for 1 channel, format S16_LE */
static int 	srate=44100;                    /* HZ,  sample rate for capture */
static bool 	enable_resample=true;           /* whether to enable resample */
static int 	latency=10000; //500000;        /* required overall latency in us */
//snd_pcm_uframes_t period_size;        /* max numbers of frames that HW can hanle each time */
static snd_pcm_uframes_t chunk_frames=1024;    /* in frame, expected frames for readi/writei each time */
static int 	chunk_size;                     /* =frame_size*chunk_frames */
static int16_t buff[1024*1];                   /* chunk_frames, int16_t for S16 */
static int16_t Mamp=(1U<<15)-1;     		/* Max. amplitude for format S16 */
static int32_t fp16Sin[1024];                 /* Fix point sine value lookup table */

static pthread_t	pthread_play;

/*-------------------------------------
  A thread to play tone data in buff[]
 -------------------------------------*/
static void *tone_play(void *arg)
{
	int ret;

   while(true)
   {
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


/*--------------------
	MAIN
---------------------*/
int main(void)
{
	int  i,j,k;
	int  repeat;
	char strp[256];
	int  nkindex;
	int  holdus;

	/* for FFT, nexp+aexp Max. 21 */
	unsigned int 	nexp=10; 	// 10
	unsigned int 	np=1<<nexp;  	/* input element number for FFT */
	unsigned int 	aexp=11;   	/* 11 MAX Amp=1<<aexp */
	int		*nx;
	EGI_FCOMPLEX 	*ffx;  		/* FFT result */
        EGI_FCOMPLEX 	*wang; 		/* unit phase angle factor */
	unsigned int 	ns=1<<6;   	/* points for spectrum diagram */
	unsigned int    avg;
	int		ng=np/ns;	/* each ns covers ng numbers of np */
	int		step;
	int 		sdx[1<<6];//ns	/* displaying points  */
	int 		sdy[1<<6];
	int		sx0;
	int		spwidth=240;    /* displaying widht for the spectrum diagram */
	int		nk[1<<6]= {	/* index of ffx[] for displaying */
				1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,
				56,64,72,80,88,96,104,112,120,128,136,144,152,160,168,176,
				192,208,224,240,256,272,288,304,320,336,352,368,384,400,416,432
			};

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


       /* <<<<<  EGI general init  >>>>>> */
#if 0
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_fb") != 0) {	/* start logger */
                printf("Fail to init logger, quit.\n");
                return -1;
        }
#endif
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts(sympg) */
                printf("Fail to load sym pages, quit.\n");
                return -2;
        }
	if(FTsymbol_load_allpages() !=0 ) {
                printf("Fail to load sympg ascii, quit.\n"); /* load ASCII sympg */
                return -2;
	}
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;


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

        /* generate fixed sine value, 1024 points in 2*PI */
        printf("Generate fp16Sin[]...");
        for(k=0; k<1024; k++)
                fp16Sin[k]=sin(2*MATH_PI/1024*k)*(1<<16);
        printf("\n");


        /* adjust record volume, or to call ALSA API */
        system("amixer -D hw:0 set Capture 85%");
        system("amixer -D hw:0 set 'ADC PCM' 85%");

	/* clear screen */
	clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);
	//draw_line(&gv_fb_dev, 0, 240, 239, 240);

	/* put a tab */
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_appfonts.regular,  /* FBdev, fontface */
                                     20, 20, (unsigned char *)"FFT音频演示",               /* fw,fh, pstr */
                                     240, 1,  0,           /* pixpl, lines, gap */
                                     55, 270,                      /* x0,y0, */
                                     WEGI_COLOR_YELLOW, -1, -1);   /* fontcolor, stranscolor,opaque */


	/* create a thread to play tone */
	if(pthread_create(&pthread_play, NULL, tone_play, NULL)!=0) {
		printf("Fail to create playback thread!\n");
		return -1;
	}


	/*  <<<<<<<<<<<<<<<<<<    LOOP TEST 	>>>>>>>>>>>>>>>>>  */
        printf("Start generating tone and playing ...\n");

        j=256;//164; //512;
        while(1)
        {
                /* j: number of samples to form a sine circle,
                 * fs=44.1k, f=fs/j;
		 * j=164 f=268Hz; 	j=256 f=44100/256=172Hz;
                 * j=4  f=11kHz;	j=9 5KHz;
		 * j=64 f=689Hz;  nkindex=16
                 */
		if(j>64)
			j-=6;	/* increase LOW freq span */
		else
			j-=1;

                if(j<3)j=256;

		sprintf(strp, "Freq=%dHz", 44100/j);
                printf("j=%d  %s\n",j,strp);

                /* 1. generate a tone, a pure sine wave */
                for(k=0; k<1024; k++)
                        buff[k]= ( fp16Sin[ (1024/j*k) & (1024-1) ]*(Mamp>>2) )>>16;

	/* <<<<<<<<<<<<<<<<<<<   FFT  >>>>>>>>>>>>>>>>>>>> */
		/* convert PCM buff[] to FFT nx[] */
		for(i=0; i<np; i++) {
			nx[i]=buff[i]>>5;  /* trim amplitude to Max 2^11 */
			//nx[i]=buff[i];
		}

		/* Run FFT with INT nx[] */
        	mat_egiFFFT(np, wang, NULL, nx, ffx);

		/* update sdy[], spectrum diagram (half) */
		for(i=0; i<ns; i++) {
			/* according to nk[], more grids for LOW freq  */
			if(i<16) {
				step=2;//1;
			}
			else if(i<48) {
				step=8;//2;
			}
			else if(i<176) {
				step=32;//8;
			}
			else
				step=32;

		   	/* average, or some frequency will be lost */
			avg=0;
			for( k=0; k<step ; k++ ) {
				avg+=mat_uintCompAmp(ffx[ nk[i]+k ])>>(nexp-1 -4); /* get amplitude */
			}
			sdy[i]=240-avg/step;

			/* trim sdy[] */
			if(sdy[i]<100)
				sdy[i]=100;
		}
	/* <<<<<<<<<<<<<<<<<<<   FFT END  >>>>>>>>>>>>>>>>>>>> */

		/* draw spectrum */
	        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        	fb_filo_on(&gv_fb_dev);    /* start collecting old FB pixel data */

		fbset_color(WEGI_COLOR_GREEN);
		for(i=0; i<ns-1; i++) {
			//draw_dot(&gv_fb_dev,sdx[i],240-sdy[i]);
			draw_line(&gv_fb_dev,sdx[i], sdy[i], sdx[i+1],sdy[i+1]);
		}

		/* display frequency */
		symbol_string_writeFB(&gv_fb_dev, &sympg_ascii,
                		      WEGI_COLOR_RED, -1,     /* fontcolor, int transpcolor */
				      50, 30,			 /* int x0, int y0 */
				      strp, -1);		 /* string, opaque */

	        fb_filo_off(&gv_fb_dev); /* turn off filo */

		/* For each frequency, with different hold_on time, depends on frequency span in nk[] */
		nkindex=44100/j/44; /* current freq index */
		if(nkindex<16) {
			holdus=30000; //2;
		}
		else if(nkindex<48) {
			holdus=30000;
		}
		else if(nkindex<176) {
			holdus=20000;
		}
		else
			holdus=200000;

		usleep(holdus);
		//usleep(55000);
		//tm_delayms(2000);

    } /* end while */


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
