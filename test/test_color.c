/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Test EGI COLOR functions

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "egi_timer.h"
#include "egi_fbgeom.h"
#include "egi_color.h"
#include "egi_log.h"
#include "egi.h"

int main(void)
{
	int i;
	int value[3]={0}; /* control param for brightness adjustment */
	int step[3]={18,12,18};  /* increase/decrease step for value */
	int delt[3]={18,12,18};

	EGI_16BIT_COLOR color[3],subcolor[3];

	/* --- init logger --- */
  	if(egi_init_log("/mmc/log_color") != 0)
	{
		printf("Fail to init logger,quit.\n");
		return -1;
	}
        /* --- start egi tick --- */
//        tm_start_egitick();
        /* --- prepare fb device --- */
        gv_fb_dev.fdfd=-1;
        init_dev(&gv_fb_dev);

	/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<  test draw_wline <<<<<<<<<<<<<<<<<<<<<<*/


 //void symbol_strings_writeFB( FBDEV *fb_dev, const struct symbol_page *sym_page, unsigned int pixpl,
 //                unsigned int gap, int fontcolor, int transpcolor, int x0, int y0, const char* str )


	/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<  test draw_wline <<<<<<<<<<<<<<<<<<<<<<*/
	EGI_POINT p1,p2;
	EGI_BOX box={{0,0},{240-1,320-1,}};
   while(1)
  {
	egi_randp_inbox(&p1, &box);
	egi_randp_inbox(&p2, &box);
	fbset_color(egi_color_random(medium));
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
	/* >>>>>>>>>>>>>>>>>>>>>>>>>>> end testing draw_wlines >>>>>>>>>>>>>>>>>*/


	/* get a random color */
	for(i=0;i<3;i++) {
		color[i]= egi_color_random(deep);
	}
	color[0]=WEGI_COLOR_RED;
	color[1]=WEGI_COLOR_GREEN;
	color[2]=WEGI_COLOR_BLUE;

while(1)
{
        for(i=0;i<3;i++)
	{
		subcolor[i]=egi_colorbrt_adjust(color[i],value[i]);
		//printf("---k=%d,  subcolor: 0x%02X  ||||  color: 0x%02X ---\n",k,subcolor,color);
		fbset_color(subcolor[i]);
		draw_filled_rect(&gv_fb_dev, 15+(60+15)*i, 150, (15+60)*(i+1), 150+60);

		value[i] += step[i]; /* Y Max.255, k to be little enough */
		if(subcolor[i]==0xFFFF || egi_color_getY(subcolor[i])>=210 )
			step[i]= -delt[i];
		if(subcolor[i]==0x0000 || egi_color_getY(subcolor[i])<=10 ) //255
			step[i]= delt[i];

		printf("------- subcolor[%d]:  Y=%d step[%d]=%d-------\n",
						i,egi_color_getY(subcolor[i]),i,step[i]);
	}
	//tm_delayms(50);
	usleep(55000);
}

  	/* quit logger */
  	egi_quit_log();

        /* close fb dev */
        munmap(gv_fb_dev.map_fb,gv_fb_dev.screensize);
        close(gv_fb_dev.fdfd);


	return 0;
}

