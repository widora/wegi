/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Test EGI FBGEOM functions

Journal:
2021-08-30/31:
	1. Test egi_filled_triangle2/3()
2022-03-04/05:
	1. Test fdraw_dot()/fdraw_line()
2022-03-08:
	1. Test sin/cos graph with anti_aliasing effect.
2022-03-09:
	1. Test draw_filled_triangle() and draw_filled_circle()

Midas Zhou
-----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "egi_math.h"
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_timer.h"
#include "egi_math.h"

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

#if 0 /* <<<<<<<<<<<<<< TEST: fdraw_dot(), fdraw_line()  <<<<<<<<<<<<<<< */

	float ang;
	float kl=1.0; /* Slope of the line */
	float dk=0.1;
	float x=0.0;
	float step=0.5;
	float r;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev, true);
        fb_position_rotate(&gv_fb_dev, 0);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);



    #if 0  //////////////////////  fdraw_line(), fdraw_circle()  /////////////////
	//fdraw_line(&gv_fb_dev, 0,0, 320,240);

    while(1) {
	#if 1 /* Draw circle */
        clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY2);
	for( r=10; r<200; r+=5) {  //1.5
		printf("r=%f\n", r);
		fbset_color(egi_color_random(color_medium));
		//draw_circle(&gv_fb_dev, 160, 120, r);
		fdraw_circle(&gv_fb_dev, 160, 120, r);
	}
	sleep(2);
	#endif

	#if 1 /* Draw lines */
        clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY2);
	for( ang=0.0; ang<=360; ang+=3.0 ) {
		printf("ang=%f\n", ang);
		fbset_color(egi_color_random(color_light)); //medium));
		fdraw_line(&gv_fb_dev, 160, 120,
		//draw_line(&gv_fb_dev, 160, 120,
				160+200.0*cos(MATH_PI*ang/180.0), 120+200.0*sin(MATH_PI*ang/180.0) );
//		usleep(100000);
	}
	sleep(2);
	#endif

	/* Draw sin */
//	sleep(2);
    }
	exit(0);
    #endif


    #if 0  //////////////////////  fdraw_dot() /////////////////

    clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY2);
    fbset_color(WEGI_COLOR_ORANGE);

    while(1) {
	    //for(kl=0.0; kl<30.0; kl+=(kl<10?0.0025:0.1) )
	    for( ang=0.0, kl=0.0;
        	 ang<360.0;
		 ang+=1, kl=tan(MATH_PI*ang/180.0) )
    	    {
		printf("kl=%f\n", kl);

	        clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY2);

		//usleep(10000);
	        //fbset_color(egi_color_random(color_medium));

		/* Draw an oblique line, by drawing a series of float dots. */
	#if 0
		for(x=0; x<320.0; x+=step)
		    fdraw_dot(&gv_fb_dev, x, kl*x);
	#endif

		for(x=160; x<320.0; x+=step)
		    fdraw_dot(&gv_fb_dev, x, 120+kl*(x-160));
		for(x=160; x>0.0; x-=step)
		    fdraw_dot(&gv_fb_dev, x, 120-kl*(160-x));

		usleep(100000);
     	     }
   }
   exit(0);
   #endif  //////////////////////  fdraw_dot() /////////////////

#endif


