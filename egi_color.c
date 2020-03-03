/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

TODO: Using pointer params is better than just adding inline qualifier ?

EGI Color Functions

Midas ZHou
-------------------------------------------------------------------*/

#include "egi_color.h"
#include "egi_debug.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
//#include <time.h>
#include <stdio.h>
#include <sys/time.h> /*gettimeofday*/


/*----------------------------------------------------------------------
Get a 16bit color value/alpha between two given colors/alphas by interpolation.

@color1, color2:	The first and second input color value.
@alpha1, alpha2:	The first and second input alpha value.
@f15_ratio:		Ratio for interpolation, in fixed point type f15.
			that is [0-1]*(2^15);
			!!! A bigger ratio makes alpha more closer to color2/alpha2 !!!
@colro, alpha		Pointer to pass output color and alpha.
			Ignore if NULL.

-----------------------------------------------------------------------*/
inline void egi_16bitColor_interplt( EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2,
				     unsigned char alpha1,  unsigned char alpha2,
				     int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha)
{
	unsigned int R,G,B,A;

	/* interpolate color value */
	if(color) {
	 	R  =((color1&0xF800)>>8)*((1U<<15)-f15_ratio);	/* R */
		R +=((color2&0xF800)>>8)*f15_ratio;
		R >>= 15;

	 	G  =((color1&0x7E0)>>3)*((1U<<15)-f15_ratio);	/* G */
		G +=((color2&0x7E0)>>3)*f15_ratio;
		G >>= 15;

 		B  =((color1&0x1F)<<3)*((1U<<15)-f15_ratio);	/* B */
		B +=((color2&0x1F)<<3)*f15_ratio;
		B >>= 15;

        	*color=COLOR_RGB_TO16BITS(R, G, B);
	}
	/* interpolate alpha value */
	if(alpha) {
	        A  = alpha1*((1U<<15)-f15_ratio);  /* R */
	        A += alpha2*f15_ratio;
		A >>= 15;
		*alpha=A;
	}
}

/*------------------------------------------------------------------
Get average color value

@colors:	array of color values
@n		number of input colors

--------------------------------------------------------------------*/
inline EGI_16BIT_COLOR egi_16bitColor_avg(EGI_16BIT_COLOR *colors, int n)
{
	int i;
	unsigned int avgR,avgG,avgB;

	/* to enusre array colors is not NULL */
	avgR=avgG=avgB=0;
	for(i=0; i<n; i++) {
        	avgR += ((colors[i]&0xF800)>>8);
	        avgG += ((colors[i]&0x7E0)>>3);
        	avgB += ((colors[i]&0x1F)<<3);
        }

        return COLOR_RGB_TO16BITS(avgR/n, avgG/n, avgB/n);
}


/*------------------------------------------------------------------
                16bit color blend function
Note: Ignore back alpha value.
--------------------------------------------------------------------*/
inline EGI_16BIT_COLOR egi_16bitColor_blend(int front, int back, int alpha)
{
	float	fa;
	float	gamma=2.2; //0.45; //2.2;

#if 0 	/* Gamma correction, SLOW!!! */
	fa=(alpha+0.5)/256;
	fa= pow(fa,1.0/gamma);
	alpha=(unsigned char)(fa*256-0.5);

#elif 1 /* WARNING!!!! Must keep 0 alplha value unchanged, or BLACK bk color will appear !!!
           a simple way to improve sharpness */
	alpha = alpha*3/2;
        if(alpha>255)
                alpha=255;

#endif

        return COLOR_16BITS_BLEND(front, back, alpha);
}


/*------------------------------------------------------------------------------
                16bit color blend function
Note: Back alpha value also applied.
-------------------------------------------------------------------------------*/
inline EGI_16BIT_COLOR egi_16bitColor_blend2(EGI_16BIT_COLOR front, unsigned char falpha,
					     EGI_16BIT_COLOR back,  unsigned char balpha )
{
	/* applay front color if both 0 */
	if(falpha==0 && balpha==0) {
		return front;
	}
	/* blend them */
	else {
          return  COLOR_RGB_TO16BITS (
         	  	  ( ((front&0xF800)>>8)*falpha + ((back&0xF800)>>8)*balpha) /(falpha+balpha),
		          ( ((front&0x7E0)>>3)*falpha + ((back&0x7E0)>>3)*balpha) /(falpha+balpha),
        		  ( ((front&0x1F)<<3)*falpha + ((back&0x1F)<<3)*balpha) /(falpha+balpha)
                  );
	}
}


