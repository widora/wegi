/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test for adsXXX touch driver.

Also Refer to touchscreen_calib.c



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

Midas Zhou
midaszhou@yahoo.com
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


#define INPUT_TOUCH_DEVNAME "/dev/input/event0"

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

        /* the sliding deviation of coordXY from the beginnig touch point,
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

int egi_touch_waitPress(void);
int egi_touch_readXY(EGI_POINT *pt);


/*----------------------------
           MAIN()
----------------------------*/
int main(int argc, char **argv)
{
    	int           rc;

#if 0
	char          name[64];           /* RATS: Use ok, but could be better */
   	int           fd = 0;
    	char          *tmp;
	int 	      cnt=0;
#endif
#if 0
	struct input_event event;

	const int     ns=16;	/* samples */
	int 	      ni=0;	/* sample index */
	int	      sumTX=0, sumTY=0; /* sum up all raw X/Y as per ns */
	int           tmpX,tmpY;
#endif

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev, true);
        fb_position_rotate(&gv_fb_dev, 1);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);

	//clear_screen(&gv_fb_dev, WEGI_COLOR_GRAY);
	/* Draw marsk */
	draw_marks();


/* TEST: egi_touch_readXY() ---------------- */
	EGI_POINT pt={0,0};

	egi_touch_waitPress(); /* Waiting for press on touchpad */
	while(1) {
		 printf("readXY...\n");
		 rc=egi_touch_readXY(&pt);
		 if(rc==0) {
			printf("Touch point: (%d,%d)\n", pt.x, pt.y);

			/* Clear screen */
			if( pt.x<20 && pt.y<20) {
				clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY);
				fb_render(&gv_fb_dev);
			}
			else {
	               		/* Draw a circle there */
	        	        fbset_color(WEGI_COLOR_GREEN);
       		        	draw_circle(&gv_fb_dev, pt.x, pt.y, 10);
			}
		 }
		 else { //if(rc>0) { /* Released */
			printf("Wait for press...\n");
			while(egi_touch_waitPress()!=0){};
			printf("press...\n");
		 }
	}

exit(0);
///////////////////////////////////////////////////

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


///////////////////////////////////////////////////



/*-----------------------------------------------------------
Wait for touch press in BLOCK mode.

Note:
1. !!! ----- CAUTION ----- !!!
   fd_touch MUST be open each time before calliing select()!
   If the touch device has other file descriptor refering
   to it, then it will affect select() behavour! ???


Return:
	0	OK
	<0	Fails
-----------------------------------------------------------*/
int egi_touch_waitPress(void)
{
	int fd_touch; 	/* File descriptor for touch event device */
	struct timeval tmout;
        fd_set rfds;
	int res;
	int ret=0;

	/* Open input touch device each time before calling select() */
	//if(fd_touch<1) {
            if((fd_touch = open(INPUT_TOUCH_DEVNAME, O_RDWR|O_CLOEXEC, 0))<0)
		return -1;
	//}

        /* Select and read fd[] */
        FD_ZERO(&rfds);
	FD_SET(fd_touch, &rfds);

	/* Reset timeout value each time before calling select */
        //tmout.tv_sec=0;
        //tmout.tv_usec=10000;
        res=select(fd_touch+1, &rfds, NULL, NULL, NULL); //&tmout); /* Block forever */
	if(res<0) {
		egi_dperr("Fail to select fd_touch");
		ret=-2;
	}
	else if(res==0) { /* Impossible when timeout set as NULL ?? */
		egi_dpstd("Select res=0!\n");
		ret=-3;
	}
	//else
	//	egi_dpstd("res=%d\n", res);

	/* Close fd_touch */
	close(fd_touch);

	return ret;
}