#if 0 /* <<<<<<<<<<<<<< TEST: egi_filled_triangle3()  <<<<<<<<<<<<<<< */

	//void draw_filled_triangle3( FBDEV *fb_dev, int x0, int y0, int x1, int y1, int x2, int y2,
	//                            EGI_16BIT_COLOR color0, EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2 )

	int angle=0;
	//float x,y;
	EGI_POINT points[4];
	char strtmp[256];

   while(1) {

	printf("     ----- Anlge: %d -----\n", angle);

	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAYB);

	points[0].x=roundf(110.0*cos(MATH_PI*angle/180.0)+160.0);
	points[0].y=roundf(110.0*sin(MATH_PI*angle/180.0)+120.0);
	points[1].x=roundf(110.0*cos(MATH_PI*(angle+90)/180.0)+160.0);
	points[1].y=roundf(110.0*sin(MATH_PI*(angle+90)/180.0)+120.0);
	points[2].x=roundf(110.0*cos(MATH_PI*(angle+180)/180.0)+160.0);
	points[2].y=roundf(110.0*sin(MATH_PI*(angle+180)/180.0)+120.0);
	points[3].x=roundf(110.0*cos(MATH_PI*(angle+270)/180.0)+160.0);
	points[3].y=roundf(110.0*sin(MATH_PI*(angle+270)/180.0)+120.0);

	#if 1  /* ------- TEST: degenerated to a line ------------ */
	if( angle <180 )	/* --------  CASE: Tree points NO overlap ------- */
		draw_filled_triangle3(&gv_fb_dev, points[0].x, points[0].y, 160, 120, points[2].x, points[2].y,
					WEGI_COLOR_RED, WEGI_COLOR_BLUE, WEGI_COLOR_GREEN);
	else  			/* --------- CASE: Two points at SAME coords! ---------- */
		draw_filled_triangle3(&gv_fb_dev, points[0].x, points[0].y, points[2].x, points[2].y, points[2].x, points[2].y,
					WEGI_COLOR_GREEN, WEGI_COLOR_RED, WEGI_COLOR_BLUE);
	#endif

	#if 0  /* ------- TEST: joint part(line) ------------ */
	draw_filled_triangle3(&gv_fb_dev, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y,
					WEGI_COLOR_RED, WEGI_COLOR_GREEN, WEGI_COLOR_BLUE);

	draw_filled_triangle3(&gv_fb_dev, points[0].x, points[0].y, points[3].x, points[3].y, points[2].x, points[2].y,
					WEGI_COLOR_RED, WEGI_COLOR_GREEN, WEGI_COLOR_BLUE);
	#endif

//	fbset_color2(&gv_fb_dev, WEGI_COLOR_WHITE);
//	draw_triangle(&gv_fb_dev,points);
	sprintf(strtmp,"Angle: %d, %s\n", angle, angle>180?"Two pts":"Three pts");
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold,    /* FBdev, fontface */
               	                        16, 16, (const UFT8_PCHAR)strtmp, /* fw,fh, pstr */
                       	                320, 1, 0,                        /* pixpl, lines, fgap */
                               	        10, 5, 	                	  /* x0,y0, */
                                       	WEGI_COLOR_BLACK, -1, 255,        /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	fb_render(&gv_fb_dev);
  	tm_delayms(300);

	angle +=3;
	if(angle>360)angle=0;
  }
	exit(0);
#endif

#if 0 /* <<<<<<<<<<<<<< TEST: egi_filled_triangle2()  <<<<<<<<<<<<<<< */

	//void draw_filled_triangle2( FBDEV *fb_dev, float x0, float y0, float x1, float y1, float x2, float y2,
	//                            EGI_16BIT_COLOR color0, EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2 )

	int angle=0;
	float x,y;
	EGI_POINT points[3];

   while(1) {
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_BLACK);

	x=160.0*cos(MATH_PI*angle/180.0)+160.0;
	y=120.0*sin(MATH_PI*angle/180.0)+120.0;

	draw_filled_triangle2(&gv_fb_dev, x, y, 0.0f, 120.0f-30, 319.0f, 120.0f+30,
					WEGI_COLOR_RED, WEGI_COLOR_GREEN, WEGI_COLOR_BLUE);

	points[0].x=x; 		points[0].y=y;
	points[1].x=0; 		points[1].y=120-30;
	points[2].x=319; 	points[2].y=120+30;
	fbset_color2(&gv_fb_dev, WEGI_COLOR_WHITE);
	draw_triangle(&gv_fb_dev,points);

	fb_render(&gv_fb_dev);

	if( (angle>=180-20 && angle<=180+20)
 	    || angle<=20 || angle >=360-20 )
	 tm_delayms(200);
	  //egi_sleep(1,1,100);

	angle +=3;
	if(angle>360)angle=0;
  }
	exit(0);
#endif


