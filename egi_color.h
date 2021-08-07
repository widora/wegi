/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Color classification method:
	Douglas.R.Jacobs' RGB Hex Triplet Color Chart
  	http://www.jakesan.com

LCD test/calibration refrence:
	http://www.lagom.nl/lcd-test/

TODO:
1. GAMMA CORRECTION for TFT LCD.
2. LCD sub_pixel deviation depends on pixel structure, RGB, RGBW,WRGB,RGB delt.. etc.
3. Sub_pixel layout will affect geometry/color displaying.
   Example:
   Verical or horizontal lines may appear BLACKER/THICKER if there is same sub_pixel
   color nearby ....

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef	__EGI_COLOR_H__
#define __EGI_COLOR_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* color definition */
typedef uint16_t		EGI_16BIT_COLOR;
typedef uint32_t		EGI_24BIT_COLOR;
typedef uint32_t		EGI_32BIT_COLOR;
typedef unsigned char		EGI_8BIT_CCODE;  /* For Y,U,V */
typedef unsigned char		EGI_8BIT_ALPHA;
typedef struct egi_hsv_color	EGI_HSV_COLOR;
typedef struct egi_color_band 	EGI_COLOR_BAND;
typedef struct egi_color_band_map 	EGI_COLOR_BANDMAP;


/* convert 24bit rgb(3*8bits) to 16bit LCD rgb */
#if 0  /* Just truncate other bits to get 565 RBG bits */
 #define COLOR_RGB_TO16BITS(r,g,b)	  ((uint16_t)( ( ((r)>>3)<<11 ) | ( ((g)>>2)<<5 ) | ((b)>>3) ))

#else  /* Round NOT truncate, to get 565 RGB bits  */
  #if 0
  #define COLOR_RGB_TO16BITS(r,g,b)	  ((uint16_t)(    ( ((((r)>>2)+1)>>1)<<11 )	\
							| ( ((((g)>>1)+1)>>1)<<5 )	\
							| ( (((b)>>2)+1)>>1 )		\
						      ))
  #endif
 #define COLOR_RGB_TO16BITS(r,g,b)	  ((uint16_t)(    ( ( ( (((r)>>2)+1)>0x3F ? ((r)>>2):(((r)>>2)+1) ) >>1) <<11 )	\
							| ( ( ( (((g)>>1)+1)>0x7F ? ((g)>>1):(((g)>>1)+1) ) >>1) <<5 )	\
							|   ( ( (((b)>>2)+1)>0x3F ? ((b)>>2):(((b)>>2)+1) ) >>1 )	\
						      ))

#endif

/* convert between 24bits and 16bits color */
#define COLOR_24TO16BITS(rgb)	(COLOR_RGB_TO16BITS( ((rgb)>>16), (((rgb)&0x00ff00)>>8), ((rgb)&0xff) ) )
#define COLOR_16TO24BITS(rgb)   ((uint32_t)( (((rgb)&0xF800)<<8) + (((rgb)&0x7E0)<<5) + (((rgb)&0x1F)<<3) ))


/* get 16bit complementary colour for a 16bit color
 * !!! NOTE: For 16bit color, it's NOT possible to get a true complementary color !!!
 */
#define COLOR_COMPLEMENT_16BITS(rgb)	 ( 0xFFFF-(rgb) )


/*  MACRO 16bit color blend
 *  front_color(16bits), background_color(16bits), alpha channel value(0-255)
 *  TODO: GAMMA CORRECTION
 */

#define COLOR_16BITS_BLEND(front, back, alpha)	\
		 COLOR_RGB_TO16BITS (							\
			  ( (((front)&0xF800)>>8)*(alpha) + (((back)&0xF800)>>8)*(255-(alpha)) )/255, 	\
			  ( (((front)&0x7E0)>>3)*(alpha) + (((back)&0x7E0)>>3)*(255-(alpha)) )/255,   	\
			  ( (((front)&0x1F)<<3)*(alpha) + (((back)&0x1F)<<3)*(255-(alpha)) )/255     	\
			)									\


#define COLOR_24BITS_BLEND(front, back, alpha)  \
			(			\
			    (( ( ((front)>>16)*(alpha) + ((back)>>16)*(255-(alpha)) )>>8 )<<16) +  \
			    (( ( (((front)&0xFF00)>>8)*(alpha) + (((back)&0xFF00)>>8)*(255-(alpha)) )>>8 )<<8) +  \
			    ( ( ((front)&0xFF)*(alpha) + ((back)&0xFF)*(255-(alpha)) )>>8 )  \
			)

/*-----------------------------------------------------------------------o
	  	16bit color blend function
Note: Back alpha value ingored.
-------------------------------------------------------------------------*/
EGI_16BIT_COLOR egi_16bitColor_blend(int front, int back, int alpha);


/*------------------------------------------------------------------------------
                16bit color blend function
Note: Back alpha value also applied.
-------------------------------------------------------------------------------*/
EGI_16BIT_COLOR egi_16bitColor_blend2(EGI_16BIT_COLOR front, unsigned char falpha,
                                             EGI_16BIT_COLOR back,  unsigned char balpha );



