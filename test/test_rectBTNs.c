/*-----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A demo for simple rectangular buttons. An example of using switch/toggle
buttons and bounce_back buttons to control a music player.

NOTES:
1. For a switch/toggle button:
   When it is touched, it will keep pressed_down status though pen up,
   until another touch to switch back to released status.

2. For a bounce_back button:
   It keeps pressed_down status only when it is being touched/pressed,
   and will bounce back to released status once you raise up pen/finger.

3. A black banner will show up at the bottom of the screen when a
   button is touched, to indicate the type of the button. and with
   restore to back ground color when the button is released.

4. Sliding on the bottom banner to adjust the volume.

5. Add your command at reaction functions and a simple system() call is
   a quick workaround.

6. A more beautiful button is usually an (PNG/JPG) image icon.
   See attributes and behavior of btns[BTNID_LOGO].

7. A bounce_back button may react (execute private command) at time of
   pressing the button, OR at time of releasing, OR at time when pen is
   sliding out of the button(Lose Focus), It depends on your scenario.
   While its geom. shape always changes immediatly at time of pressing.
   (See OPTIONs in bounceback_button_react() )


Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>

#define PANEL_BKGCOLOR 	WEGI_COLOR_DARKGRAY 	/* The panel back ground color */

/* Rect Buttons and its IDs */
#define MAX_BTNS		5
static EGI_RECTBTN	  btns[MAX_BTNS];
#define BTNID_ONOFF		0
#define	BTNID_NEXT		1
#define BTNID_MODE		2
#define BTNID_SLIDE		3
#define BTNID_LOGO		4

/* Images */
static EGI_IMGBUF	  *bkimg=NULL;  		/* A W320xH240 backgroud image */
static EGI_IMGBUF	  *blockimg=NULL;		/* To store save banner area image */
static EGI_IMGBUF  	  *slideEffect=NULL;    	/* A sliding effect image */
static EGI_IMGBUF	  *logoimg=NULL;		/* A W140xH150 picture */
static EGI_IMGBUF	  *bannerMask=NULL;		/* A banner mask */
static EGI_IMGBUF	  *slideMask=NULL;		/* A mask, for sliding operation. */

/* play mode selection token */
static int play_mode;					/* 0- radio, 1-mp3 */

/* Reaction functions */
void switch_button_react(EGI_RECTBTN *btn);
void bounceback_button_react(EGI_RECTBTN *btn);
void slide_button_react(EGI_RECTBTN *btn);
void logo_button_react(EGI_RECTBTN *btn);
void ripple_mark(EGI_POINT touch_pxy, uint8_t alpha, EGI_16BIT_COLOR color);


