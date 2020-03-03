/*-----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
	Never forget why you start! ---Just For Fun!

TODO:
1. Putting subtitle displaying codes in thdf_Display_Pic() may be better.
2. Apply EGI_FIFO for frame buffering instead of picbuff[];


Midas_Zhou
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <dirent.h>
#include <limits.h> /* system: NAME_MAX 255; PATH_MAX 4096 */

#include "egi_common.h"
#include "utils/egi_utils.h"
#include "sound/egi_pcm.h"
#include "egi_FTsymbol.h"
#include "ffmusic.h"
#include "ffmusic_utils.h"

/* in seconds, playing time elapsed for Video */
//int ff_sec_Velapsed;
int ff_sub_delays=3; /* delay sub display in seconds, relating to ff_sec_Velapsed */

/* ff control command */
enum ffmuz_cmd control_cmd;	/* Open to module caller for command convey  */

/* predefine seek position */
//long start_tmsecs=60*95; /* starting position */

/***
 * 1. Thanks to slow SPI transfering speed, producing speed is usually greater than consuming speed.
 * 2. Following keys to be initialized in ff_malloc_PICbuffs().
 */
static int ifplay;		/* Current playing frame index of pPICbuffs[] */
static unsigned long nfc;	/* Total frames read from pPICbuffs and consumed */
static unsigned long nfp;	/* Total frames produced and copied to pPICbuffs */

static uint8_t** pPICbuffs=NULL; /* PIC_BUF_NUM*Screen_size*16bits, data buff for several screen pictures
				  * malloc in thread_ffplay_music(), free in display_MusicPic()
				  */

static bool IsFree_PICbuff[PIC_BUFF_NUM]={true,true,true};  /* Tag to indicate availiability of pPICbuffs[x].
						    * True:  Obsoleted data inside, ready for new data input.
						    * False: New image data inside, ready for displaying.
						    * LoadPic2Buff() put 'false' tag
						    * thdf_Display_Pic() put 'true' tag,
						    */

static long seek_Subtitle_TmStamp(char *subpath, unsigned int tmsec);
static bool bkimg_updated;  	/* TRUE: Indicating back ground image for music playing is updated,
			     	 * It will reset to FALSE when display_MusicPic() exits.
			     	 */
static bool pic_off;	    	/* TRUE: Indicating there is no picture embedded in the audio file,
			     	 *       or it's disabled by user.
			     	 */


/*-----------------------------------------------------
Init FFmuz context, allocate FFmuz_Ctx, and sort out
all media files in path.

@path           path for media files
@fext:          File extension name, MUST exclude ".",
                Example: "avi","mp3", "jpg, avi, mp3"

return:
        0       OK
        <0    Fails
------------------------------------------------------*/
int init_ffmuzCtx(char *path, char *fext)
{
        int fcount;

        FFmuz_Ctx=calloc(1,sizeof(FFMUSIC_CONTEXT));
        if(FFmuz_Ctx==NULL) {
                printf("%s: Fail to calloc FFmuz_Ctx.\n",__func__);
                return -1;
        }

        /* search for files and put to ffCtx->fpath */
        FFmuz_Ctx->fpath=egi_alloc_search_files(path, fext, &fcount);
        FFmuz_Ctx->ftotal=fcount;

        return 0;
}

/*-----------------------------------------
        Free a FFMUSIC_CONTEXT struct
-----------------------------------------*/
void free_ffmuzCtx(void)
{
        if(FFmuz_Ctx==NULL) return;

        if( FFmuz_Ctx->ftotal > 0 )
                egi_free_buff2D((unsigned char **)FFmuz_Ctx->fpath, FFmuz_Ctx->ftotal);

        free(FFmuz_Ctx);

        FFmuz_Ctx=NULL;
}


