/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

To display a png/jpg/GIF file.

Usage:
	showpic  file

Exampe:
	./showpic /tmp/bird.png
	./showpic /tmp/*

Note:
1. Keyboard Control:
	'w' or UP_ARROW	 	pan up
	's' or DOWN_ARROW	pan down
	'a' or LEFT_ARROW	pan left
	'd' or RIGHT_ARROW	pan right
	( For GIF: 'w,s,a,d' pan image relative to displaying windows. )
	'z'			zoom up
	'n'			zoom down
	'r'			rotate clockwise
	't'			rotate colocwise
	SPACE			to display next picture.
	'q'			quit

	'i' --- General image recognization
	'O' --- OCR detect
	'P' --- Face detect
2. Mouse Control:
	LeftKeyDownHold and move	Grab and pan
	MidKey rolling			zoom up/down.
	RightKey Click			Shift to next/prev picture

3. For GIF imgbuf updating:
   OPTION_1:  Continue decoding/updating in a thread.
	      Call egi_gif_runDisplayThread(&gif_ctxt), and it runs
	      in an independent thread.
	      Set gif_ctxt with TimeSynchOn=false AND nloop=-1.
   OPTION_2:  Carry out synchronized decoding/updating.
	      Call egi_gif_displayGifCtxt(&gif_ctxt) before each FB
	      rendering, It runs in the main loop.
	      Set gif_ctxt with TimeSynchOn=ture AND nloop=0.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <egi_common.h>
#include <egi_input.h>
#include <egi_gif.h>
#include <egi_FTsymbol.h>
#include <egi_procman.h>

#define GIF_RUN_THREAD	1   /***
			     * 1: Call egi_gif_runDisplayThread() to decode gif in a private thread.
			     * 0: Run egi_gif_displayGifCtxt() in main loop.
			     */

static EGI_MOUSE_STATUS mostat;
static EGI_MOUSE_STATUS *pmostat;
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
static void draw_mcursor(int x, int y);
static void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py);


