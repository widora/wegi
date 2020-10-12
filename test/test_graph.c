/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

TODO:
1. Autogrow data buffer.
2. Way to review old data.

Midas Zhou
--------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include <sys/time.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"
#include "egi_iwinfo.h"

void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py);

int main(int argc, char **argv)
{
	int opt;	/* imput option */
	float scale=1.0; /* Option scale for gif */
	int dms=0;	/*  Option delay in ms for each frame */
	char *fpath=NULL;
	int i,k,j;
    	int xres;
    	int yres;

	EGI_GIF		*egif=NULL;
	EGI_GIF_CONTEXT gif_ctxt={ 0 };
	EGI_IMGBUF	**mvpic=NULL;
	EGI_IMGBUF	*logo;

	bool PortraitMode=false; 	/* LCD display mode: Portrait or Landscape */
	bool ImgTransp_ON=false; 	/* Suggest: TURE */
	bool DirectFB_ON=false; 	/* For transparency_off GIF,to speed up,tear line possible. */
	int User_DispMode=-1;   	/* <0, disable */
	int User_TransColor=-1; 	/* <0, disable, if>=0, auto set User_DispMode to 2. */
	int User_BkgColor=-1;   	/* <0, disable */
	int xp=0,yp=0;
	int xw=0,yw=0;
	struct timeval tm_start, tm_end;
	//int tms=0;
	int luma=0;

	int gh=160;			/* Graph area height */
	int gw=300;			/* Graph area width */
	int margin=10;			/* gh+2*margin=pad height*/
	int ph=gh+2*margin;		/* pad height */
	int tgap=(240-gh)/2;

	EGI_16BIT_COLOR color;
	char *ifname=NULL; //"apcli0";
	unsigned long long Brecv[2]={0};   	/* Total bytes received */
	unsigned long long Btrans[2]={0};   	/* Total bytes transmitted */
	long long RXBps=0,TXBps=0; 		/* Traffic speed bytes per second */
	int st=300;		/* Sample total in buffer */
	int step=320/st;	/* Graph unit,sample gap */
	//int sn=0;		/* Sample buff index */
	long long strans[st];		/* Speed of transmitting, buffer */
	long long srecv[st];		/* Speed of receiving, buffer */
	//int srecvmax=1;		/* NOT 0 */
	long long smax=1;		/* Max speed in buff */
	bool smax_is_tx=false;
	int xs;			/* X coordinate */
	float avgload[3]={0};
	int vload[st];
	int vx[st];
	char strtmp[128]={0};

	int scount=0;		/* Sampling counter */

	/* Init vars */
	for( i=0; i<st; i++) {
		vx[i]=i*step+320-gw;  /* 320-gw=20 for color band */
		strans[i]=0;
		srecv[i]=0;
		vload[i]=0;
	}

	/* Parse input option */
	while( (opt=getopt(argc,argv,"htdps:m:u:f:"))!=-1)
	{
    		switch(opt)
    		{
		       case 'h':
		           printf("usage:  %s [-h] [-t] [-d] [-p] [-s:] [-m:] [-f:] fpath \n", argv[0]);
		           printf("         -h   help \n");
		           printf("         -t   Image_transparency ON. ( default is OFF ) \n");
		           printf("         -d   Wirte to FB mem directly, without buffer. ( default use FB working buffer ) \n");
		           printf("         -p   Portrait mode. ( default is Landscape mode ) \n");
		           printf("         -s   scale    scale for gif image \n");
		           printf("         -m   delayms  delay in ms for each frame \n");
		           printf("         -u	 average luma \n");
		           printf("         -f	 ifname \n");
			   printf("fpath   Gif file path \n");
		           return 0;
		       case 't':
		           printf(" Set ImgTransp_ON=true.\n");
		           ImgTransp_ON=true;
		           break;
		       case 'd':
		           printf(" Set DirectFB_ON=true.\n");
		           DirectFB_ON=true;
		           break;
		       case 'p':
			   printf(" Set PortraitMode=true.\n");
			   PortraitMode=true;
			   break;
		       case 's':
			   scale=atof(optarg);
			   printf(" Set Scale=%f.2\n", scale);
			   break;
		       case 'm':
			   dms=atoi(optarg);
			   printf(" Set dms=%d\n", dms);
			   break;
		       case 'u':
			   luma=atoi(optarg);
			   printf(" Set luma=%d\n",luma);
			   break;
		       case 'f':
			   ifname=optarg;
			   printf(" Set ifname='%s'\n", ifname);
			   break;
		       default:
		           break;
	    	}
	}
	/* Get file path */
	printf(" optind=%d, argv[%d]=%s\n", optind, optind, argv[optind] );
	fpath=argv[optind];
	if(fpath==NULL) {
	           printf("usage:  %s [-htdps:] fpath\n", argv[0]);
	}

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_gif") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
	}

        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
