/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Test EGI FBGEOM functions

Midas Zhou
-----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"


const wchar_t *wslogan=L"EGI:  孵化在OpenWrt_Widora上                 一款小小小开源UI";
void display_slogan(void)
{
//	return;

            FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,         /* FBdev, fontface */
                                          18, 18, wslogan,                      /* fw,fh, pstr */
                                          320-10, 2, 6,                            /* pixpl, lines, gap */
                                          10, 100+40+5,                                /* x0,y0, */
                              WEGI_COLOR_BLACK, -1, 255 );   /* fontcolor, transcolor, opaque */
}


int main(int argc, char ** argv)
{
	EGI_16BIT_COLOR color;
	struct timeval tms,tme;


#if 0
	int i;
//   for(i=0;i<30;i++) {
//	printf("sqrt of %ld is %ld. \n", 1<<i, (mat_fp16_sqrtu32(1<<i)) >> 16 );
//  }
#endif


        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_geom") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif 

#if 1
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

#endif
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

#if 0
        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */
#endif

        /* <<<<------------------  End EGI Init  ----------------->>>> */





#if 0  /* <<<<<<<<<<<<<<  1. test inbox  <<<<<<<<<<<<<<<*/
	EGI_BOX inbox[4];
	inbox[0]= (EGI_BOX){(EGI_POINT){0,0}, (EGI_POINT){23,45} };
	inbox[1]= (EGI_BOX){(EGI_POINT){23,59}, (EGI_POINT){20,310} };
	inbox[2]= (EGI_BOX){(EGI_POINT){200,200}, (EGI_POINT){30,120} };
	inbox[3]= (EGI_BOX){(EGI_POINT){200,200}, (EGI_POINT){230,319} };

	for(i=0;i<4;i++) {
		if(box_inbox(inbox+i, &gv_fb_box))
			printf("inbox[%d] is within FB box!\n",i);
		else
			printf("inbox[%d] is NOT within FB box!\n",i);
	}

	exit(0);
#endif



#if 0  /* <<<<<<<<<<<<<<  2. test draw_dot()  <<<<<<<<<<<<<<<*/
	int i,j;
	fbset_color(WEGI_COLOR_RED);
	printf("draw block...\n");
	for(i=500; i<800; i++) {
		for(j=500; j<800; j++) {
			draw_dot(&gv_fb_dev, j, i);  /* (fb,x,y) */
		}
	}

	usleep(900000);
	usleep(900000);
#endif



#if 0  /* <<<<<<<<<<<<<<  3. test draw triangle  <<<<<<<<<<<<<<<*/

	EGI_BOX box={ {0,0},{600,800}}; //239,319}};
	EGI_BOX tri_box;
	EGI_POINT pts[3]={ {50,50}, {100,100}, {75, 90}};

while(1) {

	egi_randp_inbox(&tri_box.startxy, &box);
	tri_box.endxy.x=tri_box.startxy.x+100;
	tri_box.endxy.y=tri_box.startxy.y+100;

	for(i=0;i<3;i++)
		egi_randp_inbox(pts+i, &tri_box);

        color=egi_color_random(color_all);
	fbset_color(color);
	draw_filled_triangle(&gv_fb_dev, pts);
	//usleep(5000);
	//tm_delayms(75);
}
	return 0;
#endif



#if 0  /* <<<<<<<<<<<<<<  4. test draw pcircle  <<<<<<<<<<<<<<<*/
	int w;

	if(argc>1) w=atoi(argv[1]);
	else w=1;

//	clear_screen(&gv_fb_dev,0);
while(1) {

        color=egi_color_random(medium);
	//color=WEGI_COLOR_YELLOW;
	gettimeofday(&tms,NULL);
        for(i=5; i<40; i++) {
       	 	fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
	        fb_filo_on(&gv_fb_dev); /* start collecting old FB pixel data */

		draw_filled_circle2(&gv_fb_dev,120,120,2*i,WEGI_COLOR_BLACK);
		draw_filled_annulus2(&gv_fb_dev, 120, 120, 2*i, w, color);
//                draw_pcircle(&gv_fb_dev, 120, 120, i, 5); //atoi(argv[1]), atoi(argv[2]));
		draw_circle2(&gv_fb_dev,120,120,2*i+10,color);
		draw_circle2(&gv_fb_dev,120,120,2*i+20,color);

                tm_delayms(75);

	        fb_filo_off(&gv_fb_dev); /* start collecting old FB pixel data */
        }
	gettimeofday(&tme,NULL);

	printf("--------- cost time in us: %d -------\n",tm_diffus(tms,tme));

}
        return 0;

#endif


#if 1  /* <<<<<<<<<<<<<<  5. test draw  arc  <<<<<<<<<<<<<<<*/
	int i;
	int x0=160;
	int y0=120+5;
	double ka;
	double da=(MATH_PI*2.0-0.5*2)/15.0;
	int n=(MATH_PI*2.0-0.5*2)/da;

	/* set FB mode as NONbuffer*/
	//gv_fb_dev.map_bk=gv_fb_dev.map_fb;

        /* Set FB mode as LANDSCAPE  */
        fb_position_rotate(&gv_fb_dev, 3);

   	clear_screen(&gv_fb_dev, COLOR_RGB_TO16BITS(190,190,190));


while(1) {

 fb_clear_backBuff(&gv_fb_dev, COLOR_RGB_TO16BITS(120,120,120)); //WEGI_COLOR_GRAY3);
 //fbset_color(egi_color_random(color_deep));
 fbset_color(WEGI_COLOR_DARKRED);

// display_slogan();
//for( i=0; i<n; i++)
// for(ka=0.5; ka<MATH_PI*2.0-0.5; ka += da )
 for(ka=0.5; ka<MATH_PI*2.0+0.5; ka += da )
 {
     for( i=0; i<10; i++ ) {
	fbset_color(egi_color_random(color_all));
	draw_arc(&gv_fb_dev, x0, y0, 110+10*i, -ka-da, -ka, 10);
     }
	fbset_color(WEGI_COLOR_WHITE);
	draw_arc(&gv_fb_dev, x0, y0, 92, -ka-da, -ka, 5); //0.5-MATH_PI*2.0, -0.5, 5); //MATH_PI-0.5, 0, 3);
	fbset_color(WEGI_COLOR_RED);
	draw_arc(&gv_fb_dev, x0, y0, 75, -ka-da, -ka, 10); //0.5-MATH_PI*2.0, -0.5, 10); //MATH_PI-0.5, 0, 3);
	fbset_color(WEGI_COLOR_GREEN);
	draw_arc(&gv_fb_dev, x0, y0, 50, -ka-da, -ka, 20); //0.5-MATH_PI*2.0, -0.5, 10); //MATH_PI-0.5, 0, 3);
	fbset_color(WEGI_COLOR_BLUE);
 	draw_arc(&gv_fb_dev, x0, y0, 15, -ka-da, -ka, 30); //0.5-MATH_PI*2.0, -0.5, 30); //MATH_PI-0.5, 0, 3);

 	//display_slogan();
  	fb_page_refresh(&gv_fb_dev);
	tm_delayms(10);
 }

 /* wedge */
// for(ka=-MATH_PI/20*4; ka< 6/5*MATH_PI; ka += MATH_PI/60 ) {
 for(i=-2; i<12; i++) {
 	fbset_color(WEGI_COLOR_PINK); //egi_color_random(color_deep));
	draw_filled_pieSlice(&gv_fb_dev, x0, y0, 65, MATH_PI/10*i, MATH_PI/10*(i+1));  //MATH_PI*2.0-ka);//  0.5-da );
 	//display_slogan();
  	fb_page_refresh(&gv_fb_dev);
	tm_delayms(10);
 }

 //display_slogan();
 fb_page_refresh(&gv_fb_dev);

 tm_delayms(3000);

}

#endif


#if 0  /* <<<<<<<<<<<<<<  6. test draw_wline & draw_pline  <<<<<<<<<<<<<<<*/
/*
	EGI_POINT p1,p2;
	EGI_BOX box={{0,0},{240-1,320-1,}};
   while(1)
  {
	egi_randp_inbox(&p1, &box);
	egi_randp_inbox(&p2, &box);
	fbset_color(egi_color_random(medium));
	draw_wline(&gv_fb_dev,p1.x,p1.y,p2.x,p2.y,egi_random_max(11));

	usleep(200000);
  }
*/
	int rad=50; /* radius */
	int div=4;/* 4 deg per pixel, 240*2=480deg */
	int num=240/1; /* number of points */
	EGI_POINT points[240/1]; /* points */
	int delt=0;

	mat_create_fptrigontab();/* create lookup table for trigonometric funcs */

	/* clear areana */
	fbset_color(WEGI_COLOR_BLACK);
	draw_filled_rect(&gv_fb_dev,0,30,240-1,320-100);

	/* draw a square area of grids */
	fbset_color(WEGI_COLOR_GREEN);
	for(i=0;i<6;i++)
	 	draw_line(&gv_fb_dev,0,30+5+i*40,240-1,30+5+i*40);
 	draw_line(&gv_fb_dev,0,30+5+6*40-1,240-1,30+5+6*40-1); //when i=6

	for(i=0;i<6;i++)
	 	draw_line(&gv_fb_dev,0+i*40,30+5,0+i*40,30+5+240-1);
 	draw_line(&gv_fb_dev,0+6*40-1,30+5,0+6*40-1,30+5+240-1); //when i=6

	/* draw wlines */
	fbset_color(WEGI_COLOR_ORANGE);//GREEN);
while(1)
{
        /* flush FB FILO */
	printf("start to flush filo...\n");
        fb_filo_flush(&gv_fb_dev);
	/* draw poly line with FB FILO */
        fb_filo_on(&gv_fb_dev);

	/* 1. generate sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		points[i].y=(rad*2*fp16_sin[(i*div+delt)%360]>>16)+40*3+35; /* 4*rad to shift image to Y+ */
	}
	fbset_color(WEGI_COLOR_BLUE);//GREEN);
	draw_pline(&gv_fb_dev, points, num, 5);

	/* 2. generate AN OTHER sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		points[i].y=(rad*fp16_sin[( i*div + (delt<<1) )%360]>>16)+40*3+35; /* 4*rad to shift image to Y+ */
	}
	fbset_color(WEGI_COLOR_ORANGE);//GREEN);
	draw_pline(&gv_fb_dev, points, num, 5);

	/* alway shut off filo after use */
	fb_filo_off(&gv_fb_dev);

	tm_delayms(80);
