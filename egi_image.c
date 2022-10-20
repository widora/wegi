/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

NOTE: Try not to use EGI_PDEBUG() here!

EGI_IMGBUF functions

Jurnal
2021-02-22:
	1. Modify egi_imgbuf_resetColorAlpha().

2021-04-20:
	1. Add param keep_ratio for egi_imgbuf_resize() AND egi_imgbuf_resize_update().

2021-06-05:
	1. Modify egi_imgbuf_resize(): if ineimg and outeimg is same size, then goto CREATE_OUTEIMG.
	2. Add  egi_imgbuf_resize_nolock().
	3. Add  egi_imgbuf_scale(), egi_imgbuf_scale_update().
2021-08-21/22:
	1. Add egi_imgbuf_mapTriWriteFB()
	2. Add egi_imgbuf_mapTriWriteFB2()
2021-08-25:
	1. Add egi_imgbuf_uvToPixel();
2021-08-27:
	XXX 1. egi_imgbuf_mapTriWriteFB2():
	   XXX 1.1 Check/make barycentric factor a/b/r within [0.0 1.0].
	   XXX 1.2 Check/make u/v within [0.0 1.0]
	XXX 2. egi_imgbuf_uvToPixel(): Check/make u/v within [0.0 1.0].
2021-09-08:
	1. Add egi_imgbuf_mapTriWriteFB3().
2021-09-09:
	1. Improve egi_imgbuf_mapTriWriteFB3(): interpolation u/v for line pixel.
	2. Add egi_subimg_serialWriteFB().
2021-09-10:
	1. Add egi_imgmotion_saveHeader()
	2. Add egi_imgmotion_saveFrame()
	3. Add egi_imgmotion_playFile()
2021-09-13
	1. Zlib compress/uncompress IMGMOTION,
	   and update egi_imgmotion_xxxx() functions accordingly.
2021-09-24:
	1. egi_imgbuf_mapTriWriteFB3(): Apply fb_dev->pixalpha.
2021-09-27:
	1. egi_imgbuf_mapTriWriteFB3(): If u/v<0, make it positive u/v=1.0+u/v. ?????
2021-12-06:
	1. Add egi_imgbuf_get_fitsize()
2022-02-10:
	1. egi_imgbuf_mapTriWriteFB3(): with input params of float z0,z1,z2 to compute pixZ
	   for each pixel.
2022-03-22:
	1. egi_imgbuf_readfile(): Call egi_simpleCheck_jpgfile() to identify JPEG or PNG.
2022-04-05:
	1. egi_imgbuf_resize_nolock(): Create a transition imgbuf to improve scaledown precision.
2022-04-06:
	1. Add egi_imgbuf_rotBlockCopy2(): float type angle and 4p interpolation.
2022-04-08:
	1. Add egi_imgbuf_createLinkFBDEV() and egi_imgbuf_freeLinkFBDEV().
2022-05-05:
	1. Add egi_imgbuf_getColor().
2022-05-08:
	1. Add egi_imgbuf_blockResetColorAlpha().

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
-------------------------------------------------------------------*/
#include <pthread.h>
#include <math.h>
#include <sys/time.h>
#include "egi_image.h"
#include "egi_bjp.h"
#include "egi_utils.h"
#include "egi_log.h"
#include "egi_math.h"
#include "egi_log.h"
#include "egi_timer.h"
#include "egi_debug.h"
#include "egi_matrix.h"

typedef struct fbdev FBDEV; /* Just a declaration, referring to definition in egi_fbdev.h */

/*--------------------------------------------
   	   Allocate an EGI_IMGBUF
Allocate an EGI_IMGBUF and init img_mutex.
---------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_alloc(void) //new(void)
{
	EGI_IMGBUF *eimg;
	eimg=calloc(1, sizeof(EGI_IMGBUF));
	if(eimg==NULL) {
		printf("%s: Fail to calloc EGI_IMGBUF.\n",__func__);
		return NULL;
	}

        /* init imgbuf mutex */
        if(pthread_mutex_init(&eimg->img_mutex,NULL) != 0)
        {
                printf("%s: fail to initiate img_mutex.\n",__func__);
		free(eimg);
                return NULL;
        }

	return eimg;
}


/*-----------------------------------------
Allocate an array of EGI_BOX

@n: Size of EGI_BOX array.

Return:
	Pointer to an array of EGI_BOX   OK
	NULL	Fails
-----------------------------------------*/
EGI_IMGBOX *egi_imgboxes_alloc(int n)
{
	EGI_IMGBOX* boxes;

	if(n<=0)
		return NULL;

	boxes=calloc(n, sizeof(EGI_IMGBOX));

	return boxes;
}
/*-----------------------------------------
Free an array of EGI_BOX

@n: Size of EGI_BOX array.
-----------------------------------------*/
void egi_imgboxes_free(EGI_IMGBOX *imboxes)
{
	free(imboxes);
}


/*------------------------------------------------------------
Free data in EGI_IMGBUF, but NOT itself!

NOTE:
  1. WARNING!!!: No mutex operation here, the caller shall take
     care of imgbuf mutex lock.

-------------------------------------------------------------*/
void egi_imgbuf_cleardata(EGI_IMGBUF *egi_imgbuf)
{
	if(egi_imgbuf != NULL) {
	        if(egi_imgbuf->imgbuf != NULL) {
        	        free(egi_imgbuf->imgbuf);
                	egi_imgbuf->imgbuf=NULL;
        	}
     	   	if(egi_imgbuf->alpha != NULL) {
        	        free(egi_imgbuf->alpha);
                	egi_imgbuf->alpha=NULL;
        	}
       		if(egi_imgbuf->data != NULL) {
               	 	free(egi_imgbuf->data);
                	egi_imgbuf->data=NULL;
        	}
		if(egi_imgbuf->subimgs != NULL) {
			free(egi_imgbuf->subimgs);
			egi_imgbuf->subimgs=NULL;
		}
		if(egi_imgbuf->pcolors !=NULL) {
	        	egi_free_buff2D((unsigned char **)egi_imgbuf->pcolors, egi_imgbuf->height);
			egi_imgbuf->pcolors=NULL;
		}
		if(egi_imgbuf->palphas !=NULL) {
		        egi_free_buff2D(egi_imgbuf->palphas, egi_imgbuf->height);
			egi_imgbuf->palphas=NULL;
		}

		/* reset size and submax */
		egi_imgbuf->height=0;
		egi_imgbuf->width=0;
		egi_imgbuf->submax=0;
	}

	/* Only clear data !!!!DO NOT free egi_imgbuf itself ; */
}

/*------------------------------------------
Free EGI_IMGBUF and its data
The caller MUST reset egi_imgbuf to NULL!!!
-------------------------------------------*/
void egi_imgbuf_free(EGI_IMGBUF *egi_imgbuf)
{
        if(egi_imgbuf == NULL)
                return;

	/* Hope there is no other user */
	if(pthread_mutex_lock(&egi_imgbuf->img_mutex) !=0 )
		EGI_PLOG(LOGLV_TEST,"%s:Fail to lock img_mutex!\n",__func__);

	/* !MOVE TO egi_imgbuf_cleardata(), free 2D array data if any */
        //egi_free_buff2D((unsigned char **)egi_imgbuf->pcolors, egi_imgbuf->height);
        //egi_free_buff2D(egi_imgbuf->palphas, egi_imgbuf->height);

	/* free data inside */
	egi_imgbuf_cleardata(egi_imgbuf);

#if 1	/*  ??????? necesssary ????? */
	/*  "It shall be safe to destroy an initialized mutex that is unlocked. Attempting to
	 *   destroy a locked mutex results in undefined behavior"  --- POSIX man/Linux man 3
	 */
	if( pthread_mutex_unlock(&egi_imgbuf->img_mutex) != 0)
		EGI_PLOG(LOGLV_TEST,"%s:Fail to unlock img_mutex!\n",__func__);
#endif

        /* Destroy thread mutex lock for page resource access */
        if(pthread_mutex_destroy(&egi_imgbuf->img_mutex) !=0 )
		EGI_PLOG(LOGLV_TEST,"%s:Fail to destroy img_mutex!. Err'%s'. \n",__func__, strerror(errno));
					/*  Err'No message of desired type'
					   Fail to destroy img_mutex!. Err'File exists'.??? */
	/* NOTE: It seems a previous ERROR/errno will affect pthread_mutex_destroy()!? */

	free(egi_imgbuf);

//	egi_imgbuf=NULL; /* ineffective though...*/
}

/*------------------------------------------
	Free EGI_IMGBUF and itsef
-------------------------------------------*/
void egi_imgbuf_free2(EGI_IMGBUF **pimg)
{
	if(pimg==NULL || *pimg==NULL)
		return;

	egi_imgbuf_free(*pimg);

	*pimg=NULL;
}


/*-----------------------------------------------------------------------------
Initiate/alloc imgbuf as an image canvas, with all alpha=0!!!
If egi_imgbuf has old data, then it will be cleared/freed first.

NOTE:
  1. !!!!WARNING!!!: No mutex operation here, the caller shall take
     care of imgbuf mutex lock.

@height		height of image
@width		width of image
@AlphaON	If ture, egi_imgbuf->alpha will be allocated.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------*/
int egi_imgbuf_init(EGI_IMGBUF *egi_imgbuf, int height, int width, bool AlphaON)
{
	if(egi_imgbuf==NULL)
		return -1;

	if(height<=0 || width<=0)
		return -2;

	/* Empty old data if any */
	egi_imgbuf_cleardata(egi_imgbuf);

        /* Calloc imgbuf->imgbuf */
        egi_imgbuf->imgbuf = calloc(1,height*width*sizeof(EGI_16BIT_COLOR));
        if(egi_imgbuf->imgbuf == NULL) {
                egi_dperr("Fail to calloc egi_imgbuf->imgbuf.");
		return -2;
        }

        /* Calloc imgbuf->alpha, alpha=0, 100% canvas color. */
	if(AlphaON) {
	        egi_imgbuf->alpha= calloc(1, height*width*sizeof(unsigned char)); /* alpha value 8bpp */
        	if(egi_imgbuf->alpha == NULL) {
	                egi_dperr("Fail to calloc egi_imgbuf->alpha.");
			free(egi_imgbuf->imgbuf);
                	return -3;
        	}
	}

        /* Retset height and width for imgbuf */
        egi_imgbuf->height=height;
        egi_imgbuf->width=width;

	return 0;
}


/*-----------------------------------------------
To get color/alpha value for the specified point.

@imgbuf:	An pointer to EGI_IMGBUF
@px,py:	        Point in the imgbuf.
@color,alpha    Pointers to pass out values.

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
int egi_imgbuf_getColor(EGI_IMGBUF *imgbuf, int px, int py, EGI_16BIT_COLOR *color, EGI_8BIT_ALPHA *alpha)
{
	if(imgbuf==NULL || imgbuf->imgbuf==NULL)
		return -1;
	if(px<0 || px>imgbuf->width-1 || py<0 || py>imgbuf->height-1)
		return -1;

	if(color)
		*color=imgbuf->imgbuf[py*imgbuf->width+px];
	if(alpha && imgbuf->alpha)
		*alpha=imgbuf->alpha[py*imgbuf->width+px];

	return 0;
}


/*--------------------------------------------------------------------------------
Modify color/alpha data of an EGI_IMGBUF to set an bounding rectangle(limit box)
with given color and width.

@ineimg:	pointer to an EGI_IMGBUF.
@color:		Color value for boundary lines.
@lw:		width of boundary lines, in pixels.

Return:
	0	OK
	<0	Fails
--------------------------------------------------------------------------------*/
int egi_imgbuf_addBoundaryBox(EGI_IMGBUF *ineimg, EGI_16BIT_COLOR color, int lw)
{
	int i,j;

	if(ineimg==NULL || ineimg->imgbuf==NULL)
		return -1;

	/* limit wl */
	if(lw <= 0 )
		lw=1;
	if(lw > ineimg->width/2 )
		lw=ineimg->width/2;
	if(lw > ineimg->height/2 )
		lw=ineimg->height/2;

	/* set upper boundary */
	for(i=0; i < lw*(ineimg->width); i++) {
		ineimg->imgbuf[i]=color;
		if(ineimg->alpha)
			ineimg->alpha[i]=255;
	}
	/* set bottom boundary */
	for( i=(ineimg->height-lw)*(ineimg->width); i < (ineimg->height)*(ineimg->width); i++ ) {
		ineimg->imgbuf[i]=color;
		if(ineimg->alpha)
                        ineimg->alpha[i]=255;
	}
	/* set left and right boundary */
	for(i=0; i < ineimg->height; i++) {
		for(j=0; j<lw; j++) {
			ineimg->imgbuf[i*(ineimg->width)+j]=color;  		/* left */
			ineimg->imgbuf[(i+1)*(ineimg->width)-1-j]=color;  	/* right */
			if(ineimg->alpha) {
				ineimg->alpha[i*(ineimg->width)+j]=255; 	/* left */
				ineimg->alpha[(i+1)*(ineimg->width)-1-j]=255; 	/* right */
			}
		}
	}

	return 0;
}



/* ------------------------------------------------------
Create an EGI_IMGBUF, set color and alpha value.

@height,width:	height and width of the imgbuf.
@alpha:		>0, alpha value for all pixels.
		(else, default 0)
@color:		>0 basic color of the imgbuf.
		(else, default 0)

Return:
	A pointer to EGI_IMGBUF		OK
			   FULL		Fails
-------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_create( int height, int width,
				unsigned char alpha, EGI_16BIT_COLOR color )
{
	int i;

	EGI_IMGBUF *imgbuf=egi_imgbuf_alloc();
	if(imgbuf==NULL)
		return NULL;

	/* init the struct, initial color and alpha all to be 0! */
	if ( egi_imgbuf_init(imgbuf, height, width, true) !=0 )
		return NULL;

	/* set alpha and color */
	if(alpha>0)     /* alpha default calloced as 0 already */
		memset( imgbuf->alpha, alpha, height*width );
	if(color>0) {   /* color default calloced as 0 already */
		for(i=0; i< height*width; i++)
			*(imgbuf->imgbuf+i)=color;
	}

	return imgbuf;
}


/* -----------------------------------------------------------
Create an EGI_IMGBUF without alpha space.

@height,width:	height and width of the imgbuf.
@color:		>0 basic color of the imgbuf.
		(else, default 0)

Return:
	A pointer to EGI_IMGBUF		OK
			   FULL		Fails
---------------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_createWithoutAlpha(int height, int width, EGI_16BIT_COLOR color)
{
	int i;

	EGI_IMGBUF *imgbuf=egi_imgbuf_alloc();
	if(imgbuf==NULL)
		return NULL;

	/* init the struct, initial color and alpha all to be 0! */
	if ( egi_imgbuf_init(imgbuf, height, width, false) !=0 )
		return NULL;

	/* set alpha and color */
	if(color>0) {   /* color default calloced as 0 already */
		for(i=0; i< height*width; i++)
			*(imgbuf->imgbuf+i)=color;
	}

	return imgbuf;
}


/*--------------------------------------------------------
Allocate eimg->subimgs and set eimg->submax. and it will
NOT check eimg->imgbuf.
All eimg->subimgs[] shall be assigned properly after this.

@eimg:	Pointer to an EGI_IMGBUF
@num:	Total number of subimgs

Return:
	0	OK
	<0	Fail
-------------------------------------------------------*/
int egi_imgbuf_setSubImgs(EGI_IMGBUF *eimg, int num)
{
	if(eimg==NULL)
		return -1;

	/* Allocate sumbimgs */
	eimg->subimgs=egi_imgboxes_alloc(num);
	if(eimg->subimgs==NULL)
		return -2;

	/* Assign submax */
	eimg->submax=num-1;

	return 0;
}


/*-------------------------------------------------------
Create an EGI_IMGBUF whose imgbuf refers/links to FBDEV buffer.
Call egi_imgbuf_freeLinkFBDEV() to free it!

Note:
1. Only a pointer reference to FBDEV image data! NO synch mechanism!

@fbdev: An pointer to FBDEV

Return:
	!NULL	OK
	NULL	Fails
-------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_createLinkFBDEV(const FBDEV *fbdev)
{
        EGI_IMGBUF *fbimg=egi_imgbuf_alloc();
	if(fbimg==NULL)
		return NULL;

        fbimg->width=fbdev->pos_xres;
        fbimg->height=fbdev->pos_yres;

	/* Link imgbuf data */
        fbimg->imgbuf=(EGI_16BIT_COLOR*)fbdev->map_fb;

	return fbimg;
}
/*------------------------------------------------------
Free an imgbuf whose imgbuf refers/links to FBDEV buffer.
MUST NOT to free linked data memory!
------------------------------------------------------*/
void egi_imgbuf_freeLinkFBDEV(EGI_IMGBUF **imgbuf)
{
	if(imgbuf==NULL || *imgbuf==NULL)
		return;

	/* Hope there is no other user */
	if(pthread_mutex_lock(&(*imgbuf)->img_mutex) !=0 ) {
		EGI_PLOG(LOGLV_TEST,"%s:Fail to lock img_mutex!\n",__func__);
		return;
	}

        /* Destroy thread mutex lock */
        if(pthread_mutex_destroy(&(*imgbuf)->img_mutex) !=0 ) {
                EGI_PLOG(LOGLV_TEST,"%s:Fail to destroy img_mutex!. Err'%s'. \n",__func__, strerror(errno));
		return;
	}

	free(*imgbuf);
	*imgbuf=NULL;
}


/*--------------------------------------------------------------
Read an image file and load data to an EGI_IMGBUF as for return.

@fpath:	Full path to an image file.
	Supports only JPG and PNG currently.

Return:
	A pointer to EGI_IMGBUF		Ok
	NULL				Fails
----------------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_readfile(const char* fpath)
{
	//if(fpath==NULL)  /* check fpath in egi_imgbuf_loadjpg() */
	//	return NULL;

	EGI_IMGBUF *eimg=egi_imgbuf_alloc();
	if(eimg==NULL)
		return NULL;
#if 0
        if( egi_imgbuf_loadjpg(fpath, eimg)!=0 ) {
                if ( egi_imgbuf_loadpng(fpath, eimg)!=0 ) {
			egi_imgbuf_free2(&eimg);
			return NULL;
                }
        }
#else

	/* PNG File */
	if(egi_simpleCheck_jpgfile(fpath)==JPEGFILE_INVALID) {
	    if ( egi_imgbuf_loadpng(fpath, eimg)!=0 ) {
                     egi_imgbuf_free2(&eimg);
                     return NULL;
            }
	}
	/* JPEG File */
	else  {
	    if ( egi_imgbuf_loadjpg(fpath, eimg)!=0 ) {
                     egi_imgbuf_free2(&eimg);
                     return NULL;
            }
	}
#endif

	return eimg;
}

/*----------------------------------------------------------------
Copy a block of image from input EGI_IMGBUF, and create
a new EGI_IMGBUF to hold the data.
Only color/alpha of ineimg will be copied to outeimg, other members
such as subimg will be ignored.

@ineimg:  The original image from which we'll copy an image block.
@px,py:   Coordinates of the origin for the wanted image block,
	  relative to ineimg image coords.
@height:  Height of the image block
@width:   Width of the image block

Return:
	A pointer to EGI_IMGBUF		Ok
	NULL				Fails
-----------------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_blockCopy( const EGI_IMGBUF *ineimg,
				  int px, int py, int height, int width )
{
	int i,j;
	unsigned int indx,outdx;
	EGI_IMGBUF *outeimg=NULL;
	bool alpha_on;

	if( ineimg==NULL || ineimg->imgbuf==NULL )
		return NULL;

	/* create a new imgbuf, h/w check inside. */
	outeimg=egi_imgbuf_create( height, width, 255, 0); /* default alpha 255 */
	if(outeimg==NULL) {
		printf("%s: Fail to create outeimg!\n",__func__);
		return NULL;
	}

	/* alpha  ON/OFF */
	if( ineimg->alpha != NULL )
		alpha_on=true;
	else {
		alpha_on=false;
		/* free it */
		free(outeimg->alpha);
		outeimg->alpha=NULL;
	}

	/* Copy color/alpha data */
	for(i=0; i<height; i++) {
		for(j=0; j<width; j++) {
			/* Range check */
			if(  py+i < 0 || px+j < 0 ||
			     py+i >= ineimg->height || px+j >= ineimg->width  ) {
				  continue;
			}
			outdx=i*width+j;	  /* data index for outeimg*/
			indx=(py+i)*ineimg->width+(px+j); /* data index for ineimg */
			/* copy data */
			outeimg->imgbuf[outdx]=ineimg->imgbuf[indx];
			if(alpha_on) {
				outeimg->alpha[outdx]=ineimg->alpha[indx];
			}
		}
	}

	return outeimg;
}


/*---------------------------------------------------------------------
Copy a block from srcimg and paste it to destimg.

Note:
1. If destimg has alpha values,then copy it also. while if original
   destimg dose not has alpha space, then allocate it first.

			!!! WARNING !!!
        It MAY allocate memory space for destimg->alpha!

2. The destination image and srouce image may be the same.

@destimg:  	The destination EGI_IMGBUF.
@srcimg:  	The source EGI_IMGBUF.
@blendON:	True: Applicable only when srcimg has alpha values.
		      Pixels of two images will be blended together.
		      The copied pixels will be merged to destimg by callying
		      egi_16bitColor_blend(), where only destimg->alpha
		      takes effect, and alpha value of srcimg (if it has)
		      will not be changed.
		False: Copied pixels will replace old pixels, with colors
		      and alphas (if srcimg has alphas.)

@xd, yd:	Block left top point coord relative to destimg.
@xs, ys:	Block left top point coord relative to srcimg.
@bw:   		Width of the copying block
@bh:	  	Height of the copying block

Return:
        0         Ok
        <0        Fails
-----------------------------------------------------------------------*/
int  egi_imgbuf_copyBlock( EGI_IMGBUF *destimg, const EGI_IMGBUF *srcimg, bool blendON,
			   int bw, int bh,
                           int xd, int yd, int xs, int ys )
{
	int i,j;
	int destimg_size; 	/* image size, in pixels */
	//int srcimg_size;
	int pos_dest;		/* offset position  */
	int pos_src;

#if 0	/* TEST: ----- */
	egi_dpstd("destimg(W%dxH%d), srcimg(W%dxH%d), bw*bh(%dx%d) (xd,yd)(%d,%d), (xs,ys)(%d,%d) \n",
			destimg->width, destimg->height, srcimg->width, srcimg->height, bw, bh, xd,yd, xs,ys);
#endif

	/* Check input */
        if( destimg==NULL || destimg->imgbuf==NULL || srcimg==NULL || srcimg->imgbuf==NULL )
                return -1;

	/* check block size */
	if( bw <=0 || bh <=0 )
		return -1;

	/* Assume width/heigth of EGI_IMGBUF are sane! */

	/* Check (xd,yd) and  (xs,ys), rule out situations that block does NOT cover any part of the image. */
	if( xd+bw-1<0 || yd+bh-1<0 || xd > destimg->width-1 || yd > destimg->height-1 ) {
		printf("%s: Block covers no part of destimg! please check positon xd,yd. \n",__func__);
		return -2;
	}
	if( xs+bw-1<0 || ys+bh-1<0 || xs > srcimg->width-1 || ys > srcimg->height-1 ) {
		printf("%s: Block covers no part of srcimg! please check postion xs,ys. \n",__func__);
		return -2;
	}

	/* Get image size, in pixels */
	//srcimg_size=srcimg->height*srcimg->width;
	destimg_size=destimg->height*destimg->width;

#if 1	/* If srcimg has alpha values while destimg dose not, then allocate and memset with 255 */
	if( srcimg->alpha != NULL && destimg->alpha == NULL ) {
		egi_dpstd("Allocate alpha for destimg!\n");
		destimg->alpha=calloc(1, destimg_size*sizeof(EGI_8BIT_ALPHA));
		if(destimg->alpha==NULL) {
			printf("%s: Fail to calloc destimg->alpha!\n",__func__);
			return -3;
		}
		memset(destimg->alpha, 255, destimg_size*sizeof(EGI_8BIT_ALPHA));
	}
#endif

	/* Copy pixels */
	for(i=0; i<bh; i++) {
		//printf("i=%d dw=%d,dh=%d\n",i, destimg->width, destimg->height);
		for(j=0; j<bw; j++) {
			/* If pixel coord. out of srcimg */
			if( xs+j < 0 || ys+i <0 )
				continue;
			else if( xs+j > srcimg->width-1 || ys+i > srcimg->height-1 )
				continue;

			/* If pixel coord. out of destimg */
			if( xd+j < 0 || yd+i <0 )
				continue;
			else if( xd+j >destimg->width-1 || yd+i > destimg->height-1 )
				continue;

			//printf("i=%d, j=%d\n",i,j);

			/* Get offset position of data */
			pos_src=(ys+i)*srcimg->width+xs +j;
			pos_dest=(yd+i)*destimg->width+xd +j;

			/* copy color and alpha data */
			if( blendON && srcimg->alpha && destimg->alpha ) { /* If need to blend together */
				destimg->imgbuf[pos_dest]=egi_16bitColor_blend( srcimg->imgbuf[pos_src],
										destimg->imgbuf[pos_dest], srcimg->alpha[pos_src]);
				/* Alpha values of destimg NOT changes */
			}
			else {
				destimg->imgbuf[pos_dest]=srcimg->imgbuf[pos_src];
				if(srcimg->alpha)
					destimg->alpha[pos_dest]=srcimg->alpha[pos_src];
			}

		}
	}

	return 0;
}


/*-------------------------------------------------------------------
Copy a subimage from input EGI_IMGBUF, and create
a new EGI_IMGBUF to hold the data.

@eimg:		An EGI_IMGBUF with subimges inside.
@index:		Index of subimage, as of eimg->subimg[x]

Return:
	A pointer to EGI_IMGBUF		Ok
	NULL				Fails
--------------------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_subImgCopy( const EGI_IMGBUF *eimg, int index )
{
        /* Check input data */
        if( eimg==NULL || eimg->subimgs==NULL || index<0 || index > eimg->submax ) {
                printf("%s:Input eimg or subimage index is invalid!\n",__func__);
                return NULL;
        }

	return egi_imgbuf_blockCopy( eimg, eimg->subimgs[index].x0, eimg->subimgs[index].y0,
					   eimg->subimgs[index].h,  eimg->subimgs[index].w    );
}


/*-----------------------------------------------------------
Set/create a frame for an EGI_IMGBUF.
The frame are formed by different patterns of alpha values.

Note:
1. Usually the frame is to be used as cutout_shape of an image.
2. If input eimg has alpha value:
   2.1 If input alpha >=0, then modify eimg->alpha all to alpah.
   2.2 If input alpha <0, no change for eimg->alpha.
3. If input eimg has no alpha value, then allocate it and:
   3.1 If input alpha >=0, modify eimg->alpha all to be alpha
   2.2 If input alpha <0, modify eimg->alpha all to be 255.

@eimg:		An EGI_IMGBUF holding pixel colors.
@type:          type/shape of the frame.
@alpha:		>=0: Rset all eimg->alpha to be alpha before cutting frame.
		<0:  If input eimg has alpha value. ignore/no change.
		     If input eimg has no alpha value, allocate
		     and initiate with 255 before cutting frame.
@pn:            numbers of paramters
@param:         array of params

Return:
        0      OK
        <0     Fail
---------------------------------------------------------*/
int egi_imgbuf_setFrame( EGI_IMGBUF *eimg, enum imgframe_type type,
			 int alpha, int pn, const int *param )
{
	int i,j;

	if( pn<1 || param==NULL ) {
		printf("%s: Input param invalid!\n",__func__);
		return -1;
	}
	if( eimg==NULL || eimg->imgbuf==NULL ) {
		printf("%s: Invali input eimg!\n",__func__);
		return -1;
	}

	int height=eimg->height;
	int width=eimg->width;

	/* allocate alpha if NULL, and set alpha value */
	if( eimg->alpha == NULL ) {
		eimg->alpha=calloc(1, height*width*sizeof(unsigned char));
		if(eimg->alpha==NULL) {
			printf("%s: Fail to calloc eimg->alpha!\n",__func__);
			return -2;
		}
		/* set alpha value */
		if( alpha<0 )  /* all set to 255 */
			memset( eimg->alpha, 255, height*width );
		else
			memset( eimg->alpha, alpha, height*width );
	}
	else {  /* reset alpha, if input alpha >=0  */
		if ( alpha >=0 )
			memset( eimg->alpha, alpha, height*width );
	}

	/*  --- edit alpha value to create a frame --- */
	/* 1. Rectangle with 4 round corners */
	if(type==frame_round_rect)
	{
		int rad=param[0];
		int ir;

		/* adjust radius */
		if( rad < 0 )rad=0;
		if( rad > height/2 ) rad=height/2;
		if( rad > width/2 ) rad=width/2;

		/* cut out 4 round corners */
		for(i=0; i<rad; i++) {
		        ir=rad-round(sqrt(rad*rad*1.0-(rad-i)*(rad-i)*1.0));
			for(j=0; j<ir; j++) {
				/* upp. left corner */
				*(eimg->alpha+i*width+j)=0; /* set alpha 0 at corner */
				/* upp. right corner */
				*(eimg->alpha+((i+1)*width-1)-j)=0;
				/* down. left corner */
				*(eimg->alpha+(height-1-i)*width+j)=0;
				/* down. right corner */
				*(eimg->alpha+(height-i)*width-1-j)=0;
			}
		}
	}
	/* TODO: other types */
	else {
		/* Original rectangle shape  */
	}

	return 0;
}

/*---------------------------------------------------------------------------------
Get alpha value on a certain X-Alpha mapping curve.

Note:
1. Somthness of result alpha values depends not only on fading
   'type'(algorithm), but also on 'range' value, the bigger the better--xxxNOPE!!!
2. In respect of transition smothness, linear algorithm is better
   than nonlinear one?  Abrupt change of alpha values will cause
   some crease lines in final image.

3. !!! WARING !!! Be careful of calculation overflow. Change to float type
   if necessary.

@max_alpha:  Max alpha value as transition position [0 range] maps to [0 max_alpha]

@range:  The range of image transition band width, corresponding to
	 alpha value range of [0 max_alpha].   (in pixels)
	 when x=range, alpha to be max_alpha.

@type:   Mapping curve type, see in function codes.
	 Recommand type 0, type 2, and type 4.

@x:	 Input X, distance from image side, in pixels.
	 The range of input X, [0 range]  (in pixels)

Return:  Mapped alpha value. [0 max_alpha] <-- [0 range-1]
	 or 255 if no match.
------------------------------------------------------------------------------------*/
unsigned char get_alpha_mapCurve( EGI_8BIT_ALPHA max_alpha, int range, int type, int x)
{
	unsigned char alpha; /* mapped alpha value */
	long tx;

	/* check input range */
	if(range<=0) return max_alpha;

	/* check input x */
	if(x<0)
		x=0;
	else if(x>range)
		x=range;

	switch(type)
	{
		/* 0. Linear curve: Linear transition */
		case 0:
			alpha=max_alpha*x/range;
			break;

		/* 1. Cubic curve: GOOD_Smooth transition at start, Fast transition at end
				   but rather bigger invisible edge areas.
		 */
		case 1:
			#if 1
			alpha=(long long)max_alpha*x*x*x/((long long)range*range*range);
			#else
			alpha=1.0*max_alpha*x*x*x/(1.0*range*range*range);
			#endif
			break;

		/* 2. Reversed cubic curve: GOOD_Smooth transition at end, Fast transition at start
					    and rather smaller invisible edge areas.
		*/
		case 2:
			tx=range-x;
			#if 1
			alpha=max_alpha-(long long)max_alpha*tx*tx*tx/((long long)range*range*range);  /* abrupt at end */
			#else
			alpha=max_alpha-1.0*max_alpha*tx*tx*tx/(1.0*range*range*range);  /* abrupt at end */
			#endif
			break;

		/* 3. Circle point curve:  Faster transition at end, to emphasize end border line.
		  			   but rather bigger invisible edge areas.
		 */
		case 3:
			alpha=( 1.0- sqrt(1.0-(1.0*x/range)*(1.0*x/range)) )*max_alpha;
			break;

		/* 4. (1-COS(x))/2  curve: GOOD_Smooth transition at both start and end. */
		case 4:
			alpha=(1.0-cos(1.0*x*MATH_PI/range))*max_alpha/2;
			break;

		default:
			alpha=max_alpha;
	}


	return alpha;
}