/*----------------------------
            MAIN
----------------------------*/
int main(int argc, char** argv)
{
	int i,j;
	int xres;
	int yres;
	int xp,yp;
	int xw=0,yw=0;
	int step=20;
	int Sh=0,Sw=0;	/* Show_height, Show_width */
	int lastX=0,lastY=0;
	int angle=0;
	struct timeval tm_start, tm_end;

	/* percentage step  1/4 */
	EGI_IMGBUF* eimg=NULL;
	EGI_IMGBUF* tmpimg=NULL;
	EGI_IMGBUF* showimg=NULL;

	EGI_GIF*    egif=NULL;
	EGI_GIF_CONTEXT gif_ctxt;
	int	OLDindex=-1;

	int 	opt;
	bool 	PortraitMode=false;
	bool	TranspMode=false;
	bool	DirectFB_ON=false;

	char	cmdchar=-1; /* Init -1, not 0; A char as for command */
	int	byte_deep=0;


        /* parse input option */
        while( (opt=getopt(argc,argv,"htdp"))!=-1)
        {
                switch(opt)
                {
                       case 'h':
                           printf("usage:  %s [-h] [-p] [-t] fpath \n", argv[0]);
                           printf("         -h   help \n");
			   printf("         -t   Transparency on\n");
                           printf("         -p   Portrait mode. ( default is Landscape mode ) \n");
                           printf("fpath    file path \n");
                           return 0;
                       case 'p':
                           printf(" Set PortraitMode=true.\n");
                           PortraitMode=true;
                           break;
			case 't':
			   printf(" Set TranspMode=true.\n");
			   TranspMode=true;
			   break;
                       default:
                           break;
                }
        }

        /* Check input files  */
        //printf(" optind=%d, argv[%d]=%s\n", optind, optind, argv[optind] );
	if( optind < argc ) {
		i=optind;
		printf("Following files found:\n");
		while( i<argc)
			printf("%s\n",argv[i++]);
	} else {
		printf("No input file!\n");
		exit(1);
	}

	/* Set terminal IO attributes */
	egi_set_termios();

  	/* Start sys tick */
  	tm_start_egitick();

	/* Load sysfonts */
  	if(FTsymbol_load_sysfonts() !=0) {
        	printf("Fail to load FT sysfonts, quit.\n");
        	return -1;
  	}

        /* Init sys FBDEV  */
        if( init_fbdev(&gv_fb_dev) )
                return -1;

        /* Set FB position mode: LANDSCAPE  or PORTRAIT */
        if(PortraitMode)
                fb_position_rotate(&gv_fb_dev,0);
        else
                fb_position_rotate(&gv_fb_dev,3);

        xres=gv_fb_dev.pos_xres;
        yres=gv_fb_dev.pos_yres;

        /* set FB buffer mode: Direct(no buffer) or Buffer */
        if(DirectFB_ON) {
            //gv_fb_dev.map_bk=gv_fb_dev.map_fb; /* Direct map_bk to map_fb */
	    fb_set_directFB(&gv_fb_dev, true);
        }
	else {  /* FB BUFFER */

		/* Prepare backgroup grids image for transparent image */
		for(i=0; i < gv_fb_dev.vinfo.yres/40; i++) {
			for(j=0; j < gv_fb_dev.vinfo.xres/40; j++) {
				if( (i*(gv_fb_dev.vinfo.xres/40)+j+i)%2 )
					fbset_color(WEGI_COLOR_GRAY);
				else
					fbset_color(WEGI_COLOR_DARKGRAY);
				draw_filled_rect(&gv_fb_dev, j*40, i*40, (j+1)*40-1, (i+1)*40-1);
			}
		}
		/* Put mark */
	        //draw_filled_circle2(&gv_fb_dev, 320/2, 240-45, 10, WEGI_COLOR_DARKGREEN);
        	FTsymbol_writeFB("EGI", 30,30, WEGI_COLOR_DARKGREEN, 320/2-30, (240/2+80)-30-3);
	        FTsymbol_writeFB("SHOWPIC", 26,26, WEGI_COLOR_DARKGREEN, 320/2-70, (240/2+80)-3);

	    	fb_render(&gv_fb_dev);

		/* Copy FB mmap data to WORKING_BUFF and BKG_BUFF */
		#if 1
		fb_init_FBbuffers(&gv_fb_dev);
		//fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);

	    	#else
	            /* Init FB back ground buffer page */
        	    //memcpy(gv_fb_dev.map_buff+gv_fb_dev.screensize, gv_fb_dev.map_fb, gv_fb_dev.screensize);

	            /*  Init FB working buffer page */
        	    //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLUE);
	            memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_fb, gv_fb_dev.screensize);
	   	#endif
        }

	/* Start mouse read thread */
	if(egi_start_mouseread(NULL, mouse_callback)<0)
		return -2;


	/* NON_BLOCK read tty */
	int fd_tty;
	fd_tty=open("/dev/tty", O_RDONLY|O_NONBLOCK);
	if(fd_tty<0) {
		printf("Fail to open /dev/tty.\n");
		return -3;
	}

	/* ---------- Load image files ----------------- */