#endif
  	printf("FTsymbol_load_sysfonts()...\n");
	if(FTsymbol_load_sysfonts() !=0) {
        	printf("Fail to load FT sysfonts, quit.\n");
	        return -1;
  	}

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 0
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif

        /* <<<<------------------  End EGI Init  ----------------->>>> */

	/* Read in logo */
	logo=egi_imgbuf_readfile("/mmc/logo.png");

        /* refresh working buffer */
        //clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);

	/* Set FB mode as LANDSCAPE  */
	if(PortraitMode)
        	fb_position_rotate(&gv_fb_dev,0);
	else
        	fb_position_rotate(&gv_fb_dev,3);

    	xres=gv_fb_dev.pos_xres;
    	yres=gv_fb_dev.pos_yres;

        /* Set FB mode */
    	if(DirectFB_ON) {
            gv_fb_dev.map_bk=gv_fb_dev.map_fb; /* Direct map_bk to map_fb */

    	} else {

	    draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY4, 0, 0, 320-1, (240-ph)/2);
	    draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_BLACK, 0, (240-ph)/2+1, 320-1, 240-(240-ph)/2-1);
	    draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY4, 0, 240-(240-ph)/2, 320-1, 240-1);
	    fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF,FBDEV_BKG_BUFF);

    	}

        /* <<<<------------------  End FB Setup ----------------->>>> */

	/* Read GIF data into an EGI_GIF */
	egif= egi_gif_slurpFile( fpath, ImgTransp_ON); /* fpath, bool ImgTransp_ON */

	if( egif==NULL ) {
		printf("Fail to read in gif file!\n");
	}
	else {
        	printf("Finish reading GIF file into EGI_GIF by egi_gif_slurpFile().\n");

		/* Calloc movpic */
		mvpic=calloc(egif->ImageTotal, sizeof(EGI_IMGBUF *));
		if(mvpic==NULL) {
			printf("Fail to calloc mvpic!\n");
			exit(1);
		}

		/* Set RWidth/RHeight */
		egif->RWidth=scale*egif->SWidth;
		egif->RHeight=scale*egif->SHeight;

		/* Cal xp,yp xw,yw, to position it to the center of LCD  */
 		if(egif->RWidth > 0 && egif->RWidth!=egif->SWidth ) {
			xp=egif->RWidth>xres ? (egif->RWidth-xres)/2:0;
			yp=egif->RHeight>yres ? (egif->RHeight-yres)/2:0;
			xw=egif->RWidth>xres ? 0:(xres-egif->RWidth)/2;
			yw=egif->RHeight>yres ? 0:(yres-egif->RHeight)/2;
		}
		else {
			xp=egif->SWidth>xres ? (egif->SWidth-xres)/2:0;
			yp=egif->SHeight>yres ? (egif->SHeight-yres)/2:0;
			xw=egif->SWidth>xres ? 0:(xres-egif->SWidth)/2;
			yw=egif->SHeight>yres ? 0:(yres-egif->SHeight)/2;
		}

        	/* Lump context data */
	        gif_ctxt.fbdev=NULL; //&gv_fb_dev;
        	gif_ctxt.egif=egif;
	        gif_ctxt.nloop=0;    /* 0: Set one frame by one frame , <0: Forever, Else: number of loops */
		gif_ctxt.delayms=0;
	        gif_ctxt.DirectFB_ON=DirectFB_ON;
        	gif_ctxt.User_DisposalMode=User_DispMode;
	        gif_ctxt.User_TransColor=User_TransColor;
        	gif_ctxt.User_BkgColor=User_BkgColor;
	        gif_ctxt.xp=xp;
        	gif_ctxt.yp=yp;
	        gif_ctxt.xw=xw;
        	gif_ctxt.yw=yw;
		if(egif->RWidth > 0) {
        	    gif_ctxt.winw=egif->RWidth>xres ? xres : egif->RWidth; 	/* Put window at center of LCD */
	            gif_ctxt.winh=egif->RHeight>yres ? yres : egif->RHeight;
		} else {
	            gif_ctxt.winw=egif->SWidth>xres ? xres:egif->SWidth; 	/* Put window at center of LCD */
        	    gif_ctxt.winh=egif->SHeight>yres ? yres:egif->SHeight;
		}

		/* Decode GIF and save data to mvpic[] */
		for(i=0; i < egif->ImageTotal; i++) {
		   	/* Display one frame/block each time, then refresh FB page.  */
	    		egi_gif_displayGifCtxt( &gif_ctxt );
			printf("-Step %d/%d\n", gif_ctxt.egif->ImageCount, egif->ImageTotal);

			/* Transfer ownership of imgbuf to mvpic[] */
			mvpic[i]=egi_imgbuf_resize(gif_ctxt.egif->Simgbuf, egif->RWidth,  egif->RHeight);
			mvpic[i]->delayms=gif_ctxt.egif->Simgbuf->delayms;

			/* Apply luminance */
			if(luma>0)
				egi_imgbuf_avgLuma( mvpic[i], luma );
		}
	}

	gettimeofday(&tm_start,NULL);

	/* Draw graph */
        /* Copy FB mmap data to WORKING_BUFF and BKG_BUFF */
        //fb_init_FBbuffers(&gv_fb_dev); //fb_clear_bkgBuff(&gv_fb_dev, WEGI_COLOR_GRAY2);
	i=0;
	while(1) {
		/* For() one round of GIF image, as background image. */
		if( egif != NULL ) {
				if(i==gif_ctxt.egif->ImageTotal)
					i=0;
				/* Display mvpic[] */
				printf("I%d\n",i);
				fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
				egi_imgbuf_windisplay2( mvpic[i], &gv_fb_dev, // -1,
		                                          xp, yp, xw, yw, gif_ctxt.winw, gif_ctxt.winh);
				i++;
		}
//		else /* No GIF */
//			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);

		/* Check time gap:  Load and Net traffic data reading */
		gettimeofday(&tm_end,NULL);
		if( tm_diffus(tm_start, tm_end) >= 1000000 )
		{
			scount++;
			gettimeofday(&tm_start,NULL);

			/* A. Get net traffic/speed and display to the graph */
			//if( iw_get_speed(&Bps, "apcli0") !=0 )
			if( iw_read_traffic(ifname, Brecv+1, Btrans+1) !=0 ) {
				printf("Fail to read net traffic!\n");
			}
			else {
				printf("read_traffic Brecv: %llu Bps\n",Brecv[1]);
				printf("read_traffic Btrans: %llu Bps\n",Btrans[1]);

				/* --- 1. Calculate receiving speed --- */
				if(Brecv[0]!=0) {
					RXBps=Brecv[1]-Brecv[0];
					if(RXBps<0) printf("!!! RXBps<0!\n");
				}
				if(RXBps==0)  /* Also for Brecv[0]==0 */
					RXBps=1;  /*  NOT 0, div  */

				/* Update Brecv[0] */
				Brecv[0]=Brecv[1];

				/* Shift srecv data */
				memmove(srecv+1, srecv, (st-1)*sizeof(srecv[0])); //typeof(srecv[0])));

				/* Push new data */
				srecv[0]=RXBps;

				/* --- 2. Calculate transmitting speed --- */
				if(Btrans[0]!=0)
					TXBps=Btrans[1]-Btrans[0];
				if(TXBps==0)  /* Also for Brecv[0]==0 */
					TXBps=1;  /*  NOT 0 */

				/* Update Brecv[0] */
				Btrans[0]=Btrans[1];

				/* Shift srecv data */
				memmove(strans+1, strans, (st-1)*sizeof(typeof(strans[0])));

				/* Push new data */
				strans[0]=TXBps;

				/* 3. Get current Max speed in buff  */
				smax=1; /* NOT 0 */
				for( k=0; k<st; k++) {
					if( srecv[k] > smax ) {
						smax=srecv[k];
						smax_is_tx=false;
						xs=vx[k];
					}
					if( strans[k] >smax ) {
						smax=strans[k];
						smax_is_tx=true;
						xs=vx[k];
					}
				}
			}

			/* B. Get average load */
			if( iw_read_cpuload( avgload )!=0 ) {
				printf("Fail to read avgload!\n");
			}
			else
			{
				/* Shift vload */
				memmove( vload+1, vload, (st-1)*sizeof(vload[0]));

				/* Update vload[0] */
				vload[0]=avgload[0]*gh/5.0;   /* Set Max of avg is 5 */
			}
		}
		else if( egif==NULL ) {
			tm_delayms(10);
			continue;
		}
		/* END of time check */

		/* Draw BKG */
		if( egif==NULL )
			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);

		/* Fill gap if GIF size < 320 */
		if( (egif != NULL) && (egif->SHeight < 240) ) {
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY2, 0,0, 320-1, (240-egif->SHeight)/2);
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY2, 0,240-(240-egif->SHeight)/2 , 320-1, 240-1);
		}

		/* Draw color mark for CPU LOAD */
		EGI_HSV_COLOR hsv; 	/* R 0, G 120, B 240 */
		for(k=0; k<gh; k++){
			hsv=(EGI_HSV_COLOR){ round(1.0*k*(120-0)/gh), 10000, 255}; /* hue 0-120 Deg */
			fbset_color(egi_color_HSV2RGB(&hsv));
			draw_line(&gv_fb_dev, 5, (240-gh)/2+k, 15-1, (240-gh)/2+k);
		}

		/* Draw scale mark 5 */
		fbset_color(WEGI_COLOR_GRAYA);
		for(k=0; k<6; k++) {
			/* Vertical dot lines */
			for( j=0; j<gh; j+=4)
				draw_dot(&gv_fb_dev, (320-gw)+( k!=5 ? k*gw/5 : gw-1 ), (240-gh)/2+j);
			/* Horizontal dot lines */
			for( j=0; j<gw; j+=4)
				draw_dot(&gv_fb_dev, (320-gw)+j, (240-gh)/2+k*gh/5);
		}

		/* Draw logo */
		if(logo) {
			egi_imgbuf_windisplay( logo, &gv_fb_dev, -1,
						  0, 0, gv_fb_dev.pos_xres-100, gv_fb_dev.pos_yres-45,
								logo->width, logo->height);
		}

		/* 1. Draw a pad */