/*--------------------------------------------------------------------
Modify alpha values for an EGI_IMGBUF, in order to make the 4 edges
of the image fade out (hide) smoothly into its surrounding.

@eimg: 	     Pointer to an EGI_IMGBUF

@max_alpha:  Max alpha value for get_alpha_mapCurve()
	     transition position [0 width-1] maps to [0 max_alpha]
	     Usually to be 255. or same as original eimg->alpha.

@width: Width of the gradient transition belt.
	    Adjust to [1  imgbuf.height] or [1 imgbuf.width]

@ssmode:  --- Side select mode ---
	FADEOUT_EDGE_TOP 	0b0001  effective for Top side only.
	FADEOUT_EDGE_RIGHT 	0b0010  effective for Right side only.
	FADEOUT_DEGE_BOTTOM	0b0100  effective for Bottom side only.
	FADEOUT_DEGE_LEFT	0b1000  effective for Left side only.
	OR use operator '|' to combine them.

@type:  Type of fading transition as defined in get_alpha_mapCurve().

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------------*/
int egi_imgbuf_fadeOutEdges(EGI_IMGBUF *eimg, EGI_8BIT_ALPHA max_alpha, int width, unsigned int ssmode, int type)
{
	int i,j;
	int wb;  	 /* effective width of transition belt, may be different from width.  */
	unsigned char alpha;

	if( eimg==NULL || eimg->imgbuf==NULL )
			return -1;

	if( ssmode==0 || width<=0 )
		return 1;

	/* Create alpha data if NULL */
	if(eimg->alpha==NULL) {
                eimg->alpha=calloc(1, eimg->height*eimg->width*sizeof(unsigned char)); /* alpha value 8bpp */
                if(eimg->alpha == NULL) {
                        printf("%s: fail to calloc eimg->alpha.\n",__func__);
                        return -2;
		}
		memset(eimg->alpha,255,eimg->height*eimg->width); /* init. value 255 */
	}
	/* Set top side */
	if( ssmode & FADEOUT_EDGE_TOP ) {
		if( width > eimg->height )
			wb=eimg->height;
		else
			wb=width;

		for(i=0; i<wb; i++) {
			alpha=get_alpha_mapCurve(max_alpha, wb, type, i);
			for(j=0; j < eimg->width; j++) {
				if( alpha < eimg->alpha[i*eimg->width+j] )
					eimg->alpha[i*eimg->width+j]=alpha;
			}
		}
	}
	/* Set right side */
	if( ssmode & FADEOUT_EDGE_RIGHT ) {
		if(width > eimg->width)
			wb=eimg->width;
		else
			wb=width;

		for(i=0; i<wb; i++) {
			alpha=get_alpha_mapCurve(max_alpha, wb, type, i);
			for(j=0; j < eimg->height; j++) {
				if( alpha < eimg->alpha[ (j+1)*eimg->width-1 -i ] )
					eimg->alpha[ (j+1)*eimg->width-1 -i ]=alpha;
			}
		}
	}
	/* Set bottom side */
	if( ssmode & FADEOUT_EDGE_BOTTOM ) {
		if(width>eimg->height)
			wb=eimg->height;
		else
			wb=width;

		for(i=0; i<wb; i++) {
			alpha=get_alpha_mapCurve(max_alpha, wb, type, i);
			for(j=0; j < eimg->width; j++) {
				if( alpha < eimg->alpha[ (eimg->height-i)*eimg->width-1 -j ] )
					eimg->alpha[ (eimg->height-i)*eimg->width-1 -j ]=alpha;
			}
		}
	}
	/* Set left side */
	if( ssmode & FADEOUT_EDGE_LEFT ) {
		if(width>eimg->width)
			wb=eimg->width;
		else
			wb=width;

		for(i=0; i<wb; i++) {
			alpha=get_alpha_mapCurve(max_alpha, wb, type, i);
			for(j=0; j < eimg->height; j++) {
				if( alpha < eimg->alpha[ j*eimg->width +i ] )
					eimg->alpha[ j*eimg->width +i ]=alpha;
			}
		}
	}

	return 0;
}


/*--------------------------------------------------------------------------
Modify alpha values for an EGI_IMGBUF, in order to make the
image fade out (hide) smoothly around a circle.

@eimg: 	     Pointer to an EGI_IMGBUF
@rad:	     Radius of the innermost circle.
@max_alpha:  Max alpha value for get_alpha_mapCurve()
	     transition position [0 width-1] maps to [0 max_alpha]
	     Usually to be 255. or same as original eimg->alpha.

@width: Width of the gradient transition belt. Direction from innermost circle
	to outer circle where r=ri+width.  ri+width may be greater than ro.
	Adjust to [1  outmost_radius], and set alpha=0 where r>ri+width

@type:  Type of fading transition as defined in get_alpha_mapCurve().

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------------------*/
int egi_imgbuf_fadeOutCircle(EGI_IMGBUF *eimg, EGI_8BIT_ALPHA max_alpha, int rad, int width, int type)
{

	int x0,y0;	 /* center of the circle under coord. of eimg->imgbuf */
	int ro;		 /* outmost radius */
	int ri;		 /* innermost radius */
	int r;
	int rdev;	 /* rdev=ri+width-ro */
	int py;		 /* 1/4 arch Y, X */
	float fpy;
	int px;
	unsigned long pos; /* alpha position/offset */
	unsigned char alpha;

	if( eimg==NULL || eimg->imgbuf==NULL )
		return -1;

//	if( width<=0 )
//		return 1;
	if( width<0 ) width=0;

	/* Check rad */
	if(rad<0)rad=0;

	/* Create alpha data if NULL */
	if(eimg->alpha==NULL) {
                eimg->alpha=calloc(1, eimg->height*eimg->width*sizeof(unsigned char)); /* alpha value 8bpp */
                if(eimg->alpha == NULL) {
                        printf("%s: fail to calloc eimg->alpha.\n",__func__);
                        return -2;
		}
		memset(eimg->alpha,255,eimg->height*eimg->width); /* init. value 255 */
	}

	/* Circle center */
	x0=eimg->width/2;   /* here Not width/2 -1  */
	y0=eimg->height/2;

	/* Innermost radius: ri to be Min. of width/2, height/w, and rad */
	ri=rad;
	if(ri>x0)ri=x0;
	if(ri>y0)ri=y0;

	/* Outermost radius: ro to be (Max. of 4 tips) distance from (x0,y0) to (0,0) */
	//ro=round(sqrt(1.0*x0*x0+1.0*y0*y0));
	ro=round(sqrt(x0*x0+y0*y0));

	/* Transition band width >0 */
	rdev=ri+width-ro;  /* If width+ri>ro, rdev>0, else rdev<0 */

        for(r=ro; r>=ri; r--)  /* or o.25, there maybe 1 pixel deviation */
        {
		/* Get alpha value for current radius */
		if(r>ri+width)
			alpha=0;	/* outer area is hidden */
		else {
			alpha=get_alpha_mapCurve(max_alpha, width, type, rdev+(ro-r));
		}

		/* For each circle */
		for(fpy=0; fpy<r; fpy+=0.5) { /* or o.25, there maybe 1 pixel deviation */
	                px=round(sqrt(r*r-fpy*fpy));
			//py=round(fpy);
			py=fpy;
			if( y0+py>=0 && y0+py < eimg->height && x0-px>=0 && x0-px < eimg->width  ) {
				pos=eimg->width*(y0+py)+(x0-px);
				eimg->alpha[pos]=alpha;
			}
			if( y0+py>=0 && y0+py < eimg->height && x0+px>=0 && x0+px < eimg->width  ) {
				pos=eimg->width*(y0+py)+(x0+px);
				eimg->alpha[pos]=alpha;
			}
			if( y0-py>=0 && y0-py < eimg->height && x0-px>=0 && x0-px < eimg->width  ) {
				pos=eimg->width*(y0-py)+(x0-px);
				eimg->alpha[pos]=alpha;
			}
			if( y0-py>=0 && y0-py < eimg->height && x0+px>=0 && x0+px < eimg->width  ) {
				pos=eimg->width*(y0-py)+(x0+px);
				eimg->alpha[pos]=alpha;
			}
			/* Flip X Y */
			if( y0+px>=0 && y0+px < eimg->height && x0-py>=0 && x0-py < eimg->width  ) {
				pos=eimg->width*(y0+px)+(x0-py);
				eimg->alpha[pos]=alpha;
			}
			if( y0+px>=0 && y0+px < eimg->height && x0+py>=0 && x0+py < eimg->width  ) {
				pos=eimg->width*(y0+px)+(x0+py);
				eimg->alpha[pos]=alpha;
			}
			if( y0-px>=0 && y0-px < eimg->height && x0-py>=0 && x0-py < eimg->width  ) {
				pos=eimg->width*(y0-px)+(x0-py);
				eimg->alpha[pos]=alpha;
			}
			if( y0-px>=0 && y0-px < eimg->height && x0+py>=0 && x0+py < eimg->width  ) {
				pos=eimg->width*(y0-px)+(x0+py);
				eimg->alpha[pos]=alpha;
			}
		}
        }

	return 0;
}

/*--------------------------------------------------------
Create a new imgbuf with certain shape/frame.
The frame are formed by different patterns of alpha values.
Usually the frame is to be used as cutout_shape of a image.

@width,height   basic width/height of the frame.
@color:		basic color of the shape
@type:		type/shape of the frame.
@pn:		numbers of paramters
@param:		array of params

Return:
	A pointer to an EGI_IMGBUF	OK
	NULL				Fail
---------------------------------------------------------*/
EGI_IMGBUF *egi_imgbuf_newFrameImg( int height, int width,
				 unsigned char alpha, EGI_16BIT_COLOR color,
				 enum imgframe_type type,
				 int pn, const int *param )
{

	EGI_IMGBUF *imgbuf=egi_imgbuf_create(height, width, alpha, color);
	if(imgbuf==NULL)
		return NULL;

	if( egi_imgbuf_setFrame(imgbuf, type, alpha, pn, param ) !=0 ) {
		printf("%s: Frame imgbuf created, but fail to set frame type!\n",__func__);
		/* Go on anyway....*/
	}

	return imgbuf;
}


/*------------------------------------------------------------------------------
To soft/blur an image by averaging pixel colors/alpha, with allocating 2D arrays
in input ineimg(ineimg->pcolors[][] and ineimg->palphas[][]).
The 2D arrays are to buffer color/alpha data and improve sorting speed, the final
results will be remainded in them.

Note:
1. The original EGI_IMGBUF keeps intact, a new EGI_IMGBUF with modified
   color/alpha values will be created.

2. If succeeds, 2D array of ineimg->pcolors and ineimg->palpha(if ineimg has alpha values)
   will be created for ineimg!(NOT for outeimg, but for imtermediate buffer!!!)
   and will NOT be freed here, he final results will be remainded in them.

3. !!! WARNING !!! After avgsoft, ineimg->pcolors/palphas has been processed/blured
   and NOT an exact copy of ineimg->imbuf any more!

4. If input ineimg has no alpha values, so will the outeimg.

5. Other elements in ineimg will NOT be copied to outeimg, such as
   ineimg->subimgs,ineimg->pcolors, ineimg->palphas..etc.


@eimg:	object image.
@size:  number of pixels taken to average, window size of avgerage filer.
	The greater the value, the more blurry the result will be.
	If input size<1; then it will be ajusted to 1.
	!!! --- NOTICE --- !!!
	If size==1, the result outeimg has a copy of original eimg's colors/alphas data.

@alpha_on:  True:  Also need to blur alpha values, only if the image has alpha value.
	    False: Do not blur alpha values.
	    !!!NOTE!!!: Even if alpha_on if False, ineimg->alpha will be copied to
	   	         outeimg->alpha if it exists.

@hold_on:    True: To continue to use ineimg->pcolors(palphas) as intermediate results,
		   DO NOT re_memcpy from ineimg->imgbuf(palphas).
	     False: Re_memcpy from ineimg->imgbuf(palphas).

Return:
	A pointer to a new EGI_IMGBUF with blured image  	OK
	NULL							Fail
------------------------------------------------------------------------------*/
EGI_IMGBUF  *egi_imgbuf_avgsoft( EGI_IMGBUF *ineimg, int size, bool alpha_on, bool hold_on)
{
	int i,j,k;
	EGI_16BIT_COLOR *colors;	/* to hold colors in avg filter windown */
	unsigned int avgALPHA;
	int height, width;
	EGI_IMGBUF *outeimg=NULL;

	/* a copy to ineimg->pcolors and palphas */
	EGI_16BIT_COLOR **pcolors=NULL; /* WARNING!!! pointer same as ineimg->pcolors */
	unsigned char 	**palphas=NULL; /* WARNING!!! pointer same as ineimg->palphas */


	if( ineimg==NULL || ineimg->imgbuf==NULL )
		return NULL;

	height=ineimg->height;
	width=ineimg->width;

	/* adjust size to Min. 1 */
	if(size<1)
		size=1;

	/***  ---- Redefine and adjust alpha_on ----
	 * alpha_on: only image has alpha value AND input alpha_on is true!
	 */
	alpha_on = ( ineimg->alpha && alpha_on ) ? true : false;
	if(alpha_on) { printf("%s: --- alpha on ---\n",__func__); }
	else 	     { printf("%s: --- alpha off ---\n",__func__); }

	/* adjust Max. of filter window size to be Min(width/2, height/2) */
#if 0  /* Not necessary any more */
	if(size>width/2 || size>height/2) {
		size=width/2;
		if( size > height/2 )
			size=height/2;
	}
#endif

	/* calloc colors */
	colors=calloc(size, sizeof(EGI_16BIT_COLOR));
	if(colors==NULL) {
		printf("%s: Fail to calloc colors!\n",__func__);
		return NULL;
	}

/* -------- Malloc/assign  2D array ineimg->pcolors and ineimg->palphas  --------- */
if( ineimg->pcolors==NULL || ( alpha_on && ineimg->alpha !=NULL && ineimg->palphas==NULL ) )
{
	printf("%s: malloc 2D array for ineimg->pcolros, palphas ...\n",__func__);

	/* free them both before re_malloc */
	egi_free_buff2D((unsigned char **)ineimg->pcolors, height);
	egi_free_buff2D(ineimg->palphas, height);

	/* alloc pcolors */
	ineimg->pcolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(height,width*sizeof(EGI_16BIT_COLOR));
	pcolors=ineimg->pcolors; /* WARNING!!! pointing to the same data */
	if(pcolors==NULL) {
		printf("%s: Fail to malloc pcolors.\n",__func__);
		return NULL;
	}
	/* copy color from input ineimg */
	for(i=0; i<height; i++)
		memcpy( pcolors[i], ineimg->imgbuf+i*width, width*sizeof(EGI_16BIT_COLOR) );

	/* alloc alpha if alpha_on and original image has alpha!!! */
	if(alpha_on && ineimg->alpha) {
		ineimg->palphas=egi_malloc_buff2D(height,width*sizeof(unsigned char));
		palphas=ineimg->palphas; /* WARNING!!! pointing to the same data */
		if(palphas==NULL) {
			printf("%s: Fail to malloc palphas.\n",__func__);
			egi_free_buff2D((unsigned char **)ineimg->pcolors, height);
			pcolors=NULL;
			return NULL;
		}
		/* copy color from input ineimg */
		for(i=0; i<height; i++)
			memcpy( palphas[i], ineimg->alpha+i*width, width*sizeof(unsigned char) );
	}

}
else  {
	/*** 	<<<<  If pcolors/palphas already allocated in input ineimg  >>>
	 * 2D array data may have been contanimated/processed already !!!
	 * If we don't want to continue to use it, then we NEED to update to the same as
	 * original ineimg->imgbuf and ineimg->alpha
	 */

	/* WARNING!!! make a copy pointers. before we use 'pcolors' instead of 'ineimg->pcolors' */
	pcolors=ineimg->pcolors;
	palphas=ineimg->palphas;

	/* If do not hold to use old ineimg->pcolors(palphas) */
	if(!hold_on) {
		for(i=0; i<height; i++) {
			//printf("%s: memcpy to update ineimg->pcolros...\n",__func__);
			memcpy( pcolors[i], ineimg->imgbuf+i*width, width*sizeof(EGI_16BIT_COLOR) );

			//printf("%s: memcpy to update ineimg->alphas...\n",__func__);
			if(alpha_on && ineimg->alpha ) /* only if alpha_on AND ineimg has alpha value */
				memcpy( palphas[i], ineimg->alpha+i*width, width*sizeof(unsigned char) );
		}
	}

} /* ------------ END 2D ARRAY MALLOC --------------- */

	/* create output imgbuf */
	outeimg= egi_imgbuf_create( height, width, 0, 0); /* (h,w,alpha,color) will be replaced by avg later */
	if(outeimg==NULL) {
		free(colors);
		egi_free_buff2D((unsigned char **)ineimg->pcolors, height);
		egi_free_buff2D(ineimg->palphas, height);
		pcolors=NULL; palphas=NULL;
		return NULL;
	}

	/* free alpha if original is NULL, ingore alpha_on for this.*/
	if(ineimg->alpha==NULL) {
		free(outeimg->alpha);
		outeimg->alpha=NULL;
	}

	/* --- STEP 1:  blur rows --- */
	for(i=0; i< height; i++) {
		/* first avg, for left to right */
		for(j=0; j< width; j++) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j+k > width-1 ) {
					colors[k]=pcolors[i][j+k-width]; /* loop back */
					if(alpha_on)
						avgALPHA += palphas[i][j+k-width];
				}
				else {
					colors[k]=pcolors[i][j+k];
					if(alpha_on)
						avgALPHA += palphas[i][j+k];
				}
			}
			/* --- update intermediatey pcolors[] and alphas[] here --- */
			pcolors[i][j]=egi_16bitColor_avg(colors,size);
			if(alpha_on)
				palphas[i][j]=avgALPHA/size;
		}
		/* second avg, for right to left */
		for(j=width-1; j>=0; j--) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j-k < 0) {		/* loop back if out of range */
					colors[k]=pcolors[i][j-k+width];
					if(alpha_on)
						avgALPHA += palphas[i][j-k+width];
				}
				else  {
					colors[k]=pcolors[i][j-k];
					if(alpha_on)
						avgALPHA += palphas[i][j-k];
				}
			}
			/* --- update intermediatey pcolors[] and alphas[] here --- */
			pcolors[i][j]=egi_16bitColor_avg(colors,size);
			if(alpha_on)
				palphas[i][j]=avgALPHA/size;
		}
	}

	/* --- STEP 2:  blur columns --- */
	for(i=0; i< width; i++) {
		/* first avg, from top to bottom */
		for(j=0; j< height; j++) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j+k > height-1 ) {
					colors[k]=pcolors[j+k-height][i]; /* loop back */
					if(alpha_on)
						avgALPHA += palphas[j+k-height][i];
				}
				else {
					colors[k]=pcolors[j+k][i];
					if(alpha_on)
						avgALPHA += palphas[j+k][i];
				}
			}
			/*  ---- final output to outeimg ---- */
			pcolors[j][i]=egi_16bitColor_avg(colors,size);
			if(alpha_on)
				palphas[j][i]=avgALPHA/size;
		}
		/* second avg, from bottom to top */
		for(j=height-1; j>=0; j--) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j-k < 0 ) {		/* loop back if out of range */
					colors[k]=pcolors[j-k+height][i];
					if(alpha_on)
						avgALPHA += palphas[j-k+height][i];
				}
				else {
					colors[k]=pcolors[j-k][i];
					if(alpha_on)
						avgALPHA += palphas[j-k][i];
				}
			}
			/*  ---- final output to outeimg ---- */
			//outeimg->imgbuf[j*width+i]=egi_16bitColor_avg(colors, size);
			pcolors[j][i]=egi_16bitColor_avg(colors,size);
			if(alpha_on)
				//outeimg->alpha[j*width+i]=avgALPHA/size;
				palphas[j][i]=avgALPHA/size;
		}
	}

		/* ------- memcpy finished data ------ */
	/* now ineimg->pcolors[]/palphas[] has final processed data, memcpy to outeimg->imgbuf */
	for( i=0; i<height; i++ ) {
		memcpy(outeimg->imgbuf+i*width, ineimg->pcolors[i], width*sizeof(EGI_16BIT_COLOR));

		/*** !!!Remind that if(alpha_on), ineimg MUST has alpha values
		 * SEE redifinition of alpha_on at the very beginning of the func.
		 */
		if(alpha_on)
			memcpy( outeimg->alpha+i*width, ineimg->palphas[i], width*sizeof(unsigned char));
	}

	/* If ineimg has alpha values, but alpha_on set is false, just copy alpha to outeimg */
	if( !alpha_on && ineimg->alpha != NULL)
		memcpy( outeimg->alpha, ineimg->alpha, height*width*sizeof(unsigned char));


	/* free colors */
	free(colors);

	/* Don NOT free here, let egi_imgbuf_free() do it! */
//	egi_free_buff2D((unsigned char **)ineimg->pcolors, height);
//	egi_free_buff2D(ineimg->palphas, height);

	return outeimg;
}


/*-------------------- !!! NO 2D ARRAYS APPLIED !!!------------------------
Nearly same speed as of egi_imgbuf_avgsoft(), But beware of the size of the
picture, especailly like 1024x901(odd)   !!! FAINT !!!

!!! --- NOTICE --- !!!
If size==1, the result outeimg has a copy of original eimg's colors/alphas data.

Note:
1. Function same as egi_imgbuf_avgsoft(), but without allocating  additional
   2D array for color/alpha data processsing.
2. For small size picture, 240x320 etc, this func is a litter faster than
   egi_imgbuf_avgsoft().

3. For big size picture, depends on picture size ???????
   study cases:

	1.   1000x500 JPG        nearly same speed for all blur_size.

	     	( !!!!!  Strange for 1024x901  !!!!! )
	2.   1024x901 PNG,RBG    blur_size>13, 2D faster; blur_size<13, nearly same speed.
	     1024x900 PNG,RBG    nearly same speed for all blur_size, 1D just a litter slower.

	3.   1200x1920 PNG,RGBA  nearly same speed for all blur_size.
     	     1922x1201 PNG,RGBA  nearly same speed for all blur_size.


Return:
	A pointer to a new EGI_IMGBUF with blured image  	OK
	NULL							Fail
----------------------------------------------------------------------------*/
EGI_IMGBUF  *egi_imgbuf_avgsoft2(const EGI_IMGBUF *ineimg, int size, bool alpha_on)
{
	int i,j,k;
	EGI_16BIT_COLOR *colors;	/* to hold colors in avg filter windown */
	unsigned int avgALPHA;
	int height, width;
	unsigned int index;
	EGI_IMGBUF *outeimg=NULL;

	if( ineimg==NULL || ineimg->imgbuf==NULL )
		return NULL;

	height=ineimg->height;
	width=ineimg->width;

	/* adjust size to Min. 1 */
	if(size<1)
		size=1;


	/***   Redefine alpha_on
	 * alpha_on: only image has alpha value AND input alpha_on is true!
	 */
	alpha_on = (ineimg->alpha && alpha_on) ? true : false;
	if(alpha_on) { printf("%s: --- alpha on ---\n",__func__); }
	else 	     { printf("%s: --- alpha off ---\n",__func__); }

	/* adjust Max. of filter window size to be Min(width/2, height/2) */
#if 0  /* Not necessary any more */
	if(size>width/2 || size>height/2) {
		size=width/2;
		if( size > height/2 )
			size=height/2;
	}
#endif

	/* calloc colors */
	colors=calloc(size, sizeof(EGI_16BIT_COLOR));
	if(colors==NULL) {
		printf("%s: Fail to calloc colors!\n",__func__);
		return NULL;
	}

	/* create output imgbuf */
	outeimg= egi_imgbuf_create( height, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced by avg later */
	if(outeimg==NULL) {
		free(colors);
		return NULL;
	}
	/***
	 * copy ineimg->imgbuf to outeimg->imgbuf, we'll process on outeimg!
	 *  Or, in 'first blur rows, from left to right' we update outeimg->imgbuf with proceeded data! */
	//memcpy(outeimg->imgbuf, ineimg->imgbuf, height*width*sizeof(EGI_16BIT_COLOR));

	/* free alpha is original is NULL*/
	if(ineimg->alpha==NULL) {
		free(outeimg->alpha);
		outeimg->alpha=NULL;
	}

	/* --- STEP 1:  blur rows, pick data in ineimg  --- */
	for(i=0; i< height; i++) {
		/* first avg, for left to right */
		for(j=0; j< width; j++) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j+k > width-1 ) {
					index=i*width+j+k-width; /* loop back */
					/* Since outeimg has data in imgbuf[] now, do not pick from ineimg!
					 * Same for alpha data.
					 */
					//colors[k]=ineimg->imgbuf[index];
					colors[k]=outeimg->imgbuf[index];
					if(alpha_on)
						//avgALPHA += ineimg->alpha[index];
						avgALPHA += outeimg->alpha[index];
				}
				else {
					index=i*width+j+k;
					/* outeimg has NO data in imgbuf[], need to pick from ineimg */
					colors[k]=ineimg->imgbuf[index];
					if(alpha_on)
						avgALPHA += ineimg->alpha[index];
				}
			}
			/* --- update intermediatey pcolors and alphas here --- */
			index=i*width+j;
			outeimg->imgbuf[index]=egi_16bitColor_avg(colors,size);
			if(alpha_on)
				outeimg->alpha[index]=avgALPHA/size;
		}
		/* second avg, for right to left !!!Now we have proceed data in outeimg->imgbuf  */
		for(j=width-1; j>=0; j--) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j-k < 0) {		/* loop back if out of range */
					index=i*width+j-k+width;
					//colors[k]=ineimg->imgbuf[index];
					colors[k]=outeimg->imgbuf[index];   /* outeimg has proceeded data now! */
					if(alpha_on)
						//avgALPHA += ineimg->alpha[index];
						avgALPHA += outeimg->alpha[index];
				}
				else  {
					index=i*width+j-k;
					//colors[k]=ineimg->imgbuf[index];
					colors[k]=outeimg->imgbuf[index]; /* outeimg has proceeded data! */
					if(alpha_on)
						//avgALPHA += ineimg->alpha[index];
						avgALPHA += outeimg->alpha[index];
				}
			}
			/* --- update intermediatey pcolors and alphas here --- */
			index=i*width+j;
			outeimg->imgbuf[index]=egi_16bitColor_avg(colors,size);
			if(alpha_on)
				outeimg->alpha[index]=avgALPHA/size;
		}
	}

	/* --- STEP 2:  blur columns, pick data in outeimg now.  --- */
	for(i=0; i< width; i++) {
		/* first avg, from top to bottom */
		for(j=0; j< height; j++) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j+k > height-1 ) {
					index=(j+k-height)*width+i; /* loop back */
					colors[k]=outeimg->imgbuf[index]; /* now use outeimg instead of ineimg */
					if(alpha_on)
						avgALPHA += outeimg->alpha[index];
				}
				else {
					index=(j+k)*width+i;
					colors[k]=outeimg->imgbuf[index];
					if(alpha_on)
						avgALPHA += outeimg->alpha[index];
				}
			}
			/*  ---- final output to outeimg ---- */
			index=j*width+i;
			outeimg->imgbuf[index]=egi_16bitColor_avg(colors, size);
			if(alpha_on)
				outeimg->alpha[index]= avgALPHA/size;
		}
		/* second avg, from bottom to top */
		for(j=height-1; j>=0; j--) {
			avgALPHA=0;
			/* in the avg filter window */
			for(k=0; k<size; k++) {
				if( j-k < 0 ) {		/* loop back if out of range */
					index=(j-k+height)*width+i;
					colors[k]=outeimg->imgbuf[index];
					if(alpha_on)
						avgALPHA += outeimg->alpha[index];
				}
				else {
					index=(j-k)*width+i;
					colors[k]=outeimg->imgbuf[index];
					if(alpha_on)
						avgALPHA += outeimg->alpha[index];
				}
			}
			/*  ---- final output to outeimg ---- */
			index=j*width+i;
			outeimg->imgbuf[index]=egi_16bitColor_avg(colors, size);
			if(alpha_on)
				outeimg->alpha[index]=avgALPHA/size;
		}
	}

	/* If ineimg has alpha values, but alpha_on is false, just copy alpha to outeimg */
	if( !alpha_on && ineimg->alpha != NULL)
		memcpy( outeimg->alpha, ineimg->alpha, height*width*sizeof(unsigned char)) ;

	/* free colors */
	free(colors);

	return outeimg;
}


/*-----------------------------------------------------------
Compute an approriate size to fit in the image.
The result size should be well fit into the original w*h size
while keep aspect ratio unchanged.

@imgbuf:	Pointer to an EGI_IMGBUF.
@w,h:		Pointers to canvas size, it will be modified
		according to input imgbuf.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------*/
int egi_imgbuf_get_fitsize(EGI_IMGBUF *imgbuf, int *w, int *h)
{
	if( imgbuf==NULL || imgbuf->imgbuf==NULL)
		return -1;
	if( w==NULL || h==NULL)
		return -1;
	if( *w<1 || *h<1)
		return -1;

	int oldwidth=imgbuf->width;
	int oldheight=imgbuf->height;

        /* Keep image aspect ratio */
        if( 1.0*(*h)/(*w) > 1.0*oldheight/oldwidth )
		/* Then to adjust h in ratio */
		*h=round(1.0*(*w)*oldheight/oldwidth);
        else   /* Then to adjust w in ration */
	        *w=round(1.0*(*h)*oldwidth/oldheight);

	return 0;
}


