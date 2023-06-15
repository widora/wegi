/*---------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Neural Network Description:
   Input(64) |<<<    Hidden_Layer(64i,20o)    >>>|<<<   Hidden_Layer(20i,20o)  >>>|<<<  Output_Layer(20i,10o)  >>>|


Reference:
1. Neural Network Training Model:
   https://peterroelants.github.io/posts/neural-network-implementation-part05/

2. MNIST handwrriten digit database:
   http://yann.lecun.com/exdb/mnist/

3. scikit-learn digit data:
   import sklearn
   from sklearn import datasets
   digits = datasets.load_digits()
   print(digits.data[k])


Note:
1. Only consider linear X/Y factors for Touch_XY to Screen_XY mapping.
2. Center point x[4]y[4] is the base point for coordinate mapping/converting.
3. Simple, but not good for average touch pad.

		X--- Calibration refrence points
		       ( A 320x240 LCD )

   X <-------------------  Max. 320 ------------------> X
   ^  x[2]y[2]				      x[0]y[0]  ^
   |							|
   |							|
   |							|
  Max. 240		   X x[4]y[4]		     Max. 240
   |							|
   |							|
   |							|
   V   x[3]y[3]				      x[1]y[1]	V
   X <-------------------  Max. 320 ------------------> X

   factX=dLpx/dTpx   factY=dLpy/dTpy  ( dLpx --- delta LCD px, dTpy --- delta TOUCH PAD py )


Journal:
2023-05-19: Create the file.
2023-06-13:
	1. Draw color strips according to nvcells.dout values.
2023-06-14:
	1. sklearn 8*8 handwritten data set test.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <xpt2046.h>
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_FTsymbol.h>
#include <endian.h>

#include "digits_data.h"
#include "nnc_digits.h"


static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color);
static void write_digit(int k, int px, int py, EGI_16BIT_COLOR color);


#define DIGITS_DATASET_TEST 1


/*------------------
    	MAIN
-------------------*/
int main(int argc, char **argv)
{

 /* <<<<<<  EGI general init  >>>>>> */

  /* Start sys tick */
  printf("tm_start_egitick()...\n");
  tm_start_egitick();

  #if 0
  /* Start egi log */
  printf("egi_init_log()...\n");
  if(egi_init_log("/mmc/log_scrollinput")!=0) {
        printf("Fail to init egi logger, quit.\n");
        return -1;
  }
  #endif

  #if 0
  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }
  #endif

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;


  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,true);
  fb_position_rotate(&gv_fb_dev, 0);  /* relative to touch_coord */

  /* Create a fbimg to link to FB data */
  EGI_IMGBUF *fbimg=NULL;
  fbimg=egi_imgbuf_alloc();
  if(fbimg==NULL) return -1;
  fbimg->width=gv_fb_dev.pos_xres;
  fbimg->height=gv_fb_dev.pos_yres;
  fbimg->imgbuf=(EGI_16BIT_COLOR*)gv_fb_dev.map_fb;

#if 0/* TEST: ------ Reset xpt factors aft. egi_start_touchread()! --------- */
  sleep(1); /* Just wait to let egi_start_touchread() finish calling xpt_set_factors() */
  xpt_set_factors(2.1667, 3.0, 60, 56);
  //factX=2.166667, factY=3.000000  tbaseX=60 tbaseY=56
  //factX=2.194805, factY=3.000000  tbaseX=61 tbaseY=57
