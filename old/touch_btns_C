/*-----------------------  touch_btns.c ------------------------------
1. Only first byte read from XPT is meaningful !!!??

2. Keep enough time-gap for XPT to prepare data in each read-seesion,
usleep 3000us seems OK, or the data will be too scattered.

3. Hold LCD-Touch modle carefully, it may influence touch-pad read data.

4. TOUCH Y edge doesn't have good homogeneous property along X,
   try to draw a horizontal line and check visually, it will bend.

5. if sx0=sy0=0, it means that last TOUCH coordinate is invalid.

6. Linear piont to piont mapping from TOUCH to LCD. There are
   unmapped points on LCD.

7. point to an oval area mapping form TOUCH to LCD.


Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include "spi.h"
#include "fblines.h"
#include "egi.h"



/*--------------   8BITS CONTROL COMMAND FOR XPT2046   -------------------
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
---------------------------------------------------------------*/
#define XPT_CMD_READXP	0xD0 //0xD0 //1101,0000  /* read X position data */
#define XPT_CMD_READYP	0x90 //0x90 //1001,0000 /* read Y position data */

/* ----- XPT bias and limit value ----- */
#define XPT_XP_MIN 7
#define XPT_XP_MAX 116 //actual 116
#define XPT_YP_MIN 17
#define XPT_YP_MAX 116  //actual 116

/* ------ touch read sample number ------*/
#define XPT_SAMPLE_NUMBER 2*2*2*2  /* sample for each touch-read session */
#define XPT_SAMPLE_EXPNUM 4  /* 2^4=2*2*2*2 */

/* ----- LCD parameters ----- */
#define LCD_SIZE_X 240
#define LCD_SIZE_Y 320



/*---------------------------------------------------------------
read XPT touching coordinates, and normalize it.
*x --- 2bytes value
*y --- 2bytes value
return:
	0 	Ok
	<0 	pad untouched or invalid value

Note:
	1. Only first byte read from XPT is meaningful !!!??
---------------------------------------------------------------*/
int xpt_read_xy(uint8_t *xp, uint8_t *yp)
{
	uint8_t cmd;

	/* poll to get XPT touching coordinate */
	cmd=XPT_CMD_READXP;
	SPI_Write_then_Read(&cmd, 1, xp, 2); /* return 2bytes valid X value */
	cmd=XPT_CMD_READYP;
	SPI_Write_then_Read(&cmd, 1, yp, 2); /* return 2byte valid Y value */

	/*  valify data,
		when untouched: Xp[0]=0, Xp[1]=0
		when untouched: Yp[0]=127=[2b0111,1111] ,Yp[1]=248=[2b1111,1100]
	*/
	if(xp[0]>0 && yp[0]<127)
	{
   		//printf("xpt_read_xy(): Xp[0]=%d, Yp[0]=%d\n",xp[0],yp[0]);

		/* normalize TOUCH pad x,y data */
		if(xp[0]<XPT_XP_MIN)xp[0]=XPT_XP_MIN;
		if(xp[0]>XPT_XP_MAX)xp[0]=XPT_XP_MAX;
		if(yp[0]<XPT_YP_MIN)yp[0]=XPT_YP_MIN;
		if(yp[0]>XPT_YP_MAX)yp[0]=XPT_YP_MAX;

		return 0;
    	}
	else
	{  /* meanless xp, or pen_up */
		*xp=0;*(xp+1)=0;
		*yp=0;*(yp+1)=0;

		return -1;
	}
}

/*------------------------------------------------------------------------------
convert XPT coordinates to LCD coodrinates
xp,yp:   XPT touch pad coordinates (uint8_t)
xs,ys:   LCD coodrinates (uint16_t)

NOTE:
1. Because of different resolustion(value range), mapping XPT point to LCD point is
actually not one to one, but one to several points. however, we still keep one to
one mapping here.
--------------------------------------------------------------------------------*/
void xpt_maplcd_xy(const uint8_t *xp, const uint8_t *yp, uint16_t *xs, uint16_t *ys)
{
	*xs=LCD_SIZE_X*(xp[0]-XPT_XP_MIN)/(XPT_XP_MAX-XPT_XP_MIN+1);
	*ys=LCD_SIZE_Y*(yp[0]-XPT_YP_MIN)/(XPT_YP_MAX-XPT_YP_MIN+1);
}