/*-----------------------------------------------------------------------
Resize an image and create a new EGI_IMGBUF to hold the new image data.
Only size/color/alpha of ineimg will be transfered to outeimg, others
such as subimg will be ignored. )

TODO:
XXX 1. Before scale down an image, merge and shrink it to a certain size first!!!
	--OK, see egi_imgbuf_scale()
XXX 2. Block resize (zoom), resize a block of the original image to a given size.
	--Use egi_imgbuf_blockCopy(), egi_imgbuf_coplyBlock().

NOTE:
0. 			----- IMPORTANT -----
   This function is good at enlarging an image by creating new pixels with linear interpolation.
   However, when shrink(scale down) an image, it only select discrete pixles to form
   a new image. Without compressing/merging nearby pixels, it's NOT a linear way.
   Call egi_imgbuf_scale() to linearly shrink/scale_down an image.

1. Linear interpolation is carried out with fix point calculation.
2. If either width or height is <1, then adjust width/height proportional to oldwidth/oldheight.
3. !!! --- LIMIT --- !!!
   Fix point data type: 			int
   Pixel 16bit color:				16bit
   Fix point multiplier(or divisor): 		f15 ( *(1U<<15) )
   interpolation ratio:				[0 1]
   Scale limit/Interpolation resolution:

   Min. ratio 1/(1U<<15), so more than 1k points can be interpolated between
   two pixels.


@ineimg:	Input EGI_IMGBUF holding the original image data.
@keep_ratio:	True: Keep aspect ratio.
		False: Stretch size.
@width:		Width for new image.
		If width<=0 AND height>0: adjust width/height proportional to oldwidth/oldheight.
		Aft above, if width<2, auto. adjust it to 2.

@height:	Height for new image.
		If heigth<=0 AND width>0: adjust height/width proportional to oldheight/oldwidth.
		Aft above, if height<2, auto. adjust it to 2.
Return:
	A pointer to EGI_IMGBUF with new image 		OK
	NULL						Fails
------------------------------------------------------------------------*/
EGI_IMGBUF  *egi_imgbuf_resize( EGI_IMGBUF *ineimg, bool keep_ratio,
				//unsigned int width, unsigned int height )
				int width, int height )
{
	int i,j;
	int ln,rn;		/* left/right(or up/down) index of pixel in a row of ineimg */
	int f15_ratio;
	unsigned int color_rowsize, alpha_rowsize;
	EGI_IMGBUF *outeimg=NULL;
//	EGI_IMGBUF *tmpeimg=NULL;

	/* for intermediate processing */
	EGI_16BIT_COLOR **icolors=NULL;
	unsigned char 	**ialphas=NULL;

	/* for final processing */
	EGI_16BIT_COLOR **fcolors=NULL;
	unsigned char 	**falphas=NULL;

	if( ineimg==NULL || ineimg->imgbuf==NULL) //|| width<=0 || height<=0 ) adjust to 2
		return NULL;


	/* Get mutex lock */
	if(pthread_mutex_lock(&ineimg->img_mutex) !=0 ){
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to lock image mutex!", __func__);
		return NULL;
	}
 /* ------ >>>  Critical Zone  */

	unsigned int oldwidth=ineimg->width;
	unsigned int oldheight=ineimg->height;

	bool alpha_on=false;
	if(ineimg->alpha!=NULL)
			alpha_on=true;

	/* If same size, OK */
	if( height==ineimg->height && width==ineimg->width )
		goto CREATE_OUTEIMG;

	/* If W or H is <=0: Adjust width/height proportional to oldwidth/oldheight */
	if(width<1 && height<1) {
		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	}
	else if(width<1)
		width=height*oldwidth/oldheight;
	else if(height<1)
		height=width*oldheight/oldwidth;

	/* Keep image aspect ratio */
	if(keep_ratio) {
//		if( 1.0*height/width > 1.0*oldheight/oldwidth )
		if( 1.0*height/width < 1.0*oldheight/oldwidth )
			width=round(1.0*height*oldwidth/oldheight);
		else
			height=round(1.0*width*oldheight/oldwidth);
	}

	/* Limit width and height to 2, ==1 will cause Devide_By_Zero exception. */
	if(width<2) width=2;
	if(height<2) height=2;


#if 0 /* ----- FOR TEST ONLY ----- */
	/* create temp imgbuf */
	tmpeimg= egi_imgbuf_create(oldheight, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced later */
	if(tmpeimg==NULL) {
		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	}
	if(!alpha_on) {
		free(tmpeimg->alpha);
		tmpeimg->alpha=NULL;
	}
#endif /* ----- TEST ONLY ----- */




CREATE_OUTEIMG:
	/* Create output imgbuf */
	if( alpha_on )
		outeimg = egi_imgbuf_create( height, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced later */
	else /* Alpha_on==false */
		outeimg = egi_imgbuf_createWithoutAlpha( height, width, 0);
	/* If fails */
	if(outeimg==NULL) {
		egi_dperr("Fail to create outeimg!");
		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	}

	/* If same size, just memcpy data */
	if( height==ineimg->height && width==ineimg->width ) {
		memcpy( outeimg->imgbuf, ineimg->imgbuf, sizeof(EGI_16BIT_COLOR)*height*width);
		if(alpha_on) {
			memcpy( outeimg->alpha, ineimg->alpha, sizeof(unsigned char)*height*width);
		}

		pthread_mutex_unlock(&ineimg->img_mutex);
		return outeimg;
	}

	/* TODO: NOT keep ratio, if height or width is the same size, to speed up */


	/* Allocate mem to hold oldheight x width image for intermediate processing */
	//printf("Malloc icolors...\n");
	icolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(oldheight,width*sizeof(EGI_16BIT_COLOR));
	if(icolors==NULL) {
		egi_dperr("Fail to malloc icolors.");
		egi_imgbuf_free(outeimg);

		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	}
	if(alpha_on) {
	   ialphas=egi_malloc_buff2D(oldheight,width*sizeof(unsigned char));
	   if(ialphas==NULL) {
		egi_dperr("Fail to malloc ipalphas.");
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);

		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	   }
	}

	/* Allocate mem to hold final image size height x width  */
	//printf("Malloc fcolors...\n");
	fcolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(height,width*sizeof(EGI_16BIT_COLOR));
	if(fcolors==NULL) {
		egi_dperr("Fail to malloc fcolors.");
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		if(alpha_on)
			egi_free_buff2D(ialphas, oldheight);

		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	}
	if(alpha_on) {
	    falphas=egi_malloc_buff2D(height,width*sizeof(unsigned char));
	    if(falphas==NULL) {
		egi_dperr("Fail to malloc ipalphas.");
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		egi_free_buff2D(ialphas, oldheight);
		egi_free_buff2D((unsigned char **)fcolors, height);

		pthread_mutex_unlock(&ineimg->img_mutex);
		return NULL;
	    }
	}

	egi_dpstd("height=%d, width=%d, oldheight=%d, oldwidth=%d \n",
								height, width, oldheight, oldwidth );

	/* Get new rowsize in bytes */
	color_rowsize=width*sizeof(EGI_16BIT_COLOR);
	alpha_rowsize=width*sizeof(EGI_8BIT_ALPHA);

	/* ----- STEP 1 -----  scale image from [oldheight_X_oldwidth] to [oldheight_X_width] */
	//printf("STEP 1 scale image to [oldheight_X_width]...\n");
	for(i=0; i<oldheight; i++)
	{
//		printf(" \n STEP 1: ----- row %d ----- \n",i);
		for(j=0; j<width; j++) /* apply new width */
		{
			/* Note:
			 *   1. Here ln is left point index, and rn is right point index of a row of pixels.
			 *      ln and rn are index of original image row.
			 *      The inserted new point is between ln and rn.
			 *   2. Notice that (oldwidth-1)/(width-1) is acutual color_width ratio.
			 */
			ln=j*(oldwidth-1)/(width-1);/* xwidth-1 is gap numbers */
			/* j of new_width map to that of old_width: 1.0*j*(oldwidth-1)/(width-1)-ln --> Fixed point (1U<<15) */
			f15_ratio=(j*(oldwidth-1)-ln*(width-1))*(1U<<15)/(width-1); /* >= 0 */
			/* If last point, the ratio must be 0! no more point at its right now! */
			if(ln == oldwidth-1)
				rn=ln;
			else
				rn=ln+1;
#if 0 /* --- TEST --- */
			printf( "row: ln=%d, rn=%d,  f15_ratio=%d, ratio=%f \n",
						ln, rn, f15_ratio, 1.0*f15_ratio/(1U<<15) );
#endif
			/* Interpolate pixel color/alpha value, and store to icolors[]/ialphas[]  */
			if(alpha_on) {
				//printf("alpha_on interpolate ...\n");
				egi_16bitColor_interplt(ineimg->imgbuf[i*oldwidth+ln], /* color1 */
							ineimg->imgbuf[i*oldwidth+rn], /* color2 */
			                                /* uchar alpha1,  uchar alpha2 */
					 	ineimg->alpha[i*oldwidth+ln], ineimg->alpha[i*oldwidth+rn],
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 	f15_ratio, icolors[i]+j, ialphas[i]+j  );
			}
			else {
				//printf("alpha_off interpolate ...\n");
				egi_16bitColor_interplt(ineimg->imgbuf[i*oldwidth+ln], /* color1 */
							ineimg->imgbuf[i*oldwidth+rn], /* color2 */
			                                /* uchar alpha1,  uchar alpha2 */
					 		 0, 0,   /* whatever when out pointer is NULL */
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 		f15_ratio, icolors[i]+j, NULL );
			}
		}
#if 0 /* ----- FOR TEST ONLY ----- */
		/* copy row data to tmpeimg */
		memcpy( tmpeimg->imgbuf+i*width, icolors[i], color_rowsize );
		if(alpha_on)
		    memcpy( tmpeimg->alpha+i*width, ialphas[i], alpha_rowsize );
#endif
	}

	/* NOW: rowsize keep same here! Just need to scale height. */

/* ------- <<<  Critical Zone  */
	/* put mutex lock for ineimg */
  	pthread_mutex_unlock(&ineimg->img_mutex);

	/* ----- STEP 2 -----  scale image from [oldheight_X_width] to [height_X_width] */
	//printf("STEP 2: scale image to [height_X_width]...\n");
	for(i=0; i<width; i++)
	{
//		printf(" \n STEP 2: ----- column %d ----- \n",i);
		for(j=0; j<height; j++) /* apply new height */
		{
			/* Here ln is upper point index, and rn is lower point index of a column of pixels.
			 * ln and rn are index of original image column.
			 * The inserted new point is between ln and rn.
			 */
			ln=j*(oldheight-1)/(height-1);/* xwidth-1 is gap numbers */
			f15_ratio=(j*(oldheight-1)-ln*(height-1))*(1U<<15)/(height-1); /* >= 0 */
			/* If last point, the ratio must be 0! no more point at its lower position now! */
			if( ln == oldheight-1 )
				rn=ln;
			else
				rn=ln+1;
#if 0 /* --- TEST --- */
			printf( "column: ln=%d, rn=%d,  f15_ratio=%d, ratio=%f \n",
						ln, rn, f15_ratio, 1.0*f15_ratio/(1U<<15) );
#endif
			/* interpolate pixel color/alpha value, and store data to fcolors[]/falphas[]  */
			if(alpha_on) {
				//printf("column: alpha_on interpolate ...\n");
				egi_16bitColor_interplt(
							//ineimg->imgbuf[ln*width+i], /* color1 */
							//ineimg->imgbuf[rn*width+i], /* color2 */
							 icolors[ln][i], icolors[rn][i], /* old: color1, color2 */
			                                /* old: uchar alpha1,  uchar alpha2 */
					 	//ineimg->alpha[ln*width+i], ineimg->alpha[rn*width+i],
						   	 ialphas[ln][i], ialphas[rn][i],
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 	         f15_ratio, fcolors[j]+i, falphas[j]+i  );
			}
			else {
				//printf("column: alpha_off interpolate ...\n");
				egi_16bitColor_interplt(
							//ineimg->imgbuf[ln*width+i], /* color1 */
							//ineimg->imgbuf[rn*width+i], /* color2 */
							 icolors[ln][i], icolors[rn][i], /* color1, color2 */
			                                /* uchar alpha1,  uchar alpha2 */
					 		 0, 0,   /* whatever, when passout pointer is NULL */
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 		f15_ratio, fcolors[j]+i, NULL );
			}
		}
		/* Can NOT copy row data here! as it transverses column.  */
	}

	/* Copy row data to outeimg when all finish. */
	for( i=0; i<height; i++) {
		memcpy( outeimg->imgbuf+i*width, fcolors[i], color_rowsize );
		if(alpha_on)
		    memcpy( outeimg->alpha+i*width, falphas[i], alpha_rowsize );
	}

	/* Free buffers */
	//printf("%s: Free buffers...\n", __func__);
	egi_free_buff2D((unsigned char **)icolors, oldheight);
	egi_free_buff2D(ialphas, oldheight);   /* no matter alpha off */
	egi_free_buff2D((unsigned char **)fcolors, height);
	egi_free_buff2D(falphas, height);      /* no matter alpha off */

#if 0 /* ----- FOR TEST ONLY ----- */
	if(tmpeimg!=NULL) {
		printf("return tmpeimg...\n");
		egi_imgbuf_free(outeimg);
		return tmpeimg;
	}
#endif /* ----- FOR TEST ONLY ----- */

	return outeimg;
}

/*----------------  egi_imgbuf_resize No Mutex_lock  -------------------
Refer to egi_imgbuf_resize();

Note:
1. ineimg will be replaced by transimg in scale down case.

-----------------------------------------------------------------------*/
EGI_IMGBUF  *egi_imgbuf_resize_nolock( EGI_IMGBUF *ineimg, bool keep_ratio, int width, int height )
{
	int i,j;
	int ln,rn;		/* left/right(or up/down) index of pixel in a row of ineimg */
	int f15_ratio;
	unsigned int color_rowsize, alpha_rowsize;
	EGI_IMGBUF *outeimg=NULL;
//	EGI_IMGBUF *tmpeimg=NULL;
	EGI_IMGBUF *transimg=NULL;

	/* for intermediate processing */
	EGI_16BIT_COLOR **icolors=NULL;
	unsigned char 	**ialphas=NULL;

	/* for final processing */
	EGI_16BIT_COLOR **fcolors=NULL;
	unsigned char 	**falphas=NULL;


	/* Input check */
	if( ineimg==NULL || ineimg->imgbuf==NULL) //|| width<=0 || height<=0 ) adjust to 2
		return NULL;
	if(width<1 && height<1)
		return NULL;


	bool alpha_on;
	if(ineimg->alpha!=NULL)
		alpha_on=true;
	else
		alpha_on=false;

	/* Original width/height. notice that ineimg MAY be replaced! */
	unsigned int origwidth=ineimg->width;
	unsigned int origheight=ineimg->height;

	/* If same size, OK */
	if( height==ineimg->height && width==ineimg->width )
		goto CREATE_OUTEIMG;

#if 1   /* Create a transition imgbuf to improve scaledown precision */

	/*** NOTE:
	 *  1. Ceate a transition imgbuf with nearest size to the new image, then replace input ineimg.
	 *  Interpolate on this transimg to scale down image to improve precsion.
	 *  2. Pixles in each BLCOK(nsw*nsh) will be merged into just ONE pixel.
	 *  3. TO compare with egi_imgbuf_scale()  <--------------
	 *
	 *  MidasHK_2202_04_05
	 */
	int nsw=ineimg->width/width;	/* Integer scaledown number, as BLOCK width */
	int nsh=ineimg->height/height;  /* Integer scaledown number, as BLOCK height */

	/* Limit, in case only H or W to be scaled down. */
	if(nsw<1)nsw=1;
	if(nsh<1)nsh=1;

	if( nsw >1 || nsh >1) {
		/* Round to get transW/transH:  transW>=nsw; tranH>=nsh */
		int transW=roundf(1.0*ineimg->width/nsw);  /* Rounded scaledown number */
		int transH=roundf(1.0*ineimg->height/nsh);

		int k,m,l;
		unsigned int n;
		EGI_16BIT_COLOR color; //alpha;
		unsigned int pcnt;
		unsigned int Rsum,Gsum,Bsum,Asum;

		/* T1. Create transimg */
		if(alpha_on) {
			transimg=egi_imgbuf_create(transH, transW, 0, 0);
			if(transimg==NULL)
				return NULL;
		} else {
			transimg=egi_imgbuf_createWithoutAlpha(transH, transW, 0);
			if(transimg==NULL)
				return NULL;
		}

		/* T1. Merge pixels in one integer BLOCK(nsh*nsw) to one pixel */
		/* Traverse BLOCKs vertically, K as row index for transimg */
		for(k=0; k< ineimg->height/nsh; k++) {
			/* T1.M1. Traverse BLOCKs horizontally, M as column index for transimg */
			for(m=0; m< ineimg->width/nsw; m++) {
			    Rsum=Gsum=Bsum=Asum=0;

			   /* T1.M1.1 Traverse block rows, l as row index of ineimg */
			   for(l=k*nsh; l< (k+1)*nsh; l++) {
				 /* T1.M1.1.2 Traverse pixels to be merged to one, n as ineimg pixel index */
				for(n=l*ineimg->width+m*nsw; n < l*ineimg->width+(m+1)*nsw; n++) {
					color=ineimg->imgbuf[n];
					Rsum += COLOR_R8_IN16BITS(color);
					Gsum += COLOR_G8_IN16BITS(color);
					Bsum += COLOR_B8_IN16BITS(color);
					if(alpha_on)
						Asum += ineimg->alpha[n];
				}
			   }
			   /* T1.M1.2 Average color/alpha for transimg */
			   pcnt = nsw*nsh;
			   transimg->imgbuf[k*transW+m]=COLOR_RGB_TO16BITS(Rsum/pcnt, Gsum/pcnt, Bsum/pcnt);
			   if(alpha_on)
				transimg->alpha[k*transW+m]=Asum/pcnt;
			}

			/* T1.M2. The last residual BLOCK if exits. */
			if( transW > ineimg->width/nsw ) {
			   /* T1.M2.1 The last m, as column index of transimg */
			   m=ineimg->width/nsw;  /* <-----<<<<<  The last m */
		    	   Rsum=Gsum=Bsum=Asum=0;

			   /* T1.M2.2 Traverse block rows, l as row index of ineimg */
			   for(l=k*nsh; l< (k+1)*nsh; l++) {
				 /* T1.M2.2.1 Traverse pixels to be merged to one, n as ineimg pixel index */
				for(n=l*ineimg->width+m*nsw; n < (l+1)*ineimg->width; n++) {  /* <-----<<<<< */
					color=ineimg->imgbuf[n];
					Rsum += COLOR_R8_IN16BITS(color);
					Gsum += COLOR_G8_IN16BITS(color);
					Bsum += COLOR_B8_IN16BITS(color);
					if(alpha_on)
						Asum += ineimg->alpha[n];
				}
			   }
			   /* T1.M2.3 Average color/alpha for transimg */
			   pcnt = (ineimg->width-m*nsw)*nsh; /* pixels in the residual Block */
			   /* NOW: m as last column index */
			   transimg->imgbuf[k*transW+m]=COLOR_RGB_TO16BITS(Rsum/pcnt, Gsum/pcnt, Bsum/pcnt);
			   if(alpha_on)
				transimg->alpha[k*transW+m]=Asum/pcnt;
			}
		}

		/* T2. Last residul row of BLOCKs if exits. */
		if( transH > ineimg->height/nsh) {
			/* K as row index for transimg */
			k= ineimg->height/nsh; /* <-----<<<<<  The last k!!! */

			/* T2.M1. Traverse BLOCKs horizontally, M as column index for transimg */
			for(m=0; m< ineimg->width/nsw; m++) {
			    Rsum=Gsum=Bsum=Asum=0;
			   /* T2.M1.1 Traverse block height, l as row index of ineimg */
			   for(l=k*nsh; l< ineimg->height; l++) {  /* <-----<<<< to last row of ineimg!!! */
				 /* T2.M1.1.2 Traverse pixels to be merged to one, n as ineimg pixel index */
				for(n=l*ineimg->width+m*nsw; n < l*ineimg->width+(m+1)*nsw; n++) {
					color=ineimg->imgbuf[n];
					Rsum += COLOR_R8_IN16BITS(color);
					Gsum += COLOR_G8_IN16BITS(color);
					Bsum += COLOR_B8_IN16BITS(color);
					if(alpha_on)
						Asum += ineimg->alpha[n];
				}
			   }
			   /* T2.M1.2 Average color/alpha for transimg */
			   pcnt = nsw*(ineimg->height-k*nsh); /* NOW k as last row index for transimg */
			   transimg->imgbuf[k*transW+m]=COLOR_RGB_TO16BITS(Rsum/pcnt, Gsum/pcnt, Bsum/pcnt);
			   if(alpha_on)
				transimg->alpha[k*transW+m]=Asum/pcnt;
			}

			/* T2.M2. The last residual BLOCK if exits. */
			if( transW > ineimg->width/nsw ) {
			   /* T2.M2.1 The last m, as column index of transimg */
			   m=ineimg->width/nsw;  /* The last m, as column index of transimg */
		    	   Rsum=Gsum=Bsum=Asum=0;

			   /* T2.M2.2 Traverse block height, l as row index of ineimg */
			   for(l=k*nsh; l< ineimg->height; l++) {  /* <-----<<<< to last row of ineimg!!! */
				 /* T2.M2.2.1 Traverse pixels to be merged to one, n as ineimg pixel index */
				for(n=l*ineimg->width+m*nsw; n < l*ineimg->width+ineimg->width; n++) {  /* <-------<<<< */
					color=ineimg->imgbuf[n];
					Rsum += COLOR_R8_IN16BITS(color);
					Gsum += COLOR_G8_IN16BITS(color);
					Bsum += COLOR_B8_IN16BITS(color);
					if(alpha_on)
						Asum += ineimg->alpha[n];
				}
			   }
			   /* T1.M2.3 Average color/alpha for transimg */
			   pcnt = (ineimg->width-m*nsw)*(ineimg->height-k*nsh); /* pixels in the residual Block */
			   /* NOW: k as last row index, and m as last colum index. */
			   transimg->imgbuf[k*transW+m]=COLOR_RGB_TO16BITS(Rsum/pcnt, Gsum/pcnt, Bsum/pcnt);
			   if(alpha_on)
				transimg->alpha[k*transW+m]=Asum/pcnt;
			}
		}

/* -----> Replace ineimg with transimg <----- */
		ineimg=transimg;

		egi_dpstd("transimg W%dxH%d created!\n", transimg->width, transimg->height);
	}
#endif

	/* Notice: orignal ineimg may be replaced by transimg!*/
	unsigned int oldwidth=ineimg->width;   /* !!! X= origwidth */
	unsigned int oldheight=ineimg->height; /* !!! X= origheight */

	/* If W or H is <=0: Adjust width/height proportional to oldwidth/oldheight */
	//if(width<1 && height<1) { ---- > Move to  input_check
	//	return NULL;
	//}
	if(width<1)
		width=height*origwidth/origheight;
	else if(height<1)
		height=width*origheight/origwidth;

	/* Keep image aspect ratio. use origheight/origwidht!!!*/
	if(keep_ratio) {
//		if( 1.0*height/width > 1.0*origheight/origwidth ) {
		if( 1.0*height/width < 1.0*origheight/origwidth ) {
			width=round(1.0*height*origwidth/origheight);
		}
		else
			height=round(1.0*width*origheight/origwidth);
	}

	/* Limit width and height to 2, ==1 will cause Devide_By_Zero exception. */
	if(width<2) width=2;
	if(height<2) height=2;


CREATE_OUTEIMG:
	/* Create output imgbuf */
	if( alpha_on )
		outeimg = egi_imgbuf_create( height, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced later */
	else /* Alpha_on==false */
		outeimg = egi_imgbuf_createWithoutAlpha( height, width, 0);
	if(outeimg==NULL) {
		egi_dperr("Fail to create outeimg!");
		egi_imgbuf_free2(&transimg);
		return NULL;
	}

	/* If same size, just memcpy data */
	if( height==ineimg->height && width==ineimg->width ) {
		memcpy( outeimg->imgbuf, ineimg->imgbuf, sizeof(EGI_16BIT_COLOR)*height*width);
		if(alpha_on) {
			memcpy( outeimg->alpha, ineimg->alpha, sizeof(unsigned char)*height*width);
		}
		egi_imgbuf_free2(&transimg);
		return outeimg;
	}

	/* Allocate mem to hold oldheight x width image for intermediate processing */
	//printf("Malloc icolors...\n");
	icolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(oldheight,width*sizeof(EGI_16BIT_COLOR));
	if(icolors==NULL) {
		egi_dperr("Fail to malloc icolors.");
		egi_imgbuf_free(outeimg);
		egi_imgbuf_free2(&transimg);
		return NULL;
	}
	if(alpha_on) {
	   ialphas=egi_malloc_buff2D(oldheight,width*sizeof(unsigned char));
	   if(ialphas==NULL) {
		egi_dperr("Fail to malloc ipalphas.");
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		egi_imgbuf_free2(&transimg);
		return NULL;
	   }
	}

	/* Allocate mem to hold final image size height x width  */
	//printf("Malloc fcolors...\n");
	fcolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(height,width*sizeof(EGI_16BIT_COLOR));
	if(fcolors==NULL) {
		egi_dperr("Fail to malloc fcolors.");
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		if(alpha_on)
			egi_free_buff2D(ialphas, oldheight);

		egi_imgbuf_free2(&transimg);
		return NULL;
	}
	if(alpha_on) {
	    falphas=egi_malloc_buff2D(height,width*sizeof(unsigned char));
	    if(falphas==NULL) {
		egi_dperr("Fail to malloc ipalphas.");
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		egi_free_buff2D(ialphas, oldheight);
		egi_free_buff2D((unsigned char **)fcolors, height);

		egi_imgbuf_free2(&transimg);
		return NULL;
	    }
	}

	egi_dpstd("height=%d, width=%d, oldheight=%d, oldwidth=%d \n",
								height, width, oldheight, oldwidth );

	/* Get new rowsize in bytes */
	color_rowsize=width*sizeof(EGI_16BIT_COLOR);
	alpha_rowsize=width*sizeof(unsigned char);

	/* ----- STEP 1 -----  scale image from [oldheight_X_oldwidth] to [oldheight_X_width] */
	//printf("STEP 1 scale image to [oldheight_X_width]...\n");
	for(i=0; i<oldheight; i++)
	{
//		printf(" \n STEP 1: ----- row %d ----- \n",i);
		for(j=0; j<width; j++) /* apply new width */
		{
			/* Note:
			 *   1. Here ln is left point index, and rn is right point index of a row of pixels.
			 *      ln and rn are index of original image row.
			 *      The inserted new point is between ln and rn.
			 *   2. Notice that (oldwidth-1)/(width-1) is acutual color_width ratio.
			 */
			ln=j*(oldwidth-1)/(width-1);/* xwidth-1 is gap numbers */
			/* j of new_width map to that of old_width: 1.0*j*(oldwidth-1)/(width-1)-ln --> Fixed point (1U<<15) */
			f15_ratio=(j*(oldwidth-1)-ln*(width-1))*(1U<<15)/(width-1); /* >= 0 */
			/* If last point, the ratio must be 0! no more point at its right now! */
			if(ln == oldwidth-1)
				rn=ln;
			else
				rn=ln+1;
#if 0 /* --- TEST --- */
			printf( "row: ln=%d, rn=%d,  f15_ratio=%d, ratio=%f \n",
						ln, rn, f15_ratio, 1.0*f15_ratio/(1U<<15) );
#endif
			/* Interpolate pixel color/alpha value, and store to icolors[]/ialphas[]  */
			if(alpha_on) {
				//printf("alpha_on interpolate ...\n");
				egi_16bitColor_interplt(ineimg->imgbuf[i*oldwidth+ln], /* color1 */
							ineimg->imgbuf[i*oldwidth+rn], /* color2 */
			                                /* uchar alpha1,  uchar alpha2 */
					 	ineimg->alpha[i*oldwidth+ln], ineimg->alpha[i*oldwidth+rn],
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 	f15_ratio, icolors[i]+j, ialphas[i]+j  );
			}
			else {
				//printf("alpha_off interpolate ...\n");
				egi_16bitColor_interplt(ineimg->imgbuf[i*oldwidth+ln], /* color1 */
							ineimg->imgbuf[i*oldwidth+rn], /* color2 */
			                                /* uchar alpha1,  uchar alpha2 */
					 		 0, 0,   /* whatever when out pointer is NULL */
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 		f15_ratio, icolors[i]+j, NULL );
			}
		}
#if 0 /* ----- FOR TEST ONLY ----- */
		/* copy row data to tmpeimg */
		memcpy( tmpeimg->imgbuf+i*width, icolors[i], color_rowsize );
		if(alpha_on)
		    memcpy( tmpeimg->alpha+i*width, ialphas[i], alpha_rowsize );
#endif
	}

	/* NOW: rowsize keep same here! Just need to scale height. */


	/* ----- STEP 2 -----  scale image from [oldheight_X_width] to [height_X_width] */
	//printf("STEP 2: scale image to [height_X_width]...\n");
	for(i=0; i<width; i++)
	{
//		printf(" \n STEP 2: ----- column %d ----- \n",i);
		for(j=0; j<height; j++) /* apply new height */
		{
			/* Here ln is upper point index, and rn is lower point index of a column of pixels.
			 * ln and rn are index of original image column.
			 * The inserted new point is between ln and rn.
			 */
			ln=j*(oldheight-1)/(height-1);/* xwidth-1 is gap numbers */
			f15_ratio=(j*(oldheight-1)-ln*(height-1))*(1U<<15)/(height-1); /* >= 0 */
			/* If last point, the ratio must be 0! no more point at its lower position now! */
			if( ln == oldheight-1 )
				rn=ln;
			else
				rn=ln+1;
#if 0 /* --- TEST --- */
			printf( "column: ln=%d, rn=%d,  f15_ratio=%d, ratio=%f \n",
						ln, rn, f15_ratio, 1.0*f15_ratio/(1U<<15) );
#endif
			/* interpolate pixel color/alpha value, and store data to fcolors[]/falphas[]  */
			if(alpha_on) {
				//printf("column: alpha_on interpolate ...\n");
				egi_16bitColor_interplt(
							//ineimg->imgbuf[ln*width+i], /* color1 */
							//ineimg->imgbuf[rn*width+i], /* color2 */
							 icolors[ln][i], icolors[rn][i], /* old: color1, color2 */
			                                /* old: uchar alpha1,  uchar alpha2 */
					 	//ineimg->alpha[ln*width+i], ineimg->alpha[rn*width+i],
						   	 ialphas[ln][i], ialphas[rn][i],
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 	         f15_ratio, fcolors[j]+i, falphas[j]+i  );
			}
			else {
				//printf("column: alpha_off interpolate ...\n");
				egi_16bitColor_interplt(
							//ineimg->imgbuf[ln*width+i], /* color1 */
							//ineimg->imgbuf[rn*width+i], /* color2 */
							 icolors[ln][i], icolors[rn][i], /* color1, color2 */
			                                /* uchar alpha1,  uchar alpha2 */
					 		 0, 0,   /* whatever, when passout pointer is NULL */
                                         /* int f15_ratio, EGI_16BIT_COLOR* color, unsigned char *alpha */
					 		f15_ratio, fcolors[j]+i, NULL );
			}
		}
		/* Can NOT copy row data here! as it transverses column.  */
	}

	/* Copy row data to outeimg when all finish. */
	for( i=0; i<height; i++) {
		memcpy( outeimg->imgbuf+i*width, fcolors[i], color_rowsize );
		if(alpha_on)
		    memcpy( outeimg->alpha+i*width, falphas[i], alpha_rowsize );
	}

	/* Free buffers */
	//printf("%s: Free buffers...\n", __func__);
	egi_free_buff2D((unsigned char **)icolors, oldheight);
	egi_free_buff2D(ialphas, oldheight);   /* no matter alpha off */
	egi_free_buff2D((unsigned char **)fcolors, height);
	egi_free_buff2D(falphas, height);      /* no matter alpha off */
	egi_imgbuf_free2(&transimg);

#if 0 /* ----- FOR TEST ONLY ----- */
	if(tmpeimg!=NULL) {
		printf("return tmpeimg...\n");
		egi_imgbuf_free(outeimg);
		return tmpeimg;
	}
#endif /* ----- FOR TEST ONLY ----- */

	return outeimg;
}


/*-----------------------------------------------------------------------
Scale up/down an image while keeping same W/H ratio. Special treatment for case
of scale_down_factor >= 2.

TODO:  Change original W/H ratio to width/height as input.

@ineimg:	Input EGI_IMGBUF holding the original image data.
@width:		Width for new image.
@height:	Height for new image.

Return:
	A pointer to EGI_IMGBUF with new image 		OK
	NULL						Fails
------------------------------------------------------------------------*/
EGI_IMGBUF  *egi_imgbuf_scale( EGI_IMGBUF *ineimg, int width, int height )
{
	int sm;	 /* Scale multiples */
	int sqsm; /* sm*sm */
	EGI_IMGBUF *outeimg;
	EGI_IMGBUF *tmpimg;
	float factor;
	int i,j, k,n;
	unsigned int index;
	unsigned int color;
	unsigned int R,G,B;
	unsigned int alpha;
	bool alpha_on;

	if(ineimg==NULL)
		return NULL;

	if( width<1 || height<1)
		return NULL;

	if( ineimg->alpha !=NULL )
		alpha_on=true;
	else
		alpha_on=false;

	/* 1. If Scale_down_factor < 2, call egi_imgbuf_resize_nolock(), interpolation method. */
	factor=1.0*ineimg->width/width;
	if( factor < 2.0 ) {
		egi_dpstd("Scale_down_factor < 2, call egi_imgbuf_resize_nolock()...\n");
		return egi_imgbuf_resize_nolock(ineimg, true, width, height);
	}

	/* NOW: Scale_down_factor >= 2 */

	/* Note:
	 * 1. For most case, the scale factor should be an integer! plus some allowance.
	 * 2. Ok, you may take within [0 0.5) instead of 0.3.
	 *    Example: sm=3.4, expect_width=10, while ineimg->width =34.  take intermediate_width=10*3=30.
	 *    From 34 --> 30:  Interpolate within 34 to shrink width to 30, though it's better to interpolate linerly to enlarge width.
	 */
	if( sm - floor(factor) < 0.3 )
		sm = floor(factor);
	else
		sm=floor(factor)+1; /* floor(x): the largest integral value that is not greater than x */

	sqsm=sm*sm;

	egi_dpstd("Scale_down_factor=%d >= 2, scale down to merge pixles...\n", sm);

	/* 2. Create output imgbuf */
	if( alpha_on )
		outeimg = egi_imgbuf_create( height, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced later */
	else /* Alpha_on==false */
		outeimg = egi_imgbuf_createWithoutAlpha( height, width, 0);

	if(outeimg==NULL) {
		egi_dperr("Fail to create outeimg!");
		return NULL;
	}

	/* 3. Resize/enlarge original image to width/height*sm */
	tmpimg=egi_imgbuf_resize_nolock(ineimg, false, width*sm, height*sm); /* !!! Keep_ration MAY NOT keep tmpimg size as (width*sm,height*sm) */
	if(tmpimg==NULL) {
		egi_dperr("Fail to resize image!");
		egi_imgbuf_free(outeimg);
		return NULL;
	}

	/* 4. Merge/map pixel blocks to one pixle */
	for(i=0; i<width; i++) {
		for(j=0; j<height; j++) {
			/* Merge pixle color/alpah */
			R=0; G=0; B=0;
			alpha=0;
			for(k=0; k<sm; k++) {  /* block width */
				for(n=0; n<sm; n++)  { /* block height */
					index = (j*width*sqsm+i*sm) + width*sm*n + k; /* sqsm=sm*sm */
					color = tmpimg->imgbuf[index];
					R += (color&0xF800)>>8;
					G += (color&0x7E0)>>3;
					B += (color&0x1F)<<3;
					if(alpha_on) {
						alpha += tmpimg->alpha[index];
					}
				}
			}
			/* Average R/G/B */
			outeimg->imgbuf[j*width+i]= COLOR_RGB_TO16BITS(R/sqsm, G/sqsm, B/sqsm);
			if(alpha_on)
				outeimg->alpha[j*width+i]=alpha/sqsm;
		}
	}

	/* 5. Free tmpimg */
	egi_imgbuf_free(tmpimg);

	return outeimg;
}



/*--------------------------------------------------------------------
Blur an EGI_IMGBUF by calling  egi_imgbuf_softavg2(). If it succeeds,
the original imgbuf data will be updated then.

Return:
	0  	OK
	<0	Fails
--------------------------------------------------------------------*/
int egi_imgbuf_blur_update(EGI_IMGBUF **pimg, int size, bool alpha_on)
{
	EGI_IMGBUF  *tmpimg=NULL;

	if( pimg==NULL || *pimg==NULL )
		return -1;

	/* Create a blured image by egi_imgbuf_softavg2() */
	tmpimg=egi_imgbuf_avgsoft2(*pimg, size, alpha_on);
	if(tmpimg==NULL)
		return -2;

	/* Free original imgbuf and replaced by tmpimg */
	egi_imgbuf_free(*pimg);
	*pimg=tmpimg;

	return 0;
}

/*--------------------------------------------------------------------
Resize an EGI_IMGBUF by calling  egi_imgbuf_resize(). If it succeeds,
the original imgbuf data will be updated then.

Return:
	0  	OK
	<0	Fails
--------------------------------------------------------------------*/
int egi_imgbuf_resize_update(EGI_IMGBUF **pimg, bool keep_ratio, unsigned int width, unsigned int height)
{
	EGI_IMGBUF  *tmpimg;

	if( pimg==NULL || *pimg==NULL )
		return -1;

	/* If same size */
	if( (*pimg)->width==width && (*pimg)->height==height )
		return 0;

	/* Resize the imgbuf by egi_imgbuf_resize() */
	tmpimg=egi_imgbuf_resize_nolock(*pimg, keep_ratio, width, height);
	if(tmpimg==NULL)
		return -2;

	/* Free original imgbuf and replace it with tmpimg */
	egi_imgbuf_free(*pimg);
	*pimg=tmpimg;

	return 0;
}

/*--------------------------------------------------------------------
Scale an EGI_IMGBUF by calling  egi_imgbuf_scale(). If it succeeds,
the original imgbuf data will be updated then.

@pimg:			Pointer pointer to an EGI_IMGBUF.
@width, height:		Size of image, >0!

Return:
	0  	OK
	<0	Fails
--------------------------------------------------------------------*/
int egi_imgbuf_scale_update(EGI_IMGBUF **pimg, int width, int height)
{
	EGI_IMGBUF  *tmpimg;

	if( pimg==NULL || *pimg==NULL )
		return -1;

	/* If same size */
	if( (*pimg)->width==width && (*pimg)->height==height )
		return 0;

	/* resize the imgbuf by egi_imgbuf_resize() */
	tmpimg=egi_imgbuf_scale(*pimg, width, height);
	if(tmpimg==NULL)
		return -2;

	/* Free original imgbuf and replace it with tmpimg */
	egi_imgbuf_free(*pimg);
	*pimg=tmpimg;

	return 0;
}

/*------------------------------------------------------------------------------
Blend two images of EGI_IMGBUF together, and allocate alpha if eimg->alpha is
NULL.

1. The holding eimg should already have image data inside, or has been initialized
   by egi_imgbuf_init() with certain size of a canvas (height x width) inside.
2. The canvas size of the eimg shall be big enough to hold the bitmap,
   or pixels out of the canvas will be omitted.
3. Size of eimg canvas keeps the same after blending.

@eimg           The EGI_IMGBUF to hold blended image.
@xb,yb          origin of the adding image relative to EGI_IMGBUF canvas coord,
                left top as origin.
@addimg		The adding EGI_IMGBUF.

return:
        0       OK
        <0      fails
--------------------------------------------------------------------------------*/
int egi_imgbuf_blend_imgbuf(EGI_IMGBUF *eimg, int xb, int yb, const EGI_IMGBUF *addimg )
{
        int i,j;
        EGI_16BIT_COLOR color;
        unsigned char alpha;
        unsigned long size; /* alpha size */
        int sumalpha;
        int epos,apos;

        if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
                printf("%s: input holding eimg is NULL or uninitiliazed!\n", __func__);
                return -1;
        }
        if( addimg==NULL || addimg->imgbuf==NULL ) {
                printf("%s: input addimg or its imgbuf is NULL!\n", __func__);
                return -2;
        }

        /* calloc and assign alpha, if NULL */
        if( eimg->alpha==NULL ) {
                size=eimg->height*eimg->width;
                eimg->alpha = calloc(1, size); /* alpha value 8bpp */
                if(eimg->alpha==NULL) {
                        printf("%s: Fail to calloc eimg->alpha\n",__func__);
                        return -3;
                }
                memset(eimg->alpha, 255, size); /* init alpha as 255  */
        }

        for( i=0; i< addimg->height; i++ ) {            /* traverse bitmap height  */
                for( j=0; j< addimg->width; j++ ) {   /* traverse bitmap width */
                        /* check range limit */
                        if( yb+i <0 || yb+i >= eimg->height ||
                                    xb+j <0 || xb+j >= eimg->width )
                                continue;

                        epos=(yb+i)*(eimg->width) + xb+j; /* eimg->imgbuf position */
			apos=i*addimg->width+j;		  /* addimg->imgbuf position */

			/* get color in addimg */
                        color=addimg->imgbuf[apos];

			if(addimg->alpha==NULL)
				alpha=255;
			else
				alpha=addimg->alpha[apos];

                        /* blend color (front,back,alpha) */
                        color=COLOR_16BITS_BLEND( color, eimg->imgbuf[epos], alpha);

			/* assign blended color to imgbuf */
                        eimg->imgbuf[epos]=color;

                        /* blend alpha value */
                        sumalpha=eimg->alpha[epos]+alpha;
                        if( sumalpha > 255 )
				sumalpha=255;
                        eimg->alpha[epos]=sumalpha;
                }
        }

        return 0;
}


/*-------------------------------------------------------------------------------
Create an EGI_IMGBUF by rotating the input eimg.

1. The new imgbuf size(H&W) are made odd, so it has a symmetrical center point as
   the rotating center.
2. Only imgbuf and alpha data are created in new EGI_IMGBUF, other memebers such
   as subimgs are ignored hence.

TODO: more accurate way is to get rotated pixel by interpolation method.

@eimg:		Original imgbuf;
@angle: 	Rotating angle in degree, clockwise as positive.
		Right hand rule: LCD X,Y
Return:
	A pointer to EGI_IMGBUF with new image 		OK
	NULL						Fails
--------------------------------------------------------------------------------*/
EGI_IMGBUF* egi_imgbuf_rotate(EGI_IMGBUF *eimg, int angle)
{
	int i,j;
	int m,n;
	int xr,yr;		/* rotating point */
	int xrl,xrr;		/* left most and right most x of rotating point */
	int width, height;	/* W,H for outimg */
	int wsin,wcos, hsin,hcos;
	int index_in, index_out;
	int ang, asign;
	EGI_IMGBUF *outimg=NULL;

	int xu; //yu=0	/* upper tip point,  */
	int yl;	//xl=0	/* left tip point  */
	int xb,yb;	/* bottom tip pint */
	int map_method;

	/* Check input */
        if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
                printf("%s: input holding eimg is NULL or uninitiliazed!\n", __func__);
                return NULL;
        }

        /* Normalize angle to be within [0-360] */
#if 1
        ang=angle%360;      /* !!! WARING !!!  The modulo result is depended on the Compiler
			     * For C99: a%b=a-(a/b)*b	,whether a is positive or negative.
			     */
        asign= ang >= 0 ? 1 : -1; /* angle sign */
        ang= ang>=0 ? ang : -ang;
	//printf("%s: angle=%d, ang=%d \n", __func__, angle, ang);

#else /* Input angle limited to [0 +/-360) only */
        asign= angle >= 0 ? 1 : -1; /* angle sign */
        angle= angle >= 0 ? angle : -angle ;

	/* now angle >=0  */
	if(angle >= 360 ) ang=0;
	else		ang=angle;

	//printf("%s: angle=%d, ang=%d \n", __func__, asign*angle, ang);
#endif

        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0) {
		printf("%s: Start to create fixed point trigonometric table...\n",__func__);
                mat_create_fpTrigonTab();
	}

        /* Get mutex lock */
        if(pthread_mutex_lock(&eimg->img_mutex) !=0){
                //printf("%s: Fail to lock image mutex!\n",__func__);
                EGI_PLOG(LOGLV_ERROR,"%s: Fail to lock image mutex!", __func__);
                return NULL;
        }

	/* If angle is 0, just copy the original imgbuf */
	if(ang==0) {
		outimg=egi_imgbuf_blockCopy( eimg, 0, 0, eimg->height, eimg->width);
		if(outimg==NULL) {
			printf("%s: Fail to create outimg by blockCopy!\n",__func__);
			pthread_mutex_unlock(&eimg->img_mutex);
			return NULL;
		}

		/* unlock eimg */
		pthread_mutex_unlock(&eimg->img_mutex);
		return outimg;
	}

	/* Get size for rotated imgbuf, which shall cover original eimg at least */
	#if 0	/* Float point method */
	height=0.5+abs(eimg->width*sin(ang/180.0*MATH_PI))+abs(eimg->height*cos(ang/180.0*MATH_PI));
	width=0.5+abs(eimg->width*cos(ang/180.0*MATH_PI))+abs(eimg->height*sin(ang/180.0*MATH_PI));
	/* NEED TO: cal. wsin,hcons, hsin etc. for xu,yu, xl,yl, xb,yb */

	#else	/* Fixed point method */
	wsin=eimg->width*fp16_sin[ang]>>16;
	wsin=wsin>0 ? wsin : -wsin;
	hcos=eimg->height*fp16_cos[ang]>>16;
	hcos=hcos>0 ? hcos : -hcos;
	height=wsin+hcos;

	wcos=eimg->width*fp16_cos[ang]>>16;
	wcos=wcos>0 ? wcos : -wcos;
	hsin=eimg->height*fp16_sin[ang]>>16;
	hsin=hsin>0 ? hsin : -hsin;
	width=wcos+hsin;
	#endif

	/* Make H/W an odd value, then it has a symmetrical center point.   */
	height |= 0x1;
	width |= 0x1;
	if(height<3)height=3;
	if(width<3)width=3;

//	printf("Angle=%d, rotated imgbuf: height=%d, width=%d \n", ang, height, width);

	/* Create an imgbuf accordingly */
	outimg=egi_imgbuf_create( height, width, 0, 0); /* H, W, alpah, color */
	if(outimg==NULL) {
		printf("%s: Fail to create outimg!\n",__func__);
		pthread_mutex_unlock(&eimg->img_mutex);
		return NULL;
	}
	/* Check ALPHA data */
	if(eimg->alpha==NULL) {
		free(outimg->alpha);
		outimg->alpha=NULL;
	}

#if 0 /* TEST: ----- */
	struct timeval tm_start;
	struct timeval tm_end;
	gettimeofday(&tm_start,NULL);
#endif

	/* --- Rotation map and copy --- */

#if 0 /*  MAPPING METHOD 1:    Back map all points in outimg to eimg */
	map_method=1;
	m=height>>1;
	n=width>>1;
	for(i=-m; i<=m; i++) {
		for(j=-n; j<=n; j++) {
			/* Map to original coordiante (xr,yr), Origin at center.
			 * 2D point rotation formula ( a: Right_Hand Rule ):
			 *	x'=x*cos(a)-y*sin(a)
			 *	y'=x*sin(a)+y*cos(a)
			 */
			xr = (j*fp16_cos[ang]+i*asign*fp16_sin[ang])>>16; /* !!! Arithmetic_Right_Shifting */
                        yr = (-j*asign*fp16_sin[ang]+i*fp16_cos[ang])>>16;

			/* Shift Origin to left_top, as of eimg->imgbuf */
			xr += eimg->width>>1;
			yr += eimg->height>>1;

			/* Copy pixel alpha and color */
			if( xr >= 0 && xr < eimg->width && yr >=0 && yr < eimg->height) {
				index_out=width*(i+m)+(j+n);
				index_in=eimg->width*yr+xr;
				outimg->imgbuf[index_out]=eimg->imgbuf[index_in];
				if(eimg->alpha!=NULL)
					outimg->alpha[index_out]=eimg->alpha[index_in];
			}
		}
	}

#else /* MAPPING METHOD 2:   Back map only points which are located at rotated_eimg area of outimg. */
	/* Intend to save time for images with great value of |hegith-width|
	   For a W1024xH1024 png picture, comparing with METHOD 1, it reduces abt...Min.10%... depends on |hegith-width|/Max.(height,width)
	   TODO__ok: outimg is adjusted to have odd number of pixels for each sides, and outimg->H/W >= eimg->H/W,
		 this seems to cause a crease/misplace(of 1 pixel) between right/left halfs as by following algorithm...
	 */
	map_method=2;

	/* Get rotated_eimg tip points coordinates, relative to the outimg coord. all x,y>=0 */
	if(    ( (asign >= 0) && ((0<=ang && ang<90) || (180<=ang && ang<270)) )
	    || ( (asign < 0) && ((90<=ang && ang<180) || (270<=ang && ang<360)) )
	)
	{
		xu=hsin;  /*yu=0;*/
	/* xl=0;*/ 	  yl=hcos;
		xb=wcos;  yb=height-1;
	} else {
		xu=wcos;  /* yu=0; */
	/*xl=0;*/ 	  yl=wsin;
		xb=hsin;  yb=height-1;
	}

	/* Map points in rotated_eimg area back to original eimg */
	for(i=0; i<height; i++) /* traverse outimg->height i */
	{
	   if ( yl==height-1 || yl==0 ) {  /* Rotated image and eimg have same size and position: yl==height-1 or yl==0 */
		xrl=0;
		xrr=(width-1)*(height-1-i)/(height-1);
		//xrr=(eimg->width-1)*(eimg->height-1-i)/(eimg->height-1); /* a crease ...*/
	   }
	   //for(i=0; i<yl; i++)
	   else if( i <= yl ) {
		/* Most left point in triangle of the half rotated eimg, as projected in outimg */
		xrl=xu*(yl-i)/yl;
		xrr=xu+(xb-xu)*i/yb;	/* xu+(xb-xu)*i/(yb-yu) = xu+(xb-xu)*i/yb as yu=0 */
	   }
	   //for(i=yl; i<yb; i++)
	   else {  /* To avoid yb==yl: as divisor never be zero! */
		/* Most left point in triangle of the half rotated eimg, as projected in outimg */
		xrl=xb*(i-yl)/(yb-yl);
		xrr=xu+(xb-xu)*i/yb;	/* xu+(xb-xu)*i/(yb-yu) = xu+(xb-xu)*i/yb as yu=0 */
	   }

	   /* Map all point in the line */
	   for(j=xrl; j<=xrr; j++) {	/* traverse piont on the line */
	   	/*  --- 1. Left part ---  */
		/* Relative to outimg center coord */
		n=j-(width>>1);
		m=i-(height>>1);

	        /* Map to original eimg center. */
        	xr = (n*fp16_cos[ang]+m*asign*fp16_sin[ang])>>16; /* !!! Arithmetic_Right_Shifting */
                yr = (-n*asign*fp16_sin[ang]+m*fp16_cos[ang])>>16;

	        /* Shift Origin to left_top, as of eimg->imgbuf */
        	xr += eimg->width>>1;
                yr += eimg->height>>1;

		/* Copy pixel alpha and color */
		/* outimg: i-rows,j-columns   eimg: yr-row, xr-colums */
		if( xr >= 0 && xr < eimg->width && yr >=0 && yr < eimg->height) {  /* Need to recheck range */
			index_out=width*i+j;
			index_in=eimg->width*yr+xr;
			outimg->imgbuf[index_out]=eimg->imgbuf[index_in];
			if(eimg->alpha!=NULL)
				outimg->alpha[index_out]=eimg->alpha[index_in];
		}

	   	/*  --- 2. Right part --- :  centrally symmetrical to left half. */
		/* Relative to outimg center coord */
		n=-n;
		m=-m;

     		/* Map to original eimg center. */
        	xr = (n*fp16_cos[ang]+m*asign*fp16_sin[ang])>>16; /* !!! Arithmetic_Right_Shifting */
                yr = (-n*asign*fp16_sin[ang]+m*fp16_cos[ang])>>16;

	        /* Shift Origin to left_top, as of eimg->imgbuf */
        	xr += eimg->width>>1;
                yr += eimg->height>>1;

		/* Copy pixel alpha and color */
		/* outimg: i-rows,j-columns   eimg: yr-row, xr-colums */
		if( xr >= 0 && xr < eimg->width && yr >=0 && yr < eimg->height) {  /* Need to recheck range */
			index_out=width*(height-1-i)+(width-1-j); /* 2. Right part : centrally symmetrical point */
			index_in=eimg->width*yr+xr;
			outimg->imgbuf[index_out]=eimg->imgbuf[index_in];
			if(eimg->alpha!=NULL)
				outimg->alpha[index_out]=eimg->alpha[index_in];
		}

	   } /* End for() */

	} /* End: for(i=0; i<yb; i++) */

#endif

#if 0 /* TEST: ----- */
	gettimeofday(&tm_end,NULL);
	printf("%s: Method_%d: %ldus\n",__func__, map_method, tm_diffus(tm_start, tm_end));
#endif

	/* unlock eimg */
	pthread_mutex_unlock(&eimg->img_mutex);

	return outimg;
}


