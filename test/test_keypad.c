/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


An example of alphabets/numbers keypad input.


Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <getopt.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_gif.h"

#define  KEYIN_MAX_LEN	128+1


void write_tag( int keynum, int x0, int y0, int fw, int fh, const wchar_t *pustr, EGI_16BIT_COLOR color );

int main(int argc, char **argv)
{
	int i,j;
	int nrow=0,ncol=0;
	EGI_TOUCH_DATA touch_data;        /*  touch_data */
	EGI_IMGBUF 	*imgmask;
	char keybuff[KEYIN_MAX_LEN]={0};
	int  pbuf=0;

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();                     /* start sys tick */

#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_gif") != 0) {        /* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif

        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {       /* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }

        if(FTsymbol_load_appfonts() !=0 ) {     /* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }

        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )            /* init sys FB */
                return -1;

        printf("start touchread thread...\n");
        egi_start_touchread();                  /* start touch read thread */

        /* <<<<------------------  End EGI Init  ----------------->>>> */

	/* create keypad mask */
	imgmask=egi_imgbuf_create( 48, 40, 150, WEGI_COLOR_GRAY2);
	if(imgmask==NULL)
		goto RELEASE_EGI;


	/* keychars charts/tables */
	const char keychars_symbols_upper[]=
	{
		'0','1','2','3','4','5',
		'6','7','8','9','(',')',
		',','!','"','.','/','%',
		':','<','>','-','+','=',
		'?',' ',' ',' ',' ',' ',	/* ?, SHIFT(blue effecive), SP, BKSP,RET, 英 */
	};
	const char keychars_symbols_lower[]=
	{
		';',0x27,0x5C,0x7C,'~','$',  // 0x27--', 0x5C--\,  0x7C--|
		'@','#','^','_','{','}',
		'[',']','`','`','&','*',
		' ',' ',' ',' ',' ',' ',
		'\n',' ',' ',' ',' ',' ',
	};


        const char keychars_alphabets_upper[]=
        {
		'A','B','C','D','E','F',
		'G','H','I','J','K','L',
		'M','N','O','P','Q','R',
		'S','T','U','V','W','X',
		' ',' ',' ',' ','Y','Z',	/* 符, SHIFT(blue effectiv), SP, BKSP, Y，Z */
        };
        const char keychars_alphabets_lower[]=
        {
                'a','b','c','d','e','f',
		'g','h','i','j','k','l',
		'm','n','o','p','q','r',
		's','t','u','v','w','x',
		' ',' ',' ',' ','y','z',
        };

	/* 2 keypad pages */
	int KeyPage=0;	/* 0--alphabets, 1--symbols */
	bool Is_PageShift=false;
	int KeyCase=0;	/* 0--upper case, 1--lower case */
	/* 4 key charts */
	const char *keycharts[2][2]={ { keychars_symbols_upper, keychars_symbols_lower}, { keychars_alphabets_upper, keychars_alphabets_lower } };
	const char *pchart=NULL;
	bool	Is_ControlKey=false;
	bool	Is_ShiftDown=false;
	bool	Is_Flicker=false;
	char keystr[2]={0};
	int penx=0,peny=0;
	int count=0;		/* Key input char counter */
	int lnleft;

	/* Set FB mode: default */
        //fb_set_directFB(&gv_fb_dev, true);
        //fb_position_rotate(&gv_fb_dev,3);

	/* <<<<<<<<<<<<<<<<<<<<<<<<   Draw Alphabets keypad  >>>>>>>>>>>>>>>>>>>> */
	//gv_fb_dev.map_bk=gv_fb_dev.map_buff+gv_fb_dev.screensize*2;
	fb_shift_buffPage(&gv_fb_dev, 2); /* ship map_bk to 2nd bkground buffer */
	draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAYB, 0, 0, 239, 79);	/* color zones */
	draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_WHITE, 0, 80, 239, 319);
	fbset_color(WEGI_COLOR_GRAY);
	for(i=0; i<5; i++) {		/* row */
		for(j=0; j<6; j++) {	/* column */
			draw_wrect(&gv_fb_dev, j*40, 80+i*48, (j+1)*40, 80+(i+1)*48, 1);

			/* draw upper cases */
			keystr[0]=keychars_alphabets_upper[6*i+j];
        		FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold,      	/* FBdev, fontface */
	                                	        16, 16, (const unsigned char *)keystr,  /* fw,fh, pstr */
        	                                	40, 1, 0,                           	/* pixpl, lines, gap */
	                	                        5+40*j, 80+5+48*i, 		/* x0,y0, */
        	                	                WEGI_COLOR_BLACK, -1, -1,      		/* fontcolor, transcolor,opaque */
                	                	        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
			/* darw lower cases */
			keystr[0]=keychars_alphabets_lower[6*i+j];
        		FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular,      	/* FBdev, fontface */
	                                	        16, 16, (const unsigned char *)keystr,  /* fw,fh, pstr */
        	                                	40, 1, 0,                           	/* pixpl, lines, gap */
	                	                        24+40*j, 80+22+48*i, 			/* x0,y0, */
        	                	                WEGI_COLOR_GRAY3, -1, -1,      	/* fontcolor, transcolor,opaque */
                	                	        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
		}
	}
	/* The last row: 符, SHIFT(blue effectiv), SP, BKSP, Y，Z */
	write_tag(24+0, 10, 12, 20, 20, (const wchar_t *)L"符", WEGI_COLOR_BLUE );  /* keynum, x0,y0,fw,fh, pustr */
        write_tag(24+1, 0, 16, 12, 12, (const wchar_t *)L"SHIFT", WEGI_COLOR_GRAYA );  /* keynum, x0,y0,fw,fh, pustr */
        write_tag(24+2, 12, 13, 16, 16, (const wchar_t *)L"SP", WEGI_COLOR_PURPLE);
        write_tag(24+3, 8, 14, 16, 16, (const wchar_t *)L"<=", WEGI_COLOR_PURPLE);


	fb_page_refresh(&gv_fb_dev,2);
	sleep(1);


	/* <<<<<<<<<<<<<<<<<<<<<<<<   Draw Symbols keypad  >>>>>>>>>>>>>>>>>>>> */
	fb_shift_buffPage(&gv_fb_dev, 1); /* put to 1st bkground buffer */
	draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_GRAYB, 0, 0, 239, 79);	/* color zones */
	draw_filled_rect2(&gv_fb_dev,WEGI_COLOR_WHITE, 0, 80, 239, 319);
	fbset_color(WEGI_COLOR_GRAY);

	for(i=0; i<5; i++) {		/* row */
		for(j=0; j<6; j++) {	/* column */
			draw_wrect(&gv_fb_dev, j*40, 80+i*48, (j+1)*40, 80+(i+1)*48, 1);

			/* draw upper cases */
			keystr[0]=keychars_symbols_upper[6*i+j];
        		FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.bold,      	/* FBdev, fontface */
	                                	        16, 16, (const unsigned char *)keystr,  /* fw,fh, pstr */
        	                                	40, 1, 0,                           	/* pixpl, lines, gap */
	                	                        5+40*j, 80+5+48*i, 		/* x0,y0, */
        	                	                WEGI_COLOR_BLACK, -1, -1,      	/* fontcolor, transcolor,opaque */
                	                	        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
			/* darw lower cases */
			keystr[0]=keychars_symbols_lower[6*i+j];
        		FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular,      	/* FBdev, fontface */
	                                	        16, 16, (const unsigned char *)keystr,  /* fw,fh, pstr */
        	                                	40, 1, 0,                           	/* pixpl, lines, gap */
	                	                        24+40*j, 80+22+48*i, 			/* x0,y0, */
        	                	                WEGI_COLOR_GRAY3, -1, -1,      	/* fontcolor, transcolor,opaque */
                	                	        NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
		}

	}

	/* The last row: '?, SHIFT(blue effecive), SP, BKSP,RET, 英' */
	write_tag(24+1, 0, 16, 12, 12, (const wchar_t *)L"SHIFT",WEGI_COLOR_GRAYA );  /* keynum, x0,y0,fw,fh, pustr */
	write_tag(24+2, 12, 13, 16, 16, (const wchar_t *)L"SP",WEGI_COLOR_PURPLE );
	write_tag(24+3, 8, 14, 16, 16, (const wchar_t *)L"<=",WEGI_COLOR_PURPLE );
	write_tag(24+4, 8, 16, 12, 12, (const wchar_t *)L"RET",WEGI_COLOR_DARKRED );
	write_tag(24+5, 10, 12, 20, 20, (const wchar_t *)L"英", WEGI_COLOR_BLUE );

	fb_page_refresh(&gv_fb_dev,1);
	/* push to FB back ground buffer */
	//fb_page_saveToBuff(&gv_fb_dev, 1);

	/* shift to FB working buffer */
	fb_shift_buffPage(&gv_fb_dev, 0);

	nrow=0; ncol=0;
	KeyPage=0;  /* 0 alphabets page */
	KeyCase=0;  /* 0 upper case */

	/* --------------------------- Loop Input Process ----------------------------- */