/*--------------------------------------------------------------
WARNING: !!! for 1_producer and 1_consumer scenario only !!!
Allocate memory for PICbuffs[]


width,height:	picture size
pixel_size:	in byte, size for one pixel.

Return value:
	 NULL   --- fails
	!NULL 	--- OK
----------------------------------------------------------------*/
uint8_t**  ff_malloc_PICbuffs(int width, int height, int pixel_size )
{
        int i,k;

        pPICbuffs=(uint8_t **)malloc( PIC_BUFF_NUM*sizeof(uint8_t *) );
        if(pPICbuffs == NULL) return NULL;

        for(i=0;i<PIC_BUFF_NUM;i++) {
                pPICbuffs[i]=(uint8_t *)calloc(1,width*height*pixel_size); /* default BLACK */

                /* if fails, free those buffs */
                if(pPICbuffs[i] == NULL) {
                        for(k=0;k<i;k++) {
                                free(pPICbuffs[k]);
                        }
                        free(pPICbuffs);
                        return NULL;
                }
        }

        /* set tag */
        for(i=0;i<PIC_BUFF_NUM;i++) {
                IsFree_PICbuff[i]=true;
        }

   	/* init key values */
   	nfc=0;
   	nfp=0;
   	ifplay=0;

        return pPICbuffs;
}

/*-------------------------------------------
Return a free PICbuff slot/tag number.

Return value:
        >=0  OK
        <0   fails
---------------------------------------------*/
static inline int ff_get_FreePicBuff(void)
{
        int i;
	int index;

        for(i=0;i<PIC_BUFF_NUM;i++) {
		/* check and get free slot number in preceding ifplay order. */
		index=(ifplay+i+1)&(PIC_BUFF_NUM-1);

                if(IsFree_PICbuff[index]) {

                        return index;
		}
        }

        return -1;
}

/*----------------------------------
   	Free pPICbuffs
----------------------------------*/
static void ff_free_PicBuffs(void)
{
        int i;

	if(pPICbuffs == NULL)
		return;

        for(i=0; i<PIC_BUFF_NUM; i++)
	{
		//printf("PIC_BUFF_NUM: %d/%d start to free...\n",i,PIC_BUFF_NUM);
		if(pPICbuffs[i] != NULL)  {
	                free(pPICbuffs[i]);
			pPICbuffs[i]=NULL;
		}
		//printf("PIC_BUFF_NUM: %d/%d freed.\n",i,PIC_BUFF_NUM);
	}
        free(pPICbuffs);

	pPICbuffs=NULL;
}