/*-------------------------------------------------------------

Tpxy[0]:(349,209) Tpxy[1]:(3522,210) Tpxy[2]:(542,3459) Tpxy[3]:(3482,3687) Tpxy[4]:(2113,2074)

Lpx[]={ (333,188), (3750,188), (295,3695), (3830,3675), (2080,1965) }

factX= 1.0*((Lpx[1]-Lpx[0] + Lpx[3]-Lpx[2])/2) / ((Tpx[1]-Tpx[0] + Tpx[3]-Tpx[2])/2);
factY= 1.0*((Lpy[2]-Lpy[0] + Lpy[3]-Lpy[1])/2) / ((Tpy[2]-Tpy[0] + Tpy[3]-Tpy[1])/2);

factX=1.0*240/(((3750-333)+(3830-295))/2)=0.0690;
factY=1.0*320/(((3695-188)+(3675-188))/2)=0.0915;
Center: (2080,1965)

Note:
1. If 'TOUCH presess': then initiate/clear all vars, in case there are old data
   still remained in file buffer.
2. If 'TOUCH release': then close(fd) and return 1;


Return:
	0	OK
	<0 	Fails
	>0(1)   Touch release
-------------------------------------------------------------*/
int egi_touch_readXY(EGI_POINT *pt)
{
	static int fd=-1;
	int rc;
	struct input_event event;
	const int     np=1;
	const int     ns=(1<<np); /* samples */

	/* ni, sumTX/TY, tmpX/Y to init/clear in case BTN_TOUCH */
	int 	      ni=0;		/* sample index */
	int	      sumTX=0, sumTY=0; /* sum up all raw X/Y as per ns */
	int           tmpX=-1,tmpY=-1;  /* <0 as invalid */

	static bool   Tpressing=false;   /* the first press point */
	EGI_TOUCH_DATA touch;

	/* Conversion factors */
	const float factX=0.0690;
	const float factY=0.0915;
	const int   baseX=2080, baseY=1965; /* TouchPad Center BASE */

	/* Open input touch device */
	if(fd<0) {
            if((fd = open(INPUT_TOUCH_DEVNAME, O_RDWR|O_NONBLOCK|O_CLOEXEC, 0))<0)
		return -1;
	}


	/* Read ns samples */
        //while ((rc = read(fd, &event, sizeof(event))) > 0) {
	while(1) {
		rc=read(fd, &event, sizeof(event));

		/* Break if read error */
		if( rc<0 ) {
			if(errno!=EAGAIN) {
			   printf("Read ERROR! rc=%d\n", rc);
/* TEST: -------------- */
			   close(fd); fd=-1;
			   break;
			}
			else {   /* errno==EAGAIN */
//			   printf("read EAGAIN!\n");
			   continue;
			}
		}

#if 0 /* TEST: ------------------------- */
            printf("%-24.24s.%06lu type 0x%04x; code 0x%04x;"
                " value 0x%08x; ",
        	ctime(&event.time.tv_sec),
                event.time.tv_usec,
                event.type, event.code, event.value);
#endif

           switch (event.type) {
		/* Touch Press/release event */
		case EV_KEY:
		    switch( event.code ) {
		       case BTN_TOUCH:    /* BTN_TOUCH: 0x14a */
			  if(event.value) {
			        printf(" >>>>> TOUCH press \n");
				Tpressing=true;

				/* Init and clear. In case there are old data in event buffer before the TOUCH... */
				ni=0; tmpX=-1; tmpY=-1;
				sumTX=0; sumTY=0;

				/* TODO Discard first set of data ??? */
				//read(fd, &event, sizeof(event));
			  }
			  else {
			        printf(" TOUCH release <<<<<\n");
				/* Empty all data */
				//read(fd, &event, sizeof(event)); //NO WAY! nonblocking mode!
				//while( read(fd, &event, sizeof(event))>0 ) {}

				close(fd);
				return 1;
				//Tpressing=false;
			  }

			  /* Update touch status */
//			  touch.status= event.value ? pressing : releasing;

		          break;
		    }
		    break;
		  /* Touch position/pressure abs. value */
                  case EV_ABS:
                    switch (event.code) {
                       case ABS_X:
				/* update tmpX */
				tmpX=event.value;
				break;
                       case ABS_Y:
				/* update tmpY */
				tmpY=event.value;
			        break;
                       case ABS_PRESSURE:  break;
                       default:            break;
                    }

		    /* Update touch status */
		    //touch.status=pressed_hold;

                    break;
	   }

	   /* Assume ABS_X ALWAYS preceds ABS_Y and come as a pair!!!  Ignore invalid data. */
	   if(tmpY>=0 && tmpX<0) {
		printf(" ---- X Missing ----\n");
		tmpY = -1; /* Reset Y */
		continue;
	   }
	   /* Sum up XY. ONLY both ABS_X and ABS_Y events have been received.  */
	   else if(ni<ns && tmpX>=0 && tmpY>=0 ) {
		sumTX += tmpX; //(tmpX>>4);
		sumTY += tmpY; //(tmpY>>4);

		printf("tmpXY[%d]: %d, %d\n", ni, tmpX, tmpY);

		tmpX=-1; tmpY=-1; /* reset */

		ni++; /* Sampling increment */
//	   	printf(": ni=%d, sumTX=%d, sumTY=%d\n", ni, sumTX, sumTY);
	   }

	   /* Checking (ns) samplings to average for each PXY. */
	   if(ni==ns)
		break;

	} /* END While() */

	/* If while breaks unexpectedly. ni!=ns then. */
	if(rc<0) {
//		printf("Read touch fails! rc=%d\n", rc);
		close(fd);
		return -3;
	}

	/* Compute XY,  TODO: map to touch_pad coord. */
	if(pt) {
	   //pt->x = sumTX/ns;
	   //pt->y = sumTY/ns;

	   sumTX >>=np;  /* Average TX/TY */
	   sumTY >>=np;

	   /* Convert to get Pxy */
           pt->x=(sumTX-baseX)*factX + 240/2;
           pt->y=(sumTY-baseY)*factY + 320/2;
	}

/* TEST: ------------- */
       	//close(fd);

	return 0;
}
