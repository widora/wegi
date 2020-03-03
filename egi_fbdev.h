/*------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Referring to: http://blog.chinaunix.net/uid-22666248-id-285417.html
 本文的copyright归yuweixian4230@163.com 所有，使用GPL发布，可以自由拷贝，转载。
但转载请保持文档的完整性，注明原作者及原链接，严禁用于任何商业用途。

作者：yuweixian4230@163.com
博客：yuweixian4230.blog.chinaunix.net


Modified and appended by: Midas Zhou
-----------------------------------------------------------------------------*/
#ifndef __EGI_FBDEV_H__
#define __EGI_FBDEV_H__

#include <stdio.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdbool.h>
//#include "egi.h"  /* definition conflict */
#include "egi_filo.h"
#include "egi_imgbuf.h"
//#include "egi_image.h" /* definition conflict */

#ifdef LETS_NOTE
#define EGI_FBDEV_NAME "/dev/fb1"
#else
#define EGI_FBDEV_NAME "/dev/fb0"
#endif

#define FBDEV_BUFFER_PAGES 3	/* Max FB buffer pages */

typedef struct fbdev{
        int 		fbfd; 		/* FB device file descriptor, open "dev/fbx" */

        bool 		virt;           /* 1. TRUE: virtural fbdev, it maps to an EGI_IMGBUF
	                                 *   and fbfd will be ineffective.
					 *   vinfo.xres,vinfo.yres and vinfo.screensize MUST set.
					 *   as .width and .height of the EGI_IMGBUF.
					 *  2. FALSE: maps to true FB device, fbfd is effective.
					 *  3. FB FILO will be ineffective then.
                                	 */

        struct 		fb_var_screeninfo vinfo;
        struct 		fb_fix_screeninfo finfo;

        unsigned long 	screensize;	/* in bytes */
        unsigned char 	*map_fb;  	/* Pointer to kernel FB buffer, mmap to FB data */
	unsigned char 	*map_buff;	/* Pointer to user FB buffers, 1-3 pages, maybe more.
					 * Default:
					 *    1st buff page as working buffer.
					 *    2nd buff page as background buffer.
					 */
	unsigned char   *map_bk;	/* Pointer to curret working back buffer page, mmap mem
					 * Default/init FB: map_bk=map_buff
					 * When ENABLE_BACK_BUFFER is defined, all write/read operation to
					 * the FB will be directed to the map_buff through map_bk,
					 * The map_fb will be updated only when fb_refresh() is called, or
					 * memcpy back buffer to it explicitly.
					 */
	unsigned int	npg;		/* index of back buffer page, Now npg=0 or 1, maybe 2  */


	EGI_IMGBUF	*virt_fb;	/* virtual FB data as an EGI_IMGBUF
					 * Ownership of the imgbuf will NOT be taken from the caller, that
					 * means FB will never try to free it, whatever.
					 */

	bool		pixcolor_on;	/* default/init as off */
	uint16_t 	pixcolor;	/* pixel color */
	unsigned char	pixalpha;	/* pixel alpha value in use, 0: 100% bkcolor, 255: 100% frontcolor */
	bool		pixalpha_hold;  /* Normally, pixalpha will be reset to 255 after each draw_dot() operation
					 * True: pixalpha will NOT be reset after draw_dot(), it keeps effective
					 *	 to all draw_dot()/writeFB() operations afterward.
					 * False: As defaulst set.
					 * Note: Any function that need to set pixalpha_hold MUST reset it and
 					 *       reassign pixcolor to 255 before the end.
					 */

	 /*  Screen Position Rotation:  Not applicable for virtual FBDEV!
	  *  Call fb_position_rotate() to change following items.
	  */
	int   		pos_rotate;
					/* 0: default X,Y coordinate of FB
					 * 1: clockwise rotation 90 deg: Y  maps to (vinfo.xres-1)-FB.X,
					 *				 X  maps to FB.Y
				         * 2: clockwise rotation 180 deg
					 * 3: clockwise rotation 270 deg
					 * Others: as default 0
					 * Note: Default FB set, as pos_rotate=0, is faster than other sets.
					 *	for it only needs value assignment, while other sets need
					 *	coordinate transforming/mapping calculations.
					 */

	int		pos_xres;	/* Resultion for X and Y direction, as per pos_rotate */
	int		pos_yres;

	/* pthread_mutex_t fbmap_lock; */

	EGI_FILO 	*fb_filo;
	int 		filo_on;	/* >0, activate FILO push */

//	uint16_t 	*buffer[FBDEV_BUFFER_PAGES];  /* FB image data buffer */

}FBDEV;

/* distinguished from PIXEL in egi_bjp.h */
typedef struct fbpixel {
	long int position;
	#ifdef LETS_NOTE
	uint32_t argb;
	#else
	uint16_t color;
	#endif
}FBPIX;

/* Default FBDEV, global variale, Frame buffer device */
extern FBDEV   gv_fb_dev;

/* functions */
int     init_fbdev(FBDEV *dev);
void    release_fbdev(FBDEV *dev);
int 	init_virt_fbdev(FBDEV *fr_dev, EGI_IMGBUF *eimg);
void	release_virt_fbdev(FBDEV *dev);
void 	fb_shift_buffPage(FBDEV *fb_dev, unsigned int numpg);
void 	fb_set_directFB(FBDEV *fb_dev, bool NoBuff);
void 	fb_init_FBbuffers(FBDEV *fb_dev);
void 	fb_clear_backBuff(FBDEV *dev, uint32_t color);
void 	fb_page_refresh(FBDEV *dev, unsigned int numpg);
void 	fb_lines_refresh(FBDEV *dev, unsigned int numpg, unsigned int sind, unsigned int n);
//void	fb_render()
int 	fb_page_saveToBuff(FBDEV *dev, unsigned int buffNum);
int 	fb_page_restoreFromBuff(FBDEV *dev, unsigned int buffNum);
int 	fb_page_refresh_flyin(FBDEV *dev, int speed);
int 	fb_slide_refresh(FBDEV *dev, int offl);
void    fb_filo_on(FBDEV *dev);
void    fb_filo_off(FBDEV *dev);
void    fb_filo_flush(FBDEV *dev);
void    fb_filo_dump(FBDEV *dev);
void	fb_position_rotate(FBDEV *dev, unsigned char pos);

#endif