#if 0	/* <<<<<<<<<<<<<<  test fb_position_rotate()  <<<<<<<<<<<<<<< */
	 int k=0;
	 EGI_IMGBUF *eimg=egi_imgbuf_readfile("/mmc/linuxpet.jpg");

	 //fb_init_FBbuffers(&gv_fb_dev);
    while(1) {
        fb_position_rotate(&gv_fb_dev, (k++)&3);
	fbclear_bkBuff(&gv_fb_dev, 0);
	egi_subimg_writeFB(eimg, &gv_fb_dev, 0, -1, 0, 0);    /* imgbuf, fbdev, subnum, subcolor, x0, y0 */
	//clear_screen(&gv_fb_dev,0);
	fb_page_refresh(&gv_fb_dev,0);
	egi_sleep(1,0,100);
    }
	exit(0);
#endif


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


#if 0 /* <<<<<<<<<<<<<<  3. test draw triangle  <<<<<<<<<<<<<<<*/
	int i;
	int j, k; /* j -- pixlen moved, 0 from right most, k -- triangles drawn before putting slagon */
	int count;
//	EGI_BOX box={ {0,0},{600-1,800-1}};
	EGI_BOX box={ {0-50,0-50},{320-1,240-1}};
	EGI_BOX tri_box;
	EGI_POINT pts[3]={ {50,50}, {100,100}, {75, 90}};
	bool antialias_on=true;
	EGI_CLOCK eclock={0};
	const UFT8_PCHAR slogan=(UFT8_PCHAR)"'技术需要沉淀，成长需要痛苦，成功需要坚持，敬仰需要奉献！'";
	int fw=30;
	int fh=30;
	int pixlen=FTsymbol_uft8strings_pixlen(egi_appfonts.regular, fw, fh, slogan);

	//fb_set_directFB(&gv_fb_dev,true);
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_DARKGRAY);
	fb_render(&gv_fb_dev);

    #if 0 /* TEST draw_filled_triangle() and draw_triangle()  -----------*/
	EGI_POINT ps[3]={ {160,50}, {50, 200}, {320-50, 200} };
	fbset_color(WEGI_COLOR_PINK);
	draw_filled_triangle(&gv_fb_dev, ps);
	fbset_color(WEGI_COLOR_BLACK);
	draw_triangle(&gv_fb_dev, ps);

	fb_render(&gv_fb_dev);
	exit(0);
    #endif

	fbset_color(WEGI_COLOR_BLACK);
	draw_filled_rect(&gv_fb_dev, 0,0,320-1,240-1);

	count=0;
	j=0; k=0;
	egi_clock_start(&eclock);
while(1) {

	if( egi_clock_peekCostUsec(&eclock)>=1000000 ) {
		printf("Draw speed: %d triangles per second.\n", count);
		egi_clock_stop(&eclock);
		egi_clock_start(&eclock);
		count=0;
	}

	egi_randp_inbox(&tri_box.startxy, &box);
	tri_box.endxy.x=tri_box.startxy.x+175;
	tri_box.endxy.y=tri_box.startxy.y+175;

	for(i=0;i<3;i++)
		egi_randp_inbox(pts+i, &tri_box);

        color=egi_color_random(color_all);
	fbset_color(color);

	gv_fb_dev.antialias_on=true; /* Apply fdraw_line() */

	draw_filled_triangle(&gv_fb_dev, pts);
	draw_triangle(&gv_fb_dev, pts);

	gv_fb_dev.antialias_on=false;

	//fb_render(&gv_fb_dev);

	if(k>5){
		fb_copy_FBbuffer(&gv_fb_dev,FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);

        #if 0 /* Put slagon */
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
                	                        fw, fh,(const UFT8_PCHAR)slogan,  /* fw,fh, pstr */
                        	                3200, 1, 0,                       /* pixpl, lines, fgap */
                                	        320-j, 110,                 /* x0,y0, */
                                        	WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
		if( j > 320 + pixlen ) {
			j=j-pixlen-320/2;
			/* Redraw, since above writeFB is invisiable */
	        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
        	        	                        fw, fh,(const UFT8_PCHAR)slogan,  /* fw,fh, pstr */
                	        	                3200, 1, 0,                       /* pixpl, lines, fgap */
                        	        	        320-j, 110,                 /* x0,y0, */
                                	        	WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                	        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
		}
		else if( j > 320/2 + pixlen ) {
	        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
        	        	                        fw, fh,(const UFT8_PCHAR)slogan,  /* fw,fh, pstr */
                	        	                3200, 1, 0,                       /* pixpl, lines, fgap */
                        	        	        320-(j-pixlen-320/2), 110,                 /* x0,y0, */
                                	        	WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
	                                	        NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

		}
   	#endif /* END slogan */

		fb_render(&gv_fb_dev);
		//usleep(60000);
		tm_delayms(50);
		k=0;

		fb_copy_FBbuffer(&gv_fb_dev,FBDEV_BKG_BUFF, FBDEV_WORKING_BUFF);
	}

	count++;
	j+=1;
	k++;
}
	return 0;