/*-------------------------------------------------------------------------
Get a random 16bit color from Douglas.R.Jacobs' RGB Hex Triplet Color Chart

@rang:  Expected color range:
		0--all range
		1--light color
		2--medium range
		3--deep color

with reference to  http://www.jakesan.com

R: 0x00-0x33-0x66-0x99-0xcc-0xff
G: 0x00-0x33-0x66-0x99-0xcc-0xff
B: 0x00-0x33-0x66-0x99-0xcc-0xff

------------------------------------------------------------------------*/
inline EGI_16BIT_COLOR egi_color_random(enum egi_color_range range)
{
        int i,j;
        uint8_t color[3]; /*RGB*/
        struct timeval tmval;
        EGI_16BIT_COLOR retcolor;

        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);

        /* random number 0-5 */
        for(i=0;i<3;i++)
        {
	        #if 0   /* Step 0x33 */
                j=(int)(6.0*rand()/(RAND_MAX+1.0));
                color[i]= 0x33*j;  /*i=0,1,2 -> R,G,B, j=0-5*/
		#else   /* Step 0x11 */
                j=(int)(15.0*rand()/(RAND_MAX+1.0));
                color[i]= 0x11*j;  /*i=0,1,2 -> R,G,B, j=0-5*/
		#endif

                if( range > 0 && i==1) /* if not all color range */
		{
			/* to select G range, so as to select color range */
                        if( color[i] >= 0x33*(2*range-2) && color[i] <= 0x33*(2*range-1) )
                        {
		                EGI_PDEBUG(DBG_COLOR," ----------- color G =0X%02X\n",color[i]);
				continue;
			}
			else /* retry */
			{
                                i--;
                                continue;
                        }
		}
        }

        retcolor=COLOR_RGB_TO16BITS(color[0],color[1],color[2]);

        EGI_PDEBUG(DBG_COLOR,"egi random color GRAY: 0x%04X(16bits) / 0x%02X%02X%02X(24bits) \n",
									retcolor,color[0],color[1],color[2]);
        return retcolor;
}


/*-----------------------------------------------------------
Get a random color value with Min. Luma/brightness Y.

!!! TOo big Luma will cost much more time !!!

@rang:  Expected color range:
		0--all range
		1--light color
		2--medium range
		3--deep color

@luman: Min. luminance of the color
-------------------------------------------------------------*/
EGI_16BIT_COLOR egi_color_random2(enum egi_color_range range, unsigned char luma)
{
	EGI_16BIT_COLOR color;

	/* Set LIMIT of input Luma
	 * Rmax=0xF8; Gmax=0xFC; Bmax=0xF8
         * Max Luma=( 307*(31<<3)+604*(63<<2)+113*(31<<3) )>>10 = 250;
	 */
	if( luma > 220 ) {
		printf("%s: Input luma is >220!\n",__func__);
	}
	if( luma > 250 )
		luma=250;

	do {
		color=egi_color_random(range);
	} while( egi_color_getY(color) < luma );

	return color;
}


/*---------------------------------------------------
Get a random 16bit gray_color from Douglas.R.Jacobs'
RGB Hex Triplet Color Chart

rang: color range:
	0--all range
	1--light color
	2--mid range
	3--deep color

with reference to  http://www.jakesan.com
R: 0x00-0x33-0x66-0x99-0xcc-0xff
G: 0x00-0x33-0x66-0x99-0xcc-0xff
B: 0x00-0x33-0x66-0x99-0xcc-0xff
---------------------------------------------------*/
EGI_16BIT_COLOR egi_colorGray_random(enum egi_color_range range)
{
        int i;
        uint8_t color; /*R=G=B*/
        struct timeval tmval;
        uint16_t ret;

        gettimeofday(&tmval,NULL);
        srand(tmval.tv_usec);

        /* random number 0-5 */
        for(;;)
        {
                i=(int)(15.0*rand()/(RAND_MAX+1.0));
                color= 0x11*i;

                if( range > 0 ) /* if not all color range */
		{
			/* to select R/G/B SAME range, so as to select color-GRAY range */
                        if( color>=0x11*5*(range-1) && color <= 0x11*(5*range-1) )
				break;
			else /* retry */
                                continue;
		}

		break;
        }

        ret=(EGI_16BIT_COLOR)COLOR_RGB_TO16BITS(color,color,color);

        EGI_PDEBUG(DBG_COLOR,"egi random color GRAY: 0x%04X(16bits) / 0x%02X%02X%02X(24bits) \n",
											ret,color,color,color);
        return ret;
}

