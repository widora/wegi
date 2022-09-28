/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI COLOR functions

Journal:
2022-04-19:
	1. Test 143 HTML color definitions.

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "egi_math.h"
#include "egi_timer.h"
#include "egi_fbgeom.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi.h"
#include "egi_FTsymbol.h"

/*-------------------------------------
        FTsymbol WriteFB TXT
@txt:           Input text
@px,py:         LCD X/Y for start point.
--------------------------------------*/
void FTsymbol_writeFB(char *txt, int fw, int fh, EGI_16BIT_COLOR color, int px, int py)
{
        FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular,       /* FBdev, fontface */
                                        fw, fh,(const unsigned char *)txt,      /* fw,fh, pstr */
                                        320-10, 240/(fh+fh/5), fh/5,               /* pixpl, lines, fgap */
                                        px, py,                         /* x0,y0, */
                                        color, -1, 250,                 /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);        /* int *cnt, int *lnleft, int* penx, int* peny */
}

/* Draw color band map */
void draw_CBmap(const EGI_COLOR_BANDMAP *cbmap)
{
	int i,j;
	int swid=40; 	/* color strip with on screen */
	int ns_start, ns_end; /* start/end of a band, as to Screen Strip index [0 - 240/swid-1] */

	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

	if(cbmap==NULL) {
		fb_render(&gv_fb_dev);
		return;
	}

	/* Draw the band map */
	for(i=0; i< cbmap->size; i++) {
		ns_start=cbmap->bands[i].pos/320;
		ns_end=(cbmap->bands[i].pos+cbmap->bands[i].len-1)/320;

		/* A band appears shown in more than one strip */
		if( ns_end > ns_start ) {
			/* Upper part */
			draw_filled_rect2(&gv_fb_dev, cbmap->bands[i].color,
					  cbmap->bands[i].pos%320, ns_start*swid,
					  320-1, (ns_start+1)*swid-1 );
			/* Mid part, whole strip */
			if( ns_end-ns_start >1 ) {   // ns_start+1, ns_start+2,
				for(j=ns_start+1; j<ns_end; j++) {
					draw_filled_rect2(&gv_fb_dev, cbmap->bands[i].color,
							  0, j*swid,
							  320-1, (j+1)*swid-1 );
				}
			}

			/* Lower part */
			draw_filled_rect2(&gv_fb_dev, cbmap->bands[i].color,
					   0, ns_end*swid,
					  (cbmap->bands[i].pos+cbmap->bands[i].len-1)%320, (ns_end+1)*swid-1 );
		}
		/* Else: ns_start==ns_end, A band shown in ONE strip */
		else {
			draw_filled_rect2(&gv_fb_dev, cbmap->bands[i].color,
					  cbmap->bands[i].pos%320, ns_start*swid,
					  (cbmap->bands[i].pos+cbmap->bands[i].len-1)%320, (ns_start+1)*swid-1);
		}
	}

	fb_render(&gv_fb_dev);
}


