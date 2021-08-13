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
#ifndef __EGI_FBGEOM_H__
#define __EGI_FBGEOM_H__

#include <stdio.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdbool.h>
#include "egi_fbdev.h"
#include "egi.h"
#include "egi_filo.h"

#ifdef __cplusplus
 extern "C" {
#endif

/* for draw_dot(), roll back to start if reach boundary of FB mem */
#define no_FB_DOTOUT_ROLLBACK /* also check FB_SYMBOUT_ROLLBACK in symbol.h */

extern EGI_BOX gv_fb_box;


/* functions */
//////////////////////////////////////////////////////////
void    fbprint_fbcolor(void);
void    fbprint_fbpix(FBDEV *dev);
void 	fbset_speed(unsigned int speed);
void 	fbset_color(uint16_t color);
void	fbset_color2(FBDEV *dev, uint16_t color);

void 	fbset_alpha(FBDEV *dev, EGI_8BIT_ALPHA alpha);
void 	fbreset_alpha(FBDEV *dev);

void 	fbclear_bkBuff(FBDEV *dev, uint16_t color);  /* OBOSELETE, to use fb_clear_backBuff() instead. */

void 	clear_screen(FBDEV *dev, uint16_t color);

bool 	pxy_online(int px,int py, int x1, int y1,int x2, int y2);
bool	point_online(const EGI_POINT *pxy, const EGI_POINT *pts);
//bool 	point_inbox(int px,int py,int x1,int y1,int x2,int y2);
bool 	pxy_inbox(int px,int py,int x1,int y1,int x2,int y2);
//bool 	point_inbox2(const EGI_POINT *point, const EGI_BOX* box);
bool 	point_inbox(const EGI_POINT *point, const EGI_BOX* box);
bool 	point_incircle(const EGI_POINT *pxy, const EGI_POINT *pc, int r);
bool 	point_intriangle(const EGI_POINT *pxy, const EGI_POINT *tripts);
bool    box_inbox(EGI_BOX* box, EGI_BOX* container);
bool    box_outbox(EGI_BOX* box, EGI_BOX* container);

EGI_16BIT_COLOR fbget_pixColor(FBDEV *fb_dev, int x, int y);
int	fbget_zbuff(FBDEV *fb_dev, int x, int y);

////////////////  Draw function   ///////////////
   /******  NOTE: for 16bit color only!  ******/
int 	draw_dot(FBDEV *dev,int x,int y);
void 	draw_line_simple(FBDEV *dev,int x1,int y1,int x2,int y2);
void 	draw3D_line_simple(FBDEV *dev, int x1,int y1, int z1, int x2,int y2, int z2);
void 	draw_line_antialias(FBDEV *dev,int x1,int y1,int x2,int y2);
void 	draw_line(FBDEV *dev,int x1,int y1,int x2,int y2);
void 	draw3D_line(FBDEV *dev, int x1,int y1,int z1, int x2,int y2,int z2);
void 	draw_button_frame( FBDEV *dev, unsigned int type, EGI_16BIT_COLOR color,
                        int x0, int y0, unsigned int width, unsigned int height, unsigned int w);
void 	draw_wline_nc(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w);
void 	draw_wline(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w);
void 	float_draw_wline(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w, bool roundEnd);
void 	draw_dash_wline(FBDEV *dev,int x1,int y1, int x2,int y2, unsigned int w, int sl, int vl);
void 	draw_pline_nc(FBDEV *dev, EGI_POINT *points,int pnum, unsigned int w);
void 	draw_pline(FBDEV *dev, EGI_POINT *points,int pnum, unsigned int w);
void 	draw_oval(FBDEV *dev,int x,int y);
void 	draw_rect(FBDEV *dev,int x1,int y1,int x2,int y2);
void 	draw_wrect(FBDEV *dev,int x1,int y1,int x2,int y2, int w);
void 	draw_roundcorner_wrect(FBDEV *dev,int x1,int y1,int x2,int y2, int r, int w);
int 	draw_filled_rect(FBDEV *dev,int x1,int y1,int x2,int y2);
int 	draw_blend_filled_roundcorner_rect(FBDEV *dev,int x1,int y1,int x2,int y2, int r,
							EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha);
void 	draw_filled_box(FBDEV *dev, EGI_BOX *box);
void 	draw_blend_filled_rect( FBDEV *dev, int x1, int y1, int x2, int y2,
                                		EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha );
int     draw_filled_rect2(FBDEV *dev,uint16_t color, int x1,int y1,int x2,int y2);
void 	draw_warc(FBDEV *dev, int x0, int y0, int r, float Sang, float Eang, unsigned int w);
void 	draw_filled_pieSlice(FBDEV *dev, int x0, int y0, int r, float Sang, float Eang );
void 	draw_circle(FBDEV *dev, int x, int y, int r);
void 	draw_pcircle(FBDEV *dev, int x0, int y0, int r, unsigned int w);
void 	draw_triangle(FBDEV *dev, EGI_POINT *points);
void 	draw_filled_triangle(FBDEV *dev, EGI_POINT *points);
void 	draw_blend_filled_triangle(FBDEV *dev, EGI_POINT *points, EGI_16BIT_COLOR color, uint8_t alpha);
void 	draw_filled_annulus(FBDEV *dev, int x0, int y0, int r, unsigned int w);
void 	draw_filled_circle(FBDEV *dev, int x, int y, int r);

//////////////// new draw function, with color /////////////
void 	draw_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color);
void 	draw_filled_annulus2(FBDEV *dev, int x0, int y0, int r, unsigned int w, EGI_16BIT_COLOR color);
void 	draw_filled_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color);