/*-----------------------------------------------------------------------------------------
Copy a block from eimg, the rectangular area of the block is not up right, but
with a certain angle, as relative to the original eimg.

1. The new imgbuf size(H&W) are made odd, so it has a symmetrical center point.
2. Only imgbuf and alpha data are created/copied in new EGI_IMGBUF, other memebers such
   as subimgs are ignored hence.
3. Only eimg is mutex_locked, oimg is NOT mutex_locked.

TODO:
1. more accurate way is to get rotated pixel by interpolation method.
2. Correct mutex_lock operation.

@eimg:		  Original imgbuf;
@oimg:	 	  1. If oimg!=NULL, then use oimg to store color/alpha data.
		     input height,width will be ingored then.
		  2.Else if oimg==NULL, then create outimg with input params.
		    !!! DO NOT re_assign it with function return:
		     oimg=egi_imgbuf_rotBlockCopy(eimg, oimg, height, width, px,py, angle);
		     It may return a NULL pointer !!!
		  3.If oimg->alpha is NULL, then alpha will NOT be copied.
		  4.!!! NOTE !!! oimg->height and oimg->width to be 2*n+1.
@px,py:		  Coordinate of the center of outimg relative to eimg coord.
@width, height:   Width and height of the outimg, to be adjusted to 2*n+1.
@angle: 	  Angle of outimg_X axis relative to eimg_X axis, in degree,
		  clockwise as positive.	Right hand rule.

Return:
	A pointer to EGI_IMGBUF with block_copied image 	OK
	NULL							Fails
--------------------------------------------------------------------------------------------*/
EGI_IMGBUF* egi_imgbuf_rotBlockCopy( EGI_IMGBUF *eimg, EGI_IMGBUF *oimg, int height, int width,
					int px, int py, int angle)
{
	int i,j;
	int ang, asign;
	int xr,yr;		/* rotating point */
	int index_in, index_out;
	EGI_IMGBUF	*outimg=NULL;

        /* Normalize angle to be within [0-360] */
        ang=angle%360;      /* !!! WARING !!!  The modulo result is depended on the Compiler
			     * For C99: a%b=a-(a/b)*b ,whatever a is positive or negative.
			     */
        asign= ang >= 0 ? 1 : -1; /* angle sign */
        ang= ang>=0 ? ang : -ang;
	//printf("%s: angle=%d, ang=%d \n", __func__, angle, ang);

	/* Check input eimg */
        if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
                printf("%s: input holding eimg is NULL or uninitiliazed!\n", __func__);
                return NULL;
        }

	/* Check input oimg */
	if( oimg!=NULL && (oimg->imgbuf==NULL || oimg->height<3 || oimg->width<3 || (oimg->height&0x1)==0 || (oimg->width&0x1)==0 ) ) {
		printf("%s input oimg is invalid, It may be W,H<3, or W,H is NOT odd number! \n", __func__);
		return NULL;  /* NOTE */
	}


        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0) {
		printf("%s: Start to create fixed point trigonometric table...\n",__func__);
                mat_create_fpTrigonTab();
		egi_dpstd("Finish mat_create_fpTrigonTab().\n");
	}

	/* Input oimg is NULL */
   	if(oimg==NULL) {
		/* Make H/W an odd value, then it has a symmetrical center point. */
		height |= 0x1;
		width |= 0x1;
		if(height<3)height=3;
		if(width<3)width=3;

		/* Create an imgbuf accordingly */
		outimg=egi_imgbuf_create( height, width, 0, 0); /* H, W, alpah, color */
		if(outimg==NULL) {
			printf("%s: Fail to create outimg!\n",__func__);
			pthread_mutex_unlock(&eimg->img_mutex);
			return NULL;
		}
		/* Check ALPHA data */
		if(eimg->alpha==NULL) {
			free(outimg->alpha);
			outimg->alpha=NULL;
		}
   	}
	/* Input oimg is NOT NULL */
	else {
		height=oimg->height;
		width=oimg->width;
		outimg=oimg;
	}

        /* Get mutex lock */
        if(pthread_mutex_lock(&eimg->img_mutex) !=0){
                EGI_PLOG(LOGLV_ERROR,"%s: Fail to lock image mutex!", __func__);
		return NULL; /* NOTE */
        }

	/* Clear outimg first */
	egi_imgbuf_resetColorAlpha(outimg, WEGI_COLOR_GRAY2, outimg->alpha==NULL ? -1:255 );  /* img, color, alpha */

	int m=height>>1;
	int n=width>>1;
	/* Map back point coordinates to eimg */
	for(i=-m; i<=m; i++) {
		for(j=-n; j<=n; j++) {
			/* Map to original coordiante (xr,yr), Origin at center.
			 * 2D point rotation formula ( a positive: Right_hand Rule. ):
			 *	x'=x*cos(a)-y*sin(a)
			 *	y'=x*sin(a)+y*cos(a)
			 *   Coord axis anti_clockwise, point colokwise rotate.
			 *   points coordinates relative to up_right block coord.
			 */
			xr = (j*fp16_cos[ang]-i*asign*fp16_sin[ang])>>16; /* !!! Arithmetic_Right_Shifting */
                        yr = (j*asign*fp16_sin[ang]+i*fp16_cos[ang])>>16;

			/* Shift Origin to left_top, as of eimg->imgbuf */
			xr += px;
			yr += py;

			/* Copy pixel alpha and color */
			if( xr >= 0 && xr < eimg->width && yr >=0 && yr < eimg->height) {
				index_out=width*(i+m)+(j+n);
				index_in=eimg->width*yr+xr;
				outimg->imgbuf[index_out]=eimg->imgbuf[index_in];
				if(eimg->alpha!=NULL && outimg->alpha!=NULL)	/* outimg is oimg NOW if not NULL */
					outimg->alpha[index_out]=eimg->alpha[index_in];
			}
		}
	}

	/* unlock eimg */
	pthread_mutex_unlock(&eimg->img_mutex);

	return outimg;
}
/*------------------------------------------------------------------------------
Float type version of egi_imgbuf_rotBlockCopy(), with 4_pixels interpolation.
------------------------------------------------------------------------------*/
EGI_IMGBUF* egi_imgbuf_rotBlockCopy2( EGI_IMGBUF *eimg, EGI_IMGBUF *oimg, int height, int width,
					int px, int py, float angle)
{
	int i,j;
	float xr,yr;		/* rotating point */
	int index_out;
	EGI_IMGBUF	*outimg=NULL;

	float sina=sin(MATH_PI*angle/180);
	float cosa=cos(MATH_PI*angle/180);

	/* Check input eimg */
        if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
                egi_dpstd("Input holding eimg is NULL or uninitiliazed!\n");
                return NULL;
        }

	/* Check input oimg */
	if( oimg!=NULL && (oimg->imgbuf==NULL || oimg->height<3 || oimg->width<3 || (oimg->height&0x1)==0 || (oimg->width&0x1)==0 ) ) {
		egi_dpstd("Input oimg is invalid, It may be W,H<3, or W,H is NOT odd number!\n");
		return NULL;  /* NOTE */
	}

	/* Input oimg is NULL */
   	if(oimg==NULL) {
		/* Make H/W an odd value, then it has a symmetrical center point. */
		height |= 0x1;
		width |= 0x1;
		if(height<3)height=3;
		if(width<3)width=3;

		/* Create an imgbuf accordingly */
		outimg=egi_imgbuf_create(height, width, 0, 0); /* H, W, alpah, color */
		if(outimg==NULL) {
			egi_dpstd("Fail to create outimg!\n");
			//pthread_mutex_unlock(&eimg->img_mutex);
			return NULL;
		}
		/* Check ALPHA data */
		if(eimg->alpha==NULL) {
			free(outimg->alpha);
			outimg->alpha=NULL;
		}
   	}
	/* Input oimg is NOT NULL */
	else {
		height=oimg->height;
		width=oimg->width;
		outimg=oimg;
	}

        /* Get mutex lock */
        if(pthread_mutex_lock(&eimg->img_mutex) !=0){
                EGI_PLOG(LOGLV_ERROR,"%s: Fail to lock image mutex!", __func__);
		return NULL; /* NOTE */
        }

	/* Clear outimg first. ALPHA to be 0, OR same as eimg.  */
	egi_imgbuf_resetColorAlpha(outimg, WEGI_COLOR_GRAY2, outimg->alpha==NULL ? -1:0 );  /* img, color, alpha */

	/* Map back point coordinates to eimg */
	int m=height>>1;
	int n=width>>1;
	for(i=-m; i<=m; i++) {
		for(j=-n; j<=n; j++) {
			/* Map to original coordiante (xr,yr), Origin at center.
			 * 2D point rotation formula ( a positive: Right_hand Rule. ):
			 *	x'=x*cos(a)-y*sin(a)
			 *	y'=x*sin(a)+y*cos(a)
			 *   Coord axis anti_clockwise, point colokwise rotate.
			 *   points coordinates relative to up_right block coord.
			 */
			xr=cosa*j-sina*i;
			yr=sina*j+cosa*i;

			/* Shift Origin to left_top, as of eimg->imgbuf */
			xr += px;
			yr += py;

			/* Copy pixel alpha and color.  Limit xr,yr, ignore if they are out of original imgbuf area. */
			if( xr >= 0.0 && xr <= eimg->width-1 && yr >=0.0 && yr <= eimg->height-1) {
				index_out=width*(i+m)+(j+n);

				int indx1 = eimg->width*floorf(yr)+floorf(xr);
				int indx2 = eimg->width*floorf(yr)+ceilf(xr);
				int indx3 = eimg->width*ceilf(yr)+floorf(xr);
				int indx4 = eimg->width*ceilf(yr)+ceilf(xr);
				EGI_16BIT_COLOR  pcolor;
				EGI_8BIT_ALPHA  palpha;
				float pft;

				/* Interpolate within 4 pixles */
				if(eimg->alpha!=NULL && outimg->alpha!=NULL) {
				    egi_16bitColor_interplt4p(eimg->imgbuf[indx1], eimg->imgbuf[indx2],  /* color1, color2 */
							eimg->imgbuf[indx3], eimg->imgbuf[indx4],	/* color3, color4 */
							eimg->alpha[indx1], eimg->alpha[indx2],		/* alpha1, alpha2 */
							eimg->alpha[indx3], eimg->alpha[indx4],		/* alpha3, alpah4 */
							//(xr-floorf(xr))*(1<<15), (yr-floorf(yr))*(1<<15),  /* f15_x, f15_y */
							modff(xr,&pft)*(1<<15), modff(yr, &pft)*(1<<15),  /* f15_x, f15_y */
							&pcolor, &palpha );

				   outimg->imgbuf[index_out]=pcolor;
				   outimg->alpha[index_out]=palpha;
				}
				else {
				    egi_16bitColor_interplt4p(eimg->imgbuf[indx1], eimg->imgbuf[indx2],  /* color1, color2 */
							eimg->imgbuf[indx3], eimg->imgbuf[indx4],	/* color3, color4 */
							0, 0, 0, 0,	/* alpha1, alpha2, alpha3, alpha4 */
							//(xr-floorf(xr))*(1<<15), (yr-floorf(yr))*(1<<15),  /* f15_x, f15_y */
							modff(xr,&pft)*(1<<15), modff(yr, &pft)*(1<<15),  /* f15_x, f15_y */
							&pcolor, NULL );

				   outimg->imgbuf[index_out]=pcolor;
				}
			}
		}
	}

	/* unlock eimg */
	pthread_mutex_unlock(&eimg->img_mutex);

	return outimg;
}


