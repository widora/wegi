/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


	   (linux/input.h)
more details: https://www.kernel.org/doc/html/latest/input/input-programming.html

struct input_event {
        struct timeval time;
        __u16 type;
        __u16 code;
        __s32 value;
};
   	--- Event types ---
#define EV_SYN                  0x00
#define EV_KEY                  0x01
#define EV_REL                  0x02  -- Relative value
#define EV_ABS                  0x03  -- Absolute value
#define EV_MSC                  0x04
#define EV_SW                   0x05
#define EV_LED                  0x11
#define EV_SND                  0x12
#define EV_REP                  0x14
#define EV_FF                   0x15
#define EV_PWR                  0x16
#define EV_FF_STATUS            0x17
#define EV_MAX                  0x1f
#define EV_CNT                  (EV_MAX+1)

   	--- Relative Axes ---
#define REL_X                   0x00
#define REL_Y                   0x01
#define REL_Z                   0x02
#define REL_RX                  0x03
#define REL_RY                  0x04
#define REL_RZ                  0x05
#define REL_HWHEEL              0x06
#define REL_DIAL                0x07
#define REL_WHEEL               0x08
#define REL_MISC                0x09
#define REL_MAX                 0x0f
#define REL_CNT                 (REL_MAX+1)

	--- Button Value ---
...
#define BTN_MOUSE               0x110
#define BTN_LEFT                0x110
#define BTN_RIGHT               0x111
#define BTN_MIDDLE              0x112
#define BTN_SIDE                0x113
#define BTN_EXTRA               0x114
#define BTN_FORWARD             0x115
#define BTN_BACK                0x116
#define BTN_TASK                0x117


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


static int mouseX, mouseY;
static bool pen_down;
static bool clear_bkgbuff;
static bool last_pen_down;

void mouse_callback(struct input_event *inevent);
void draw_mcursor(int x, int y);
EGI_POINT ptstart, ptend;

int main(void)
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
  fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */



                  /*-----------------------------------
                   *            Main Program
                   -----------------------------------*/
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);

        /* Put to FBDEV_BKG_BUFF */
        fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */


#if 0  /*  -------- TEST:  start,end_inputread() -------- */
   while(1) {
	printf("start input read...\n");
	egi_start_inputread("/dev/input/event0");
	sleep(2);
	printf("end input read...\n");
	egi_end_inputread();
   }
#endif


	/* Set mouse callback function */
	egi_input_setCallback(mouse_callback);

	/* Start inputread loop */
	egi_start_inputread("/dev/input/event0");


#if 1  /*  Put following codes to mouse_callback() only if eventX is NOT mapped to /dev/input/mice or /dev/input/mouse */
	while(1) {
		if(clear_bkgbuff)
			fb_clear_bkgBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
        	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

		/* TODO: put to callback to save time */
		if(pen_down) {
			if(last_pen_down==false) {
				ptend.x=mouseX; ptend.y=mouseY;
				ptstart.x=mouseX; ptstart.y=mouseY;
			} else {
				ptstart.x=ptend.x; ptstart.y=ptend.y;
				ptend.x=mouseX; ptend.y=mouseY;
			}
			last_pen_down=true;

			fbset_color(WEGI_COLOR_RED);
			//draw_circle(&gv_fb_dev, mouseX, mouseY, 3);
			draw_wline(&gv_fb_dev, ptstart.x, ptstart.y, ptend.x, ptend.y, 7);
	        	/* Put to FBDEV_BKG_BUFF */
        		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
		}
		else if(last_pen_down==true)
			last_pen_down=false;

		draw_mcursor(mouseX, mouseY);
		fb_render(&gv_fb_dev);
	}
#endif
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
	Callback for input event
---------------------------------------------*/
void mouse_callback(struct input_event *inevent)
{
	/* Parse input code and value */
        switch(inevent->type)
        {
        	case EV_KEY:
                	switch(inevent->code)
                        {
                        	case BTN_LEFT:
                			printf("BTN_LEFT\n");
					if(inevent->value) {
	                                	//printf("Left key pressed!\n");
						pen_down=true;
					}
					else {
						//printf("Left key released!\n");
						pen_down=false;
					}
                                        break;
                                case BTN_RIGHT:
                			printf("BTN_RIGHT\n");
					if(inevent->value) {
                                        	//printf("Right key pressed\n");
						/* Clear FB working buffer */
						clear_bkgbuff=true;
					}
					else
						clear_bkgbuff=false;
                                        break;
                                case BTN_MIDDLE:
                			printf("BTN_MIDDLE\n");
                                        break;
                                default:
                                        printf("Key 0x%03x pressed\n", inevent->code);
                                        break;
                        }
                        break;
                 case EV_REL:
                 	switch(inevent->code)
                        {
                        	case REL_X:     /* Minus for left movement, Plus for right movement */
                                	printf("slip dX value: %d\n", inevent->value);
					mouseX+=inevent->value;
					if(mouseX > gv_fb_dev.pos_xres -5)
						mouseX=gv_fb_dev.pos_xres -5;
					else if(mouseX<0)
						mouseX=0;
                                        break;
                                case REL_Y:     /* Minus for up movement, Plus for down movement */
                                        printf("slip dY value: %d\n", inevent->value);
					mouseY+=inevent->value;
					if(mouseY > gv_fb_dev.pos_yres -5)
						mouseY=gv_fb_dev.pos_yres -5;
					else if(mouseY<0)
						mouseY=0;
                                        break;
				case REL_Z:
					printf("wheel dZ %d\n", inevent->value);
					break;

                         }
                         break;
	        default:
        		//printf("EV type: 0x%02x\n", inevent->type);
                        break;
	}


#if 0	/* ---- Actions drived by mouse : !!! sluggish if eventX is mapped to /dev/input/mouse or /dev/input/mice !!! ---- */
	if(clear_bkgbuff)
		fb_clear_bkgBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
       	fb_copy_FBbuffer(&gv_fb_dev, FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);  /* fb_dev, from_numpg, to_numpg */

	if(pen_down) {
		if(last_pen_down==false) {
			ptend.x=mouseX; ptend.y=mouseY;
			ptstart.x=mouseX; ptstart.y=mouseY;
		} else {
			ptstart.x=ptend.x; ptstart.y=ptend.y;
			ptend.x=mouseX; ptend.y=mouseY;
		}
		last_pen_down=true;

		fbset_color(WEGI_COLOR_RED);
		//draw_circle(&gv_fb_dev, mouseX, mouseY, 3);
		draw_wline(&gv_fb_dev, ptstart.x, ptstart.y, ptend.x, ptend.y, 7);
        	/* Put to FBDEV_BKG_BUFF */
       		fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);  /* fb_dev, from_numpg, to_numpg */
	}
	else if(last_pen_down==true)
		last_pen_down=false;

	draw_mcursor(mouseX, mouseY);
	fb_render(&gv_fb_dev);
#endif


}


/*--------------------------------
 Draw cursor for the mouse movement
----------------------------------*/
void draw_mcursor(int x, int y)
{
	static EGI_IMGBUF *mcimg=NULL;
	if(mcimg==NULL)
		mcimg=egi_imgbuf_readfile("/mmc/mcursor.png");

	/* OPTION 1: Use EGI_IMGBUF */
	if(mcimg)
		egi_subimg_writeFB(mcimg, &gv_fb_dev, 0, -1, mouseX,mouseY ); /* subnum,subcolor,x0,y0 */


	/* OPTION 2: Draw geometry */


}
