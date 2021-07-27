/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

			---  IMPORTANT NOTE ---

  1. Visional illusions will mar the appearance of your GUI design.
  2. Always Select correct colours to cover visual defects for your GUI!


TODO: Using pointer params is better than just adding inline qualifier ?

EGI Color Functions

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------------*/

#include "egi_color.h"
#include "egi_debug.h"
#include "egi_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
//#include <time.h>
#include <stdio.h>
#include <sys/time.h> /*gettimeofday*/
#include <stdint.h>



/*------------------------------------------------------------------------------------
Get a 16bit color value/alpha between two given colors/alphas by interpolation.

@color1, color2:	The first and second input color value.
@alpha1, alpha2:	The first and second input alpha value.
@f15_ratio:		Ratio for interpolation, in fixed point type f15.
			that is [0-1]*(2^15);
			!!! A bigger ratio makes alpha more closer to color2/alpha2 !!!
@colro, alpha		Pointer to pass output color and alpha.
			Ignore if NULL.

-------------------------------------------------------------------------------------*/
inline void egi_16bitColor_interplt( EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2,
				     unsigned char alpha1,  unsigned char alpha2,
				     int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha)
{
	unsigned int R,G,B,A;

	/* Interpolate color value */
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
	/* Interpolate alpha value */
	if(alpha) {
	        A  = alpha1*((1U<<15)-f15_ratio);  /* R */
	        A += alpha2*f15_ratio;
		A >>= 15;
		*alpha=A;
	}
}


/*---------------------------------------------------------------------
To generate Xterm 256 colors as defined by ISO/IEC-6429.

code:
	0-7:     Same as 8_color,  CSI: "ESC[30-37m"
	8-15:    Bright version of above colors. CSI: "ESC[90-97m"
	16-231:  6x6x6=216 colors ( 16+36*r+6*g+b  where 0<=r,g,b<=5 )
	232-255: 24_grade grays

Return:
	code<256  EGI_16BIT_COLOR
	code>255  0 as BLACK
----------------------------------------------------------------------*/
EGI_16BIT_COLOR egi_256color_code(unsigned int code)
{
	int nr,ng,nb;

	/* 1. Code 0-7: 8 standard xterm color */
	if( code<8 ) {
		if(code==7)  /* Silver(192,192,192) */
			return COLOR_RGB_TO16BITS(192,192,192);
		else /* 0-7: BLACK(0,0,0)
		      *	     Maroon(128,0,0), Green(0,128,0), Olive(128,128,0),
		      *	     Navy(0,0,128), Purple(128,0,128), Teal(0,128,128)
		      */
			return COLOR_RGB_TO16BITS((code&0x1)<<7, ((code>>1)&0x1)<<7, (code>>2)<<7); /* /2 %2 *128 */
	}
	/* 2. Code 8-15: 8 bright version standard xterm color */
	else if(code<16) {
		if(code==8) /* Grey(128,128,128) */
			return COLOR_RGB_TO16BITS(128,128,128);
                else /* 9-15: Red(255,0,0)
                      *       Lime(0,255,0), Yellow(255,255,0), Blue(0,0,255),
                      *       Fuchsia(255,0,255), Aqua(0,255,255), White(255,255,255)
                      */
                        return COLOR_RGB_TO16BITS((code&0x1)*255, ((code>>1)&0x1)*255, ((code>>2)&0x1)*255); /* /2 %2 *256 */

	}
	/* 3. Code 16-231:  6x6x6=216 colors ( 16+36*r+6*g+b  where 0<=r,g,b<=5 ) */
	else if(code<232) {
		nr=(code-16)/36;
		ng=(code-16)%36/6;
		nb=(code-16)%6;
		return COLOR_RGB_TO16BITS(nr==0?0:(95+40*(nr-1)), ng==0?0:(95+40*(ng-1)),nb==0?0:(95+40*(nb-1)));
	}
	/* 4. Code 232-255: 24_grade grays, Grey(8,8,8) (18,18,18)...(238,238,238) */
	else if( code<256 ) {
		int val=8+(code-232)*10;
		return COLOR_RGB_TO16BITS( val, val, val);
	}
	/* 5. CODE >255 , illegal. */
	else
		return 0;
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

#elif 1 /* WARNING!!!! Must keep 0 alpha value unchanged, or BLACK bk color will appear !!!
           a simple way to improve sharpness */
	alpha = alpha*3/2;
        if(alpha>255)
                alpha=255;
	//if(alpha<10)
	//	alpha=0;
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
2. R,G,B each contributes differently to the luminance, with G the most and B the least !

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


/*----------------------------------------------
	Convert YUYV to 24BIT RGB color
Y,U,V [0 255]

@src:	Source of YUYV data.
@dest:	Dest of RGB888 data
@w,h:	Size of image blocks.
	W,H MUST be multiples of 2.

@reverse:	To reverse data.

Note:
1. The caller MUST ensure memory space for dest!

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
int egi_color_YUYV2RGB888(const unsigned char *src, unsigned char *dest, int w, int h, bool reverse)
{
	int i,j;
	unsigned char y1,y2,u,v;
	int r1,g1,b1, r2,g2,b2;

	if(src==NULL || dest==NULL)
		return -1;

	if(w<1 || h<1)
		return -2;

	for(i=0; i<h; i++) {
	    for(j=0; j<w; j+=2) { /* 2 pixels each time */

		if(reverse) {
			y1 =*(src + (((h-1-i)*w+j)<<1));
			u  =*(src + (((h-1-i)*w+j)<<1) +1);
			y2 =*(src + (((h-1-i)*w+j)<<1) +2);
			v  =*(src + (((h-1-i)*w+j)<<1) +3);
		}
		else {
			y1 =*(src + ((i*w+j)<<1));
			u  =*(src + ((i*w+j)<<1) +1);
			y2 =*(src + ((i*w+j)<<1) +2);
			v  =*(src + ((i*w+j)<<1) +3);
		}

		r1 =(y1*4096 + 5765*v -737935)>>12;
		g1 =((4096*y1-1415*u-2936*v)>>12)+136;
		b1 =(y1*4096+7287*u-932708)>>12;
		if(r1>255) r1=255; else if(r1<0) r1=0;
		if(g1>255) g1=255; else if(g1<0) g1=0;
		if(b1>255) b1=255; else if(b1<0) b1=0;

		r2 =(y2*4096 + 5765*v -737935)>>12;
		g2 =((4096*y2-1415*u-2936*v)>>12)+136;
		b2 =(y2*4096+7287*u-932708)>>12;
		if(r2>255) r2=255; else if(r2<0) r2=0;
		if(g2>255) g2=255; else if(g2<0) g2=0;
		if(b2>255) b2=255; else if(b2<0) b2=0;

		*(dest + (i*w+j)*3)	=(unsigned char)r1;
		*(dest + (i*w+j)*3 +1)	=(unsigned char)g1;
		*(dest + (i*w+j)*3 +2)	=(unsigned char)b1;
		*(dest + (i*w+j)*3 +3)	=(unsigned char)r2;
		*(dest + (i*w+j)*3 +4)	=(unsigned char)g2;
		*(dest + (i*w+j)*3 +5)	=(unsigned char)b2;
	    }
	}

	return 0;
}


