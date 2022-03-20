/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test for ads7846 touch screen driver.

Also refer to touchscreen_calib.c


TODO:
1. XXX ABS_X/Y NOT always come as a pair, one of them MAY be missed!?
   Ignore if NOT reach as a pair.
2. The first set of data read from event dev MAY carrys with big error,
   and should be discarded! ???
3. Pick out the MAX. and MIN. data in sampling before averaging.
4. Refer to egi_read_kbdcode(EGI_KBD_STATUS *kstat, const char *kdev)
   consider NON_BLOCK and BLOCK mode.
5. To speed up....

Journal:
2022-02-23: Create this file.
2022-02-24: Adjust factXY.
2022-02-25: Add egi_touch_readXY()
2022-02-28: Add egi_touch_waitPress()
2022-03-01: egi_touch_readXY():  read out pressure.
2022-03-03: Transfer functions to egi_touch.c.
2022-03-05: Test egi_read_kbdcode() for touch data.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <linux/input.h>


#include "egi_debug.h"
#include "egi_fbdev.h"
#include "egi_fbgeom.h"
#include "egi_touch.h"
#include "egi_input.h" /* TEST input */

#if 0 ///////////// Refer to EGI_TOUCH_DATA in egi.h //////////////
typedef struct egi_touch_data
{
        /* need semaphore lock ?????
        TODO:         */

        /* flag, whether the data is updated after read out */
        bool updated;

        /* the last touch status */
        enum egi_touch_status   status;

        /* the latest touched point coordinate */
        //struct egi_point_coord coord;
        EGI_POINT       coord;

        /* the sliding deviation of coordXY from the beginning touch point,
        in LCD coordinate */
        int     dx;
        int     dy;
} EGI_TOUCH_DATA;
enum egi_touch_status
{
        unkown=0,               /* during reading or fails */
        releasing=1,            /* status transforming from pressed_hold to released_hold */
        pressing=2,             /* status transforming from released_hold to pressed_hold */
        released_hold=3,
        pressed_hold=4,
        db_releasing=5,         /* double click, the last releasing */
        db_pressing=6,          /* double click, the last pressing */
        undefined=7,            /* as for limit */
};
#endif //////////////////////////////////////////

EGI_TOUCH_DATA touch;

//static float      factX=1.0;      /* Linear factor:  dLpx/dTpx */
//static float      factY=1.0;      /* Linear factor:  dLpy/dTpy */

static int        Lpx[5]= { 0, 240-1,    0,    240-1, 240/2 };  /* LCD refrence X,  In order of seqnum */
static int        Lpy[5]= { 0,   0,    320-1,  320-1, 320/2 };  /* LCD refrence Y,  In order of seqnum */

//static int        Lpx[5]= { 35, 240-1-35,    35,    240-1-35, 240/2 };  /* LCD refrence X,  In order of seqnum */
//static int        Lpy[5]= { 35,   35,    320-1-35,  320-1-35, 320/2 };  /* LCD refrence Y,  In order of seqnum */

//static uint16_t   Tpx[5],Tpy[5];        /* Raw touch data picked correspondingt to Lpx[], Lpy[] */

uint16_t TpxTest, TpyTest;
uint16_t LpxTest, LpyTest;

void draw_marks(void);
void kbd_input(void);

/*----------------------------
           MAIN()
----------------------------*/
int main(int argc, char **argv)
{
    	int           rc;

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev, false); //true);
        fb_position_rotate(&gv_fb_dev, 1);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);

	//clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);
	/* Draw marsk */
	draw_marks();

	/* TEST: egi_touch_readXYP() ---------------- */
	int press;
	EGI_POINT pt={0,0};
	EGI_POINT pts, pte;
	bool	 press_point=false;


	/* Waiting for press on touchpad */
	egi_touch_waitPress(NULL);
	press_point=true;


#if 0 /* TEST: kbd_input() -------------------- */
	while(1) {
		kbd_input();
		usleep(10000);
	}
#endif

	while(1) {
		 printf("readXY...\n");
		 rc=egi_touch_readXYP(&pt,&press);
		 //rc=egi_touch_readXYP(NULL,&press);
		 if(rc==0) {
			printf("Touch point: (%d,%d)\n", pt.x, pt.y);

			if(!press_point) {
			   pts=pte; pte=pt;
			}
			/* The first pressing point */
			else {
			   pts=pt; pte=pt;
			   press_point=false;
			}

           #if 0	/* Clear screen */
			if( pt.x<20 && pt.y<20) {
				clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY);
				fb_render(&gv_fb_dev);
			}
			else
           #endif
			{
	               		#if 0 /* XY: Draw a circle there */
	        	        fbset_color(WEGI_COLOR_GREEN);
       		        	//draw_circle(&gv_fb_dev, pt.x, pt.y, 6);
				draw_wline(&gv_fb_dev, pts.x, pts.y, pte.x, pte.y, 5);
				#endif

				#if 1 /* PRESSURE: Draw circle at center */
				clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY2);
	        	        fbset_color(WEGI_COLOR_ORANGE);
				//fbset_color(egi_color_random(color_light));
				//draw_circle(&gv_fb_dev, 240/2, 320/2, (press-160)*2); /* Press range: ~150 - ~230 */
				//draw_filled_circle(&gv_fb_dev, 240/2, 320/2, (press-150)*2); /* Press range: ~150 - ~230 */
				int k;
				k=(press-130); //(press-150)*2;  /* Pressure(press) value range: ~150 - ~230 */
				while(k>10) {
				   //draw_circle(&gv_fb_dev, 240/2, 320/2, k); /* Press range: ~150 - ~230 */
				   draw_circle(&gv_fb_dev, pt.x, pt.y, k); /* Press range: ~150 - ~230 */
				   k-=5; //10;
				}

				fb_render(&gv_fb_dev);
				//usleep(5000);
				#endif
			}
		 }
		 else { //if(rc>0) { /* Released */
			printf("Wait for press...\n");
			egi_touch_waitPress(NULL);
			press_point=true;
			//while(egi_touch_waitPress()!=0){};
			printf("press...\n");
		 }
	}

    return 0;
}