/*----------------------------------------------------------
		     A thread function
In a loop to display pPICBuffs[]

WARNING: !!! for 1_producer and 1_consumer scenario only!!!
-----------------------------------------------------------*/
void* display_MusicPic(void * argv)
{
   if(FFmuz_Ctx==NULL) {
	printf("%s: FFmuz_Ctx is NULL!\n",__func__);
	return (void *)-1;
   }

   int 	i;
   int  ret;
   int  index;
   int  blur_size;
   int  xsize, ysize; /* size of FB screen  */
   unsigned long nfc_tmp;
   bool still_image=false;
   char wallpaper[]="/home/musicback.jpg"; /* as for default back ground, if no embedded pic. */

   struct PicInfo *ppic =(struct PicInfo *) argv;
   EGI_IMGBUF  *tmpimg=NULL;
   EGI_IMGBUF  *imgbuf=NULL;

   imgbuf=egi_imgbuf_alloc(); /* To hold motion/still picture data */
   if(imgbuf==NULL) {
	EGI_PLOG(LOGLV_INFO,"%s: fail to call egi_imgbuf_alloc() for imgbuf.",__func__);
	return (void *)-1;
   }

   /* Get screen size */
   xsize= ff_fb_dev.vinfo.xres;
   ysize= ff_fb_dev.vinfo.yres;

   /* Check if picture OFF */
   printf("%s: Check ppic->PicOff...\n", __func__);
   if(ppic->PicOff) {	/* 1. If no picture embedded in the media file. */
	pic_off=true;
	printf("%s: No picture in the media file/Or disabled, use default wallpaper.\n", __func__);

	/* Load pic to imgbuf */
	tmpimg=egi_imgbuf_alloc();
	if(tmpimg==NULL) {
		EGI_PLOG(LOGLV_INFO,"%s: fail to call egi_imgbuf_alloc() for tmpimg.",__func__);
		return (void *)-1;
   	}
	if( egi_imgbuf_loadjpg(wallpaper,tmpimg)!=0 ) {
		if ( egi_imgbuf_loadpng(wallpaper,tmpimg)!=0 ) {
		 	 EGI_PLOG(LOGLV_ERROR, "%s: Fail to load file '%s' to tmping!", __func__, wallpaper);
			 //Go on...
		}
	}

	/* blur and resize the imgbuf  */
	blur_size=tmpimg->height/100;
	if( tmpimg->height <240 ) {	/* A. If a small size image, blur then resize */
		egi_imgbuf_blur_update( &tmpimg, blur_size, false);
  	     ret=egi_imgbuf_resize_update( &tmpimg, xsize, ysize);
	}
	else  {				/* B. If a big size image, resize then blur */
		egi_imgbuf_resize_update( &tmpimg, xsize, ysize);
	    ret=egi_imgbuf_blur_update( &tmpimg, blur_size, false);
	}
	if(ret !=0 )
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to blur and resize tmpimg!", __func__);

	/* --- Adjust Luminance --- */
	egi_imgbuf_avgLuma(tmpimg, 255/3);

        /* Free old frame_img in PAGE */
	/* !!!! NOT THREAD SAFE FOR PAGE OPERATION !!!, however we have a mutex lock in EGI_IMGBUF. */
	egi_imgbuf_free(ppic->app_page->ebox->frame_img);
	ppic->app_page->ebox->frame_img=NULL;

	/* Assign frame_img/default picture to PAGE */
	ppic->app_page->ebox->frame_img=tmpimg; tmpimg=NULL; /* Ownership transfered!!! */
	ppic->app_page->fpath=wallpaper; /* Second selection, if tmpimg == NULL */

	/* Put to page and its child eboxes for refresh */
	egi_page_needrefresh(ppic->app_page);

	/* set token */
	bkimg_updated=true;
   }
   else {	/* 2. A picture embeded in the media file. */
	   pic_off=false;
   	   /* check if it's image, 	TODO: still or motion picture */
	   if(IS_IMAGE_CODEC(ppic->vcodecID)) {
		   still_image=true;
		   imgbuf->width=ppic->width;
		   imgbuf->height=ppic->height;
		   EGI_PLOG(LOGLV_INFO,"%s: Still image width=%d, height=%d",
							__func__, imgbuf->width, imgbuf->height );
	   }  else {
		  still_image=false;
		  printf("%s: Not a recognizable image type! Maybe a motion picture!\n",__func__);
	   }
    }


   /* check picture size limit --- Ignored! */

   /* --- Loop Checking pPICbuff[] and put to display --- */
   while(1)
   {
	   /* starting from nfc_tmp, check PIC_BUFF_NUM buffs one by one */
	   nfc_tmp=nfc;
	   for(i=0; i<PIC_BUFF_NUM; i++) /* to display all pic buff in pPICbuff[] */
	   {
		/* Breadk if no picture */
		if(ppic->PicOff) break;

		index= (nfc_tmp+i) & (PIC_BUFF_NUM-1);
		if( IsFree_PICbuff[index]==false ) {
			/* set index of pPICbuff[] for current playing frame*/
			ifplay=index;
			EGI_PLOG(LOGLV_CRITICAL,"%s: Get image data from pPICbuffs[%d] and put to imgbuf.",
										 __func__, index);
			imgbuf->imgbuf=(uint16_t *)pPICbuffs[index]; /* Ownership transfered! */

		   	/* put a FREE tag after display, then it can be overwritten. */
	  	   	IsFree_PICbuff[index]=true;

			tm_delayms(25);
	   		//usleep(20000);

			/* increase number of frames consumed */
			nfc++;
		}
	   }

	  /* Still picture handling */
	  if( still_image )  {
		/* reset PAGE frame_img,
                 * Call only once, for bkimg_updated....
		 */
		if( !bkimg_updated && imgbuf->imgbuf != NULL ) { /* Only if imgbuf holds an image */
			printf("%s: Start to set imgbuf as PAGE wallpaper...\n",__func__);

	                /*--- 1. Copy a Block from the image ---*/
			/* If original image ratio is H4:W3, just same as LCD ratio. */
			if( imgbuf->height*3 == (imgbuf->width<<2) ) {
				printf("%s: embedded picture ratio 4:3\n",__func__);
				tmpimg=egi_imgbuf_blockCopy(imgbuf,  0, 0,  	       /* eimg, xp,yp */
 						imgbuf->height, imgbuf->width );       /* H, W*/
			}
			/* Else if original image is square, or whatever else....
			 * then copy a H4:W3 block from it. */
			else {
				printf("%s: embedded picture ratio is NOT 4:3\n",__func__);
				tmpimg=egi_imgbuf_blockCopy(imgbuf,  imgbuf->width>>3, 0,  /* eimg, xp,yp */
 						imgbuf->height, imgbuf->width*3/4 );       /* H, W*/
			}
			/* Assert tmpimg */
			if(tmpimg==NULL) {
 			        EGI_PLOG(LOGLV_ERROR,"%s: imgbuf block copy fails, tmpimg is NULL!",__func__);
				nfc=1; 		/* since ff_get_FreePicBuff() from 1, not 0. */
				IsFree_PICbuff[1]=false;
			    	continue; /* continue while() */
			}

			/*--- 2. Blur and resize the imgbuf ---*/
			blur_size=imgbuf->height/60; /* original imgbuf */
			if( tmpimg->height <240 ) {		/* A. If a small size image, blur then resize */
				egi_imgbuf_blur_update( &tmpimg, blur_size, false);
				egi_imgbuf_resize_update( &tmpimg, xsize, ysize);
			}
			else if(tmpimg) {			/* B. If a big size image, resize then blur */
				egi_imgbuf_resize_update( &tmpimg, xsize, ysize);
				egi_imgbuf_blur_update( &tmpimg, blur_size, false);
			}

			/* --- Adjust Luminance --- */
			egi_imgbuf_avgLuma(tmpimg, 255/3);

			/*--- 3. Put image pointer to PAGE ---*/
			/* Free old imagbuf
			 * !!!! NOT THREAD SAFE FOR PAGE OPERATION !!!, however we have a mutex lock in EGI_IMGBUF.
		         */
			egi_imgbuf_free(ppic->app_page->ebox->frame_img);
			ppic->app_page->ebox->frame_img=tmpimg; tmpimg=NULL; /* Ownership transfered! */

			/*--- 4. Need to refresh PAGE in routine ---*/
			printf("%s: set page frame_img needrefresh...\n",__func__);
			egi_page_needrefresh(ppic->app_page);/* page and its child eboxes */

			/*--- 5. Reset update token ---*/
			bkimg_updated=true;

		} /* End set PAGE frame_img as song file embedded image */

		tm_delayms(100);

		/* reset token  --- !!! Not necessary anymore, pic is put to page->ebox->frame_img. */
		#if 0
		nfc=1; 		/* since ff_get_FreePicBuff() from 1, not 0. */
		IsFree_PICbuff[1]=false;
		#endif
	  } /* End still_image handling */
	  /* ELSE: if motion pciture...TBD & TODO. */

	   /* Check command & Quit thread function */
	   if(control_cmd == cmd_exit_display_thread ) {
		EGI_PLOG(LOGLV_INFO,"%s: Exit commmand is received!",__func__);
		break;
	   }

	  tm_delayms(25);
	  //usleep(2000);

  } /* End While() */


  /* Free tmpimg */
  egi_imgbuf_free(tmpimg);
  tmpimg=NULL;

  /*  ------- Free frame_img of PAGE -----
   *  !!!! NOT THREAD SAFE FOR PAGE OPERATION !!!
   *  However we have a mutex lock in EGI_IMGBUF.
   */
  egi_imgbuf_free(ppic->app_page->ebox->frame_img);
  ppic->app_page->ebox->frame_img=NULL;

  /* Reset bkimg_updated */
  bkimg_updated=false;

  /* Free PicBuffs */
  ff_free_PicBuffs();
  imgbuf->imgbuf=NULL; /* As freed by ff_free_PicBuffs() already. */

  /* Free imgbuf, WARN: imgbuf->imgbuf refers to PICbuffs[] */
  egi_imgbuf_free(imgbuf);

  return (void *)0;
}