/*----------------------------------------------
	Convert YUYV to YUV
Y,U,V [0 255]

@src:	Source of YUYV data.
@dest:	Dest of YUV data
@w,h:	Size of image blocks.
	W,H MUST be multiples of 2.

@reverse:	To reverse data.

Note:
1. The caller MUST ensure memory space for dest!

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
int egi_color_YUYV2YUV(const unsigned char *src, unsigned char *dest, int w, int h, bool reverse)
{
	int i,j;
	unsigned char y1,y2,u,v;

	if(src==NULL || dest==NULL)
		return -1;

	if(w<1 || h<1)
		return -2;

	for(i=0; i<h; i++) {
	    for(j=0; j<w; j+=2) { /* 2 pixels each time */

		if(reverse) {
			y1 =*(src + (((h-1-i)*w+j)<<1));
			u  =*(src + (((h-1-i)*w+j)<<1) +1);
			y2 =*(src + (((h-1-i)*w+j)<<1) +2);
			v  =*(src + (((h-1-i)*w+j)<<1) +3);
		}
		else {
			y1 =*(src + ((i*w+j)<<1));
			u  =*(src + ((i*w+j)<<1) +1);
			y2 =*(src + ((i*w+j)<<1) +2);
			v  =*(src + ((i*w+j)<<1) +3);
		}

		*(dest + (i*w+j)*3)	=y1;
		*(dest + (i*w+j)*3 +1)	=u;
		*(dest + (i*w+j)*3 +2)	=v;
		*(dest + (i*w+j)*3 +3)	=y2;
		*(dest + (i*w+j)*3 +4)	=u;
		*(dest + (i*w+j)*3 +5)	=v;
	    }
	}

	return 0;
}


