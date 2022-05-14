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
4. TFT LCD flickering problem. (Under variable environment/circumstance...)
5. TFT LCD view angle is NOT the same as from each side, be sure to make it
   the right for Left/Right/Top/Bottom sides.
   View Left/Right symmetrical, however View Top/Bottom NOT etc..

Journal:
2021-08-27:
	1. Add COLOR_R8_IN16BITS, COLOR_G8_IN16BITS, COLOR_B8_IN16BITS

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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
typedef unsigned char		EGI_8BIT_CCODE;  /* For Y,U,V, and others... */
typedef unsigned char		EGI_8BIT_ALPHA;
typedef struct egi_hsv_color	EGI_HSV_COLOR;
typedef struct egi_color_band 	EGI_COLOR_BAND;
typedef struct egi_color_band_map 	EGI_COLOR_BANDMAP;

/* To fetch 8bits R/G/B from 16 bits color rgb */
#define COLOR_R8_IN16BITS(rgb)	( ((rgb)&0xF800)>>8 )
#define COLOR_G8_IN16BITS(rgb)	( ((rgb)&0x7E0)>>3 )
#define COLOR_B8_IN16BITS(rgb)	( ((rgb)&0x1F)<<3 )

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
#define COLOR_COMPLEMENT_16BITS(rgb)	 ( COLOR_24TO16BITS(0xFFFFFF-COLOR_16TO24BITS(rgb)) ) /* :) Hi,U!  MidasHK_2022-04-19 */


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
#define WEGI_COLOR_DARKRED		 COLOR_RGB_TO16BITS(0xAA,0,0)
//#define WEGI_COLOR_DARKRED		 COLOR_RGB_TO16BITS(0x8B,0x00,0x00)

#define WEGI_COLOR_ORANGE		 COLOR_RGB_TO16BITS(255,160,16)
#define WEGI_COLOR_DARKGOLDEN		 COLOR_RGB_TO16BITS(0xCC,0x99,00)

#define WEGI_COLOR_LTYELLOW		 COLOR_RGB_TO16BITS(0xFF,0xFF,0x99)
#define WEGI_COLOR_YELLOW		 COLOR_RGB_TO16BITS(255,255,0)

#define WEGI_COLOR_PINK			 COLOR_RGB_TO16BITS(255,96,208)
#define WEGI_COLOR_LTSKIN		 COLOR_RGB_TO16BITS(240,150,110)

#define WEGI_COLOR_GREEN		 COLOR_RGB_TO16BITS(0,255,0)
#define WEGI_COLOR_LTGREEN		 COLOR_RGB_TO16BITS(0x99,0xFF,0x99)  /* 0.6, 1.0, 0.6 */
#define WEGI_COLOR_YELLOWGREEN		 COLOR_RGB_TO16BITS(185,255,20)
#define WEGI_COLOR_SPRINGGREEN		 COLOR_RGB_TO16BITS(125,255,0)
#define WEGI_COLOR_BLUSIHGREEN		 COLOR_RGB_TO16BITS(140,255,155)
#define WEGI_COLOR_DARKGREEN		 COLOR_RGB_TO16BITS(0,0xCC,0)

#define WEGI_COLOR_FOLIAGE		COLOR_RGB_TO16BITS(90,105,40)   /*  0.3529 0.4118 0.1569 */

#define WEGI_COLOR_TURQUOISE		 COLOR_RGB_TO16BITS(0,255,125)
#define WEGI_COLOR_CYAN			 COLOR_RGB_TO16BITS(0,255,255)

#define WEGI_COLOR_LTOCEAN		 COLOR_RGB_TO16BITS(125,255,255)
#define WEGI_COLOR_OCEAN		 COLOR_RGB_TO16BITS(0,125,255)
#define WEGI_COLOR_DARKOCEAN		 COLOR_RGB_TO16BITS(0,125/2,255/2)

#define WEGI_COLOR_BLUE			 COLOR_RGB_TO16BITS(0,0,255)
#define WEGI_COLOR_DARKBLUE		 COLOR_RGB_TO16BITS(0,0,0xCC)
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

/* ----- HTML COLOR Names (https://www.w3school.com.cn/html/html_colornames.asp) ----- */