/*-------------------------------------------------------------------------
Rotate the input eimg, then substitue original imgbuf with the rotated one.
By calling egi_imgbuf_rotate()

Note:
1. Only imgbuf and alpha data rotated, other memebers such as subimgs are
   are ignored hence.

@eimg:		PP. to an EGI_IMGBUF for operation;
@angle: 	Rotating angle in degree, positive as clockwise.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------------*/
int egi_imgbuf_rotate_update(EGI_IMGBUF **eimg, int angle)
{
	EGI_IMGBUF *tmpimg=NULL;

	if(eimg==NULL || *eimg==NULL)
		return -1;

	/* If same position */
	if(angle%360==0)
		return 0;

	/* Get rotated imgbuf */
	tmpimg=egi_imgbuf_rotate(*eimg, angle);
	if(tmpimg==NULL)
		return -2;

	/* Free original imgbuf and replaced by tmpimg */
	egi_imgbuf_free(*eimg);
	*eimg=tmpimg;

#if 0
	/* Substitue original imgbuf and alpha */
	eimg->height=tmpimg->height;
	eimg->width=tmpimg->width;

	free(eimg->imgbuf);
	eimg->imgbuf=tmpimg->imgbuf;
	tmpimg->imgbuf=NULL;

	free(eimg->alpha);
	eimg->alpha=tmpimg->alpha;
	tmpimg->alpha=NULL;

	/* Free tmp. imgbuf */
	egi_imgbuf_free(tmpimg);
#endif

	return 0;
}

/*--------------------------------------------------------------------------------------
Display image in a defined window.
For 16bits color only!!!!

Note:
1. Advantage: call draw_dot(), it is effective for FILO,
2. Advantage: draw_dot() will ensure x,y range within screen.
3. Write image data of an EGI_IMGBUF to a window in FB.
4. Set outside color as black.
5. window(xw, yw) defines a looking window to the original picture, (xp,yp) is the left_top
   start point of the window. If the looking window covers area ouside of the picture,then
   those area will be filled with BLACK.

6. ??? Carefully defining (xp,yp),(xw,yw) and (winw,winh)to narrow down displaying area MAY
       improve running speed considerably when you deal with a big picture ???

@egi_imgbuf:    an EGI_IMGBUF struct which hold bits_color image data of a picture.
@fb_dev:	FB device
@subcolor:	substituting color, only applicable when >0.
@(xp,yp):       Displaying image block origin(left top) point coordinate, relative to
                the coord system of the image(also origin at left top).
@(xw,yw):       displaying window origin, relate to the LCD coord system.
@(winw,winh):   width and height(row/column for fb) of the displaying window.
                !!! Note: You'd better set winw,winh not exceeds acutual LCD size, or it will
                waste time calling draw_dot() for pixels outsie FB zone. --- OK


Return:
		0	OK
		<0	fails
------------------------------------------------------------------------------------------*/
int egi_imgbuf_windisplay( EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int subcolor,
			   		int xp, int yp, int xw, int yw, int winw, int winh)
{
        /* check data */
	if(fb_dev==NULL)
		return -1;
        if(egi_imgbuf == NULL) {
                printf("%s: egi_imgbuf is NULL. fail to display.\n",__func__);
                return -1;
        }
        if(egi_imgbuf->imgbuf == NULL) {
                printf("%s: egi_imgbuf->imgbuf is NULL. fail to display.\n",__func__);
                return -1;
        }

	/* get mutex lock */
//	printf("%s: Start lock image mutext...\n",__func__);
	if(pthread_mutex_lock(&egi_imgbuf->img_mutex) !=0){
		//printf("%s: Fail to lock image mutex!\n",__func__);
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to lock image mutex!", __func__);
		return -1;
	}

        int imgw=egi_imgbuf->width;     /* image Width and Height */
        int imgh=egi_imgbuf->height;

        if( imgw<=0 || imgh<=0 )
        {
		pthread_mutex_unlock(&egi_imgbuf->img_mutex);
                printf("%s: egi_imgbuf->width or height is <=0. fail to display.\n",__func__);
		EGI_PLOG(LOGLV_ERROR,"%s: imgbuf width or height is <=0!", __func__);
                return -2;
        }
//	printf("%s: height=%d, width=%d \n",__func__, imgh,imgw);

        int i,j;
        int xres;
        int yres;
//        long int screen_pixels;

//        unsigned char *fbp=fb_dev->map_fb;
        uint16_t *imgbuf=egi_imgbuf->imgbuf;
        unsigned char *alpha=egi_imgbuf->alpha;
        long int locimg=0; /* location of image buf, in pixel, xxxxin byte */

  /* If FB position rotate 90deg or 270deg, swap xres and yres */
  if(fb_dev->pos_rotate & 0x1) {	/* Landscape mode */
	xres=fb_dev->vinfo.yres;
	yres=fb_dev->vinfo.xres;
  }
  else {				/* Portrait mode */
	xres=fb_dev->vinfo.xres;
	yres=fb_dev->vinfo.yres;
  }

   /* pixel total number */
//   screen_pixels=xres*yres;

  /* reset winh and winw */
//  if( winh > yres) winh=yres;
//  if( winw > xres) winw=xres;

  /* if no alpha channle*/
  if( egi_imgbuf->alpha==NULL )
  {
        for(i=0; i<winh; i++) {  /* row of the displaying window */
                for(j=0; j<winw; j++) {

			/* Rule out points which are out of the SCREEN boundary */
			if( i+yw<0 || i+yw>yres-1 || j+xw<0 || j+xw>xres-1 )
				continue;

                        /* FB data location */
//                        locfb = (i+yw)*xres+(j+xw);

                     /*  ---- draw only within screen  ---- */
//                     if( locfb>=0 && locfb <= (screen_pixels-1) ) {

                        /* check if exceeds IMAGE(not screen) boundary */
                        if( ( xp+j > imgw-1 || xp+j <0 ) || ( yp+i > imgh-1 || yp+i <0 ) )
                        {
//replaced by draw_dot()        *(uint16_t *)(fbp+locfb)=0; /* black for outside */

				#if 0 /* 0- DO NOT draw outside pixels, for it will affect FB FILO */
                                fbset_color2(fb_dev,0);     /* black for outside */
                                draw_dot(fb_dev,j+xw,i+yw); /* call draw_dot */
				#endif
                        }
                        else {
                                /* image data location */
                                locimg= (i+yp)*imgw+(j+xp);

				if(subcolor<0) {
	                                fbset_color2(fb_dev,imgbuf[locimg]);
				}
				else {  /* use subcolor */
	                                fbset_color2(fb_dev,(uint16_t)subcolor);
				}

                                draw_dot(fb_dev,j+xw,i+yw); /* call draw_dot */
                         }

//                     } /* --- if within screen --- */

                }
        }
  }
  else /* with alpha channel */
  {
	//printf("----- alpha ON -----\n");
        for(i=0;i<winh;i++)  { /* row of the displaying window */
                for(j=0;j<winw;j++)  {

			/* Rule out points which are out of the SCREEN boundary */
			if( i+yw<0 || i+yw>yres-1 || j+xw<0 || j+xw>xres-1 )
				continue;

                        /* FB data location, in pixel */
//                        locfb = (i+yw)*xres+(j+xw); /*in pixel,  2 bytes per pixel */

                     /*  ---- draw_dot() only within screen  ---- */
//                     if(  locfb>=0 && locfb<screen_pixels ) {

                        /* check if exceeds IMAGE(not screen) boundary */
                        if( ( xp+j > imgw-1 || xp+j <0 ) || ( yp+i > imgh-1 || yp+i <0 ) )
                        {
				#if 0 /* 0- DO NOT draw outside pixels, for it will affect FB FILO */
                                fbset_color2(fb_dev,0); /* black for outside */
                                draw_dot(fb_dev,j+xw,i+yw); /* call draw_dot */
				#endif
                        }
                        else {
                                /* image data location, 2 bytes per pixel */
                                locimg= (i+yp)*imgw+(j+xp);

                                if( alpha[locimg]==0 ) {   /* ---- 100% backgroud color ---- */
                                        /* Transparent for background, do nothing */
                                        //fbset_color2(fb_dev,*(uint16_t *)(fbp+(locfb<<1)));

				    	/* for Virt FB ???? */
                                }

				else if(subcolor<0) {	/* ---- No subcolor ---- */
#if 0  ///////////////////////////// replaced by fb.pxialpha ///////////////////////
                                     if(alpha[locimg]==255) {    /* 100% front color */
                                          fbset_color2(fb_dev,*(uint16_t *)(imgbuf+locimg));
				     }
                                     else {                           /* blend */
                                          fbset_color2(fb_dev,
                                              COLOR_16BITS_BLEND( *(uint16_t *)(imgbuf+locimg),   /* front pixel */
                                                            *(uint16_t *)(fbp+(locfb<<1)),  /* background */
                                                             alpha[locimg]  )               /* alpha value */
                                             );
                                     }
#endif  ///////////////////////////////////////////////////////////////////////////
				     fb_dev->pixalpha=alpha[locimg];
				     fbset_color2(fb_dev,imgbuf[locimg]);
                                     draw_dot(fb_dev,j+xw,i+yw);
				}

				else  {  		/* ---- use subcolor ----- */
#if 0  ///////////////////////////// replaced by fb.pxialpha ////////////////////////
                                    if(alpha[locimg]==255) {    /* 100% subcolor as front color */
                                          fbset_color2(fb_dev,subcolor);
				     }
                                     else {                           /* blend */
                                          fbset_color2(fb_dev,
                                              COLOR_16BITS_BLEND( subcolor,   /* subcolor as front pixel */
                                                            *(uint16_t *)(fbp+(locfb<<1)),  /* background */
                                                             alpha[locimg]  )               /* alpha value */
                                             );
                                     }
#endif  //////////////////////////////////////////////////////////////////////////
				     fb_dev->pixalpha=alpha[locimg];
				     fbset_color2(fb_dev,subcolor);
                                     draw_dot(fb_dev,j+xw,i+yw);
				}

                            }

//                        }  /* --- if within screen --- */

                } /* for() */
        }/* for()  */
  }/* end alpha case */

  /* put mutex lock */
  pthread_mutex_unlock(&egi_imgbuf->img_mutex);

  return 0;
}


#if 1 ////////////////////////// TODO: range limit check ///////////////////////////
/*---------------------------------------------------------------------------------------
Display image in a defined window.
For 16bits color only!!!! and NO pos_rotate remap!

WARING:
1. Writing directly to FB without calling draw_dot()!!!
   FB_FILO, Virt_FB, Pos_rotate Remap all disabled!!!
2. Take care of image boudary check and locfb check to avoid outrange points skipping
   to next line !!!!
3. No range limit check, which may cause segmentation fault!!!

Note:
1. No subcolor and write directly to FB, so FB FILO is ineffective !!!!!
2. FB.pos_rotate is NOT supported.
3. Write image data of an EGI_IMGBUF to a window in FB.
4. Set outside color as black.
5. window(xw, yw) defines a looking window to the original picture, (xp,yp) is the left_top
   start point of the window. If the looking window covers area ouside of the picture,then
   those area will be filled with BLACK.

egi_imgbuf:     an EGI_IMGBUF struct which hold bits_color image data of a picture.
fb_dev:		FB device
(xp,yp):        coodinate of the displaying window origin(left top) point, relative to
                the coordinate system of the picture(also origin at left top).
(xw,yw):        displaying window origin, relate to the LCD coord system.
winw,winh:      width and height(row/column for fb) of the displaying window.
                !!! Note: You'd better set winw,winh not exceeds acutual LCD size, or it will
                waste time calling draw_dot() for pixels outsie FB zone.

Return:
		0	OK
		<0	fails
------------------------------------------------------------------------------------------*/
int egi_imgbuf_windisplay2(EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev,
			   		int xp, int yp, int xw, int yw, int winw, int winh)
{
        /* Check data */
	if(fb_dev == NULL)
		return -1;
        if(egi_imgbuf == NULL || egi_imgbuf->imgbuf == NULL )
        {
                printf("%s: egi_imgbuf is NULL. fail to display.\n",__func__);
                return -1;
        }

	/* Get mutex lock */
	if( pthread_mutex_lock(&egi_imgbuf->img_mutex)!=0 ){
		printf("%s: Fail to lock image mutex!\n",__func__);
		return -2;
	}

        int imgw=egi_imgbuf->width;     /* image Width and Height */
        int imgh=egi_imgbuf->height;
        if( imgw<0 || imgh<0 )
        {
                printf("%s: egi_imgbuf->width or height is negative. fail to display.\n",__func__);
		pthread_mutex_unlock(&egi_imgbuf->img_mutex);
                return -3;
        }

        int i,j;
        int xres=fb_dev->vinfo.xres;
        int yres=fb_dev->vinfo.yres;

	#ifdef ENABLE_BACK_BUFFER
        unsigned char *fbp =fb_dev->map_bk;
	#else
        unsigned char *fbp =fb_dev->map_fb;
	#endif

        uint16_t *imgbuf = egi_imgbuf->imgbuf;
        unsigned char *alpha=egi_imgbuf->alpha;
	long unsigned int location=0;
        long int locimg=0; /* location of image buf, in pixel, xxxxin byte */
	uint32_t rgb;  /* LETS_NOTE */


  /* If no alpha channle*/
  if( egi_imgbuf->alpha==NULL )
  {
        for(i=0;i<winh;i++) {  /* row of the displaying window */
                for(j=0;j<winw;j++) {

			/* Rule out points which are out of the SCREEN boundary */
			if( i+yw<0 || i+yw>yres-1 || j+xw<0 || j+xw>xres-1 )
				continue;

                        /* FB data location */
        		location=(j+xw+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel>>3)+
                     		 (i+yw+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

                        /* Check if exceeds image boundary */
                        if( ( xp+j > imgw-1 || xp+j <0 ) || ( yp+i > imgh-1 || yp+i <0 ) )
                        {
				#ifdef LETS_NOTE /*--- 4 bytes per pixel ---*/
				// *(uint32_t *)(fbp+location)=0;  /* back for outside */
				#else		/*--- 2 bytes per pixel ---*/
			        // *(uint16_t *)(fbp+location)=0; /* black for outside */
				#endif
                        }
                        else {
                                /* image data location */
                                locimg= (i+yp)*imgw+(j+xp);
				#ifdef LETS_NOTE /*--- 4 bytes per pixel ---*/
				 *(uint32_t *)(fbp+location)=COLOR_16TO24BITS(imgbuf[locimg])+(255<<24);
				#else		/*--- 2 bytes per pixel ---*/
			         *(uint16_t *)(fbp+location)=*(uint16_t *)(imgbuf+locimg);
				#endif

                       }
                }
        }
  }
  else /* with alpha channel */
  {
        for(i=0;i<winh;i++)  { /* row of the displaying window */
                for(j=0;j<winw;j++)  {

			/* Rule out points which are out of the SCREEN boundary */
			if( i+yw<0 || i+yw>yres-1 || j+xw<0 || j+xw>xres-1 )
				continue;

                        /* FB data location, in pixel */
        		location=(j+xw+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel>>3)+
                     		 (i+yw+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

                        /* Check if exceeds image boundary */
                        if( ( xp+j > imgw-1 || xp+j <0 ) || ( yp+i > imgh-1 || yp+i <0 ) )
                        {
				#ifdef LETS_NOTE /*--- 4 bytes per pixel ---*/
				// *(uint32_t *)(fbp+location)=0;   /* black for outside */
				#else		/*--- 2 bytes per pixel ---*/
       				// *(uint16_t *)(fbp+location)=0;   /* black for outside */
				#endif
                        }
                        else {
                            /* Image data location, 2 bytes per pixel */
                            locimg= (i+yp)*imgw+(j+xp);

                            /*  ---- Draw only within screen  ---- */
			    if( location < fb_dev->screensize ) {
                                if(alpha[locimg]==0) {           /* use backgroud color */
                                        /* Transparent for background, do nothing */
                                }

			     #ifdef LETS_NOTE   /* ------- 4 bytes per pixel ------ */
                                else if(alpha[locimg]==255) {    /* use front color */
	         		       *(uint32_t *)(fbp+location)=COLOR_16TO24BITS(imgbuf[locimg]) \
												+(255<<24);
				}
                                else {                           /* Blend front,back color */
				     #if 1
                        		rgb=(*((uint32_t *)(fbp+location)))&0xFFFFFF; /* Get back color */
		                        /* BLEND:  front, back, alpha */
                		        rgb=COLOR_24BITS_BLEND(COLOR_16TO24BITS(imgbuf[locimg]), rgb, alpha[locimg]);
                        		*((uint32_t *)(fbp+location))=rgb+(255<<24);
				     #else /* NOT effective */
			    	        *(uint32_t *)(fbp+location)=COLOR_16TO24BITS(imgbuf[locimg]) +(alpha[locimg]<<24);
				     #endif
				}

			     #else 		/* ------ 2 bytes per pixel ------- */
                                else if(alpha[locimg]==255) {    /* use front color */
			               *(uint16_t *)(fbp+location)=*(uint16_t *)(imgbuf+locimg);
				}
                                else {                           /* blend */
                                             *(uint16_t *)(fbp+location)= COLOR_16BITS_BLEND(
							*(uint16_t *)(imgbuf+locimg),   /* front pixel */
                                                        *(uint16_t *)(fbp+location),  /* background */
                                                        alpha[locimg]  );               /* alpha value */
				}

			    #endif /*  <<< end 4/2 Bpp select  >>> */
                            }
                       } /* if  within screen */
                } /* for() */
        }/* for()  */
  }/* end alpha case */

  /* Put mutex lock */
  pthread_mutex_unlock(&egi_imgbuf->img_mutex);

  return 0;
}
#endif ///////////////////////////////////////////////////////////////////////


/*---------------------------------------------------------------------------------------
1. Create a rotated imgbuf with refrence to original input imgbuf.
2. The rotated image will be displayed on the LCD where its center (xri',yri') coincides
   with (xrl, yrl) of LCD.
3. The rotated EGI_IMGBUF will be freed after displaying.


egi_imgbuf:     an EGI_IMGBUF struct which hold bits_color image data of a picture.
fb_dev:		FB device
@angle: 	Rotating angle in degree, clockwise as positive.
		Right hand rule: LCD X,Y
@xri,yri:	i-image, Rotating center coordiantes, relative to imgbuf coord.
@xrl,yrl:	l-lcd, Rotating center coordiantes, relative to LCD coord.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------------------*/
int egi_image_rotdisplay( EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int angle,
	                                        	int xri, int yri, int xrl, int yrl)
{
	EGI_IMGBUF *rotimg=NULL;
	int ang, asign;
	int xr, yr;	  /* image center under different coord./origin  */
	int xnri,ynri;	  /* rotate image center under rotated image coord. */
	int ret;

	#if 0 /* Ok, egi_imgbuf_rotate() will check input imgbuf */
        if(egi_imgbuf == NULL || egi_imgbuf->imgbuf == NULL )
        {
                printf("%s: egi_imgbuf is NULL. fail to display.\n",__func__);
                return -1;
        }
	#endif


        /* Normalize angle to be within [0-360] */
#if 1
        ang=angle%360;      /* !!! WARING !!!  The modulo result is depended on the Compiler
                             * For C99: a%b=a-(a/b)*b   ,whether a is positive or negative.
                             */
        asign= ang >= 0 ? 1 : -1; /* angle sign */
        ang= ang>=0 ? ang : -ang ;

#else /* Input limited to [0 360) */
        asign= (angle >= 0 ? 1 : -1); /* angle sign */
        angle= (angle >= 0 ? angle : -angle);

	/* now angle >=0  */
	if(angle >= 360 ) ang=0;
	else		ang=angle;

	printf("%s: angle=%d, ang=%d \n", __func__, angle, ang);
#endif


        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0) {
		printf("%s: Start to create fixed point trigonometric table...\n",__func__);
                mat_create_fpTrigonTab();
	}

        /* check data */
	if( fb_dev == NULL )
		return -1;

	/* Get rotated image */
	rotimg=egi_imgbuf_rotate(egi_imgbuf, angle);  /* egi_imgbuf_rotate() mutex_lock applied */
	if(rotimg==NULL)
		return -2;

	/* egi_imgbuf_rotate() around image center, so image center is fixed,
	 * next get new coordinates of (xri, yri) under rotimg center coord.
	 * (xr,yr) is under coord. whose origin is at original image center. with opposite Y direction as of LCD's
	 * 2D point rotation formula ( a: counter_clockwise as positive ):
	 *	x'=x*cos(a)-y*sin(a)
	 *	y'=x*sin(a)+y*cos(a)
	 */
	xr = ( (xri-(egi_imgbuf->width>>1))*fp16_cos[ang]+ ((egi_imgbuf->height>>1)-yri)*asign*fp16_sin[ang])>>16; /* !!! Arithmetic_Right_Shifting */
        yr = (-(xri-(egi_imgbuf->width>>1))*asign*fp16_sin[ang]+((egi_imgbuf->height>>1)-yri)*fp16_cos[ang])>>16;

	/* Get coordinates of (xri,yri) under rotimg left_top coord. */
	xnri=(rotimg->width>>1)+xr;
	ynri=(rotimg->height>>1)-yr;

	/* Display rotated image to let point (xnic,ynic) be positioned at LCD (xrl, yrl)  */
	ret=egi_subimg_writeFB(	rotimg, fb_dev, 0, -1,  xrl-xnri, yrl-ynri );

	/* Free rotimg */
	egi_imgbuf_free(rotimg);

	return ret;
}


/*-----------------------------------------------------------------------------------
Write a sub image in the EGI_IMGBUF to FB.

egi_imgbuf:     an EGI_IMGBUF struct which hold bits_color image data of a picture.
fb_dev:		FB device
subindex:	index number of the sub image.
		if subindex<0 or EGI_IMGBOX subimgs==NULL, no sub_image defined in the
		egi_imgbuf.
subcolor:	substituting color, only applicable when >0.
(x0,y0):        displaying window origin, relate to the LCD coord system.

Return:
		0	OK
		<0	fails
-------------------------------------------------------------------------------------*/
int egi_subimg_writeFB(EGI_IMGBUF *egi_imgbuf, FBDEV *fb_dev, int subindex,
							int subcolor, int x0,	int y0)
{
	int ret;
	int xp,yp;
	int w,h;

	if(fb_dev==NULL)
		return -1;

	if(egi_imgbuf==NULL || egi_imgbuf->imgbuf==NULL ) {
		printf("%s: egi_imbuf or egi_imgbuf->imgbuf is NULL!\n",__func__);
		return -1;
	}
	/* get mutex lock */
	if( pthread_mutex_lock(&egi_imgbuf->img_mutex)!=0 ){
		printf("%s: Fail to lock image mutex!\n",__func__);
		return -2;
	}

	if(subindex > egi_imgbuf->submax) {
		printf("%s: EGI_IMGBUF subindex is out of range!\n",__func__);
	  	pthread_mutex_unlock(&egi_imgbuf->img_mutex);
		return -4;
	}

	/* get position and size of the subimage */
	if( subindex<0 || egi_imgbuf->subimgs==NULL ) {	/* No subimg, only one image */
		xp=0;
		yp=0;
		w=egi_imgbuf->width;
		h=egi_imgbuf->height;
	}
	else {				/* otherwise, get subimg size and location in image data */
		xp=egi_imgbuf->subimgs[subindex].x0;
		yp=egi_imgbuf->subimgs[subindex].y0;
		w=egi_imgbuf->subimgs[subindex].w;
		h=egi_imgbuf->subimgs[subindex].h;
	}

  	/* put mutex lock, before windisplay()!!! */
  	pthread_mutex_unlock(&egi_imgbuf->img_mutex);

	/* !!!! egi_imgbuf_windisplay() will get/put image mutex by itself */
	ret=egi_imgbuf_windisplay(egi_imgbuf, fb_dev, subcolor, xp, yp, x0, y0, w, h); /* with mutex lock */

	return ret;
}

/*---------------------------------------------------------------------------------
Write a sub_images in the EGI_IMGBUF to FB one by one, with delayms for each image.

egi_imgbuf:     an EGI_IMGBUF struct which hold bits_color image data of a picture.
fb_dev:		FB device
(x0,y0):        displaying window origin, relate to the LCD coord system.
delayms:	Delay ms for each subimage.
		If <0, apply egi_imgbuf->delayms.

Return:
		0	OK
		<0	fails
---------------------------------------------------------------------------------*/
void egi_subimg_serialWriteFB(FBDEV *fb_dev, EGI_IMGBUF *egi_imgbuf, int x0, int y0, int delayms)
{
      	int i;

	/* egi_subimg_writeFB() will check fb_dev, egi_imgbuf AND subindex */
	if(egi_imgbuf==NULL)
		return;

	for(i=0; i < egi_imgbuf->submax +1; i++) {
		egi_subimg_writeFB(egi_imgbuf, fb_dev, i, -1, x0, y0);
		fb_render(fb_dev);
		if(delayms<0)
			tm_delayms(egi_imgbuf->delayms);
		else
			tm_delayms(delayms);
	}
}


/*-----------------------------------------------------------------
        	 Reset Color and Alpha value

@color:		16bit color value. If <0 ingore it.
@alpah:		8bit alpha value.  If <0 ingore it.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------*/
int egi_imgbuf_resetColorAlpha(EGI_IMGBUF *egi_imgbuf, int color, int alpha )
{
	int i;

	if( egi_imgbuf==NULL || egi_imgbuf->imgbuf==NULL )
		return -1;

#if 0 	/* Get mutex lock */
	if( pthread_mutex_lock(&egi_imgbuf->img_mutex)!=0 ){
		printf("%s: Fail to lock image mutex!\n",__func__);
		return -2;
	}
#endif

	/* Limit */
	if(color>0xFFFF) color=0xFFFF;
	if(alpha>0xFF) alpha=0xFF;

	/* Case 1: Reset COLOR + ALPHA */
	if( ( alpha>=0 && egi_imgbuf->alpha != NULL ) && color>=0 ) {
		for(i=0; i< egi_imgbuf->height*egi_imgbuf->width; i++) {
			egi_imgbuf->alpha[i]=alpha;
			egi_imgbuf->imgbuf[i]=color;
		}
	}
	/* Case 2: Reset ALPHA only */
	else if( alpha>=0 && (egi_imgbuf->alpha != NULL) ) {
		for(i=0; i< egi_imgbuf->height*egi_imgbuf->width; i++)
			egi_imgbuf->alpha[i]=alpha;
	}
	/* Case 3: Reset COLOR only */
	else if( color>=0 ) {
		for(i=0; i< egi_imgbuf->height*egi_imgbuf->width; i++)
			egi_imgbuf->imgbuf[i]=color;
	}

#if 0  	/* Put mutex lock  */
  	pthread_mutex_unlock(&egi_imgbuf->img_mutex);
#endif
	return 0;
}

/*-----------------------------------------------------------------
Reset Color and Alpha value for a block area in the imgbuf.

@imgbuf:	Pointer to an EGI_IMGBUF.
@color:		16bit color value. If <0 ingore it.
@alpah:		8bit alpha value.  If <0 ingore it.
@px,py:		Origin/start point of the rectangular block area.
@height, width: Height and width of the block area.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------*/
int egi_imgbuf_blockResetColorAlpha(EGI_IMGBUF *egi_imgbuf, int color, int alpha,
					int px, int py, int height, int width)
{
	int i,j;
	int pex, pey;
	int pos;

	if( egi_imgbuf==NULL || egi_imgbuf->imgbuf==NULL )
		return -1;

	/* Limit px,py within [0  width-1],[0 height-1] */
	if(px > egi_imgbuf->width-1) return 0;
	if(py > egi_imgbuf->height-1) return 0;

	if(px<0)px=0;
	if(py<0)py=0;

	/* Limit pex,pey */
	pex = px+width;
	pey = py+height;
	if(pex>egi_imgbuf->width) pex=egi_imgbuf->width;
	if(pey>egi_imgbuf->height) pey=egi_imgbuf->height;

	/* Limit */
	if(color>0xFFFF) color=0xFFFF;
	if(alpha>0xFF) alpha=0xFF;

	/* Case 1: Reset COLOR + ALPHA */
	if( ( alpha>=0 && egi_imgbuf->alpha != NULL ) && color>=0 ) {
		for(i=py; i< pey; i++) {
			for(j=px; j< pex; j++) {
				pos=i*egi_imgbuf->width+j;
				egi_imgbuf->alpha[pos]=alpha;
				egi_imgbuf->imgbuf[pos]=color;
			}
		}
	}
	/* Case 2: Reset ALPHA only */
	else if( alpha>=0 && (egi_imgbuf->alpha != NULL) ) {
		for(i=py; i< pey; i++) {
			for(j=px; j< pex; j++) {
				pos=i*egi_imgbuf->width+j;
				egi_imgbuf->alpha[pos]=alpha;
			}
		}
	}
	/* Case 3: Reset COLOR only */
	else if( color>=0 ) {
		for(i=py; i< pey; i++) {
			for(j=px; j< pex; j++) {
				pos=i*egi_imgbuf->width+j;
				egi_imgbuf->imgbuf[pos]=color;
			}
		}
	}

	return 0;
}


/*-----------------------------------------------------------------------------------
Clear a subimag in an EGI_IMGBUF, reset all color and alpha data.

@egi_imgbuf:    an EGI_IMGBUF struct which hold bits_color image data of a picture.
@subindex:	index number of the sub image.
		if subindex<=0 or EGI_IMGBOX subimgs==NULL, no sub_image defined in the
		egi_imgbuf.
@color:		color value for all pixels in the imgbuf
		if <0, ignored.  meaningful  [16bits]
@alpha:		alpha value for all pixels in the imgbuf
		if <0, ignored.  meaningful [0 255]

Return:
		0	OK
		<0	fails
-------------------------------------------------------------------------------------*/
int egi_imgbuf_reset(EGI_IMGBUF *egi_imgbuf, int subindex, int color, int alpha)
{
	int height, width; /* of host image */
	int hs, ws;	   /* of sub image */
	int xs, ys;
	int i,j;
	unsigned long pos;

	if(egi_imgbuf==NULL || egi_imgbuf->imgbuf==NULL ) {
		printf("%s: egi_imbug or egi_imgbuf->imgbuf is NULL!\n",__func__);
		return -1;
	}

	/* get mutex lock */
	if( pthread_mutex_lock(&egi_imgbuf->img_mutex)!=0 ){
		printf("%s: Fail to lock image mutex!\n",__func__);
		return -2;
	}

	if(egi_imgbuf->submax < subindex) {
		printf("%s: submax < subindex! \n",__func__);
	  	pthread_mutex_unlock(&egi_imgbuf->img_mutex);
		return -3;
	}

	height=egi_imgbuf->height;
	width=egi_imgbuf->width;

	/* upper limit: color and alpha */
	if(alpha>255)alpha=255;
	if(color>0xffff)color=0xFFFF;

	/* if only 1 image, NO subimg, or RESET whole image data */
	if( egi_imgbuf->submax <= 0 || egi_imgbuf->subimgs==NULL ) {
		for( i=0; i<height*width; i++) {
			/* reset color */
			if(color>=0) {
				egi_imgbuf->imgbuf[i]=color;
			}
			/* reset alpha */
			if(egi_imgbuf->alpha && alpha>=0 ) {
				egi_imgbuf->alpha[i]=alpha;
			}
		}
	}

	/* else, >1 image */
	else if( egi_imgbuf->submax > 0 && egi_imgbuf->subimgs != NULL ) {
		xs=egi_imgbuf->subimgs[subindex].x0;
		ys=egi_imgbuf->subimgs[subindex].y0;
		hs=egi_imgbuf->subimgs[subindex].h;
		ws=egi_imgbuf->subimgs[subindex].w;

		for(i=ys; i<ys+hs; i++) {           	/* transverse subimg Y */
			if(i < 0 ) continue;
			if(i > height-1) break;

			for(j=xs; j<xs+ws; j++) {   	/* transverse subimg X */
				if(j < 0) continue;
				if(j > width -1) break;

				pos=i*width+j;
				/* reset color and alpha */
				if( color >=0 )
					egi_imgbuf->imgbuf[pos]=color;
				if( egi_imgbuf->alpha && alpha >=0 )
					egi_imgbuf->alpha[pos]=alpha;
			}
		}
	}

  	/* put mutex lock */
  	pthread_mutex_unlock(&egi_imgbuf->img_mutex);

	return 0;
}



/*------------------------------------------------------------------------------------
        	   Add FreeType FT_Bitmap to EGI imgbuf

1. The input eimg should has been initialized by egi_imgbuf_init()
   with certain size of a canvas (height x width) inside.

2. The canvas size of the eimg shall be big enough to hold the bitmap,
   or pixels out of the canvas will be omitted.

3.   		!!! -----   WARNING   ----- !!!
   In order to align all characters in same horizontal level, every char bitmap must be
   align to the same baseline, i.e. the vertical position of each char's origin
   (baseline) MUST be the same.
   So (xb,yb) should NOT be left top coordinate of a char bitmap,
   while use char's 'origin' coordinate relative to eimg canvan as (xb,yb) can align all
   chars at the same level !!!

TODO: Alpha blend is NOT good!

@eimg		The EGI_IMGBUF to hold the bitmap
@xb,yb		coordinates of bitmap origin relative to EGI_IMGBUF canvas coord.!!!!

@bitmap		pointer to a bitmap in a FT_GlyphSlot.
		typedef struct  FT_Bitmap_
		{
			    unsigned int    rows;
			    unsigned int    width;
			    int             pitch;
			    unsigned char*  buffer;
			    unsigned short  num_grays;
			    unsigned char   pixel_mode;
			    unsigned char   palette_mode;
			    void*           palette;
		} FT_Bitmap;

@subcolor	>=0 as substituting color of EGI_16BIT_COLOR.
		<0  use bitmap buffer data as gray value. 0-255 (BLACK-WHITE)

return:
	0	OK
	<0	fails
--------------------------------------------------------------------------------*/
int egi_imgbuf_blend_FTbitmap(EGI_IMGBUF* eimg, int xb, int yb, FT_Bitmap *bitmap,
								int subcolor)
{
	int i,j;
	EGI_16BIT_COLOR color;
	unsigned char alpha;
	unsigned long size; /* alpha size */
	int	sumalpha;
	int 	pos;

	if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
		printf("%s: input EGI_IMBUG is NULL or uninitiliazed!\n", __func__);
		return -1;
	}
	if( bitmap==NULL || bitmap->buffer==NULL ) {
//		printf("%s: input FT_Bitmap or its buffer is NULL!\n", __func__);
		return -2;
	}

#if 0	/* calloc and assign alpha, if NULL */
	if(eimg->alpha==NULL) {
		size=eimg->height*eimg->width;
		eimg->alpha = calloc(1, size); /* alpha value 8bpp */
		if(eimg->alpha==NULL) {
			printf("%s: Fail to calloc eimg->alpha\n",__func__);
			return -3;
		}
		memset(eimg->alpha, 255, size); /* assign to 255 */
	}
#endif

//	printf("bitmap: rows=%d, width=%d. \n", bitmap->rows, bitmap->width);
	for( i=0; i< bitmap->rows; i++ ) {	      /* traverse bitmap height  */
		for( j=0; j< bitmap->width; j++ ) {   /* traverse bitmap width */
			/* Check range limit, TODO */
			if( yb+i <0 || yb+i >= eimg->height ||
				    xb+j <0 || xb+j >= eimg->width )
				continue;

			/* buffer value(0-255) deemed as gray value OR alpha value */
			alpha=bitmap->buffer[i*bitmap->width+j];

			pos=(yb+i)*(eimg->width) + xb+j; /* eimg->imgbuf position */

			/* blend color	*/
			if( subcolor>=0 ) {	/* use subcolor */
				//color=subcolor;
				/* !!!WARNG!!!  NO Gamma Correctio in color blend macro,
				 * color blend will cause some unexpected gray
				 * areas/lines, especially for two contrasting colors.
				 * Select a suitable backgroud color to weaken this effect.
				 */
				if(alpha>180)alpha=255;  /* set a limit as for GAMMA correction, too simple! */
				color=COLOR_16BITS_BLEND( subcolor, eimg->imgbuf[pos], alpha );
							/* front, background, alpha */
			}
			else {	/* use Font bitmap gray value */
				/* alpha=0 MUST keep unchanged! */
				if(alpha>0 && alpha<180)alpha=255; /* set a limit as for GAMMA correction, too simple! */
				color=COLOR_16BITS_BLEND( COLOR_RGB_TO16BITS(alpha,alpha,alpha),
							  eimg->imgbuf[pos], alpha );
			}
			eimg->imgbuf[pos]=color; /* assign color to imgbuf */

			/* blend alpha value */
			if(eimg->alpha) {
				sumalpha=eimg->alpha[pos]+alpha;
				if( sumalpha > 255 ) sumalpha=255;
				eimg->alpha[pos]=sumalpha; //(alpha>0 ? 255:0); //alpha;
			}
		}
	}


	return 0;
}