/*--------------------------------------------------
 Get Y(Luma/Brightness) value from a 16BIT RGB color
 as of YUV

 Y=0.30R+0.59G+0.11B=(307R+604G+113B)>>10 [0 255]
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


/*--------------------------------------------------------------
Convert HSV to RGB
H--Hue 		[0 360]  or X%360
S--Saturation	[0 100%]*10000
V--Value 	[0 255]

Note: All vars to be float type will be more accurate!
---------------------------------------------------------------*/
EGI_16BIT_COLOR egi_color_HSV2RGB(const EGI_HSV_COLOR *hsv)
{
	unsigned int h, hi,p,q,t,v;  /* float type will be more accurate! */
	float f;
	EGI_16BIT_COLOR color;

	if(hsv==NULL)
		return WEGI_COLOR_BLACK;

	/* Convert hsv->h to [0 360] */
	if( hsv->h < 0)
		h=360+(hsv->h%360);
	else
		h=hsv->h%360;

	v=hsv->v;
	hi=(h/60)%6;
	f=h/60.0-hi;
	p=v*(10000-hsv->s)/10000;
	q=round(v*(10000-f*hsv->s)/10000);
	t=round(v*(10000-(1-f)*hsv->s)/10000);

	switch(hi) {
		case 0:
			color=COLOR_RGB_TO16BITS(v,t,p); break;
		case 1:
			color=COLOR_RGB_TO16BITS(q,v,p); break;
		case 2:
			color=COLOR_RGB_TO16BITS(p,v,t); break;
		case 3:
			color=COLOR_RGB_TO16BITS(p,q,v); break;
		case 4:
			color=COLOR_RGB_TO16BITS(t,p,v); break;
		case 5:
			color=COLOR_RGB_TO16BITS(v,p,q); break;
	}

	return color;
}

/*--------------------------------------------------------------
Convert RGB to HSV
H--Hue 		[0 360]  or X%360
S--Saturation	[0 100%]*10000
V--Value 	[0 255]

Note: All vars to be float type will be more accurate!

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------*/
int egi_color_RGB2HSV(EGI_16BIT_COLOR color, EGI_HSV_COLOR *hsv)
{
	uint8_t R,G,B;
	uint8_t max,min;

	if(hsv==NULL) return -1;

	/* Get 3*8bit R/G/B */
	R = (color>>11)<<3;
	G = (((color>>5)&(0b111111)))<<2;
	B = (color&0b11111)<<3;

	/* Get max and min */
	max=R;
	if( max < G ) max=G;
	if( max < B ) max=B;
	min=R;
	if( min > G ) min=G;
	if( min > B ) min=B;

	/* Cal. hue */
	if( max==min )
		hsv->h=0;
	else if(max==R && G>=B)
		hsv->h=60*(G-B)/(max-min)+0;
	else if(max==R && G<B)
		hsv->h=60*(G-B)/(max-min)+360;
	else if(max==G)
		hsv->h=60*(B-R)/(max-min)+120;
	else if(max==B)
		hsv->h=60*(R-G)/(max-min)+240;

	/* Cal. satuarion */
	if(max==0)
		hsv->s=0;
	else
		hsv->s=10000*(max-min)/max;

	/* Cal. value */
	hsv->v=max;

	return 0;
}


/*-------------------------------------------------------
Create an EGI_COLOR_BANDMAP holding 1 band, and set
its default color and len(>=1).

@color:	Initial color for the map.
@len:	Initial total length of the map.
	Min 1.

Return:
	An pointer	OK
	NULL		Fails
-------------------------------------------------------*/
EGI_COLOR_BANDMAP *egi_colorBandMap_create(EGI_16BIT_COLOR color, unsigned int len)
{
	EGI_COLOR_BANDMAP *map;

	/* Limit len */
	if(len<=0)len=1;

	/* Calloc map */
	map=calloc(1,sizeof(EGI_COLOR_BANDMAP));
	if(map==NULL)
		return NULL;
	map->bands=calloc(COLORMAP_BANDS_GROW_SIZE, sizeof(EGI_COLOR_BAND));
	if(map->bands==NULL) {
		free(map);
		return NULL;
	}

	/* Set default_color to the first band */
	map->default_color=color;
	map->bands[0].pos=0;
	map->bands[0].len=len;
	map->bands[0].color=color;

	/* Set other params */
	map->size=1;  		/* Init. one band */
	map->capacity=COLORMAP_BANDS_GROW_SIZE;

	return map;
}

/*------------------------------
Free an EGI_COLOR_BANDMAP.
-------------------------------*/
void egi_colorBandMap_free(EGI_COLOR_BANDMAP **map)
{
	if(map==NULL || *map==NULL)
		return;

	free( (*map)->bands );
	free( *map );

	*map=NULL;
}

