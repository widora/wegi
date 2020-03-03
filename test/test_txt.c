/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to analyze MIC captured audio and display its spectrum.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_pcm.h"
#include "egi_FTsymbol.h"

int main(void)
{
	int i,j,k;
	int ret;
	EGI_POINT pt;


        /* <<<<<  EGI general init  >>>>>> */
#if 1
        printf("tm_start_egitick()...\n");
        tm_start_egitick();		   	/* start sys tick */
        printf("egi_init_log()...\n");
        if(egi_init_log("/mmc/log_fb") != 0) {	/* start logger */
                printf("Fail to init logger,quit.\n");
                return -1;
        }
#endif
        printf("symbol_load_allpages()...\n");
        if(symbol_load_allpages() !=0 ) {   	/* load sys fonts */
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
        if(FTsymbol_load_appfonts() !=0 ) {  	/* load FT fonts LIBS */
                printf("Fail to load FT appfonts, quit.\n");
                return -2;
        }
        printf("init_fbdev()...\n");
        if( init_fbdev(&gv_fb_dev) )		/* init sys FB */
                return -1;


	/* ------ prepare TXT type ebox ------- */
EGI_DATA_TXT *memo_txt=NULL;
EGI_EBOX     *ebox_txt=NULL;
unsigned char utext[200]="千山鸟飞绝,\n万径人踪灭。\n孤舟蓑笠翁,\n独钓寒江雪。";
int eboxW=120;
int offx=5;
int offy=5;

show_jpg("/tmp/fish.jpg",&gv_fb_dev, false, 0, 0);
do {    ///////////////////////     LOOP TEST 	////////////////////////

	/* For FTsymbols */
	memo_txt=egi_utxtdata_new( offx, offy,   		    /* offset from ebox left top */
                       		         4, 125,             	    /* lines, pixels per line */
		                         egi_appfonts.regular,      /* font face type */
                                	 20, 20,         	    /* font width and height, in pixels */
                                	 15,			    /* adjust gap */
					 WEGI_COLOR_BLACK           /* font color  */
                        	       );
	/* assign uft8 string */
	memo_txt->utxt=utext;

	ebox_txt= 	egi_txtbox_new(	"memo stick",   /* tag */
			              	memo_txt,  	/* EGI_DATA_TXT pointer */
                		 	true, 		/* bool movable */
			                0,0, 		/* int x0, int y0 */
			                eboxW-50, 60, 	/* width, height(adjusted as per nl and fw) */
			                -1, 		/* int frame, -1=no frame */
			                WEGI_COLOR_LTGREEN   /* prmcolor*/
        			     );
	ebox_txt->frame_alpha=frame_none; //100;

	/* move ebox txt */
	egi_randp_inbox(&pt, &gv_fb_box);
	ebox_txt->x0=pt.x;
	ebox_txt->y0=pt.y;

	/* activate and display */
	ebox_txt->activate(ebox_txt);

	tm_delayms(75);

	/* refresh ebox txt */
	ebox_txt->need_refresh=true;
	ebox_txt->refresh(ebox_txt);

	/* erase ebox txt image and free */
	ebox_txt->sleep(ebox_txt);
	ebox_txt->free(ebox_txt);

} while(0);  ////////////////////   END LOOP TEST   ///////////////////



/////////////////////////    ROAM TEST    //////////////////////////
int stepnum;
int step=5;
EGI_POINT pa,pb; /* 2 points define a picture image box */
EGI_POINT pn; /* origin point of displaying window */
/* define a box, within which the ebox_txt is limited */
EGI_BOX	  box;
/* define left_top and right_bottom point of the picture */
pa.x=0;
pa.y=0;
pb.x=239;
pb.y=319;


/* For FTsymbols */
memo_txt=egi_utxtdata_new( 13, offy,   		    /* offset from ebox left top */
              		   4, 125,             /* lines, pixels per line */
                           egi_appfonts.bold,      /* font face type */
                           20, 20,         	    /* font width and height, in pixels */
                           10,			    /* adjust gap */
			   WEGI_COLOR_ORANGE //BLACK           /* font color  */
                       	  );

/* assign uft8 string */
memo_txt->utxt=utext;

ebox_txt= 	egi_txtbox_new(	"memo stick",   /* tag */
		              	memo_txt,  	/* EGI_DATA_TXT pointer */
               		 	true, 		/* bool movable */
		                0,0, 		/* int x0, int y0 */
		                eboxW-50, 60, 	/* width, height(adjusted as per nl and fw) */
		                frame_round_rect,   /* int frame, -1=no frame >100 use frame_img */
		                -1 //WEGI_COLOR_BLUE   /* prmcolor, -1 transparent*/
       			     );
ebox_txt->frame_alpha=100;

/* activate and display */
ebox_txt->activate(ebox_txt);

/* assign limit box */
box.startxy=pa;
box.endxy=(EGI_POINT){pb.x-ebox_txt->width, pb.y-ebox_txt->height};

/* set  start point */
egi_randp_inbox(&pb, &box);

//for(k=0;k<ntrip;k++)
while(1)
{
	/* pick a random point on box sides for pn, as end point of this trip */
	egi_randp_boxsides(&pn, &box);
	printf("random point: pn.x=%d, pn.y=%d\n",pn.x,pn.y);

	/* shift pa pb */
        pa=pb; /* now pb is starting point */
        pb=pn;

        /* count steps from pa to pb */
        stepnum=egi_numstep_btw2p(step,&pa,&pb);

        /* walk through those steps, displaying each step */
        for(i=0;i<stepnum;i++)
        {
        	/* get interpolate point */
	        egi_getpoit_interpol2p(&pn, step*i, &pa, &pb);

                /* move ebox_txt */
		ebox_txt->x0=pn.x;
		ebox_txt->y0=pn.y;

		/* refresh ebox txt */
		ebox_txt->need_refresh=true;
		ebox_txt->refresh(ebox_txt);

                tm_delayms(200);
        }
}

/////////////////////////   END ROAM TEST    //////////////////////

	/* erase ebox txt image and free */
	ebox_txt->sleep(ebox_txt);
	ebox_txt->free(ebox_txt);

        /* <<<<<  EGI general release >>>>> */
        printf("FTsymbol_release_allfonts()...\n");
        FTsymbol_release_allfonts();
        printf("symbol_release_allpages()...\n");
        symbol_release_allpages();
	printf("release_fbdev()...\n");
        fb_filo_flush(&gv_fb_dev);
        release_fbdev(&gv_fb_dev);
        printf("egi_quit_log()...\n");
        egi_quit_log();
        printf("<-------  END  ------>\n");

return 0;
}