/*=====================
	MAIN()
=====================*/
int main(void)
{
	int i,j,k;
	int value[3]={0}; /* control param for brightness adjustment */
	//int step[3]={18,12,18};  /* increase/decrease step for value */
	int delt[3]={18,12,18};

	EGI_16BIT_COLOR color[3],subcolor[3];
 	EGI_HSV_COLOR hsv;

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

        /* --- prepare fb device --- */
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;


#if 0	/* ------- TEST: EGI COLOR Definition ------ */

EGI_16BIT_COLOR  colors[4*3*12]= {  /* 4x3 for one screen page */
/* 8*17 + 7 = 143  */
COLOR_AliceBlue, COLOR_AntiqueWhite, COLOR_Aqua, COLOR_Aquamarine, COLOR_Azure, COLOR_Beige, COLOR_Bisque, COLOR_Black,
COLOR_BlanchedAlmond, COLOR_Blue, COLOR_BlueViolet, COLOR_Brown, COLOR_BurlyWood, COLOR_CadetBlue, COLOR_Chartreuse, COLOR_Chocolate,
COLOR_Coral, COLOR_CornflowerBlue, COLOR_Cornsilk, COLOR_Crimson, COLOR_Cyan, COLOR_DarkBlue, COLOR_DarkCyan, COLOR_DarkGoldenRod,
COLOR_DarkGray, COLOR_DarkGreen, COLOR_DarkKhaki, COLOR_DarkMagenta, COLOR_DarkOliveGreen, COLOR_DarkOrange, COLOR_DarkOrchid, COLOR_DarkRed,
COLOR_DarkSalmon, COLOR_DarkSeaGreen, COLOR_DarkSlateBlue, COLOR_DarkSlateGray, COLOR_DarkTurquoise, COLOR_DarkViolet, COLOR_DeepPink, COLOR_DeepSkyBlue,
COLOR_DimGray, COLOR_DodgerBlue, COLOR_Feldspar, COLOR_FireBrick, COLOR_FloraWhit, COLOR_ForestGreen, COLOR_Fuchsia, COLOR_Gainsboro,
COLOR_GhostWhite, COLOR_Gold, COLOR_GoldenRod, COLOR_Gray, COLOR_Green, COLOR_GreenYellow, COLOR_HoneyDew, COLOR_HotPink,
COLOR_IndianRed, COLOR_Indigo, COLOR_Ivory, COLOR_Khaki, COLOR_Lavender, COLOR_LavenderBlush, COLOR_LawnGreen, COLOR_LemonChiffon,
COLOR_LightBlue, COLOR_LightCoral, COLOR_LightCyan, COLOR_LightGoldenRodYellow, COLOR_LightGrey, COLOR_LightGreen, COLOR_LightPink, COLOR_LightSalmon,
COLOR_LightSeaGreen, COLOR_LightSkyBlue, COLOR_LightSlateBlue, COLOR_LightSlateGray, COLOR_LightSteelBlue, COLOR_LightYellow, COLOR_Lime, COLOR_LimeGreen,
COLOR_Linen, COLOR_Magenta, COLOR_Maroon, COLOR_MediumAquaMarine, COLOR_MediumBlue, COLOR_MediumOrchid, COLOR_MediumPurple, COLOR_MediumSeaGreen,
COLOR_MediumSlateBlue, COLOR_MediumSpringGreen, COLOR_MediumTurquoise, COLOR_MediumVioletRed, COLOR_MidnightBlue, COLOR_MintCream, COLOR_MistyRose, COLOR_Moccasin,
COLOR_NavajoWhite, COLOR_Navy, COLOR_OldLace, COLOR_Olive, COLOR_OliveDrab, COLOR_Orange, COLOR_OrangeRed, COLOR_Orchid,
COLOR_PaleGoldenRod, COLOR_PaleGreen, COLOR_PaleTurquoise, COLOR_PaleVioletRed, COLOR_PapayaWhip, COLOR_PeachPuff, COLOR_Peru, COLOR_Pink,
COLOR_Plum, COLOR_PowderBlue, COLOR_Purple, COLOR_Red, COLOR_RosyBrown, COLOR_RoyalBlue, COLOR_SaddleBrown, COLOR_Salmon,
COLOR_SandyBrown, COLOR_SeaGreen, COLOR_SeaShell, COLOR_Sienna, COLOR_Silver, COLOR_SkyBlue, COLOR_SlateBlue, COLOR_SlateGray,
COLOR_Snow, COLOR_SpringGreen, COLOR_SteelBlue, COLOR_Tan, COLOR_Teal, COLOR_Thistle, COLOR_Tomato, COLOR_Turquoise,
COLOR_Violet, COLOR_VioletRed, COLOR_Wheat, COLOR_White, COLOR_WhiteSmoke, COLOR_Yellow, COLOR_YellowGreen,

};

const char *color_names[4*3*12]={  /* 4*3 for one screen page */

"AliceBlue", "AntiqueWhite", "Aqua", "Aquamarine", "Azure", "Beige", "Bisque", "Black",
"BlanchedAlmond", "Blue", "BlueViolet", "Brown", "BurlyWood", "CadetBlue", "Chartreuse", "Chocolate",
"Coral", "CornflowerBlue", "Cornsilk", "Crimson", "Cyan", "DarkBlue", "DarkCyan", "DarkGoldenRod",
"DarkGray", "DarkGreen", "DarkKhaki", "DarkMagenta", "DarkOliveGreen", "DarkOrange", "DarkOrchid", "DarkRed",
"DarkSalmon", "DarkSeaGreen", "DarkSlateBlue", "DarkSlateGray", "DarkTurquoise", "DarkViolet", "DeepPink", "DeepSkyBlue",
"DimGray", "DodgerBlue", "Feldspar", "FireBrick", "FloraWhit", "ForestGreen", "Fuchsia", "Gainsboro",
"GhostWhite", "Gold", "GoldenRod", "Gray", "Green", "GreenYellow", "HoneyDew", "HotPink",
"IndianRed", "Indigo", "Ivory", "Khaki", "Lavender", "LavenderBlush", "LawnGreen", "LemonChiffon",
"LightBlue", "LightCoral", "LightCyan", "LightGoldenRodYellow", "LightGrey", "LightGreen", "LightPink", "LightSalmon",
"LightSeaGreen", "LightSkyBlue", "LightSlateBlue", "LightSlateGray", "LightSteelBlue", "LightYellow", "Lime", "LimeGreen",
"Linen", "Magenta", "Maroon", "MediumAquaMarine", "MediumBlue", "MediumOrchid", "MediumPurple", "MediumSeaGreen",
"MediumSlateBlue", "MediumSpringGreen", "MediumTurquoise", "MediumVioletRed", "MidnightBlue", "MintCream", "MistyRose", "Moccasin",
"NavajoWhite", "Navy", "OldLace", "Olive", "OliveDrab", "Orange", "OrangeRed", "Orchid",
"PaleGoldenRod", "PaleGreen", "PaleTurquoise", "PaleVioletRed", "PapayaWhip", "PeachPuff", "Peru", "Pink",
"Plum", "PowderBlue", "Purple", "Red", "RosyBrown", "RoyalBlue", "SaddleBrown", "Salmon",
"SandyBrown", "SeaGreen", "SeaShell", "Sienna", "Silver", "SkyBlue", "SlateBlue", "SlateGray",
"Snow", "SpringGreen", "SteelBlue", "Tan", "Teal", "Thistle", "Tomato", "Turquoise",
"Violet", "VioletRed", "Wheat", "White", "WhiteSmoke", "Yellow", "YellowGreen", " ",

};

 EGI_16BIT_COLOR fontColor,pickColor;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev, true);
  fb_position_rotate(&gv_fb_dev,0);

  //gv_fb_dev.lumadelt=60; /* This will affect pickColor */

   /* Load appfonts */
   if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
          printf("Fail to load FT appfonts, quit.\n");
          return -2;
   }