/* --------------------------------------
Allocate more sapce for map->bands.

Return:
	0	OK
	<0	Fails
----------------------------------------*/
int egi_colorBandMap_memGrowBands(EGI_COLOR_BANDMAP *map, unsigned int more_bands)
{
	if(map==NULL || map->bands==NULL)
		return -1;

	printf("%s: grow mem. space from %u to %u (bands).\n", __func__, map->capacity, map->capacity+more_bands);

        /* Check capacity and growsize */
        if( egi_mem_grow( (void **)&map->bands, map->capacity*sizeof(EGI_COLOR_BAND), more_bands*sizeof(EGI_COLOR_BAND)) !=0 ) {
        	printf("%s: Fail to mem grow map->bands!\n", __func__);
                        return -2;
        }
        else {
                        map->capacity += more_bands;
        }

	return 0;
}

/*-----------------------------------------------------------------------
Pick color from the band map,
NOPE !!!  XXXX --- The color is selected in the band map BEFORE pos
NOPE !!!  This is differenct from egi_colorBandMap_get_bandIndex();

Note:
1. If map unavailbale, return 0 as black;
2. If map->size==0, then return map->default_color.
3. If position is out of range, then it returns the color of last band.
TODO: A fast way, binary search.

@map:	An EGI_COLOR_BANDMAP pointer
@pos:   Position of the band map.
----------------------------------------------------------------------*/
EGI_16BIT_COLOR  egi_colorBandMap_pickColor(const EGI_COLOR_BANDMAP *map, unsigned int pos)
{
	unsigned int i;

	if(map==NULL || map->bands==NULL)
		return 0;

	/* If map->size<1 */
	if(map->size==0) {
		//return 0;
		return map->default_color;
	}

	/* If pos==0 */
	if(pos==0)
		return map->bands[0].color;

	/* Most frequent case for editor input: Pos is the last inserting position!! */
	else if(pos == map->bands[map->size-1].pos+map->bands[map->size-1].len ) {
		return map->bands[ map->size-1 ].color;
	}

	/* Pick color value */
	for(i=0; i< map->size-1; i++) {
		if( pos >= map->bands[i].pos && pos < map->bands[i+1].pos )
			return map->bands[i].color;
	}

	/* pos at the last band, or out of range: Same color as the last band */
	return map->bands[ map->size-1 ].color;
}


/*------------------------------------------------------------------------
Get index of map->bands[] accroding to pos.
 !!! --- The Index is selected for the first band as pos>=bands[index].pos --- !!!

Note:
1. If map is unavailable, it returns 0!
2. !!! If pos out of range, it returns the last band index(map->size-1)
   !!! Especially when map->size=0, it still returns index as 0!

TODO: A fast way. binary search!


@map:	An EGI_COLOR_BANDMAP pointer
@pos:   Position of the band map.
--------------------------------------------------------------------------*/
unsigned int egi_colorBandMap_get_bandIndex(const EGI_COLOR_BANDMAP *map, unsigned int pos)
{
	unsigned int i;

	if(map==NULL || map->bands==NULL)
		return 0;

	if(map->size==0)
		return 0;

	/* Most frequent case for editor input: Pos is the last inserting position!! */
	else if(pos == map->bands[map->size-1].pos+map->bands[map->size-1].len ) {
		return map->size-1;
	}

	for(i=0; i< map->size-1; i++) {
		if( pos >= map->bands[i].pos && pos < map->bands[i+1].pos )
			return i;
	}

	/* Any way, same as the last band! */
	return map->size-1;
}