/*---------------------------------------------------------------------
Color Luminance/Brightness control, brightness to be increase/decreased
according to k.

@color	 Input color in 16bit
@k	 Deviation value

Note:
1. Permit Y<0, otherwise R,G,or B MAY never get  to 0, when you need
   to obtain a totally BLACK RGB(000) color in conversion, as of a check
   point, say.

	--- RGB to YUV ---
Y=0.30R+0.59G+0.11B=(307R+604G+113G)>>10   [0-255]
U=0.493(B-Y)+128=( (505(B-Y))>>10 )+128    [0-255]
V=0.877(R-Y)+128=( (898(R-Y))>>10 )+128	   [0-255]

	--- YUV to RGB ---
R=Y+1.4075*(V-128)=Y+1.4075*V-1.4075*128
	=(Y*4096 + 5765*V -737935)>>12
G=Y-0.3455*(U-128)-0.7169*(V-128)=Y+136-0.3455U-0.7169V
	=(4096*Y+(136<<12)-1415*U-2936*V)>>12
B=Y+1.779*(U-128)=Y+1.779*U-1.779*128
	=(Y*4096+7287*U-932708)>>12
---------------------------------------------------------------------*/
EGI_16BIT_COLOR egi_colorLuma_adjust(EGI_16BIT_COLOR color, int k)
{
	int32_t R,G,B; /* !!! to be signed int32, same as YUV */
	int32_t Y,U,V;

	/* get 3*8bit R/G/B */
	R = (color>>11)<<3;
	G = (((color>>5)&(0b111111)))<<2;
	B = (color&0b11111)<<3;
//	printf(" color: 0x%02x, ---  R:0x%02x -- G:0x%02x -- B:0x%02x  \n",color,R,G,B);
//	return (EGI_16BIT_COLOR)COLOR_RGB_TO16BITS(R,G,B);

	/* convert RBG to YUV */
	Y=(307*R+604*G+113*B)>>10;
	U=((505*(B-Y))>>10)+128;
	V=((898*(R-Y))>>10)+128;
	//printf("R=%d, G=%d, B=%d  |||  Y=%d, U=%d, V=%d \n",R,G,B,Y,U,V);
	/* adjust Y, k>0 or k<0 */
	Y += k; /* (k<<12); */
	if(Y<0) {
		Y=0;
//		printf("------ Y=%d <0 -------\n",Y);
		/* !! Let Y<0,  otherwise R,G,or B MAY never get back to 0 when you need a totally BLACK */
		// Y=0; /* DO NOT set to 0 when you need totally BLACK RBG */
	}
	else if(Y>255)
		Y=255;

	/* convert YUV back to RBG */
	R=(Y*4096 + 5765*V -737935)>>12;
	//printf("R'=0x%03x\n",R);
	if(R<0)R=0;
	else if(R>255)R=255;

	G=((4096*Y-1415*U-2936*V)>>12)+136;
	//printf("G'=0x%03x\n",G);
	if(G<0)G=0;
	else if(G>255)G=255;

	B=(Y*4096+7287*U-932708)>>12;
	//printf("B'=0x%03x\n",B);
	if(B<0)B=0;
	else if(B>255)B=255;
	//printf(" Input color: 0x%02x, aft YUV adjust: R':0x%03x -- G':0x%03x -- B':0x%03x \n",color,R,G,B);

	return (EGI_16BIT_COLOR)COLOR_RGB_TO16BITS(R,G,B);
}

/*--------------------------------------------------
 Get Y(Luma/Brightness) value from a 16BIT RGB color
 as of YUV

 Y=0.30R+0.59G+0.11B=(307R+604G+113G)>>10 [0 255]
---------------------------------------------------*/
unsigned char egi_color_getY(EGI_16BIT_COLOR color)
{
        uint16_t R,G,B;

        /* get 3*8bit R/G/B */
        R = (color>>11)<<3;
        G = (((color>>5)&(0b111111)))<<2;
        B = (color&0b11111)<<3;
        //printf(" color: 0x%02x, ---  R:0x%02x -- G:0x%02x -- B:0x%02x  \n",color,R,G,B);

        /* convert to Y */
        return (307*R+604*G+113*B)>>10;
}