/*----------------------------------------------------------------
Set average Luminance/brightness for an image

Limit: [uint]2^31 = [Y 2^8] * [ Max. Pixel number 2^23 ]

@eimg:	Input imgbuf
@luma:	Expected average luminance/brightness value for the image. [0 255]


Return:
	0		OK
	others		Fails
------------------------------------------------------------------*/
int egi_imgbuf_avgLuma( EGI_IMGBUF *eimg, unsigned char luma )
{
	int i,j;
	int index;
	unsigned int	pixnum;	/* total number of pixels in the image */
	unsigned int	luma_sum; /* sum of brightness Y */
	unsigned int	luma_dev; /* deviation of Y */

	/* check input data */
	if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
		printf("%s: input EGI_IMBUG is NULL or uninitiliazed!\n", __func__);
		return -1;
	}

	/* check pixel number limit */
	pixnum=eimg->height * eimg->width;
	if( pixnum > (1<<23) ) {
		printf("%s: Total number of pixels is out of LIMIT!\n", __func__);
		return -2;
	}

	/* calculate average brightness of the image */
	luma_sum=0;
	for( i=0; i < eimg->height; i++ ) {
		for( j=0; j < eimg->width; j++ ) {
			luma_sum += egi_color_getY(eimg->imgbuf[i*eimg->width+j]);
		}
	}

	/* adjust brightness for each pixel */
	luma_dev=luma-luma_sum/pixnum; /* get average dev. value */
	for( i=0; i < eimg->height; i++ ) {
		for( j=0; j < eimg->width; j++ ) {
			index=i*eimg->width+j;
			eimg->imgbuf[index]=egi_colorLuma_adjust(eimg->imgbuf[index], luma_dev);
		}
	}

	return 0;
}


/*----------------------------------------------------------------
Flip image (color/alpha) in imgbuf_Y direction. ( vertically )

@eimg:	Input imgbuf
Return:
	0		OK
	others		Fails
------------------------------------------------------------------*/
int egi_imgbuf_flipY( EGI_IMGBUF *eimg )
{
	int i;
	char *tmp=NULL;
	int  line_length;  /* in bytes */

	/* Check input data */
	if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
		printf("%s: input EGI_IMBUG is NULL or uninitiliazed!\n", __func__);
		return -1;
	}

	/* Get mutex lock */
	if( pthread_mutex_lock(&eimg->img_mutex)!=0 ){
		printf("%s: Fail to lock image mutex!\n",__func__);
		return -2;
	}

	/* Color width length in bytes */
	line_length=eimg->width*sizeof(EGI_16BIT_COLOR);

	/* Temp. data buffer */
	tmp=calloc(1, line_length);
	if(tmp==NULL) {
		printf("%s: Fail to calloc tmp\n",__func__);
  		pthread_mutex_unlock(&eimg->img_mutex);
		return -3;
	}

	/* Swap color lines */
	for(i=0; i< eimg->height/2; i++) {
		memcpy(tmp, (char *)(eimg->imgbuf+i*eimg->width), line_length);  /* buffer i_th line */
		memcpy((char *)(eimg->imgbuf+i*eimg->width), (char *)(eimg->imgbuf+(eimg->height-1-i)*eimg->width), line_length); /* cpy height-1-i_th line to i_th line */
		memcpy((char *)(eimg->imgbuf+(eimg->height-1-i)*eimg->width), tmp, line_length); /* cpy tmp to height-1-i_th line */
	}

	/* Swap alpha lines */
	if(eimg->alpha) {
		/* Alpha width length in bytes, assume color_line_lengh > alpha_line_length */
		line_length=eimg->width*sizeof(EGI_8BIT_ALPHA);
		for(i=0; i< eimg->height/2; i++) {
			memcpy(tmp, (char *)(eimg->alpha+i*eimg->width), line_length);  /* buffer i_th line */
			memcpy((char *)(eimg->alpha+i*eimg->width), (char *)(eimg->alpha+(eimg->height-1-i)*eimg->width), line_length); /* cpy height-1-i_th line to i_th line */
			memcpy((char *)(eimg->alpha+(eimg->height-1-i)*eimg->width), tmp, line_length); /* cpy tmp to height-1-i_th line */
		}
	}

	free(tmp);

  	/* Put mutex lock */
  	pthread_mutex_unlock(&eimg->img_mutex);

	return 0;
}



/*---------------------------------------------------------------------------------
Transpose the image to its centrosymmetrical position.
Sway pixels(color/alpha) in position (x,y)  to  (eimg->width-1-x, eimg->height-1-y)

@eimg:	Input imgbuf
Return:
	0		OK
	others		Fails
---------------------------------------------------------------------------------*/
int egi_imgbuf_centroSymmetry( EGI_IMGBUF *eimg )
{
	int i,j;
	char *tmp=NULL;
	int  line_length;  /* in bytes */

	/* Check input data */
	if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
		printf("%s: input EGI_IMBUG is NULL or uninitiliazed!\n", __func__);
		return -1;
	}

	/* Get mutex lock */
	if( pthread_mutex_lock(&eimg->img_mutex)!=0 ){
		printf("%s: Fail to lock image mutex!\n",__func__);
		return -2;
	}

	/* Color width length in bytes */
	line_length=eimg->width*sizeof(EGI_16BIT_COLOR);

	/* Temp. data buffer */
	tmp=calloc(1, line_length);
	if(tmp==NULL) {
		printf("%s: Fail to calloc tmp\n",__func__);
  		pthread_mutex_unlock(&eimg->img_mutex);
		return -3;
	}

	/* Swap color values  */
	for(j=0; j < (eimg->height+1)/2; j++) {   /* +1 for height is an odd number */
		for(i=0; i < eimg->width; i++) {
			/* swap pixels j_th line to tmp */
			*(EGI_16BIT_COLOR *)(tmp+(eimg->width-1-i)*sizeof(EGI_16BIT_COLOR))  =  eimg->imgbuf[j*eimg->width+i];

			/* swap pixels in height-1-j_th line to j_th line. */
			eimg->imgbuf[j*eimg->width+i] = eimg->imgbuf[(eimg->height-1-j)*eimg->width+(eimg->width-1-i)];
		}
		/* copy tmp line to height-1-j_th line */
		memcpy( (char *)(eimg->imgbuf+(eimg->height-1-j)*eimg->width), tmp, line_length );
	}

	/* Swap color values  */
	if(eimg->alpha) {
		line_length=eimg->width*sizeof(EGI_8BIT_ALPHA);
		for(j=0; j < (eimg->height+1)/2; j++) {
			for(i=0; i < eimg->width; i++) {
				/* swap pixels in j_th line to tmp */
				*(EGI_8BIT_ALPHA *)(tmp+(eimg->width-1-i)*sizeof(EGI_8BIT_ALPHA))  =  eimg->alpha[j*eimg->width+i];

				/* swap pixels in height-1-j_th line to j_th line. */
				eimg->alpha[j*eimg->width+i] = eimg->alpha[(eimg->height-1-j)*eimg->width+(eimg->width-1-i)];
			}
			/* copy tmp line to height-1-j_th line */
			memcpy( (char *)(eimg->alpha+(eimg->height-1-j)*eimg->width), tmp, line_length );
		}
	}

	free(tmp);

  	/* Put mutex lock */
	pthread_mutex_unlock(&eimg->img_mutex);

	return 0;
}



/*----------------------------------------------------------------
Flip image (color/alpha) in imgbuf_X direction. ( horizontally )

@eimg:	Input imgbuf
Return:
	0		OK
	others		Fails
------------------------------------------------------------------*/
int egi_imgbuf_flipX( EGI_IMGBUF *eimg )
{
	if( egi_imgbuf_flipY(eimg)!=0 )
		return -1;
	if( egi_imgbuf_centroSymmetry(eimg)!=0 )
		return -2;

	return 0;
}

/*------------------------ DirectFB Write -------------------------------------
DirectFB write RGB 24bit color data to FB.
The orginal (x0,y0) and image size MUST be appropriate so as to ensure that the
image is totally contained within the screen! Or it will abort.

@rgb:		Pointer to raw RGB data.
@width,height:	Width and Height of the image
@fb_dev:	FB dev.
@x0,y0:		Original coord of image, relative to screen.

------------------------------------------------------------------------------*/
int  egi_imgbuf_showRBG888( const unsigned char *rgb, int width, int height,
			    FBDEV *fb_dev, int x0, int y0 )
{
	unsigned char *fbp;
	int xres;
	int yres;
	int bytes_per_pixel;
	unsigned char *dat;  /* color data position */
	uint16_t color;
	long int location = 0;  /* Of FB */
	int x,y;
	int ltx=0, lty=0, rbx=0, rby=0; /* LeftTop and RightBottom tip coords of image, within and relative to the screen */

	if(fb_dev==NULL)
		return -1;
	if(rgb==NULL)
		return -1;


	/* Get fb map pointer and params */
	if(fb_dev->map_bk)
		fbp =fb_dev->map_bk;
	else
		fbp =fb_dev->map_fb;
	xres=fb_dev->finfo.line_length/(fb_dev->vinfo.bits_per_pixel>>3);
	yres=fb_dev->vinfo.yres;
	bytes_per_pixel=(fb_dev->vinfo.bits_per_pixel)>>3;

#if 0 ////////////////  Image MUST totally contained in the screen  ////////////////////////////
	/* Set dat */
	dat=(unsigned char *)rgb;

	/* Check image size and position */
	if( x0<0 || y0<0 || x0+width > xres || y0+height > yres ) {
		printf("Image cannot fit into the screen!\n");
		return -2;
	}

	for( y=0; y<height; y++) {   /* Bottom to top */
	   for(x=0; x<width; x++) {
		location = (x+x0) * bytes_per_pixel + (y+y0) * xres * bytes_per_pixel;  /* Of FB */
		/* FB pixel: R8G8B8A8, LETS_NOTE */
		if(bytes_per_pixel==4)  {
			*(fbp+location+2)=*dat++;
			*(fbp+location+1)=*dat++;
			*(fbp+location)=*dat++;
			*(fbp+location+3)=255;		/* Alpha */
		}
		/* FB pixel: R5G6B5 */
   		else if(bytes_per_pixel==2) {
			color=COLOR_RGB_TO16BITS(*dat,*(dat+1),*(dat+2));
			*(uint16_t *)(fbp+location)=color;
			dat+=3;
		}
	   }
	}

#else  ///////////////////////////////////////////////

        /* If totally out of range */
        if(x0 < -width+1 || x0 > xres-1)
                return 0;
        if(y0 < -height+1 || y0 > yres-1)
                return 0;

        /* Image LeftTop point (x,y), within and relatvie to screen */
        if(x0<0) ltx=0;
        else     ltx=x0;
        if(y0<0) lty=0;
        else     lty=y0;

        /* Image RightBottom point (x,y), within and relatvie to screen */
        if(x0+width-1 > xres-1)  rbx=xres-1;
        else                     rbx=x0+width-1;
        if(y0+height-1 > yres-1) rby=yres-1;
        else                     rby=y0+height-1;

//        printf("LT(%d,%d), RB(%d,%d)\n", ltx,lty, rbx, rby);

        /* Here (x,y) are within screen coord */
        for( y=lty; y<rby+1; y++) {
                for(x=ltx; x<rbx+1; x++) {
		   /* FB/Image map position */
                   location = x*bytes_per_pixel + y*xres*bytes_per_pixel;  /* of FB */
                   dat = (unsigned char*)rgb + (x-x0)*3 + (y-y0)*width*3;  /* Of image, RGB888 */

		   /* FB pixel: R8G8B8A8, LETS_NOTE */
                   if( bytes_per_pixel==4 ) {
			*(fbp+location+2)=*dat++;
			*(fbp+location+1)=*dat++;
			*(fbp+location)=*dat++;
			*(fbp+location+3)=255;		/* Alpha */
                   }
		   /* FB pixel: R5G6B5 */
                   else if( bytes_per_pixel==2 ) {
			color=COLOR_RGB_TO16BITS(*dat,*(dat+1),*(dat+2));
			*(uint16_t *)(fbp+location)=color;
		  }
	       }
	}

#endif

	return 0;
}

/*----------------------------------------------------------------------
Interpolate to get a pixel from an EGI_IMBUF with given u,v[0 1] value.

@imgbuf:	An pointer to EGI_IMGBUF.
@u,v:		Normalized X/Y ratio, all within [0 1].
@color,alpha:   Pointer to pass out color/alpha as result.
		If NULL, ignore.

Return:
	0	OK
	<0	Fails

inline void egi_16bitColor_interplt4p( EGI_16BIT_COLOR color1, EGI_16BIT_COLOR color2,
                                       EGI_16BIT_COLOR color3, EGI_16BIT_COLOR color4,
                                       unsigned char alpha1,  unsigned char alpha2,
                                       unsigned char alpha3,  unsigned char alpha4,
                                       int f15_ratioX, int f15_ratioY,
                                       EGI_16BIT_COLOR* color, unsigned char *alpha )
--------------------------------------------------------------------------------*/
int egi_imgbuf_uvToPixel(EGI_IMGBUF *imgbuf, float u, float v,
					EGI_16BIT_COLOR* color, unsigned char *alpha )
{
	/* Check input data */
	if( imgbuf==NULL || imgbuf->imgbuf==NULL ) {
		egi_dpstd("Input EGI_IMBUG is NULL or uninitiliazed!\n");
		return -1;
	}

	EGI_16BIT_COLOR c1, c2, c3, c4;
	EGI_8BIT_ALPHA  a1, a2, a3, a4;

	int imgw=imgbuf->width;
	int imgh=imgbuf->height;

	float ftmp;
	int Wl, Wr, Hu, Hd; /* left/right width index, upper/down heigth index */

	/* Check u/v */
	if(u<0.0f || u>1.0f) return -1;
	if(v<0.0f || v>1.0f) return -1;

	/* Get f15_ratio for interpolatoin. */
	int f15_ratioX=roundf((modff(u*imgw, &ftmp)*(1<<15)));
	Wl=ftmp; //roundf(ftmp);
	Wr=Wl+1;

	int f15_ratioY=roundf((modff(v*imgh, &ftmp)*(1<<15)));
	Hu=ftmp; //roundf(ftmp);
	Hd=Hu+1;

	/* Get four pixel as boxing points. */
	c1=imgbuf->imgbuf[Hu*imgw+Wl];
	c2=imgbuf->imgbuf[Hu*imgw+Wr];
	c3=imgbuf->imgbuf[Hd*imgw+Wl];
	c4=imgbuf->imgbuf[Hd*imgw+Wr];
	if(imgbuf->alpha) {
		a1=imgbuf->alpha[Hu*imgw+Wl];
		a2=imgbuf->alpha[Hu*imgw+Wr];
		a3=imgbuf->alpha[Hd*imgw+Wl];
		a4=imgbuf->alpha[Hd*imgw+Wr];
	}

	egi_16bitColor_interplt4p(c1,c2,c3,c4, a1,a2,a3,a4,
					f15_ratioX, f15_ratioY, color, alpha);

	return 0;
}

/* --------------<<<  ALGORITHM_1:  Matrix Mapping   >>>----------------

Map a triangle to imgbuf and write mapped pixles to FB.

Note:
1. if u/v is very near to 1.0, then locimg MAY be out of range for imgbuf->imgbuf[locimg]
   notice u/v --> [0 1)

TODO:
1. Fail to inverse matrix_XYZ in some postion!  However cube test MAYBE OK!!!


@imbufg:	A pointer to EGI_IMGBUF
@fb_dev:	A pointer to FBDEV
@ux,vx:		u/v[0 1) for mapping triangle vertices.
@x/y/z:		XYZ coordinates for trianlge vertice.
----------------------------------------------------------------------*/
void egi_imgbuf_mapTriWriteFB(EGI_IMGBUF *imgbuf, FBDEV *fb_dev,
				      float u0, float v0,
				      float u1, float v1,
				      float u2, float v2,
				      float x0, float y0, float z0,
				      float x1, float y1, float z1,
				      float x2, float y2, float z2 )

{
	int i, k, kstart, kend;
	int nl=0,nr=0; /* left and right point index */
	int nm; /* mid point index */
	float klr,klm,kmr;

	/* OR use INT type */
	float yu=0;
	float yd=0;
	float ymu=0;
	float zu, zd;
	//float tmp;
	long int locimg;
	EGI_16BIT_COLOR color;

	/* 0. Check input data */
	if( imgbuf==NULL || imgbuf->imgbuf==NULL ) {
		egi_dpstd("Input EGI_IMBUG is NULL or uninitiliazed!\n");
		return;
	}
//	int imgw=imgbuf->width;
//	int imgh=imgbuf->height;

	/* 1. Mapping matrix computation */
	/* 1.1 Prepare Matrix data for 3 vertices. */
	//float uvmat[3*3]={ u0, v0, 1.0f, u1, v1, 1.0f, u2, v2, 1.0f };
	float uvmat[3*3]={ u0, v0, 0.0f, u1, v1, 0.0f, u2, v2, 0.0f };
	float xyzmat[3*3]={x0, y0, z0, x1, y1, z1, x2, y2, z2};
	float tmat[3*3];	/* Transform/map matrix */
	float Ixyzmat[3*3];	/* Inversed xyzmat */

 	struct float_Matrix matUV;
 	matUV.nr=3; matUV.nc=3; matUV.pmat=uvmat;

 	struct float_Matrix matXYZ;
 	matXYZ.nr=3; matXYZ.nc=3; matXYZ.pmat=xyzmat;

 	struct float_Matrix matT;
	matT.nr=3; matT.nc=3; matT.pmat=tmat;

 	struct float_Matrix matIXYZ;
	matIXYZ.nr=3; matIXYZ.nc=3; matIXYZ.pmat=Ixyzmat;

	/* 1.2 Inverse matXYZ */
	if( Matrix_Inverse(&matXYZ, &matIXYZ)==NULL ) {
		egi_dpstd("Fail to inverse matrix_XYZ!\n");
		return;
	}

	/* 1.3 matT = matIXYZ*matUV */
	Matrix_Multiply(&matIXYZ, &matUV, &matT);


	/* 2. Define matPuv and matPxyz */
	float ptuv[3]={0,0,1.0f};  /* U,V,1 */
	struct float_Matrix matPuv;
	matPuv.nr=1; matPuv.nc=3; matPuv.pmat=ptuv;

	float ptxyz[3]={0,0,0};
	struct float_Matrix matPxyz;
	matPxyz.nr=1; matPxyz.nc=3; matPxyz.pmat=ptxyz;

	/* 3. Define a point array */
	struct float_3dpoints {
		float x; float y; float z;
	} points[3];

	points[0].x=x0; points[0].y=y0; points[0].z=z0;
	points[1].x=x1; points[1].y=y1; points[1].z=z1;
	points[2].x=x2; points[2].y=y2; points[2].z=z2;


	/* 4. Find Left, Right and Mid. point */
	/* Cal nl, nr */
	for(i=1;i<3;i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

#if 0	/* TODO: othree points are collinear */
	if(nl==nr) {
		draw_pline_nc(dev, points, 3, 1);
		return;
	}
#endif

	/* get x_mid point index */
	nm=3-nl-nr;

	/* 5. Compute side slopes. */
	/* Ruled out (points[nr].x == points[nl].x), as nl==nr.  */
	klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);

	if(points[nm].x != points[nl].x) {
		klm=1.0*(points[nm].y-points[nl].y)/(points[nm].x-points[nl].x);
	}
	else
		klm=1000000.0;

	if(points[nr].x != points[nm].x) {
		kmr=1.0*(points[nr].y-points[nm].y)/(points[nr].x-points[nm].x);
	}
	else
		kmr=1000000.0;
	//printf("klr=%f, klm=%f, kmr=%f \n",klr,klm,kmr);

	/* 6. Left part of the triangle: traverse pixels and map to get color value. */
	for( i=0; i<roundf(points[nm].x-points[nl].x+1); i++)
	{
		yu=klr*i+points[nl].y;	//points[nl].y+klr*i;
		yd=klm*i+points[nl].y;	//points[nl].y+klm*i;

		zu=points[nl].z+(points[nr].z-points[nl].z)*i/(points[nr].x-points[nl].x);
		zd=points[nl].z+(points[nm].z-points[nl].z)*i/(points[nm].x-points[nl].x);

		if(yu>yd) {
			kstart=roundf(yd);
			kend=roundf(yu);
		}
		else	  {
			kstart=roundf(yu);
			kend=roundf(yd);
		}

		/* Traverse pixels on the vertical line */
		for(k=kstart; k<=kend; k++) {
			ptxyz[0]=i+points[nl].x;   		//X
			ptxyz[1]=k;				//Y
			ptxyz[2]=zd+(zu-zd)*(k-yd)/(yu-yd); 	//Z

			/* matPuv =matPxyz*matT */
			if( Matrix_Multiply(&matPxyz, &matT, &matPuv)==NULL ) {
				egi_dpstd("Fail to do matPuv =matPxyz*matT!\n");
				//return;
			}

			/* Get mapped pixel and draw_dot */
		     #if 0 /* OPTION_1: Non_interpolation. */
                        /* image data location */
                        locimg=(roundf(ptuv[1]*imgh))*imgw+roundf(ptuv[0]*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
                            draw_dot(fb_dev, roundf(points[nl].x+i), k); // k as y
			}
                     #else /* OPTION_2: Get pixel color/alpha by interpolation */
                        if( egi_imgbuf_uvToPixel(imgbuf, ptuv[0], ptuv[1], &color, NULL)==0 ) {
                                fbset_color2(fb_dev, color);
                                draw_dot(fb_dev, roundf(points[nl].x+i), k); // k as y
                        }
                    #endif

		}
	}

	/* 7. Right part of the triangle: traverse pixels and map to get color value. */
	ymu=yu;
	for( i=0; i<roundf(points[nr].x-points[nm].x+1); i++)
	{
		yu=klr*i+ymu;          //yu=ymu+klr*i;
		yd=kmr*i+points[nm].y; //yd=points[nm].y+kmr*i;

		zu=points[nl].z+(points[nr].z-points[nl].z)*(points[nm].x-points[nl].x+i)/(points[nr].x-points[nl].x);
		zd=points[nm].z+(points[nr].z-points[nm].z)*i/(points[nr].x-points[nm].x);

		if(yu>yd) { kstart=roundf(yd); kend=roundf(yu); }
		else	  { kstart=roundf(yu); kend=roundf(yd); }

		/* Traverse pixels on the vertical line */
		for(k=kstart; k<=kend; k++) {
			ptxyz[0]=i+points[nm].x;   		//X
			ptxyz[1]=k;				//Y
			ptxyz[2]=zd+(zu-zd)*(k-yd)/(yu-yd); 	//Z

			/* matPuv =matPxyz*matT */
			if( Matrix_Multiply(&matPxyz, &matT, &matPuv)==NULL ) {
				egi_dpstd("Fail to do matPuv =matPxyz*matT!\n");
				//return;
			}

			/* Get mapped pixel and draw_dot */
		     #if 0 /* OPTION_1: Non_interpolation. */
                        /* image data location */
                        locimg=(roundf(ptuv[1]*imgh))*imgw+roundf(ptuv[0]*imgw); /* roundf */
			if(locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
                            draw_dot(fb_dev, roundf(points[nm].x+i), k); // k as y
			}
		     #else /* OPTION_2: Get pixel color/alpha by interpolation */
			if( egi_imgbuf_uvToPixel(imgbuf, ptuv[0], ptuv[1], &color, NULL)==0 ) {
				fbset_color2(fb_dev, color);
        	                draw_dot(fb_dev, roundf(points[nm].x+i), k); // k as y
			}
		    #endif
		}

	}
}