/*---------------------------------------------------------------------
Insert a color band into BANDMAP.  !!! A recursive function !!!
Note:
0. Pos MUST be 0 if the map is empty with map->size==0.
1. If pos out of range, then it will fail!
   !!! It's OK to insert just at bottom/end of the last band.
2. If the inserted band holds the same color as the located original band,
   then just expand the original band.
3. All followed map->bans[].pos MUST be updated after insersion!

@map:   An EGI_COLOR_BANDMAP pointer
@pos:   Inserting position of the map, as start point of the new band.
@len:	length of the inserted band.
@color: Color of the inserted band.

Return:
        0       OK
        <0      Fails, or out of range!
-------------------------------------------------------------------------*/
int  egi_colorBandMap_insertBand(EGI_COLOR_BANDMAP *map, unsigned int pos, unsigned int len, EGI_16BIT_COLOR color)
{
        unsigned int  index; /* pos located in map->bands[index] */
	unsigned int i;

        if(map==NULL || map->bands==NULL)
                return -1;

	/* A. If insert to an empty map, then pos MUST be 0! */
	if(map->size==0) {
		if(pos!=0) {
			printf("%s: Insert into an empty map, pos MUST be 0!\n", __func__);
			return -2;
		}
		else {
			printf("%s: Insert into an empty map!\n", __func__);
			map->bands[0].pos=0;
			map->bands[0].color=color;
			map->bands[0].len=len;

			map->size +=1;

			return 0;
		}
	}

	/* B. map->size>0: If out of range (BUT! it's OK to insert just at end of the band. == see following case: 4. ELSE IF ) */
	if( pos > map->bands[map->size-1].pos + map->bands[map->size-1].len ) {
		printf("%s: Insert pos is out of range!\n", __func__);
		return -2;
	}

        /* Get index of band, as pos refers, (If pos out of range it will return the last band index, we ruled out the case as above.) */
        index=egi_colorBandMap_get_bandIndex(map, pos);

	/* 1. If just insert at the beginning of the map */
	if(pos==0) {
		printf("%s: Insert a band at pos=0!\n", __func__);
		/* 1.1 If same color as the first band, merge with it. (map->size==0 ruled out) */
                if( map->bands[0].color==color ) {
                        map->bands[0].len += len;

			/* Update all followed bands */
			for(i=1; i < map->size; i++)
				map->bands[i].pos+=len;

                        return 0;
                }
		/* 1.2 Not same color, insert a new band then. */
		else {
			/* Grow bands mem */
			if(map->capacity-map->size <1){
				if( egi_colorBandMap_memGrowBands(map,COLORMAP_BANDS_GROW_SIZE)!=0 ) {
					printf("%s: Fail to mem grow map->bands(case 1.2)!\n", __func__);
					return -3;
				}
			}

			/* Set aside bands[0] space for the new band */
			/* NOTE: map->size == 0 is ruled out at very beginning. */
			memmove( map->bands+1, map->bands+0, sizeof(EGI_COLOR_BAND)*map->size );

			/* Increase map->size and assign new band */
			map->size +=1;

			map->bands[0].pos=0;
			map->bands[0].len=len;
			map->bands[0].color=color;

			/* Update all followed bands */
			for(i=1; i < map->size; i++)
				map->bands[i].pos+=len;

			return 0;
		}
	}
	/* 2. If insert just at start pos of a band */
	else if( map->bands[index].pos==pos ) {
//		printf("%s: Insert at start of bands[%u]!\n", __func__, index);
		/* 2.1 If same color as previous indexed band, merge with it. */
		if( map->bands[index-1].color==color ) {
			 map->bands[index-1].len += len;
			 /* Update all pos of bands following bands[index-1] */
			 for(i=index; i < map->size; i++)
				map->bands[i].pos +=len;

			return 0;
		}
		/* 2.2 If same as the indexed band color, merge with it. */
		else if(map->bands[index].color==color ) {
			map->bands[index].len += len;
			/* Update all pos of bands following bands[index] */
                 	for(i=index+1; i < map->size; i++)
                        	map->bands[i].pos +=len;

			return 0;
		}
		/* 2.3 If NOT same color as its nearby bands, then insert an EGI_COLOR_BAND there. */
		else {
			/* Grow bands mem */
			if(map->capacity-map->size <1){
				if( egi_colorBandMap_memGrowBands(map,COLORMAP_BANDS_GROW_SIZE)!=0 ) {
					printf("%s: Fail to mem grow map->bands(case 2.3)!\n", __func__);
					return -3;
				}
			}

			/* Set aside bands[index] space for the new band */
			memmove( map->bands+index+1, map->bands+index, sizeof(EGI_COLOR_BAND)*(map->size-index));

			/* Insert the new band data */
			map->bands[index].pos=pos;
			map->bands[index].len=len;
			map->bands[index].color=color;

			/* Update map size */
			map->size +=1;

			/* Update all pos of bands following bands[index] */
                 	for(i=index+1; i < map->size; i++)
                        	map->bands[i].pos +=len;

			return 0;
		}
	}
	/* 3. ELSE If: NOT start pos, but same color, just expend map->bands[index].len
	 *    Note:  This also includes case 4.1, as pos out of range, _get_bandIndex() will
         *    return index of the last band!
	 */
	else if(map->bands[index].color==color) {
//		printf("%s: Insert at bands[%u] with same color!\n", __func__, index);
		 map->bands[index].len += len;
		 /* Update all pos of following bands */
		 for(i=index+1; i < map->size; i++)
			map->bands[i].pos +=len;

		return 0;
	}
	/* 4. ELSE if: pos just at bottom/end of the map!  (map->size==0 ruled out) */
	else if(pos == map->bands[map->size-1].pos+map->bands[map->size-1].len) {
//		printf("%s: Insert at map end!\n", __func__);

		/* 4.1 If same color as the last band, merge with it. */
		#if 0 /* Note: this is included in case 3. */
                if(map->bands[map->size-1].color==color ) {
                        map->bands[map->size-1].len += len;
                        return 0;
                }
		#endif

		/* 4.2 Not same color, add a new band. */
		/* Grow bands mem */
		if(map->capacity-map->size <1){
			if( egi_colorBandMap_memGrowBands(map,COLORMAP_BANDS_GROW_SIZE)!=0 ) {
				printf("%s: Fail to mem grow map->bands(case 4.2)!\n", __func__);
				return -3;
			}
		}

		/* Increase map->size frist, then assign new band */
		map->size +=1;

		map->bands[map->size-1].pos=pos; //map->bands[map->size-2].pos+map->bands[map->size-2].len;
		map->bands[map->size-1].len=len;
		map->bands[map->size-1].color=color;

		/* This is the last band now! */

		return 0;
	}
	/* 5. ELSE, insert into mid of a band, split the indexed band into two parts and do recursive job! */
	else {
//		printf("%s: Insert in mid of bands[%u], to split first!\n",  __func__, index);
		/* 5.1 Split indexed band into 2 bands */
		if( egi_colorBandMap_splitBand(map, pos)!=0 ) {
			printf("%s: Fail to split band(case 5.1)!\n",__func__);
			return -3;
		}

		/* 5.2 NOW the position MUST be start of an band! Recursive calling... */
		return egi_colorBandMap_insertBand(map, pos, len, color);
	}

	/* It should NOT get here! */
	return 0;
}

