/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Journal
2021-02-22:
	1. init_fbdev(): Add zbuff.
2021-02-28:
	1. Apply zbuff_on/pixz for FBDEV.
	2. Add fb_reset_zbuff();
2021-03-17:
	1. Add reinit_virt_fbdev().
2021-03-31:
	1. Add init_virt_fbdev2(fb_dev, xres, yres, alpha, color).
2021-04-04:
	1. Add member VFrameImg for virtual FBDEV
	   and accordingly revise following functions:
	   init_virt_fbdev(), init_virt_fbdev2(), release_virt_fbdev(), reinit_virt_fbdev()
	   int vfb_render().
2021-08-07:
	1. Add fb_init_zbuff().
2021-08-12:
	1. Add memeber FBDEV.flipZ

Midas Zhou
midaszhou@yahoo.com
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
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdlib.h>


/* Global/SYS Framebuffer Device */
FBDEV   gv_fb_dev={ .devname="/dev/fb0", .fbfd=-1,  }; //__attribute__(( visibility ("hidden") )) ;

/*-------------------------------------
Initialize a FB device.

@fb_dev:   Pointer to a static FBDEV.

Return:
        0       OK
        <0      Fails
-------------------------------------*/
int init_fbdev(FBDEV *fb_dev)
{
//	int i;
//	int bytes_per_pixel;

        if(fb_dev->fbfd > 0) {
	    egi_dpstd("Input FBDEV already open!\n");
            return -1;
        }
	if(fb_dev->devname==NULL) {
	    egi_dpstd("Input FBDEV name is invalid!\n");
	    return -1;
	}

        //fb_dev->fbfd=open(WEGI_FBDEV_NAME,O_RDWR|O_CLOEXEC);
        fb_dev->fbfd=open(fb_dev->devname,O_RDWR|O_CLOEXEC);
        if(fb_dev<0) {
	    egi_dperr("Open dev '%s'", fb_dev->devname);
            return -1;
        }

	/* Get FBDEV Info */
	egi_dpstd("Framebuffer device opened successfully.\n");
        ioctl(fb_dev->fbfd,FBIOGET_FSCREENINFO,&(fb_dev->finfo));
        ioctl(fb_dev->fbfd,FBIOGET_VSCREENINFO,&(fb_dev->vinfo));

	//bytes_per_pixel=fb_dev->vinfo.bits_per_pixel>>3;
        //fb_dev->screensize=fb_dev->vinfo.xres*fb_dev->vinfo.yres*(fb_dev->vinfo.bits_per_pixel>>3); /* >>3 /8 */
        fb_dev->screensize=fb_dev->finfo.line_length*fb_dev->vinfo.yres; /* >>3 /8 */

	/* Calloc zbuff */
	fb_dev->zbuff=calloc(1, (fb_dev->vinfo.xres)*(fb_dev->vinfo.yres)*sizeof(typeof(*fb_dev->zbuff)));
	if(fb_dev->zbuff==NULL) {
		egi_dperr("Fail to calloc zbuff!");
                close(fb_dev->fbfd); fb_dev->fbfd=-1;
		return -2;
	}

        /* mmap FB */
        fb_dev->map_fb=(unsigned char *)mmap(NULL,fb_dev->screensize,PROT_READ|PROT_WRITE, MAP_SHARED,
                                                                                        fb_dev->fbfd, 0);
        if(fb_dev->map_fb==MAP_FAILED) {
		egi_dperr("Fail to mmap map_fb!");
		free(fb_dev->zbuff);
                close(fb_dev->fbfd); fb_dev->fbfd=-1;
                return -2;
        }

	/* mmap back mem, map_bk */
	#if defined(ENABLE_BACK_BUFFER) || defined(LETS_NOTE)
	fb_dev->map_buff=(unsigned char *)mmap(NULL,fb_dev->screensize*FBDEV_BUFFER_PAGES, PROT_READ|PROT_WRITE,
									MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if(fb_dev->map_buff==MAP_FAILED) {
		egi_dperr("Fail to mmap map_buff!");
                munmap(fb_dev->map_fb,fb_dev->screensize);
		free(fb_dev->zbuff);
                close(fb_dev->fbfd); fb_dev->fbfd=-1;
                return -2;
	}
	fb_dev->npg=0;	/* init current back buffer page number */
	fb_dev->map_bk=fb_dev->map_buff;  /* Point map_bk to map_buff */
	#else /* -- DISABLE BACK BUFFER -- */
	fb_dev->npg=0;  	/* not applicable */
	fb_dev->map_bk=NULL;    /* not applicable */
	#endif

	/* Init current back buffer page */
	//fb_dev->npg=0;
	//fb_dev->map_bk=fb_dev->map_buff;

	/* Reset virtual FB, as EGI_IMGBUF */
	fb_dev->virt_fb=NULL;
	fb_dev->VFrameImg=NULL;

	/* Reset pos_rotate */
	fb_dev->pos_rotate=0;
        fb_dev->pos_xres=fb_dev->vinfo.xres;
        fb_dev->pos_yres=fb_dev->vinfo.yres;

	/* Check: vinfo.xres NOT equals finfo.line_length/bytes_per_pixel */
	if( fb_dev->finfo.line_length/(fb_dev->vinfo.bits_per_pixel>>3) != fb_dev->vinfo.xres )
	{
	 	  fprintf(stderr,"\n\e[38;5;196;48;5;0m WARNING: vinfo.xres != finfo.line_length/bytes_per_pixel. \e[0m\n");
	 	  //egi_dperr("\n\e[38;5;196;48;5;0m WARNING: vinfo.xres != finfo.line_length/bytes_per_pixel. \e[0m");
	}

        /* Reset pixcolor and pixalpha */
	fb_dev->pixcolor_on=false;
        fb_dev->pixcolor=(30<<11)|(10<<5)|10;
        fb_dev->pixalpha=255;

        /* Init fb_filo */
        fb_dev->filo_on=0;
        fb_dev->fb_filo=egi_malloc_filo(1<<13, sizeof(FBPIX), FILO_AUTO_DOUBLE);//|FILO_AUTO_HALVE
        if(fb_dev->fb_filo==NULL) {
                egi_dpstd("Fail to malloc FB FILO!\n");
                munmap(fb_dev->map_fb, fb_dev->screensize);
                munmap(fb_dev->map_buff, fb_dev->screensize*FBDEV_BUFFER_PAGES);
		free(fb_dev->zbuff);
                close(fb_dev->fbfd); fb_dev->fbfd=-1;
                return -3;
        }

        /* Assign gv_fb_box */
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

#if 1
//      printf("init_dev successfully. fb_dev->map_fb=%p\n",fb_dev->map_fb);
	printf("\e[38;5;34;48;5;0m");
        printf("\n -------- FB Parameters --------\n");
        printf(" bits_per_pixel: %d bits \n",fb_dev->vinfo.bits_per_pixel);
        printf(" line_length: %d bytes\n",fb_dev->finfo.line_length);
        printf(" xres: %d pixels, yres: %d pixels \n", fb_dev->vinfo.xres, fb_dev->vinfo.yres);
        printf(" xoffset: %d,  yoffset: %d \n", fb_dev->vinfo.xoffset, fb_dev->vinfo.yoffset);
        printf(" screensize: %ld bytes\n", fb_dev->screensize);
        printf(" Total buffer pages: %d\n", FBDEV_BUFFER_PAGES);
	printf(" Zbuffer: ON\n");
        printf(" ------------  EGi  ------------\n\n");
	printf("\e[0m\n");
#endif

        return 0;
}

/*---------------------------------------------------
Release FB and free map
Virtual FBDEV: see release_virt_fbdev().

Note:
1. Input FBDEV is considered as statically allocated.
   All members MUST be properly initialized and hold
   valid memory space.

----------------------------------------------------*/
void release_fbdev(FBDEV *dev)
{
        if( !dev || !dev->map_fb )
                return;
	if( dev->virt ) {
		egi_dpstd("It's a virtual FBDEV! Call release_virt_fbdev() instead!\n");
		return;
	}

	/* Free FILO, reset fb_filo to NULL inside */
        egi_filo_free(&dev->fb_filo);

	/* Unmap FB */
        if( munmap(dev->map_fb, dev->screensize) != 0) {
		egi_dperr("unmap dev->map_fb");
	}
	dev->map_fb=NULL;

	/* Unmap FB back memory */
        if( munmap(dev->map_buff,dev->screensize*FBDEV_BUFFER_PAGES) !=0 ) {
		egi_dperr("unmap dev->map_buff");
	}
	dev->map_buff=NULL;

	/* Free zbuff */
	free(dev->zbuff);
	dev->zbuff=NULL;

	/* Close fbfd */
        close(dev->fbfd);
        dev->fbfd=-1;
}


/*----------- TODO: FB driver support ------------------------------------------
(Temporarily) Set screen Vinfo for FB HW, and update
fb_dev params accordingly.

NOTE:
1. This is NOT for soft po_rotate screen, it's for HW set screen params !!!
2. Not applicable for virtual FBDEV.

@old_vinfo: Old screen vinfo to be saved.
@new_vifno: new screen vinfo to be set.

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------------------------*/
int fb_set_screenVinfo(FBDEV *fb_dev, struct fb_var_screeninfo *old_vinfo,  const struct fb_var_screeninfo *new_vinfo)
{

	if(fb_dev==NULL || fb_dev->map_buff==NULL)
		return -1;

	/* Set new params for HW FB */
	/* Check: Driver FBIOPUT */
        if(ioctl(fb_dev->fbfd,FBIOPUT_VSCREENINFO, new_vinfo)!=0) {
		printf("%s: Fail to ioctl  FBIOSET_VSCREENINFO\n",__func__);
		return -2;
	}

	/* Save old vinfo */
	if(old_vinfo!=NULL)
		*old_vinfo=fb_dev->vinfo;

	/* Read in new HW vinfo */
	ioctl(fb_dev->fbfd,FBIOGET_VSCREENINFO,&(fb_dev->vinfo));
#if 1
	printf("%s: New vinfo set as: xres=%d, yres=%d\n",__func__, fb_dev->vinfo.xres, fb_dev->vinfo.yres);
#endif
	/* Reset fb_dev parmas */
        //fb_dev->screensize=fb_dev->vinfo.xres*fb_dev->vinfo.yres*(fb_dev->vinfo.bits_per_pixel>>3); /* >>3 /8 */
        fb_dev->screensize=fb_dev->vinfo.yres*fb_dev->finfo.line_length;

	/* Note: pos_xres, pos_yres NOT updated! */

	return 0;
}


/*----------- TODO: FB driver support --------------------------------------
(Temporarily) Set screen position via FB HW, and update
fb_dev params accordingly.

NOTE:
1. This is NOT for soft po_rotate screen, it's for HW set screen params !!!
2. Not applicable for virtual FBDEV.

@xres:	    xres of screen
@yres:	    yres of screen
@offx:      X offset of screen image
@offy: 	    Y offset of screen image

TODO:       offx, offy NOT applicable yet!

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------------*/
int fb_set_screenPos(FBDEV *fb_dev, unsigned int xres, unsigned int yres)
				  //  unsigned int xoff,  unsigned yoff )
{
	if(fb_dev==NULL || fb_dev->map_buff==NULL)
		return -1;

	struct fb_var_screeninfo new_vinfo=fb_dev->vinfo;

	/* Set new params */
	new_vinfo.xres=xres;
	new_vinfo.yres=yres;
//	new_vinfo.xoffset=xoff;
//	new_vinfo.yoffset=yoff;

	/* Set HW and update fb_dev */
        if( fb_set_screenVinfo(fb_dev, NULL, &new_vinfo)!=0 )
		return -2;

	/* Note: pos_xres, pos_yres NOT updated! */

	return 0;
}


/*--------------------------------------------------------------------
Initiate a virtual FB device with an EGI_IMGBUF
, which is only a ref. pointer and will NOT be freed
in release_virt_fbdev(), except explicitly set fb_dev->vimg_owner=true.

!!! WARNING !!! Caller clear fb_dev first.

@dev:   	Pointer to statically allocated FBDEV.
@fbimg:  	Pointer to an EGI_IMGBUF, as fb_dev->virt_fb.
		MUST NOT be NULL!
@FramImg:	Pointer to an EGI_IMGBUF, as fb_dev->VFrameImg.
		MAY be NULL.

Return:
        0       OK
        <0      Fails
----------------------------------------------------------------------*/
int init_virt_fbdev(FBDEV *fb_dev, EGI_IMGBUF *fbimg, EGI_IMGBUF *FrameImg)
{
	/* Check input data */
	if( fbimg==NULL || fbimg->imgbuf==NULL || fbimg->width<=0 || fbimg->height<=0 ) {
		egi_dpstd("Input fbimg is invalid!\n");
		return -1;
	}

	/* Check FrameImg */
	if( FrameImg ) {
		if( FrameImg->imgbuf==NULL || FrameImg->width<=0 || FrameImg->height<=0 ) {
			egi_dpstd("Input FrameImg is invalid!\n");
			return -1;
		}
		/* Compare size with fbimg */
		if( fbimg->width != FrameImg->width || fbimg->height != FrameImg->height ) {
			egi_dpstd("Input FrameImg is invalid!\n");
			return -1;
		}
	}

	/* check alpha
	 * NULL is OK
	 */

	/* Set virt */
	fb_dev->virt=true;

	/* Disable FB parmas */
	fb_dev->fbfd=-1;
	fb_dev->map_fb=NULL;
	fb_dev->map_buff=NULL;
	fb_dev->map_bk=NULL;
	fb_dev->fb_filo=NULL;
	fb_dev->filo_on=0;

	fb_dev->zbuff_on=false;

	/* reset virtual FB, as EGI_IMGBUF */
	fb_dev->virt_fb=fbimg;
	fb_dev->VFrameImg=FrameImg;
	/* XXX fb_dev->img_owner=false;
	 * NOTE: Since fb_dev is a static FBDEV, just let the caller set it before calling this function,
	 * 	 see in init_virt_fbdev2(); thus it can print vimg_owner together with other paramters here.
	 */

        /* Reset pixcolor and pixalpha */
	fb_dev->pixcolor_on=false;
        fb_dev->pixcolor=(30<<11)|(10<<5)|10;
        fb_dev->pixalpha=255;

	/* Set params for virt FB */
	fb_dev->vinfo.bits_per_pixel=16;
	fb_dev->finfo.line_length=fbimg->width*2;
	fb_dev->vinfo.xres=fbimg->width;
	fb_dev->vinfo.yres=fbimg->height;
	fb_dev->vinfo.xoffset=0;
	fb_dev->vinfo.yoffset=0;
	fb_dev->screensize=fbimg->height*fbimg->width;

	/* Reset pos_rotate */
	fb_dev->pos_rotate=0;
	fb_dev->pos_xres=fb_dev->vinfo.xres;
	fb_dev->pos_yres=fb_dev->vinfo.yres;

	/* Clear buffer */
//	for(i=0; i<FBDEV_BUFFER_PAGES; i++) {
//		fb_dev->buffer[i]=NULL;
//	}

#if 0
        printf(" \n--- Virtual FB Parameters ---\n");
        printf(" bits_per_pixel: %d bits \n",		fb_dev->vinfo.bits_per_pixel);
        printf(" line_length: %d bytes\n",		fb_dev->finfo.line_length);
        printf(" xres: %d pixels, yres: %d pixels \n", 	fb_dev->vinfo.xres, fb_dev->vinfo.yres);
        printf(" xoffset: %d,  yoffset: %d \n", 	fb_dev->vinfo.xoffset, fb_dev->vinfo.yoffset);
        printf(" screensize: %ld bytes\n", 		fb_dev->screensize);
        printf(" VFrameImg:	%s\n",			(fb_dev->VFrameImg!=NULL)?"Yes":"No");
        printf(" Vimg_owner:	%s\n",			fb_dev->vimg_owner?"True":"False");
        printf(" ----------------------------\n\n");
#endif

	return 0;
}


/*--------------------------------------------------
Initiate a virtual FB device by creating EGI_IMGBUFs
as fb_dev->virt_fb, and fb_dev->VFrameImg.

@dev:     Pointer to statically allocated FBDEV.
@xres:    Width of virt_fb pointed IMGBUF.
@yres:    Height of virt_fb pointed IMGBUF.
@alpha:	  Inital alpha value for virt_fb pointed IMGBUF
	  If <0, NO alpha.
@color:   Initial color value for virt_fb pointed IMGBUF

Return:
        0       OK
        <0      Fails
---------------------------------------------------*/
int init_virt_fbdev2(FBDEV *fb_dev, int xres, int yres, int alpha, EGI_16BIT_COLOR color)
{
	EGI_IMGBUF *fbimg=NULL;
	EGI_IMGBUF *FrameImg=NULL;

	/* Check input */
	if( xres<=0 || yres<=0 )
		return -1;

	/* Limt Alpha */
	if(alpha>255)
		alpha=255;

	/* Create fbimg */
	if(alpha<0)
		fbimg=egi_imgbuf_createWithoutAlpha( yres, xres, color);
	else
		fbimg=egi_imgbuf_create( yres, xres, (unsigned char)alpha, color);

	if(fbimg==NULL) {
		egi_dpstd("Fail to create fbimg!\n");
		return -2;
	}

	/* Create FrameImg */
	if(alpha<0)
		FrameImg=egi_imgbuf_createWithoutAlpha( yres, xres, color);
	else
		FrameImg=egi_imgbuf_create( yres, xres, (unsigned char)alpha, color);

	if(FrameImg==NULL) {
		egi_dpstd("Fail to create FrameImg!\n");
		egi_imgbuf_free2(&fbimg);
		return -3;
	}


	/*** Set Ownership of virt_fb pointed IMGBUF
	 * NOTE: set it before init_virt_fbdev() to let print the information in init_virt_fbdev()! */
	fb_dev->vimg_owner=true;

	/* Init virt fbdev with fbimg and FrameImg */
	if( init_virt_fbdev(fb_dev, fbimg, FrameImg)<0 ) {
		egi_dpstd("Fail to init fbdev!\n");
		egi_imgbuf_free2(&fbimg);
		egi_imgbuf_free2(&FrameImg);
		return -4;
	}

	return 0;
}


/*-----------------------------------------
	Release a virtual FB
@dev: Pointer to statically allocated FBDEV.
------------------------------------------*/
void release_virt_fbdev(FBDEV *dev)
{
	dev->virt=false;

	if(dev->vimg_owner) {
		egi_imgbuf_free2(&dev->virt_fb);
		egi_imgbuf_free2(&dev->VFrameImg);
	}

	dev->virt_fb=NULL;
	dev->VFrameImg=NULL;
}


/*--------------------------------------------------------
Re_initilize a virtual FBDEV. Just to upate the reference
pointer of fb_dev->virt_fb with eimg.

@dev:   Pointer to statically allocated FBDEV.
@eimg:  Pointer to an EGI_IMGBUF.

Return:
        0       OK
        <0      Fails
--------------------------------------------------------*/
int reinit_virt_fbdev(FBDEV *fb_dev, EGI_IMGBUF *fbimg, EGI_IMGBUF *FrameImg)
{
	/* If fb_dev is the Owner of virt_fb, then abort. */
	if(fb_dev->vimg_owner) {
		egi_dpstd("FBDEV owns the virt_fb IMGBUF! Can NOT reintialize it!\n");
		return -1;
	}

	//release_virt_fbdev(fb_dev);

	/* It's reenterable */
	return init_virt_fbdev(fb_dev, fbimg, FrameImg);
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

@directFB:	True,  set map_bk to map_fb
		False, set map_bk to map_buff
------------------------------------------------------*/
void fb_set_directFB(FBDEV *fb_dev, bool directFB)
{
        if( fb_dev==NULL || fb_dev->map_bk==NULL || fb_dev->map_buff==NULL )
                return;

	if(directFB)
		fb_dev->map_bk=fb_dev->map_fb;
	else
		fb_dev->map_bk=fb_dev->map_buff;  /* Default as in init_fbdev() */
}


/*-------------------------------------------------
Get current FB mode.

Return:
	-1:	Invalid FB mode.
	 1:	Direct FB mode.
		1 to coincide with fb_set_directFB(,ture)
	 0:	Buffered FB mode
---------------------------------------------------*/
int fb_get_FBmode(FBDEV *fb_dev)
{
        if( fb_dev==NULL || fb_dev->map_bk==NULL || fb_dev->map_buff==NULL )
                return -1;

        if( fb_dev->map_bk==fb_dev->map_fb )   		/* DirectFB mode */
		return 0;
        else  /* fb_dev->map_bk==fb_dev->map_buff */    /* Non_DirectFB mode */
		return 1;
}


/*-------------------------------------------
Prepare FB background and working buffer.
Init buffers with current FB mmap data.
-------------------------------------------*/
void fb_init_FBbuffers(FBDEV *fb_dev)
{
        if( fb_dev==NULL || fb_dev->map_bk==NULL || fb_dev->map_buff==NULL )
                return;

         /* Prepare FB background buffer */
         memcpy(fb_dev->map_buff+fb_dev->screensize, fb_dev->map_fb, fb_dev->screensize);

         /* prepare FB working buffer */
         memcpy(fb_dev->map_buff, fb_dev->map_buff+fb_dev->screensize, fb_dev->screensize);
}


/*---------------------------------------------------
Copy between buffer pages in a FB dev.
----------------------------------------------------*/
void fb_copy_FBbuffer(FBDEV *fb_dev,unsigned int from_numpg, unsigned int to_numpg)
{
        if( fb_dev==NULL || fb_dev->map_bk==NULL || fb_dev->map_buff==NULL )
                return;

	if(from_numpg==to_numpg)
		return;

        from_numpg=from_numpg%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */
        to_numpg=to_numpg%FBDEV_BUFFER_PAGES;

	memcpy( fb_dev->map_buff+to_numpg*fb_dev->screensize,
		fb_dev->map_buff+from_numpg*fb_dev->screensize,
		fb_dev->screensize );

}


/*-----------------------------------------------------------
    Clear FB back buffs by filling with given color

		  !!! --- OBSELET ---- !!!
To call following functions instead:
 fb_clear_workBuff(), fb_clear_bkgBuff(), fb_clear_mapBuffer()


Also see: fb_clear_workBuff() for WEGI_16BIT_COLOR.

@fb_dev:	struct FBDEV whose buffer to be cleared.
@color:		Color used to fill the buffer, 16bit or 32bits.
		Depends on fb_dev->vinfo.bits_per_pixel.

    !!! Note: to make sure that type INT is 32bits !!!
------------------------------------------------------------*/
void fb_clear_backBuff(FBDEV *fb_dev, uint32_t color)
{
	int i,j;
	int bytes_per_pixel;
	unsigned int pos;
        unsigned int pixels; /* Total pixels in the buffer */

        if( fb_dev==NULL || fb_dev->map_bk==NULL)
                return;

	bytes_per_pixel=fb_dev->vinfo.bits_per_pixel>>3;

        pixels=fb_dev->vinfo.xres*fb_dev->vinfo.yres;

	/* For 16bits RGB color pixel */
        if(fb_dev->vinfo.bits_per_pixel==2*8) {
		for(i=0; i<pixels; i++)
			*(uint16_t *)(fb_dev->map_bk+(i<<1))=color;
	}
	/* For 32bits ARGB color pixel */
        else if(fb_dev->vinfo.bits_per_pixel==4*8) {
		#if 0
		for(i=0; i<pixels; i++)
			*(uint32_t *)(fb_dev->map_bk+(i<<2))=color;
		#else
		for(i=0; i < fb_dev->vinfo.yres; i++) {
			for(j=0; j< fb_dev->vinfo.xres; j++) {
				pos=i*fb_dev->finfo.line_length+j*bytes_per_pixel;
				*(uint32_t *)(fb_dev->map_bk+pos)=color;
			}
		}
		#endif
	}
	else
		printf("%s: bits_per_pixel is neither 16 nor 32!\n",__func__);
        //else 	--- NOT SUPPORT --

}


/*-------------------------------------------------------
	Clear map_buff[numpg] with given color
For 16bit color only.
--------------------------------------------------------*/
void fb_clear_mapBuffer(FBDEV *dev, unsigned int numpg, EGI_16BIT_COLOR color)
{
	int i;
        unsigned int pixels; /* Total pixels in the buffer */
	unsigned long buffpos;

        if( dev==NULL || dev->map_bk==NULL || dev->map_buff==NULL )
                return;

        pixels=dev->screensize>>1; /* for 16bit color only */

        numpg=numpg%FBDEV_BUFFER_PAGES; /* Note: Modulo result is compiler depended */

	buffpos= dev->screensize*numpg;
	for(i=0; i<pixels; i++)
			*(uint16_t *)(dev->map_buff + buffpos + (i<<1))=color;
}


/*------------------------------------------------
	Clear FB working buffer
-------------------------------------------------*/
void fb_clear_workBuff(FBDEV *fb_dev, EGI_16BIT_COLOR color)
{
	fb_clear_mapBuffer(fb_dev, FBDEV_WORKING_BUFF, color);
	fb_reset_zbuff(fb_dev);
}


/*------------------------------------------------
	Clear FB background backbuffer
-------------------------------------------------*/
void fb_clear_bkgBuff(FBDEV *fb_dev, EGI_16BIT_COLOR color)
{
	fb_clear_mapBuffer(fb_dev, FBDEV_BKG_BUFF, color);
	fb_reset_zbuff(fb_dev);
}

/*---------------------------------
Reset zbuff values to zero.
---------------------------------*/
void fb_reset_zbuff(FBDEV *fb_dev)
{
	unsigned int xres=fb_dev->vinfo.xres;
	unsigned int yres=fb_dev->vinfo.yres;

	if(fb_dev->zbuff)
		memset( fb_dev->zbuff, 0, xres*yres*sizeof(typeof(*(fb_dev->zbuff))) );
}


/*---------------------------------
Init. all zbuff[] to be z0.
---------------------------------*/
void fb_init_zbuff(FBDEV *fb_dev, int z0)
{
	int i;
	unsigned int xres=fb_dev->vinfo.xres;
	unsigned int yres=fb_dev->vinfo.yres;

	for(i=0; i<xres*yres; i++)
		fb_dev->zbuff[i]=z0;

}


/*----------------------------------------------------
 Refresh FB screen with FB back buffer map_buff[numpg]
-----------------------------------------------------*/
int fb_page_refresh(FBDEV *dev, unsigned int numpg)
{
	if(dev==NULL)
		return -1;

	/* only for ENABLE_BACK_BUFFER */
	if( dev->map_bk==NULL || dev->map_fb==NULL || dev->map_buff==NULL )
		return -2;

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

	return 0;
}


/*------------------------------------------------------------------------------------
 Refresh FB screen lines with FB back buffer map_buff[numpg]
 !!! NOTE: Soft Pos_roation has NO effect for this function. !!!

@startln:	Starting Line index. [0  FBDEV.vinfo.yres-1]
@n: 		Number of lines to be refreshed.

-------------------------------------------------------------------------------------*/
void fb_lines_refresh(FBDEV *dev, unsigned int numpg, unsigned int startln, int n)
{
	unsigned int xres;
	unsigned int Bpp; /* byte per pixel */
	unsigned int Bpl; /* bytes per line */

        if(dev==NULL)
                return;
        if( dev->map_bk==NULL || dev->map_fb==NULL || dev->map_buff==NULL )
                return;
	if( n<=0 || startln > dev->vinfo.yres-1 )
		return;

	if( startln+n > dev->vinfo.yres ) /* startln+n-1 > dev->vinfo.yres-1 */
		n=dev->vinfo.yres-startln;
	if(n<=0)
		return;

	if(numpg>FBDEV_BUFFER_PAGES-1)
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
				memcpy(dev->map_fb+startln*Bpl, dev->map_buff+startln*Bpl, n*Bpl);
				break;
			case 1:
				memcpy(dev->map_fb+startln*Bpl, dev->map_buff+dev->screensize+startln*Bpl, n*Bpl);
				break;
			case 2:
				memcpy(dev->map_fb+startln*Bpl, dev->map_buff+(dev->screensize<<1)+startln*Bpl, n*Bpl);
				break;
			default:
				memcpy(dev->map_fb+startln*Bpl, dev->map_buff+dev->screensize*numpg+startln*Bpl, n*Bpl);
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

	if( dev->map_bk==NULL || dev->map_fb==NULL || dev->map_buff==NULL )
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

		tm_delayms(50);
		//usleep(10000);
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


/*---------------------------------------------
Render image and bring it to screen.
Now it only copys working buffer map_buff[0]
to FB driver.

TODO: works with other FB map_buffers

Return:
	0	OK
	<0	Fails

Midas Zhou
---------------------------------------------*/
int fb_render(FBDEV *dev)
{
	/* OK: checkec in fb_page_refresh() */
        //if( dev->map_bk==NULL || dev->map_fb==NULL || dev->map_buff==NULL )
     	//          return -1;

	/* If directFB mode. OR virtual FBDEV, all NULL. */
	if( dev->map_bk==dev->map_fb )
		return 0;

	/* Anti-aliasing */


	/* Refresh FB mmap data */
	fb_page_refresh(dev, FBDEV_WORKING_BUFF);  /* Input data check inside */

	return 0;
}


/*---------------------------------------------
Render virtual FBDEV, NOW just copy completed
FBDEV.virt_fb to FBDEV.VFrameImg.

Return:
	0	OK
	<0	Fails

Midas Zhou
---------------------------------------------*/
int vfb_render(FBDEV *dev)
{
	int ret;

	if(dev->virt==false) {
		egi_dpstd("It's NOT virtual FBDEV!\n");
		return -1;
	}
	if( dev->VFrameImg==NULL ) {
		egi_dpstd("VFrameImg is NULL!\n");
		return -2;
	}

        /* Copy virt_fb to VFrameImg */
        ret=egi_imgbuf_copyBlock( dev->VFrameImg, dev->virt_fb, false,   /* dest, src, blend_on */
                              dev->virt_fb->width, dev->virt_fb->height, 0, 0, 0, 0);    /* bw, bh, xd, yd, xs,ys */
	if(ret<0) {
		egi_dpstd("Fail to call egi_imgbuf_copyBlock!\n");
		return -3;
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
