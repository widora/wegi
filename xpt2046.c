/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


XPT2046 touch pad controller handler.

Midas Zhou
----------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include "spi.h"
#include "xpt2046.h"
#include "egi_debug.h"
#include "egi_cstring.h"

//static int xpt_nsample; /* sample index for XPT touch point coordinate */

/* X/Y factors for mapping touch coordiantes to LCD coordinates. */
/* For 240x320 Touch LCD:  Xmax=240-1  Ymax=320-1 */
static float factX=2.17;
static float factY=2.95;

/* Touch pad base piont (center) */
static uint8_t  tbaseX=62;
static uint8_t  tbaseY=60;

/*--------------------------------------
Set mapping factor factX/factY
--------------------------------------*/
void xpt_set_factors(float fx, float fy, uint8_t tbx, uint8_t tby)
{
	factX=fx;
	factY=fy;
	tbaseX=tbx;
	tbaseY=tby;
}


/*----------------------------------------------
Initiate touch_pad calibrated factors/parameters.
If EGI config file contains factors, then
read in and use them. otherwise use defaults.

----------------------------------------------*/
void xpt_init_factors(void)
{
	char strfactor[EGI_CONFIG_VMAX];

	memset(strfactor,0,sizeof(strfactor));
	if( egi_get_config_value("TOUCH_PAD", "xpt_factX", strfactor) ==0 ) {
		printf("%s: Get xpt_factX='%s' in EGI config file.\n",__func__, strfactor);
		factX=atof(strfactor);
	}

	memset(strfactor,0,sizeof(strfactor));
	if( egi_get_config_value("TOUCH_PAD", "xpt_factY", strfactor) ==0 ) {
		printf("%s: Get xpt_factY='%s' in EGI config file.\n",__func__, strfactor);
		factY=atof(strfactor);
	}

	memset(strfactor,0,sizeof(strfactor));
	if( egi_get_config_value("TOUCH_PAD", "xpt_tbaseX", strfactor) ==0 ) {
		printf("%s: Get xpt_tbaseX='%s' in EGI config file.\n",__func__, strfactor);
		tbaseX=atoi(strfactor);
	}

	memset(strfactor,0,sizeof(strfactor));
	if( egi_get_config_value("TOUCH_PAD", "xpt_tbaseY", strfactor) ==0 ) {
		printf("%s: Get xpt_tbaseY='%s' in EGI config file.\n",__func__, strfactor);
		tbaseY=atoi(strfactor);
	}

}