#define WEGI_COLOR_BLACK 		 COLOR_RGB_TO16BITS(0,0,0)
#define WEGI_COLOR_WHITE 		 COLOR_RGB_TO16BITS(255,255,255)

#define WEGI_COLOR_RED 			 COLOR_RGB_TO16BITS(255,0,0)
#define WEGI_COLOR_DARKRED		 COLOR_RGB_TO16BITS(0x99,0,0)
//#define WEGI_COLOR_DARKRED		 COLOR_RGB_TO16BITS(0x8B,0x00,0x00)

#define WEGI_COLOR_ORANGE		 COLOR_RGB_TO16BITS(255,160,16)
#define WEGI_COLOR_DARKGOLDEN		 COLOR_RGB_TO16BITS(0xCC,0x99,00)

#define WEGI_COLOR_LTYELLOW		 COLOR_RGB_TO16BITS(0xFF,0xFF,0x99)
#define WEGI_COLOR_YELLOW		 COLOR_RGB_TO16BITS(255,255,0)

#define WEGI_COLOR_PINK			 COLOR_RGB_TO16BITS(255,96,208)
#define WEGI_COLOR_LTSKIN		 COLOR_RGB_TO16BITS(240,150,110)

#define WEGI_COLOR_GREEN		 COLOR_RGB_TO16BITS(0,255,0)
#define WEGI_COLOR_LTGREEN		 COLOR_RGB_TO16BITS(0x99,0xFF,0x99)
#define WEGI_COLOR_YELLOWGREEN		 COLOR_RGB_TO16BITS(185,255,20)
#define WEGI_COLOR_SPRINGGREEN		 COLOR_RGB_TO16BITS(125,255,0)
#define WEGI_COLOR_BLUSIHGREEN		 COLOR_RGB_TO16BITS(140,255,155)
#define WEGI_COLOR_DARKGREEN		 COLOR_RGB_TO16BITS(0,0xAA,0)

#define WEGI_COLOR_FOLIAGE		COLOR_RGB_TO16BITS(90,105,40)

#define WEGI_COLOR_TURQUOISE		 COLOR_RGB_TO16BITS(0,255,125)
#define WEGI_COLOR_CYAN			 COLOR_RGB_TO16BITS(0,255,255)

#define WEGI_COLOR_LTOCEAN		 COLOR_RGB_TO16BITS(125,255,255)
#define WEGI_COLOR_OCEAN		 COLOR_RGB_TO16BITS(0,125,255)
#define WEGI_COLOR_DARKOCEAN		 COLOR_RGB_TO16BITS(0,125/2,255/2)

#define WEGI_COLOR_BLUE			 COLOR_RGB_TO16BITS(0,0,255)
#define WEGI_COLOR_DARKBLUE		 COLOR_RGB_TO16BITS(0,0,0x99)
#define WEGI_COLOR_BLUESKY		 COLOR_RGB_TO16BITS(95,120,170)
#define WEGI_COLOR_BLUEFLOWER		 COLOR_RGB_TO16BITS(165,130,195)

#define WEGI_COLOR_GYBLUE		 COLOR_RGB_TO16BITS(0x00,0x99,0xcc)   /* gray blue */
#define WEGI_COLOR_LTBLUE		 COLOR_RGB_TO16BITS(0x99,0xFF,0xFF)   /* light blue */

#define WEGI_COLOR_VIOLET		 COLOR_RGB_TO16BITS(125,0,225)
#define WEGI_COLOR_MAGENTA		 COLOR_RGB_TO16BITS(255,0,255)
#define WEGI_COLOR_RASPBERRY		 COLOR_RGB_TO16BITS(255,0,125)

#define WEGI_COLOR_PURPLE		 COLOR_RGB_TO16BITS(0x80,0,0x80)
#define WEGI_COLOR_DARKPURPLE		 COLOR_RGB_TO16BITS(0x30,0x19,0x34)
#define WEGI_COLOR_BROWN		 COLOR_RGB_TO16BITS(0xA5,0x2A,0x2A)
#define WEGI_COLOR_FIREBRICK		 COLOR_RGB_TO16BITS(178,34,34)
/* GRAY2 deeper than GRAY1 */
#define WEGI_COLOR_GRAYC		 COLOR_RGB_TO16BITS(0xF8,0xF8,0xF8)
#define WEGI_COLOR_GRAYB		 COLOR_RGB_TO16BITS(0xEE,0xEE,0xEE)
#define WEGI_COLOR_GRAYA		 COLOR_RGB_TO16BITS(0xDD,0xDD,0xDD)
#define WEGI_COLOR_GRAY			 COLOR_RGB_TO16BITS(0xCC,0xCC,0xCC)
#define WEGI_COLOR_GRAY1		 COLOR_RGB_TO16BITS(0xBB,0xBB,0xBB)
#define WEGI_COLOR_GRAY2		 COLOR_RGB_TO16BITS(0xAA,0xAA,0xAA)
#define WEGI_COLOR_GRAY3		 COLOR_RGB_TO16BITS(0x99,0x99,0x99)
#define WEGI_COLOR_GRAY4		 COLOR_RGB_TO16BITS(0x88,0x88,0x88)
#define WEGI_COLOR_GRAY5	 	 COLOR_RGB_TO16BITS(0x77,0x77,0x77)
#define WEGI_COLOR_DARKGRAY	 	 COLOR_RGB_TO16BITS(0x64,0x64,0x64)
#define WEGI_COLOR_DARKGRAY1	 	 COLOR_RGB_TO16BITS(0x32,0x32,0x32)
#define WEGI_COLOR_DARKGRAY2	 	 COLOR_RGB_TO16BITS(0x16,0x16,0x16)
#define WEGI_COLOR_MAROON	 	 COLOR_RGB_TO16BITS(128,0,0)