//	egi_sleep(0,0,150);

	delt+=16;
}
/* END >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
#endif


#if 0 /* <<<<<<<<<<<<<<  6. test line Chart  <<<<<<<<<<<<<<<*/
	int num=240/10+1; /* number of data */
	int cdata[240]={0}; /* 240 data */
	EGI_POINT points[240/1]; /* points */


while(1)
{
        /* flush FB FILO */
	printf("start to flush filo...\n");
        fb_filo_flush(&gv_fb_dev);
	/* draw poly line with FB FILO */
        fb_filo_on(&gv_fb_dev);

	/* ----------Prepare data and draw plines */
	fbset_color(WEGI_COLOR_ORANGE);//GREEN);
        for(i=0; i<num; i++)
	{
		points[i].x=i*(240/24); /* assign X */
		points[i].y=100+egi_random_max(80); /* assign Y */
	}
	printf("draw ploy lines...\n");
	draw_pline(&gv_fb_dev, points, num, 3);
	/* ----------Prepare data and draw plines */
	fbset_color(WEGI_COLOR_GREEN);
        for(i=0; i<num; i++)
	{
		points[i].x=i*(240/24); /* assign X */
		points[i].y=80+egi_random_max(120); /* assign Y */
	}
	printf("draw ploy lines...\n");
	draw_pline(&gv_fb_dev, points, num, 3);


        fb_filo_off(&gv_fb_dev);

	tm_delayms(55);
}

/* END >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
#endif


        /* <<<<<-----------------  EGI general release  ----------------->>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
        printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev); /* Flush FB filo if necessary */
        release_fbdev(&gv_fb_dev);
        printf("egi_end_touchread()...\n");
        egi_end_touchread();
        printf("egi_quit_log()...\n");
#if 0
        egi_quit_log();
        printf("<-------  END  ------>\n");
#endif

	return 0;
}