while(1) {
  for(k=0; k<12; k++) {
    for(i=0; i<3; i++) {
	for(j=0; j<4; j++) {
		draw_filled_rect2(&gv_fb_dev, colors[12*k+4*i+j], j*(320/4), i*(240/3), (j+1)*(320/4)-1, (i+1)*(240/3)-1);

		//fontColor=WEGI_COLOR_WHITE;
		pickColor=fbget_pixColor(&gv_fb_dev, j*(320/4)+5, i*(240/3)+10);
		fontColor=COLOR_COMPLEMENT_16BITS(pickColor);
		printf("Color: 0x%04X, PickColor: 0x%04X, Fontcolor: 0x%04X\n", colors[12*k+4*i+j], pickColor, fontColor);

		FTsymbol_writeFB((char *)color_names[12*k+4*i+j], 12, 12, fontColor, j*(320/4)+5, i*(240/3)+10);

		usleep(50000);
	}
    }
    sleep(1);
  }
}

exit(0);
#endif


#if 0	/* ------- TEST: EGI COLOR BANDMAP ------ */

int inpos;  /* insert pos */
int inlen;  /* insert len */
int endpos;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,0);


EGI_COLOR_BANDMAP *cbmap=NULL;

do {  	/* ----------------------- LOOP TEST ---------------------*/

draw_CBmap(cbmap);
sleep(1);

/* Create cbmap */
if(cbmap==NULL) {
	cbmap=egi_colorBandMap_create(WEGI_COLOR_WHITE, 150);
	if(cbmap==NULL)
		exit(1);
}
draw_CBmap(cbmap);
sleep(1);

/* Insert a band at beginning */
egi_colorBandMap_insertBand(cbmap, 0, 50, WEGI_COLOR_BLUE); /* map, pos, len, color */
draw_CBmap(cbmap);
sleep(1);

/* Insert a band at end */
egi_colorBandMap_insertBand(cbmap, cbmap->bands[cbmap->size-1].pos+cbmap->bands[cbmap->size-1].len, 50, WEGI_COLOR_RED); /* map, pos, len, color */
draw_CBmap(cbmap);
sleep(1);

/* Insert a band at middle */
egi_colorBandMap_insertBand(cbmap, (cbmap->bands[cbmap->size-1].pos+cbmap->bands[cbmap->size-1].len)/2, 50, WEGI_COLOR_GREEN); /* map, pos, len, color */
draw_CBmap(cbmap);
sleep(1);

#if 0 //////* Test: Delete a band */
for(i=0; i<10; i++) {
	egi_colorBandMap_deleteBands(cbmap, 50, 50); /* map,pos,len */
	draw_CBmap(cbmap);
	printf("ColorMap size=%u\n", cbmap->size);
	sleep(1);
}
continue;
#endif ///////////////

/* Random insersion */
for(k=0; ; k++) {
	endpos=cbmap->bands[cbmap->size-1].pos+cbmap->bands[cbmap->size-1].len;
	inpos = mat_random_range( endpos>320 ? 320 : endpos+1 );
	inlen =mat_random_range(100+1);
	printf("Insert(k=%d): pos=%d, len=%d \n", k, inpos, inlen);
	egi_colorBandMap_insertBand(cbmap,  inpos, inlen, egi_color_random(color_all));
	/* Draw the band map */
	draw_CBmap(cbmap);

	/* Out of range, define limit  */
	if( cbmap->bands[cbmap->size-1].pos > 6*320-3*100 ) /* 3*100 inserting blocks */
		break;

	usleep(80000);
}
 sleep(1);

 /* Insert a band at END of the map */
 printf("Insert: pos=%d, len=%d \n", inpos, inlen);
 egi_colorBandMap_insertBand(cbmap, cbmap->bands[cbmap->size-1].pos+cbmap->bands[cbmap->size-1].len, 100, WEGI_COLOR_BLUE); /* map, pos, len, color */
 draw_CBmap(cbmap);
 usleep(600000);

 /* Insert a band at beginning of a band */
 printf("Insert: pos=%d, len=%d \n", inpos, inlen);
 egi_colorBandMap_insertBand(cbmap, cbmap->bands[cbmap->size/2].pos, 100, WEGI_COLOR_RED); /* map, pos, len, color */
 draw_CBmap(cbmap);
 usleep(600000);

 /* Insert a band at beginning of the MAP */
 printf("Insert: pos=%d, len=%d \n", inpos, inlen);
 egi_colorBandMap_insertBand(cbmap, 0, 100, WEGI_COLOR_GREEN); /* map, pos, len, color */
 draw_CBmap(cbmap);
 sleep(2);

 /* Combine bands */
 inpos=mat_random_range(2*320);
 inlen=2*320;
 printf("Combine bands from pos=%d, len=%d \n", inpos, inlen);
 egi_colorBandMap_combineBands(cbmap, inpos, inlen, WEGI_COLOR_PURPLE);
 draw_CBmap(cbmap);
 sleep(2);

 /* Delete above combined band */
 printf("Delete bands from pos=%d, len=%d \n", inpos, inlen);
 egi_colorBandMap_deleteBands(cbmap, inpos, inlen);
 draw_CBmap(cbmap);
 sleep(2);

 /* Loop delete bands */
 for(i=0; i<32; i++) {
	egi_colorBandMap_deleteBands(cbmap, 160, 80); /* map,pos,len */
	draw_CBmap(cbmap);
	printf("ColorMap size=%u\n", cbmap->size);
	usleep(500000);
 }

 /* Free */
 egi_colorBandMap_free(&cbmap);


 } while(1); /* ------ EDN: LOOP TEST ----- */ 

