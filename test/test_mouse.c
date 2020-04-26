/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to read mice data and move the mouse cursor accordingly.


Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <linux/input.h>
#include "egi_common.h"
#include "egi_input.h"
#include "egi_FTsymbol.h"


EGI_POINT ptstart, ptend;	/* line start/end points */

static int mouseX;
static int mouseY;
static int mouseZ;

static bool LeftKey_down;	/* Mouse left key hold_down */
static bool clear_bkgbuff;	/* Mouse Rigth key down to clear background image */
static bool last_LeftKey_down;	/* To remember whether left key was down the last time */

static void mouse_callback(unsigned char *mouse_data, int size); /* Mouse driven actions */
static void draw_mcursor(int x, int y);				 /* Draw the mouse cursor */


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
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;
  #endif

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  if(argc > 1)
	fb_position_rotate(&gv_fb_dev,atoi(argv[1]));
  else
  	fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/

	/* Init. mouse position */
	mouseX=gv_fb_dev.pos_xres/2;
	mouseY=gv_fb_dev.pos_yres/2;

	/* Init. FB working buffer */
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */


#if 0  /*  -------- TEST:  start,end_inputread() -------- */
   while(1) {
	printf("start mouse read...\n");
	egi_mouse_setCallback(mouse_callback);
	egi_start_mouseread("/dev/input/mice");
	egi_sleep(2, 2, 0);
	printf("end mouse read...\n");
	egi_end_mouseread();
	egi_sleep(2, 1, 0);
   }
#endif

	/* Set mouse callback function */
	egi_mouse_setCallback(mouse_callback);

	/* Start inputread loop */
	egi_start_mouseread("/dev/input/mice"); //mouse0");

	while(1) {
		egi_sleep(1,1,0);
	}

                  /*-----------------------------------
                   *         End of Main Program
                   -----------------------------------*/


 /* <<<<<  EGI general release   >>>>>> */
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


/*--------------------------------------------
	Callback for mouse input
---------------------------------------------*/
static void mouse_callback(unsigned char *mouse_data, int size)
{
	/* 1. Check pressed key */
	if(mouse_data[0]&0x1) {
		printf("Leftkey down!\n");
		LeftKey_down=true;
	} else
		LeftKey_down=false;

	if(mouse_data[0]&0x2) {
		printf("Right key down!\n");
                //RightKey_down=true;
		clear_bkgbuff=true;
        } else
		clear_bkgbuff=false;

	if(mouse_data[0]&0x4) {
		printf("Mid key down!\n");
                //MidKey_down=true;
        }

	/*  2. Get mouse X */
	mouseX += ( (mouse_data[0]&0x10) ? mouse_data[1]-256 : mouse_data[1] );
	if(mouseX > gv_fb_dev.pos_xres -5)
        	mouseX=gv_fb_dev.pos_xres -5;
        else if(mouseX<0)
        	mouseX=0;

	/* 3. Get mouse Y: Notice LCD Y direction!  Minus for down movement, Plus for up movement!
	 * !!! For eventX: Minus for up movement, Plus for down movement!
	 */
	mouseY -= ( (mouse_data[0]&0x20) ? mouse_data[2]-256 : mouse_data[2] );
	if(mouseY > gv_fb_dev.pos_yres -5)
        	mouseY=gv_fb_dev.pos_yres -5;
        else if(mouseY<0)
        	mouseY=0;

	/* 4. Get mouse Z */
	mouseZ += ( (mouse_data[3]&0x80) ? mouse_data[3]-256 : mouse_data[3] );
	//printf("get X=%d, Y=%d, Z=%d \n", mouseX, mouseY, mouseZ);


	/* --- 5. Actions driven by mouse --- */
	if(clear_bkgbuff)
		fb_clear_bkgBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
       	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

	if(LeftKey_down) {
		if(last_LeftKey_down==false) {  /* init pen position */
			ptend.x=mouseX; ptend.y=mouseY;
			ptstart.x=mouseX; ptstart.y=mouseY;
		} else {
			ptstart.x=ptend.x; ptstart.y=ptend.y;
			ptend.x=mouseX; ptend.y=mouseY;
		}
		last_LeftKey_down=true;
		fbset_color(WEGI_COLOR_RED);
		//draw_circle(&gv_fb_dev, mouseX, mouseY, 3);
		draw_wline(&gv_fb_dev, ptstart.x, ptstart.y, ptend.x, ptend.y, 7);
        	/* Put to FBDEV_BKG_BUFF */
       		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	}
	else if(last_LeftKey_down==true) {
		last_LeftKey_down=false;
	}

	draw_mcursor(mouseX, mouseY);
	fb_render(&gv_fb_dev);
}


/*--------------------------------
 Draw cursor for the mouse movement
----------------------------------*/
static void draw_mcursor(int x, int y)
{
	static EGI_IMGBUF *mcimg=NULL;
	if(mcimg==NULL)
		mcimg=egi_imgbuf_readfile("/mmc/mcursor.png");

	/* OPTION 1: Use EGI_IMGBUF */
	if(mcimg)
		egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, mouseX, mouseY ); /* subnum,subcolor,x0,y0 */

	/* OPTION 2: Draw geometry */

}