/*----------------------------
	   MAIN
-----------------------------*/
int main(int argc, char **argv)
{

 /* <<<<<<  EGI general init  >>>>>> */
  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();
  #if 0
  /* Start egi log */
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }
  #endif

  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  #if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }
  #endif

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,0);

 /* <<<<<  End of EGI general init  >>>>>> */
  int vol=30;
  egi_getset_pcm_volume(NULL, &vol);
  clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);

	    		  /*-----------------------------------
    			   *            Main Program
    			   -----------------------------------*/

  int i;
  EGI_TOUCH_DATA  touch_data;

  /* Read in logoimg */
  int param=60;
  logoimg=egi_imgbuf_readfile("/mmc/scat.png");
  egi_imgbuf_setFrame( logoimg, frame_round_rect, 255, 1, &param);


  /* Init. bannerMask and slidMask */
  slideMask=egi_imgbuf_create( 240-190, 320, 220, WEGI_COLOR_GREEN); /* height, width, alpha, color */
  egi_imgbuf_fadeOutEdges(slideMask, 255, 320, FADEOUT_EDGE_LEFT, 0); /* imgbuf, maxalpha, width, ssmode, type */
  bannerMask=egi_imgbuf_create( 240-190, 320, 55, WEGI_COLOR_BLACK); /* height, width, alpha, color */
  slideEffect=egi_imgbuf_create( 240-190, 60, 255, WEGI_COLOR_WHITE); /* height, width, alpha, color */
  egi_imgbuf_fadeOutEdges(slideEffect, 255, 35, FADEOUT_EDGE_LEFT|FADEOUT_EDGE_RIGHT, 0); /* imgbuf, maxalpha, width, ssmode, type */

  if( bannerMask==NULL || slideMask==NULL )
	printf("Fail to create Mask image!\n");

  /* Assign buttons' attributors */
  for(i=0; i<3; i++) {
	btns[i].id=i;
  	btns[i].x0=50;		btns[i].y0=10+60*i;
	btns[i].width=120;	btns[i].height=50;
	btns[i].sw=4;
	btns[i].offy=-3;
	btns[i].reaction=(i%2==0?switch_button_react:bounceback_button_react);
	btns[i].pressed=false;
  }
  /* Slide button */
  btns[BTNID_SLIDE]=(EGI_RECTBTN){.id=BTNID_SLIDE, .x0=0, .y0=190, .width=320, .height=50, .reaction=slide_button_react };
  /* Logo button */
  btns[BTNID_LOGO]=(EGI_RECTBTN){.id=BTNID_LOGO, .x0=180, .y0=0, .width=140, .height=150, .reaction=logo_button_react };

  /* Button color */
  btns[BTNID_ONOFF].color=COLOR_24TO16BITS(0x66cc99);	/* ON /OFF */
  btns[BTNID_NEXT].color=COLOR_24TO16BITS(0x6666ff);	/* NEXT */
  btns[BTNID_MODE].color=COLOR_24TO16BITS(0xff6699);	/* Mode: Radio/MP3 */


  /* Clear screen and init. scene */
  if( (bkimg=egi_imgbuf_readfile("/mmc/linux.jpg")) != NULL ) {

	  /* Reset bkimg.subimgs */
	  bkimg->subimgs=egi_imgboxes_alloc(2);
	  bkimg->submax=2-1;
	  bkimg->subimgs[0]=(EGI_IMGBOX){ 0, 0, bkimg->width, bkimg->height };
	  bkimg->subimgs[1]=(EGI_IMGBOX){ 180, 0, 140, 150 };

	/* put a mask on bkimg for banner */
	egi_imgbuf_copyBlock(bkimg, bannerMask, true, 320, 240-190, 0, 190, 0, 0); /* destimg, srcimg, blendON, bw, bh, xd, yd, xs, ys */

	/* writeFB bkimg */
	//egi_subimg_writeFB(logoimg, &gv_fb_dev, 0, -1, bkimg->subimgs[0].x0, bkimg->subimgs[0].y0);  /* img, fbdev, subnum, subcolor, x0, y0 */
	egi_subimg_writeFB(bkimg, &gv_fb_dev, 0, -1, bkimg->subimgs[0].x0, bkimg->subimgs[0].y0);  /* img, fbdev, subnum, subcolor, x0, y0 */
	/* Save banner area */
	blockimg=egi_imgbuf_blockCopy(bkimg, 0, 190, 240-190, 320);  /* ineimg, px, py, height, width */
  }
  else  /* If no bkimage then use pure color */
  	fb_clear_backBuff(&gv_fb_dev, PANEL_BKGCOLOR);


  /* TEST: pressimg and releaseimg assignment */
  btns[BTNID_SLIDE].releaseimg=blockimg;
  btns[BTNID_SLIDE].pressimg=slideMask;

  btns[BTNID_LOGO].pressimg=logoimg;
  btns[BTNID_LOGO].releaseimg=bkimg;
  btns[BTNID_LOGO].releaseimg_subnum=1;

  /* Put buttuns */
  egi_sbtn_refresh(&btns[BTNID_ONOFF], "PLAY" );
  egi_sbtn_refresh(&btns[BTNID_NEXT], "NEXT" );
  egi_sbtn_refresh(&btns[BTNID_MODE], "RADIO" );
  egi_sbtn_refresh(&btns[BTNID_LOGO], NULL );

  /* Bring to screen */
  //fb_page_refresh(&gv_fb_dev, 0);
  fb_render(&gv_fb_dev);

  /* Loop touch events and button reactions: Assume that only one button may be touched each time!  */
  while(1) {
  	if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
		continue;
        /* Touch_data converted to the same coord as of FB */
        egi_touch_fbpos_data(&gv_fb_dev, &touch_data, -90);

	/* Check if touched any of the buttons */
	for(i=0; i<MAX_BTNS; i++) {
		if( egi_touch_on_rectBTN(&touch_data, &btns[i]) ) { //&& touch_data.status==pressing ) {  /* Re_check event! */
			printf("Button_%d touched!\n",i);
			btns[i].reaction(&btns[i]);
	  		fb_page_refresh(&gv_fb_dev, 0);
			break;
		}
	}

	/* i==MAX_BTNS No button touched!  Assume that only one button may be touched each time!  */
       	/* Touch missed: Ripple effect */
	if( i==MAX_BTNS ) {
	        ripple_mark(touch_data.coord, 185, WEGI_COLOR_YELLOW);
		continue;
	}

   }

 /* --- my releae --- */
 egi_imgbuf_free2(&bkimg);
 egi_imgbuf_free2(&blockimg);
 egi_imgbuf_free2(&bannerMask);
 egi_imgbuf_free2(&slideEffect);
 egi_imgbuf_free2(&logoimg);


	    		  /*-----------------------------------
    			   *         End of Main Program
    			   -----------------------------------*/


 /* <<<<<  EGI general release 	 >>>>>> */
 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);
 #if 0
 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);
 #endif
 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif
 printf("<-------  END  ------>\n");


