/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple JPG/PNG file display test.

Usage:
	./test_show file
	./show1pic file   <Renamed>

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
        printf("Usage: %s [-hmt:s:u:l:] file [title]\n", cmd);
        printf("        -h   Help \n");
        printf("        -m   Roam the picture\n");
	printf("	-t   Duration in seconds, default: 0(forever)\n");
	printf("	-s   Step for roam, default: 3 pixs.\n");
	printf("	-u   useconds for each roam frame. default:75000. \n");
	printf("	-l   Luma delt, default:0. \n");
}

void write_descript(const char *title);
void write_pc_descript(const char *title);

int main(int argc, char** argv)
{
	int opt;
	const char *fpath=NULL;
	const char *title=NULL;
	unsigned int us=75000;
	bool roam_on=false;
	int roam_step=3; 	/* For pic roam */
	int durts=0;		/* Total show time in second. <=0 forever.  */
	EGI_CLOCK eclock={0};

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

 /* <<<<<  End of EGI general init  >>>>>> */

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hmt:s:u:l:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
                        case 'm':
                                roam_on=true;
                                break;
			case 't':
				durts=atoi(optarg);
				break;
			case 's':
				roam_step=atoi(optarg);
				break;
			case 'u':
				if(atoi(optarg)>0)
					us=atoi(optarg);
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

	/* Start clock */
	egi_clock_start(&eclock);

	/* Read in imgbuf */
	EGI_IMGBUF* origimg=egi_imgbuf_readfile(fpath);
	if(origimg==NULL)exit(1);

	EGI_IMGBUF* myimg=egi_imgbuf_resize(origimg, 320, 240);
	if(myimg==NULL)exit(1);

	egi_imgbuf_windisplay2( myimg, &gv_fb_dev,
        	                0,0,0,0,320,240 );     /* xp,yp,xw,yw,w,h */

	/* Put descriptions */
	write_descript(title);
	fb_render(&gv_fb_dev);
	sleep(3);

	/* Roam the original picture */
	if(roam_on) {
		//egi_roampic_inwin( fpath, &gv_fb_dev, 3, 10000,  /* step, ntrips */
                //	           0,0, gv_fb_dev.pos_xres, gv_fb_dev.pos_yres ); /* xw, yw, winw, winh */
		//

	        int i,k;
        	int stepnum;
		int ntrip=10000;
		int xw=0;
		int yw=0;
		int winw=gv_fb_dev.pos_xres;
		int winh=gv_fb_dev.pos_yres;

	        EGI_POINT pa,pb; /* 2 points define a picture image box */
        	EGI_POINT pn; /* origin point of displaying window */

	        /* define left_top and right_bottom point of the picture */
        	pa.x=0;
	        pa.y=0;
        	pb.x=origimg->width-1;
	        pb.y=origimg->height-1;

        	/* define a box, within which the displaying origin(xp,yp) related to the picture is limited */
	        EGI_BOX box={ pa, {pb.x-winw,pb.y-winh}};

        	/* set  start point */
	        egi_randp_inbox(&pb, &box);

        	for(k=0;k<ntrip;k++)
        	{
                	/* pick a random point on box sides for pn, as end point of this trip */
	                egi_randp_boxsides(&pn, &box);
        	        printf("random point: pn.x=%d, pn.y=%d\n",pn.x,pn.y);

                	/* shift pa pb */
	                pa=pb; /* now pb is starting point */
        	        pb=pn;

        	        /* count steps from pa to pb */
                	stepnum=egi_numstep_btw2p(roam_step,&pa,&pb);
	                /* walk through those steps, displaying each step */
        	        for(i=0;i<stepnum;i++)
                	{
                        	/* get interpolate point */
	                        egi_getpoit_interpol2p(&pn, roam_step*i, &pa, &pb);

        	                #if 1 /* Display in the window */
				fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_BLACK);
                	        egi_imgbuf_windisplay( origimg, &gv_fb_dev, -1, pn.x, pn.y, xw, yw, winw, winh ); /* use window */
				#endif

				#if 0 /* TEST: -----  */
				fbset_color(WEGI_COLOR_RED);
				draw_dot(&gv_fb_dev,pn.x,pn.y);
				//draw_circle(&gv_fb_dev, pn.x, pn.y, 5);
				#endif

				/* Put description */
				#ifdef LETS_NOTE
				write_pc_descript(title);
				#else
				write_descript(title);
				#endif

				fb_render(&gv_fb_dev);
                        	//usleep(125000);
				usleep(us);

				/* Check clock */
				if( durts>0 && egi_clock_peekCostUsec(&eclock)/1000000 >= durts )
					goto END_PROG;
                	}
        	}
	} /* End roam on */


END_PROG:
	/* Free */
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

/*---------------------------
	Put title
-----------------------------*/
void write_descript( const char *title )
{
        /* Put descriptions */
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                      16, 16,(const unsigned char *)"Bing 必应",   /* fw,fh, pstr */
                                      300, 1, 1,                                /* pixpl, lines, fgap */
                                      320-80, 5,                                /* x0,y0, */
                                      WEGI_COLOR_WHITE, -1, 175,               /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */

        if(title)
                FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                       15, 15,(const unsigned char *)title,     /* fw,fh, pstr */
                                       300, 3, 3,                               /* pixpl, lines, fgap */
                                       5, 240-40,                               /* x0,y0, */
                                       WEGI_COLOR_WHITE, -1, 175,               /* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */

}

void write_pc_descript( const char *title )
{
        /* Put descriptions */
        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                      24, 24,(const unsigned char *)"Bing 必应 :",   /* fw,fh, pstr */
                                      300, 1, 1,                                /* pixpl, lines, fgap */
                                      300, 10,                                /* x0,y0, */
                                      WEGI_COLOR_WHITE, -1, 200,               /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */

        if(title)
                FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                       24, 25,(const unsigned char *)title,     /* fw,fh, pstr */
                                       gv_fb_dev.pos_xres, 1, 0,                /* pixpl, lines, fgap */
                                       450, 10,                               /* x0,y0, */
                                       WEGI_COLOR_WHITE, -1, 200,               /* fontcolor, transcolor,opaque */
                                       NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */

        FTsymbol_uft8strings_writeFB(&gv_fb_dev, egi_sysfonts.regular,     /* FBdev, fontface */
                                      24, 24,(const unsigned char *)"EGI: A Linux Player.",   /* fw,fh, pstr */
                                      550, 1, 1,                                /* pixpl, lines, fgap */
                                      gv_fb_dev.pos_xres/2-200, 50,                                /* x0,y0, */
                                      WEGI_COLOR_PURPLE, -1, 200,               /* fontcolor, transcolor,opaque */
                                      NULL, NULL, NULL, NULL);                 /* int *cnt, int *lnleft, int* penx, int* peny */
}
