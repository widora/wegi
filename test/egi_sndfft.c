/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

EGI定点快速傅立叶变换(EGI Fixed_point FFT)展示拾音频谱
An example to analyze MIC captured audio and display its spectrum.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <alsa/asoundlib.h>
#include "egi_fbgeom.h"
#include "egi_FTsymbol.h"
#include "egi_math.h"

int main(void)
{
	int i,j;
	int ret;

	/* FFT参数设置 nexp+aexp Max. 21 */
	unsigned int 	nexp=7;
	unsigned int 	np=1<<nexp;  	/* 采样点数　input element number for FFT */
	unsigned int 	aexp=14;     	/* 最大幅度限制 MAX Amp=2^(14+1)-1 */
	unsigned int    fs=8000;	/* 采样频率fs, PCM录音采样率,则基频为 fs/np=62.5Hz */
	int		*nx;		/* 用于存放输入值 FFT samples */
	EGI_FCOMPLEX 	*ffx;  		/* 用于存放输出值 FFT output */
        EGI_FCOMPLEX 	*wang; 		/* 用于存放FFT系列相位角 phase angle factor */
	unsigned int 	ns=1<<6;   	/* 用于显示的点数,不是所有的np.　Points for spectrum diagram */
	int		ng=np/ns;	/* 每隔ng个点显示,ng=2,则刻度为 2*62.5=125HZ, each ns covers ng numbers of np */
	int 		pps;		/* 每个显示点间隔像素　pps=spwidth/(ns-1);  pixel per ns */
	int 		sdx[ns];	/* 显示点的屏幕坐标 displaying result points  */
	int 		sdy[ns];
	int		sx0;
	int		spwidth=320;    /* 频谱在屏幕上的宽度 displaying width(in pixels) for the spectrum diagram */
	char 		strtmp[32];

	/* 计算各显示点的横坐标　CoordX for displaying results, however sdx[ns] NOT applicable! */
	pps=spwidth/ns;
	sx0=(320-pps*ns)/2;
	for(i=0; i<ns; i++) {
		sdx[i]=sx0+pps*i;
	}
	/* Freq span per ns: fgap=fs/ns=8k/64=125Hz, 32*125=4000KHz, =8kHz/2 */

	/* FFT数据准备　prepare FFT */
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
        wang=mat_CompFFTAng(np); /*  产生FFT系列相位角 phase angle */

        /* 设置ALSA拾音参数 For PCM Capture */
	snd_pcm_t *rec_handle;
        int 	nchanl=1;   			/* 单声道　number of channles */
        int 	format=SND_PCM_FORMAT_S16_LE;   /* PCM数据格式, 16位带符号 */
        int 	frame_size=2;               	/* bytes per frame, for 1 channel, format S16_LE */
        int 	rec_srate=fs;			/* 采样率　HZ, output sample rate from capture */
        bool 	enable_resample=true;      	/* whether to enable soft resample */
	/* Adjust latency to a suitable value,according to CAPTURE/PLAYBACK period_size */
        int 	rec_latency=5000;     		/* required overall latency in us */
        /* For CAPTURE, period_size=1536 frames, while for PLAYBACK: 278 frames */
        snd_pcm_uframes_t chunk_frames=np;	/* in frame, expected frames for readi/writei each time */
	const int buffsize=1536;		/* Try PCM Capture period size，实际每次读取np个 */
	int16_t buff[buffsize];			/* 存放每次拾音得到的PCM数据 */


        /* <<<<<   初始化EGI系统 EGI general init >>>>>> */
        if(FTsymbol_load_sysfonts() !=0 ) {  	/* 加载EGI系统字体 load FT fonts */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
        if( init_fbdev(&gv_fb_dev) )		/* 初始化FB显示设备　init sys FB */
                return -1;

        /* 设置FB显示方式 Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,true);  /* 直接写屏 */
        fb_position_rotate(&gv_fb_dev,0);  /* 屏幕方向 */

        /* 打开ALSA默认录音设备　Open pcm captrue device  */
/* mode, SND_PCM_NO_AUTO_RESAMPLE,SND_PCM_NO_AUTO_CHANNELS,SND_PCM_NO_AUTO_FORMAT,SND_PCM_NO_SOFTVOL  */
        if( snd_pcm_open(&rec_handle, "default", SND_PCM_STREAM_CAPTURE, 0) <0 ) {
                printf("Fail to open pcm captrue device!\n");
                return -1;
        }

        /* 设置录音参数 Set params for pcm capture handle */
#if 1 //////////////////////////////////////////////////////////////////////////////
	snd_pcm_t *pcm_handle=rec_handle;
	 snd_pcm_hw_params_t *params;
	        int dir=0;
        snd_pcm_uframes_t frames;
        int rc;
	unsigned int srate=rec_srate;

        /* allocate a hardware parameters object */
	snd_pcm_hw_params_alloca(&params);

        /* fill it in with default values */
        snd_pcm_hw_params_any(pcm_handle, params);

        /* set access type  */
        snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

        /* HW signed 16-bit little-endian format */
        snd_pcm_hw_params_set_format(pcm_handle, params, format);

        /* set channel  */
        snd_pcm_hw_params_set_channels(pcm_handle, params, nchanl);

        /* sampling rate */
        snd_pcm_hw_params_set_rate_near(pcm_handle, params, &srate, &dir);
        if(dir != 0)
                printf("%s: Actual sampling rate is set to %d HZ!\n",__func__, srate);

        /* set HW params */
        if ( (rc=snd_pcm_hw_params(pcm_handle,params)) != 0 )
        {
                printf("%s: unable to set hw parameter: %s\n",__func__, snd_strerror(rc));
                snd_pcm_close(pcm_handle);
                return NULL;
        }

        /* get period size */
        snd_pcm_hw_params_get_period_size(params, &frames, &dir);
        printf("%s: snd pcm period size is %d frames.\n", __func__, (int)frames);

#else  ///////////////////////////////////////////////////////////////////////////////

        if( snd_pcm_set_params( rec_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                                nchanl, rec_srate, enable_resample, rec_latency )  <0 ) {
                printf("Fail to set params for pcm capture handle.!\n");
                snd_pcm_close(rec_handle);
                return -2;
        }
#endif

        /* 调节录音音量大小,或者后期用alsamixer命令条件．Adjust record volume, or to call ALSA API */
        system("amixer -D hw:0 set Capture 85%");
        system("amixer -D hw:0 set 'ADC PCM' 85%");

	/* 清屏 Clear screen */
	clear_screen(&gv_fb_dev,WEGI_COLOR_DARKPURPLE);

	/* 绘制频谱刻度 Draw spectrum scale:
	 * 125Hz*ns=125Hz*64=8kHz.  4*125Hz=500Hz for each scale.
	 * Each 125Hz takes spwidth/(ns-1)=300/63=4pixels.
	 */
	fbset_color(WEGI_COLOR_CYAN);
	draw_line(&gv_fb_dev, sx0, 215, sx0+pps*ns-1, 215);
	for(i=0; i<ns+1; i++) {  /* Total results points. ns */
		if(i%8==0) {	/* each 8 * 1/ns, 8*125=1000Hz */
			draw_line(&gv_fb_dev, sx0+i*pps, 215, sx0+i*pps, 215-9);
			sprintf(strtmp,"%dk", (i/8 -(i>32?2*(i-32)/8:0)) );
		    //if(i<ns/2+1)
		        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,  /* FBdev, fontface */
                		                     12, 12, (UFT8_PCHAR)strtmp,        /* fw,fh, pstr */
                                		     100, 1,  0,           	  /* pixpl, lines, gap */
		                                     sx0+i*pps-(i==0?0:6), 215,                /* x0,y0, */
                		                     WEGI_COLOR_CYAN, -1, 255,   /* fontcolor, stranscolor,opaque */
						     NULL, NULL, NULL, NULL);
		}
		else if(i%4==0) { /* each 500Hz */
			draw_line(&gv_fb_dev, sx0+i*pps, 215, sx0+i*pps, 215-6);
		}
		else	/* each 125Hz */
			draw_line(&gv_fb_dev, sx0+i*pps, 215, sx0+i*pps, 215-3);  /* each 1/ns (4 pixel) as 125Hz */


	}

	/* 写上标题 Put a tab */
	draw_blend_filled_rect( &gv_fb_dev, 0, 40-10, 320-1, 40+30+10, WEGI_COLOR_GRAY, 150);
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,  /* FBdev, fontface */
                                     25, 25, (UFT8_PCHAR)"EGI FFT DEMO",             /* fw,fh, pstr */
                                     240, 1,  0,           	  /* pixpl, lines, gap */
                                     80,  40,                     /* x0,y0, */
                                     WEGI_COLOR_WHITE, -1, 255,   /* fontcolor, stranscolor,opaque */
				     NULL, NULL, NULL, NULL);

	/* 循环: 拾音/FFT/显示频谱 */
        while(1) /* Let user to interrupt, or if(count<record_size) */
        {
		/* 从声卡读取拾音数据,每次np个． */
                /* snd_pcm_sframes_t snd_pcm_readi (snd_pcm_t pcm, void buffer, snd_pcm_uframes_t size) */
		/* return number of frames, for 1 chan, 1 frame=size of a sample (S16_Le)  */
                ret=snd_pcm_readi(rec_handle, buff, np);
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

		/* <<<<<   EGI FFT 开始  >>>>> */
		/* 将拾音数据转化成FFT输入数据 Convert PCM buff[] to FFT nx[] */
		for(i=0; i<np; i++)
			nx[i]=buff[i];  /* 如有必要的话对数据进行修剪，使其不大于2^(aexp+1)-1. trim amplitude to Max 2^(aexp+1)-1 */

		/* 对nx[]进行np点FFT运算,结果存入ffx[].  Run FFT with INT nx[] */
        	mat_egiFFFT(np, wang, NULL, nx, ffx);

		/* 计算频谱对应各点的纵向坐标. Update sdy[] */
#if 1  /* -----  1. 显示对称频谱　Symmetric spectrum diagram ----- */
		for(i=0; i<ns; i++) {
			sdy[i]=200-( mat_uintCompAmp(ffx[i*ng])>>(nexp-1 ) ); /* 如有必要对幅度进行缩小，以方便显示 */

			/* trim sdy[] */
			if(sdy[i]<0)
				sdy[i]=0;
		}

#else  /* -----  2. Normal spectrum diagram ----- */
		if(ng==1)
			step=1;
		else
			step=ng>>1;

		for(i=0; i<ns; i++) {		     /* fs=8k, 1024 elements, resolution 8Hz, ng=2^4 */
		  #if 1 /* Direct calculation, Amp=FFT_A/(NN/2)  */
			if( ns < 5 )
				sdy[i]=220-( mat_uintCompAmp(ffx[i*step])>>((nexp-1) +4) ); // -6) );  /* ng shrink 1/2,  -3 shrink  */
			else
				sdy[i]=220-( mat_uintCompAmp(ffx[i*step])>>((nexp-1) -6) ); // -6) );  /* ng shrink 1/2,  -3 shrink  */
			//printf("sdy[%d]=%d\n", sdy[i]);

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

#if 0		/* 如有必要可对频谱进行光滑等处理 */
                /* Apply 4 points average filter for sdy[], to smooth spectrum diagram, so it looks better! */
                for(i=0; i<ns-(4-1); i++) {
                        for(j=0; j<(4-1); j++)
                                sdy[i] += sdy[i+j];
                        sdy[i]>>=2;
                }
#endif

		/* 清除上一次的显示图谱 */
	        fb_filo_off(&gv_fb_dev);   /* turn off filo */
	        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
        	fb_filo_on(&gv_fb_dev);    /* start collecting old FB pixel data */

		/* 绘制频谱,用线段连接 */
		fbset_color(WEGI_COLOR_PINK);
		for(i=1; i<ns-1; i++) {  /* i=0/ns-1 is DC */
			draw_line(&gv_fb_dev,sdx[i], sdy[i], sdx[i+1],sdy[i+1]);
		}
//	        fb_filo_off(&gv_fb_dev); /* turn off filo */


		/* <<<<<  EGI FFT 结束  >>>>> */
	}

	/* 关闭录音设备,释放资源 */
        snd_pcm_close(rec_handle);
	free(ffx);
	free(wang);

        /* <<<<<  释放EGI系统资源 EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("<-------  END  ------>\n");

return 0;
}