return 0;
}



/*----------------------------------------
	Switch/Toggle button reaction

For btns[BTNID_ONOFF] and btns[BTNID_MODE]
-----------------------------------------*/
void switch_button_react(EGI_RECTBTN *btn)
{
	if(btn==NULL)
		return;

	/* BTNID_MODE button only applicable when btns[BTNID_ONOFF] if released! */
	if(btn->id==BTNID_MODE && btns[BTNID_ONOFF].pressed==true)
		return;

        /* Toggle/switch button */
        btn->pressed=!btn->pressed;

        /* Draw button */
	if(btn->id == BTNID_ONOFF)
		egi_sbtn_refresh(btn, btn->pressed?"STOP":"PLAY");
	else if(btn->id == BTNID_MODE)
		egi_sbtn_refresh(btn, btn->pressed?"MP3":"RADIO");

        /* Write Hello World! */
        if(btn->pressed) {
		if(blockimg!=NULL)
			egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190); /* Use saved image block */
		else
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* OR a black banner */
                FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                                24, 24,(const unsigned char *)"A toggle button!",    /* fw,fh, pstr */
                                                320, 1, 0,                                  /* pixpl, lines, gap */
                                                20, 190+10,                                    /* x0,y0, */
                                                btn->color, -1, 255,       /* fontcolor, transcolor,opaque */
                                                NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	}
	else {
		/* Clear Hello World with PANEL_BKGCOLOR */
		if(blockimg!=NULL)
			egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
		else
			draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* OR a black banner */
	}

	/* --- My command --- */
	if(btn->pressed) {
		if(btn->id==BTNID_ONOFF) {
			if(play_mode==0)
				system("/home/myradio.sh start &");
			else if(play_mode==1)
				system("/home/mp3.sh &");
		}
		else if(btn->id==BTNID_MODE)
			play_mode=1;
	}
	else {
		if(btn->id==BTNID_ONOFF) {
			if(play_mode==0)
				system("/home/myradio.sh quit &");
			else if(play_mode==1)
				system("/home/mp_quit.sh &");
		}
		else if(btn->id==BTNID_MODE)
			play_mode=0;
	}
}


/*-----------------------------------------------------------------
	Bounce_back button reaction
Pressed_hold then wait for releasing touch event.

Note:
1. There shall be no time delay before egi_touch_timeWait_release()!
   or it may miss the releasing event!

For btns[BTNID_NEXT]
-----------------------------------------------------------------*/
void bounceback_button_react(EGI_RECTBTN *btn)
{
	EGI_TOUCH_DATA touch_data;

	/* Draw pressed button */
        btn->pressed=true;

	/* Draw button */
  	egi_sbtn_refresh(btn, "gfjl/next");

	/* Write Hello World! clear banner area first */
	if(blockimg!=NULL)
		egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
	else
		draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* OR a black banner */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
      	       		                24, 24,(const unsigned char *)"A bounce_back button!",    /* fw,fh, pstr */
               	               	 	320, 1, 0,                                  /* pixpl, lines, gap */
                       	       		20, 190+10,                                    /* x0,y0, */
                       			btn->color, -1, 255,       /* fontcolor, transcolor,opaque */
                       			NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
  	fb_page_refresh(&gv_fb_dev, 0);

    #if 0  /* OPTION_1: Wait for Touch releasing */
  	egi_touch_timeWait_release(-1, 0, &touch_data);
    #else  /* OPTION_2: Wait for Focus lose */
	do{
		tm_delayms(50);
		while(!egi_touch_getdata(&touch_data));
	 	egi_touch_fbpos_data(&gv_fb_dev, &touch_data, -90);
	}
	while( egi_touch_on_rectBTN(&touch_data, btn) );
    #endif
	btn->pressed=false;

	/* Draw released button */
	tm_delayms(60); /*  hold on for a while before refresh, in case a too fast 'touch_and_release' event! */
  	egi_sbtn_refresh(btn, "NEXT");

	/* Clear Hello World with PANEL_BKGCOLOR */
	if(blockimg!=NULL)
		egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
	else
		draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_DARKGRAY, 0, 190, 320-1, 240-1);  /* Or a black banner */

	/* --- My command --- */
	if(play_mode==0)
		system("/home/myradio.sh next &");
	else if(play_mode==1)
		system("/home/mp_next.sh 1 &");
}