/*-------------------------------------------------------------------
Split a color band in map into two, pos is the start position of the
new band. The new band holds the same color as the original band.

This function usually to be called before redefine a band in
a map, first the map will be split at new band boundary, then to merge
all bands within the boundary. and also merger nearby bands if their color
is the same.

Note:
1. Map->size at least be ONE!
2. If pos is out of map range, need NOT to split.
3. If pos just the bottom/end of the map, need NOT to split.
4. If pos is already start position of a band in map, then need NOT
   to split the band.

@pos:   As split position to map, start point of a new band.
@map:	An EGI_COLOR_BANDMAP pointer

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------*/
int  egi_colorBandMap_splitBand(EGI_COLOR_BANDMAP *map, unsigned int pos)
{
	unsigned int  index; /* pos located in map->bands[index] */
	unsigned posend;     /* Bottom position of the map */

	if(map==NULL || map->bands==NULL)
		return -1;

	if(map->size<1)
		return -2;

	/* 1. If pos is out of range */
	posend=map->bands[map->size-1].pos+map->bands[map->size-1].len;
	if( pos >= posend ) /* out of range */
		return 0;
	//else if( pos == posend ) /* the bottom */
	//	return 0;

	/* Get index of the band, to which pos refers */
	index=egi_colorBandMap_get_bandIndex(map, pos);

	/* 2. If pos is start position of a band, need NOT to split */
	if( pos == map->bands[index].pos )
		return 0;

	/* Here: Need to split the band */

	/* Check capacity and growsize */
        if(map->capacity-map->size <1){
        	if( egi_colorBandMap_memGrowBands(map, COLORMAP_BANDS_GROW_SIZE)!=0 ) {
                	printf("%s: Fail to mem grow map->bands!\n", __func__);
                        return -4;
                }
        }

	/* Set aside bands[index+1] space for the new band */
	memmove( map->bands+index+2, map->bands+index+1, sizeof(EGI_COLOR_BAND)*(map->size-index-1));

	/* Insert the new band data */
	map->bands[index+1].pos=pos;
	map->bands[index+1].len=map->bands[index].pos+map->bands[index].len-pos; /* part of old band length */
	map->bands[index+1].color=map->bands[index].color;  /* original color */

	/* Modify/shorten the old band length. */
	map->bands[index].len=pos-map->bands[index].pos;

	/* Upate map->size */
	map->size++;

	/* Split operation need NOT to update following bands[].pos~ */

	return 0;
}


