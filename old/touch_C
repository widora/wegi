
/*---------------------------------------------------------------
0. TOUCH Y edge doesn't have good homogeneous property along X, 
   try to draw a horizontal line and check visually, it will bend.

1. if sx0=sy0=0, it means that last TOUCH coordinate is invalid.

1. Linear piont to piont mapping from TOUCH to LCD. There are
   unmapped points on LCD.

2. point to an oval area mapping form TOUCH to LCD.


Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include "spi.h"
#include "fblines.h"

/*--------------   8BITS CONTROL FOR XPT2046   -------------------
[7] 	S 		-- 1:  new control bits,
	     		   0:  ignore data on pins

[6-4]   A2-A0 		-- 101: X-position
		   	   001: Y-position

[3]	MODE 		-- 1:  8bits resolution
		 	   0:  12bits resolution

[2]	SER/(-)DFR	-- 1:  normal mode
			   0:  differential mode

[1-0]	PD1-PD0		-- 11: normal power
			   00: power saving mode
----------------------------------------------------------------*/
#define XPT_CMD_READXP	0xD0 //0xD0 //1101,0000  /* read X position data */
#define XPT_CMD_READYP	0x90 //0x90 //1001,0000 /* read Y position data */


#define XPT_XP_MIN 7  //set limit value for X,Y
#define XPT_XP_MAX 116 //actual 116
#define XPT_YP_MIN 17
#define XPT_YP_MAX 116  //actual 116
#define LCD_SIZE_X 240
#define LCD_SIZE_Y 320


/*-------------------  MAIN  ---------------------*/
int main(void)
{
	int k,j;
	uint8_t  xp[2],yp[2];
	int sx,sy;  //current TOUCH point coordinate, it's a LCD screen coordinates derived from TOUCH coordinate.
	int sx0=0,sy0=0;//the last TOUCH point coordinate, for comparison.
	uint8_t cmd;
        FBDEV     fr_dev;

	/* open spi dev */
	SPI_Open();
	/* prepare fb device */
        fr_dev.fdfd=-1;
        init_dev(&fr_dev);


	/*------- test  squares ------*/
	fbset_color((30<<11)|(10<<5)|10);//R5-G6-B5
	draw_rect(&fr_dev,40,20,80,60);
	fbset_color((10<<11)|(50<<5)|10);//R5-G6-B5
	draw_rect(&fr_dev,40+60,20,80+60,60);
	fbset_color((10<<11)|(10<<5)|30);//R5-G6-B5
	draw_rect(&fr_dev,40+120,20,80+120,60);


	while(1)
	{

	    usleep(3000);  //small value increase width of pen

	    cmd=XPT_CMD_READXP;
	    SPI_Write_then_Read(&cmd, 1, xp, 2);
	    cmd=XPT_CMD_READYP;
	    SPI_Write_then_Read(&cmd, 1, yp, 2);

	   /*  valify data */
	    if(xp[0]){
	    	    printf("Xp[0]=%d\n",xp[0]); //untoched: Xp[0]=0, Xp[1]=0,
	    }
	    else{  /* meanless xp, or pen_up */
		/* reset sx0,sy0 */
		sx0=0;sy0=0;
		continue;
	   }

	    if(yp[0]<117){
 	     	printf("Yp[0]=%d\n",yp[0]); //untoched: Yp[0]=127=[2b0111,1111] ,Yp[1]=248=[2b1111,1100]
	    }
	    else  /* meanless yp, or pen_up */
	    {
		  yp[0]=0;
		  /* reset sx0,sy0 */
		  sx0=0;sy0=0;
		  continue;
	     }

	    /* normalize TOUCH pad x,y data */
	    if(xp[0]<XPT_XP_MIN)xp[0]=XPT_XP_MIN;
	    if(xp[0]>XPT_XP_MAX)xp[0]=XPT_XP_MAX;
	    if(yp[0]<XPT_YP_MIN)yp[0]=XPT_YP_MIN;
	    if(yp[0]>XPT_YP_MAX)yp[0]=XPT_YP_MAX;


	    /* ---- calculate LCD screen coordinate --- */
	    /* Warning: Different range of LCD_SIZE and XPT_XP(YP) cause point lost during transform calculation */
	    sx=LCD_SIZE_X*(xp[0]-XPT_XP_MIN)/(XPT_XP_MAX-XPT_XP_MIN+1);
	    sy=LCD_SIZE_Y*(yp[0]-XPT_YP_MIN)/(XPT_YP_MAX-XPT_YP_MIN+1);
//	    printf("xp=%d  sx=%d;  yp=%d  sy=%d \n",xp[0],sx,yp[0],sy);

	    /*  restore data */
	    if(sx0==0 || sy0==0){
		sx0=sx;
		sy0=sy;
	    }


	    /* ---- if touch color palette ----- */
	    if(point_inbox(sx,sy,40,20,80,60)){
	    	fbset_color((30<<11)|(10<<5)|10);//R5-G6-B5
		printf("pick pink\n");
		continue;
	    }
	    else if(point_inbox(sx,sy,40+60,20,80+60,60)){
		fbset_color((10<<11)|(50<<5)|10);//R5-G6-B5
		printf("pick green\n");
		continue;
	    }
	    else if(point_inbox(sx,sy,40+120,20,80+120,60)){
	    	fbset_color((10<<11)|(10<<5)|30);//R5-G6-B5
		printf("pick blue\n");
		continue;
	    }


	    /* ignore uncontinous points */
	    // too small value will cause more breaks and gaps
	    if( abs(sx-sx0)>3 || abs(sy-sy0)>3 ) {
		  /* reset sx0,sy0 */
		  sx0=0;sy0=0;
  		  continue;
	    }

	    /* ignore repeated points */
	    if(sx0==sx && sy0==sy)continue;


	    /*  ---------  TOUCH to LCD mapping  ---------- */


	    /*  ---Method 1. draw point---  */
//	    printf("sx=%d, sy=%d\n",sx,sy);
	    /*  --- valid range of sx,sy ---- */
	    if(sx<1+1 || sx>240-1 || sy<1+1 || sy>320-1)continue;
	    /* linear expand sx, from TOUCH 1-120 to LCD 1-240 */
//	    draw_dot(&fr_dev,sx,sy);draw_dot(&fr_dev,sx-1,sy);draw_dot(&fr_dev,sx+1,sy);
	    /* linear expand sy, from TOUCH 1-107 to LCD 1-320 */
//	    draw_dot(&fr_dev,sx,sy-1);draw_dot(&fr_dev,sx,sy);draw_dot(&fr_dev,sx,sy+1);

	    /*  ---Method 2. draw oval---  */
	    draw_oval(&fr_dev,sx,sy);

	    /*  ---Method 3. draw rectangle---  */
//    	    draw_rect(&fr_dev,sx-3,sy-1,sx,sy+2); //2*4 rect


	    /*  draw line */
//	    draw_line(&fr_dev,sx0,sy0,sx,sy);

	    /* update sx0,sy0 */
	    sx0=sx;sy0=sy;
	}

	/* close fb dev */
        munmap(fr_dev.map_fb,fr_dev.screensize);
        close(fr_dev.fdfd);

	/* close spi dev */
	SPI_Close();
	return 0;
}