/* OBSOLETE: replaced by egi_imgbuf_mapTriWriteFB3() */
/* ---------<<<  ALGORITHM_2: Barycentric coordinates mapping  >>>----------
			    (FLOAT x/y)
Map a triangle to imgbuf and write mapped pixles to FB.

NOTE:
1. DO NOT check/make a/b/r AND u/v to within [0.0 1.0], just get rid of them
   if not in the range. OR some noise pixels will appear on result image!!!
2. if u/v is very near to 1.0, then locimg MAY be out of range for imgbuf->imgbuf[locimg]
   notice u/v --> [0 1)

@imbufg:	A pointer to EGI_IMGBUF
@fb_dev:	A pointer to FBDEV
@u/v:		u/v[0 1) coords/factors for mapping triangle vertices.
@x/y/z:		XYZ coordinates for trianlge vertice.
---------------------------------------------------------------------------*/
void egi_imgbuf_mapTriWriteFB2(EGI_IMGBUF *imgbuf, FBDEV *fb_dev,
				      float u0, float v0,
				      float u1, float v1,
				      float u2, float v2,
				      float x0, float y0,
				      float x1, float y1,
				      float x2, float y2 )
{
	/* Check input data */
	if( imgbuf==NULL || imgbuf->imgbuf==NULL ) {
		egi_dpstd("Input EGI_IMBUG is NULL or uninitiliazed!\n");
		return;
	}
	int imgw=imgbuf->width;
	int imgh=imgbuf->height;

	/* Barycentric coordinates (a,b,r) for points inside the triangle:
	 * P(x,y)=a*A + b*B + r*C;  where a+b+r=1.0.
	 */
	float a, b, r, ftmp;
	float u,v;
	float x; //y;

	/* As 2D */
	struct {
		float x; float y; //float z;
	} points[3];
	points[0].x=x0; points[0].y=y0; //points[0].z=z0;
	points[1].x=x1; points[1].y=y1; //points[1].z=z1;
	points[2].x=x2; points[2].y=y2; //points[2].z=z2;

	int i, k, kstart, kend;
	int nl=0,nr=0; /* left and right point index */
	int nm; /* mid point index */

	float klr,klm,kmr;

	/* OR use INT type */
	float yu=0;
	float yd=0;
	float ymu=0;

	long int locimg;


	/* Cal nl, nr */
	for(i=1;i<3;i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

	/* Case 1: all points are collinear as a vertical line. */
	if(nl==nr) {
		/* Get yu yd */
		yu=points[0].y;
		yd=points[0].y;
		for(i=1; i<3; i++) {
			if(points[i].y>yu) yu=points[i].y;
			if(points[i].y<yd) yd=points[i].y;
		}

		x=points[0].x;
		for(k=roundf(yd); k<=roundf(yu); k++) {
			#if 1
			a=(-(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/(-(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1));
			//if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			b=(-(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/(-(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2));
			//if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			//if(r<0.0f)r=0.0f; //else if(r>1.0f)r=1.0f;
			#else  /* SAME as above */
			r=((y0-y1)*x+(x1-x0)*k+x0*y1-x1*y0)/((y0-y1)*x2+(x1-x0)*y2+x0*y1-x1*y0);
			b=((y0-y2)*x+(x2-x0)*k+x0*y2-x2*y0)/((y0-y2)*x1+(x2-x0)*y1+x0*y2-x2*y0);
			a=1.0-r-b;
			#endif

                        /* Normalize a/b/r */
                        if(a<0)a=-a; if(b<0)b=-b;
                        ftmp=a+b;
                        if(ftmp>1.0+0.001) {
                                //egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
                                a=a/ftmp; b=b/ftmp;
                        }
                        if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
                        if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
                        r=1.0-a-b;
                        if(r<0.0f)r=0.0;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
                            draw_dot(fb_dev, roundf(x), k);
			}
			#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
			#endif
		}

		return;
	}

	/* Get x_mid point index, NOW: nl != nr. */
	nm=3-nl-nr;

	/* Ruled out (points[nr].x == points[nl].x), as nl==nr.  */
	//if(nl!=nr)
	       klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);
	//else
	//       klr=1000000.0;

	if(points[nm].x != points[nl].x) {
		klm=1.0*(points[nm].y-points[nl].y)/(points[nm].x-points[nl].x);
	}
	else
		klm=1000000.0;

	if(points[nr].x != points[nm].x) {
		kmr=1.0*(points[nr].y-points[nm].y)/(points[nr].x-points[nm].x);
	}
	else
		kmr=1000000.0;

	/* Draw lines for two tri */
	for( i=0; i<roundf(points[nm].x-points[nl].x+1); i++)
	{
		yu=klr*i+points[nl].y;	//points[nl].y+klr*i;
		yd=klm*i+points[nl].y;	//points[nl].y+klm*i;

		/* Cal. x */
		x=points[nl].x+i;

		if(yu>yd) { kstart=roundf(yd); kend=roundf(yu); }
		else	  { kstart=roundf(yu); kend=roundf(yd); }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates:  k as Y. */
			// y=k;
			#if 1
			a=(-(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/(-(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1));
			//if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			b=(-(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/(-(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2));
			//if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			//if(r<0.0f)r=0.0f; //else if(r>1.0f)r=1.0f;
			#else  /* SAME as above */
			r=((y0-y1)*x+(x1-x0)*k+x0*y1-x1*y0)/((y0-y1)*x2+(x1-x0)*y2+x0*y1-x1*y0);
			b=((y0-y2)*x+(x2-x0)*k+x0*y2-x2*y0)/((y0-y2)*x1+(x2-x0)*y1+x0*y2-x2*y0);
			a=1.0-r-b;
			#endif

                        /* Normalize a/b/r */
                        if(a<0)a=-a; if(b<0)b=-b;
                        ftmp=a+b;
                        if(ftmp>1.0+0.001) {
                                //egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
                                a=a/ftmp; b=b/ftmp;
                        }
                        if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
                        if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
                        r=1.0-a-b;
                        if(r<0.0f)r=0.0;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

//			printf("XY(%d,%d): a=%f, b=%f, r=%f, u=%f, v=%f\n", (int)x,k, a,b,r, u,v);

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
                            draw_dot(fb_dev, roundf(x), k);
			}
			#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
			#endif
		}
	}

	ymu=yu;
	for( i=0; i<roundf(points[nr].x-points[nm].x+1); i++)
	{
		yu=klr*i+ymu;          //yu=ymu+klr*i;
		yd=kmr*i+points[nm].y; //yd=points[nm].y+kmr*i;

		/* Cal. x */
		x=points[nm].x+i;

		if(yu>yd) { kstart=roundf(yd); kend=roundf(yu); }
		else	  { kstart=roundf(yu); kend=roundf(yd); }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates:  k as Y. */
			// y=k;
			#if 1
			a=(-(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/(-(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1));
			//if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			b=(-(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/(-(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2));
			//if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			//if(r<0.0f)r=0.0f; //else if(r>1.0f)r=1.0f;
			#else /* Same as above */
			r=((y0-y1)*x+(x1-x0)*k+x0*y1-x1*y0)/((y0-y1)*x2+(x1-x0)*y2+x0*y1-x1*y0);
			b=((y0-y2)*x+(x2-x0)*k+x0*y2-x2*y0)/((y0-y2)*x1+(x2-x0)*y1+x0*y2-x2*y0);
			a=1.0-r-b;
			#endif

                        /* Normalize a/b/r */
                        if(a<0)a=-a; if(b<0)b=-b;
                        ftmp=a+b;
                        if(ftmp>1.0+0.001) {
                                //egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
                                a=a/ftmp; b=b/ftmp;
                        }
                        if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
                        if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
                        r=1.0-a-b;
                        if(r<0.0f)r=0.0;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;
//			printf("XY(%d,%d): a=%f, b=%f, r=%f, u=%f, v=%f\n", (int)x,k, a,b,r, u,v);

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf! */
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
                            draw_dot(fb_dev, roundf(x), k);
			}
			#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
			#endif
		}
	}

}

/* ---------<<<  ALGORITHM_2: Barycentric coordinates mapping  >>>----------
			    (INT x/y)
Map a triangle to imgbuf and write mapped pixles to FB.

Case 1: All points are the SAME! the triangle converges into ONE point!.
Case 2: All points are collinear, as a vertical line.
Case 3: All points are collinear, as an oblique OR horizontal line.
Case 4: As a true resonalbe triangle.

                        --- XYZ values and precision ---
To use integer data type for XYZ will sacrifice certain precisions, especially
where mesh gradient is very large.

NOTE:
1. DO NOT check/make a/b/r AND u/v to within [0.0 1.0], just get rid of them
   if not in the range. OR some noise pixels will appear on result image!!!
2. if u/v is very near to 1.0, then locimg MAY be out of range for imgbuf->imgbuf[locimg]
   notice u/v --> [0 1)

TODO: 
1. Check midpoint pixZ value, position(front/back), make it effective or not.
   If midpoint is effecive, then draw two segments( as two sides of a triangle
   are visible). In this case,  z0/z1/z2 should be provided also!
2. To apply light reflection strength for EACH pixel..., NOW renderMesh() uses
   the Tri face normal to decide/compute light strength for ALL pixels!
   and this also needs vetex normals of the 3 points.

@imbufg:	A pointer to EGI_IMGBUF
@fb_dev:	A pointer to FBDEV
@u/v:		u/v[0 1) coords/factors for mapping triangle vertices.
@x/y/z:		XYZ coordinates for trianlge vertice.
---------------------------------------------------------------------------*/
#if 0  //////////////////////// without (float z0, float z1, float z2) //////////////////////
void egi_imgbuf_mapTriWriteFB3(EGI_IMGBUF *imgbuf, FBDEV *fb_dev,
				      float u0, float v0,
				      float u1, float v1,
				      float u2, float v2,
				      int x0, int y0,
				      int x1, int y1,
				      int x2, int y2 )
//float z0, float z1, float z2 )
{
	/* Check input data */
	if( imgbuf==NULL || imgbuf->imgbuf==NULL ) {
		egi_dpstd("Input EGI_IMBUG is NULL or uninitiliazed!\n");
		return;
	}
	int imgw=imgbuf->width;
	int imgh=imgbuf->height;

	/* Barycentric coordinates (a,b,r) for points inside the triangle:
	 * P(x,y)=a*A + b*B + r*C;  where a+b+r=1.0.
	 */
	float a, b, r, ftmp;
	float u,v;
	int x; //y;

	/* As 2D */
	struct {
		int x; int y; //float z;
	} points[3];
	points[0].x=x0; points[0].y=y0; //points[0].z=z0;
	points[1].x=x1; points[1].y=y1; //points[1].z=z1;
	points[2].x=x2; points[2].y=y2; //points[2].z=z2;

	int i, k, kstart, kend;
	int nl=0,nr=0; /* left and right point index */
	int nm; /* mid point index */

	float klr,klm,kmr;
        float  fcheck1,fcheck2;

#if 0   /* NO MORE USE TO SUBSTITUE x/y, Instead, use us/ue/vs/ve to inertpolate u/v at a line. */
        /* If 3 points are collinear, substitue x0/y0 later... */
        int x0s=x0, x1s=x1, x2s=x2;
        int y0s=y0, y1s=y1, y2s=y2;
#endif

	/* use INT type */
	int yu=0;
	int yd=0;
	int ymu=0;

	float us,ue, vs,ve; /* u,v for start-end point */

	/* Imgbuf pixel data offset */
	long int locimg;

	/* Check input u/v. TODO: Reasoning... */
	if( u0<0 ) u0=1.0+u0;
	if( v0<0 ) v0=1.0+v0;
	if( u1<0 ) u1=1.0+u1;
	if( v1<0 ) v1=1.0+v1;
	if( u2<0 ) u2=1.0+u2;
	if( v2<0 ) v2=1.0+v2;


	/* --- Case 1 ---: All points are the SAME! */
	if(x0==x1 && x1==x2 && y0==y1 && y1==y2) {
//		egi_dpstd("the Tri degenerates into a point!\n");
        	/* Set a/b/r. */
		a=0.3333;    b=0.3333;    r=0.3333;

		/* Get interpolated u,v */
		u=a*u0+b*u1+r*u2;
		//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
		v=a*v0+b*v1+r*v2;
		//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

		/* Get mapped pixel and draw_dot */
                /* image data location */
                locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf! */
		if( locimg>=0 && locimg < imgh*imgw ) {
			fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
			if(imgbuf->alpha)
				fb_dev->pixalpha=imgbuf->alpha[locimg];
                        draw_dot(fb_dev, x0, y0);
		}
#if 1 /* TEST: --------- */
		else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif

		return;
	}

	/* If all points are collinear, including the case that TWO pionts are at SAME position! */
	fcheck1 = -1.0*(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1); // 1.0*(y0-y1)*(x2-x1);
	fcheck2 = -1.0*(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2); // 1.0*(y1-y2)*(x0-x2);

#if 0   ////////////////////////////    NO MORE USE!   /////////////////////////////
	/* Abandoned!!!  Instead, use us/ue/vs/ve to inertpolate u/v at a line. */

	/* If it degenerates into a line:  Move one vertex a little, to make it a NEW resonable triangle! So we can
	* compute barycentric a/b/r! However, color of the midpoint will ineffective! Means ONLY one side
	* of the triangle is drawn.
	* TODO: If necessary, re_assign input x0y0~x2y2 as the NEW triangle.
        */
	if( abs(fcheck1) < 0.0001 ) {
//		egi_dpstd("fcheck1=%e, the Tri is degenerated into a line!\n", fcheck1);
		//fcheck1=0.001;   FAILS!!! a,b OR r INALID!

		/* Just move x1 OR y1 1 pixel! to avoid it's SAME as x0,y0 OR x2,y2 */
		if( x0==x1 && x1==x2 )
			x1s=x1-1;
		else   // if( y0==y1 && y1==y2)
			y1s=y1-1;

		/* Recalculate fcheck */
		fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
		fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); //  1.0*(y1s-y2s)*(x0s-x2s);
//		egi_dpstd("Aft min move: fcheck1=%e, fcheck2=%e\n", fcheck1,fcheck2);

		 /* Check again! MUST NOT apply x1s/y2s both, in case it's a 45deg line! */
		if(abs(fcheck1)<0.0001 || abs(fcheck2)<0.0001) {

			/* Just Re_move x1 OR y1 1 pixel! to avoid it's SAME as x0,y0 OR x2,y2 */
			if( x0==x1 && x1==x2 )
                        	x1s=x1+1;
	                else   // if( y0==y1 && y1==y2)
                        	y1s=y1+1;
			//x1s=x1+1;  //y1s=y1+1;

			/* Recalculate fcheck */
			fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
			fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); // 1.0*(y1s-y2s)*(x0s-x2s);
//			egi_dpstd("Aft min. re_move: fcheck1=%e\n", fcheck1);
		}
	}
	if( abs(fcheck2) < 0.0001 ) {
//		egi_dpstd("fcheck2=%e, the Tri is degenerated into a line!\n", fcheck2);
		//fcheck2=0.001;   FAILS!!! a,b OR r INALID!

		/* Just move x2/y2 1 pixel! to avoid it's SAME as x1,y1 OR x0,y0 */
		if( x0==x1 && x1==x2 )
			x2s=x2-1;
		else   // if( y0==y1 && y1==y2)
			y2s=y2-1;

		/* Recalculate fcheck */
		fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
		fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); // 1.0*(y1s-y2s)*(x0s-x2s);
//		egi_dpstd("Aft min. move: fcheck1=%e, fcheck2=%e\n", fcheck1,fcheck2);

		if(abs(fcheck2)<0.0001 ||abs(fcheck1)<0.0001) {
			/* Just move x1/y1 1 pixel! to avoid it's SAME as x0,y0 OR x2,y2 */
			if( x0==x1 && x1==x2 )
				x2s=x2+1;
			else   // if( y0==y1 && y1==y2)
				y2s=y2+1;

			/* Recalculate fcheck */
			fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
			fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); // 1.0*(y1s-y2s)*(x0s-x2s);
//			egi_dpstd("Aft min. re_move: fcheck1=%e, fcheck2=%e\n", fcheck1,fcheck2);
		}
	}
	/* Check again, should NOT appear NOW! */
	if(abs(fcheck1)<0.0001 || abs(fcheck2)<0.0001)
		egi_dpstd("fcheck~=0!, points: {%d,%d} {%d,%d} {%d,%d}\n", x0,y0,x1,y1,x2,y2);