/*-----------------------------------------------------------------------------------------
Combine/merge band area within the range of [pos, pos+len-1] to one band, and modify map
data accordingly.

Note:
1. Map bottom/end (of the last band) will be modified if input scope exceeds its range.
   However, input pos MUST be within original map spectrum!
2. It's necessary to split bands at boundary of [pos, pos+len-1] first.
3. After combination, if its nearby band bears the same color, then they will also be merged
   to one band.

@map:   An EGI_COLOR_BANDMAP pointer
@pos:   Inserting position of the map, as start point of the new band.
@len:	length of the inserted band.
@color: Color of the inserted band.

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------------------------------*/
int  egi_colorBandMap_combineBands(EGI_COLOR_BANDMAP *map, unsigned int pos, unsigned int len, EGI_16BIT_COLOR color)
{
	unsigned int i;
	unsigned int headIdx, endIdx; /* indice of bands, in which new band head_pos and end_pos are located. */

	if(map==NULL || map->bands==NULL )
		return -1;

	if( len==0)
		return -1;

	/* Get bottom/end position */
	unsigned int pend1=map->bands[map->size-1].pos+map->bands[map->size-1].len;
	unsigned int pend2=pos+len;

	if( pos >= pend1 )
		return -1;

	/* 1. Extand length of last band to cover the scope. */
	if( pend2 > pend1 ) {
		map->bands[map->size-1].len += pend2-pend1;
		/* !!! NOW pend1==pend2, we need this in following (4.1) */
		pend1=pend2;
	}

	/* 2. Split original map at pos and pos+len */
	if( egi_colorBandMap_splitBand(map, pos)!=0 ) {
		printf("%s: Fail to split band(case 2.1)!\n",__func__);
		return -2;
	}
	if( egi_colorBandMap_splitBand(map, pos+len)!=0 ) {
		printf("%s: Fail to split band(case 2.2)!\n",__func__);
		return -3;
	}

	/* 3. Get index of pos and pos+len, NOW they MUST be starting pos of two bands */
	headIdx=egi_colorBandMap_get_bandIndex(map, pos);
	endIdx=egi_colorBandMap_get_bandIndex(map, pos+len);
	/* NOTE: pos just at map END,  then .._get_bandIndex() will return the last band index,  so we need to
	 * check/confirm this condition in following 4.1.
         */

	/* 4. combine bands */
	/* 4.1 If new band bottom/END is just the original map bottom/END, including expended pend1 */
	if( pend2 == pend1 ) {
		/* Combine bands aft headIdx */
		/* Update len and color */
		for(i=headIdx+1; i < map->size; i++)
			map->bands[headIdx].len += map->bands[i].len;

		/* Reset color */
		map->bands[headIdx].color=color;

		/* Update size */
		map->size = headIdx+1;

		/* If same color as previous band, then merge with it. */
		if( headIdx>0 && map->bands[headIdx-1].color==color ) {
			/* Update len */
			map->bands[headIdx-1].len += map->bands[headIdx].len;
			/* Update size */
			map->size -=1;
		}

		return 0;
	}
	/* 4.2 Merge bands between [headIdx and endIdx-1] to bands[headIdx] */
	else {
//		printf("%s: Merge bands[%u - %u] to bands[%u].\n", __func__, headIdx, endIdx-1, headIdx);
		/* Update bands[headIdx].len and .color */
		for(i=headIdx+1; i<endIdx; i++)
			map->bands[headIdx].len += map->bands[i].len;

		/* Reset color */
		map->bands[headIdx].color=color;

		/* Erase mid bands */
		memmove(map->bands+headIdx+1, map->bands+endIdx, (map->size-endIdx)*sizeof(EGI_COLOR_BAND));

		/* Update size */
		map->size -= (endIdx-headIdx-1);

		/* Following: After merge to bands[headIdx] */

		/* Check color */
		/* If same color as next band, merge two bands to bands[headIdx]
		 * Here, headIdx+1 is NOT the map END! see above if( pend2 == pend1 ).
		 */
		if(  map->bands[headIdx].color == map->bands[headIdx+1].color ) {
			/* Update len */
			map->bands[headIdx].len += map->bands[headIdx+1].len;

			/* Erase bands[headIdx+1] */
			if( map->size > headIdx+2 )
				memmove(map->bands+headIdx+1, map->bands+headIdx+2, (map->size-headIdx-2)*sizeof(EGI_COLOR_BAND));
			//else: map->size == headIdx+2, need not to memmove.

			/* Update size */
			map->size -=1;
		}
		/* Then If, same color as previous band, merge two bands to bands[headIdx-1] */
		if( headIdx>0 && map->bands[headIdx].color == map->bands[headIdx-1].color ) {
			/* Update len */
			map->bands[headIdx-1].len += map->bands[headIdx].len;

			/* Erase bands[headIdx], NOW headIdx+1 may be the map bottom/END! see above if. */
			if( map->size > headIdx+1 )
				memmove(map->bands+headIdx, map->bands+headIdx+1, (map->size-headIdx-1)*sizeof(EGI_COLOR_BAND));
			//else: map->size==headIdx+1, need not to memmove.

			/* Update size */
			map->size -=1;
		}

		return 0;
	}

	/* It should never get here! */
	return 0;
}