#endif


#if 0  /* <<<<<<<<<<<<<<  test anti_aliasing effect  <<<<<<<<<<<<<<<*/
	int i,k;
	int r=200;//75,180
	int xc=160,yc=120;
	int x1,x2,y1,y2;
	int tmp[4]={3, 180-3, 93,180-93};
	bool antialias_on=true;

        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0) {
                printf("%s: Start to create fixed point trigonometric table...\n",__func__);
                mat_create_fpTrigonTab();
        }

while(1) {
	fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY);

	/* Tip */
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
        	                        24, 24,				  /* fw,fh */
					antialias_on?(UFT8_PCHAR)"Antialias_ON":(UFT8_PCHAR)"Antialias_OFF", /* pstr */
       	        	                300, 1, 0,                        /* pixpl, lines, fgap */
               	        	        10, 6, 	               		  /* x0,y0, */
                              	        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
	                               	NULL, NULL, NULL, NULL );         /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */

	/* Draw lines with/without anti_aliasing effect */
	fbset_color(egi_color_random(color_deep));
	for(i=0; i<180; i+=3) {
		x1=xc+(r*fp16_cos[i]>>16);
		y1=yc-(r*fp16_sin[i]>>16);
		x2=xc+(r*fp16_cos[i+180]>>16);
		y2=yc-(r*fp16_sin[i+180]>>16);

		printf("i=%d: line (%d,%d)-(%d,%d) \n", i,x1,y1,x2,y2);

		gv_fb_dev.antialias_on=antialias_on;
		//fbset_color(egi_color_random(color_deep));
		if(antialias_on) {
			draw_line_antialias(&gv_fb_dev, x1,y1, x2,y2);	/* Note: LCD coord sys! */
			//draw_wline(&gv_fb_dev, x1,y1, x2,y2,5);
		}
		else
			draw_line(&gv_fb_dev, x1,y1, x2,y2);	/* Note: LCD coord sys! */
		gv_fb_dev.antialias_on=false;

		fb_render(&gv_fb_dev);
	}

	/* Tip */
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular, /* FBdev, fontface */
        	                        24, 24,				  /* fw,fh */
					antialias_on?(UFT8_PCHAR)"Antialias_ON":(UFT8_PCHAR)"Antialias_OFF", /* pstr */
       	        	                300, 1, 0,                        /* pixpl, lines, fgap */
               	        	        10, 6, 	                  /* x0,y0, */
                              	        WEGI_COLOR_WHITE, -1, 255,        /* fontcolor, transcolor,opaque */
	                               	NULL, NULL, NULL, NULL);          /*  *charmap, int *cnt, int *lnleft, int* penx, int* peny */
	fb_render(&gv_fb_dev);

	/* Hold on */
	sleep(2);

	/* Toggle */
	antialias_on=!antialias_on;
}

	fb_render(&gv_fb_dev);

#endif


