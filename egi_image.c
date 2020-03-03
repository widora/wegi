/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

NOTE: Try not to use EGI_PDEBUG() here!

EGI_IMGBUF functions

Midas Zhou
-------------------------------------------------------------------*/
#include <pthread.h>
#include <math.h>
#include "egi_image.h"
#include "egi_bjp.h"
#include "egi_utils.h"
#include "egi_log.h"
#include "egi_math.h"
#include "egi_log.h"

typedef struct fbdev FBDEV; /* Just a declaration, referring to definition in egi_fbdev.h */

/*--------------------------------------------
   	   Allocate  a EGI_IMGBUF
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

	/*  ??????? necesssary ????? */
	pthread_mutex_unlock(&egi_imgbuf->img_mutex);

        /* Destroy thread mutex lock for page resource access */
        if(pthread_mutex_destroy(&egi_imgbuf->img_mutex) !=0 )
		EGI_PLOG(LOGLV_TEST,"%s:Fail to destroy img_mutex!\n",__func__);

	free(egi_imgbuf);

	egi_imgbuf=NULL; /* ineffective though...*/
}


/*-------------------------------------------------------------
Initiate/alloc imgbuf as an image canvas, with all alpha=0!!!

NOTE:
  1. !!!!WARNING!!!: No mutex operation here, the caller shall take
     care of imgbuf mutex lock.

@height		height of image
@width		width of image

Return:
	0	OK
	<0	Fails
----------------------------------------------------------------*/
int egi_imgbuf_init(EGI_IMGBUF *egi_imgbuf, int height, int width)
{
	if(egi_imgbuf==NULL)
		return -1;

	if(height<=0 || width<=0)
		return -2;

	/* empty old data if any */
	egi_imgbuf_cleardata(egi_imgbuf);

        /* calloc imgbuf->imgbuf */
        egi_imgbuf->imgbuf = calloc(1,height*width*sizeof(uint16_t));
        if(egi_imgbuf->imgbuf == NULL) {
                printf("%s: fail to calloc egi_imgbuf->imgbuf.\n",__func__);
		return -2;
        }

        /* calloc imgbuf->alpha, alpha=0, 100% canvas color. */
        egi_imgbuf->alpha= calloc(1, height*width*sizeof(unsigned char)); /* alpha value 8bpp */
        if(egi_imgbuf->alpha == NULL) {
                printf("%s: fail to calloc egi_imgbuf->alpha.\n",__func__);
		free(egi_imgbuf->imgbuf);
                return -3;
        }

        /* retset height and width for imgbuf */
        egi_imgbuf->height=height;
        egi_imgbuf->width=width;

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
	if ( egi_imgbuf_init(imgbuf, height, width) !=0 )
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
	EGI_IMGBUF *eimg=egi_imgbuf_alloc();
	if(eimg==NULL)
		return NULL;

        if( egi_imgbuf_loadjpg(fpath, eimg)!=0 ) {
                if ( egi_imgbuf_loadpng(fpath, eimg)!=0 ) {
			egi_imgbuf_free(eimg);
			return NULL;
                }
        }

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

	/* create a new imgbuf */
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

@holdon:    True: To continue to use ineimg->pcolors(palphas) as intermediate results,
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


/*-----------------------------------------------------------------------
Resize an image and create a new EGI_IMGBUF to hold the new image data.
Only size/color/alpha of ineimg will be transfered to outeimg, others
such as subimg will be ignored. )

TODO: Before scale down an image, merge and shrink it to a certain size first!!!

NOTE:
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
@width:		Width for new image.
		If width<=0 AND height>0: adjust width/height proportional to oldwidth/oldheight.
		If width<2, auto. adjust it to 2.

@height:	Height for new image.
		If heigth<=0 AND width>0: adjust width/height proportional to oldwidth/oldheight.
		If height<2, auto. adjust it to 2.
Return:
	A pointer to EGI_IMGBUF with new image 		OK
	NULL						Fails
------------------------------------------------------------------------*/
EGI_IMGBUF  *egi_imgbuf_resize( const EGI_IMGBUF *ineimg,
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


	if( ineimg==NULL || ineimg->imgbuf==NULL ) //|| width<=0 || height<=0 ) adjust to 2
		return NULL;

	unsigned int oldwidth=ineimg->width;
	unsigned int oldheight=ineimg->height;

	bool alpha_on=false;
	if(ineimg->alpha!=NULL)
			alpha_on=true;

	/* If W or H is <=0: Adjust width/height proportional to oldwidth/oldheight */
	if(width<1 && height<1)
		return NULL;
	else if(width<1)
		width=height*oldwidth/oldheight;
	else if(height<1)
		height=width*oldheight/oldwidth;

	/* adjust width and height to 2, ==1 will cause Devide_By_Zero exception. */
	if(width<2) width=2;
	if(height<2) height=2;

#if 0 /* ----- FOR TEST ONLY ----- */
	/* create temp imgbuf */
	tmpeimg= egi_imgbuf_create(oldheight, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced later */
	if(tmpeimg==NULL) {
		return NULL;
	}
	if(!alpha_on) {
		free(tmpeimg->alpha);
		tmpeimg->alpha=NULL;
	}
#endif /* ----- TEST ONLY ----- */

	/* create output imgbuf */
	outeimg= egi_imgbuf_create( height, width, 0, 0); /* (h,w,alpha,color) alpha/color will be replaced later */
	if(outeimg==NULL) {
		return NULL;
	}
	if(!alpha_on) {
		free(outeimg->alpha);
		outeimg->alpha=NULL;
	}

	/* if same size, just memcpy data */
	if( height==ineimg->height && width==ineimg->width ) {
		memcpy( outeimg->imgbuf, ineimg->imgbuf, sizeof(EGI_16BIT_COLOR)*height*width);
		if(alpha_on) {
			memcpy( outeimg->alpha, ineimg->alpha, sizeof(unsigned char)*height*width);
		}

		return outeimg;
	}

	/* TODO: if height or width is the same size, to speed up */

	/* Allocate mem to hold oldheight x width image for intermediate processing */
	icolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(oldheight,width*sizeof(EGI_16BIT_COLOR));
	if(icolors==NULL) {
		printf("%s: Fail to malloc icolors.\n",__func__);
		egi_imgbuf_free(outeimg);
		return NULL;
	}
	if(alpha_on) {
	   ialphas=egi_malloc_buff2D(oldheight,width*sizeof(unsigned char));
	   if(ialphas==NULL) {
		printf("%s: Fail to malloc ipalphas.\n",__func__);
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		return NULL;
	   }
	}

	/* Allocate mem to hold final image size height x width  */
	fcolors=(EGI_16BIT_COLOR **)egi_malloc_buff2D(height,width*sizeof(EGI_16BIT_COLOR));
	if(fcolors==NULL) {
		printf("%s: Fail to malloc fcolors.\n",__func__);
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		if(alpha_on)
			egi_free_buff2D(ialphas, oldheight);
		return NULL;
	}
	if(alpha_on) {
	    falphas=egi_malloc_buff2D(height,width*sizeof(unsigned char));
	    if(falphas==NULL) {
		printf("%s: Fail to malloc ipalphas.\n",__func__);
		egi_imgbuf_free(outeimg);
		egi_free_buff2D((unsigned char **)icolors, oldheight);
		egi_free_buff2D(ialphas, oldheight);
		egi_free_buff2D((unsigned char **)fcolors, height);
		return NULL;
	    }
	}

	printf("%s: height=%d, width=%d, oldheight=%d, oldwidth=%d \n", __func__,
								height, width, oldheight, oldwidth );

	/* get new rowsize in bytes */
	color_rowsize=width*sizeof(EGI_16BIT_COLOR);
	alpha_rowsize=width*sizeof(unsigned char);

	/* ----- STEP 1 -----  scale image from [oldheight_X_oldwidth] to [oldheight_X_width] */
	for(i=0; i<oldheight; i++)
	{
//		printf(" \n STEP 1: ----- row %d ----- \n",i);
		for(j=0; j<width; j++) /* apply new width */
		{
			/* Note:
			 *   1. Here ln is left point index, and rn is right point index of a row of pixels.
			 *      ln and rn are index of original image row.
			 *      The inserted new point is between ln and rn.
			 *   2. Notice that (oldwidth-1)/(width-1) is acutual width ratio.
			 */
			ln=j*(oldwidth-1)/(width-1);/* xwidth-1 is gap numbers */
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
			/* interpolate pixel color/alpha value, and store to icolors[]/ialphas[]  */
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

	/* NOTE: rowsize keep same here! Just need to scale height. */

	/* ----- STEP 2 -----  scale image from [oldheight_X_width] to [height_X_width] */
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
//	printf(" STEP 2: copy row data to outeimg...\n");
	for( i=0; i<height; i++) {
		memcpy( outeimg->imgbuf+i*width, fcolors[i], color_rowsize );
		if(alpha_on)
		    memcpy( outeimg->alpha+i*width, falphas[i], alpha_rowsize );
	}

	/* free buffers */
	printf("%s: Free buffers...\n", __func__);
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
int egi_imgbuf_resize_update(EGI_IMGBUF **pimg, unsigned int width, unsigned int height)
{
	EGI_IMGBUF  *tmpimg;

	if( pimg==NULL || *pimg==NULL )
		return -1;

	/* If same size */
	if( (*pimg)->width==width && (*pimg)->height==height )
		return 0;

	/* resize the imgbuf by egi_imgbuf_resize() */
	tmpimg=egi_imgbuf_resize(*pimg, width, height);
	if(tmpimg==NULL)
		return -2;

	/* Free original imgbuf and replaced by tmpimg */
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

1. The new imgbuf size(H&W) are made odd, so it has a symmetrical center point.
2. Only imgbuf and alpha data are created in new EGI_IMGBUF, other memebers such
   as subimgs are ignored hence.

@eimg:		Original imgbuf;
@angle: 	Rotating angle in degree, positive as clockwise.

Return:
	A pointer to EGI_IMGBUF with new image 		OK
	NULL						Fails
--------------------------------------------------------------------------------*/
EGI_IMGBUF* egi_imgbuf_rotate(EGI_IMGBUF *eimg, int angle)
{
	int i,j;
	int m,n;
	int xr,yr;
	int width, height;	/* W,H for outimg */
	int wsin,wcos, hsin,hcos;
	int index_in, index_out;
	int ang, asign;
	EGI_IMGBUF *outimg=NULL;

        /* Normalize angle to be within [0-360] */
        ang=angle%360;      /* !!! WARING !!!  The modulo result is depended on the Compiler
			     * For C99: a%b=a-(a/b)*b	,whether a is positive or negative.
			     */
        asign= ang >= 0 ? 1 : -1; /* angle sign */
        ang= ang>=0 ? ang : -ang ;

        if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
                printf("%s: input holding eimg is NULL or uninitiliazed!\n", __func__);
                return NULL;
        }

        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0)
                mat_create_fpTrigonTab();

	/* Get size for rotated imgbuf, which shall cover original eimg at least */
#if 0	/* Float point method */
	height=0.5+abs(eimg->width*sin(ang/180.0*MATH_PI))+abs(eimg->height*cos(ang/180.0*MATH_PI));
	width=0.5+abs(eimg->width*cos(ang/180.0*MATH_PI))+abs(eimg->height*sin(ang/180.0*MATH_PI));
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
		return NULL;
	}
	/* Check ALPHA data */
	if(eimg->alpha==NULL) {
		free(outimg->alpha);
		outimg->alpha=NULL;
	}

	/* Rotation map and copy */
	m=height>>1;
	n=width>>1;
	for(i=-m; i<=m; i++) {
		for(j=-n; j<=n; j++) {
			/* Map to original coordiante (xr,yr), Origin at center. */
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
        long int screen_pixels;

        unsigned char *fbp=fb_dev->map_fb;
        uint16_t *imgbuf=egi_imgbuf->imgbuf;
        unsigned char *alpha=egi_imgbuf->alpha;

//        long int locfb=0; /* location of FB mmap, in pxiel, xxxxxbyte */
        long int locimg=0; /* location of image buf, in pixel, xxxxin byte */
//      int bytpp=2; /* bytes per pixel */

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
   screen_pixels=xres*yres;

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
For 16bits color only!!!!

WARING:
1. Writing directly to FB without calling draw_dot()!!!
   FB_FILO, Virt_FB disabled!!!
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
        /* check data */
        if(egi_imgbuf == NULL)
        {
                printf("%s: egi_imgbuf is NULL. fail to display.\n",__func__);
                return -1;
        }


	/* get mutex lock */
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
        long int screen_pixels=xres*yres;

	#ifdef ENABLE_BACK_BUFFER
        unsigned char *fbp =fb_dev->map_bk;
	#else
        unsigned char *fbp =fb_dev->map_fb;
	#endif

        uint16_t *imgbuf = egi_imgbuf->imgbuf;
        unsigned char *alpha=egi_imgbuf->alpha;
        long int locfb=0; /* location of FB mmap, in pxiel, xxxxxbyte */
        long int locimg=0; /* location of image buf, in pixel, xxxxin byte */
//      int bytpp=2; /* bytes per pixel */


  /* if no alpha channle*/
  if( egi_imgbuf->alpha==NULL )
  {
        for(i=0;i<winh;i++) {  /* row of the displaying window */
                for(j=0;j<winw;j++) {

			/* Rule out points which are out of the SCREEN boundary */
			if( i+yw<0 || i+yw>yres-1 || j+xw<0 || j+xw>xres-1 )
				continue;

                        /* FB data location */
                        locfb = (i+yw)*xres+(j+xw);

                        /* check if exceed image boundary */
                        if( ( xp+j > imgw-1 || xp+j <0 ) || ( yp+i > imgh-1 || yp+i <0 ) )
                        {
				#ifdef LETS_NOTE /*--- 4 bytes per pixel ---*/
				*(uint32_t *)(fbp+(locfb<<2))=0;
				#else		/*--- 2 bytes per pixel ---*/
			        *(uint16_t *)(fbp+(locfb<<1))=0; /* black for outside */
				#endif
                        }
                        else {
                                /* image data location */
                                locimg= (i+yp)*imgw+(j+xp);
				#ifdef LETS_NOTE /*--- 4 bytes per pixel ---*/
	         		*(uint32_t *)(fbp+(locfb<<2))=COLOR_16TO24BITS(*(imgbuf+locimg))+(255<<24);
				#else		/*--- 2 bytes per pixel ---*/
			         *(uint16_t *)(fbp+(locfb<<1))=*(uint16_t *)(imgbuf+locimg);
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
                        locfb = (i+yw)*xres+(j+xw); /*in pixel,  2 bytes per pixel */

                        if( ( xp+j > imgw-1 || xp+j <0 ) || ( yp+i > imgh-1 || yp+i <0 ) )
                        {
				#ifdef LETS_NOTE /*--- 4 bytes per pixel ---*/
				*(uint32_t *)(fbp+(locfb<<2))=0;
				#else		/*--- 2 bytes per pixel ---*/
       				*(uint16_t *)(fbp+(locfb<<1))=0;   /* black */
				#endif
                        }
                        else {
                            /* image data location, 2 bytes per pixel */
                            locimg= (i+yp)*imgw+(j+xp);

                            /*  ---- draw only within screen  ---- */
                            if( locfb>=0 && locfb <= (screen_pixels-1) ) {

                                if(alpha[locimg]==0) {           /* use backgroud color */
                                        /* Transparent for background, do nothing */
                                        //fbset_color2(fb_dev,*(uint16_t *)(fbp+(locfb<<1)));
                                }

			     #ifdef LETS_NOTE   /* ------- 4 bytes per pixel ------ */
                                else if(alpha[locimg]==255) {    /* use front color */
	         		       *(uint32_t *)(fbp+(locfb<<2))=COLOR_16TO24BITS(*(imgbuf+locimg)) \
												+(255<<24);
				}
                                else {                           /* blend */
				       *(uint32_t *)(fbp+(locfb<<2))=COLOR_16TO24BITS(*(imgbuf+locimg))	\
										     +(alpha[locimg]<<24);
				}

			     #else 		/* ------ 2 bytes per pixel ------- */
                                else if(alpha[locimg]==255) {    /* use front color */
			               *(uint16_t *)(fbp+(locfb<<1))=*(uint16_t *)(imgbuf+locimg);
				}
                                else {                           /* blend */
                                            *(uint16_t *)(fbp+(locfb<<1))= COLOR_16BITS_BLEND(
							*(uint16_t *)(imgbuf+locimg),   /* front pixel */
                                                        *(uint16_t *)(fbp+(locfb<<1)),  /* background */
                                                        alpha[locimg]  );               /* alpha value */
				}

			    #endif /*  <<< end 4/2 Bpp select  >>> */
                            }
                       } /* if  within screen */
                } /* for() */
        }/* for()  */
  }/* end alpha case */

  /* put mutex lock */
  pthread_mutex_unlock(&egi_imgbuf->img_mutex);

  return 0;
}
#endif ///////////////////////////////////////////////////////////////////////


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

	if(egi_imgbuf==NULL || egi_imgbuf->imgbuf==NULL ) {
		printf("%s: egi_imbug or egi_imgbuf->imgbuf is NULL!\n",__func__);
		return -1;;
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

/*-----------------------------------------------------------------
		        Reset Color and Alpha value
@color:		16bit color value. if <0 ingore.
@alpah:		8bit alpha value.  if <0 ingore.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------------*/
int egi_imgbuf_resetColorAlpha(EGI_IMGBUF *egi_imgbuf, int color, int alpha )
{
	int i;

	if( egi_imgbuf==NULL || egi_imgbuf->alpha==NULL )
		return -1;


	if(alpha>=0 && color>=0) {
		for(i=0; i< egi_imgbuf->height*egi_imgbuf->width; i++) {
			egi_imgbuf->alpha[i]=alpha;
			egi_imgbuf->imgbuf[i]=color;
		}
	}
	else if(alpha>=0) {
		for(i=0; i< egi_imgbuf->height*egi_imgbuf->width; i++)
			egi_imgbuf->alpha[i]=alpha;
	}
	else if( color>=0 ) {
		for(i=0; i< egi_imgbuf->height*egi_imgbuf->width; i++)
			egi_imgbuf->imgbuf[i]=color;
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
		return -1;;
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
   In order to algin all characters in same horizontal level, every char bitmap must be
   align to the same baseline, i.e. the vertical position of each char's origin
   (baseline) MUST be the same.
   So (xb,yb) should NOT be left top coordinate of a char bitmap,
   while use char's 'origin' coordinate relative to eimg canvan as (xb,yb) can align all
   chars at the same level !!!


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

@subcolor	>=0 as substituting color
		<0  use bitmap buffer data as gray value. 0-255 (BLACK-WHITE)

return:
	0	OK
	<0	fails
--------------------------------------------------------------------------------*/
int egi_imgbuf_blend_FTbitmap(EGI_IMGBUF* eimg, int xb, int yb, FT_Bitmap *bitmap,
								EGI_16BIT_COLOR subcolor)
{
	int i,j;
	EGI_16BIT_COLOR color;
	unsigned char alpha;
	unsigned long size; /* alpha size */
	int	sumalpha;
	int pos;

	if(eimg==NULL || eimg->imgbuf==NULL || eimg->height<=0 || eimg->width<=0 ) {
		printf("%s: input EGI_IMBUG is NULL or uninitiliazed!\n", __func__);
		return -1;
	}
	if( bitmap==NULL || bitmap->buffer==NULL ) {
		printf("%s: input FT_Bitmap or its buffer is NULL!\n", __func__);
		return -2;
	}
	/* calloc and assign alpha, if NULL */
	if(eimg->alpha==NULL) {
		size=eimg->height*eimg->width;
		eimg->alpha = calloc(1, size); /* alpha value 8bpp */
		if(eimg->alpha==NULL) {
			printf("%s: Fail to calloc eimg->alpha\n",__func__);
			return -3;
		}
		memset(eimg->alpha, 255, size); /* assign to 255 */
	}

	for( i=0; i< bitmap->rows; i++ ) {	      /* traverse bitmap height  */
		for( j=0; j< bitmap->width; j++ ) {   /* traverse bitmap width */
			/* check range limit */
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
			else {			/* use Font bitmap gray value */
				/* alpha=0 MUST keep unchanged! */
				if(alpha>0 && alpha<180)alpha=255; /* set a limit as for GAMMA correction, too simple! */
				color=COLOR_16BITS_BLEND( COLOR_RGB_TO16BITS(alpha,alpha,alpha),
							  eimg->imgbuf[pos], alpha );
			}
			eimg->imgbuf[pos]=color; /* assign color to imgbuf */

			/* blend alpha value */
			sumalpha=eimg->alpha[pos]+alpha;
			if( sumalpha > 255 ) sumalpha=255;
			eimg->alpha[pos]=sumalpha; //(alpha>0 ? 255:0); //alpha;
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