/*------------------------------------------------------------------------
Copy RGB data from *data to PicInfo.data

  ppic: 	a PicInfo struct
  data:		data source
  numbytes:	amount of data copied, in byte.

TODO: Pic data loading must NOT exceed displaying by one circle of PIC buff.

 Return value:
	>=0 Ok (slot number of PICBuffs)
	<0  fails
--------------------------------------------------------------------------*/
int ff_load_Pic2Buff(struct PicInfo *ppic,const uint8_t *data, int numBytes)
{
	int nbuff;

	nbuff=ff_get_FreePicBuff(); /* get a slot number */
	//printf("Load_Pic2Buff(): get_FreePicBuff() =%d\n",nbuff);

	/* only if PICBuff has free slot, and no more than PIC_BUFF_NUM(one circle of buff), then renew ppic */
	if( nbuff >= 0 && nfp-nfc < PIC_BUFF_NUM  ){
		ppic->data=pPICbuffs[nbuff]; /* get pointer to the PICBuff */
		//printf("Load_Pic2Buff(): start memcpy..\n");
		memcpy(ppic->data, data, numBytes);
		IsFree_PICbuff[nbuff]=false; /* put a NON_FREE tag to the buff slot */
		ppic->nPICbuff=nbuff;	/* put slot number */

		/* increase total number of frames produced */
		nfp++;
	}

	return nbuff;
}