#endif

 /* <<<<<  End of EGI general init  >>>>>> */


    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int k;
	int ret;
	EGI_TOUCH_DATA	touch;
	EGI_POINT	startxy;
	EGI_POINT	endxy;
	EGI_BOX		btn_confirm ={ {190, 100},{190+120, 100+50} };  //{ {65, 190},{65+120, 190+50} };
	EGI_BOX		wbox;		/* Write arae */
	wbox.startxy.x=240; //35;
	wbox.startxy.y=35; //120
	wbox.endxy.x=wbox.startxy.x+41; //80;
	wbox.endxy.y=wbox.startxy.y+41; //80;

	int wboxH=wbox.endxy.y-wbox.startxy.y+1;
	int wboxW=wbox.endxy.x-wbox.startxy.x+1;

	EGI_IMGBUF *digitimg=NULL;
	EGI_IMGBUF *recimg=NULL;   /* Image for recogination */
	EGI_IMGBUF *recsimg=NULL;  /* stretched recimg */

	EGI_16BIT_COLOR tcolor;

	/* dout imag and its stretched image */
	EGI_IMGBUF *doutimg0=NULL, *doutsimg0=NULL;
	EGI_IMGBUF *doutimg1=NULL, *doutsimg1=NULL;
	EGI_IMGBUF *doutimg2=NULL, *doutsimg2=NULL;

	double imgdata[8*8]; /* [0-16] Gray value for a 8x8 image */
	int digit=-1;


        const int layer0_nin=64, layer0_nout=20;
        const int layer1_nin=20, layer1_nout=20;
        const int layer2_nin=20, layer2_nout=10;
	float *dout=NULL;


	/* Allocate dout */
        dout=calloc(layer0_nout+layer1_nout+layer2_nout, sizeof(float));
	if(dout==NULL) exit(1);

	/* Allocate doutimgs */
	doutimg0=egi_imgbuf_createWithoutAlpha(20,1, WEGI_COLOR_BLACK); /* H,W, color */
	doutimg1=egi_imgbuf_createWithoutAlpha(20,1, WEGI_COLOR_BLACK);
	doutimg2=egi_imgbuf_createWithoutAlpha(10,1, WEGI_COLOR_BLACK);
	if(doutimg0==NULL || doutimg1==NULL || doutimg2==NULL)
		exit(1);


#if 0  ////////////* Open MNIST handwriten digit database *//////////////////
	/* **
	  [offset]   [type]          [value]             [description]
	  0000	   32bit integer   0x00000803(2051)     magic number
     	  0004     32bit integer   10000		number of images
	  0008     32bit integer   28	                number of rows
	  0012     32bit integer   28                   number of columns
	  (followings are all pixel data -----: hdiimgdata )
	  0016     unsigned byte   ??                   pixel
	  0017     unsigned byte   ??                   pixel
	  ... ...
	 */
	unsigned int magic;
	unsigned int imgcnt;
	unsigned int  nr,nc;
	unsigned char *hdimgdata;
	unsigned int offset;      /* offset from hdimgdata */
	EGI_FILEMMAP *fmap=NULL;
	EGI_IMGBUF *hdimg=NULL;

	/* Open MNIST data file */
	fmap=egi_fmap_create("/mmc/t10k-images.idx3-ubyte", 0, PROT_READ, MAP_SHARED);
	if(fmap==NULL) exit(1);

	/* Read MNIST data header */
	magic=be32toh( *(unsigned int*)(fmap->fp+0) );
	imgcnt=be32toh( *(unsigned int*)(fmap->fp+4) );
	nr=be32toh( *(unsigned int*)(fmap->fp+8) );
	nc=be32toh( *(unsigned int*)(fmap->fp+12) );
	printf("MNIST t10k-images: magic=%d, imgcnt=%d, nr=%d, nc=%d\n", magic, imgcnt, nr,nc);

	/* Assign hdimgdata */
	hdimgdata=(unsigned char*)fmap->fp+16;

#endif

