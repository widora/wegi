/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-------------------------------------------------------------------*/
#include "egi.h"
#include "egi_timer.h"
#include "egi_fbdev.h"
#include "egi_fbgeom.h"
#include "egi_filo.h"
#include "egi_debug.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>


/* global variale, Frame buffer device */
FBDEV   gv_fb_dev={ .fbfd=-1, }; //__attribute__(( visibility ("hidden") )) ;

/*-------------------------------------
Initiate a FB device.
Return:
        0       OK
        <0      Fails
---------------------------------------*/
int init_fbdev(FBDEV *fb_dev)
{
//	int i;

        if(fb_dev->fbfd > 0) {
           printf("Input FBDEV already open!\n");
           return -1;
        }

        fb_dev->fbfd=open(EGI_FBDEV_NAME,O_RDWR|O_CLOEXEC);
        if(fb_dev<0) {
          printf("Open /dev/fb0: %s\n",strerror(errno));
          return -1;
        }
        printf("%s:Framebuffer device opened successfully.\n",__func__);
        ioctl(fb_dev->fbfd,FBIOGET_FSCREENINFO,&(fb_dev->finfo));
        ioctl(fb_dev->fbfd,FBIOGET_VSCREENINFO,&(fb_dev->vinfo));

        fb_dev->screensize=fb_dev->vinfo.xres*fb_dev->vinfo.yres*(fb_dev->vinfo.bits_per_pixel>>3); /* >>3 /8 */

        /* mmap FB */
        fb_dev->map_fb=(unsigned char *)mmap(NULL,fb_dev->screensize,PROT_READ|PROT_WRITE, MAP_SHARED,
                                                                                        fb_dev->fbfd, 0);
        if(fb_dev->map_fb==MAP_FAILED) {
                printf("Fail to mmap FB: %s\n", strerror(errno));
                close(fb_dev->fbfd);
                return -2;
        }

	/* ---- mmap back mem, map_bk ---- */
	#if defined(ENABLE_BACK_BUFFER) || defined(LETS_NOTE)
	fb_dev->map_buff=(unsigned char *)mmap(NULL,fb_dev->screensize*FBDEV_BUFFER_PAGES, PROT_READ|PROT_WRITE,
									MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if(fb_dev->map_buff==MAP_FAILED) {
                printf("Fail to mmap back mem map_buff for FB: %s\n", strerror(errno));
                munmap(fb_dev->map_fb,fb_dev->screensize);
                close(fb_dev->fbfd);
                return -2;
	}
	#endif

	/* init current back buffer page */
	fb_dev->npg=0;
	fb_dev->map_bk=fb_dev->map_buff;

	/* reset virtual FB, as EGI_IMGBUF */
	fb_dev->virt_fb=NULL;

	/* reset pos_rotate */
	fb_dev->pos_rotate=0;
        fb_dev->pos_xres=fb_dev->vinfo.xres;
        fb_dev->pos_yres=fb_dev->vinfo.yres;

        /* reset pixcolor and pixalpha */
	fb_dev->pixcolor_on=false;
        fb_dev->pixcolor=(30<<11)|(10<<5)|10;
        fb_dev->pixalpha=255;

        /* init fb_filo */
        fb_dev->filo_on=0;
        fb_dev->fb_filo=egi_malloc_filo(1<<13, sizeof(FBPIX), FILO_AUTO_DOUBLE);//|FILO_AUTO_HALVE
        if(fb_dev->fb_filo==NULL) {
                printf("%s: Fail to malloc FB FILO!\n",__func__);
                munmap(fb_dev->map_fb,fb_dev->screensize);
                munmap(fb_dev->map_buff,fb_dev->screensize*FBDEV_BUFFER_PAGES);
                close(fb_dev->fbfd);
                return -3;
        }

        /* assign fb box */
	if(fb_dev==&gv_fb_dev) {
	        gv_fb_box.startxy.x=0;
        	gv_fb_box.startxy.y=0;
	        gv_fb_box.endxy.x=fb_dev->vinfo.xres-1;
        	gv_fb_box.endxy.y=fb_dev->vinfo.yres-1;
	}

	/* clear buffer */
//	for(i=0; i<FBDEV_BUFFER_PAGES; i++) {
//		fb_dev->buffer[i]=NULL;
//	}

#if 0
//      printf("init_dev successfully. fb_dev->map_fb=%p\n",fb_dev->map_fb);
        printf(" \n------- FB Parameters -------\n");
        printf(" bits_per_pixel: %d bits \n",fb_dev->vinfo.bits_per_pixel);
        printf(" line_length: %d bytes\n",fb_dev->finfo.line_length);
        printf(" xres: %d pixels, yres: %d pixels \n", fb_dev->vinfo.xres, fb_dev->vinfo.yres);
        printf(" xoffset: %d,  yoffset: %d \n", fb_dev->vinfo.xoffset, fb_dev->vinfo.yoffset);
        printf(" screensize: %ld bytes\n", fb_dev->screensize);
        printf(" Total buffer pages: %d\n", FBDEV_BUFFER_PAGES);
        printf(" ----------------------------\n\n");
#endif
        return 0;
}

/*-------------------------
Release FB and free map
--------------------------*/
void release_fbdev(FBDEV *dev)
{
//	int i;

        if(!dev || !dev->map_fb)
                return;

	/* free FILO, reset fb_filo to NULL inside */
        egi_free_filo(dev->fb_filo);

	/* unmap FB */
        if( munmap(dev->map_fb,dev->screensize) != 0)
		printf("Fail to unmap FB: %s\n", strerror(errno));

	/* unmap FB back memory */
        if( munmap(dev->map_buff,dev->screensize*FBDEV_BUFFER_PAGES) !=0 )
		printf("Fail to unmap back mem for FB: %s\n", strerror(errno));

	/* free buffer */
//	for( i=0; i<FBDEV_BUFFER_PAGES; i++ )
//	{
//		if( dev->buffer[i] != NULL )
//		{
//			free(dev->buffer[i]);
//			dev->buffer[i]=NULL;
//		}
//	}

        close(dev->fbfd);
        dev->fbfd=-1;
}


/*--------------------------------------------------
Initiate a virtual FB device with an EGI_IMGBUF

Return:
        0       OK
        <0      Fails
---------------------------------------------------*/
int init_virt_fbdev(FBDEV *fb_dev, EGI_IMGBUF *eimg)
{
//	int i;

	/* check input data */
	if(eimg==NULL || eimg->width<=0 || eimg->height<=0 ) {
		printf("%s: Input EGI_IMGBUF is invalid!\n",__func__);
		return -1;
	}

	/* check alpha
	 * NULL is OK
	 */

	/* set virt */
	fb_dev->virt=true;

	/* disable FB parmas */
	fb_dev->fbfd=-1;
	fb_dev->map_fb=NULL;
	fb_dev->fb_filo=NULL;
	fb_dev->filo_on=0;

	/* reset virtual FB, as EGI_IMGBUF */
	fb_dev->virt_fb=eimg;

        /* reset pixcolor and pixalpha */
	fb_dev->pixcolor_on=false;
        fb_dev->pixcolor=(30<<11)|(10<<5)|10;
        fb_dev->pixalpha=255;

	/* set params for virt FB */
	fb_dev->vinfo.bits_per_pixel=16;
	fb_dev->finfo.line_length=eimg->width*2;
	fb_dev->vinfo.xres=eimg->width;
	fb_dev->vinfo.yres=eimg->height;
	fb_dev->vinfo.xoffset=0;
	fb_dev->vinfo.yoffset=0;
	fb_dev->screensize=eimg->height*eimg->width;

	/* reset pos_rotate */
	fb_dev->pos_rotate=0;
	fb_dev->pos_xres=fb_dev->vinfo.xres;
	fb_dev->pos_yres=fb_dev->vinfo.yres;

	/* clear buffer */
//	for(i=0; i<FBDEV_BUFFER_PAGES; i++) {
//		fb_dev->buffer[i]=NULL;
//	}

#if 0
        printf(" \n--- Virtal FB Parameters ---\n");
        printf(" bits_per_pixel: %d bits \n",		fb_dev->vinfo.bits_per_pixel);
        printf(" line_length: %d bytes\n",		fb_dev->finfo.line_length);
        printf(" xres: %d pixels, yres: %d pixels \n", 	fb_dev->vinfo.xres, fb_dev->vinfo.yres);
        printf(" xoffset: %d,  yoffset: %d \n", 	fb_dev->vinfo.xoffset, fb_dev->vinfo.yoffset);
        printf(" screensize: %ld bytes\n", 		fb_dev->screensize);
        printf(" ----------------------------\n\n");
#endif

	return 0;
}


/*---------------------------------
	Release a virtual FB
---------------------------------*/
void release_virt_fbdev(FBDEV *dev)
{
	dev->virt=false;
	dev->virt_fb=NULL;
}

/*-----------------------------------------------------------
Shift/switch fb_dev->map_bk to point to the indicated buffer
page, as for the current working buffer page.

@fb_dev:	struct FBDEV to operate.
@numpg:		The number/index of buffer page put into play.
		[0  FBDEV_MAX_BUFFE-1]

-----------------------------------------------------------*/
inline void fb_shift_buffPage(FBDEV *fb_dev, unsigned int numpg)
{
	if(fb_dev==NULL || fb_dev->map_buff==NULL)
		return;

	numpg=numpg%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */
	fb_dev->map_bk=fb_dev->map_buff+fb_dev->screensize*numpg;
}

/*-----------------------------------------------------
		Set/Reset DirectFB mode

@NoBuff:	True,  set map_bk to map_fb
		False, set map_bk to map_buff
------------------------------------------------------*/
void fb_set_directFB(FBDEV *fb_dev, bool NoBuff)
{
        if( fb_dev==NULL || fb_dev->map_bk==NULL)
                return;

	if(NoBuff)
		fb_dev->map_bk=fb_dev->map_fb;
	else
		fb_dev->map_bk=fb_dev->map_buff;  /* Default as in init_fbdev() */
}


/*-------------------------------------------
Prepare FB background and working buffer.
Init buffers with current FB mmap data.
-------------------------------------------*/
void fb_init_FBbuffers(FBDEV *fb_dev)
{
        if( fb_dev==NULL || fb_dev->map_bk==NULL)
                return;

         /* Prepare FB background buffer */
         memcpy(fb_dev->map_buff+fb_dev->screensize, fb_dev->map_fb, fb_dev->screensize);

         /* prepare FB working buffer */
         memcpy(fb_dev->map_buff, fb_dev->map_buff+fb_dev->screensize, fb_dev->screensize);
}



/*-----------------------------------------------------------
    Clear all FB back buffs by filling with given color

@fb_dev:	struct FBDEV whose buffer to be cleared.
@color:		Color used to fill the buffer, 16bit or 32bits.
		Depends on fb_dev->vinfo.bits_per_pixel.

    !!! Note: to make sure that type INT is 32bits !!!
------------------------------------------------------------*/
void fb_clear_backBuff(FBDEV *fb_dev, uint32_t color)
{
        unsigned int pixels; /* Total pixels in the buffer */
	int i;

        if( fb_dev==NULL || fb_dev->map_bk==NULL)
                return;

        pixels=fb_dev->vinfo.xres*fb_dev->vinfo.yres;

	/* For 16bits RGB color pixel */
        if(fb_dev->vinfo.bits_per_pixel==2*8) {
		for(i=0; i<pixels; i++)
			*(uint16_t *)(fb_dev->map_bk+(i<<1))=color;
	}
	/* For 32bits ARGB color pixel */
        else if(fb_dev->vinfo.bits_per_pixel==4*8) {
		for(i=0; i<pixels; i++)
			*(uint32_t *)(fb_dev->map_bk+(i<<2))=color;
	}
        //else 	--- NOT SUPPORT --
}


/*-------------------------------------------------
 Refresh FB screen with FB back buffer map_buff[numpg]
--------------------------------------------------*/
void fb_page_refresh(FBDEV *dev, unsigned int numpg)
{
	if(dev==NULL)
		return;

	if( dev->map_bk==NULL || dev->map_fb==NULL )
		return;

        numpg=numpg%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */

	/* Try to synchronize with FB kernel VSYNC */
	if( ioctl( dev->fbfd, FBIO_WAITFORVSYNC, 0) !=0 ) {
#ifdef LETS_NOTE
                printf("Fail to ioctl FBIO_WAITFORVSYNC.\n");
        } else { /* memcpy to FB, ignore VSYNC signal. */
#endif
		switch(numpg) {
			case 0:
				memcpy(dev->map_fb, dev->map_buff, dev->screensize);
				break;
			case 1:
				memcpy(dev->map_fb, dev->map_buff+dev->screensize, dev->screensize);
				break;
			case 2:
				memcpy(dev->map_fb, dev->map_buff+(dev->screensize<<1), dev->screensize);
				break;
			default:
				memcpy(dev->map_fb, dev->map_buff+dev->screensize*numpg, dev->screensize);
				break;
		}
	}
}


/*------------------------------------------------------------------------------------
 Refresh FB screen lines with FB back buffer map_buff[numpg]
 !!! NOTE: Soft Pos_roation has NO effect for this function. !!!

@sind:	Starting Line index. [0  FBDEV.vinfo.yres-1]
@n: 	Number of lines to be refreshed.
-------------------------------------------------------------------------------------*/
void fb_lines_refresh(FBDEV *dev, unsigned int numpg, unsigned int sind, unsigned int n)
{
	unsigned int xres;
	unsigned int Bpp; /* byte per pixel */
	unsigned int Bpl; /* bytes per line */

        if(dev==NULL)
                return;
        if( dev->map_bk==NULL || dev->map_fb==NULL )
                return;
	if( n==0 || sind > dev->vinfo.yres-1 )
		return;

	if( sind+n > dev->vinfo.yres ) /* sind+n-1 > dev->vinfo.yres-1 */
		n=dev->vinfo.yres-sind;

        numpg=numpg%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */
	xres=dev->vinfo.xres;
	Bpp=dev->vinfo.bits_per_pixel>>3;
	Bpl=Bpp*xres;

        /* Try to synchronize with FB kernel VSYNC */
        if( ioctl( dev->fbfd, FBIO_WAITFORVSYNC, 0) !=0 ) {
#ifdef LETS_NOTE
                printf("Fail to ioctl FBIO_WAITFORVSYNC.\n");
        } else { /* memcpy to FB, ignore VSYNC signal. */
#endif
		switch(numpg) {
			case 0:
				memcpy(dev->map_fb+sind*Bpl, dev->map_buff+sind*Bpl, n*Bpl);
				break;
			case 1:
				memcpy(dev->map_fb+sind*Bpl, dev->map_buff+dev->screensize+sind*Bpl, n*Bpl);
				break;
			case 2:
				memcpy(dev->map_fb+sind*Bpl, dev->map_buff+(dev->screensize<<1)+sind*Bpl, n*Bpl);
				break;
			default:
				memcpy(dev->map_fb+sind*Bpl, dev->map_buff+dev->screensize*numpg+sind*Bpl, n*Bpl);
				break;
		}
	}
}


/*-----------------------------------------------------
 Backup current page data in dev->map_fb
 to dev->map_buff[index], which normally is
 NOT current working back buff!

@dev:	 current FB device.
@buffNum:  index of dev->map_buff[].

Retrun:
	0	OK
	<0 	Fails
------------------------------------------------------*/
int fb_page_saveToBuff(FBDEV *dev, unsigned int buffNum)
{
	if( dev==NULL || dev->map_fb==NULL || dev->map_buff==NULL )
		return -1;

        buffNum=buffNum%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */

        memcpy( dev->map_buff+(buffNum*dev->screensize), dev->map_fb, dev->screensize);

	return 0;
}


/*---------- !!! similar to fb_page_refresh() !!! -----------

 Restore current page data in dev->map_fb
 from dev->map_buff[index], which normally is
 NOT current working back buff!

@dev:	 current FB device.
@buffNum:  index of dev->map_buff[].

Retrun:
	0	OK
	<0 	Fails
-----------------------------------------------------------*/
int fb_page_restoreFromBuff(FBDEV *dev, unsigned int buffNum)
{
	if( dev==NULL || dev->map_fb==NULL || dev->map_buff==NULL )
		return -1;

        buffNum=buffNum%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */
        memcpy( dev->map_fb, dev->map_buff+(buffNum*dev->screensize), dev->screensize);

	return 0;
}


/*-------------------------------------------
 Refresh FB with current back buffer
 pointed by fb_dev->map_bk.

 @speed:	in pixels per refresh.
		Not so now...

 Method: Fly in...
--------------------------------------------*/
int fb_page_refresh_flyin(FBDEV *dev, int speed)
{
	int i;
	int n;
	unsigned int line_length=dev->finfo.line_length;

	if(dev==NULL)
		return -1;

	if( dev->map_bk==NULL || dev->map_fb==NULL )
		return -2;

	/* numbers of fly steps */
	n=dev->vinfo.yres/speed;

	for(i=1; i<=n; i++)
	{
		/* Try to synchronize with FB kernel VSYNC */
		if( ioctl( dev->fbfd, FBIO_WAITFORVSYNC, 0) !=0 ) {
                	//printf("Fail to ioctl FBIO_WAITFORVSYNC.\n");

	        // } else  { /* memcpy to FB, ignore VSYNC signal. */
			memcpy( dev->map_fb + (dev->screensize-i*speed*line_length),
				dev->map_bk,
				i*speed*line_length );
		}

		//tm_delayms(10);
		usleep(10000);
	}

	return 0;
}


/*--------------------------------------------------------------
Refresh FB with back buffer starting from offset lines(offl),
If offl is out of back buffer range[0 yres*FBDEV_BUFFER_PAGES-1],
it just ignores and returns.

@offl:	Offset lines from the starting point of the whole FB
	back buffers, whose virtual addresses are consecutive.
	Lines may align with X or Y LCD screen direction,
	depending on FB setup for Portrait or Landscape mode.

Note: The caller shall ensure that offl is indexed within the back
      buffer range, or the function will just ignore and return. In
      such case, the caller shall takes its modulo value as loop back
      to within the range.

      It's better for the caller to take modulo calculation!!!


----------------------------------------------------------------*/
int fb_slide_refresh(FBDEV *dev, int offl)
{

	/* IGNORE: check input params */

	unsigned int yres=dev->vinfo.yres;
	unsigned int line_length=dev->finfo.line_length;

        /* CASE 1: offl is within the resonable range, but NOT in the last buffer page. */
        if ( offl > -1 && offl < yres*(FBDEV_BUFFER_PAGES-1)+1 ) {
                memcpy(dev->map_fb, dev->map_buff+line_length*offl, dev->screensize);
        }

        /* CASE 2: offl is out of back buffer range */
        else if (offl<0 || offl > yres*FBDEV_BUFFER_PAGES-1) {
		return -1;
        }

        /* CASE 3: offl is indexed to within the last page of FB back buffer,
	 *  then the mem for FB displaying is NOT consective, and shall be copied
	 *  from 2 blocks in the back buffer.
	 */
        else  {  /* ( offl>yres*(FBDEV_BUFFER_PAGES-1) && offl<yres*FBDEV_BUFFER_PAGES) */

        	/*  Copy from the last buffer page: Line [offl to yres*N-1] */
                memcpy( dev->map_fb, dev->map_buff+line_length*offl,
                                                  line_length*(yres*FBDEV_BUFFER_PAGES-offl) );

   		/*  Copy from buffer page 0:  Line [0 to offl-yres*(N-1) ]   */
                memcpy( dev->map_fb+line_length*(yres*FBDEV_BUFFER_PAGES-offl), dev->map_buff,
                                                   line_length*(offl-yres*(FBDEV_BUFFER_PAGES-1)) );
        }

	return 0;
}


/*-------------------------------------------------------------
Put fb->filo_on to 1, as turn on FB FILO.

Note:
1. To activate FB FILO, depends also on FB_writing handle codes.

Midas Zhou
--------------------------------------------------------------*/
inline void fb_filo_on(FBDEV *dev)
{
        if(!dev || !dev->fb_filo)
                return;

        dev->filo_on=1;
}

inline void fb_filo_off(FBDEV *dev)
{
        if(!dev || !dev->fb_filo)
                return;

        dev->filo_on=0;
}


/*----------------------------------------------
Pop out all FBPIXs in the fb filo
Midas Zhou
----------------------------------------------*/
inline void fb_filo_flush(FBDEV *dev)
{
        FBPIX fpix;

        if(!dev || !dev->fb_filo)
                return;

        while( egi_filo_pop(dev->fb_filo, &fpix)==0 )
        {
                /* write back to FB */
                //printf("EGI FILO pop out: pos=%ld, color=%d\n",fpix.position,fpix.color);

		#ifdef LETS_NOTE  /*--- 4 bytes per pixel ---*/
		  *((uint32_t *)(dev->map_bk+fpix.position)) = fpix.argb;

		#else		  /*--- 2 bytes per pixel ---*/
		   #ifdef ENABLE_BACK_BUFFER
                   *((uint16_t *)(dev->map_bk+fpix.position)) = fpix.color;
		   #else
                   *((uint16_t *)(dev->map_fb+fpix.position)) = fpix.color;
		   #endif
		#endif
        }
}

/*----------------------------------------------
Dump and get rid of all FBPIXs in the fb filo,
Do NOT write back to FB.
Midas Zhou
----------------------------------------------*/
void fb_filo_dump(FBDEV *dev)
{
        if(!dev || !dev->fb_filo)
                return;

        while( egi_filo_pop(dev->fb_filo, NULL)==0 ){};
}


/*--------------------------------------------------
Rotate FB displaying position relative to LCD screen.

Pos	Rotatio(clockwise)
0	0   ( --- LCD original position --- )
1	90
2	180
3	270

For 240x320:
Landscape displaying: pos=1 or 3
Portrait displaying: pos=0 or 2

---------------------------------------------------*/
void fb_position_rotate(FBDEV *dev, unsigned char pos)
{

        if(dev==NULL || dev->fbfd<0 ) {
		printf("%s: Input FBDEV is invalid!\n",__func__);
		return;
	}

        /* Set pos_rotate */
        dev->pos_rotate=(pos & 0x3); /* Restricted to 0,1,2,3 only */

	/* set pos_xres and pos_yres */
	if( (pos & 0x1) == 0 ) {		/* 0,2  Portrait mode */
		/* reset new resolution */
	        dev->pos_xres=dev->vinfo.xres;
        	dev->pos_yres=dev->vinfo.yres;

		/* reset gv_fb_box END point */
		if(dev==&gv_fb_dev) {
			gv_fb_box.endxy.x=dev->vinfo.xres-1;
			gv_fb_box.endxy.y=dev->vinfo.yres-1;
		}
	}
	else {					/* 1,3  Landscape mode */
		/* reset new resolution */
	        dev->pos_xres=dev->vinfo.yres;
        	dev->pos_yres=dev->vinfo.xres;

		/* reset gv_fb_box END point */
		if(dev==&gv_fb_dev) {
			gv_fb_box.endxy.x=dev->vinfo.yres-1;
			gv_fb_box.endxy.y=dev->vinfo.xres-1;
		}
	}
}