void 	draw_blend_filled_circle(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha);
void 	draw_blend_filled_annulus( FBDEV *dev, int x0, int y0, int r, unsigned int w,
                                                               EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha );
/////////////// Draw curves //////////////
int 	draw_spline(FBDEV* dev, int np, EGI_POINT *pxy, int endtype, unsigned int w);
int 	draw_filled_spline( FBDEV *fbdev, int np, EGI_POINT *pxy, int endtype, unsigned int w,
        	                int baseY, EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha);
int 	draw_spline2(FBDEV* dev, int np, EGI_POINT *pxy, int endtype, unsigned int w);
int 	draw_bezier_curve(FBDEV *fbdev, int np, EGI_POINT *pxy, float *ws, unsigned int w);
int 	draw_Bspline(FBDEV *fbdev, int np, EGI_POINT *pxy, float *ws, int deg, unsigned int w);

int 	fb_cpyto_buf(FBDEV *fb_dev, int x1, int y1, int x2, int y2, uint16_t *buf);
int 	fb_cpyfrom_buf(FBDEV *fb_dev, int x1, int y1, int x2, int y2, const uint16_t *buf);
int 	fb_buffer_FBimg(FBDEV *fb_dev, int nb);
int 	fb_restore_FBimg(FBDEV *fb_dev, int nb, bool clean);

/* square mapping */
void fb_drawimg_SQMap(int n, struct egi_point_coord x0y0, const uint16_t *image,
   	                                           const struct egi_point_coord *SQMat_XRYR);
/* annulus mapping */
void fb_drawimg_ANMap(int n, int ni, struct egi_point_coord x0y0, const uint16_t *image,
                                                       const struct egi_point_coord *ANMat_XRYR);

int fb_scale_pixbuf(unsigned int owid, unsigned int ohgt, unsigned int nwid, unsigned int nhgt,
                       const uint16_t *obuf, uint16_t *nbuf);

int egi_getpoit_interpol2p(EGI_POINT *pn, int off, const EGI_POINT *pa, const EGI_POINT *pb);
int egi_numstep_btw2p(int step, const EGI_POINT *pa, const EGI_POINT *pb);
int egi_randp_inbox(EGI_POINT *pr, const EGI_BOX *box);
int egi_randp_boxsides(EGI_POINT *pr, const EGI_BOX *box);

#ifdef __cplusplus
 }
#endif

#endif