#define WEGI_COLOR_NEUTRAL5		 COLOR_RGB_TO16BITS(117,117,117)

/* color range */
enum egi_color_range
{
	color_light=3,
	color_medium=2,
	color_deep=1,
	color_all=0,
};

/* HSV color */
struct egi_hsv_color {
	int h; 			/* H--Hue	 [0 360]  or X%360 */
	unsigned int s; 	/* S--Saturation [0 100%]*10000  */
	uint8_t v; 		/* V--Value      [0 255] */
};

/* EGI Color Treatment Functions */
void 		egi_16bitColor_interplt( EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2,
                       	                 unsigned char alpha1,  unsigned char alpha2,
                               	         int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha);
EGI_16BIT_COLOR 	egi_256color_code(unsigned int code);
EGI_16BIT_COLOR 	egi_16bitColor_avg(EGI_16BIT_COLOR *colors, int n);
EGI_16BIT_COLOR 	egi_color_random(enum egi_color_range range);
EGI_16BIT_COLOR 	egi_color_random2(enum egi_color_range range, unsigned char luma);
EGI_16BIT_COLOR 	egi_colorGray_random(enum egi_color_range range);
EGI_16BIT_COLOR 	egi_colorLuma_adjust(EGI_16BIT_COLOR color, int k);
unsigned char		egi_color_getY(EGI_16BIT_COLOR color);
int 			egi_color_YUYV2RGB888(const unsigned char *yuyv, unsigned char *rgb, int w, int h, bool reverse);
int 			egi_color_YUYV2YUV(const unsigned char *src, unsigned char *dest, int w, int h, bool reverse);
EGI_16BIT_COLOR 	egi_color_HSV2RGB(const EGI_HSV_COLOR *hsv);
int 			egi_color_RGB2HSV(EGI_16BIT_COLOR color, EGI_HSV_COLOR *hsv);


/***  EGI COLOR BANDMAP and Functions
 * Note/Pitfalls:
 * 	1. _pickColor(map,pos): returns color value of the bands[], pos is located within [bands[].pos ~ bans[].pos+len-1].
 *	2. _get_bandIndex(): returns index of a band, input postion(pos) is within [bands[index].pos ~ bands[index].pos+len-1].
 *	   		     If pos out of range, it returns the last band index(map->size-1),
 *			     Especially when map->size=0, it still returns index as 0!
 *
 */
struct egi_color_band   /* EGI_COLOR_MAP */
{
	unsigned int	pos;	/* Start position/offset of the band */
	unsigned int  	len;	/* Length of the band */
	EGI_16BIT_COLOR	color;  /* Color of the band */
};
struct egi_color_band_map  /* EGI_COLOR_BANDMAP */
{
	EGI_16BIT_COLOR	default_color;   /* Default color, when bands==NULL */
	EGI_COLOR_BAND *bands;		 /* An array of EGI_COLOR_BAND, sorted in order. */
	#define COLORMAP_BANDS_GROW_SIZE 256 /* Capacity GROW SIZE for bands, also as initial value. */

	unsigned int 	size;		/* Current size of the bands */
	unsigned int 	capacity;	/* Capacity of mem that capable of holding MAX of EGI_COLOR_BANDs */
};
EGI_COLOR_BANDMAP *egi_colorBandMap_create(EGI_16BIT_COLOR color, unsigned int len);
void egi_colorBandMap_free(EGI_COLOR_BANDMAP **map);
int egi_colorBandMap_memGrowBands(EGI_COLOR_BANDMAP *map, unsigned int more_bands);
EGI_16BIT_COLOR  egi_colorBandMap_pickColor(const EGI_COLOR_BANDMAP *map, unsigned int pos);
int  egi_colorBandMap_splitBand(EGI_COLOR_BANDMAP *map, unsigned int pos); /* Split one to two */
int  egi_colorBandMap_insertBand(EGI_COLOR_BANDMAP *map, unsigned int pos, unsigned int len, EGI_16BIT_COLOR color);
int  egi_colorBandMap_combineBands(EGI_COLOR_BANDMAP *map, unsigned int pos, unsigned int len, EGI_16BIT_COLOR color);
int  egi_colorBandMap_deleteBands(EGI_COLOR_BANDMAP *map, unsigned int pos, unsigned int len);

#ifdef __cplusplus
 }
#endif

#endif