#define COLOR_AliceBlue	                COLOR_24TO16BITS(0xF0F8FF) // COLOR_RGB_TO16BITS(0xF0, 0xF8, 0xFF)
#define COLOR_AntiqueWhite              COLOR_24TO16BITS(0xFAEBD7) // COLOR_RGB_TO16BITS(0xFA, 0xEB, 0xD7)
#define COLOR_Aqua                      COLOR_24TO16BITS(0x00FFFF) // COLOR_RGB_TO16BITS(0x00, 0xFF, 0xFF)
#define COLOR_Aquamarine                COLOR_24TO16BITS(0x7FFFD4) // COLOR_RGB_TO16BITS(0x7F, 0xFF, 0xD4)
#define COLOR_Azure			COLOR_24TO16BITS(0xF0FFFF) // COLOR_RGB_TO16BITS(0xF0, 0xFF, 0xFF)
#define COLOR_Beige			COLOR_24TO16BITS(0xF5F5DC) // COLOR_RGB_TO16BITS(0xF5, 0xF5, 0xDC)
#define COLOR_Bisque			COLOR_24TO16BITS(0xFFE4C4) // COLOR_RGB_TO16BITS(0xFF, 0xE4, 0xC4)
#define COLOR_Black                     COLOR_24TO16BITS(0x000000)
#define COLOR_BlanchedAlmond            COLOR_24TO16BITS(0xFFEBCD)
#define COLOR_Blue                      COLOR_24TO16BITS(0x0000FF)
#define COLOR_BlueViolet		COLOR_24TO16BITS(0x8A2BE2)
#define COLOR_Brown                     COLOR_24TO16BITS(0xA52A2A)
#define COLOR_BurlyWood                 COLOR_24TO16BITS(0xDEB887)
#define COLOR_CadetBlue                 COLOR_24TO16BITS(0x5F9EA0)
#define COLOR_Chartreuse                COLOR_24TO16BITS(0x7FFF00)
#define COLOR_Chocolate                 COLOR_24TO16BITS(0xD2691E)
#define COLOR_Coral                     COLOR_24TO16BITS(0xFF7F50)
#define COLOR_CornflowerBlue            COLOR_24TO16BITS(0x6495ED)
#define COLOR_Cornsilk                  COLOR_24TO16BITS(0xFFF8DC)
#define COLOR_Crimson                   COLOR_24TO16BITS(0xDC143C)
#define COLOR_Cyan                      COLOR_24TO16BITS(0x00FFFF)
#define COLOR_DarkBlue                  COLOR_24TO16BITS(0x00008B)
#define COLOR_DarkCyan                  COLOR_24TO16BITS(0x008B8B)
#define COLOR_DarkGoldenRod             COLOR_24TO16BITS(0xB8860B)
#define COLOR_DarkGray                  COLOR_24TO16BITS(0xA9A9A9)
#define COLOR_DarkGreen                 COLOR_24TO16BITS(0x006400)
#define COLOR_DarkKhaki                 COLOR_24TO16BITS(0xBDB76B)
#define COLOR_DarkMagenta               COLOR_24TO16BITS(0x8B008B)
#define COLOR_DarkOliveGreen            COLOR_24TO16BITS(0x556B2F)
#define COLOR_DarkOrange                COLOR_24TO16BITS(0xFF8C00)
#define COLOR_DarkOrchid                COLOR_24TO16BITS(0x9932CC)
#define COLOR_DarkRed                   COLOR_24TO16BITS(0x8B0000)
#define COLOR_DarkSalmon                COLOR_24TO16BITS(0xE9967A)
#define COLOR_DarkSeaGreen              COLOR_24TO16BITS(0x8FBC8F)
#define COLOR_DarkSlateBlue             COLOR_24TO16BITS(0x483D8B)
#define COLOR_DarkSlateGray             COLOR_24TO16BITS(0x2F4F4F)
#define COLOR_DarkTurquoise             COLOR_24TO16BITS(0x00CED1)
#define COLOR_DarkViolet                COLOR_24TO16BITS(0x9400D3)
#define COLOR_DeepPink			COLOR_24TO16BITS(0xFF1493)
#define COLOR_DeepSkyBlue               COLOR_24TO16BITS(0x00BFFF)
#define COLOR_DimGray                   COLOR_24TO16BITS(0x696969)
#define COLOR_DodgerBlue                COLOR_24TO16BITS(0x1E90FF)
#define COLOR_Feldspar                  COLOR_24TO16BITS(0xD19275)
#define COLOR_FireBrick                 COLOR_24TO16BITS(0xB22222)
#define COLOR_FloralWhite               COLOR_24TO16BITS(0xFFFAF0)
#define COLOR_ForestGreen               COLOR_24TO16BITS(0x228B22)
#define COLOR_Fuchsia                   COLOR_24TO16BITS(0xFF00FF)
#define COLOR_Gainsboro                 COLOR_24TO16BITS(0xDCDCDC)
#define COLOR_GhostWhite                COLOR_24TO16BITS(0xF8F8FF)
#define COLOR_Gold                      COLOR_24TO16BITS(0xFFD700)
#define COLOR_GoldenRod                 COLOR_24TO16BITS(0xDAA520)
#define COLOR_Gray                      COLOR_24TO16BITS(0x808080)
#define COLOR_Green                     COLOR_24TO16BITS(0x008000)
#define COLOR_GreenYellow               COLOR_24TO16BITS(0xADFF2F)
#define COLOR_HoneyDew                  COLOR_24TO16BITS(0xF0FFF0)
#define COLOR_HotPink                   COLOR_24TO16BITS(0xFF69B4)
#define COLOR_IndianRed                 COLOR_24TO16BITS(0xCD5C5C)
#define COLOR_Indigo                    COLOR_24TO16BITS(0x4B0082)
#define COLOR_Ivory                     COLOR_24TO16BITS(0xFFFFF0)
#define COLOR_Khaki                     COLOR_24TO16BITS(0xF0E68C)
#define COLOR_Lavender                  COLOR_24TO16BITS(0xE6E6FA)
#define COLOR_LavenderBlush             COLOR_24TO16BITS(0xFFF0F5)
#define COLOR_LawnGreen                 COLOR_24TO16BITS(0x7CFC00)
#define COLOR_LemonChiffon              COLOR_24TO16BITS(0xFFFACD)
#define COLOR_LightBlue                 COLOR_24TO16BITS(0xADD8E6)
#define COLOR_LightCoral                COLOR_24TO16BITS(0xF08080)
#define COLOR_LightCyan                 COLOR_24TO16BITS(0xE0FFFF)  //halcyon
#define COLOR_LightGoldenRodYellow      COLOR_24TO16BITS(0xFAFAD2)
#define COLOR_LightGrey                 COLOR_24TO16BITS(0xD3D3D3)
#define COLOR_LightGreen                COLOR_24TO16BITS(0x90EE90)
#define COLOR_LightPink                 COLOR_24TO16BITS(0xFFB6C1)
#define COLOR_LightSalmon               COLOR_24TO16BITS(0xFFA07A)
#define COLOR_LightSeaGreen             COLOR_24TO16BITS(0x20B2AA)
#define COLOR_LightSkyBlue              COLOR_24TO16BITS(0x87CEFA)
#define COLOR_LightSlateBlue            COLOR_24TO16BITS(0x8470FF)
#define COLOR_LightSlateGray            COLOR_24TO16BITS(0x778899)
#define COLOR_LightSteelBlue            COLOR_24TO16BITS(0xB0C4DE)
#define COLOR_LightYellow               COLOR_24TO16BITS(0xFFFFE0)
#define COLOR_Lime                      COLOR_24TO16BITS(0x00FF00)
#define COLOR_LimeGreen                 COLOR_24TO16BITS(0x32CD32)
#define COLOR_Linen                     COLOR_24TO16BITS(0xFAF0E6)
#define COLOR_Magenta                   COLOR_24TO16BITS(0xFF00FF)
#define COLOR_Maroon                    COLOR_24TO16BITS(0x800000)
#define COLOR_MediumAquaMarine          COLOR_24TO16BITS(0x66CDAA)
#define COLOR_MediumBlue                COLOR_24TO16BITS(0x0000CD)
#define COLOR_MediumOrchid              COLOR_24TO16BITS(0xBA55D3)
#define COLOR_MediumPurple              COLOR_24TO16BITS(0x9370D8)
#define COLOR_MediumSeaGreen            COLOR_24TO16BITS(0x3CB371)
#define COLOR_MediumSlateBlue           COLOR_24TO16BITS(0x7B68EE)
#define COLOR_MediumSpringGreen         COLOR_24TO16BITS(0x00FA9A)
#define COLOR_MediumTurquoise           COLOR_24TO16BITS(0x48D1CC)
#define COLOR_MediumVioletRed           COLOR_24TO16BITS(0xC71585)
#define COLOR_MidnightBlue              COLOR_24TO16BITS(0x191970)
#define COLOR_MintCream                 COLOR_24TO16BITS(0xF5FFFA)
#define COLOR_MistyRose                 COLOR_24TO16BITS(0xFFE4E1)
#define COLOR_Moccasin                  COLOR_24TO16BITS(0xFFE4B5)
#define COLOR_NavajoWhite               COLOR_24TO16BITS(0xFFDEAD)
#define COLOR_Navy                      COLOR_24TO16BITS(0x000080)
#define COLOR_OldLace                   COLOR_24TO16BITS(0xFDF5E6)
#define COLOR_Olive                     COLOR_24TO16BITS(0x808000)
#define COLOR_OliveDrab                 COLOR_24TO16BITS(0x6B8E23)
#define COLOR_Orange                    COLOR_24TO16BITS(0xFFA500)
#define COLOR_OrangeRed                 COLOR_24TO16BITS(0xFF4500)
#define COLOR_Orchid                    COLOR_24TO16BITS(0xDA70D6)
#define COLOR_PaleGoldenRod             COLOR_24TO16BITS(0xEEE8AA)
#define COLOR_PaleGreen                 COLOR_24TO16BITS(0x98FB98)
#define COLOR_PaleTurquoise             COLOR_24TO16BITS(0xAFEEEE)
#define COLOR_PaleVioletRed             COLOR_24TO16BITS(0xD87093)
#define COLOR_PapayaWhip                COLOR_24TO16BITS(0xFFEFD5)
#define COLOR_PeachPuff                 COLOR_24TO16BITS(0xFFDAB9)
#define COLOR_Peru                      COLOR_24TO16BITS(0xCD853F)
#define COLOR_Pink                      COLOR_24TO16BITS(0xFFC0CB)
#define COLOR_Plum                      COLOR_24TO16BITS(0xDDA0DD)
#define COLOR_PowderBlue                COLOR_24TO16BITS(0xB0E0E6)
#define COLOR_Purple                    COLOR_24TO16BITS(0x800080)
#define COLOR_Red                       COLOR_24TO16BITS(0xFF0000)
#define COLOR_RosyBrown                 COLOR_24TO16BITS(0xBC8F8F)
#define COLOR_RoyalBlue                 COLOR_24TO16BITS(0x4169E1)
#define COLOR_SaddleBrown               COLOR_24TO16BITS(0x8B4513)
#define COLOR_Salmon                    COLOR_24TO16BITS(0xFA8072)
#define COLOR_SandyBrown                COLOR_24TO16BITS(0xF4A460)
#define COLOR_SeaGreen                  COLOR_24TO16BITS(0x2E8B57)
#define COLOR_SeaShell                  COLOR_24TO16BITS(0xFFF5EE)
#define COLOR_Sienna                    COLOR_24TO16BITS(0xA0522D)
#define COLOR_Silver                    COLOR_24TO16BITS(0xC0C0C0)
#define COLOR_SkyBlue                   COLOR_24TO16BITS(0x87CEEB)
#define COLOR_SlateBlue                 COLOR_24TO16BITS(0x6A5ACD)
#define COLOR_SlateGray                 COLOR_24TO16BITS(0x708090)
#define COLOR_Snow                      COLOR_24TO16BITS(0xFFFAFA)
#define COLOR_SpringGreen               COLOR_24TO16BITS(0x00FF7F)
#define COLOR_SteelBlue                 COLOR_24TO16BITS(0x4682B4)
#define COLOR_Tan                       COLOR_24TO16BITS(0xD2B48C)
#define COLOR_Teal                      COLOR_24TO16BITS(0x008080)
#define COLOR_Thistle                   COLOR_24TO16BITS(0xD8BFD8)
#define COLOR_Tomato                    COLOR_24TO16BITS(0xFF6347)
#define COLOR_Turquoise                 COLOR_24TO16BITS(0x40E0D0)
#define COLOR_Violet                    COLOR_24TO16BITS(0xEE82EE)
#define COLOR_VioletRed                 COLOR_24TO16BITS(0xD02090)
#define COLOR_Wheat                     COLOR_24TO16BITS(0xF5DEB3)
#define COLOR_White                     COLOR_24TO16BITS(0xFFFFFF)
#define COLOR_WhiteSmoke                COLOR_24TO16BITS(0xF5F5F5)
#define COLOR_Yellow                    COLOR_24TO16BITS(0xFFFF00)
#define COLOR_YellowGreen               COLOR_24TO16BITS(0x9ACD32)


/* color range */ enum egi_color_range {
	color_light=3,
	color_medium=2,
	color_deep=1,
	color_all=0,
};

/* HSV color */
struct egi_hsv_color {
	int 		h;	/* H--Hue	 [0 360]  or X%360 */
	unsigned int    s; 	/* S--Saturation [0 100%]*10000  */
	uint8_t 	v;	/* V--Value      [0 255] */
};

/* EGI Color Treatment Functions */
void 		egi_16bitColor_interplt( EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2,
                       	                 unsigned char alpha1,  unsigned char alpha2,
                               	         int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha);
void 		egi_16bitColor_interplt4p( EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2,
                                      	   EGI_16BIT_COLOR color3, EGI_16BIT_COLOR color4,
                                           unsigned char alpha1,  unsigned char alpha2,
                                           unsigned char alpha3,  unsigned char alpha4,
                                           int f15_ratioX, int f15_ratioY,
                                           EGI_16BIT_COLOR* color, unsigned char *alpha );
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