START_LOOP:

        /* Clear screen first */
        clear_screen(&gv_fb_dev,WEGI_COLOR_BLACK); //DARKGRAY);

	/* neural network model description */
	draw_filled_rect2(&gv_fb_dev, WEGI_COLOR_GRAY2, 0,0, 320-1, 20-1);
       	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
       	                                15, 15, (unsigned char *)"3-Layer Neural Network Demo",
					//(unsigned char *)"NN(64i,20o)+NN(20i,20o)+NN(20i,10o)",    /* fw,fh, pstr */
               	                        320, 1, 4,                                  /* pixpl, lines, gap */
                       	                30, 0,                                    /* x0,y0, */
                               	        WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */

	/* Draw handwriting box */
	fbset_color(WEGI_COLOR_GREEN);
	//draw_rect(&gv_fb_dev, Lpx[0], Lpy[0], Lpx[3], Lpy[3]);
	draw_rect(&gv_fb_dev, wbox.startxy.x, wbox.startxy.y, wbox.endxy.x, wbox.endxy.y);

	/* Draw last recsimg */
	//egi_subimg_writeFB(recimg, &gv_fb_dev, 0, -1, 10, 60);  /* 8x8 image for recognition */
	egi_subimg_writeFB(recsimg, &gv_fb_dev, 0, -1, 5, 90);  /* Stretched 8x8 image */

	/* Show last digitimg copied from writeBOX */
	egi_subimg_writeFB(digitimg, &gv_fb_dev, 0, -1, 10, 140); //240-50);
	//egi_subimg_writeFB(digitimg, &gv_fb_dev, 0, -1,  btn_confirm.endxy.x+30+30, btn_confirm.startxy.y);  /* Copy image in writeBox */


#if 0   ///////////////////* Random MNIST handwriten digit image into hdimg */////////////////////
	egi_imgbuf_free2(&hdimg);
	hdimg=egi_imgbuf_createWithoutAlpha(nr, nc, WEGI_COLOR_WHITE); /* nr=nc=28 */

 READ_MNIST:
	offset = nr*nc*mat_random_range(1000); /* random offset to start of a digit data */
	if(hdimgdata[offset]!=0)
		goto READ_MNIST;

	for(k=0; k<nr*nc; k++) {
		if(k%(nc)==0) printf("\n");
		printf("%d ",hdimgdata[offset+k]);
     		tcolor=egi_colorLuma_adjust(WEGI_COLOR_WHITE,  (hdimgdata[offset+k]/125.0-1.0)*egi_color_getY(WEGI_COLOR_WHITE));
         	hdimg->imgbuf[k] = tcolor;
		//hdimg->imgbuf[(k%28)*28+(k/28)] = tcolor;
	}
	printf("\n");
	/* Resize to 40x40 to fit in writeBox */
	egi_imgbuf_resize_update(&hdimg, false, 40, 40);
	/* Draw it into the writeBox */
	egi_subimg_writeFB(hdimg, &gv_fb_dev, 0, -1, wbox.startxy.x, wbox.startxy.y);
#endif /////////////////////////////////////////////////////////////////////////////////////


#if DIGITS_DATASET_TEST  ///////////////////* Read sklearn handwriten digit image into hdimg */////////////////////
	int nrand;
	int digitdata[8*8]; /* digit data 8*8  value [0-15] */
	EGI_IMGBUF *hdimg=NULL;
	EGI_IMGBUF *hdsimg=NULL;

	egi_imgbuf_free2(&hdimg);
	hdimg=egi_imgbuf_createWithoutAlpha(8, 8, WEGI_COLOR_WHITE); /* nr=nc=28 */

 READ_MNIST:
	nrand = mat_random_range(DIGITS_DATASIZE); /* random offset to start of a digit data */
	memcpy(digitdata, &digits_data[nrand][0], 8*8*sizeof(int));

	for(k=0; k<8*8; k++) {
		if(k%(8)==0) printf("\n");
		printf("%d ",digitdata[k]);
     		tcolor=egi_colorLuma_adjust(WEGI_COLOR_WHITE,  (1.0*digitdata[k]/16.0-1.0)*egi_color_getY(WEGI_COLOR_WHITE));
         	hdimg->imgbuf[k] = tcolor;
	}
	printf("\n");
	/* Resize to 40x40 to fit in writeBox */
	egi_imgbuf_free2(&hdsimg);
	hdsimg=egi_imgbuf_stretch(hdimg, 40, 40);
	//hdsimg=egi_imgbuf_resize(hdimg, false, 40, 40);
	/* Draw it into the writeBox */
	egi_subimg_writeFB(hdsimg, &gv_fb_dev, 0, -1, wbox.startxy.x+1, wbox.startxy.y+1);