exit(0);
#endif


#if 0	/* -------- TEST: EGI HSV COLOR -------- */
	int xres=gv_fb_dev.vinfo.xres;
	int yres=gv_fb_dev.vinfo.yres;
	for(j=0; j<xres; j++) {
        	for(k=0; k<yres; k++) {
                	        hsv=(EGI_HSV_COLOR){ round(1.0*360*k/yres-60), 10000, round(1.0*255*j/xres)}; /* hue -60-300 Deg */
                        	fbset_color(egi_color_HSV2RGB(&hsv));
	                        draw_dot(&gv_fb_dev, j, k);
        	}
	}
	fb_render(&gv_fb_dev);
	exit(1);
#endif


	/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<  test draw_wline <<<<<<<<<<<<<<<<<<<<<<*/


#if 0	/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<  test draw_wline <<<<<<<<<<<<<<<<<<<<<<*/
	EGI_POINT p1,p2;
	EGI_BOX box={{0,0},{240-1,320-1,}};
   while(1)
  {
	egi_randp_inbox(&p1, &box);
	egi_randp_inbox(&p2, &box);
	fbset_color(egi_color_random(color_medium));
	draw_wline(&gv_fb_dev,p1.x,p1.y,p2.x,p2.y,egi_random_max(11));
/*
	draw_pline(&gv_fb_dev,
		   egi_random_max(232),20,
		   egi_random_max(232),320-1-20,
		   egi_random_max(11) );
*/
	usleep(200000);
  }
	exit(1);