/*---------------------------------------------------------------
read XPT touching coordinates, and normalize it.
*xp --- 2bytes value
*yp --- 2bytes value
return:
	0 	Ok
	<0 	pad untouched or invalid value

Note:
	1. Only first byte read from XPT is meaningful !!!??
---------------------------------------------------------------*/
static int xpt_read_xy(uint8_t *xp, uint8_t *yp)
{
	uint8_t cmd;

	/* Poll to get XPT touching coordinate */
	cmd=XPT_CMD_READXP;
	SPI_Write_then_Read(&cmd, 1, xp, 2); /* return 2bytes valid X value */
	cmd=XPT_CMD_READYP;
	SPI_Write_then_Read(&cmd, 1, yp, 2); /* return 2byte valid Y value */

	/*  Valify data,
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
2. Replace this function with your own map_method and data.
--------------------------------------------------------------------------------*/
static void xpt_maplcd_xy(const uint8_t *xp, const uint8_t *yp, uint16_t *xs, uint16_t *ys)
{
#if 0	/* Obselete */
	*xs=LCD_SIZE_X*(xp[0]-XPT_XP_MIN)/(XPT_XP_MAX-XPT_XP_MIN+1);
	*ys=LCD_SIZE_Y*(yp[0]-XPT_YP_MIN)/(XPT_YP_MAX-XPT_YP_MIN+1);
#else
	*xs=(xp[0]-tbaseX)*factX + LCD_SIZE_X/2;
	*ys=(yp[0]-tbaseY)*factY + LCD_SIZE_Y/2;

#endif
}



/*---------------------------------------------------------------------------------------
1. The function MUST be called at least XPT_SAMPLE_NUMBER times in order to get a meaningful data.
   This function may be called in a loop, it reads one touch data for each circle.
   It returns valid value only XPT_SAMPLE_NUMER consecutive samples have been read in.
   Anyway, put the function in the caller's loop maybe a good idea for the following reason:
   !!!!! --- Keep enough time-gap for XPT to prepare data in each read-session --- !!!!!

2. The function reads touch-pad XPT_SAMPLE_NUMBER times by calling xpt_read_xy(xp,yp), then it will
   calculate average touching coordinate x,y of those samples.

3. If pen-up or break happens and xpt_read_xy() fails, then incomplete samples will be discarded,
   the read session will restart again to ensure consistency of sample data.

4. The final average touch-pad coordinates will be converted to LCD x,y and passed to the caller.

5. The caller has the responsibility to decide whether to continue or go on its loop depending on
   return value.


avgsx,avgsy:	pointer to average x,y in tft-LCD coordinate.

Return:
	XPT_READ_STATUS_COMPLETE      0		OK, reading session just finished, (avgsx,avgsy) is ready.
	XPT_READ_STATUS_GOING 	1		during reading session, avgsx,avgsy is NOT ready.
	XPT_READ_STATUS_PENUP	2		pen-up
	( !!!! INVALID )  XPT_READ_STATUS_HOLDON  3		pressed and hold on, but may be moving...

	else		fail !!!!! no quit, until get enough consecutive samples !!!!!

TODO:

---------------------------------------------------------------------------------------*/
int xpt_getavg_xy(uint16_t *avgsx, uint16_t *avgsy)
{
        /*-------------------------------------------------
        Native XPT touch pad coordinate value
        !!!!!! WARNING: use xp[0] and yp[0] only !!!!!
        ---------------------------------------------------*/
        uint8_t  xp[2]; 	/* When untoched: Xp[0]=0, Xp[1]=0 */
        uint8_t  yp[2]; 	/* Untoched: Yp[0]=127=[2b0111,1111] ,Yp[1]=248=[2b1111,1100]  */

        static int xp_accum; 	/* Accumulator of xp */
        static int yp_accum; 	/* Accumulator of yp */
        static int nsample=0; 	/* Samples index */
	static int nfail=0; 	/* xpt_read_xy() fail counter, use to detect pen-up */
	static int last_status=XPT_READ_STATUS_PENUP; /* Record last time XPT_READ_STATUS....*/

        /* LCD coordinate value */
        //uint16_t sx,sy;  //current coordinate, it's a LCD screen coordinates derived from TOUCH coo$

	int ret;

        /*--------- Read XPT to get touch-pad coordinate --------*/
        if( xpt_read_xy(xp,yp)!=0 ) /* if read XPT fails,break or pen-up */
        {
                /* Reset nsample and accumulator then */
                nsample=0;
                xp_accum=0;yp_accum=0;

 		/* Consecutive failure counts, check if pen-up */
		nfail++;
		if( nfail > XPT_PENUP_READCOUNT ) /* pen-up status */
		{
		/* Do not change the XPT status, until a touch is read */
		/* Do not reset nfail here, it will reset when a touch is read in flowing else{} */
			//touch nfail=0;/* reset nfail */
			last_status=XPT_READ_STATUS_PENUP;
			return XPT_READ_STATUS_PENUP;
		}
        }
        else
        {
		/* else, reset nfail */
		nfail=0;
		/* Accumulate xp yp */
                xp_accum += xp[0];
                yp_accum += yp[0];
                nsample++;
        }


	/* If sample number reaches XPT_SAMPLE_NUMER, pass value to the caller. */
	if(nsample == XPT_SAMPLE_NUMBER)
	{
        	/* Average of accumulated value, */
      		xp[0]=xp_accum>>XPT_SAMPLE_EXPNUM; /* shift exponent number of 2 */
        	yp[0]=yp_accum>>XPT_SAMPLE_EXPNUM;

        	/* Reset nsample and accumulator then */
        	nsample=0;
        	xp_accum=0;yp_accum=0;

        	/* Convert to LCD coordinate, and pass to avsx,avsy */
        	xpt_maplcd_xy(xp, yp, avgsx, avgsy);
        	EGI_PDEBUG(DBG_TOUCH,"xp=%d, yp=%d;  sx=%d, sy=%d\n",xp[0],yp[0],*avgsx,*avgsy);

#if 0 /* Since status COMPLETE will always be breaked by XPT_READ_STATUS_GOING,
	 status HOLDON will never happen.!!!!! */
		if(last_status==XPT_READ_STATUS_COMPLETE)
		{
			ret=XPT_READ_STATUS_HOLDON; /* mission complete */
			/* Keep last_status as COMPLETE */
		}
		else
#endif
		{
			last_status=XPT_READ_STATUS_COMPLETE;
			ret=XPT_READ_STATUS_COMPLETE;   /* mission complete */
		}
	}
	else
	{
		last_status=XPT_READ_STATUS_GOING;
		ret=XPT_READ_STATUS_GOING; /* session is going on  */
	}

	return ret;
}



/*-------------------------------------------------------
Get raw data of touch pad coordinate values, which are
not mapped and for calibration purpose.

@rawx:	Pointer to pass raw data X. Ignore if NULL.
@rawy:  Pointer to pass raw data Y.  Ignore if NULL.

-------------------------------------------------------*/
int xpt_getraw_xy(uint16_t *rawx, uint16_t *rawy)
{
        /*-------------------------------------------------
         Native XPT touch pad coordinate value
        !!!!!! WARNING: use xp[0] and yp[0] only !!!!!
        ---------------------------------------------------*/
        uint8_t  xp[2]; 	/* When untoched: Xp[0]=0, Xp[1]=0 */
        uint8_t  yp[2]; 	/* Untoched: Yp[0]=127=[2b0111,1111] ,Yp[1]=248=[2b1111,1100]  */

        static int xp_accum; 	/* Accumulator of xp */
        static int yp_accum; 	/* Accumulator of yp */
        static int nsample=0; 	/* Samples index */
	static int nfail=0; 	/* xpt_read_xy() fail counter, use to detect pen-up */
	static int last_status=XPT_READ_STATUS_PENUP; /* Record last time XPT_READ_STATUS....*/

	int ret;

        /*--------- Read XPT to get touch-pad coordinate --------*/
        if( xpt_read_xy(xp,yp)!=0 ) /* If read XPT fails,break or pen-up */
        {
                /* Reset nsample and accumulator then */
                nsample=0;
                xp_accum=0;yp_accum=0;

 		/* Consecutive failure counts, check if pen-up */
		nfail++;
		if( nfail > XPT_PENUP_READCOUNT ) /* pen-up status */
		{
		/* Do not change the XPT status, until a touch is read */
		/* Do not reset nfail here, it will reset when a touch is read in flowing else{} */
			//touch nfail=0;/* reset nfail */
			last_status=XPT_READ_STATUS_PENUP;
			return XPT_READ_STATUS_PENUP;
		}
        }
        else /* Read XPT OK */
        {
		/* Else, reset nfail */
		nfail=0;
		/* Accumulate xp yp */
                xp_accum += xp[0];
                yp_accum += yp[0];
                nsample++;
        }

	/* If sample number reaches XPT_SAMPLE_NUMER, pass value to the caller. */
	if(nsample == XPT_SAMPLE_NUMBER)
	{
        	/* Average of accumulated value, */
      		xp[0]=xp_accum>>XPT_SAMPLE_EXPNUM; /* shift exponent number of 2 */
        	yp[0]=yp_accum>>XPT_SAMPLE_EXPNUM;

        	/* Reset nsample and accumulator then */
        	nsample=0;
        	xp_accum=0;yp_accum=0;

		/* Pass out rawx,rawy */
		if(rawx!=NULL)
			*rawx=xp[0];
		if(rawy!=NULL)
			*rawy=yp[0];

#if 0 /* Since status COMPLETE will always be breaked by XPT_READ_STATUS_GOING,
	 status HOLDON will never happen.!!!!! */
		if(last_status==XPT_READ_STATUS_COMPLETE)
		{
			ret=XPT_READ_STATUS_HOLDON; /* mission complete */
			/* Keep last_status as COMPLETE */
		}
		else
#endif
		{
			last_status=XPT_READ_STATUS_COMPLETE;
			ret=XPT_READ_STATUS_COMPLETE;   /* mission complete */
		}
	}
	else	/* XPT read session NOT finished yet */
	{
		last_status=XPT_READ_STATUS_GOING;
		ret=XPT_READ_STATUS_GOING; /* session is going on  */
	}

	return ret;
}