#endif /////////////////////////////////////////////////////////////////////////////////////


	/* Draw doutsimg0,doutsimg1,doutsimg2 */
	egi_subimg_writeFB(doutsimg0, &gv_fb_dev, 0, -1, 10+5+10+35, 35);  /* Stretched  image */
	egi_subimg_writeFB(doutsimg1, &gv_fb_dev, 0, -1, 10+5+10+70, 35);  /* Stretched  image */
	egi_subimg_writeFB(doutsimg2, &gv_fb_dev, 0, -1, 10+5+10+105, 35);  /* Stretched  image */

	/* Write prediction 0-9 along doutsimg2 */
	for(k=0; k<10; k++) {
		char strp[16];
        	sprintf(strp, "%d", k);
        	strp[sizeof(strp)-1]=0;
        	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
       	                                15, 15,(const unsigned char *)strp,    /* fw,fh, pstr */
               	                        240, 1, 4,                                  /* pixpl, lines, gap */
                       	                10+5+10+140, 25+k*22,                                    /* x0,y0, */
                               	        WEGI_COLOR_WHITE, -1, 255,       /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	}

	/* Draw buttons */
	draw_filled_rect2(&gv_fb_dev,  WEGI_COLOR_GRAY, btn_confirm.startxy.x, btn_confirm.startxy.y, btn_confirm.endxy.x, btn_confirm.endxy.y);
	draw_button_frame( &gv_fb_dev, 0, WEGI_COLOR_GRAY, btn_confirm.startxy.x, btn_confirm.startxy.y,
				   btn_confirm.endxy.x-btn_confirm.startxy.x+1, btn_confirm.endxy.y-btn_confirm.startxy.y+1, 4);
	write_txt("数字识别", btn_confirm.startxy.x+10, btn_confirm.startxy.y+10, WEGI_COLOR_BLACK);

	/* Draw last digit */
	if(digit<0)
		write_txt("无法识别", btn_confirm.startxy.x, btn_confirm.startxy.y+60, WEGI_COLOR_YELLOW);
	else
		write_digit(digit, btn_confirm.endxy.x-70, btn_confirm.startxy.y+60,WEGI_COLOR_YELLOW);


	/* Wait touch down */
	while(1) {

#if !DIGITS_DATASET_TEST
		egi_touch_timeWait_press(-1, 0, &touch);
               	egi_touch_fbpos_data(&gv_fb_dev, &touch, 3);
#endif
		/* Press: Get start point */
        	startxy=touch.coord;
        	endxy=startxy;

		/* If in btn_confirm */

#if DIGITS_DATASET_TEST
		sleep(2);
		if(1) {
#else
		if(point_inbox(&startxy, &btn_confirm)) {
#endif
			/* Press down */
			draw_filled_rect2(&gv_fb_dev,  WEGI_COLOR_GRAY, btn_confirm.startxy.x, btn_confirm.startxy.y,
						       btn_confirm.endxy.x, btn_confirm.endxy.y);
			draw_button_frame( &gv_fb_dev, 1, WEGI_COLOR_GRAY, btn_confirm.startxy.x, btn_confirm.startxy.y,
				   btn_confirm.endxy.x-btn_confirm.startxy.x+1, btn_confirm.endxy.y-btn_confirm.startxy.y+1, 4);
			write_txt("识别中...", btn_confirm.startxy.x+10, btn_confirm.startxy.y+10, WEGI_COLOR_BLACK);

			/* Grep image in wbox */
			egi_imgbuf_free2(&digitimg);
			digitimg=egi_imgbuf_blockCopy(fbimg, wbox.startxy.x+1, wbox.startxy.y+1, wboxH-2, wboxW-2);
			//egi_subimg_writeFB(digitimg, &gv_fb_dev, 0, -1, 160, wbox.startxy.y-2);

			/* Convert to recimg for nnc */
			egi_imgbuf_free2(&recimg);
			recimg=egi_imgbuf_scale(digitimg, 8, 8);

			/* digitimg: Flip white/black */
			for(k=0; k< digitimg->width*digitimg->height; k++) {
				tcolor=egi_colorLuma_adjust(WEGI_COLOR_WHITE,  (-0.3)*egi_color_getY(digitimg->imgbuf[k]));
				digitimg->imgbuf[k] = tcolor;
			}

			//egi_subimg_writeFB(recimg, &gv_fb_dev, 0, -1, 35+40+20, wbox.startxy.y-2);

			/* Stretch recimg to 64*64 */
			egi_imgbuf_free2(&recsimg);
			recsimg=egi_imgbuf_stretch(recimg, 8*5, 8*5);

			/* Brighten recsimg */
			for(k=0; k < recsimg->width*recsimg->height; k++) {
				tcolor=egi_colorLuma_adjust(recsimg->imgbuf[k],  0.2*egi_color_getY(recsimg->imgbuf[k]));
				recsimg->imgbuf[k]=tcolor;
			}
			//egi_subimg_writeFB(recsimg, &gv_fb_dev, 0, -1, 35+40+20+10, wbox.startxy.y-2);


#if DIGITS_DATASET_TEST
			/* Grep gray values from digit dataset */
        		for(k=0; k<8*8; k++) {
                		imgdata[k] = digitdata[k];
		        }

#else
			/* Compute gray values */
			double max=0.0f;
			for(k=0; k<8*8; k++) {
				/* convert to 4bits grayscale: [0-15] */
				imgdata[k] = (double)(egi_color_getY(recimg->imgbuf[k])>>4);
				if(imgdata[k]>max) max=imgdata[k];
			}
			double delta=16.0-max;
			for(k=0; k<8*8; k++) {
				if(imgdata[k]>2.0f) {
					imgdata[k]+=delta;

					/* TEST: ----- */
					imgdata[k]+=5;
					if(imgdata[k]>16.0)imgdata[k]=16.0;
				}
			}
#endif

			/* NNC predict */
			digit=nnc_predictDigit(imgdata, dout);

			/* doutimg0 1x20 */
			for(k=0; k<20; k++) {
				//doutimg0->imgbuf[k] = WEGI_COLOR_ORANGE;
				tcolor=egi_colorLuma_adjust(WEGI_COLOR_YELLOW,  (dout[k]-1.0)*egi_color_getY(WEGI_COLOR_ORANGE));
				doutimg0->imgbuf[k] = tcolor;
			}
			/* stretch to 1*20x20*20*/
			egi_imgbuf_free2(&doutsimg0);
			doutsimg0=egi_imgbuf_resize(doutimg0, false, 1*20, 20*10); /* width, height */
			//doutsimg0=egi_imgbuf_stretch(doutimg0, 1*20, 20*20); /* width, height */

			/* doutimg1 1x20 */
			for(k=0; k<20; k++) {
				//doutimg0->imgbuf[k] = WEGI_COLOR_ORANGE;
				tcolor=egi_colorLuma_adjust(WEGI_COLOR_GREEN,  (dout[k+20]-1.0)*egi_color_getY(WEGI_COLOR_ORANGE));
				doutimg1->imgbuf[k] = tcolor;
			}
			/* stretch to 1*20x20*20*/
			egi_imgbuf_free2(&doutsimg1);
			doutsimg1=egi_imgbuf_resize(doutimg1, false, 1*20, 20*10); /* width, height */
			//doutsimg1=egi_imgbuf_stretch(doutimg1, 1*20, 20*20); /* width, height */

			/* doutimg2 1x10 */
			for(k=0; k<10; k++) {
				//doutimg0->imgbuf[k] = WEGI_COLOR_ORANGE;
				tcolor=egi_colorLuma_adjust(WEGI_COLOR_ORANGE,  (dout[k+20+20]-1.0)*egi_color_getY(WEGI_COLOR_ORANGE));
				doutimg2->imgbuf[k] = tcolor;
			}
			/* stretch to 1*20x10*20*/
			egi_imgbuf_free2(&doutsimg2);
			doutsimg2=egi_imgbuf_resize(doutimg2, false, 1*20, 10*20); /* width, height */
			//doutsimg0=egi_imgbuf_stretch(doutimg1, 1*20, 10*20); /* width, height */


//		        printf(" ---- Predict: %d ------\n", nnc_predictDigit(imgdata) );
			if(digit<0)
				write_txt("无法识别", btn_confirm.endxy.x+30, btn_confirm.startxy.y, WEGI_COLOR_YELLOW);
			else
				write_digit(digit, btn_confirm.endxy.x+30, btn_confirm.startxy.y,WEGI_COLOR_YELLOW);

			goto START_LOOP;
			//exit(0);
		}

		while (1) {
                	while(!egi_touch_getdata(&touch));
                	egi_touch_fbpos_data(&gv_fb_dev, &touch, 3);

	                /* Releasing: confirm the end point */
        	        if(touch.status==releasing || touch.status==released_hold) {
                	        break;
                	}

	                /* Get end point */
        	        startxy=endxy;
                	endxy=touch.coord;

			printf("Startxy: %d,%d		Endxy: %d,%d\n", startxy.x, startxy.y, endxy.x, endxy.y );

        	        /* DriectFB Draw Line */
                	fbset_color(WEGI_COLOR_WHITE); //YELLOW);
	                draw_wline(&gv_fb_dev, startxy.x, startxy.y, endxy.x, endxy.y, 5); //Pen size: 4,5
		}
	}


 /* ----- MY release ---- */
// egi_fmap_free(&fmap);


 /* <<<<<  EGI general release 	 >>>>>> */
 printf("FTsymbol_release_allfonts()...\n");
 FTsymbol_release_allfonts(); /* release sysfonts and appfonts */
 printf("symbol_release_allpages()...\n");
 symbol_release_allpages(); /* release all symbol pages*/
 printf("FTsymbol_release_allpage()...\n");
 FTsymbol_release_allpages(); /* release FT derived symbol pages: sympg_ascii */

 printf("fb_filo_flush() and release_fbdev()...\n");
 fb_filo_flush(&gv_fb_dev);
 release_fbdev(&gv_fb_dev);
 #if 0
 printf("release virtual fbdev...\n");
 release_virt_fbdev(&vfb);
 #endif
 printf("egi_end_touchread()...\n");
 egi_end_touchread();
 #if 0
 printf("egi_quit_log()...\n");
 egi_quit_log();
 #endif
 printf("<-------  END  ------>\n");

 return 0;
}


/*-------------------------------
	Write TXT
@txt: 	Input text
@px,py:	LCD X/Y for start point.
-------------------------------*/
static void write_txt(char *txt, int px, int py, EGI_16BIT_COLOR color)
{
       FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        24, 24,(const unsigned char *)txt,    /* fw,fh, pstr */
                                        240, 2, 4,                                  /* pixpl, lines, gap */
                                        px, py,                                    /* x0,y0, */
                                        color, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
}

/* Write digit */
static void write_digit(int k, int px, int py, EGI_16BIT_COLOR color)
{
	char strp[16];
	sprintf(strp, "%d", k);
	strp[sizeof(strp)-1]=0;

       FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                        32, 32,(const unsigned char *)strp,    /* fw,fh, pstr */
                                        240, 2, 4,                                  /* pixpl, lines, gap */
                                        px, py,                                    /* x0,y0, */
                                        color, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
}