for( i=optind; i<argc; i++) {

        /* 1. Load pic to imgbuf */
	eimg=egi_imgbuf_readfile(argv[i]);
	if(eimg==NULL) {
        	//printf("Fail to read and load file '%s'!", argv[i]);
		//continue;
	} else {

		/* Init. params */
		Sh=eimg->height;
		Sw=eimg->width;
		showimg=egi_imgbuf_resize(eimg, Sw, Sh); /* Same size */

		xp=yp=0;
                xw=showimg->width>xres ? 0:(xres-showimg->width)/2;
                yw=showimg->height>yres ? 0:(yres-showimg->height)/2;
		angle=0;
	}

        /* Read GIF data into an EGI_GIF */
	if(eimg==NULL) {
        	egif= egi_gif_slurpFile(argv[i], TranspMode); /* fpath, bool ImgTransp_ON */
	        if(egif==NULL) {
        	        printf("Unrecognizable file '%s'!\n", argv[i]);
			continue;
		}

		/* Assign RWidth and RHeight */
		Sh=egif->SHeight;
		Sw=egif->SWidth;

		/* Set ctxt with init. params */
        	gif_ctxt = (EGI_GIF_CONTEXT) {
                        .fbdev=NULL,
                        .egif=egif,
			#if GIF_RUN_THREAD
                         .nloop=-1, /* 0 for TimeSyncOn, -1 forever */
			 .TimeSyncOn=false,
			#else
                         .nloop=0, /* 0 for TimeSyncOn, -1 forever */
			 .TimeSyncOn=true,
			#endif
			.delayms=50, /* valid only for TimeSyncOFF, */
                        .DirectFB_ON=false,
                        .User_DisposalMode=-1,
                        .User_TransColor=-1,
                        .User_BkgColor=-1,
                        .xp=egif->SWidth>xres ? (egif->SWidth-xres)/2:0,
                        .yp=egif->SHeight>yres ? (egif->SHeight-yres)/2:0,
		        .xw=egif->SWidth>xres ? 0:(xres-egif->SWidth)/2,
                	.yw=egif->SHeight>yres ? 0:(yres-egif->SHeight)/2,
                        .winw=egif->SWidth>xres ? xres:egif->SWidth,
                        .winh=egif->SHeight>yres ? yres:egif->SHeight
        	};

		/* Set xw,yw xp,yp for display */
		xw=gif_ctxt.xw;  yw=gif_ctxt.yw;
		//xp=gif_ctxt.xp;  yp=gif_ctxt.yp;
		xp=0; yp=0;

		OLDindex=-1;

		/* gif_runDisplayThread */
	       	#if GIF_RUN_THREAD
		 if( egi_gif_runDisplayThread(&gif_ctxt) !=0 ) {
			printf("Fail to run gif DisplayThread!\n");
			egi_gif_free(&egif);
			continue;
		 }
	       	#else
		 egi_gif_displayGifCtxt(&gif_ctxt);
	       	#endif
	}

	/* Rotate the imgbuf */
        /* Adjust Luminance */
        // egi_imgbuf_avgLuma(eimg, 255/3);

	/* 4. 	------------- Displaying  image ------------ */
  do {
	/* Read stdin */
	cmdchar=0;
	//read(fd_tty, &cmdchar, 1);
	read(STDIN_FILENO,&cmdchar,1);
	if(cmdchar!=0)
		printf("cmdchar: %d '%c'\n", cmdchar, cmdchar);

	/* B1. Keyboard control event */
	switch(cmdchar)
	{
		/* ----- Backward ----- */
		case 'b':
			i -=2 ;
			if(i < optind-1 )
				i=optind-1;
				cmdchar=' ';
			break;
		/* ---------------- Parse 'q' ------------------- */
		case 'q':
			goto END_DISPLAY;

		/* ---------------- Parse 'z' and 'n' ------------------- */
		case 'z':	/* zoom up */
			/* Limit size to limit memory */
			if( Sh*Sw < 4800000 ) {
				xw = xw-(Sh*9/8-Sh)/2; /* Zoom center at image center */
				yw = yw-(Sw*9/8-Sw)/2;

	 			Sh = Sh*9/8;
				Sw = Sw*9/8;
				printf("Zoom up: Sh=%d, Sw=%d\n",Sh, Sw);
			}

			/* Resize image */
			egi_imgbuf_free2(&showimg);
			showimg=egi_imgbuf_resize(eimg, Sw, Sh);
			if(showimg==NULL) {
				printf("showimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			}
			else {  /* Adjust Sh/Sw as the resized image */
				Sh=showimg->height;
				Sw=showimg->width;
			}
			break;
		case 'n':	/* zoom down */
			xw = xw+(Sh-Sh*7/8)/2; /* Zoom center at image center */
			yw = yw+(Sw-Sw*7/8)/2;

			/* Limit size to avoid a NULL showimg */
			if(Sh>2 && Sw>2) {
				Sh = Sh*7/8;
				Sw = Sw*7/8;
			}
			printf("Zoom down: Sh=%d, Sw=%d\n",Sh, Sw);

			/* Resize image */
			egi_imgbuf_free2(&showimg);
			showimg=egi_imgbuf_resize(eimg, Sw, Sh);
			if(showimg==NULL) {
				printf("showimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			}
			else {  /* Adjust Sh/Sw as the resized image */
				Sh=showimg->height;
				Sw=showimg->width;
			}
			break;
		/* -------------- Parse r and t for rotation ----------*/
		case 'r':	/* Rotate image */
			angle -=15;
			break;
		case 't':	/* Rotate image */
			angle +=15;
			break;
		/* ---------------- Parse arrow keys -------------------- */
		case '\033':
			byte_deep=1;
			break;
		case '[':
			if(byte_deep!=1) {
				byte_deep=0;
				break;
			}
			byte_deep=2;
			break;
		case 'D':		/* <--- */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
                        xw-=step;

			byte_deep=0;
                        break;
		case 'C':		/* ---> */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
			xw+=step;

			byte_deep=0;
                        break;
		case 'A':		   /* UP ARROW */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
			yw-=step;

			byte_deep=0;
			break;
		case 'B':		   /* DOWN ARROW */
			if(byte_deep!=2)
				break;
			yw+=step;

			byte_deep=0;
			break;
		/* ---------------- Parse Key 'a' 'd' 'w' 's' -------------------- */
		case 'a':
			xw-=step;
			break;
		case 'd':
			xw+=step;
			break;
		case 'w':
			yw-=step;
			break;
		case 's':
			yw+=step;
			break;

		/* ---------------- Parse Key 'i' 'o' 'p' -------------------- */
		case 'i':	/* General image recognization */
			system("/tmp/imgrecog.sh");
			break;
		case 'o':	/* OCR detect */
			system("/tmp/ocr.sh");
			break;
		case 'p':	/* Face detect */
			system("/tmp/facerecog.sh");
			break;
		default:
			byte_deep=0;
			break;
	}

	/* B2. Mouse control event */
   	//if(mostat.request) {
	if( egi_mouse_getRequest(pmostat) )
	{
		/* B2.1  To load next/prev image */
		if(pmostat->RightKeyDown) {
			/* Forward OR Back */
			if(pmostat->mouseX > xres/2) {
				printf("Forward---------\n");
				//do nothing
			}
			else {
				printf("Backward---------\n");
			  	i -=2 ;
				if(i < optind-1 ) i=optind-1;
			}
			pmostat->request=false;
			// cmdchar=' '; 		/* To trigger display */

			/* Put mouse request before break */
			egi_mouse_putRequest(pmostat);
			break;
		}
		/* B2.2 To reload image */
		if(pmostat->MidKeyDown) {
			i-=1;

                        /* Put mouse request before break */
                        egi_mouse_putRequest(pmostat);
                        break;
		}
		/* B2.3 To save current mouseXY */
		if(pmostat->LeftKeyDown) {
			lastX=pmostat->mouseX;
			lastY=pmostat->mouseY;
		}
		/* B2.4 To pan image */
		if(pmostat->LeftKeyDownHold) {
			/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
			 * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
			 * while mouseDX/DY do NOT has limits!!!
			 * We need to retrive DX/DY from previous mouseX/Y.
			 */
			xw  += (pmostat->mouseX-lastX);
			yw  += (pmostat->mouseY-lastY);

			/* update lastX,Y */
			lastX=pmostat->mouseX; lastY=pmostat->mouseY;
		}
		/* B2.5 To zoom up/down image */
		if( pmostat->mouseDZ!=0 ) {; //&& mostat.request ) {
		   /* For JPG/PNG Image */
		   if(eimg) {
			if( pmostat->mouseDZ <0 ) {	/* zoom up */
				/* Limit size to limit memory */
				if( Sh*Sw < 4800000 ) {
					xw = pmostat->mouseX-(pmostat->mouseX-xw)*9/8;  /* Zoom center at mouse tip */
					yw = pmostat->mouseY-(pmostat->mouseY-yw)*9/8;

	 				Sh = Sh*9/8;
					Sw = Sw*9/8;
					printf("Zoom Up: Sh=%d, Sw=%d\n",Sh, Sw);
				}
			}
			else {	/* zoom down */
				xw = pmostat->mouseX-(pmostat->mouseX-xw)*7/8;  /* Zoom center at mouse tip */
				yw = pmostat->mouseY-(pmostat->mouseY-yw)*7/8;

				/* Limit size to avoid a NULL showimg */
				if(Sh>2 && Sw>2) {
					Sh = Sh*7/8;
					Sw = Sw*7/8;
				}
				printf("Zoom Down: Sh=%d, Sw=%d\n",Sh, Sw);
			}
			/* Resize image */
			egi_imgbuf_free2(&showimg);
			showimg=egi_imgbuf_resize(eimg, Sw, Sh);
			if(showimg==NULL) {
				printf("EIMG showimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			}
			else {  /* Adjust Sh/Sw as the resized image */
				Sh=showimg->height;
				Sw=showimg->width;
			}
		    }
		    /* For GIF image */
		    else if( egif ) {
			if( pmostat->mouseDZ <0 ) {	/* zoom up */
				/* Limit size to limit memory */
				if( Sh*Sw < 4800000 ) {
					xw = pmostat->mouseX-(pmostat->mouseX-xw)*9/8;  /* Zoom center at mouse tip */
					yw = pmostat->mouseY-(pmostat->mouseY-yw)*9/8;

	 				Sh = Sh*9/8;
					Sw = Sw*9/8;
					printf("Zoom Up: Sh=%d, Sw=%d\n",Sh, Sw);
				}
			}
			else {	/* zoom down */
				xw = pmostat->mouseX-(pmostat->mouseX-xw)*7/8;  /* Zoom center at mouse tip */
				yw = pmostat->mouseY-(pmostat->mouseY-yw)*7/8;

				/* Limit size to avoid a NULL showimg */
				if(Sh>2 && Sw>2) {
					Sh = Sh*7/8;
					Sw = Sw*7/8;
				}
				printf("Zoom Down: Sh=%d, Sw=%d\n",Sh, Sw);
			}
		    }
		}

		/* Put request */
		egi_mouse_putRequest(pmostat);
   	}


	/* B3. FB write image */
//	if( cmdchar!=0 || mostat.LeftKeyDownHold || mostat.mouseDZ!=0 ) {

	    /* B3.1  Display PNG/JPG */
	    if( eimg ) {
		//if(!TranspMode)
		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
	 	//fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_GRAY); /* for transparent picture */

		gettimeofday(&tm_start, NULL);
	   #if 1 /* Rotate_display */
  		/* imgbuf, fb_dev, angle, int xri, int yri, int xrl, int yrl */
		egi_image_rotdisplay( showimg, &gv_fb_dev, angle,
						showimg->width/2, showimg->height/2, xw+showimg->width/2, yw+showimg->height/2 );
	   #else
       	 	egi_imgbuf_windisplay2( showimg, &gv_fb_dev, //-1,
         	        	        xp, yp, xw, yw,
               			        showimg->width, showimg->height);
	   #endif
		gettimeofday(&tm_end, NULL);
		//printf("writeFB time: %ldms \n",(tm_diffus(tm_start, tm_end)+400)/1000);
	   }

	   /* B3.2  Display GIF */
	   else if( egif ) {
#if 0
		if(OLDindex<0)
			OLDindex=egif->ImageCount;
		else if ( OLDindex==egif->ImageCount ) {
			Sh=showimg->height;
			Sw=showimg->width;
			goto START_GIF_DISPLAY;
		}
#endif

		#if !GIF_RUN_THREAD
		 /* Update gif imgbuf only, fbdev is NULL. */
		 egi_gif_displayGifCtxt(&gif_ctxt);
		#endif

	     	/* Resize each frame of GIF image to get required size! */
		tmpimg=showimg; /* back it up, in case showimg is NULL. */
		showimg=egi_imgbuf_resize(gif_ctxt.egif->Simgbuf, Sw, Sh);  /* egif->Simgbuf MAYBE init time with all alphas ==0 */
		if(showimg==NULL) {
			printf("GIF showimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			showimg=tmpimg;  /* Revive */
		}
		else {  /* Adjust Sh/Sw as the resized image */
			Sh=showimg->height;
			Sw=showimg->width;
			/* free tmpimg now */
			egi_imgbuf_free2(&tmpimg);
		}

	START_GIF_DISPLAY:
		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
	        egi_imgbuf_windisplay2( showimg, &gv_fb_dev, //-1,    /* img, fb, subcolor */
        	                        xp, yp,            /* xp, yp */
                	                xw, yw,            /* xw, yw */
                        	        showimg->width, showimg->height //Sw, Sh /* winw, winh */
                             );
		OLDindex=egif->ImageCount;

	   }

//	}

	draw_mcursor(mostat.mouseX, mostat.mouseY);

        /* B4.1 Refresh FB by memcpying back buffer to FB */
	//gettimeofday(&tm_start, NULL);
        //fb_page_refresh(&gv_fb_dev,0);
	fb_render(&gv_fb_dev);
	//gettimeofday(&tm_end, NULL);
	//printf("Render time: %ldms \n",(tm_diffus(tm_start, tm_end)+400)/1000);

	/* B4.2 Mouse direct to FB */
	//draw_mcursor(mostat.mouseX, mostat.mouseY);
	tm_delayms(20);

	/* B5. Reset request */
	mostat.request=false;

#if 0	/* B6. Wait for mouse/keybaord events */
        do {
		cmdchar=0;
		read(STDIN_FILENO, &cmdchar, 1);
		if(cmdchar!=0)
			printf("cmdchar: %d '%c'\n", cmdchar, cmdchar);
	} while ( cmdchar==0 && !mostat.request );
#endif

  } while( cmdchar != ' ' ); /* input SPACE to end displaying current image */

        //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLACK);
	//fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
	//fb_render(&gv_fb_dev);

	/* 5. Free imgbuf and gif */
	egi_imgbuf_free2(&eimg);
	egi_imgbuf_free2(&showimg);

	/* 6. Stop gif */
	if(egif) {
		#if GIF_RUN_THREAD
		 egi_gif_stopDisplayThread(egif);
		#endif
		egi_gif_free(&egif);
	}

} /* End displaying all image files */


END_DISPLAY:
	close(fd_tty);

        /* <<<<<  EGI general release >>>>> */
	printf("egi_end_mouseread()...\n");
	egi_end_mouseread();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);

	/* Reset terminal attr */
	egi_reset_termios();

return 0;

}


/*-----------------------------------------------------------
		Callback for mouse event
------------------------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus)
{
 	/* Wait until main thread finish last mouse_request respond */
//	while( mostat.request && !mostatus->cmd_end_loopread_mouse ) { usleep(1000); };
	while( egi_mouse_checkRequest(mostatus) && !mostatus->cmd_end_loopread_mouse ) { usleep(2000); };

	/* If request to quit mouse thread */
	if( mostatus->cmd_end_loopread_mouse )
		return;

        /* Get mostatus mutex lock */
        if( pthread_mutex_lock(&mostatus->mutex) !=0 ) {
                printf("%s: Fail to mutex lock mostatus.mutex!\n",__func__);
        }
 /*  --- >>>  Critical Zone  */

	/* Pass out mouse data */
	mostat=*mostatus;
	pmostat=mostatus;

	/* Request for respond */
	mostat.request = true;
	pmostat->request = true;

/*  --- <<<  Critical Zone  */
        /* Put mutex lock */
        pthread_mutex_unlock(&mostatus->mutex);
}

/*------------------------------------
	   Draw mouse
-------------------------------------*/
static void draw_mcursor(int x, int y)
{
        static EGI_IMGBUF *mcimg=NULL; /* cursor */
        static EGI_IMGBUF *mgimg=NULL; /* grab */

#ifdef LETS_NOTE
	#define MCURSOR_ICON_PATH 	"/home/midas-zhou/egi/mcursor.png"
	#define MGRAB_ICON_PATH 	"/home/midas-zhou/egi/mgrab.png"
	#define TXTCURSOR_ICON_PATH 	"/home/midas-zhou/egi/txtcursor.png"
#else
	#define MCURSOR_ICON_PATH 	"/mmc/mcursor.png"
	#define MGRAB_ICON_PATH 	"/mmc/mgrab.png"
	#define TXTCURSOR_ICON_PATH 	"/mmc/txtcursor.png"
#endif

        if(mcimg==NULL)
                mcimg=egi_imgbuf_readfile(MCURSOR_ICON_PATH);
        if(mgimg==NULL)
                mgimg=egi_imgbuf_readfile(MGRAB_ICON_PATH);

	/* Always draw mouse in directFB mode */
	/* directFB and Non_directFB mixed mode will disrupt FB synch, and make the screen flash?? */
   //fb_filo_off(&gv_fb_dev);
	//mode=fb_get_FBmode(&gv_fb_dev);
//	fb_set_directFB(&gv_fb_dev,true);

        /* OPTION 1: Use EGI_IMGBUF */
	if(mostat.LeftKeyDown || mostat.LeftKeyDownHold )
	        egi_subimg_writeFB(mgimg, &gv_fb_dev, 0, -1, x, y); /* subnum,subcolor,x0,y0 */
	else
	        egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, x, y); /* subnum,subcolor,x0,y0 */

        /* OPTION 2: Draw geometry */

	/* Restore FBmode */
//	fb_set_directFB(&gv_fb_dev,false);
	//fb_set_directFB(&gv_fb_dev, mode);
   //fb_filo_on(&gv_fb_dev);

}

/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:           Input text
@px,py:         LCD X/Y for start point.
--------------------------------------*/
static void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,//regular,       /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,      /* fw,fh, pstr */
                                        320, 1, 0,                      /* pixpl, lines, fgap */
                                        px, py,                         /* x0,y0, */
                                        color, -1, 255,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
}


/*-----------------------------------
Warning: race condition with main()
FT_Load_Char()...
------------------------------------*/
void signal_handler(int signal)
{
        printf("signal=%d\n",signal);
        if(signal==SIGINT) {
                printf("--- SIGINT ----\n");
		egi_reset_termios();
        }
}

