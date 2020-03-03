/*-------------------  touch_home.c -----------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


XPT2046 touch pad

Midas Zhou
----------------------------------------------------------------*/
#ifndef __XPT2046_H__
#define __XPT2046_H__


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
#define XPT_CMD_READXP  0xD0 //0xD0 //1101,0000  /* read X position data */
#define XPT_CMD_READYP  0x90 //0x90 //1001,0000 /* read Y position data */

/* ----- XPT bias and limit value ----- */
#define XPT_XP_MIN 7
#define XPT_XP_MAX 116 //actual 116
#define XPT_YP_MIN 17
#define XPT_YP_MAX 116  //actual 116

/* ------ touch read sample number ------*/
#define XPT_SAMPLE_EXPNUM 4  /* 2^4=2*2*2*2 */
#define XPT_SAMPLE_NUMBER 1<<XPT_SAMPLE_EXPNUM  /* sample for each touch-read session */

/* ----- LCD parameters  ----- */
/* to be replaced with fb parameters .... */
#define LCD_SIZE_X 240
#define LCD_SIZE_Y 320

#define XPT_PENUP_READCOUNT 5 /* use to detect pen-up scenario */

/* status for XPT touch data reading */
#define XPT_READ_STATUS_COMPLETE      0   /* touched!! OK, reading session is just finished, data is ready.*/
#define XPT_READ_STATUS_GOING    1        /* touched!! session is going on,  data is NOT ready.*/
#define XPT_READ_STATUS_PENUP    2       /* no-touch!! pen-up status, pen untouched */
//#define XPT_READ_STATUS_HOLDON   3	/* NOT APPLICABLE!!!!  pressed and hold on */

/* -------------------------- functions ------------------------ */
//static int xpt_read_xy(uint8_t *xp, uint8_t *yp);
//static void xpt_maplcd_xy(const uint8_t *xp, const uint8_t *yp, uint16_t *xs, uint16_t *ys);
int xpt_getavg_xy(uint16_t *avgsx, uint16_t *avgsy);

#endif