/*-----------------------------------------------------------------------------------------
Delete band area within the range of [pos, pos+len-1], and modify map data accordingly.

Note:

TODO: Shrink map->capacity (free bands mem) if necessary.

@map:   An EGI_COLOR_BANDMAP pointer
@pos:   Starting position for deleting.
@len:	Length of bands to be deleted.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------------------*/
int  egi_colorBandMap_deleteBands(EGI_COLOR_BANDMAP *map, unsigned int pos, unsigned int len)
{
	unsigned int i;
	unsigned int headIdx, endIdx; /* indice of bands, in which pos and pos+len are located. */
	unsigned int lensum;

	if(map==NULL || map->bands==NULL)
		return -1;

	if(map->size==0) {
		printf("%s: Fail to split a BANDMAP with map->size=0!\n", __func__);
		return -1;
	}

	/* If pos is out of range OR bottom of map!
	 * Note: _get_bandIndex(map, pos) will return 0 when map->size==0! so we have to rule out first!
	 */
	if( pos >= map->bands[map->size-1].pos +map->bands[map->size-1].len ) {
		printf("%s: Fail to split a BANDMAP, input pos out of range!\n", __func__);
		return -1;
	}

	if(len==0)
		return 0;

	/* 1. Split original map at pos and pos+len */
	if( egi_colorBandMap_splitBand(map, pos)!=0 ) {
		printf("%s: Fail to split band(case 2.1)!\n",__func__);
		return -2;
	}
	if( egi_colorBandMap_splitBand(map, pos+len)!=0 ) {
		printf("%s: Fail to split band(case 2.2)!\n",__func__);
		return -3;
	}

	/* 2. After split, get deleting start/end position */
	headIdx=egi_colorBandMap_get_bandIndex(map, pos);
	endIdx=egi_colorBandMap_get_bandIndex(map, pos+len); /* bands[endIdx] should NOT be deleted! */
	/* NOTE: if pos just at map END,  then .._get_bandIndex() will return the last band index,  so we need to
	 * check/confirm this condition in following case 3.
         */

	/* 3. If pos+len out of range, then it deletes all bands from bands[headIdx]  */
	if( pos+len >= map->bands[map->size-1].pos+map->bands[map->size-1].len ) {
		/* 3.1 If from the very beginning of map, delete all! */
		if( pos==0 ) {
			map->size=0;
			/* TODO: shrink capacity if necessary */
			return 0;
		}
		/* 3.2 Else: Delete from pos to the last */
		else {
			map->size -= (map->size-headIdx);
			/* TODO: shrink capacity if necessary */
			return 0;
		}
	}
	/* 4. Else, delete bands and update bands[].pos */
	else {

		/* Cal. total length of bands to be deleted */
		lensum=0;
		for(i=headIdx; i<endIdx; i++)
			lensum += map->bands[i].len;

		/* Memmove bands to squeeze out unwanted bands, from bands[headIdx] */
		memmove(map->bands+headIdx, map->bands+endIdx, (map->size-endIdx)*sizeof(EGI_COLOR_BAND));

		/* Update size */
		map->size -= endIdx-headIdx;

		/* Update following pos */
		for(i=headIdx; i< map->size; i++)
			map->bands[i].pos -= lensum;

		/* If new neighbors have same color, merge then. */
		if( headIdx>0 && map->bands[headIdx-1].color==map->bands[headIdx].color ) {
			/* Merge to bands[headIdx-1] */
			map->bands[headIdx-1].len += map->bands[headIdx].len;

			/* Erase bands[headIdx], move up following bands */
			if(map->size > headIdx+1) /* Only if headIdx+1 is NOT he bottom of map. */
				memmove(map->bands+headIdx, map->bands+headIdx+1, (map->size-headIdx-1)*sizeof(EGI_COLOR_BAND));

			/* Update size */
			map->size -= 1;

			/* NOT necessary to update following pos */

		}

		return 0;
	}

	/* It should never get here! */
	return 0;
}