#endif  //////////////////////////////////////////////////////////////////////////////////////////////

	/* Cal nl, nr. just after collinear checking! */
	for(i=1; i<3; i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

	/* --- Case 2 ---: All points are collinear as a vertical line. */
	if(nl==nr) {
//		egi_dpstd("the Tri degenerates into a vertical line!\n");

		/* Init yu yd, u,v. */
		yu=points[0].y;  ue=u0; ve=v0;
		yd=points[0].y;  us=u0; vs=v0;

		/* Get yu,yd,  us/ue/vs/ve */
		for(i=1; i<3; i++) {
			if(points[i].y>yu) {
				yu=points[i].y;
				if(i==1) { ue=u1; ve=v1; }
				else     { ue=u2; ve=v2; } //i==2
			}
			if(points[i].y<yd) {
				yd=points[i].y;
				if(i==1) { us=u1; vs=v1; }
				else     { us=u2; vs=v2; } //i==2
			}
		}

		x=points[0].x;
		for(k=yd; k<=yu; k++) {
			/* Get interpolated u,v */
			u=us+(ue-us)*(k-yd)/(yu-yd);
			v=vs+(ve-vs)*(k-yd)/(yu-yd);

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
			    if(imgbuf->alpha)
				  fb_dev->pixalpha=imgbuf->alpha[locimg];
                            draw_dot(fb_dev, x, k);
			}
#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}

		return;
	}

	/* ---- Case 3 ---: All points are collinear as an oblique/horizontal line. */
	/* Note:
	 *	1. You may skip case_3, to let Case_4 draw the line, however it draws ONLY discrete
	 *	   dots for a steep line.
	 *	2. Color of the midpoint of the line will be ineffective!
	 * 	   TODO: NEW algrithm for interpolation color at a three_point line.
	 */
	if( (x0-x1)*(y2-y1)==(y0-y1)*(x2-x1) || (x1-x2)*(y0-y2)==(y1-y2)*(x0-x2) )
	{
//		egi_dpstd("the Tri degenerates into an oblique/horiz line!\n");

		int j;

                int x1=points[nl].x;
                int y1=points[nl].y;
                int x2=points[nr].x;
                int y2=points[nr].y;

		int tekxx=x2-x1;
		int tekyy=y2-y1;

		float foff;
		float flen=sqrtf(1.0*(x2-x1)*(x2-x1)+1.0*(y2-y1)*(y2-y1));

		int tmp;
//		EGI_16BIT_COLOR colorTmp; /* Color corresponds to k(y)=tmp */
		EGI_16BIT_COLOR colorJ;   /* Color corresponds to k(y)=J */
		EGI_16BIT_COLOR color;	  /* draw_dot() pixel color */

		/* NOW: x2>x1 */
		tmp=y1;

		/* nl as start point */
		if(nl==0) { us=u0; vs=v0; }
		else if(nl==1) { us=u1; vs=v1; }
		else { us=u2; vs=v2; }

		/* nr as end point */
		if(nr==0) { ue=u0; ve=v0; }
		else if(nr==1) { ue=u1; ve=v1; }
		else { ue=u2; ve=v2; }

#if 0  ////////////* image data location for start point nl */
                locimg=(roundf(vs*imgh))*imgw+roundf(us*imgw); /* roundf */
		if(locimg<0)
			locimg==0;
		else if( !(locimg < imgh*imgw) )
			locimg=imgh*imgw-1;
		colorTmp=imgbuf->imgbuf[locimg];
#endif /////////////////////////

		/* Draw all points */
		for(i=x1; i<=x2; i++) {
		     	/* j as Py */
		    	j=roundf( 1.0*(i-x1)*tekyy/tekxx+y1 );

			/* Get interpolated u,v for point (i,j) */
			u=us+(ue-us)*(i-x1)/(x2-x1);
			v=vs+(ve-vs)*(i-x1)/(x2-x1);

                        /* Image pixel color location, (i,j) */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw )
			    colorJ=imgbuf->imgbuf[locimg];
			else
			    continue;

			if(y2>=y1) {
				/* Traverse tmp (+)-> pY */
				for(k=tmp; k<=j; k++) {
					if(tmp==j) {
						fbset_color2(fb_dev, colorJ);
						draw_dot(fb_dev,i,k);
						break;
					}

					/* Get interpolated u,v */
					foff=sqrtf(1.0*(i-x1)*(i-x1)+1.0*(k-y1)*(k-y1));
					u=us+(ue-us)*foff/flen;
					v=vs+(ve-vs)*foff/flen;

		                        /* Image pixel color location */
                        		locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			                if( locimg>=0 && locimg < imgh*imgw )
                            			color=imgbuf->imgbuf[locimg];
                        		else
                            			continue;

				     	/* Set color for point(i,k) */
 				     	//egi_16bitColor_interplt( colorTmp, colorJ, 0, 0, (k-tmp)*(1<<15)/(j-tmp), &color, NULL);
					fbset_color2(fb_dev, color);

				     	draw_dot(fb_dev,i,k);
				}
			}
			else {  /* y2<y1, Traverse tmp (-)-> pY*/
				for(k=tmp; k>=j; k--) {
					if(tmp==j) {
						fbset_color2(fb_dev, colorJ);
						draw_dot(fb_dev,i,k);
						break;
					}

					/* Get interpolated u,v */
					foff=sqrtf(1.0*(i-x1)*(i-x1)+1.0*(k-y1)*(k-y1));
					u=us+(ue-us)*foff/flen;
					v=vs+(ve-vs)*foff/flen;

		                        /* Image pixel color location */
                        		locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			                if( locimg>=0 && locimg < imgh*imgw )
                            			color=imgbuf->imgbuf[locimg];
                        		else
                            			continue;

				     	/* Color for point(i,k) */
 				     	//egi_16bitColor_interplt( colorTmp, colorJ, 0, 0, (tmp-k)*(1<<15)/(tmp-j), &color, NULL);
					fbset_color2(fb_dev, color);
			    		if(imgbuf->alpha)
				  		fb_dev->pixalpha=imgbuf->alpha[locimg];

				     	draw_dot(fb_dev,i,k);
				}
			}

			tmp=j; /* Renew tmp as j(pY) */
//			colorTmp=colorJ; /* Renew colroTmp as color J */
		}

		return;
	}


	/* ---- Case 4 ---: As a true triangle. */

	/* Get x_mid point index, NOW: nl != nr. */
	nm=3-nl-nr;

	/* Ruled out (points[nr].x == points[nl].x), as nl==nr.  */
	//if(nl!=nr)
	       klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);
	//else
	//       klr=1000000.0;

	if(points[nm].x != points[nl].x) {
		klm=1.0*(points[nm].y-points[nl].y)/(points[nm].x-points[nl].x);
	}
	else
		klm=1000000.0;

	if(points[nr].x != points[nm].x) {
		kmr=1.0*(points[nr].y-points[nm].y)/(points[nr].x-points[nm].x);
	}
	else
		kmr=1000000.0;

	/* Draw lines for two tri */
	//for( i=0; i< points[nm].x-points[nl].x; i++)
	for( i=0; i< points[nm].x-points[nl].x+1; i++)
	{
		yu=roundf(klr*i+points[nl].y);
		yd=roundf(klm*i+points[nl].y);
		//egi_dpstd("nm-nl: yu=%d, yd=%d\n", yu,yd);

		/* Cal. x */
		x=points[nl].x+i;

		if(yu>yd) { kstart=yd; kend=yu; }
		else	  { kstart=yu; kend=yd; }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates: a,b,r */
			/*Note: y=k */
			/* Note: Necessary for precesion check! */
			//a=(-1.0*(x-x1s)*(y2s-y1s)+(k-y1s)*(x2s-x1s))/fcheck1;
			//b=(-1.0*(x-x2s)*(y0s-y2s)+(k-y2s)*(x0s-x2s))/fcheck2;
			a=(-1.0*(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/fcheck1;
			b=(-1.0*(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/fcheck2;

#if 1 /* TEST: ------------------------------- */
			if(a!=a) egi_dpstd("a is NaN!\n");
			if(b!=b) egi_dpstd("b is NaN!\n");
#endif

			/* Normalize a/b/r */
			if(a<0)a=-a; if(b<0)b=-b;
			ftmp=a+b;
			if(ftmp>1.0f+0.001) {
				//egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
				a=a/ftmp; b=b/ftmp;
			}
			if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			if(r<0.0f) r=0.0f; //continue;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
#if 1 /* TEST: ---- */
			if(locimg >imgh*imgw-1) locimg=imgh*imgw-1;
#endif
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
	    		    if(imgbuf->alpha)
	  			 fb_dev->pixalpha=imgbuf->alpha[locimg];

                            draw_dot(fb_dev, x, k);
			}
#if 1 /* TEST: -------------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}
	}

	ymu=klr*(i-1)+points[nl].y; //ymu=yu; yu MAYBE replaced by yd!
	//for( i=0; i<points[nr].x-points[nm].x; i++)
	for( i=0; i< points[nr].x-points[nm].x+1; i++)
	{
		yu=roundf(klr*i+ymu);
		yd=roundf(kmr*i+points[nm].y);
		//egi_dpstd("nr-nm: yu=%d, yd=%d\n", yu,yd);

		/* Cal. x */
		x=points[nm].x+i;

		if(yu>yd) { kstart=yd; kend=yu; }
		else	  { kstart=yu; kend=yd; }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates: a,b,r */
			// y=k;
			/* Note: Necessary for precesion check! */
			//a=(-1.0*(x-x1s)*(y2s-y1s)+(k-y1s)*(x2s-x1s))/fcheck1;
			//b=(-1.0*(x-x2s)*(y0s-y2s)+(k-y2s)*(x0s-x2s))/fcheck2;
			a=(-1.0*(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/fcheck1;
			b=(-1.0*(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/fcheck2;

#if 1 /* TEST: ------------------------------- */
			if(a!=a) egi_dpstd("a is NaN!\n");
			if(b!=b) egi_dpstd("b is NaN!\n");
#endif

			/* Normalize a/b/r */
			if(a<0)a=-a; if(b<0)b=-b;
			ftmp=a+b;
			if(ftmp>1.0f+0.001) {
				//egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
				a=a/ftmp; b=b/ftmp;
			}
			if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			if(r<0.0f) r=0.0; //continue;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
#if 1 /* TEST: ---- */
			if(locimg >imgh*imgw-1) locimg=imgh*imgw-1;
#endif
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
	    		    if(imgbuf->alpha)
	  			 fb_dev->pixalpha=imgbuf->alpha[locimg];

                            draw_dot(fb_dev, x, k);
			}
#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}
	}

}

#else  //////////////////////// with float z0, float z1, float z2 //////////////////////

void egi_imgbuf_mapTriWriteFB3(EGI_IMGBUF *imgbuf, FBDEV *fb_dev,
				      float u0, float v0,
				      float u1, float v1,
				      float u2, float v2,
				      int x0, int y0,
				      int x1, int y1,
				      int x2, int y2,
				      float z0, float z1, float z2 )
{
	/* Check input data */
	if( imgbuf==NULL || imgbuf->imgbuf==NULL ) {
		egi_dpstd("Input EGI_IMBUG is NULL or uninitiliazed!\n");
		return;
	}
	int imgw=imgbuf->width;
	int imgh=imgbuf->height;

	/* Barycentric coordinates (a,b,r) for points inside the triangle:
	 * P(x,y)=a*A + b*B + r*C;  where a+b+r=1.0.
	 */
	float a, b, r, ftmp;
	float u,v;
	int x; //y;

	/* As 2D */
	struct {
		int x; int y; //float z;
	} points[3];
	points[0].x=x0; points[0].y=y0; //points[0].z=z0;
	points[1].x=x1; points[1].y=y1; //points[1].z=z1;
	points[2].x=x2; points[2].y=y2; //points[2].z=z2;

	int i, k, kstart, kend;
	int nl=0,nr=0; /* left and right point index */
	int nm; /* mid point index */

	float klr,klm,kmr;
        float  fcheck1,fcheck2;

#if 0   /* NO MORE USE TO SUBSTITUE x/y, Instead, use us/ue/vs/ve to inertpolate u/v at a line. */
        /* If 3 points are collinear, substitue x0/y0 later... */
        int x0s=x0, x1s=x1, x2s=x2;
        int y0s=y0, y1s=y1, y2s=y2;
#endif

	/* use INT type */
	int yu=0;
	int yd=0;
	int ymu=0;

	float zu,zd; /* Z value for start-end point */
	float us,ue, vs,ve; /* u,v for start-end point */

	/* Imgbuf pixel data offset */
	long int locimg;

	/* Check input u/v. TODO: Reasoning... */
	if( u0<0 ) u0=1.0+u0;
	if( v0<0 ) v0=1.0+v0;
	if( u1<0 ) u1=1.0+u1;
	if( v1<0 ) v1=1.0+v1;
	if( u2<0 ) u2=1.0+u2;
	if( v2<0 ) v2=1.0+v2;


	/* --- Case 1 ---: All points are the SAME! */
	if(x0==x1 && x1==x2 && y0==y1 && y1==y2) {
//		egi_dpstd("the Tri degenerates into a point!\n");
        	/* Set a/b/r. */
		a=0.3333;    b=0.3333;    r=0.3333;

		/* Set pixz */
		fb_dev->pixz = roundf(a*z0+b*z1+r*z2);

		/* Get interpolated u,v */
		u=a*u0+b*u1+r*u2;
		//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
		v=a*v0+b*v1+r*v2;
		//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

		/* Get mapped pixel and draw_dot */
                /* image data location */
                locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf! */
		if( locimg>=0 && locimg < imgh*imgw ) {
			fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
			if(imgbuf->alpha)
				fb_dev->pixalpha=imgbuf->alpha[locimg];
                        draw_dot(fb_dev, x0, y0);
		}
#if 1 /* TEST: --------- */
		else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif

		return;
	}

	/* If all points are collinear, including the case that TWO pionts are at SAME position! */
	fcheck1 = -1.0*(x0-x1)*(y2-y1)+(y0-y1)*(x2-x1); // 1.0*(y0-y1)*(x2-x1);
	fcheck2 = -1.0*(x1-x2)*(y0-y2)+(y1-y2)*(x0-x2); // 1.0*(y1-y2)*(x0-x2);

#if 0   ////////////////////////////    NO MORE USE!   /////////////////////////////
	/* Abandoned!!!  Instead, use us/ue/vs/ve to inertpolate u/v at a line. */

	/* If it degenerates into a line:  Move one vertex a little, to make it a NEW resonable triangle! So we can
	* compute barycentric a/b/r! However, color of the midpoint will ineffective! Means ONLY one side
	* of the triangle is drawn.
	* TODO: If necessary, re_assign input x0y0~x2y2 as the NEW triangle.
        */
	if( abs(fcheck1) < 0.0001 ) {
//		egi_dpstd("fcheck1=%e, the Tri is degenerated into a line!\n", fcheck1);
		//fcheck1=0.001;   FAILS!!! a,b OR r INALID!

		/* Just move x1 OR y1 1 pixel! to avoid it's SAME as x0,y0 OR x2,y2 */
		if( x0==x1 && x1==x2 )
			x1s=x1-1;
		else   // if( y0==y1 && y1==y2)
			y1s=y1-1;

		/* Recalculate fcheck */
		fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
		fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); //  1.0*(y1s-y2s)*(x0s-x2s);
//		egi_dpstd("Aft min move: fcheck1=%e, fcheck2=%e\n", fcheck1,fcheck2);

		 /* Check again! MUST NOT apply x1s/y2s both, in case it's a 45deg line! */
		if(abs(fcheck1)<0.0001 || abs(fcheck2)<0.0001) {

			/* Just Re_move x1 OR y1 1 pixel! to avoid it's SAME as x0,y0 OR x2,y2 */
			if( x0==x1 && x1==x2 )
                        	x1s=x1+1;
	                else   // if( y0==y1 && y1==y2)
                        	y1s=y1+1;
			//x1s=x1+1;  //y1s=y1+1;

			/* Recalculate fcheck */
			fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
			fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); // 1.0*(y1s-y2s)*(x0s-x2s);
//			egi_dpstd("Aft min. re_move: fcheck1=%e\n", fcheck1);
		}
	}
	if( abs(fcheck2) < 0.0001 ) {
//		egi_dpstd("fcheck2=%e, the Tri is degenerated into a line!\n", fcheck2);
		//fcheck2=0.001;   FAILS!!! a,b OR r INALID!

		/* Just move x2/y2 1 pixel! to avoid it's SAME as x1,y1 OR x0,y0 */
		if( x0==x1 && x1==x2 )
			x2s=x2-1;
		else   // if( y0==y1 && y1==y2)
			y2s=y2-1;

		/* Recalculate fcheck */
		fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
		fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); // 1.0*(y1s-y2s)*(x0s-x2s);
//		egi_dpstd("Aft min. move: fcheck1=%e, fcheck2=%e\n", fcheck1,fcheck2);

		if(abs(fcheck2)<0.0001 ||abs(fcheck1)<0.0001) {
			/* Just move x1/y1 1 pixel! to avoid it's SAME as x0,y0 OR x2,y2 */
			if( x0==x1 && x1==x2 )
				x2s=x2+1;
			else   // if( y0==y1 && y1==y2)
				y2s=y2+1;

			/* Recalculate fcheck */
			fcheck1 = -1.0*(x0s-x1s)*(y2s-y1s)+(y0s-y1s)*(x2s-x1s); // 1.0*(y0s-y1s)*(x2s-x1s);
			fcheck2 = -1.0*(x1s-x2s)*(y0s-y2s)+(y1s-y2s)*(x0s-x2s); // 1.0*(y1s-y2s)*(x0s-x2s);
//			egi_dpstd("Aft min. re_move: fcheck1=%e, fcheck2=%e\n", fcheck1,fcheck2);
		}
	}
	/* Check again, should NOT appear NOW! */
	if(abs(fcheck1)<0.0001 || abs(fcheck2)<0.0001)
		egi_dpstd("fcheck~=0!, points: {%d,%d} {%d,%d} {%d,%d}\n", x0,y0,x1,y1,x2,y2);
#endif  //////////////////////////////////////////////////////////////////////////////////////////////

	/* Cal nl, nr. just after collinear checking! */
	for(i=1; i<3; i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

	/* --- Case 2 ---: All points are collinear as a vertical line. */
	if(nl==nr) {
//		egi_dpstd("the Tri degenerates into a vertical line!\n");

		/* Init yu yd, u,v. zu,zd */
		yu=points[0].y;  ue=u0; ve=v0;
		yd=points[0].y;  us=u0; vs=v0;
		zu=zd=z0;

		/* Get yu,yd, zu,zd,  us/ue/vs/ve */
		for(i=1; i<3; i++) {
			if(points[i].y>yu) {
				yu=points[i].y;
				if(i==1) { ue=u1; ve=v1; zu=z1;}
				else     { ue=u2; ve=v2; zu=z2;} //i==2
			}
			if(points[i].y<yd) {
				yd=points[i].y;
				if(i==1) { us=u1; vs=v1; zd=z1;}
				else     { us=u2; vs=v2; zd=z2;} //i==2
			}
		}

		x=points[0].x;
		for(k=yd; k<=yu; k++) {
			/* Get interpolated u,v */
			u=us+(ue-us)*(k-yd)/(yu-yd);
			v=vs+(ve-vs)*(k-yd)/(yu-yd);

			/* Set pixZ */
			fb_dev->pixz = roundf(zd +(zu-zd)*(k-yd)/(yu-yd));

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
			    if(imgbuf->alpha)
				  fb_dev->pixalpha=imgbuf->alpha[locimg];
                            draw_dot(fb_dev, x, k);
			}
#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}

		return;
	}

	/* ---- Case 3 ---: All points are collinear as an oblique/horizontal line. */
	/* Note:
	 *	1. You may skip case_3, to let Case_4 draw the line, however it draws ONLY discrete
	 *	   dots for a steep line.
	 *	2. Color of the midpoint of the line will be ineffective!
	 * 	   TODO: NEW algrithm for interpolation color at a three_point line.
	 */
	if( (x0-x1)*(y2-y1)==(y0-y1)*(x2-x1) || (x1-x2)*(y0-y2)==(y1-y2)*(x0-x2) )
	{
//		egi_dpstd("the Tri degenerates into an oblique/horiz line!\n");

		int j;

                int x1=points[nl].x;
                int y1=points[nl].y;
		float pixz1= (nl==0?z0:(nl==1?z1:z2));

                int x2=points[nr].x;
                int y2=points[nr].y;
		float pixz2= (nr==0?z0:(nr==1?z1:z2));

		int tekxx=x2-x1;
		int tekyy=y2-y1;

		float foff;
		float flen=sqrtf(1.0*(x2-x1)*(x2-x1)+1.0*(y2-y1)*(y2-y1));

		int tmp;
//		EGI_16BIT_COLOR colorTmp; /* Color corresponds to k(y)=tmp */
		EGI_16BIT_COLOR colorJ;   /* Color corresponds to k(y)=J */
		EGI_16BIT_COLOR color;	  /* draw_dot() pixel color */

		/* NOW: x2>x1 */
		tmp=y1;

		/* nl as start point */
		if(nl==0) { us=u0; vs=v0; }
		else if(nl==1) { us=u1; vs=v1; }
		else { us=u2; vs=v2; }

		/* nr as end point */
		if(nr==0) { ue=u0; ve=v0; }
		else if(nr==1) { ue=u1; ve=v1; }
		else { ue=u2; ve=v2; }

#if 0  ////////////* image data location for start point nl */
                locimg=(roundf(vs*imgh))*imgw+roundf(us*imgw); /* roundf */
		if(locimg<0)
			locimg==0;
		else if( !(locimg < imgh*imgw) )
			locimg=imgh*imgw-1;
		colorTmp=imgbuf->imgbuf[locimg];
#endif /////////////////////////

		/* Draw all points */
		for(i=x1; i<=x2; i++) {
		     	/* j as Py */
		    	j=roundf( 1.0*(i-x1)*tekyy/tekxx+y1 );

			/* Get interpolated u,v for point (i,j) */
			u=us+(ue-us)*(i-x1)/(x2-x1);
			v=vs+(ve-vs)*(i-x1)/(x2-x1);

			/* Set pixZ */
			fb_dev->pixz = roundf(pixz1+(pixz2-pixz1)*(i-x1)/(x2-x1));

                        /* Image pixel color location, (i,j) */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			if( locimg>=0 && locimg < imgh*imgw )
			    colorJ=imgbuf->imgbuf[locimg];
			else
			    continue;

			if(y2>=y1) {
				/* Traverse tmp (+)-> pY */
				for(k=tmp; k<=j; k++) {
					if(tmp==j) {
						fbset_color2(fb_dev, colorJ);
						draw_dot(fb_dev,i,k);
						break;
					}

					/* Get interpolated u,v */
					foff=sqrtf(1.0*(i-x1)*(i-x1)+1.0*(k-y1)*(k-y1));
					u=us+(ue-us)*foff/flen;
					v=vs+(ve-vs)*foff/flen;

		                        /* Image pixel color location */
                        		locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			                if( locimg>=0 && locimg < imgh*imgw )
                            			color=imgbuf->imgbuf[locimg];
                        		else
                            			continue;

				     	/* Set color for point(i,k) */
 				     	//egi_16bitColor_interplt( colorTmp, colorJ, 0, 0, (k-tmp)*(1<<15)/(j-tmp), &color, NULL);
					fbset_color2(fb_dev, color);

				     	draw_dot(fb_dev,i,k);
				}
			}
			else {  /* y2<y1, Traverse tmp (-)-> pY*/
				for(k=tmp; k>=j; k--) {
					if(tmp==j) {
						fbset_color2(fb_dev, colorJ);
						draw_dot(fb_dev,i,k);
						break;
					}

					/* Get interpolated u,v */
					foff=sqrtf(1.0*(i-x1)*(i-x1)+1.0*(k-y1)*(k-y1));
					u=us+(ue-us)*foff/flen;
					v=vs+(ve-vs)*foff/flen;

		                        /* Image pixel color location */
                        		locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
			                if( locimg>=0 && locimg < imgh*imgw )
                            			color=imgbuf->imgbuf[locimg];
                        		else
                            			continue;

				     	/* Color for point(i,k) */
 				     	//egi_16bitColor_interplt( colorTmp, colorJ, 0, 0, (tmp-k)*(1<<15)/(tmp-j), &color, NULL);
					fbset_color2(fb_dev, color);
			    		if(imgbuf->alpha)
				  		fb_dev->pixalpha=imgbuf->alpha[locimg];

				     	draw_dot(fb_dev,i,k);
				}
			}

			tmp=j; /* Renew tmp as j(pY) */
//			colorTmp=colorJ; /* Renew colroTmp as color J */
		}

		return;
	}


	/* ---- Case 4 ---: As a true triangle. */

	/* Get x_mid point index, NOW: nl != nr. */
	nm=3-nl-nr;

	/* Ruled out (points[nr].x == points[nl].x), as nl==nr.  */
	//if(nl!=nr)
	       klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);
	//else
	//       klr=1000000.0;

	if(points[nm].x != points[nl].x) {
		klm=1.0*(points[nm].y-points[nl].y)/(points[nm].x-points[nl].x);
	}
	else
		klm=1000000.0;

	if(points[nr].x != points[nm].x) {
		kmr=1.0*(points[nr].y-points[nm].y)/(points[nr].x-points[nm].x);
	}
	else
		kmr=1000000.0;

	/* Draw lines for two tri */
	//for( i=0; i< points[nm].x-points[nl].x; i++)
	for( i=0; i< points[nm].x-points[nl].x+1; i++)
	{
		yu=roundf(klr*i+points[nl].y);
		yd=roundf(klm*i+points[nl].y);
		//egi_dpstd("nm-nl: yu=%d, yd=%d\n", yu,yd);

		/* Cal. x */
		x=points[nl].x+i;

		if(yu>yd) { kstart=yd; kend=yu; }
		else	  { kstart=yu; kend=yd; }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates: a,b,r */
			/*Note: y=k */
			/* Note: Necessary for precesion check! */
			//a=(-1.0*(x-x1s)*(y2s-y1s)+(k-y1s)*(x2s-x1s))/fcheck1;
			//b=(-1.0*(x-x2s)*(y0s-y2s)+(k-y2s)*(x0s-x2s))/fcheck2;
			a=(-1.0*(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/fcheck1;
			b=(-1.0*(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/fcheck2;

#if 1 /* TEST: ------------------------------- */
			if(a!=a) egi_dpstd("a is NaN!\n");
			if(b!=b) egi_dpstd("b is NaN!\n");
#endif

			/* Normalize a/b/r */
			if(a<0)a=-a; if(b<0)b=-b;
			ftmp=a+b;
			if(ftmp>1.0f+0.001) {
				//egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
				a=a/ftmp; b=b/ftmp;
			}
			if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			if(r<0.0f) r=0.0f; //continue;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Set pixz */
			fb_dev->pixz = roundf(a*z0+b*z1+r*z2);

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
#if 1 /* TEST: ---- */
			if(locimg >imgh*imgw-1) locimg=imgh*imgw-1;
#endif
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
	    		    if(imgbuf->alpha)
	  			 fb_dev->pixalpha=imgbuf->alpha[locimg];

                            draw_dot(fb_dev, x, k);
			}
#if 1 /* TEST: -------------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}
	}

	ymu=klr*(i-1)+points[nl].y; //ymu=yu; yu MAYBE replaced by yd!
	//for( i=0; i<points[nr].x-points[nm].x; i++)
	for( i=0; i< points[nr].x-points[nm].x+1; i++)
	{
		yu=roundf(klr*i+ymu);
		yd=roundf(kmr*i+points[nm].y);
		//egi_dpstd("nr-nm: yu=%d, yd=%d\n", yu,yd);

		/* Cal. x */
		x=points[nm].x+i;

		if(yu>yd) { kstart=yd; kend=yu; }
		else	  { kstart=yu; kend=yd; }

		for(k=kstart; k<=kend; k++) {
			/* Calculate barycentric coordinates: a,b,r */
			// y=k;
			/* Note: Necessary for precesion check! */
			//a=(-1.0*(x-x1s)*(y2s-y1s)+(k-y1s)*(x2s-x1s))/fcheck1;
			//b=(-1.0*(x-x2s)*(y0s-y2s)+(k-y2s)*(x0s-x2s))/fcheck2;
			a=(-1.0*(x-x1)*(y2-y1)+(k-y1)*(x2-x1))/fcheck1;
			b=(-1.0*(x-x2)*(y0-y2)+(k-y2)*(x0-x2))/fcheck2;

#if 1 /* TEST: ------------------------------- */
			if(a!=a) egi_dpstd("a is NaN!\n");
			if(b!=b) egi_dpstd("b is NaN!\n");
#endif

			/* Normalize a/b/r */
			if(a<0)a=-a; if(b<0)b=-b;
			ftmp=a+b;
			if(ftmp>1.0f+0.001) {
				//egi_dpstd("a+b>1.0! a=%e, b=%e\n",a,b);
				a=a/ftmp; b=b/ftmp;
			}
			if(a<0.0f)a=0.0f; else if(a>1.0f)a=1.0f;
			if(b<0.0f)b=0.0f; else if(b>1.0f)b=1.0f;
			r=1.0-a-b;
			if(r<0.0f) r=0.0; //continue;

			/* Get interpolated u,v */
			u=a*u0+b*u1+r*u2;
			//if(u<0.0f)u=0.0f; else if(u>1.0f)u=1.0f;
			v=a*v0+b*v1+r*v2;
			//if(v<0.0f)v=0.0f; else if(v>1.0f)v=1.0f;

			/* Set pixz */
			fb_dev->pixz = roundf(a*z0+b*z1+r*z2);

			/* Get mapped pixel and draw_dot */
                        /* image data location */
                        locimg=(roundf(v*imgh))*imgw+roundf(u*imgw); /* roundf */
#if 1 /* TEST: ---- */
			if(locimg >imgh*imgw-1) locimg=imgh*imgw-1;
#endif
			if( locimg>=0 && locimg < imgh*imgw ) {
		            fbset_color2(fb_dev,imgbuf->imgbuf[locimg]);
	    		    if(imgbuf->alpha)
	  			 fb_dev->pixalpha=imgbuf->alpha[locimg];

                            draw_dot(fb_dev, x, k);
			}
#if 1 /* TEST: --------- */
			else egi_dpstd("Out range u,v: %e,%e\n", u,v);
#endif
		}
	}
}

#endif ///////////////////////////////////////////////////////////////////


/*-----------------------------------------------------------
Save header of EGI_IMGMOTION to an file.

TODO:  Byte order compatibility for saving a file.

@fpath:		Path for the saved file.
@width/height:	Size of motion image.
@delayms:	Delayms for each motion picture.
@compress:	0--Imgbuf saved as uncompressed.
		1--Compressed.

//@motion:	Pointer to an EGI_IMGMOTION

Return:
	>0	File exists.
	0	Ok
	<0	Fails.
------------------------------------------------------------*/
int egi_imgmotion_saveHeader(const char *fpath, int width, int height, int delayms, int compress)
{
	int ret=0;
	FILE *fil;
	int nmemb;
	int nwrite;

	/* Set motion params */
	EGI_IMGMOTION  motion={ .width=width, .height=height, .frames=0, .compress=compress, .delayms=delayms };

	/* Set HeadSize */
	motion.headsize=sizeof(EGI_IMGMOTION);

	if(fpath==NULL)
		return -1;

	/* Check if file exists */
        if( access(fpath, F_OK) ==0 ) {
		egi_dpstd("Motion file '%s' already exist!\n", fpath);
		return 2;   /* HK2022-10-19 */
	}

        /* Open/create file */
        fil=fopen(fpath, "wbe");
        if(fil==NULL) {
                egi_dpstd("Fail to open file '%s' for write. ERR:%s\n", fpath, strerror(errno));
                return -3;
        }

	/* Write header info. */
	nmemb=1;
	printf("sizeof(EGI_IMGMOTION)=%d\n", sizeof(EGI_IMGMOTION));
	nwrite=fwrite((void*)&motion, sizeof(EGI_IMGMOTION), nmemb, fil);
        if(nwrite < nmemb) {
        	if(ferror(fil))
                	egi_dperr("Fail to write header of motion to '%s'.\n", fpath);
                else
                	egi_dpstd("Error! fwrite header of motion %d itmes of total %d itmess.\n", nwrite, nmemb);
                ret=-4;
                goto END_FUNC;
        }

END_FUNC:
        /* Close fil */
        if( fclose(fil) !=0 ) {
                egi_dperr("Fail to close file '%s'.\n",fpath);
                ret=-5;
        }

        return ret;
}


/*-----------------------------------------------------------
Save to append a frame to an EGI_IMGMOTION in a file, the file
shall exist and hold at least an IMGMOTION header.
If input imgbuf is NULL, then it just print motion header.

TODO:
1. Byte order compatibility for saving a file.
2. Compress between frames: differential vector.

@fpath:		Path for the saved file.
@imgbuf:	Pointer to EGI_IMGBUF.
		If NULL, just print motion header.

Return:
	0	Ok
	<0	Fails
------------------------------------------------------------*/
#include <zlib.h>
int egi_imgmotion_saveFrame(const char *fpath, EGI_IMGBUF *imgbuf)
{
	int ret=0;
	FILE *fil;
	int nmemb;
	int nread;
	int nwrite;
	int framesize;
	EGI_IMGMOTION  motion={ .width=0, .height=0, .frames=0, .delayms=0,  }; /* header */
	EGI_IMGFRAME  frameHead;

	/* For zlib compress */
	char *destBuff=NULL;

	/* Check */
	if(fpath==NULL) // || imgbuf==NULL)
		return -1;

	/* 1. Check if file exists */
        if( access(fpath, F_OK) !=0 ) {
		egi_dpstd("Motion file '%s' does NOT exist!\n", fpath);
		return -2;
	}

        /* 2. Open file for write/read */
        fil=fopen(fpath, "r+be"); /* w+ will truncate! */
        if(fil==NULL) {
                egi_dperr("Fail to open file '%s' for reading and appending. \n", fpath);
                return -3;
        }

	/* 3. Read motion header */
	nmemb=1;
	nread=fread((void*)&motion, sizeof(EGI_IMGMOTION), nmemb, fil);
        if(nread < nmemb) {
        	if(ferror(fil))
                	egi_dperr("Fail to read header of motion from '%s'.\n", fpath);
                else
                	egi_dpstd("Error! fread header of motion %d itmes of total %d itmess.\n", nread, nmemb);
                ret=-3;
                goto END_FUNC;
        }

#if 1 /* TEST: -------- */
	egi_dpstd("Motion header: Headsize=%d, Width=%d, Height=%d, frames=%d, delayms=%d.\n",
				  motion.headsize, motion.width, motion.height, motion.frames, motion.delayms );
#endif

	/* 4. Check input imgbuf */
	if( imgbuf==NULL )
		goto END_FUNC;

	/* 5. Check frame size */
	if( imgbuf->width != motion.width || imgbuf->height != motion.height ) {
		egi_dpstd("Size of imgbuf %dx%d is NOT same as motion %dx%d!\n",
					imgbuf->width,imgbuf->height, motion.width, motion.height);
		ret=-4;
		goto END_FUNC;
	}

	/* 5.1 Get framesize */
	framesize= imgbuf->height*imgbuf->width*2;

	/* 6. Modify motion.frames */
	rewind(fil); /* To SEEK_SET */

	motion.frames +=1; /* Increase frames */
        nmemb=1;
        nwrite=fwrite( (void*)&motion, sizeof(EGI_IMGMOTION), nmemb, fil);
        if(nwrite < nmemb) {
        	if(ferror(fil))
                	egi_dperr("Fail to write header of motion to '%s'.\n", fpath);
                else
                	egi_dpstd("Error! fwrite header of motion %d itmes of total %d itmess.\n", nwrite, nmemb);
                ret=-5;
                goto END_FUNC;
        }

	/* 7. Fseek to file end, to append a new frame for the motion. */
	if( fseek(fil, 0L, SEEK_END) !=0 ) {
		egi_dperr("Fail fseek.");
		ret=-6;
		goto END_FUNC;
	}

	/* 8.0 If Uncompressed data, as 16bit color. */
	if( motion.compress==0 ) {
		nmemb=1;
		nwrite=fwrite((void *)imgbuf->imgbuf, framesize, nmemb, fil); /* 16bits color */
	        if(nwrite < nmemb) {
        		if(ferror(fil))
                		egi_dperr("Fail to write header of motion to '%s'.\n", fpath);
	                else
        	        	egi_dpstd("WARNING! fwrite header of motion %d itmes of total %d itmess.\n", nwrite, nmemb);
                	ret=-7;
                        goto END_FUNC;
		}
        }
	/* 8.1 If Zlib compressed, TODO TYPE */
	else if( motion.compress ==1 ) {
		unsigned long rawsize=framesize;
		/* Min. dest buffer size */
        	unsigned long dbsize=compressBound(rawsize);
        	egi_dpstd("dbsize=%lu\n", dbsize);
		unsigned long destLen=dbsize;   /* compressed data size, uLongf == unsinged long far */

		/* Calloc destBuff */
		destBuff=calloc(1, dbsize);
		if(destBuff==NULL) {
			egi_dperr("Calloc fail!");
			goto END_FUNC;
		}

		/* compress2 (Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level) */
	        if( compress2((Bytef *)destBuff, &destLen, (Bytef *)imgbuf->imgbuf, rawsize, 9) !=Z_OK) {  /* LEVEL 0~9 */
        	        egi_dpstd("Zlib compress2 error!\n");
			goto END_FUNC;
        	}
		egi_dpstd("Succeed to compress %ld bytes imgbuf into %ld Bytes!\n", rawsize, destLen);

		/* Wrtie compressed data */
		/* write frame header */
		frameHead.datasize=destLen;
		nmemb=1;
		nwrite=fwrite((void *)&frameHead, sizeof(EGI_IMGFRAME), nmemb, fil); /* 16bits color */
	        if(nwrite < nmemb) {
        		if(ferror(fil))
                		egi_dperr("Fail to write frameHeader to '%s'.\n", fpath);
	                else
        	        	egi_dpstd("WARNING! fwrite header of frame %d itmes of total %d itmess.\n", nwrite, nmemb);
                	ret=-8;
                        goto END_FUNC;
		}

		/* write compressed head data */
		nmemb=1;
		nwrite=fwrite((void *)destBuff, destLen, nmemb, fil); /* 16bits color */
	        if(nwrite < nmemb) {
        		if(ferror(fil))
                		egi_dperr("Fail to write compressed frame data to '%s'.\n", fpath);
	                else
        	        	egi_dpstd("WARNING! fwrite compressed frame data %d itmes of total %d itmess.\n", nwrite, nmemb);
                	ret=-9;
                        goto END_FUNC;
		}
	}
	else {
		egi_dpstd("Undefined compress type!\n");
		ret=-10;
		goto END_FUNC;
	}

END_FUNC:
	/* Free destBuff */
	if(destBuff) free(destBuff);

        /* Close fil */
        if( fclose(fil) !=0 ) {
                egi_dperr("Fail to close file '%s'.\n",fpath);
                ret=-8;
        }

        return ret;
}


/*----------------------------------------------------
Play an IMGMOTION as saved in the file.

TODO:  Byte order compatibility for reading saved file.

@fpath:		Path for the saved file.
@fbdev: 	Pointer to FBDEV
@delayms:	If>0, Delay for each frame.
		OR use motion.delayms.
@x0,y0:		Origin of playing window, relative
		to screen coord.

Return:
	0	Ok
	<0	Fails
---------------------------------------------------*/
int egi_imgmotion_playFile(const char *fpath, FBDEV *fbdev, int delayms, int x0, int y0)
{
	int ret=0;
	int k;
	EGI_IMGMOTION  motion={ .width=0, .height=0, .frames=0, .delayms=0 }; /* header */
	int framesize;
	off_t off;
	EGI_IMGBUF     *simg=NULL;
	EGI_FILEMMAP   *fmap;

	EGI_IMGFRAME  frameHead;
	EGI_16BIT_COLOR	*colors=NULL;

	if(fpath==NULL || fbdev==NULL)
		return -1;

	/* 1. Mmap file */
	fmap=egi_fmap_create(fpath, 0, PROT_READ, MAP_PRIVATE);
	if(fmap==NULL) {
		egi_dpstd("Fail to mmap '%s'!\n", fpath);
		return -2;
	}
	egi_dpstd("Motion file size fsize=%jd Bytes\n", fmap->fsize); /* %jd for off_t */

	/* 2. Read motion file header */
	memcpy(&motion, fmap->fp, sizeof(EGI_IMGMOTION));

#if 1 /* TEST: -------- */
	egi_dpstd("Motion header: Headsize=%d, Width=%d, Height=%d, frames=%d, delayms=%d.\n",
				  motion.headsize, motion.width, motion.height, motion.frames, motion.delayms );
#endif

	/* Allocate simg */
	simg=egi_imgbuf_alloc();
	if(simg==NULL) {
		egi_dperr("Fail to allocate simg!\n");
		ret=-4;
		goto END_FUNC;
	}
	simg->width=motion.width;
	simg->height=motion.height;

	/* Frame size */
	framesize=simg->width*simg->height*2; /* 16bits color */

	/* Play all frames */
	/* Case: Uncompressed frames */
	if( motion.compress==0 ) {
	   off=sizeof(EGI_IMGMOTION);
	   for(k=0; k<motion.frames; k++) {
		/* Check whether the file is corrupted, with error size. */
		if( off+framesize > fmap->fsize ) {
			egi_dpstd("Motion file data broken when frame k=%d! off+framesize=%jd, fsize=%jd\n",
						k, off+framesize, fmap->fsize );
			break;
		}

		/* Get ref of imgbuf */
		simg->imgbuf=(EGI_16BIT_COLOR *)(fmap->fp+off);

		egi_dpstd("Displaying frame %d/(%d-1)...\n", k, motion.frames);
		egi_subimg_writeFB(simg, fbdev, 0, -1, x0, y0);
		fb_render(fbdev);
		if(delayms>0)
			tm_delayms(delayms);
		else
			tm_delayms(motion.delayms);

		/* Offset to next frame */
		off +=framesize;
	   }
	}
	/* Case: Compressed frames */
	else if( motion.compress==1 ) {
	    unsigned long int destLen=framesize;
	    /* Allocate colors */
	    colors=malloc(framesize);
	    if(colors==NULL) {
		egi_dperr("malloc fails!");
		goto END_FUNC;
	    }

	    off=sizeof(EGI_IMGMOTION);
	    for(k=0; k<motion.frames; k++) {

		/* C1. Check whether the file is corrupted, with error size. */
		if( off+sizeof(EGI_IMGFRAME) > fmap->fsize ) {
			egi_dpstd("Motion file data broken when frame k=%d! off=%jd, fsize=%jd\n",
						  k, off, fmap->fsize );
			break;
		}

		/* C2. Get frame header */
		memcpy(&frameHead, fmap->fp+off, sizeof(EGI_IMGFRAME));

		/* C3. offset to compressed frame data */
		off +=sizeof(EGI_IMGFRAME);

		/* C4. Check whether the file is corrupted, with error size. */
		if( off+frameHead.datasize > fmap->fsize ) {
			egi_dpstd("Motion file data broken when frame k=%d! off=%jd, compress size=%zd, fsize=%jd\n",
						  k, off, frameHead.datasize, fmap->fsize );
			break;
		}

	    	/* C5. Uncompress frame data to colors,  init. destLen=frameszie
		  (Bytef *dest,uLongf *destLen, const Bytef *source, uLong sourceLen) */
            	if(uncompress((Bytef *)colors, &destLen, (Bytef *)fmap->fp+off, frameHead.datasize)!=Z_OK) {
                	printf("Zlib uncompress error!\n");
                	exit(1);
            	}
		egi_dpstd("Succeed to uncompress frame %d/(%d-1).\n", k, motion.frames);

		/* C6. Get ref of imgbuf */
		simg->imgbuf=colors;

		egi_dpstd("Displaying frame %d/(%d-1)...\n", k, motion.frames);
		egi_subimg_writeFB(simg, fbdev, 0, -1, x0, y0);
		fb_render(fbdev);
		if(delayms>0)
			tm_delayms(delayms);
		else
			tm_delayms(motion.delayms);

		/* C7. Offset to next frameHead */
		off +=frameHead.datasize;
	    }
	}
	else {
		egi_dpstd("Undefined compress type!\n");
		ret=-10;
		goto END_FUNC;
	}

END_FUNC:
	/* Free colors */
	if(colors) free(colors);
	/* Free simg */
	simg->imgbuf=NULL;
	egi_imgbuf_free2(&simg);
	/* Free fmap */
	egi_fmap_free(&fmap);

        return ret;
}