/*-----------------------------------------------------------------
		Slide button reaction
This is just a slide area. bounceback reaction.

For btns[BTNID_SLIDE]
-----------------------------------------------------------------*/
void slide_button_react(EGI_RECTBTN *btn)
{
	EGI_TOUCH_DATA touch_data;
	int vol;
	int adjvol;
	char strtmp[16];

	/* Draw pressed button */
        btn->pressed=true;

	/* Draw button */
  	egi_sbtn_refresh(btn, NULL);
  	fb_page_refresh(&gv_fb_dev, 0);

	/* slide control : Volum  UP/DOWN */
	egi_getset_pcm_volume(&vol, NULL);
	while(1) {
		while(egi_touch_getdata(&touch_data)==false);
		if(touch_data.status!=pressed_hold)
			break;

	        /* Touch_data converted to the same coord as of FB */
        	egi_touch_fbpos_data(&gv_fb_dev, &touch_data,-90);

		//adjvol=-25*touch_data.dy/320+vol;  /* 25 MAX to  [0 100] */
		adjvol=35*touch_data.dx/320+vol;     /* 30 MAX to  [0 100] */
		egi_getset_pcm_volume(NULL, &adjvol);

		memset(strtmp,0,sizeof(strtmp));
		sprintf(strtmp,"VOL: %d",adjvol);
		egi_subimg_writeFB(blockimg, &gv_fb_dev, 0, -1, 0, 190);  /* Use saved image block */
		egi_sbtn_refresh(btn, NULL); 				  /* Add sbtn pressimg effect */
		egi_subimg_writeFB(slideEffect, &gv_fb_dev, 0, -1, touch_data.coord.x-slideEffect->width/2, 190);
                symbol_string_writeFB(&gv_fb_dev, &sympg_ascii,
                                      WEGI_COLOR_WHITE, -1,     /* fontcolor, int transpcolor */
                                      320-80, 190+10,                    /* int x0, int y0 */
		                      strtmp, 255);                 /* string, opaque */
  		fb_page_refresh(&gv_fb_dev, 0);

	}
	/* Wait for touch releasing events */
  	//egi_touch_timeWait_release(-1, 0, &touch_data);

	/* Draw released button */
        btn->pressed=false;
	tm_delayms(60); /*  hold on for a while before refresh, in case a too fast 'touch_and_release' event! */
  	egi_sbtn_refresh(btn, NULL);

}


/*-----------------------------------------------------------------
		Bounce_back button reaction

For btns[BTNID_LOGO]
-----------------------------------------------------------------*/
void logo_button_react(EGI_RECTBTN *btn)
{
	EGI_TOUCH_DATA touch_data;

	/* Draw pressed button */
        btn->pressed=true;

	/* Draw button */
  	egi_sbtn_refresh(btn, NULL);
	fb_render(&gv_fb_dev);

    #if 0  /* OPTION_1: Wait for Touch releasing */
  	egi_touch_timeWait_release(-1, 0, &touch_data);
    #else  /* OPTION_2: Wait for Focus lose */
	do{
		tm_delayms(50);
		while(!egi_touch_getdata(&touch_data));
	 	egi_touch_fbpos_data(&gv_fb_dev, &touch_data,-90);
	}
	while( egi_touch_on_rectBTN(&touch_data, btn) );
    #endif
        btn->pressed=false;

	/* Draw released button */
	tm_delayms(30); /*  hold on for a while before refresh, in case a too fast 'touch_and_release' event! */
  	egi_sbtn_refresh(btn, NULL);

	/* let caller to render */
}



/* ----------------------------------------------------------------------
Draw a water ripple mark.

@alpha:         Alpha value for color blend
@touch_pxy:     Point coordinate
---------------------------------------------------------------------- */
void ripple_mark(EGI_POINT touch_pxy, uint8_t alpha, EGI_16BIT_COLOR color)
{
        int i,k;
        int rad[]={ 20, 30, 40, 50, 60, 70, 80, 100, 120};
        int wid[]={ 7, 7, 6, 6, 5,3,2,1,1};


   for(i=0; i<8; i++) //	i<sizeof(rad)/sizeof(rad[0]); i++)
   {
        /* Turn on FB filo and set map pointer */
        fb_filo_on(&gv_fb_dev);
        gv_fb_dev.map_bk=gv_fb_dev.map_fb;

         for(k=0; k<8; k++) {
	     if(i-k>=0) {
	        draw_blend_filled_annulus(&gv_fb_dev,
        	                    touch_pxy.x, touch_pxy.y,
                	            rad[i-k], wid[i], ((k%8)%2)==0 ? WEGI_COLOR_WHITE : WEGI_COLOR_DARKGRAY, alpha );
				    /* radius, width, color, alpha */
	     }
	}

        tm_delayms(60);
        fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

        /* Turn off FB filo and reset map pointer */
        gv_fb_dev.map_bk=gv_fb_dev.map_buff;
        fb_filo_off(&gv_fb_dev);
   }

}