//		draw_blend_filled_rect( &gv_fb_dev, 0, tgap-5, 320-1, 240-tgap+5, WEGI_COLOR_DARKGRAY, 120); /* 5 as margin */

		/* 2. Draw RX traffic diagram on the graph pad */
		fbset_color(WEGI_COLOR_GREEN);
		for(k=0; k<st-1; k++)
			draw_wline_nc(&gv_fb_dev, vx[k], gh-srecv[k]*gh/smax+tgap, vx[k+1], gh-srecv[k+1]*gh/smax+tgap, 1);
		if(1.0*RXBps > 1024.0*1024.0)
			sprintf(strtmp,"RX: %.2fM", 1.0*RXBps/1024.0/1024.0);
		else
			sprintf(strtmp,"RX: %.1fk", RXBps/1024.0);
	        FTsymbol_writeFB(strtmp, 18, 18, WEGI_COLOR_LTGREEN, 5, 3);

		/* 3. Draw TX traffic diagram on the graph pad */
		fbset_color(WEGI_COLOR_LTBLUE);
		for(k=0; k<st-1; k++)
			draw_wline_nc(&gv_fb_dev, vx[k], gh-strans[k]*gh/smax+tgap, vx[k+1], gh-strans[k+1]*gh/smax+tgap, 1);
		if(TXBps>1024.0*1024.0)
			sprintf(strtmp,"TX: %.2fM", TXBps/1024.0/1024.0);
		else
			sprintf(strtmp,"TX: %.1fk", TXBps/1024.0);
	        FTsymbol_writeFB(strtmp, 18, 18, WEGI_COLOR_LTBLUE, 120, 3);

		/* 4. Print MAX speed */
		sprintf(strtmp,"Max %s: %.1fk (Bps)", smax_is_tx?"TX":"RX", smax/1024.0);
	        FTsymbol_writeFB(strtmp, 18, 18, WEGI_COLOR_ORANGE, 5, 240-27);
		/* Draw MAX mark on */
		fbset_color(WEGI_COLOR_ORANGE);
		draw_filled_circle(&gv_fb_dev, xs, tgap, 5);

		/* 5. Display avgload */
		fbset_color(WEGI_COLOR_RED);
		for(k=0; k<st-1; k++)
			draw_wline_nc(&gv_fb_dev, vx[k], gh-vload[k]+tgap, vx[k+1], gh-vload[k+1]+tgap, 1);

		/* 6. Write AVGLoad */
		if( scount&1 ) {
			sprintf(strtmp,"Load: %.2f", avgload[0]);
			if(avgload[0]>4.0) 	color=WEGI_COLOR_RED;
			else if(avgload[0]>3.0) color=WEGI_COLOR_YELLOW;
			else if(avgload[0]>2.0) color=WEGI_COLOR_YELLOWGREEN;
			else			color=WEGI_COLOR_GREEN;
	        	FTsymbol_writeFB(strtmp, 18, 18, color, 320-95, 3);
		}
		/* 6. Write Cliets number */
		else {
			sprintf(strtmp,"Clients: %d", iw_get_clients(ifname));
		        FTsymbol_writeFB(strtmp, 18, 18, WEGI_COLOR_WHITE, 320-95, 3);
		}

		/* Draw warter mark */
		#if 0
		fbset_color(WEGI_COLOR_WHITE);
		for(k=0; k<6; k++) {
			//draw_line(&gv_fb_dev, 0, (240-gh)/2+k*gh/5, (k%5==0)?20:5, (240-gh)/2+k*gh/5);
			draw_line(&gv_fb_dev, (k%5==0)?320-10:320-5, (240-gh)/2+k*gh/5, 320-1, (240-gh)/2+k*gh/5);
		}
		#endif

		/* Render */
		fb_render(&gv_fb_dev);

		/* Apply user defined delayms */
		if( dms < 1 ) {
			if( avgload[0] < 5.0)
				dms=10*(5-avgload[0]);
			else
				dms=0;
		}
		tm_delayms(dms);

	}  /* End while() */

	/* Free vars */
	if(mvpic!=NULL) {
	    for(i=0; i< egif->ImageTotal; i++)
			egi_imgbuf_free(mvpic[i]);
	    free(mvpic);
	}

    	egi_gif_free(&egif);

        /* <<<<<-----------------  EGI general release  ----------------->>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_end_touchread()...\n");
        egi_end_touchread();
        printf("egi_quit_log()...\n");

        egi_quit_log();
        printf("<-------  END  ------>\n");

	return 0;
}



/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:           Input text
@px,py:         LCD X/Y for start point.
--------------------------------------*/
void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.regular,       /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,      /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        px, py,                         /* x0,y0, */
                                        color, -1, 255,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}