#endif 	/* >>>>>>>>>>>>>>>>>>>>>>>>>>> end testing draw_wlines >>>>>>>>>>>>>>>>>*/


#if 1	/* -------------- TEST: Luma adjust ------------ */
	int step[3]={5,5,5}; //18,12,18};  /* increase/decrease step for value */

	for(i=0;i<3;i++) {
		color[i]= egi_color_random(color_deep);
	}
	subcolor[0]=color[0]=WEGI_COLOR_RED;
	subcolor[1]=color[1]=WEGI_COLOR_GREEN;
	subcolor[2]=color[2]=WEGI_COLOR_BLUE;


while(1)
{
        for(i=0;i<3;i++)
	{
		printf("-------Before: subcolor[%d]:  Y=%d step[%d]=%d, value[%d]=%d-------\n",
						i,egi_color_getY(subcolor[i]),i,step[i], i, value[i]);
		subcolor[i]=egi_colorLuma_adjust(color[i], value[i]);
		printf("-------Aft: subcolor[%d]:  Y=%d step[%d]=%d, value[%d]=%d-------\n",
						i,egi_color_getY(subcolor[i]),i,step[i], i, value[i]);

		//printf("---k=%d,  subcolor: 0x%02X  ||||  color: 0x%02X ---\n",k,subcolor,color);
		fbset_color(subcolor[i]);
		draw_filled_rect(&gv_fb_dev, 15+(60+15)*i, 150, (15+60)*(i+1), 150+60);
		fb_render(&gv_fb_dev);

		if(subcolor[i]==0xFFFF || egi_color_getY(subcolor[i])>=255) //210 )
			step[i]= -delt[i];
		if(subcolor[i]==0x0000 || egi_color_getY(subcolor[i])<=0) //10 ) //255
			step[i]= delt[i];

		value[i] += step[i]; /* Y Max.255, k to be little enough */
	}
	//tm_delayms(50);
	usleep(55000);
}
#endif /* ----- END TEST: luma adjust ----- */


        /* <<<<<-----  EGI general release  ---->>>>> */
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

	return 0;
}