/*-------------------------------------------------------------
A thread function of displaying subtitles.
Read a SRT substitle file and display it on a dedicated area.

argv*:  data for subtitle path

Note:
1. Length of each subtitle line to be 32-1.
2. Argv to pass subtitle rst file path
3. \n or \r both to be deemed as line_return codes.
4. Use "-->" to confirm as a time stamp string.

WARNING: !!! for 1_producer and 1_consumer scenario only!!!
-------------------------------------------------------------*/
void* thdf_Display_Subtitle(void * argv)
{
	if(FFmuz_Ctx==NULL) {
		printf("%s: FFmuz_Ctx is NULL!\n",__func__);
		return (void *)-1;
	}

        int subln=4; 				/* lines for subtitle displaying */
        FILE *fil;
        char *subpath=(char *)argv;
        char *pt=NULL;
        int  len; 				/* length of fgets string */
        char strtm[32]={0}; 			/* time stamp */
        int  nsect; 				/* section number */
        int  start_secs=0; 			/* sub section start time, in seconds */
        int  end_secs=0; 			/* sub section end time, in seconds */
	long off; 				/* offset to start position */
        char strsub[32*4]={0}; 			/* 64_chars x 4_lines, for subtitle content */
        EGI_BOX subbox={{0,150}, {240-1, 260}}; /* box area for subtitle display */

        /* open subtitle file */
       	fil=fopen(subpath,"r");
        if(fil==NULL) {
       	        printf("Fail to open subtitle:%s\n",strerror(errno));
               	return (void *)-2;
        }

	/* seek to the start position */
	off=seek_Subtitle_TmStamp(subpath, FFmuz_Ctx->start_tmsecs);
	fseek(fil,off,SEEK_SET);

       	/* read subtitle section by section */
        while(!(feof(fil)))
        {
	        /* 2. get time stamp first! */
	        memset(strtm,0,sizeof(strtm));
	        fgets(strtm,sizeof(strtm),fil);/* time stamp */
	        if(strstr(strtm,"-->")!=NULL) {  /* to confirm the stime stamp string */
//	                printf("time stamp: %s\n",strtm);
        	        start_secs=atoi(strtm)*3600+atoi(strtm+3)*60+atoi(strtm+6);
//                	printf("Start(sec): %d\n",start_secs);
                       	end_secs=atoi(strtm+17)*3600+atoi(strtm+20)*60+atoi(strtm+23);
//                     	printf("End(sec): %d\n",end_secs);
	        }
		else
			continue;

        	/* 3. read a section of sub and display it */
	        fbset_color(WEGI_COLOR_BLACK);
        	draw_filled_rect2(&ff_fb_dev,WEGI_COLOR_BLACK,subbox.startxy.x,subbox.startxy.y,
								subbox.endxy.x,subbox.endxy.y);
	        len=0;
        	memset(strsub,0,sizeof(strsub));
	        do {   /* read a section of subtitle */
        	       pt=fgets(strsub+len,subln*32-len-1,fil);
                       if(pt==NULL)break;
//	               printf("fgets pt:%s\n",pt);
        	       len+=strlen(pt); /* fgets also add a '\0\ */
                       if(len>=subln*32-1)
                        	        break;
	        }while( *pt!='\n' && *pt!='\r' && *pt!='\0' ); /* return code, section end token */

		/* 4. wait for a right time to display the subtitles */
		do{
		     /* check if any command comes */
		     if(control_cmd == cmd_exit_subtitle_thread ) {
			  EGI_PDEBUG(DBG_FFPLAY,"Exit commmand is received, exit thread now...!\n");
			  fclose(fil);
                	  pthread_exit( (void *)-1);
		     }

		     tm_delayms(200);
//		     printf("Elapsed time:%d  Start_secs:%d\n",ff_sec_Velapsed, start_secs);
		} while( start_secs > ff_sec_Velapsed - ff_sub_delays );

		/* 5. Disply subtitle */
       	        //symbol_strings_writeFB(&ff_fb_dev, &sympg_testfont, 240, subln, -5, WEGI_COLOR_ORANGE,
       	        symbol_strings_writeFB(&ff_fb_dev, &sympg_ascii, 240, subln, 0, WEGI_COLOR_ORANGE,
                                                                                1, 0, 170, strsub,-1);

		/* 6. wait for a right time to let go to erase the sub. */
		do{
		     /* check if any command comes */
		     if(control_cmd == cmd_exit_subtitle_thread ) {
			  EGI_PDEBUG(DBG_FFPLAY,"%s: exit commmand is received, exit thread now...!\n",
												__func__);
			  fclose(fil);
                	  pthread_exit( (void *)-2);
		     }

		     tm_delayms(200);
//		     printf("Elapsed time:%d  End_secs:%d\n",ff_sec_Velapsed, end_secs);
		} while( end_secs > ff_sec_Velapsed - ff_sub_delays );

       	        /* 7. section number or a return code, ignore it. */
                memset(strtm,0,sizeof(strtm));
       	        fgets(strtm,32,fil);
                if(*strtm=='\n' || *strtm=='\t')
       	                continue;  /* or continue if its a return */
          	else {
                   	 nsect=atoi(strtm);
//                       printf("Section: %d\n",nsect);
                }

        }/* end of sub file */

       	fclose(fil);
	return NULL;
}


