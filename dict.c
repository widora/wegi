
/*----------------- !!! OBSOLETE !!! --------------------------------
    ===---  This is for clock digital number only !!!!  ---===

Dict for symbols

A dict is a 240x320x2B symbol page, every symbol is
presented by 5-6-5RGB pixels. Symbols can be fonts,icons,pics.

--- Dicts:
h20w15:  20x15 fonts. totally 16*16 characters for a 240x320x2B img page.
s60icon: 60x60 icons.



Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* read,write,close */
#include <stdint.h>
#include <fcntl.h> /* open */
#include <linux/fb.h>
#include "egi_fbgeom.h"
#include "dict.h"

/* symbols dict data  */
uint16_t *dict_h20w15;

/*----------------------------------------------
 malloc dict_h20w15

 return:
	 NULL     fails
	 	  succeed
-----------------------------------------------*/
uint16_t *dict_init_h20w15(void)
{
	if(dict_h20w15 !=NULL) {
		printf("dict_h20w16 is not NULL, fail to malloc!\n");
		return dict_h20w15;
	}

	dict_h20w15=malloc(DICT_NUM_LIMIT_H20W15*20*15*sizeof(uint16_t));

	if(dict_h20w15 == NULL) {
		printf("fail to malloc for dict_h20w15!\n");
		return NULL;
	}

	return dict_h20w15;
}

/*-------------------------------------------------
free dict_h20w15
-------------------------------------------------*/
void dict_release_h20w15(void)
{
	if(dict_h20w15 !=NULL)
		free(dict_h20w15);
}

/*-------------------------------------------------
  display dict img 
--------------------------------------------------*/
void dict_display_img(FBDEV *fb_dev,char *path)
{
	int i;
	int fd;
	FBDEV *dev = fb_dev;
	int xres=dev->vinfo.xres;
	int yres=dev->vinfo.yres;
	uint16_t buf;

	/* open dict img file */
	fd=open(path,O_RDONLY|O_CLOEXEC);
	if(fd<0) {
		perror("open dict file");
		//printf("Fail to open dict file %s!\n",path);
		dict_release_h20w15();
		exit(-1);
	}

	for(i=0;i<xres*yres;i++)
	{
		/* read from img file one byte each time */
		read(fd,&buf,2);/* 2 bytes each pixel */
		/* write to fb one byte each time */
               	*(uint16_t *)(dev->map_fb+2*i)=buf; /* write to fb */
	}

	close(fd);
}



/*---------------------------------------------------------
malloc dict and load 20*15 symbols from img data to dict_h20w15
-----!!!!!!!!!!!!!!!!!!

Return:
	NULL		 fails
	pointer to dict  OK

----------------------------------------------------------*/
uint16_t *dict_load_h20w15(char *path)
{
	int fd;
	int i,j;
	uint16_t *dict;

	int x0,y0; /* start position of a symbol,in pixel */

	/* malloc dict data */
	if(dict_init_h20w15()==NULL)
		return NULL;

	/* -------- get pointer to dict NOW ! */
	dict=dict_h20w15;

	/* open dict img file */
	fd=open(path,O_RDONLY|O_CLOEXEC);
	if(fd<0) {
		perror("open dict file");
		//printf("Fail to open dict file %s!\n",path);
		//dict_release_h20w15();//release in main();
		return NULL;
	}

	/* read each symbol to dict */
	for(i=0;i<DICT_NUM_LIMIT_H20W15;i++) /* i each symbol */
	{
		x0=(i%(DICT_IMG_WIDTH/15))*15; /* in pixel, (DICT_IMG_WIDTH/15) symbols in each row */
		y0=(i/(DICT_IMG_WIDTH/15))*20; /* in pixel */
		//printf("x0=%d, y0=%d \n",x0,y0);
		for(j=0;j<20;j++) /* 20 row ,20-pixels */
		{
			/* seek position in byte. 2bytes per pixel */
			if( lseek(fd,(y0+j)*DICT_IMG_WIDTH*2+x0*2,SEEK_SET)<0 ) /* in bytes */
			{
				perror("lseek dict file");
				return NULL;
			}
			/* read row data,15-pixels x 2bytes */
			if( read(fd, (uint8_t *)(dict+i*20*15+j*15), 15*2) < 0 )
			{
				perror("read dict file");
				return NULL;
			}
		}
	}
	printf("finish reading %s.\n",path);

	close(fd);

	return dict;
}


/*--------------------------------------------------
	print all 20x15 symbols in a dict
	print  '*' if the pixel is not 0.
-------------------------------------------------*/
void dict_print_symb20x15(uint16_t *dict)
{
	int i,j,k;

	for(i=0;i<DICT_NUM_LIMIT_H20W15;i++) /* i each symbol */
	{
		for(j=0;j<20;j++) /* row */
		{
			for(k=0;k<15;k++) /* pixels in a row */
			{
				if(*(uint16_t *)(dict+i*20*15+j*15+k))
					printf("*");
				else
					printf(" ");
			}
			printf("\n");
		}
	}
}



/*------------------------------------------------------------------------------
write symbol to frame buffer.

blackoff:	 >0 make black pixels transparent
	  	0  keep black pixel
color:		>0 substitude symbol color
		=0 keep symbol color
symbol: 	pointer to symbol data
index:  	index of a symbol in the dict_w20h15.
x0,y0:		start position in screen.

---------------------------------------------------------------------------------*/
void dict_writeFB_symb20x15(FBDEV *fb_dev, int blackoff, uint16_t color, \
				 int index, int x0, int y0)
{
	int i,j;
	FBDEV *dev = fb_dev;
	uint16_t *pdict=dict_h20w15+index*15*20;

	int xres=dev->vinfo.xres;
	//int yres=dev->vinfo.yres;
        long int pos=0; /* position in fb */
	uint16_t symcolor;

	/* check index range */
	if( index > DICT_NUM_LIMIT_H20W15-1 || index < 0 )
	{
//		printf("dict_writeFB_symb20x15(): index out of range! check DICT_NUM_LIMIT \n");
		return;
	}

	//printf("xres=%d,yres=%d \n",xres,yres);

	for(i=0;i<20;i++)  /* in pixel, 20 row each symbol */
	{
		for(j=0;j<15;j++)  /* in pixel, 15 column each symbol */
		{
			symcolor=*(uint16_t *)(pdict+i*15+j);

			if( color && symcolor) /* use specified color */
				symcolor=color;

			pos=(y0+i)*xres+x0+j;/* in pixel */

			if(blackoff && symcolor)/* if make black transparent */
			{
                		*(uint16_t *)(dev->map_fb+pos*2)=symcolor;
			}
			else if(!blackoff) /* else keep black */
			{
                		*(uint16_t *)(dev->map_fb+pos*2)=symcolor;
			}
		}
	}
}

/*-------------------------------------------------------------------
 write a string of char to FBDEV, they must be at the same line.

-------------------------------------------------------------------*/
void wirteFB_str20x15(FBDEV *fb_dev, int blackoff, uint16_t color, \
				 char *str, int x0, int y0)
{
	int i=0;
	int index; /* index in dict_h20w15 */
	//int sx0,sy0;/* start point of each symbol */

	if(str==NULL)
	{
		printf("string is NULL!\n");
		return;
	}

	while(str[i]) /* if not end of string */
	{
		//printf("str[i]=%d\n",str[i]);
		/* convert form ASSIC index to DICT index */
		index=str[i]-48; /* ASSIC of '1' is 48 */
 		dict_writeFB_symb20x15(fb_dev, blackoff, color, \
				 index, x0+i*15, y0); /* write at the same line */
		i++;
	}

}