while(1) {
	/* pointer to keychars chart */
	pchart=keycharts[KeyPage][KeyCase];

	/* Refresh FB working buffer with 1ST background buffer, --- Alphabets Page --- */
 	memcpy(gv_fb_dev.map_bk, gv_fb_dev.map_buff+gv_fb_dev.screensize*(KeyPage+1), gv_fb_dev.screensize);

	/* --- To speed up, Refresh Blocks only --- */
	/* Refresh text box */
//	fb_lines_refresh(&gv_fb_dev, KeyPage+1, 0, 80 ); /* FBDEV*, numpg,  sind, n */

	/* For released key: to restore row */
	fb_lines_refresh(&gv_fb_dev, KeyPage+1, 80+nrow*48, 48 ); /* FBDEV*, numpg,  sind, n */

	/* display chars in keybuff[] */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular,      	/* FBdev, fontface */
                               	        20, 20, (const unsigned char *)keybuff,  		    /* fw,fh, pstr */
                                	240-5, 3, 3,                           	/* pixpl, lines, gap */
               	                        5, 5,          				/* x0,y0, */
	               	                WEGI_COLOR_BLACK, -1, -1,      		/* fontcolor, transcolor,opaque */
      	                	        &count, &lnleft, &penx, &peny );      	/* int *cnt, int *lnleft, int* penx, int* peny */


	/* --- TEST: Draw cursor --- */
	if( lnleft==0 && penx > 240-20-5 ) {
		printf("--- Input Box Full ---\n");
	}
	else {
		printf("penx=%d, peny=%d\n",penx, peny);
		Is_Flicker=!Is_Flicker;
		if(Is_Flicker)
              		FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_appfonts.regular,      /* FBdev, fontface */
                                        	20, 20, (const unsigned char *)"|", //keybuff,                     /* fw,fh, pstr */
                                        	20, 1, 0,                       /* pixpl, lines, gap */
                                        	penx, peny,          		/* x0,y0, */
                                        	WEGI_COLOR_RED, -1, -1,      	/* fontcolor, transcolor,opaque */
                                        	NULL, NULL, NULL, NULL);      	/* int *cnt, int *lnleft, int* penx, int* peny */
	}

	/* Draw SHIFT indicator dot */
	draw_blend_filled_circle(&gv_fb_dev, 60, (80+4*48)+8+KeyCase*30, 3, WEGI_COLOR_BLUE, 200);

	/* Refresh page */
	//fb_page_refresh(&gv_fb_dev,0);

	/* --- To speed up, Refresh Blocks only --- */
	/* Refresh text box */
	if(Is_PageShift) {				/* refresh whole page */
		fb_page_refresh(&gv_fb_dev,0);
		Is_PageShift=false;
	} else {
		fb_lines_refresh(&gv_fb_dev, 0, 0, 80);  	/* TXT BOX */
		fb_lines_refresh(&gv_fb_dev, 0, 320-48, 48);    /* Control key row */
	}

	/* wait for touch event */
 	if( egi_touch_timeWait_press(0,500, &touch_data)==0 ) {
                        printf(" >>>>>  Press touching pxy(%d,%d)\n",
                                                touch_data.coord.x, touch_data.coord.y);
			/* cal. row and column number of the keypad */
			nrow=(touch_data.coord.y-80)/48;
			ncol=(touch_data.coord.x/40);
			if(nrow<0)
				continue;

			/* If control keys */
			switch(6*nrow+ncol) {
				case 24:		/* page shift key in Alphabet page */
					if(KeyPage==1) {
						KeyPage=0; Is_PageShift=true;
						KeyCase=0;
						Is_ControlKey=true;
						//continue;
					}
					break;
				case 25:		/* SHIFT */
					Is_ControlKey=true;
					KeyCase=!KeyCase; //~(KeyCase&0x1);	/* SHIFT KeyCase 0,1 */
					Is_ShiftDown=!Is_ShiftDown;
					break;
				case 27:		/* BACKSPACE in Alphabet/Symbol page */
					if(pbuf > 0)
						keybuff[--pbuf]='\0';
					Is_ControlKey=true;
					break;
				case 29:		/* page shift key in Symbol page */
					if(KeyPage==0) {
						KeyPage=1; Is_PageShift=true;
						KeyCase=0;
						Is_ControlKey=true;
						//continue;
					}
					break;

				default:
					Is_ControlKey=false;
					break;
			}

			/* put a gray mask on the touched key */
	        	egi_imgbuf_windisplay( imgmask, &gv_fb_dev, -1,   	/* EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int subcolor */
                               		       0, 0, ncol*40, 80+nrow*48,	/* int xp, int yp, int xw, int yw, */
                               		       40, 48 );		 	/* int winw, int winh */

			//fb_page_refresh(&gv_fb_dev,0);
			/* Refresh touched key row only */
			fb_lines_refresh(&gv_fb_dev, 0, 80+nrow*48, 48 ); /* FBDEV*, numpg,  sind, n */

			/* push key char to keybuffer */
			if(!Is_ControlKey) {
				if( pbuf < KEYIN_MAX_LEN )
					keybuff[pbuf++]=pchart[6*nrow+ncol];
			}

			/* wait for release */
			while( egi_touch_timeWait_release(0, 500, &touch_data) !=0 );

	}
} /* End while() */


RELEASE_EGI:

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


/*--------------------------------------------------------
Write tag on key.

keynum:		key index 0-29
x0,y0:		offset from left top point of the key
fw,fh:		Font width and height
pustr:		Pointer to a string of wchar_t

---------------------------------------------------------*/
void write_tag( int keynum, int x0, int y0, int fw, int fh, const wchar_t *pustr, EGI_16BIT_COLOR color )
{
	/* relative to key origin */
	x0=keynum%6*40+x0;
	y0=keynum/6*48+y0+80;

	/* write to FB buffer */
        FTsymbol_unicstrings_writeFB(&gv_fb_dev, egi_appfonts.bold,     /* FBdev, fontface */
                                          fw, fh, (const wchar_t *)pustr,                /* fw,fh, pstr */
                                          240, 1,  0,                 /* pixpl, lines, gap */
                                          x0, y0,                      /* x0,y0, */
                                          color, -1, -1 );   /* fontcolor, stranscolor,opaque */
}
