/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple JPG/PNG file display test.

Usage:
	./test_show file

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"

void print_help(const char* cmd)
{
        printf("Usage: %s [-hml:] file [title]\n", cmd);
        printf("        -h   Help \n");
        printf("        -m   Roam the picture\n");
	printf("	-l   Luma delt\n");
}

int main(int argc, char** argv)
{
	int opt;
	const char *fpath=NULL;
	const char *title=NULL;
	bool roam_on=false;

        /* <<<<<  EGI general init  >>>>>> */
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
#if 0
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_test") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }

        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
#endif

	/* Load freetype fonts */
  	printf("FTsymbol_load_sysfonts()...\n");
  	if(FTsymbol_load_sysfonts() !=0) {
        	printf("Fail to load FT sysfonts, quit.\n");
        	return -1;
  	}
  	//FTsymbol_set_SpaceWidth(1);
  	//FTsymbol_set_TabWidth(4.5);


  	/* Initilize sys FBDEV */
  	printf("init_fbdev()...\n");
  	if(init_fbdev(&gv_fb_dev))
        	return -1;

  	/* Start touch read thread */
#if 0
  	printf("Start touchread thread...\n");
  	if(egi_start_touchread() !=0)
        	return -1;
#endif
  	/* Set sys FB mode */
	fb_set_directFB(&gv_fb_dev,false);
	fb_position_rotate(&gv_fb_dev,0);

	/* Adjust Luma */
	gv_fb_dev.lumadelt=50;

 /* <<<<<  End of EGI general init  >>>>>> */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hml:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
                        case 'm':
                                roam_on=true;
                                break;
			case 'l':
				gv_fb_dev.lumadelt=atoi(optarg);
				break;
		}
	}
        if( optind < argc ) {
		fpath=argv[optind];
		if(optind+1<argc)
			title=argv[optind+1];
        } else {
                printf("No input file!\n");
                exit(1);
        }

	/* Read in imgbuf */
	EGI_IMGBUF* origimg=egi_imgbuf_readfile(fpath);
	if(origimg==NULL)exit(1);

	EGI_IMGBUF* myimg=egi_imgbuf_resize(origimg, 320, 240);
	if(myimg==NULL)exit(1);

	egi_imgbuf_windisplay2( myimg, &gv_fb_dev,
        	                0,0,0,0,320,240 );     /* xp,yp,xw,yw,w,h */

       	FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                      16, 16,(const unsigned char *)"Bing 必应",   /* fw,fh, pstr */
                                      300, 1, 1,                   		/* pixpl, lines, fgap */
                                      320-80, 5,	                       	/* x0,y0, */
                                      WEGI_COLOR_WHITE, -1, 175,               /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */

	if(title)
        	FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                       15, 15,(const unsigned char *)title,   	/* fw,fh, pstr */
                                       300, 3, 3,                   		/* pixpl, lines, fgap */
                                       5, 240-40,                       	/* x0,y0, */
                                       WEGI_COLOR_WHITE, -1, 175,               /* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */

	fb_render(&gv_fb_dev);
	sleep(2);

	/* Roam the original picture */
	if(roam_on) {
		egi_roampic_inwin( fpath, &gv_fb_dev, 3, 10000,  /* step, ntrips */
                	           0,0, gv_fb_dev.pos_xres, gv_fb_dev.pos_yres ); /* xw, yw, winw, winh */
	}

	egi_imgbuf_free2(&origimg);
	egi_imgbuf_free2(&myimg);

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        //printf("egi_quit_log()...\n");
        //egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}


