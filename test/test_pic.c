/*-----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI_PIC functions

Usage:	show_pic /photos/

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "egi_common.h"
#include "utils/egi_utils.h"

int main(int argc, char **argv)
{
	int i,j;
	int ret;

	if(argc<2) {
		printf("Usage: %s file\n",argv[0]);
		exit(-1);
	}


        /* EGI general init */
#if 0
        tm_start_egitick();
        if(egi_init_log("/mmc/log_color") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages,quit.\n");
                return -2;
        }
#endif
        init_fbdev(&gv_fb_dev);

	char *path= argv[1]; //"/mmc/photos";
	char **fpaths;
	int count=0;

	int pich=230;
	int picw=230;
	int poffx=4;
	int poffy=4;
	int symh=26;
	EGI_DATA_PIC *data_pic=NULL;
	EGI_EBOX *pic_box=NULL;
	EGI_IMGBUF *imgbuf;

	EGI_POINT  pxy;
	/* box for x0y0 range...*/
	EGI_BOX	 box={ {0-230/2,0-130/2}, {240-230/2-1,320-130/2-1} };
	int dex,dey; /* extended deviation */
	//EGI_BOX  box={ {0-100,0-150},{240-1-100,320-1-150} };
	//EGI_BOX  box={ {0,0},{240-picw-poffx-1,320-pich-poffy-symh-1}};

#if 1 //////////////////// Display one file only /////////////////////

	/* readin file */
	imgbuf=egi_imgbuf_readfile(path);
	if(imgbuf==NULL) {
		printf("Fail to read image file '%s'.\n", path);
		exit(-1);
	}

	/* display */
	printf("display imgbuf...\n");
        egi_imgbuf_windisplay( imgbuf, &gv_fb_dev, -1,         /* img, FB, subcolor */
                               0, 0,                            /* int xp, int yp */
                               0, 0, imgbuf->width, imgbuf->height   /* xw, yw, winw,  winh */
                              );

	/* Free it */
	egi_imgbuf_free(imgbuf);

	exit(0);
#endif ////////////////////  end //////////////////////


///////////// Loop to load to EBOX PIC and activate it  ///////////////
while(1) {

   /* buff all jpg and png file path */
   fpaths=egi_alloc_search_files(path, ".jpg .png .jpeg", &count);
   printf("Totally %d files are found.\n",count);
   for(i=0; i<count; i++)
   	printf("%s\n",fpaths[i]);

   /* display file one by one */
   for(i=0;i<count+1;i++)
   {
	printf("---------- loading '%s' -----------\n", fpaths[i]);

	imgbuf=egi_imgbuf_alloc();
	/* load jpg file to buf */
	if(egi_imgbuf_loadjpg(fpaths[i], imgbuf) !=0) {
		printf("Load JPG imgbuf fail, try egi_imgbuf_loadpng()...\n");
		if(egi_imgbuf_loadpng(fpaths[i], imgbuf) !=0) {
			printf("Load PNG imgbuf fail, try egi_imgbuf_free...\n");
			egi_imgbuf_free(imgbuf);
		   	continue;
		}
		else {
			printf("Finish loading PNG image file...\n");
		}
	}
	else {
		printf("Finish loading JPG image file...\n");
	}

	/* allocate data_pic */
	/* NOTE:
	   1. WARNING, MUTEX LOCK FOR IMGBUF IS IGNORED HERE ....
	   2. egi_picdata_new() initiate a data_pic with imgbuf AND alpha with 100% transparent !!!!
	*/
	printf("Start egi_picdata_new()....\n");
        data_pic= egi_picdata_new( poffx, poffy,    	/* int offx, int offy */
				   &imgbuf,		/* !!!! Ownership transfered, and reset to NULL */
                       		   0,0,	   		/* int imgpx, int imgpy */
				   -1,			/* image canvan color, <0 for transparent */
 	       	                   &sympg_testfont  	/* struct symbol_page *font */
	                        );

	/* set title */
	data_pic->title="EGI_PIC";

	egi_randp_inbox(&pxy, &box);
	printf("start egi_picbox_new()....\n");
	pic_box=egi_picbox_new( "pic_box", /* char *tag, or NULL to ignore */
       				  data_pic,  /* EGI_DATA_PIC *egi_data */
			          1,	     /* bool movable */
			   	  pxy.x,pxy.y,//10,100,    /*  x0, y0 for host ebox*/
        			  -1,	     /* int frame */
				  -1	//WEGI_COLOR_GRAY /* int prmcolor,applys only if prmcolor>=0  */
	);
	printf("egi_picbox_activate()...\n");
	egi_picbox_activate(pic_box);

	/* TODO: scale pic to data_pic */


	/* re_set random point picking box */
	dex=(240/3 > (pic_box->width)/3) ? (pic_box->width)/3 : (240/3);
	dey=(320/3 > (pic_box->height)/3) ? (pic_box->height)/3 : (320/3);
	box.startxy=(EGI_POINT){ 240-(pic_box->width)-dex, 320-(pic_box->height)+dey }; /* -x(dex) ,+y(dey) */
	box.endxy= (EGI_POINT){0+dex, 0-dey};  /* +x(dex), -y(dey) */

//	box.startxy=(EGI_POINT){ 240-(pic_box->width)-100, 320-(pic_box->height)+150 }; /* -x(100) ,+y(150) */
//	box.endxy= (EGI_POINT){0+100, 0-150};  /* +x(100), -y(150) */

 	/* display in several position */
	 for(j=0; j<8; j++)
 	{
		/* get a random point */
		egi_randp_inbox(&pxy, &box);
		printf("egi_randp_inbox: pxy(%d,%d)\n", pxy.x, pxy.y);
		pic_box->x0=pxy.x;
		pic_box->y0=pxy.y;

		/* refresh picbox to show the picture */
		egi_ebox_needrefresh(pic_box);
		egi_picbox_refresh(pic_box);
		tm_delayms(1000);
  	}/* end displaying one file */

	/* sleep to erase the image */
	egi_picbox_sleep(pic_box);
	/* release it */
	pic_box->free(pic_box);
   }/* end displaying all files */

	/* free fpaths */
	egi_free_buff2D((unsigned char **) fpaths, count);


} /* end of while() */

	release_fbdev(&gv_fb_dev);
	symbol_release_allpages();
	egi_quit_log();

	return 0;
}