/* --------------------------------------------------
Seek subtitle file, get offset with right time stamp

subpath:	path to the subtitle file.
tmsec:		time elapsed for the movie.

return:
	>0 	OK, offset
	<0 	Fails
----------------------------------------------------*/
static long seek_Subtitle_TmStamp(char *subpath, unsigned int tmsec)
{
        char strtm[32]={0}; /* time stamp */
        int  start_secs=0; /* sub section start time, seconds part */
	int  start_ms=0;   /* sub section start time, millisecond part */
        int  end_secs=0;   /* sub section end time, seconds part  */
	int  end_ms=0;	   /* sub section end time, millisecond part */
	long off=-1;
	FILE *fil;

	/* check data */
	if(subpath==NULL) return -2;
	if(tmsec==0) return 0L;

        /* open subtitle file */
       	fil=fopen(subpath,"r");
        if(fil==NULL) {
       	        printf("Fail to open subtitle:%s\n",strerror(errno));
               	return -3;
        }

	/* seek subtitle from start */
	fseek(fil,0L,SEEK_SET);
        while(!(feof(fil)))
        {
                memset(strtm,0,sizeof(strtm));
                fgets(strtm,sizeof(strtm),fil);/* time stamp */
                if(strstr(strtm,"-->")!=NULL) {  /* to confirm the time stamp string */
//                        printf("Seek time stamp: %s\n",strtm);
                        start_secs=atoi(strtm)*3600+atoi(strtm+3)*60+atoi(strtm+6);
			start_ms=atoi(strtm+9);
//                        printf("Seek Start(sec): %d\n",start_secs);
                        end_secs=atoi(strtm+17)*3600+atoi(strtm+20)*60+atoi(strtm+23);
			end_ms=atoi(strtm+9);
//                        printf("Seek End(sec): %d\n",end_secs);
                }
		/* get offset position */
		if(start_secs > tmsec-ff_sub_delays) {
			off=ftell(fil);
			printf("Seek start position at %dsec %dms.\n",start_secs, start_ms);
			break;
		}
	}

	fclose(fil);

	return off;
}


#define 	FFT_POINTS 1024
static int	fft_nx[FFT_POINTS];	/* PCM format S16, TODO: dynamic allocation */
static bool	FFTdata_ready=false;