/*-------------------  MAIN  ---------------------*/
int main(void)
{
	int i,j,k;
	int index;
	/*-------------------------------------------------
	 native XPT touch pad coordinate value
	!!!!!!!! WARNING: use xp[0] and yp[0] only !!!!!
	---------------------------------------------------*/
	uint8_t  xp[2]; /* when untoched: Xp[0]=0, Xp[1]=0 */
	uint8_t  yp[2]; /* untoched: Yp[0]=127=[2b0111,1111] ,Yp[1]=248=[2b1111,1100]  */
	uint8_t	 xyp_samples[2][10]; /* store 10 samples of x,y */
	int xp_accum; /* accumulator of xp */
	int yp_accum; /* accumulator of yp */
	int nsample=0; /* number of samples for each read session */

	/* LCD coordinate value */
	uint16_t sx,sy;  //current TOUCH point coordinate, it's a LCD screen coordinates derived from TOUCH coordinate.
	uint16_t sx0=0,sy0=0;//saved last TOUCH point coordinate, for comparison.

	//uint8_t cmd;
        FBDEV     fr_dev;


	/* buttons array param */
	int nrow=3; /* number of buttons at Y directions */
	int ncolumn=4; /* number of buttons at X directions */
	struct egi_element_box ebox[nrow*ncolumn]; /* square boxes for buttons */
	int startX=0;  /* start X of eboxes array */
	int startY=140; /* start Y of eboxes array */
	int sbox=45; /* side length of the square box */
	int sgap=(LCD_SIZE_X - ncolumn*sbox)/(ncolumn+1); /* gaps between boxes(bottons) */


	char str_syscmd[100];


	/* open spi dev */
	SPI_Open();
	/* prepare fb device */
        fr_dev.fdfd=-1;
        init_dev(&fr_dev);

	/*------- test  squares ------*/
/*
	fbset_color((30<<11)|(10<<5)|10);//R5-G6-B5
	draw_filled_rect(&fr_dev,40,20,80,60);
	fbset_color((10<<11)|(50<<5)|10);//R5-G6-B5
	draw_filled_rect(&fr_dev,40+60,20,80+60,60);
	fbset_color((10<<11)|(10<<5)|30);//R5-G6-B5
	draw_filled_rect(&fr_dev,40+120,20,80+120,60);
*/

	/* ----- generate and draw eboxes ----- */
	for(i=0;i<nrow;i++) /* row */
	{
		fbset_color( (30-i*5)<<11 | (50-i*8)<<5 | (i+1)*10 );//R5-G6-B5
		for(j=0;j<ncolumn;j++) /* column */
		{
			/* generate boxes */
			ebox[ncolumn*i+j].height=sbox;
			ebox[ncolumn*i+j].width=sbox;
			ebox[ncolumn*i+j].x0=startX+(j+1)*sgap+j*sbox;
			ebox[ncolumn*i+j].y0=startY+i*(sgap+sbox);
			/* draw the boxes */
//			draw_filled_rect(&fr_dev,ebox[ncolumn*i+j].x0,ebox[ncolumn*i+j].y0,\
//					ebox[ncolumn*i+j].x0+sbox,ebox[ncolumn*i+j].y0+sbox);
		}
	}
	/* draw the boxes */
	for(i=0;i<nrow*ncolumn;i++)
		draw_filled_rect(&fr_dev,ebox[i].x0,ebox[i].y0,\
			ebox[i].x0+ebox[i].width,ebox[i].y0+ebox[i].height);


	for(i=0;i<nrow*ncolumn;i++)
		printf("ebox[%d]: x0=%d, y0=%d\n",i,ebox[i].x0,ebox[i].y0);


        fbset_color((30<<11)|(10<<5)|10);//R5-G6-B5

	while(1)
	{
		/*------ relate with number of checking points -----*/
		usleep(3000); //3000

		/*--------- read XPT to get touch-pad coordinate --------*/
		if( xpt_read_xy(xp,yp)!=0 ) /* if read XPT fails,break or pen-up */
		{
			/* reset sx0,sy0 if there is a break or pen-up */
			sx0=0;sy0=0;
			/* reset nsample and accumulator then */
			nsample=0;
			xp_accum=0;yp_accum=0;
			continue;
		}
		else
		{
//			xyp_samples[0][nsample]=xp;
//			xyp_samples[1][nsample]=yp;
			xp_accum += xp[0];
			yp_accum += yp[0];
			nsample++;
		}
		if(nsample<XPT_SAMPLE_NUMBER)continue; /* if not get enough samples */

		/* average of accumulated value */
		xp[0]=xp_accum>>XPT_SAMPLE_EXPNUM; /* shift exponent number of 2 */
		yp[0]=yp_accum>>XPT_SAMPLE_EXPNUM;
		/* reset nsample and accumulator then */
		nsample=0;
		xp_accum=0;yp_accum=0;

		/*----- convert to LCD coordinate sx,sy ------*/
	    	xpt_maplcd_xy(xp, yp, &sx, &sy);
	    	printf("xp=%d, yp=%d;  sx=%d, sy=%d\n",xp[0],yp[0],sx,sy);

	    	/*  if sx0,sy0 is set to 0, then store new data */
	    	if(sx0==0 || sy0==0){
			sx0=sx;
			sy0=sy;
	    	}

		/*---  get index of pressed ebox ----*/
	    	index=egi_get_boxindex(sx,sy,ebox,nrow*ncolumn);
		if(index>=0) /* if get meaningful index */
		{
			printf("button[%d] pressed!\n",index);
			sprintf(str_syscmd,"jpgshow m_%d.jpg",index+1);
			system(str_syscmd);
			//usleep(200000); //this will make touch points scattered.
		}



#if 0
	    /* ignore uncontinous points */
	    // too small value will cause more breaks and gaps
	    if( abs(sx-sx0)>3 || abs(sy-sy0)>3 ) {
		  /* reset sx0,sy0 */
		  sx0=0;sy0=0;
  		  continue;
	    }

	    /* ignore repeated points */
	    if(sx0==sx && sy0==sy)continue;
#endif

	    /* ------------- draw points to LCD -------------*/
#if 0
	    /*  ---Method 1. draw point---  */
//	    printf("sx=%d, sy=%d\n",sx,sy);
	    /*  --- valid range of sx,sy ---- */
	    if(sx<1+1 || sx>240-1 || sy<1+1 || sy>320-1)continue;

	    /*  ---Method 2. draw oval---  */
	    draw_oval(&fr_dev,sx,sy);

	    /*  ---Method 3. draw rectangle---  */
//    	    draw_rect(&fr_dev,sx-3,sy-1,sx,sy+2); //2*4 rect

	    /*  draw line */
//	    draw_line(&fr_dev,sx0,sy0,sx,sy);

#endif
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