/*----------------------------------
Draw marks for 5 caliberation points
-----------------------------------*/
void draw_marks(void)
{
	int r=5;		/* Radium of circle mark */
	int crosslen=18;	/* Length of cross mark */
	int seqnum;

	/* Clear screen first */
	clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY);


	/* Switch seqnum and show Tips */
	fbset_color(WEGI_COLOR_RED);

	/* Draw a circle and a cross */
	for(seqnum=0; seqnum<5; seqnum++)
	{
		draw_circle(&gv_fb_dev, Lpx[seqnum], Lpy[seqnum], r);
		draw_line(&gv_fb_dev, Lpx[seqnum]-crosslen/2, Lpy[seqnum], Lpx[seqnum]+crosslen/2, Lpy[seqnum]);
		draw_line(&gv_fb_dev, Lpx[seqnum], Lpy[seqnum]-crosslen/2, Lpx[seqnum], Lpy[seqnum]+crosslen/2);
	}

	/* Render  */
	//fb_render(&gv_fb_dev);
}


/*--------------------------------------------------------------------
Call egi_read_kbdcode() to read touch data.

Note:
1. ADS7846 reprots event sequence: BTN_TOUCH, ABS_X, ABS_Y, ABS_PRESSURE.
2. kstat ONLY saves the last one of above abskeys/absvalue!
3. BTN_TOUCH=0x14a, it's trimmed to be 0x4a in egi_read_kbdcode()!
4. ABS_PRESSURE absvalue=0 as TOUCH RELEASE

--------------------------------------------------------------------*/
void kbd_input(void)
{
	/* KBD GamePad status */
	static EGI_KBD_STATUS kstat={ .conkeys={.abskey=ABS_MAX} };  /* 0-->ABS_X ! */;

        /* 5.1 Reset lastkey: Let every EV_KEY read ONLY once! */
        kstat.conkeys.lastkey =0;       /* Note: KEY_RESERVED ==0. */
	kstat.conkeys.press_abskey=false;

	/* 1. Read event devs */
 	if( egi_read_kbdcode(&kstat, NULL) )
		return;

	//printf("Get ks=%d\n", kstat.ks); ks is for conkey only!

	 /* absvalue: 0: ABS_X, 1: ABS_Y, ABS_MAX(0x3F): IDLE */

	/* As touch screen event  retport sequence: BTN_TOUCH, ABS_X, ABS_Y, ABS_PRESSURE,
           So the last conkeys.abskey is always ABS_PRESSURE. */
	 switch( kstat.conkeys.abskey ) {
        	case ABS_X:
                	egi_dpstd("ABS_X: %d\n", kstat.conkeys.absvalue);
                        break;
                case ABS_Y:
                        egi_dpstd("ABS_Y: %d\n", kstat.conkeys.absvalue);
                        break;
		case ABS_PRESSURE:
                        egi_dpstd("ABS_PRESSURE: %d\n", kstat.conkeys.absvalue);
                default:
                        break;
	}


	/* NOTICE: kstat abskey/absvalue are NOT buffered! it returns ONLY the last status!
	 * Usually one call of egi_read_kbdcode() will read ABS_X/ABS_Y/ABS_PRESSURE, however ONLY the
	 * last ABS_PRESSURE report will be saved in abskey/absvalue.
	 */

//	printf("kstat.ks=%d\n", kstat.ks);
	if( kstat.ks>0 && kstat.keycode[0] ==(BTN_TOUCH&0xFF) ) { /* BTN_TOUCH=0x14a, keycode trimmed in egi_read_kbdcode! */
	     if(kstat.press[0])
		printf(" ---- BTN_TOUCH down! ----\n");
	}

	if( kstat.conkeys.press_lastkey ) {
		if(kstat.conkeys.lastkey) {
			egi_dpstd("lastkey code: %d\n", kstat.conkeys.lastkey);
		   switch(kstat.conkeys.lastkey) {
			case (BTN_TOUCH & 0xFF): /* BTN_TOUCH=0x14a, keycode trimmed in egi_read_kbdcode! */
				egi_dpstd("BTN_TOUCH down!\n");
				break;
			default:
				break;
		   }
		}
	}

	/* 5. Reset lastkey/abskey keys if necessary, otherwise their values are remained in kstat.conkey. */
	/* 5.1 Reset lastkey: Let every EV_KEY read ONLY once! */
       	kstat.conkeys.lastkey =0;       /* Note: KEY_RESERVED ==0. */

      	/* 5.2 Reset abskey XXX NOPE, we need ABSKEY repeating function. See enbale_repeat. */
	if( kstat.conkeys.press_abskey )
		kstat.conkeys.press_abskey=false;
	kstat.conkeys.abskey =ABS_MAX; /* NOTE: ABS_X==0!, use ABS_MAX as idle. */

}
