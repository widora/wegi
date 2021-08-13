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
midaszhou@yahoo.com
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
#include "egi_color.h"
//#include "egi_image.h" /* definition conflict */

#ifdef __cplusplus
 extern "C" {
#endif

/* See FBDEV.devname, or use gv_fb_dev.devname="/dev/fb0" as default. */
//#ifdef LETS_NOTE
//#define EGI_FBDEV_NAME "/dev/fb0" //1
//#else
//#define EGI_FBDEV_NAME "/dev/fb0"
//#endif

#define FBDEV_BUFFER_PAGES 3	/* Max FB buffer pages */

/*** NOTE:
 *   1. FBDEV is considered to be statically allocated!
 *   2. (.fbfd <=0) is deemed as an uninitialized FBDEV.
 */
typedef struct fbdev{
	const char*	devname;	/* FB device name, if NULL, use default "/dev/fb0" */
        int 		fbfd; 		/* FB device file descriptor, open "dev/fbx"
					 * If <=0, as an uninitialized FBDEV.
					 */

        bool 		virt;           /* 1. TRUE: virtural fbdev, it maps to an EGI_IMGBUF
	                                 *   and fbfd will be ineffective.
					 *   vinfo.xres,vinfo.yres and vinfo.screensize MUST set.
					 *   as .width and .height of the EGI_IMGBUF.
					 *  2. FALSE: maps to true FB device, fbfd is effective.
					 *  3. FB FILO will be ineffective then.
                                	 */

        struct 		fb_var_screeninfo vinfo;  /* !!! WARNING !!!
						   *  vinfo.line_length/bytes_per_pixel may NOT equal vinfo.xres
						   *  In some case, line_length/bytes_per_pixel > xres, because of LCD size limit,
						   *  not of controller RAM limit!
						   *  But in all functions we assume xres==info.line_length/bytes_per_pixel, so lookt it over
						   *  if problem arises.
						   */
        struct 		fb_fix_screeninfo finfo;

	int		*zbuff;		/* Pixel depth, for totally xres*yres pixels. NOW: integer type, ( TODO: float type)
					 * Rest all to 0 when clear working buffer.
					 * NOT for virt FB.
					 * TODO: CAVEAT: zbuff[] allocated without considering vinfo.line_length/bytes_per_pixel!
					 * TODO: Init as  INT32_MIN (-2147483648) OR INT64_MIN (-__INT64_C(9223372036854775807)-1)
					 */
	bool		zbuff_on;
	bool		flipZ;		/* If ture, pixz will be flipped to be -pixz before compare and buffer to zbuff[] */
	int		pixz;		/* Pixel z value, 0 as bkground layer If DEFAULT!(surfaces) */

        unsigned long 	screensize;	/* in bytes */
					/* TODO: To hook up map_fb and map_buff[] with EGI_IMGBUFs */
        unsigned char 	*map_fb;  	/* Pointer to kernel FB buffer, mmap to FB data */

	/* BUFF number as unsigned int */
	#define	FBDEV_WORKING_BUFF	0	/* map_buff[page FBDEV_WORKING_BUFF] */
	#define FBDEV_BKG_BUFF		1	/* map_buff[page FBDEV_BKG_BUFF] */
	unsigned char 	*map_buff;	/* Pointer to user FB buffers, 1-3 pages, maybe more.
					 * Default:
					 *    1st buff page as working buffer.
					 *    2nd buff page as background buffer.
					 * Only when ENABLE_BACK_BUFFER is defined, then map_buff will be allocated.
					 */
	unsigned char   *map_bk;	/* Pointer to curret working back buffer page, mmap mem
					 * 1. When ENABLE_BACK_BUFFER is defined, map_bk is pointed to map_buff; all write/read
					 *    operation to the FB will be directed to the map_buff through map_bk,
					 *    map_fb pointered data will be updated only when fb_refresh() is called, or
					 *    memcpy back buffer to it explicitly.
					 * 2. If ENABLE_BACK_BUFFER not defined, map_buff will not be allocated and map_bk is set to NULL.
					 *    All write/read operation will affect to FB mem directly throuhg map_fb.
 					 * 3. When fb_set_directFB() is called after init_fbdev(), then map_bk will be re-appointed to map_fb
					 *    or map_buff. However, if ENABLE_BACK_BUFFER is not defined, then it will be invalid/fail to call
					 *    fb_set_directFB(false), for map_buff and map_bk are all NULL.
					 */
	unsigned int	npg;		/* index of back buffer page, Now npg=0 or 1, maybe 2  */


	EGI_IMGBUF	*virt_fb;	/* virtual FB, as an EGI_IMGBUF */
	EGI_IMGBUF	*VFrameImg;	/* To hold a Virtual Frame IMGBUF. as result of a completed frame. render result.
					 * vfb_render(): copy virt_fb to VFrameImg
					 */
	bool		vimg_owner;	/* Ownership of virt_fb(imgbuf) and VFrameImg.
					 * True:  FBDEV has the ownership, usually imgbuf is created/allocated during init_virt_fbdev().
				 	 *	  and virt_fb will be released when release_virt_fbdev().
					 * False: Usually the imgbuf is created/allocated by the caller, and FBDEV will NOT
					 * 	  try to free it when release_virt_fbdev().
					 */

	bool		antialias_on;	/* Carry out anti-aliasing functions */

	bool		pixcolor_on;	/* default/init as off, If ture: draw_dot() use pixcolor, else: draw_dot() use fb_color.
					 * Usually to set pixcolor_on immediately after init_(virt_)fbdev.
					 */
	uint16_t 	pixcolor;	/* pixel color */
	unsigned char	pixalpha;	/* pixel alpha value in use, 0: 100% bkcolor, 255: 100% frontcolor */
	bool		pixalpha_hold;  /* Normally, pixalpha will be reset to 255 after each draw_dot() operation
					 * True: pixalpha will NOT be reset after draw_dot(), it keeps effective
					 *	 to all draw_dot()/writeFB() operations afterward.
					 * False: As defaulst set.
					 * Note: Any function that need to set pixalpha_hold MUST reset it and
 					 *       reassign pixcolor to 255 before the end.
					 */
	int		lumadelt;	 /* Luminance adjustment value if !=0 */

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
					 *      Rotation coord as per RIGHT HAND RULE of LCD X,Y   ( x-->y grip direction. thumb --> z )
					 */

	int		pos_xres;	/* Resolution for X and Y direction, as per pos_rotate */
					/* CAVEAT: confusion with fb_dev->finfo.line_length! */
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

/* Default sys FBDEV, global variale, Frame buffer device
 * Note: Most EGI advanced elements only support gv_fb_dev as default FBDEV,
 *       however, you can create a new FBDEV by init_fbdev(), and write data to it by calling
 *       supportive functions, which are usually basic element functions.
 */
extern FBDEV   gv_fb_dev;

/* functions */
int     init_fbdev(FBDEV *dev);
void    release_fbdev(FBDEV *dev);

//int 	fb_set_screenVinfo(FBDEV *fb_dev, struct fb_var_screeninfo *old_vinfo, const struct fb_var_screeninfo *new_vinfo);
//int 	fb_set_screenPos(FBDEV *fb_dev, unsigned int xres, unsigned int yres);

int 	init_virt_fbdev(FBDEV *fb_dev, EGI_IMGBUF *fbimg, EGI_IMGBUF *FrameImg);
int 	init_virt_fbdev2(FBDEV *fb_dev, int xres, int yres, int alpha, EGI_16BIT_COLOR color);
void	release_virt_fbdev(FBDEV *dev);
int	reinit_virt_fbdev(FBDEV *dev, EGI_IMGBUF *fbimg, EGI_IMGBUF *FrameImg);
void 	fb_shift_buffPage(FBDEV *fb_dev, unsigned int numpg);
void 	fb_set_directFB(FBDEV *fb_dev, bool directFB);
int 	fb_get_FBmode(FBDEV *fb_dev);
void 	fb_init_FBbuffers(FBDEV *fb_dev);
void 	fb_copy_FBbuffer(FBDEV *fb_dev,unsigned int from_numpg, unsigned int to_numpg);
void 	fb_clear_backBuff(FBDEV *dev, uint32_t color);
void 	fb_clear_workBuff(FBDEV *fb_dev, EGI_16BIT_COLOR color);
void 	fb_clear_bkgBuff(FBDEV *fb_dev, EGI_16BIT_COLOR color);
void 	fb_reset_zbuff(FBDEV *fb_dev);
void 	fb_init_zbuff(FBDEV *fb_dev, int z0);
void 	fb_clear_mapBuffer(FBDEV *dev, unsigned int numpg, uint16_t color); /* for 16bit color only */
int 	fb_page_refresh(FBDEV *dev, unsigned int numpg);
void 	fb_lines_refresh(FBDEV *dev, unsigned int numpg, unsigned int startln, int n);
int	fb_render(FBDEV *dev);
int	vfb_render(FBDEV *dev);
int 	fb_page_saveToBuff(FBDEV *dev, unsigned int buffNum);
int 	fb_page_restoreFromBuff(FBDEV *dev, unsigned int buffNum);
int 	fb_page_refresh_flyin(FBDEV *dev, int speed);
int 	fb_slide_refresh(FBDEV *dev, int offl);
void    fb_filo_on(FBDEV *dev);
void    fb_filo_off(FBDEV *dev);
void    fb_filo_flush(FBDEV *dev);
void    fb_filo_dump(FBDEV *dev);
void	fb_position_rotate(FBDEV *dev, unsigned char pos);

#ifdef __cplusplus
 }
#endif

#endif