#if 0  /* <<<<<<<<<<<<<<  test draw_circle()/draw_filled_circle()  <<<<<<<<<<<<<<<*/
	int k;
	int x0,y0, r;
	int rmax, rmin;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev, false);
        fb_position_rotate(&gv_fb_dev, 0);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);

	clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK);
	fbset_color(WEGI_COLOR_RED);

	/* Antialiasing ON */
	gv_fb_dev.antialias_on=true;

	#if 0 /*  */
  	while(1) {
		rmin=mat_random_range(40)+1;
		rmax=mat_random_range(50)+60;
		fbset_color(egi_color_random(color_medium));
		for(r=rmin; r<rmax; r++)
			draw_circle(&gv_fb_dev, 160, 120, r);

		fb_render(&gv_fb_dev);
		tm_delayms(75);
  	}
	#endif

 	while(1) {
		r=mat_random_range(50)+2;
		x0=mat_random_range(320);
		y0=mat_random_range(240);
		fbset_color(egi_color_random(color_all));
		//fdraw_circle(&gv_fb_dev, x0, y0, r);
		draw_filled_circle(&gv_fb_dev, x0, y0, r);

		fb_render(&gv_fb_dev);
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


#if 0  /* <<<<<<<<<<<<<<  5. test draw  arc  <<<<<<<<<<<<<<<*/
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
  	fb_page_refresh(&gv_fb_dev,0);
	tm_delayms(10);
 }

 /* wedge */
// for(ka=-MATH_PI/20*4; ka< 6/5*MATH_PI; ka += MATH_PI/60 ) {
 for(i=-2; i<12; i++) {
 	fbset_color(WEGI_COLOR_PINK); //egi_color_random(color_deep));
	draw_filled_pieSlice(&gv_fb_dev, x0, y0, 65, MATH_PI/10*i, MATH_PI/10*(i+1));  //MATH_PI*2.0-ka);//  0.5-da );
 	//display_slogan();
  	fb_page_refresh(&gv_fb_dev,0);
	tm_delayms(10);
 }

 //display_slogan();
 fb_page_refresh(&gv_fb_dev,0);

 tm_delayms(3000);

}

#endif