/*--------------------------------------------------------
Load PCM data to FFT

@buffer:	PCM data buffer
		Supposed to be noninterleaved, format S16L.
@nf     	Number of input audio frames
---------------------------------------------------------*/
void ff_load_FFTdata(void ** buffer, int nf)
{
	int i;

	/* load data only FFT is available */
	if(!FFTdata_ready) {

	        /* convert PCM buff[] to FFT nx[] */
		memset(fft_nx,0,sizeof(fft_nx));
		if(nf>FFT_POINTS)
			nf=FFT_POINTS;
	       	for(i=0; i<nf; i++) {
			/* Arithmetical shifting, trim amplitude to Max 2^11 */
        	        fft_nx[i]=((int16_t **)buffer)[0][i]>>4;
	       	}

		FFTdata_ready=true;
	}
}


/*------------------------------------------------------------------------
Display audio data spectrum.

Note:
1. Now only for 44100 sample rate pcm data,format noninterleaved S16.
   48000 sample rate also OK.
2. Max value of S16 PCM data is trimmed to be Max.2^11, to prevent
   egiFFFT() overflow.
3. If CPU is too busy, the spectrum displaying may lag behind audio playing.

@buffer:	PCM data buffer
		Supposed to be noninterleaved, format S16L.
@nf     	Number of input audio frames

Note:
	1. 1024 points FFT inside, if nf<1024, paddled with 0.
	2. FBDEV *fbdev shall be initialized by the caller.
--------------------------------------------------------------------------*/
void*  ff_display_spectrum(void *argv)
{
	int i,j;
	FBDEV fbdev={ 0 };

        /* for FFT, nexp+aexp Max. 21 */
        unsigned int    nexp=10;		/* !!!2^nexp=FFT_POINTS */
        unsigned int    aexp=11;        	/* Max. Amp=1<<aexp */
        unsigned int    np=1<<nexp;  		/* input element number for FFT */
        EGI_FCOMPLEX    ffx[FFT_POINTS];        /* FFT result */
static  EGI_FCOMPLEX	wang[FFT_POINTS];
static  EGI_FVAL	hamming[FFT_POINTS];    /* Hamming window factor */
static  bool	factors_ready=false;
        unsigned int    ns=1<<5;//6;    	/* points for spectrum diagram */
        unsigned int    avg;
        int             ng=np/ns;       	/* 32 each ns covers ng numbers of np */
        int             step;
        int             sdx[32];//ns    	/* displaying points  */
        int             sdy[32];
        int             sx0;
        int             spwidth=200;    	/* displaying width for the spectrum diagram */
	int		hlimit=120; //60;	/* displaying spectrum height limit */
	int		dybase=210;     	/* Y, base line for spectrum */
	int		dylimit=dybase-hlimit;
	int		nk[32]=			/* sort index of ffx[] for sdx[], ng=32 */
	{   2, 4, 8, 16, 24, 32, 40, 48,
            64, 72, 80, 88, 96, 104, 112, 120,
	    128, 136, 144, 152, 160, 168, 176, 184,
	    200, 216, 232, 248, 264, 280, 296, 312
	};

	/* init FBDEV */
	if( init_fbdev(&fbdev) !=0 )
		return (void*)-1;
	fbdev.pixcolor_on=true;
	//fbdev.pixcolor=WEGI_COLOR_WHITE;//CYAN; /* set FBDEV pixcolor */
	fbdev.pixcolor=egi_color_random(color_light);

	/* init sdx[] */
       	sx0=(240-spwidth/(ns-1)*(ns-1))/2;
        for(i=0; i<ns; i++) {
       	        sdx[i]=sx0+(spwidth/(ns-1))*i;
        }

	/* Prepare factors */
	if(!factors_ready) {
		for(i=0; i<np; i++) {
		        /* prepare FFT phase angle */
			wang[i]=MAT_CPANGLE(i, np);
			/* prepare hamming window factors */
			hamming[i]=MAT_FHAMMING(i, np);
		}

		factors_ready=true;
	}

	/* keep waiting untill back ground image updated */
	while( !bkimg_updated && control_cmd != cmd_exit_audioSpectrum_thread )
		tm_delayms(25);


  printf("%s: Start loop FFT ...\n",__func__);
  /*  ------------------------  LOOP FFT  ---------------------  */
  while(1) {
     if(FFTdata_ready) {
#if 1   /* Apply Hamming window  */
	for(i=0; i<FFT_POINTS; i++)
		 fft_nx[i]=mat_FixIntMult(hamming[i], fft_nx[i]);

#endif
	//printf("call egiFFF...\n");
        /*-------------------------  Call mat_egiFFFT() ---------------------------
	 *  1. Run FFT with INT type input nx[].
	 *  2. Input nx[] data has been trimmed in ff_load_FFTdata()
	 *  3. ( point number, angle factor, float input, int input, fft output )
	 -------------------------------------------------------------------------*/
        mat_egiFFFT(np, wang, NULL, fft_nx, ffx);

        /* update sdy */
#if 0  /***   1. Symmetric spectrum diagram (TBD)  */
        for(i=0; i<ns; i++) {
                sdy[i]=dybase-( mat_uintCompAmp( ffx[i*ng])>>(nexp-1 -5) ); //(nexp-1) );
                /* trim sdy[] */
                if(sdy[i]<0)
                       sdy[i]=0;
        }

#else  /***  2. Normal spectrum diagram
	* fs=8k,    1024 elements, resolution abt. 8Hz,  Spectrum spacing step*8Hz
	* fs=16k,
	* fs=44.1k, 1024 elements, resolution abt. 44Hz, step=32, 44*32=1.4k
	* fs=48k,   1024 elements, resolution abt. 48Hz, step=32, 48*32=1.5k
	*/
 	for(i=0; i<ns; i++) {
        	/* map sound wave amplitude to sdy[] */
		if(i<8) {  /* +1 to depress low frequency amplitude, since most of audio wave energy is
			      at low frequency band. */
		   #if 1  /* 16 point average */
			sdy[i]=0;
			for(j=0; j<16; j++) {
				sdy[i] += dybase-( mat_uintCompAmp(ffx[nk[i]+j])>>(nexp-1 +0) );
			}
			sdy[i]=sdy[i]>>4;
		   #else  /* normal sample point */
	                sdy[i]=dybase-( mat_uintCompAmp(ffx[nk[i]])>>(nexp-1 +2) ); //(nexp-1) );
		   #endif

		}
		else	{  /* -1 to boost high frequency amplitude */
		   #if 1  /* 16 point average */
			sdy[i]=0;
			for(j=0; j<16; j++) {
				sdy[i] += dybase-( mat_uintCompAmp(ffx[nk[i]+j])>>(nexp-1 -1) );
			}
			sdy[i]=sdy[i]>>4;
		   #else  /* normal sample point */
	                sdy[i]=dybase-( mat_uintCompAmp(ffx[nk[i]])>>(nexp-1 -1) ); //(nexp-1) );
		   #endif
		}

		/* trim sdy[] */
                if(sdy[i]<dylimit)
                        sdy[i]=dylimit;
        }
#endif

	/* linear decrease sdy[0]-sdy[7] */
	for(i=0; i<8; i++)
		sdy[i]=dybase-( ((i+9)*(dybase-sdy[i]))>>4 ); /*  (i+9)/16,  ratio: 9/16 -> 16/16 */

	/* Apply 4 points average filter for sdy[], to smooth spectrum diagram, so it looks better! */
	for(i=0; i<ns-(4-1); i++) {
		for(j=0; j<(4-1); j++)
			sdy[i] += sdy[i+j];
		sdy[i]>>=2;
	}

	/* draw spectrum */
        fb_filo_flush(&fbdev); /* flush and restore old FB pixel data */
        fb_filo_on(&fbdev);    /* start collecting old FB pixel data */

        for(i=0; i<ns-1; i++) {
		draw_wline_nc(&fbdev, sdx[i], dybase, sdx[i], sdy[i], 3); /* TODO fix 0 width wline--NOPE! */
        }
        fb_filo_off(&fbdev); /* turn off filo */

	FFTdata_ready=false; /* reset indicator */

     } /* end if(FFTdata_ready)  */

     /* check cmd to quit thread */
     if(control_cmd == cmd_exit_audioSpectrum_thread ) {
             EGI_PLOG(LOGLV_INFO,"%s: exit commmand is received!",__func__);
             break;
     }

     tm_delayms(75);

   } /* end while() */

   /* free mem and resource */
   //fb_filo_dump(&fbdev); /* to dump */
   fb_filo_flush(&fbdev); /*  flush and restore old FB pixel data */
   release_fbdev(&fbdev);
}

