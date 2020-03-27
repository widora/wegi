/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Example of using imported icons as egi_sbtn image.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <egi_common.h>
#include <egi_utils.h>
#include <egi_FTsymbol.h>
#include <egi_pcm.h>


static EGI_IMGBUF	*iconsImg;
#define ICONID_BULB_ON 		0
#define ICONID_BULB_OFF 	1
#define ICONID_FLIP_ON		2
#define ICONID_FLIP_OFF 	3


/* Switch Button Reaction Function */
static void switch_react(EGI_RECTBTN *btn);


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

  /* Load symbol pages */
  printf("FTsymbol_load_allpages()...\n");
  if(FTsymbol_load_allpages() !=0) /* FT derived sympg_ascii */
        printf("Fail to load FTsym pages! go on anyway...\n");
  printf("symbol_load_allpages()...\n");
  if(symbol_load_allpages() !=0) {
        printf("Fail to load sym pages, quit.\n");
        return -1;
  }

  /* Load freetype fonts */
  printf("FTsymbol_load_sysfonts()...\n");
  if(FTsymbol_load_sysfonts() !=0) {
        printf("Fail to load FT sysfonts, quit.\n");
        return -1;
  }
  #if 0
  printf("FTsymbol_load_appfonts()...\n");
  if(FTsymbol_load_appfonts() !=0) {
        printf("Fail to load FT appfonts, quit.\n");
        return -1;
  }
  #endif

  /* Initilize sys FBDEV */
  printf("init_fbdev()...\n");
  if(init_fbdev(&gv_fb_dev))
        return -1;

  /* Start touch read thread */
  printf("Start touchread thread...\n");
  if(egi_start_touchread() !=0)
        return -1;

  /* Set sys FB mode */
  fb_set_directFB(&gv_fb_dev,false);
  fb_position_rotate(&gv_fb_dev,3);

 /* <<<<<  End of EGI general init  >>>>>> */


    		  /*-----------------------------------
		   *            Main Program
    		   -----------------------------------*/

	int i;
	EGI_TOUCH_DATA	touch_data;

	/* Load image for icons */
	iconsImg=egi_imgbuf_readfile("/mmc/bulb_controls.png"); /* All icons in one file */
	if(iconsImg==NULL)exit(1);

	/* Define Icon Box in the icons_image */
	if(egi_imgbuf_setSubImgs(iconsImg, 4)!=0) exit(1);
	iconsImg->subimgs[ICONID_BULB_ON]  =(EGI_IMGBOX){141,20,130,180}; 	/* BULB_ON */
	iconsImg->subimgs[ICONID_BULB_OFF] =(EGI_IMGBOX){9,20,130,180};		/* BULB_OFF */
	iconsImg->subimgs[ICONID_FLIP_ON]  =(EGI_IMGBOX){502,7,120,90};		/* FLIP_ON */
	iconsImg->subimgs[ICONID_FLIP_OFF] =(EGI_IMGBOX){502,105,120,90}; 	/* FLIP_OFF */

	/* Define Buttons */
	#define MAX_BTNS		1
	#define BTNID_SWITCH		0
	EGI_RECTBTN 			btns[MAX_BTNS]={0};

	btns[BTNID_SWITCH]=(EGI_RECTBTN){  .id=BTNID_SWITCH,
					   .pressimg=iconsImg, .pressimg_subnum=ICONID_FLIP_ON,
					   .releaseimg=iconsImg, .releaseimg_subnum=ICONID_FLIP_OFF,
					   .x0=160, .y0=70,
					   .width=iconsImg->subimgs[ICONID_FLIP_OFF].w,
					   .height=iconsImg->subimgs[ICONID_FLIP_OFF].h,
	    				   .pressed=false, .reaction=switch_react
					 };

	/* Clear FB backbuff */
	fb_clear_backBuff(&gv_fb_dev,WEGI_COLOR_BLACK);

	/* Draw Bulb and switch */
	egi_subimg_writeFB(iconsImg, &gv_fb_dev, ICONID_BULB_OFF, -1, 0, 40); /* imgbuf, fbdev, subnum, subcolor, x0,y0  */
	egi_sbtn_refresh(&btns[BTNID_SWITCH],NULL);

	/* Title */
	FTsymbol_uft8strings_writeFB(   &gv_fb_dev, egi_sysfonts.bold,          /* FBdev, fontface */
                                      	20, 20,(const unsigned char *)"EGI: 用图标制作按钮的例子",    /* fw,fh, pstr */
                                       	320, 1, 0,                                  /* pixpl, lines, gap */
                                       	10, 5,                                    /* x0,y0, */
                                       	WEGI_COLOR_LTGREEN, -1, 255,       /* fontcolor, transcolor,opaque */
                                       	NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
	/* Render */
	fb_render(&gv_fb_dev);


while(1) {  /*  --- LOOP ---- */

        if(egi_touch_timeWait_press(-1, 0, &touch_data)!=0)
                continue;

        /* Touch_data converted to the same coord as of FB */
        egi_touch_fbpos_data(&gv_fb_dev, &touch_data);

        /* Check if touched any of the buttons */
        for(i=0; i<MAX_BTNS; i++) {
                if( egi_touch_on_rectBTN(&touch_data, &btns[i]) ) { //&& touch_data.status==pressing ) {  /* Re_check event! */
                        printf("Button_%d touched!\n",i);
			btns[i].touch_data=&touch_data;
                        btns[i].reaction(&btns[i]);
                        fb_page_refresh(&gv_fb_dev, 0);
                        break;
                }
        }

        /* Touch missed  */
        if( i==MAX_BTNS ) {
                continue;
        }

}

 /* ----- MY release ---- */
 egi_imgbuf_free2(&iconsImg);

    		  /*-----------------------------------
		   *         End of Main Program
		   -----------------------------------*/


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


/*--------------------------------------
     Switch/Toggle reaction function
---------------------------------------*/
static void switch_react(EGI_RECTBTN *btn)
{
	if(btn->touch_data==NULL)return;

        /* Toggle/switch button */
	if(btn->pressed==false) {
		/* Check if pressed left side of the button */
		if( btn->touch_data->coord.x < btn->x0+btn->width/2 )
			btn->pressed=true;
	}
	else {  /*  btn->pressed==true,  Check if pressed right side of the button */
		if( btn->touch_data->coord.x > btn->x0+btn->width/2 )
			btn->pressed=false;
	}

        /* Draw button */
	egi_sbtn_refresh(btn,NULL);

	/* Update Bulb image  */
	/* imgbuf, fbdev, subnum, subcolor, x0,y0  */
	egi_subimg_writeFB(iconsImg, &gv_fb_dev, btn->pressed?ICONID_BULB_ON:ICONID_BULB_OFF, -1, 0, 40);

}