#if 1  /* <<<<<<<<<<<<<<  6. test draw_wline & draw_pline  <<<<<<<<<<<<<<<*/

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev, false);
        fb_position_rotate(&gv_fb_dev, 0);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);

   #if 1   /* ---- draw_wline() --------- */
	int k;
	int w;
	EGI_POINT p1,p2;
	//EGI_BOX box={{0,0},{240-1,320-1,}};
	EGI_BOX box={{0,0},{320-1,240-1}};

        /* Antialias ON, to apply fdraw_line(). */
        gv_fb_dev.antialias_on=true;

   while(1) {
	for(w=1; w<15; w++) {
	   printf("w=%d\n", w);
           clear_screen(&gv_fb_dev,WEGI_COLOR_DARKGRAY2);

	   fbset_color(egi_color_random(color_all));
	   for(k=0; k<180; k+=10) {
		p1.x=160+110*sin(MATH_PI*k/180);
		p1.y=120-110*cos(MATH_PI*k/180);
		p2.x=160-110*sin(MATH_PI*k/180);
		p2.y=120+110*cos(MATH_PI*k/180);

		draw_wline(&gv_fb_dev,p1.x,p1.y,p2.x,p2.y,w);

		fb_render(&gv_fb_dev);
		//usleep(200000);
	   }
	   sleep(3);
	}
   }

     #if 0
	while(1)
  	{
		egi_randp_inbox(&p1, &box);
		egi_randp_inbox(&p2, &box);
		fbset_color(egi_color_random(color_all));
		w=egi_random_max(20);
		printf("w=%d \n",w);
		draw_wline(&gv_fb_dev,p1.x,p1.y,p2.x,p2.y,w);

		fb_render(&gv_fb_dev);
		usleep(200000);
  	}
    #endif

  #endif

	int i;
	int rad=50; /* radius */
	int div=4;/* 4 deg per pixel, 240*2=480deg */
	int num=320/1; //240/1; /* number of points */
	EGI_POINT points[320/1]; //[240/1]; /* points */
	int delt=0;

	mat_create_fpTrigonTab();/* create lookup table for trigonometric funcs */

	/* clear areana */
	fbset_color(WEGI_COLOR_BLACK);
	//draw_filled_rect(&gv_fb_dev,0,30,240-1,320-100);
	draw_filled_rect(&gv_fb_dev,0,0,320-1,240-1);

	/* draw a square area of grids */
	fbset_color(WEGI_COLOR_GRAY2);//GREEN);
	for(i=0;i<6;i++) {
	 	//draw_line(&gv_fb_dev,0,30+5+i*40,240-1,30+5+i*40);
	 	draw_line(&gv_fb_dev,0, i*40,320-1, i*40);
	}
 	//draw_line(&gv_fb_dev,0,30+5+6*40-1,240-1,30+5+6*40-1); //when i=6
 	draw_line(&gv_fb_dev,0, 6*40-1, 320-1, 6*40-1); //when i=6

	//for(i=0;i<6;i++) {
	for(i=0;i<8;i++) {
	 	//draw_line(&gv_fb_dev,0+i*40,30+5,0+i*40,30+5+240-1);
	 	draw_line(&gv_fb_dev, i*40, 0, i*40, 240-1);
	}
 	//draw_line(&gv_fb_dev,0+6*40-1,30+5,0+6*40-1,30+5+240-1); //when i=6
 	draw_line(&gv_fb_dev, 8*40-1, 0, 8*40-1, 240-1); //when i=8

	/* Save as BKG */
	//fb_copy_FBbuffer(&gv_fb_dev, FBDEV_WORKING_BUFF, FBDEV_BKG_BUFF);

	/* draw wlines */
	fbset_color(WEGI_COLOR_ORANGE);//GREEN);

   while(1)
   {
        /* flush FB FILO */
	printf("start to flush filo...\n");
        fb_filo_flush(&gv_fb_dev);
	/* draw poly line with FB FILO */
        fb_filo_on(&gv_fb_dev);

	/* Antialias ON, to apply fdraw_line(). */
	gv_fb_dev.antialias_on=true;

	/* 1. generate sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		//points[i].y=(rad*2*fp16_sin[(i*div+delt)%360]>>16)+40*3+35; /* 4*rad to shift image to Y+ */
		points[i].y=(rad*2*fp16_sin[(i*div+delt)%360]>>16)+40*3; /* shift up axis to center */
	}
	fbset_color(WEGI_COLOR_PINK); //BLUE);//GREEN);
	draw_pline(&gv_fb_dev, points, num, 5);

	/* 2. generate 2nd sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		points[i].y=(rad*6/5*fp16_sin[(i*div*3+delt*4)%360]>>16)+40*3; /* shift up axis to center */
	}
	fbset_color(WEGI_COLOR_YELLOW);//GRAYB);
	draw_pline(&gv_fb_dev, points, num, 1); //5);

	/* 3. generate 3rd sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		points[i].y=(rad*3/2*fp16_sin[(i*div/2+delt)%360]>>16)+40*3; /* shift up axis */
	}
	fbset_color(WEGI_COLOR_RED); //BLUE);//GREEN);
	draw_pline(&gv_fb_dev, points, num, 1); //5);

	/* 4. generate sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		//points[i].y=(rad*fp16_sin[( i*div + (delt<<1) )%360]>>16)+40*3+35; /* 4*rad to shift image to Y+ */
		points[i].y=(rad*fp16_sin[( i*div + (delt<<1) )%360]>>16)+40*3; /* shift up axis */
	}
	fbset_color(WEGI_COLOR_ORANGE);//GREEN);
	draw_pline(&gv_fb_dev, points, num, 1); //5);

	/* 5. generate 2nd sin() points  */
	for(i=0;i<num;i++) {
		points[i].x=i;
		//points[i].y=(rad/2*fp16_sin[( i*div + (delt<<2) )%360]>>16)+40*3+35; /* 4*rad to shift image to Y+ */
		points[i].y=(rad/2*fp16_sin[( i*div + (delt<<2) )%360]>>16)+40*3; /* shift up axis */
	}
	fbset_color(WEGI_COLOR_GREEN);//GREEN);
	draw_pline(&gv_fb_dev, points, num, 1); //5);

	/* Antialias OFF */
	gv_fb_dev.antialias_on=true;

	/* alway shut off filo after use */
	fb_filo_off(&gv_fb_dev);

	fb_render(&gv_fb_dev);
//	tm_delayms(80);
//	egi_sleep(0,0,150);

	delt+=2; //16;
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



