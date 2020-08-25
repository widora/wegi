/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

To display a png/jpg file on the LCD.

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
	'z'			zoom up
	'n'			zoom down
	SPACE			to display next picture.
	'q'			quit

	'i' --- General image recognization
	'O' --- OCR detect
	'P' --- Face detect
2. Mouse Control:
	LeftKeyDownHold and move	Grab and pan
	MidKey rolling			zoom up/down.
	RightKey Click			Shift to next/prev picture

TODO: To move image totally out of screen, forth and back, ...then
      the bug appears.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <egi_common.h>
#include <egi_input.h>
#include <egi_gif.h>

static EGI_MOUSE_STATUS mostat;
static EGI_MOUSE_STATUS *pmostat;
static void mouse_callback(unsigned char *mouse_data, int size, EGI_MOUSE_STATUS *mostatus);
static void draw_mcursor(int x, int y);


/*----------------------------
            MAIN
----------------------------*/
int main(int argc, char** argv)
{
	int i,j;
	int xres;
	int yres;
	int xp,yp;
	int step=50;
	int Sh=0,Sw=0;	/* Show_height, Show_width */
	int lastX=0,lastY=0;
	struct timeval tm_start, tm_end;

	/* percentage step  1/4 */
	EGI_IMGBUF* eimg=NULL;
	EGI_IMGBUF* tmpimg=NULL;

	EGI_GIF*    egif=NULL;
	EGI_GIF_CONTEXT gif_ctxt;

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
		printf("Follow files found:\n");
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
        } else {

	    /* Prepare backgroup grids image for transparent image */
	    for(i=0; i<6; i++) {
		for(j=0; j<8; j++) {
			if( (i*8+j+i)%2 )
				fbset_color(WEGI_COLOR_GRAY);
			else
				fbset_color(WEGI_COLOR_DARKGRAY);
			draw_filled_rect(&gv_fb_dev, j*40, i*40, (j+1)*40-1, (i+1)*40-1);
		}
	    }
	    fb_render(&gv_fb_dev);

	    /* Copy FB mmap data to WORKING_BUFF and BKG_BUFF */
	    fb_init_FBbuffers(&gv_fb_dev);
	    #if 0
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


	/* ---------- Load image files ----------------- */
for( i=optind; i<argc; i++) {

        /* 1. Load pic to imgbuf */
	eimg=egi_imgbuf_readfile(argv[i]);
	if(eimg==NULL) {
        	//printf("Fail to read and load file '%s'!", argv[i]);
		//continue;
	} else {
		Sh=eimg->height;
		Sw=eimg->width;
		tmpimg=egi_imgbuf_resize(eimg, Sw, Sh); /* Same size */
	}

        /* Read GIF data into an EGI_GIF */
	if(eimg==NULL) {
        	egif= egi_gif_slurpFile(argv[i], TranspMode); /* fpath, bool ImgTransp_ON */
	        if(egif==NULL) {
        	        printf("Unrecognizable file '%s'!\n", argv[i]);
			continue;
		}

		/* Assign RWidth and RHeight */
		egif->RWidth=egif->SWidth;
		egif->RHeight=egif->SHeight;

		/* Set ctxt */
        	gif_ctxt = (EGI_GIF_CONTEXT) {
                        .fbdev=NULL, //&gv_fb_dev,
                        .egif=egif,
                        .nloop=-1,
			.delayms=45,
                        .DirectFB_ON=false,
                        .User_DisposalMode=-1,
                        .User_TransColor=-1,
                        .User_BkgColor=-1,
                        .xp=egif->RWidth>xres ? (egif->RWidth-xres)/2:0,
                        .yp=egif->RHeight>yres ? (egif->RHeight-yres)/2:0,
		        .xw=egif->RWidth>xres ? 0:(xres-egif->RWidth)/2,
                	.yw=egif->RHeight>yres ? 0:(yres-egif->RHeight)/2,
                        .winw=egif->RWidth>xres ? xres:egif->RWidth,
                        .winh=egif->RHeight>yres ? yres:egif->RHeight
        	};

		/* gif_runDisplayThread */
		if( egi_gif_runDisplayThread(&gif_ctxt) !=0 ) {
			printf("Fail to run gif DisplayThread!\n");
			egi_gif_free(&egif);
			continue;
		}
	}

	/* rotate the imgbuf */

        /* 2. resize the imgbuf  */

        /* 3. Adjust Luminance */
        // egi_imgbuf_avgLuma(eimg, 255/3);

	/* 4. 	------------- Displaying  image ------------ */
	xp=yp=0;
  do {
	/* B1. Keyboard control event */
	switch(cmdchar)
	{
		/* ---------------- Parse 'q' ------------------- */
		case 'q':
			goto END_DISPLAY;

		/* ---------------- Parse 'z' and 'n' ------------------- */
		case 'z':	/* zoom up */
			/* Limit size to limit memory */
			if( Sh*Sw < 4800000 ) {
				xp = (xp+xres/2)*5/4 - xres/2;	/* zoom focus at center of LCD */
				yp = (yp+yres/2)*5/4 - yres/2;

	 			Sh = Sh*5/4;
				Sw = Sw*5/4;
				printf("Zoom up: Sh=%d, Sw=%d\n",Sh, Sw);
			}

			/* Resize image */
			egi_imgbuf_free2(&tmpimg);
			tmpimg=egi_imgbuf_resize(eimg, Sw, Sh);
			if(tmpimg==NULL) {
				printf("tmpimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			}
			else {  /* Adjust Sh/Sw as the resized image */
				Sh=tmpimg->height;
				Sw=tmpimg->width;
			}
			break;
		case 'n':	/* zoom down */
			xp = (xp+xres/2)*3/4 - xres/2;	/* zoom focus at center of LCD */
			yp = (yp+yres/2)*3/4 - yres/2;

			/* Limit size to avoid a NULL tmpimg */
			if(Sh>2 && Sw>2) {
				Sh = Sh*3/4;
				Sw = Sw*3/4;
			}
			printf("Zoom down: Sh=%d, Sw=%d\n",Sh, Sw);

			/* Resize image */
			egi_imgbuf_free2(&tmpimg);
			tmpimg=egi_imgbuf_resize(eimg, Sw, Sh);
			if(tmpimg==NULL) {
				printf("tmpimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			}
			else {  /* Adjust Sh/Sw as the resized image */
				Sh=tmpimg->height;
				Sw=tmpimg->width;
			}
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
                        xp-=step;
//                       if(xp<0)xp=0;

			byte_deep=0;
                        break;
		case 'C':		/* ---> */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
			xp+=step;
//			if(xp > tmpimg->width-xres )
//				xp=tmpimg->width-xres>0 ? tmpimg->width-xres:0;

			byte_deep=0;
                        break;
		case 'A':		   /* UP ARROW */
			if(byte_deep!=2) {
				byte_deep=0;
				break;
			}
			yp-=step;
//			if(yp<0)yp=0;

			byte_deep=0;
			break;
		case 'B':		   /* DOWN ARROW */
			if(byte_deep!=2)
				break;
			yp+=step;
//			if(yp > tmpimg->height-yres )
//				yp=tmpimg->height-yres>0 ? tmpimg->height-yres:0;

			byte_deep=0;
			break;
		/* ---------------- Parse Key 'a' 'd' 'w' 's' -------------------- */
		case 'a':
			xp-=step;
//			if(xp<0)xp=0;
			break;
		case 'd':
			xp+=step;
			if(xp > tmpimg->width-xres )
				xp=tmpimg->width-xres>0 ? tmpimg->width-xres:0;
			break;
		case 'w':
			yp-=step;
//			if(yp<0)yp=0;
			break;
		case 's':
			yp+=step;
			if(yp > tmpimg->height-yres )
				yp=tmpimg->height-yres>0 ? tmpimg->height-yres:0;
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

			egi_mouse_putRequest(pmostat);
			break;
		}
		if(pmostat->LeftKeyDown) {
			lastX=pmostat->mouseX;
			lastY=pmostat->mouseY;
		}
		if(pmostat->LeftKeyDownHold) {
			//xp -= mostat.mouseDX;  /* Reverse direction */
			//yp -= mostat.mouseDY;

			/* Note: As mouseDX/DY already added into mouseX/Y, and mouseX/Y have Limit Value also.
			 * If use mouseDX/DY, the cursor will slip away at four sides of LCD, as Limit Value applys for mouseX/Y,
			 * while mouseDX/DY do NOT has limits!!!
			 * We need to retrive DX/DY from previous mouseX/Y.
			 */
			xp -= (pmostat->mouseX-lastX);
			yp -= (pmostat->mouseY-lastY);
			//printf("DX,DY: %d,%d\n",mostat.mouseDX,mostat.mouseDY);

			/* update lastX,Y */
			lastX=pmostat->mouseX; lastY=pmostat->mouseY;
		}
		if( pmostat->mouseDZ!=0 ) {; //&& mostat.request ) {
			if( pmostat->mouseDZ <0 ) {	/* zoom up */
				/* Limit size to limit memory */
				if( Sh*Sw < 4800000 ) {
					xp = (xp+pmostat->mouseX)*5/4 - pmostat->mouseX;	/* Zoom center at mouse tip */
					yp = (yp+pmostat->mouseY)*5/4 - pmostat->mouseY;

	 				Sh = Sh*5/4;
					Sw = Sw*5/4;
					printf("Zoom Up: Sh=%d, Sw=%d\n",Sh, Sw);
				}
			}
			else {	/* zoom down */
				xp = (xp+pmostat->mouseX)*3/4 - pmostat->mouseX;	/* Zoom center at mouse tip */
				yp = (yp+pmostat->mouseY)*3/4 - pmostat->mouseY;

				/* Limit size to avoid a NULL tmpimg */
				if(Sh>2 && Sw>2) {
					Sh = Sh*3/4;
					Sw = Sw*3/4;
				}
				printf("Zoom Down: Sh=%d, Sw=%d\n",Sh, Sw);
			}
			/* Resize image */
			egi_imgbuf_free2(&tmpimg);
			tmpimg=egi_imgbuf_resize(eimg, Sw, Sh);
			if(tmpimg==NULL) {
				printf("tmpimg is NULL for Sw=%d, Sh=%d\n", Sw, Sh);
			}
			else {  /* Adjust Sh/Sw as the resized image */
				Sh=tmpimg->height;
				Sw=tmpimg->width;
			}
		}

		/* Put request */
		egi_mouse_putRequest(pmostat);
   	}


	/* B3. FB write image */
	if( cmdchar!=0 || mostat.LeftKeyDownHold || mostat.mouseDZ!=0 ) {

	    /* Display PNG/JPG */
	    if( eimg ) {
		//if(!TranspMode)
			fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
			 //fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_GRAY); /* for transparent picture */

		gettimeofday(&tm_start, NULL);
       	 	egi_imgbuf_windisplay2( tmpimg, &gv_fb_dev, //-1,
         	        	        xp, yp, 0, 0,
               			        xres, yres ); //tmpimg->width, tmpimg->height);
		gettimeofday(&tm_end, NULL);
		printf("writeFB time: %ldms \n",(tm_diffus(tm_start, tm_end)+400)/1000);
	   }

	   /* Display gif */
	   else if( egif ) {
		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
	        egi_imgbuf_windisplay2( gif_ctxt.egif->Simgbuf, &gv_fb_dev, //-1,    /* img, fb, subcolor */
        	                       gif_ctxt.xp, gif_ctxt.yp,            /* xp, yp */
                	               gif_ctxt.xw, gif_ctxt.yw,            /* xw, yw */
                        	       gif_ctxt.winw, gif_ctxt.winh /* winw, winh */
                             );
	   }

	}

	  draw_mcursor(mostat.mouseX, mostat.mouseY);

        /* B4.1 Refresh FB by memcpying back buffer to FB */
	//gettimeofday(&tm_start, NULL);
        //fb_page_refresh(&gv_fb_dev,0);
	fb_render(&gv_fb_dev);
	//gettimeofday(&tm_end, NULL);
	//printf("Render time: %ldms \n",(tm_diffus(tm_start, tm_end)+400)/1000);

	/* B4.2 Mouse direct to FB */
//	draw_mcursor(mostat.mouseX, mostat.mouseY);
	tm_delayms(30);

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

         fbclear_bkBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

	/* 5. Free imgbuf and gif */
	egi_imgbuf_free2(&eimg);
	egi_imgbuf_free2(&tmpimg);

	/* 6. Stop gif */
	if(egif) {
		egi_gif_stopDisplayThread(egif);
		egi_gif_free(&egif);
	}

} /* End displaying all image files */


END_DISPLAY:
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
	while( egi_mouse_checkRequest(mostatus) && !mostatus->cmd_end_loopread_mouse ) { };

	/* If request to quit mouse thread */
	if( mostatus->cmd_end_loopread_mouse )
		return;

        /* Get mostatus mutex lock */
        if( pthread_mutex_lock(&mostatus->mutex) !=0 ) {
                printf("%s: Fail to mutex lock mostatus.mutex!\n",__func__);
        }

	/* Pass out mouse data */
	mostat=*mostatus;
	pmostat=mostatus;

	/* Request for respond */
	mostat.request = true;
	pmostat->request = true;

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

