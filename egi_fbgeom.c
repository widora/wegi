/*---------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Referring to: http://blog.chinaunix.net/uid-22666248-id-285417.html
本文的copyright归yuweixian4230@163.com 所有，使用GPL发布，可以自由拷贝，转载。
但转载请保持文档的完整性，注明原作者及原链接，严禁用于任何商业用途。
作者：yuweixian4230@163.com
博客：yuweixian4230.blog.chinaunix.net

Note:
1. Not thread safe.
2. TODO: For all lines drawing functions: if distance between two input points
   is too big, it may take too long time! The Caller should avoid such case.
3. TODO: For all curve drawing functions: Current step/segment is too big, the
	 resutl of draw_line()/draw_wline() segment is NOT so smooth.
	 Call draw_dot() and apply anti_aliasing to improve it.

Jurnal
2021-1-10:
	1. Modify fbget_pixColor(): to pick color from working buffer.
2021-1-21/25:
	1. draw_dot(): Use var. pix_color instead of fb_color to store blended color value.
		       fb_color is FB sys color value!
	2. draw_line(): extend pixel/alpha as for anti_aliasing.
2021-2-9:
	1. Add draw_triangle().
2021-2-25:
	1. draw_wrect(), draw_wrect_nc(), draw_wrect()
	   Draw one additional 1_width line inside the rect...
2021-2-28:
	1. Apply fb_dev->zbuff in draw_dot().
2021-3-3:
	1. Add fbget_zbuff().
2021-3-12:
	1. Apply fb_dev->pixcolor for virtual FBDEV.
2021-3-30:
	1. draw_line(antialias): Comment 'dev->pixcolor_on=true' at start and
	   'dev->pixcolor_on=false' at end, for there's NO color in the func params.
	   Just let caller to set/reset it.
	2. draw_dot(): if(virt_fb){}: Use pix_color instead of fb_color to store blended color value.
				      fb_color is FB sys color value!

2021-04-1:
	1.draw_filled_spline(): save FBDEV.pixcolor_on, and restore it later.
2021-04-2:
	1. draw_blend_filled_roundcorner_rect() draw_blend_filled_rect()
	   draw_blend_filled_annulus() draw_blend_filled_circle():
	   Save FBDEV.pixcolor_on, and restore it later.
	2. draw_filled_spline(): fbset_color() replaced by fbset_color2().
	   draw_circle2() draw_filled_annulus2() draw_filled_circle2():
	   fbset_color() replaced by fbset_color2() to use pixcolor.
2021-05-19:
	1. Add draw_blend_filled_triangle().
2021-05-22/23:
	1. Add point_intriangle().
	2. Add pxy_online(), point_online().
2021-07-26:
	1. Add draw_line_antialias().
	2. draw_line(): Wrap with draw_line_simple() and draw_line_antialias().
2021-08-08:
	1. draw_filled_triangle(): Use draw_line_simple().
2021-08-12:
	1. Add draw3D_line(), draw3D_simple_line(), dist_points(), dist3D_points()
	2. draw_dot(): apply fb_dev->flipZ.

Modified and appended by Midas-Zhou
midaszhou@yahoo.com
----------------------------------------------------------------------------*/
#include "egi_fbgeom.h"
#include "egi.h"
#include "egi_debug.h"
#include "egi_math.h"
#include "egi_color.h"
#include <unistd.h>
#include <string.h> /*memset*/
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <math.h>
#include <stdlib.h>

/* global variale */
EGI_BOX gv_fb_box;

/* Default color set.
 * Note:
 * 1. fb_color and fb_rgb are volatile! They may be changed by draw_dot() and other functions!
 *
 */
static uint16_t fb_color=(30<<11)|(10<<5)|10;  //r(5)g(6)b(5)   /* For 16bpp,  */
#ifdef LETS_NOTE
static uint32_t fb_rgb=0x0000ff;				/* For 24/32 bpp */
#endif

/* default usleep time after each draw_point() */
static unsigned int fb_drawdot_usleep;

/* Ancillary functions */
static inline int dist_points(int x1, int y1, int x2, int y2)
{
	return roundf(sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)));
}
static inline int dist3D_points(int x1, int y1, int z1, int x2, int y2, int z2)
{
	return roundf(sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1)));
}

/* Set drawing speed: 1 - 10, 0==10  */
void fbset_speed(unsigned int speed)
{
	/* Set Limit */
	if(speed==0) speed=10;
	if(speed>10) speed=10;

	fb_drawdot_usleep=2*(10-speed)*(10-speed);
}

/* For DEBUG : print out sys.fb_color and FBDEV.pixcolor and FBDEV.pixalpha */
void fbprint_fbcolor(void)
{
   printf(" -- fb_color=%#04X -- \n", fb_color);

}
void fbprint_fbpix(FBDEV *dev)
{
  printf("pixcolor_on:%s, pixcolor=%#04X;  pixalpha_hold:%s, pixalpha=%d\n",
				dev->pixcolor_on?"true":"false", dev->pixcolor,
				dev->pixalpha_hold?"true":"false", dev->pixalpha );
}


/*  set color for every dot */
inline void fbset_color(EGI_16BIT_COLOR color)
{
	fb_color=color;
}

/*  set color for every dot */
inline void fbset_color2(FBDEV *dev, EGI_16BIT_COLOR color)
{
	if(dev->pixcolor_on)	/* Set private pixcolor */
		dev->pixcolor=color;
	else			/* Set public pixcolor */
		fb_color=color;
}

/* Set and reset alpha value for pixels, which will be applied in draw_dot()  */
inline void fbset_alpha(FBDEV *dev, EGI_8BIT_ALPHA alpha)
{
	if(dev==NULL)return;

        /* turn on FBDEV pixalpha_hold and set pixalpha */
        dev->pixalpha_hold=true;
        dev->pixalpha=alpha;
}
inline void fbreset_alpha(FBDEV *dev)
{
	if(dev==NULL)return;

        /* turn off FBDEV pixalpha_hold and reset pixalpha to 255 */
        dev->pixalpha_hold=false;
        dev->pixalpha=255;
}

/*--------------------------------------------
 check if (px,py) in box(x1,y1,x2,y2)

 return:
	 True:  within the box or on the edge
	 False

 Midas Zhou
---------------------------------------------*/
inline bool pxy_inbox(int px,int py, int x1, int y1,int x2, int y2)
{
       int xl,xh,yl,yh;

	if(x1>=x2){
		xh=x1;xl=x2;
	}
	else {
		xl=x1;xh=x2;
	}

	if(y1>=y2){
		yh=y1;yl=y2;
	}
	else {
		yh=y2;yl=y1;
	}

	if( (px>=xl && px<=xh) && (py>=yl && py<=yh))
		return true;
	else
		return false;
}


/*--------------------------------------------
 check if (px,py) in on line(x1,y1,x2,y2).

 Return:
	 True
	 False

 Midas Zhou
---------------------------------------------*/
inline bool pxy_online(int px,int py, int x1, int y1,int x2, int y2)
{
	MAT_VECTOR2D vt[3];
	float s;
	float len;
        int xl,xh,yl,yh;

	len= sqrt( (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) );

	/* 1. If degenerated into a point */
	if( len<1 ) {
		//egi_dpstd("Too short a line, it's a point!\n");
		len= sqrt( (px-x1)*(px-x1) + (py-y1)*(py-y1) );
		return (len < 2.0);
	}

	/* 2. Check vector cross product, and distance=s/len */
	vt[0]=(MAT_VECTOR2D){x1-px, y1-py}; /* x1-px */
	vt[1]=(MAT_VECTOR2D){x2-px, y2-py}; /* x2-px */
	s= VECTOR2D_CROSSPD(vt[0], vt[1]);
	if( abs(s)/len > 1.0 )
		return false;

	/* 3. NOW: On the same line, but NOT necessary on the segment. */

	/* 4. Check if on the segement */
	if(x1>=x2){
		xh=x1;xl=x2;
	}
	else {
		xl=x1;xh=x2;
	}

	if(y1>=y2){
		yh=y1;yl=y2;
	}
	else {
		yh=y2;yl=y1;
	}

	return ( px>=xl && px<=xh) && (py>=yl && py<=yh); /* NOT '||', consider Vertical OR Horizontal line! */
}


/*--------------------------------------------
 check if (px,py) in on segment defined by pts.

 Return:
	 True
	 False

 Midas Zhou
---------------------------------------------*/
inline bool point_online(const EGI_POINT *pxy, const EGI_POINT *pts)
{
	if( pxy==NULL || pts==NULL)
		return false;

       return pxy_online( pxy->x, pxy->y,
			  pts->x, pts->y, (pts+1)->x, (pts+1)->y );
}

/*-----------------------------------------------
 check if an EGI_POINT is in an EGI_BOX
 return:
	 True:  within the box or on the edge
	 False  or Fails!

 Midas Zhou
------------------------------------------------*/
inline bool point_inbox(const EGI_POINT *pxy, const EGI_BOX* box)
{
	if(pxy==NULL || box==NULL)
		return false;

	return pxy_inbox( pxy->x, pxy->y,
			    box->startxy.x, box->startxy.y,
			    box->endxy.x, box->endxy.y
			   );
}


/*------------------------------------------------------------------------
 check if an EGI_POINT is within a circle. If the point is just on the edge
 of the circle, it deems as NOT within the circle.

 @pxy:	checking point.
 @pc:   Center point of the circle.
 @r:    Radius of the circle.

 return:
	 True:  the point is totally within the circle
	 False

 Midas Zhou
-------------------------------------------------------------------------*/
inline bool point_incircle(const EGI_POINT *pxy, const EGI_POINT *pc, int r)
{
	if(pxy==NULL || pc==NULL)
		return false;

	if( r*r > (pxy->x - pc->x)*(pxy->x - pc->x)+(pxy->y - pc->y)*(pxy->y - pc->y) )
		return true;

	else
		return false;
}

/*------------------------------------------------------------------------
 Check if an EGI_POINT is within a triangle.

  		(Barycentric Coordinate Method)
 Reference: https://blackpawn.com/texts/pointinpoly/default.html

 @pxy:		checking point.
 @tripts:   	Three points to define a triangle.

 return:
	 True:  the point is within the triangle.
	 False

 Midas Zhou
-------------------------------------------------------------------------*/
inline bool point_intriangle(const EGI_POINT *pxy, const EGI_POINT *tripts)
{
	MAT_VECTOR2D vt[4];
//	float s;
	float dp00, dp01, dp02, dp11, dp12;
	float denom;
	float u,v;
//	float len;

	if(pxy==NULL || tripts==NULL)
		return false;

	vt[0] = VECTOR2D_SUB(tripts[2], tripts[0]);
	vt[1] = VECTOR2D_SUB(tripts[1], tripts[0]);
	vt[2] = VECTOR2D_SUB(pxy[0], tripts[0]);	/* <---- pxy -> ptn[0] ------- */
	vt[3]=  VECTOR2D_SUB(tripts[2], tripts[1]);

	dp00=VECTOR2D_DOTPD(vt[0], vt[0]);
	dp01=VECTOR2D_DOTPD(vt[0], vt[1]);
	dp02=VECTOR2D_DOTPD(vt[0], vt[2]);
	dp11=VECTOR2D_DOTPD(vt[1], vt[1]);
	dp12=VECTOR2D_DOTPD(vt[1], vt[2]);

	denom=dp00*dp11 - dp01*dp01;

	/* If degenrated into a line. */
	if( abs(denom) < 1 ) {
		//egi_dpstd("denom<1, It's a line! \n");
		/* Case: vt[3]~=0! */
		if(VECTOR2D_MOD( vt[3])<1 )
			return point_online(pxy, tripts);    /* Line: tripts[0], tripts[1] */
		/* Case: vt[0]~=0! || vt[1]~=0! */
		else
			return point_online(pxy, tripts+1);    /* Line: tripts[1], tripts[2] */
	}

	u=(dp11*dp02 - dp01*dp12)/denom;
	v=(dp00*dp12 - dp01*dp02)/denom;

	return	(u>=0) && (v>=0) && ( u+v <= 1 );
}


/*---------------------------------------------------------------
Check whether the box is totally WITHIN the container.
If there is an overlap of any sides, it is deemed as NOT within!

@inbox:		checking box
@container:	containing box

Return:
	True:	the box is totally WITHIN the container.
	False:  NOT totally WITHIN the container.
		or fails!

Midas Zhou
----------------------------------------------------------------*/
bool  box_inbox(EGI_BOX* inbox, EGI_BOX* container)
{
	int xcu,xcd; /*u-max, d-min*/
	int xiu,xid;
	int ycu,ycd;
	int yiu,yid;

	if( inbox==NULL || container==NULL )
		return false;

	/* 1. get Max. and Min X coord of the container */
     	if( container->startxy.x > container->endxy.x ) {
		xcu=container->startxy.x;
		xcd=container->endxy.x;
	}
	else {
		xcu=container->endxy.x;
		xcd=container->startxy.x;
	}

	/* 2. get Max. and Min X coord of the inbox */
     	if( inbox->startxy.x > inbox->endxy.x ) {
		xiu=inbox->startxy.x;
		xid=inbox->endxy.x;
	}
	else {
		xiu=inbox->endxy.x;
		xid=inbox->startxy.x;
	}

	/* 3. compare X coord */
	if( !(xcd<xid) || !(xcu>xiu) )
		return false;

	/* 4. get Max. and Min Y coord of the container */
     	if( container->startxy.y > container->endxy.y ) {
		ycu=container->startxy.y;
		ycd=container->endxy.y;
	}
	else {
		ycu=container->endxy.y;
		ycd=container->startxy.y;
	}
	/* 5. get Max. and Min Y coord of the inbox */
     	if( inbox->startxy.y > inbox->endxy.y ) {
		yiu=inbox->startxy.y;
		yid=inbox->endxy.y;
	}
	else {
		yiu=inbox->endxy.y;
		yid=inbox->startxy.y;
	}
	/* 6. compare Y coord */
	if( !(ycd<yid) || !(ycu>yiu) )
		return false;


	return true;
}


/*--------------------------------------------------------------------
Check whether the box is totally out of the container.
If there is an overlap of any sides, it is deemed as NOT totally out!

@inbox:		checking box
@container:	containing box

Return:
	True:	the box is totally out of the container.
	False:  NOT totally out of the container.
		or fails!

Midas Zhou
--------------------------------------------------------------------*/
bool  box_outbox(EGI_BOX* inbox, EGI_BOX* container)
{
	int xcu,xcd; /*u-max, d-min*/
	int xiu,xid;
	int ycu,ycd;
	int yiu,yid;

	if( inbox==NULL || container==NULL )
		return false;

	/* 1. get Max. and Min X coord of the container */
     	if( container->startxy.x > container->endxy.x ) {
		xcu=container->startxy.x;
		xcd=container->endxy.x;
	}
	else {
		xcu=container->endxy.x;
		xcd=container->startxy.x;
	}

	/* 2. get Max. and Min X coord of the inbox */
     	if( inbox->startxy.x > inbox->endxy.x ) {
		xiu=inbox->startxy.x;
		xid=inbox->endxy.x;
	}
	else {
		xiu=inbox->endxy.x;
		xid=inbox->startxy.x;
	}

	/* 3. compare X coord */
	if( !( xcd>xiu || xcu<xid ) )
		return false;


	/* 4. get Max. and Min Y coord of the container */
     	if( container->startxy.y > container->endxy.y ) {
		ycu=container->startxy.y;
		ycd=container->endxy.y;
	}
	else {
		ycu=container->endxy.y;
		ycd=container->startxy.y;
	}
	/* 5. get Max. and Min Y coord of the inbox */
     	if( inbox->startxy.y > inbox->endxy.y ) {
		yiu=inbox->startxy.y;
		yid=inbox->endxy.y;
	}
	else {
		yiu=inbox->endxy.y;
		yid=inbox->startxy.y;
	}
	/* 6. compare Y coord */
	if( !( ycd>yiu || ycu<yid ) )
		return false;


	return true;
}


/*---------------------------------------------
    clear screen with given color
      BUG:
	!!!!call egi_colorxxxx_randmon() error
 Midas
---------------------------------------------*/
void clear_screen(FBDEV *fb_dev, uint16_t color)
{
//	int i,j;
//	FBDEV *fb_dev=dev;
//	int xres=fb_dev->vinfo.xres;
//	int yres=fb_dev->vinfo.yres;
	int bytes_per_pixel=fb_dev->vinfo.bits_per_pixel/8;
	long int location=0;

	for(location=0; location < (fb_dev->screensize/bytes_per_pixel); location++)
	        *((uint16_t*)(fb_dev->map_fb+location*bytes_per_pixel))=color;

}


/*----------- Oboselete!!! Replaced by fb_clear_backBuff() -----------
 clear fb_dev->map_bk with given color
 Midas
--------------------------------------------------------------------*/
void fbclear_bkBuff(FBDEV *fb_dev, uint16_t color)
{
	int bytes_per_pixel=fb_dev->vinfo.bits_per_pixel/8;
	long int location=0;

	for(location=0; location < (fb_dev->screensize/bytes_per_pixel); location++)
	        *((uint16_t*)(fb_dev->map_bk+location*bytes_per_pixel))=color;
}


/*-------------------------------------------------------------------
Return indicated pixel color value in EGI_16BIT_COLOR.
		--- FOR 16BIT COLOR FB ---

@fb_dev:	FB under consideration
@x,y:		Pixel coordinate value. under FB.pos_rotate coord!

Return:
   Ok    16bit color value for the pixel with given coordinates(x,y).
   Fail  0 as for black

Midas Zhou
-------------------------------------------------------------------*/
EGI_16BIT_COLOR fbget_pixColor(FBDEV *fb_dev, int x, int y)
{
	if(fb_dev==NULL || fb_dev->map_fb==NULL)
		return 0;

	int  fx=0, fy=0;		/* FB mem space coordinate */
	int  xres=fb_dev->vinfo.xres;
        int  yres=fb_dev->vinfo.yres;

	/* check FB.pos_rotate: Map x,y to fx,fy under default FB coord.
	 * Note: Here xres/yres is default/HW_set FB x/y resolustion!
         */
	switch(fb_dev->pos_rotate) {
		case 0:			/* FB default position */
			fx=x;
			fy=y;
			break;
		case 1:			/* Clockwise 90 deg */
			fx=(xres-1)-y;
			fy=x;
			break;
		case 2:			/* Clockwise 180 deg */
			fx=(xres-1)-x;
			fy=(yres-1)-y;
			break;
		case 3:			/* Clockwise 270 deg */
			fx=y;
			fy=(yres-1)-x;
			break;
	}

	/* Check mapped FX FY in default FB */
	if( fx < 0 || fx > xres-1 ) return 0;
	if( fy < 0 || fy > yres-1 ) return 0;

	/* Pick pix color from working buffer */
	if(fb_dev->map_bk)
		return *(EGI_16BIT_COLOR *)(fb_dev->map_bk+(fy*xres+fx)*sizeof(EGI_16BIT_COLOR));
	/* Pick from FB mmap */
	else
		return *(EGI_16BIT_COLOR *)(fb_dev->map_fb+(fy*xres+fx)*sizeof(EGI_16BIT_COLOR));
}


/*-------------------------------------------------------------------
Return zbuff value for the given point (x,y) in FBDEV.

@fb_dev:	FBDEV.
@x,y:		Pixel coordinate value. under FB.pos_rotate coord!

Return:
	0	Fails or out of range. 	( 0 as for bkground layer.)
	>=0	Ok.

Midas Zhou
-------------------------------------------------------------------*/
int  fbget_zbuff(FBDEV *fb_dev, int x, int y)
{
	if(fb_dev==NULL || fb_dev->map_fb==NULL)
		return 0;

	int  fx=0, fy=0;		/* FB mem space coordinate */
	int  xres=fb_dev->vinfo.xres;
        int  yres=fb_dev->vinfo.yres;
        long int pixoff;

	/* check FB.pos_rotate: Map x,y to fx,fy under default FB coord.
	 * Note: Here xres/yres is default/HW_set FB x/y resolustion!
         */
	switch(fb_dev->pos_rotate) {
		case 0:			/* FB default position */
			fx=x;
			fy=y;
			break;
		case 1:			/* Clockwise 90 deg */
			fx=(xres-1)-y;
			fy=x;
			break;
		case 2:			/* Clockwise 180 deg */
			fx=(xres-1)-x;
			fy=(yres-1)-y;
			break;
		case 3:			/* Clockwise 270 deg */
			fx=y;
			fy=(yres-1)-x;
			break;
	}

	/* Check mapped FX FY in default FB */
	if( fx < 0 || fx > xres-1 ) return 0;
	if( fy < 0 || fy > yres-1 ) return 0;

	/* Get zbuff */
	pixoff=fy*xres+fx;

	return fb_dev->zbuff[pixoff];
}


/*------------------------------------------------------------------
Assign color value to a pixel in framebuffer.
Note:
1. The pixel color value will be system color fb_color, or as its
   private color of FBDEV.pixcolor.
2. The caller shall do boudary check first, to ensure that coordinates
   (x,y) is within screen size range. or just rule it out.

TEST:
   1. test luma adjustment FOR_16BITS_COLOR_FBDEV only.

Midas Zhou

Return:
	0	OK
	<0	Fails
	-1	get out of FB mem.(ifndef FB_DOTOUT_ROLLBACK)
-------------------------------------------------------------------*/
int draw_dot(FBDEV *fb_dev,int x,int y)
{
	EGI_IMGBUF *virt_fb;
	unsigned char *map;
	int fx=0;
	int fy=0;
      	long int location=0;
	long int pixoff;
	#ifdef LETS_NOTE
	unsigned char* pARGB=NULL; /* For LETS_NOTE */
	#endif
	int xres;
	int yres;
	FBPIX fpix;
	int sumalpha;
	EGI_16BIT_COLOR pix_color;

	/* check input data */
	if(fb_dev==NULL)
		return -2;

	/* <<<<<<  FB BUFFER SELECT  >>>>>> */
	#if defined(ENABLE_BACK_BUFFER) || defined(LETS_NOTE)
	map=fb_dev->map_bk; /* write to back buffer */
	#else
	map=fb_dev->map_fb; /* write directly to FB map */;
	#endif

	/* set xres and yres */
	virt_fb=fb_dev->virt_fb;
	if(virt_fb) {			/* for virtual FB */
		xres=virt_fb->width;
		yres=virt_fb->height;
	}
	else {				/* for FB */
		xres=fb_dev->vinfo.xres;
		yres=fb_dev->vinfo.yres;
	}

	/* Check FB.pos_xres, pos_yres */
	//if(fb_dev->pos_rotate) {
		if( x<0 || x>fb_dev->pos_xres-1)
			return -1;
		if( y<0 || y>fb_dev->pos_yres-1)
			return -1;
	//}

	/* Check FB.pos_rotate
	 * IF 90 Deg rotated: Y maps to (xres-1)-FB.X,  X maps to FB.Y
	 * Note: Here xres/yres is default/HW_set FB x/y resolustion!
         */
	switch(fb_dev->pos_rotate) {
		case 0:			/* FB default position */
			fx=x;
			fy=y;
			break;
		case 1:			/* Clockwise 90 deg */
			fx=(xres-1)-y;
			fy=x;
			break;
		case 2:			/* Clockwise 180 deg */
			fx=(xres-1)-x;
			fy=(yres-1)-y;
			break;
		case 3:			/* Clockwise 270 deg */
			fx=y;
			fy=(yres-1)-x;
			break;
	}

#ifdef FB_DOTOUT_ROLLBACK
	/* map to LCD(X,Y) or IMGBUF if virt_fb */
	if(fx>xres-1)
		fx=fx%xres;
	else if(fx<0)
	{
		fx=xres-(-fx)%xres;
		fx=fx%xres; /* here fx=1-240 */
	}
	if( fy > yres-1) {
		fy=fy%yres;
	}
	else if(fy<0) {
		fy=yres-(-fy)%yres;
		fy=fy%yres; /* here fy=1-320 */
	}

#else /* NO ROLLBACK */

	/* ignore out_ranged points */
	if( fx>(xres-1) || fx<0) {
		return -1;
	}
	if( fy>(yres-1) || fy<0 ) {
		return -1;
	}
#endif

   /* Check Z if zbuff_on */
   if( fb_dev->zbuff_on ) {
	pixoff=fy*xres+fx;

	/* flipZ==ture */
	if( fb_dev->flipZ ) {
		//fb_dev->pixz = -fb_dev->pixz;
		if( fb_dev->zbuff[pixoff] > -fb_dev->pixz )
			return 0;
		else
		    fb_dev->zbuff[pixoff]= -fb_dev->pixz;
	}
	/* flipZ==false */
	else {
		if( fb_dev->zbuff[pixoff] > fb_dev->pixz )
			return 0;
		else
		    fb_dev->zbuff[pixoff]=fb_dev->pixz;
	}
   }

   /* ---------------- ( for virtual FB :: for 16bit color only NOW!!! ) -------------- */
   if(virt_fb) {

	/* (in pixles): data location of the point pixel */
	location=fx+fy*virt_fb->width;

	/* check if no space left for a 16bit pixel */
	if( location < 0 || location > fb_dev->screensize - 1 ) /* screensize of imgbuf,
							       * in pixels! */
	{
		printf("WARNING: point location out of EGI_IMGBUF mem.!\n");
		return -1;
	}

	/* blend pixel color
	 * TODO:  Also apply background alpha value if virt_fb->alpha available ????
	 *	  fb_color=egi_16bitColor_blend2( fb_color, fb_dev->pixalpha,
         *                     virt_fb->imgbuf[location], virt_fb->alpha[location]);
	 */
	if(fb_dev->pixalpha==255) {	/* if 100% front color */
		if(fb_dev->pixcolor_on)
		        virt_fb->imgbuf[location]=fb_dev->pixcolor;
		else
		        virt_fb->imgbuf[location]=fb_color;
	}
	/* otherwise, blend with original color, back alpha value ignored!!! */
	else if( fb_dev->pixalpha!=0 ) {
		if(fb_dev->pixcolor_on) {
			/* NOTE: back color alpha value all deemed as 255,  */
			// XXX fb_color=COLOR_16BITS_BLEND(  fb_dev->pixcolor,		     /* Front color */
			pix_color=COLOR_16BITS_BLEND(  fb_dev->pixcolor,		     /* Front color */
						      virt_fb->imgbuf[location],     /* Back color */
						      fb_dev->pixalpha );	     /* Alpha value */
		        virt_fb->imgbuf[location]=pix_color;
		}
		else {
			/* NOTE: back color alpha value all deemed as 255,  */
			pix_color=COLOR_16BITS_BLEND(  fb_color,			     /* Front color */
						      virt_fb->imgbuf[location],     /* Back color */
						      fb_dev->pixalpha );	     /* Alpha value */
		        virt_fb->imgbuf[location]=pix_color;
		}
	}

        /* if VIRT FB has alpha data. TODO: Apply egi_16bitColor_blend2() */
	if(virt_fb->alpha) {
		/* sum up alpha value */
	        sumalpha=virt_fb->alpha[location]+fb_dev->pixalpha;
        	if( sumalpha > 255 ) sumalpha=255;
	        virt_fb->alpha[location]=sumalpha;
	}

	/* reset alpha to 255 as default, at last!!! */
	if(fb_dev->pixalpha_hold==false)
		fb_dev->pixalpha=255;

   }

   /* ------------------------ ( for real FB ) ---------------------- */
   else {

    #ifdef LETS_NOTE /* --------- FOR 32BITS COLOR (ARGB) FBDEV ------------ */

	/*(in bytes:) data location of the point pixel */
        location=(fx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel>>3)+
                     (fy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

	/* NOT necessary ???  check if no space left for a 16bit_pixel in FB mem */
	if( location<0 || location > (fb_dev->screensize-sizeof(uint32_t)) ) /* screensize in bytes! */
	{
		//printf("WARNING: point location out of fb mem.!\n");
		return -1;
	}

	/* push old data to FB FILO */
	if(fb_dev->filo_on && fb_dev->pixalpha>0 )
        {
                fpix.position=location; /* pixel to bytes, !!! FAINT !!! */
		pARGB=map+location;
		fpix.argb=*(uint32_t *)pARGB;
		/* FB alpha value no more use ? */
                egi_filo_push(fb_dev->fb_filo, &fpix);
        }

	/* assign or blend FB pixel data */
	if(fb_dev->pixalpha==255) {	/* if 100% front color */
		if(fb_dev->pixcolor_on) /* use fbdev pixcolor */
			*((uint32_t *)(map+location))=COLOR_16TO24BITS(fb_dev->pixcolor)+(255<<24);
		else			/* use system pixcolor */
			*((uint32_t *)(map+location))=COLOR_16TO24BITS(fb_color)+(255<<24);
	}
	else {	/* otherwise, blend with original color */
		if(fb_dev->pixcolor_on) { 	/* use fbdev pixcolor */

		     /* !!! LETS_NOTE FB only support 1 and 0 for alpha value of ARGB
	             *((uint32_t *)(map+location))=COLOR_16TO24BITS(fb_dev->pixcolor)   \
										+ (fb_dev->pixalpha<<24);
		      */

			fb_rgb=(*((uint32_t *)(map+location)))&0xFFFFFF; /* Get back color */
			/* front, back, alpha */
			fb_rgb=COLOR_24BITS_BLEND(COLOR_16TO24BITS(fb_dev->pixcolor), fb_rgb, fb_dev->pixalpha);
			*((uint32_t *)(map+location))=fb_rgb+(255<<24);

		}
		else {				/* use system pxicolor */

		     /* !!! LETS_NOTE: FB only support 1 and 0 for alpha value of ARGB
		     *((uint32_t *)(map+location))=COLOR_16TO24BITS(fb_color)	\
										+ (fb_dev->pixalpha<<24);
		      */

			fb_rgb=(*((uint32_t *)(map+location)))&0xFFFFFF; /* Get back color */
			/* front, back, alpha */
			fb_rgb=COLOR_24BITS_BLEND(COLOR_16TO24BITS(fb_color), fb_rgb, fb_dev->pixalpha);
			*((uint32_t *)(map+location))=fb_rgb+(255<<24);
		}
	}

    #else /* --------- FOR 16BITS COLOR FBDEV ------------ */

	/*(in bytes:) data location of the point pixel */
        location=(fx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
	                     (fy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

	/* NOT necessary ???  check if no space left for a 16bit_pixel in FB mem */
	if( location<0 || location > (fb_dev->screensize-sizeof(uint16_t)) ) /* screensize in bytes! */
	{
		printf("WARNING: point location out of fb mem.!\n");
		return -1;
	}

	/* push old data to FB FILO */
	if(fb_dev->filo_on && fb_dev->pixalpha>0 )
        {
                fpix.position=location; /* pixel to bytes, !!! FAINT !!! */
                fpix.color=*(uint16_t *)(map+location);
                egi_filo_push(fb_dev->fb_filo, &fpix);
        }

	/* assign or blend FB pixel data */
	if(fb_dev->pixalpha==255) {	/* if 100% front color */
		if(fb_dev->pixcolor_on) { /* use fbdev pixcolor */
			if( fb_dev->lumadelt!=0 ) {  /* --- TEST Luma --- */
		        	*((uint16_t *)(map+location))=egi_colorLuma_adjust(fb_dev->pixcolor,fb_dev->lumadelt);
			} else
		        	*((uint16_t *)(map+location))=fb_dev->pixcolor;
		}
		else {			/* use system pixcolor */
			if( fb_dev->lumadelt!=0 ) {  /* --- TEST Luma --- */
		        	*((uint16_t *)(map+location))=egi_colorLuma_adjust(fb_color,fb_dev->lumadelt);
			} else
			        *((uint16_t *)(map+location))=fb_color;
		}
	}
	else if (fb_dev->pixalpha !=0 ) /* otherwise, blend with original color */
	{
		if(fb_dev->pixcolor_on) { 	/* use fbdev pixcolor */
			if( fb_dev->lumadelt!=0 ) {  /* --- TEST Luma --- */
				//fb_color=COLOR_16BITS_BLEND(  egi_colorLuma_adjust(fb_dev->pixcolor,fb_dev->lumadelt),    /* Front color */
				pix_color=COLOR_16BITS_BLEND(  egi_colorLuma_adjust(fb_dev->pixcolor,fb_dev->lumadelt),    /* Front color */
							     *(uint16_t *)(map+location), /* Back color */
							      fb_dev->pixalpha );	  /* Alpha value */
			}
			else {
				//fb_color=COLOR_16BITS_BLEND(  fb_dev->pixcolor,		  /* Front color */
				pix_color=COLOR_16BITS_BLEND(  fb_dev->pixcolor,		  /* Front color */
							     *(uint16_t *)(map+location), /* Back color */
							      fb_dev->pixalpha );	  /* Alpha value */
			}

	        	*((uint16_t *)(map+location))=pix_color; //fb_color; //fb_dev->pixcolor;
		}
		else {				/* use system pxicolor */
			if( fb_dev->lumadelt!=0 ) {  /* --- TEST Luma --- */
				//fb_color=COLOR_16BITS_BLEND(  egi_colorLuma_adjust(fb_color,fb_dev->lumadelt),    /* Front color */
				pix_color=COLOR_16BITS_BLEND(  egi_colorLuma_adjust(fb_color,fb_dev->lumadelt),    /* Front color */
							     *(uint16_t *)(map+location), /* Back color */
							      fb_dev->pixalpha );	  /* Alpha value */
			}
			else {
				//fb_color=COLOR_16BITS_BLEND(  fb_color,			  /* Front color */
				pix_color=COLOR_16BITS_BLEND(  fb_color,			  /* Front color */
							     *(uint16_t *)(map+location), /* Back color */
							      fb_dev->pixalpha );	  /* Alpha value */
			}

		        *((uint16_t *)(map+location))=pix_color; //fb_color;
		}
	}

    #endif /* --------- END 16/32BITS BPP FBDEV SELECT ------------ */

	/* reset alpha to 255 as default */
	if(fb_dev->pixalpha_hold==false)
		fb_dev->pixalpha=255;

   }

   /* Sleep to adjust drawing speed */
   if(fb_drawdot_usleep)
   	usleep(fb_drawdot_usleep);

    return 0;
}


/*-----------------------------------------------------------
Draw a line with simple method (without anti-aliasing effect)
Note:
  1. fround() to improve accuracy, NOT not speed.

Midas Zhou
-----------------------------------------------------------*/
void draw_line_simple(FBDEV *dev,int x1,int y1,int x2,int y2)
{
        int i=0;
        int j=0;
	int k;
        int tekxx=x2-x1;
        int tekyy=y2-y1;
	int tmp;

	/* 1. An oblique line with X2>X1 */
        if(x2>x1) {
	    tmp=y1;
	    /* Traverse x1 (+)-> x2 */
            for(i=x1;i<=x2;i++) {
		/* j as pY */
                j=(i-x1)*tekyy/tekxx+y1;		/* TODO: fround() to improve accuracy. */
		if(y2>=y1) {
			/* Traverse tmp (+)-> pY */
			for(k=tmp;k<=j;k++)		/* fill uncontinous points */
		                draw_dot(dev,i,k);
		}
		else { /* y2<y1, Traverse tmp (-)-> pY*/
			for(k=tmp;k>=j;k--)
				draw_dot(dev,i,k);
		}
		tmp=j; /* Renew tmp as j(pY) */
            }
        }
	/* 2. A vertical straight line X1==X2 */
	else if(x2 == x1) {
	   if(y2>=y1) {
		for(i=y1;i<=y2;i++)
		   draw_dot(dev,x1,i);
	    }
	    else {
		for(i=y2;i<=y1;i++)
			draw_dot(dev,x1,i);
	   }
	}
	/* 3. An oblique line with X1>X2 */
        else /* x1>x2 */
        {
	    tmp=y2;
	    /* Traverse x2 (+)-> x1 */
            for(i=x2;i<=x1;i++) {
		/* j as pY */
                j=(i-x2)*tekyy/tekxx+y2;
		if(y1>=y2) {
			/* Traverse tmp (+)-> pY */
			for(k=tmp;k<=j;k++)		/* fill uncontinous points */
		        	draw_dot(dev,i,k);
		}
		else {  /* y2>y1, Traverse tmp (-)-> pY */
			for(k=tmp;k>=j;k--)		/* fill uncontinous points */
		        	draw_dot(dev,i,k);
		}
		tmp=j; /* Renew tmp as j(pY) */
            }
        }
}

/*-----------------------------------------------------------
Draw a line with simple method (without anti-aliasing effect)
FBDEV.pixz is applied.

@fev:		Pointer to FBDEV
@flipZ: 	If true: fbdev->pixz will flippted to be -pixz.
@x1,y1,z1,x2,y2,z2:  Coords of two points under FB SYS.

Midas Zhou
-----------------------------------------------------------*/
void draw3D_line_simple(FBDEV *dev, int x1,int y1, int z1, int x2,int y2, int z2)
{
        int i=0;
        int j=0;
	int k;
        int tekxx=x2-x1;
        int tekyy=y2-y1;
	int len=dist_points(x1,y1,x2,y2); /* 2D length! */
	int tmp;

	/* A point, Take MAX(z1,z2) as pixz value. */
	if(len==0) {
		dev->pixz=(z1>z2?z1:z2);
		draw_dot(dev,x1,y1);
		return;
	}

	/* 1. An oblique line with X2>X1 */
        if(x2>x1) {
	    tmp=y1;
	    /* Traverse x1 (+)-> x2 */
            for(i=x1;i<=x2;i++) {
		/* j as pY */
                j=(i-x1)*tekyy/tekxx+y1;		/* TODO: fround() to improve accuracy. */
		if(y2>=y1) {
			/* Traverse tmp (+)-> pY */
			for(k=tmp;k<=j;k++) {		/* fill uncontinous points */
				dev->pixz=z1+dist_points(x1,y1, i,k)*(z2-z1)/len;
		                draw_dot(dev,i,k);
			}
		}
		else { /* y2<y1, Traverse tmp (-)-> pY*/
			for(k=tmp;k>=j;k--) {
				dev->pixz=z1+dist_points(x1,y1, i,k)*(z2-z1)/len;
				draw_dot(dev,i,k);
			}
		}
		tmp=j; /* Renew tmp as j(pY) */
            }
        }
	/* 2. A vertical straight line X1==X2 */
	else if(x2 == x1) {
	   if(y2>=y1) {
		for(i=y1;i<=y2;i++) {
		   dev->pixz=z1+dist_points(x1,y1, x1,i)*(z2-z1)/len;
		   draw_dot(dev,x1,i);
		}
	    }
	    else {
		for(i=y2;i<=y1;i++) {
		   	dev->pixz=z1+dist_points(x1,y1, x1,i)*(z2-z1)/len;
			draw_dot(dev,x1,i);
		}
	   }
	}
	/* 3. An oblique line with X1>X2 */
        else /* x1>x2 */
        {
	    tmp=y2;
	    /* Traverse x2 (+)-> x1 */
            for(i=x2;i<=x1;i++) {
		/* j as pY */
                j=(i-x2)*tekyy/tekxx+y2;
		if(y1>=y2) {
			/* Traverse tmp (+)-> pY */
			for(k=tmp;k<=j;k++)  {	/* fill uncontinous points */
			   	dev->pixz=z2+dist_points(x2,y2, i,k)*(z1-z2)/len;
		        	draw_dot(dev,i,k);
			}
		}
		else {  /* y2>y1, Traverse tmp (-)-> pY */
			for(k=tmp;k>=j;k--)  {  /* fill uncontinous points */
			   	dev->pixz=z2+dist_points(x2,y2, i,k)*(z1-z2)/len;
		        	draw_dot(dev,i,k);
			}
		}
		tmp=j; /* Renew tmp as j(pY) */
            }
        }
}


/* ---------------- draw_line() ------------- */

#if  0 ///////////////////  Without anti-aliasing effect  /////////////////////
/*---------------------------------------------------
		Draw a simple line

Midas Zhou
---------------------------------------------------*/
void draw_line(FBDEV *dev,int x1,int y1,int x2,int y2)
{
	draw_line_simple(dev, x1, y1, x2, y2);
}

#else ///////////////////  With anti-aliasing effect  /////////////////////

/*----------------------------------------------------
Draw a simple line, with anti_aliasing effect.

Note:
1. If a spline is drawn by many short lines, and each line
is so short, that slope/aslope MAY be unfortuantely be
deminished. ???!!
2. Consider GAMMA correction for color blend, in general?
3. Only lines with slope/aslop >2 (0-30Deg, 60-90Deg) will be
   treated with anti_aliasing measure.

------------------------------------------------------*/
void draw_line(FBDEV *dev,int x1,int y1,int x2,int y2)
{
  #if 1 ///////// Call draw_line_antialias() //////////////////

	if(dev->antialias_on)
		draw_line_antialias(dev, x1, y1, x2, y2);
	else
		draw_line_simple(dev, x1, y1, x2, y2);

  #else //////////////////////////////////////////////////////
        int i=0;
        int j=0;
	int k;
        int tekxx;
        int tekyy;
	float fslope;
	int slope;  /* abs(tekyy/tekxx) */
	int aslope;  /* abs(tekxx/tekyy) */
	bool slope_on;
	int tmp,tmpx,tmpy;

	/* Extend pixel alphas */
	EGI_8BIT_ALPHA alphas[4]={65,45,32,25}; //{70, 40, 30, 20}; //{120,80,60};
	//EGI_8BIT_ALPHA alphas[4]={200,150,100,50}; //{70, 40, 30, 20}; //{120,80,60};

	/* Turn on pixalpha_hold */
	if( dev->antialias_on ) {
		dev->pixalpha_hold=true;
	}

	/* Make always x2>x1 OR x2==x1 */
	if(x1>x2) {
		/* Swap (x1,y1)  (x2,y2) */
		tmpx=x2;   tmpy=y2;
		x2=x1;     y2=y1;
		x1=tmpx;   y1=tmpy;
	}

	/* NOW: Case 1: X2 > X1 OR  Case 2: X2==X1 */

	/* Cal. slope/aslop */
        tekxx=x2-x1;
        tekyy=y2-y1;

	fslope=(float)tekyy/tekxx;

	if(tekxx!=0)
		slope=abs(roundf(1.0*tekyy/tekxx));
	else  /* A straight line */
		slope=1000000;
	if(tekyy!=0)
		aslope=abs(roundf(1.0*tekxx/tekyy));
	else  /* A straight line */
		aslope=1000000;

	/* Slope effective, more extend pixel.. */
	if( slope >20 || aslope >20 )
		slope_on=true;

	/* Case 1: x2>x1 */
        if(x2>x1) {
	    tmp=y1;
            for(i=x1;i<=x2;i++) {
                //j=(i-x1)*tekyy/tekxx+y1;
		j=roundf(fslope*(i-x1)+y1);
		if(y2>=y1) {
			for(k=tmp;k<=j;k++) {		/* fill uncontinous points */
			    /* A. If anti_aliasing ON */
			    if(dev->antialias_on) {
				/* A.1 Draw overlapped/step adjacent dots at start */
				if( k==tmp && j!=tmp ) {
					/* NOTE:
					 *  1. j!=tmp ensure a Y step begins, not straigh part! for checking slope<1 condition.
					 *  2. For slope>1 line, it's alway has Y steps, so j!=tmp always OK!
					 */
					/* End pixel */
					dev->pixalpha=alphas[0];
					draw_dot(dev,i,k);

					/* Extend pixel */
					dev->pixalpha=alphas[1];
					if(slope>1) {  /* Nearby_parallel to Y axis */
						draw_dot(dev,i,k-1);	/* Extend -Y */
						if(slope_on) {		/* Entend -Y again! */
							dev->pixalpha=alphas[2];
							draw_dot(dev,i,k-2);
							dev->pixalpha=alphas[3];
							draw_dot(dev,i,k-3);
						}
					}
					else if(aslope>1)  { /* Nearby_parallel to X axis */
						//draw_dot(dev, i-1,k);   /* Extend -X */
						draw_dot(dev, i+1,k);    /* Extend +X */
						if(slope_on) {		/* Extend X again! */
							dev->pixalpha=alphas[2];
							//draw_dot(dev,i-2, k); /* Extend -X */
							draw_dot(dev,i+2,k);    /* Extend +X */
							dev->pixalpha=alphas[3];
							draw_dot(dev,i+3,k);    /* Extend +X */
						}
					}
				}
				/* A.2 Draw overlapped/step adjacent dots at end */
				else if( k==j && j!=tmp ) {
					/* End pixel */
					dev->pixalpha=alphas[0];
					draw_dot(dev,i,k);

					/* Extend pixel */
					dev->pixalpha=alphas[1];
					if(slope>1) {  /* Nearby_parallel to Y axis */
						draw_dot(dev,i,k+1);	/* Extend +Y */
						if(slope_on) {		/* Entend +Y again! */
							dev->pixalpha=alphas[2];
							draw_dot(dev,i,k+2);
							dev->pixalpha=alphas[3];
							draw_dot(dev,i,k+3);
						}
					}
					else if(aslope>1) { /* Nearby_parallel to X axis */
						//draw_dot(dev, i+1,k);   /* Extend +X */
						draw_dot(dev, i-1,k);   /* Extend -X */
						if(slope_on) {		/* Extend X again! */
							dev->pixalpha=alphas[2];
							//draw_dot(dev,i+2, k); /* Extend +X */
							draw_dot(dev,i-2, k);	/* Extend -X */
							dev->pixalpha=alphas[3];
							draw_dot(dev,i-3, k);	/* Extend -X */
						}
					}
				}
				/* A.3 Draw other dots, where no overlapped/step. */
				else {
					dev->pixalpha=255;
		                	draw_dot(dev,i,k);
				}

			     /* B. ELSE: No anti_aliasing */
			     } else
			      		draw_dot(dev, i, k);
			}
		}
		else { /* y2<y1 */
			for(k=tmp;k>=j;k--) {

			    /* C. If anti_aliasing ON */
			    if(dev->antialias_on) {
				/* C.1 Draw overlapped/step adjacent dots at start */
				if(k==tmp && j!=tmp ) {
					/* NOTE:
					 *  1. j!=tmp ensure a Y step begins, not straigh part! for checking slope<1 condition.
					 *  2. For slope>1 line, it's alway has Y steps, so j!=tmp always OK!
					 */
					/* End pixel */
					dev->pixalpha=alphas[0];
					draw_dot(dev,i,k);

					/* Extend pixel */
					dev->pixalpha=alphas[1];
					/* C.1.1 */
					if(slope>1) {  /* Nearby_parallel to Y axis */
						//draw_dot(dev,i,k-1);	/* Extend -Y */
						draw_dot(dev,i,k+1);	/* Extend +Y */
						if(slope_on) {		/* Entend again! */
							dev->pixalpha=alphas[2];
							//draw_dot(dev,i,k-2);   /* Extend -Y */
							draw_dot(dev,i,k+2);     /* Extend +Y */
							dev->pixalpha=alphas[3];
							draw_dot(dev,i,k+3);
						}
					}
					/* C.1.2 */
					else if(aslope>1) { /* Nearby_parallel to X axis */
						//draw_dot(dev, i-1, k);   /* Extend -X */
						/* ??? !!! */
						draw_dot(dev, i+1, k);   /* Extend +X */
						if(slope_on) {		/* Extend again! */
							dev->pixalpha=alphas[2];
							//draw_dot(dev,i-2, k); /* Extend -X */
							draw_dot(dev,i+2, k);   /* Extend +x */
							dev->pixalpha=alphas[3];
							draw_dot(dev,i+2,k);
						}
					}
				}
				/* C.2 Draw overlapped/step adjacent dots at end */
				else if(k==j && j!=tmp ) {
					/* End pixel */
					dev->pixalpha=alphas[0];
					draw_dot(dev,i,k);

					/* Extend pixel */
					dev->pixalpha=alphas[1];
					if(slope>1) {  /* Nearby_parallel to Y axis */
						//draw_dot(dev,i,k+1);	/* Extend +Y */
						draw_dot(dev,i,k-1);	/* Extend -Y */
						if(slope_on) {		/* Entend again! */
							dev->pixalpha=alphas[2];
							//draw_dot(dev,i,k+2);   /* Extend +Y */
							draw_dot(dev,i,k-2); /* Extend -Y */
							dev->pixalpha=alphas[3];
							draw_dot(dev,i,k-3);
						}
					}
					else if(aslope>1) { /* Nearby_parallel to X axis */
						//draw_dot(dev, i+1,k);   /* Extend +X */
						/* ????? */
						draw_dot(dev, i-1,k);   /* Extend -X */

						if(slope_on) {		/* Extend again! */
							dev->pixalpha=alphas[2];
							//draw_dot(dev,i+2, k);  /* Extend +X */
							draw_dot(dev,i-2, k);  /* Extend -X */
							dev->pixalpha=alphas[3];
							draw_dot(dev,i-3, k);
						}
					}

				}
				/* C.3 Draw other dots */
				else {
					dev->pixalpha=255;
		                	draw_dot(dev,i,k);
				}

			     /* D. ELSE: No anti_aliasing */
			     } else {
			      		draw_dot(dev, i, k);
			     }
			}
		}
		tmp=j;
            }
        }
	/* Case 2: X2 == X1 */
	else   {
	   if(y2>=y1) {
		for(i=y1;i<=y2;i++) {
		   	draw_dot(dev,x1,i);
		}
	    }
	    else {
		for(i=y2;i<=y1;i++) {
			draw_dot(dev,x1,i);
		}
	   }
	}

        /* Turn off FBDEV pixalpha_hold */
	if( dev->antialias_on ) {
        	dev->pixalpha_hold=false;
	        dev->pixalpha=255;
	}

  #endif //////////////////////////////////////////////////////
}
#endif

/*----------------------------------------------------
Draw a simple 3D line, apply FBDEV.pixz for zbuff.
NO anti_aliasing effect.
------------------------------------------------------*/
void draw3D_line(FBDEV *dev, int x1,int y1,int z1, int x2,int y2,int z2)
{
	draw3D_line_simple(dev, x1,y1,z1, x2,y2,z2);
}

/*----------------------------------------------------------------
Draw a simple line with anti_aliasing effect.

Note:
1. The caller shall apply dev->pixcolor, by calling fbset_color2(),
   OR the function will set dev->pixcolor_on and use FB sys color
   as dev->pixcolor.

Midas Zhou
---------------------------------------------------------------*/
void draw_line_antialias(FBDEV *dev,int x1,int y1,int x2,int y2)
{
        int i=0;
        int j=0;
	float px,py;
	int step;
	int pxl, pxr, pyu, pyd;
	int k;
//      int tekxx=x2-x1;
//      int tekyy=y2-y1;
	float slope;
	int tmp, tmpx, tmpy;
	bool save_pixcolor_on;
	EGI_16BIT_COLOR save_pixcolor;
//	EGI_16BIT_COLOR backcolor;
	EGI_16BIT_COLOR backcolor_pyu,backcolor_pyd;
	EGI_16BIT_COLOR backcolor_pxl,backcolor_pxr;

	/* Make always x2>x1 OR x2==x1 */
	if(x1>x2) {
		/* Swap (x1,y1)  (x2,y2) */
		tmpx=x2;   tmpy=y2;
		x2=x1;     y2=y1;
		x1=tmpx;   y1=tmpy;
	}

	/* Save pixcolor */
        if(dev->pixcolor_on)    /* As private pixcolor */
                save_pixcolor=dev->pixcolor;
        else                    /* As public pixcolor */
                save_pixcolor=fb_color;
        /* Turn on FBDEV pixcolor_on anyway..... */
	save_pixcolor_on=dev->pixcolor_on;
        dev->pixcolor_on=true;
        dev->pixalpha=255;  /* No dev->pixalpha_on! */

	/* Cal. Step */
	if(y2>y1)
		step=1;
	else if(y2<y1)
		step=-1;
	else
		step=0;

	/* NOW: x2 >= x1 */

	/* 1. An oblique line with X2>X1  (But Y2!=Y1) */
        if(x2>x1 && y2!=y1 ) {
	    slope = 1.0*(y2-y1)/(x2-x1);
	    tmp=y1;
	    /* Traverse x1 (+)-> x2 */
            for(i=x1;i<=x2;i++) {

		/* Calculate j as pY */
       	        //j=(i-x1)*tekyy/tekxx+y1;
		j=roundf(slope*(i-x1)+y1); /* A litte better */

                /* Traverse tmp -> pY by step, as for k++ OR k--. */
                for(k=tmp; (step>0)?(k<=j):(k>=j); k+=step ) {            /* fill uncontinous points */
			py=y1+slope*(i-x1);
			pyd=floorf(py);
			pyu=ceil(py);

			/* Note: if py happens to be an integer, then pyd==pyu!, see following if(pyd==pyu){..}. */

			//if( 255*(pyu-py) == 0)
			//	egi_dpstd("pyu-py=0: xi=%d, x1=%d, y1=%d, pyu=%d, py=%f, slope=%f\n", i,x1,y1,pyu,py,slope);
			//if( 255*(py-pyd) == 0)
			//	egi_dpstd("py-pyd=0: xi=%d, x1=%d, y1=%d, pyd=%d, py=%f, slope=%f\n", i,x1,y1,pyd,py,slope);

			/* PY IS an Integer, (i,py) just on pixel grid! */
			if( pyd==pyu ) {
				dev->pixcolor=save_pixcolor;
				draw_dot(dev,i,pyd);
			}
			/* (i,py) NOT on pixel grid */
			else {
			   /* Get backcolr at first */
			   backcolor_pyd=fbget_pixColor(dev, i, pyd);
			   backcolor_pyu=fbget_pixColor(dev, i, pyu);

			   /* Draw Pxyd */
			   dev->pixcolor=COLOR_16BITS_BLEND(save_pixcolor, backcolor_pyd, (uint16_t)(255*(pyu-py))); /* !!! pyu-py */
			   //dev->pixcolor=egi_16bitColor_blend(save_pixcolor, backcolor_pyd, (uint16_t)(255*(pyu-py))); /* !!! pyu-py */
               		   draw_dot(dev,i,pyd);
			   /* Draw Pxyu */
			   dev->pixcolor=COLOR_16BITS_BLEND(save_pixcolor, backcolor_pyu, (uint16_t)(255*(py-pyd))); /* !!! py-pyd */
			   //dev->pixcolor=egi_16bitColor_blend(save_pixcolor, backcolor_pyu, (uint16_t)(255*(py-pyd))); /* !!! py-pyd */
               		   draw_dot(dev,i,pyu);
			}

			/* Draw pxl/pxr */
			if( slope!=0.0 ) {
				px=x1+1.0*(k-y1)/slope;
				pxl=floorf(px);
				pxr=ceil(px);

				/* PX is an Integer, (px,k) just on pixel grid! */
				if(pxl==pxr) {
				   dev->pixcolor=save_pixcolor;
				   draw_dot(dev,pxl,k);
				}
				/* (px,k) NOT on pixel grid */
				else {
				   /* Get backcolor at first */
				   backcolor_pxl=fbget_pixColor(dev, pxl, k);
				   backcolor_pxr=fbget_pixColor(dev, pxr, k);

				   /* Draw pxyl */
				   dev->pixcolor=COLOR_16BITS_BLEND(save_pixcolor, backcolor_pxl, (uint16_t)(255*(pxr-px))); /* !!! pxr-px */
				   //dev->pixcolor=egi_16bitColor_blend(save_pixcolor, backcolor_pxl, (uint16_t)(255*(pxr-px))); /* !!! pxr-px */
               			   draw_dot(dev,pxl,k);
				   /* Draw Pxyr */
				   dev->pixcolor=COLOR_16BITS_BLEND(save_pixcolor, backcolor_pxr, (uint16_t)(255*(px-pxl))); /* !!! py-pyd */
				   //dev->pixcolor=egi_16bitColor_blend(save_pixcolor, backcolor_pxr, (uint16_t)(255*(px-pxl))); /* !!! py-pyd */
               			   draw_dot(dev,pxr,k);
				}
			}
		}

		/* Renew tmp as j(pY) */
		tmp=j;

            } /* End for(i) */
        }
	/* 2. A horizontal straight line Y1==Y2 */
	else if(y2 == y1) {
	   /* NOW: always x2 >= x1 ! */
	   for(i=x1;i<=x2;i++) {
		   dev->pixcolor=save_pixcolor;
		   draw_dot(dev,i,y1);
	   }
	}
	/* 3. A vertical straight line X1==X2 */
	else if(x2 == x1) {
	   if(y2>=y1) {
		for(i=y1;i<=y2;i++) {
		   dev->pixcolor=save_pixcolor;
		   draw_dot(dev,x1,i);
		}
	    }
	    else {
		for(i=y2;i<=y1;i++) {
			dev->pixcolor=save_pixcolor;
			draw_dot(dev,x1,i);
		}
	   }
	}
	/* XXX 4. An oblique line with X1>X2  --- Ruled out! */

        /* Restore FBDEV pixcolor/pixcolor_on */
	dev->pixcolor=save_pixcolor;
	dev->pixcolor_on=save_pixcolor_on;
	dev->pixalpha=255; /* Always default */
}


/*-----------------------------------------------------
Only draw a frame for rectangluar button, to illustrate
bright/shadowy side lines. Area inside the frame will
keep unchanged.

@dev:   	pointer to FBDEV
@type:  	0  Released button frame
		1  Pressed button frame
@color:		Button color, usually same as the back
		ground color.
@x0,y0:  	Left top point coord.
@width:		Width of the button
@Height:	Height of the button
@w:		Width for side lines.
		If w=0, return.

Midas Zhou
------------------------------------------------------*/
void draw_button_frame( FBDEV *dev, unsigned int type, EGI_16BIT_COLOR color,
			int x0, int y0, unsigned int width, unsigned int height, unsigned int w)
{
	if(dev==NULL || w==0)
		return;

	/* Note: R,G,B contributes differently to the luminance, with G the most and B the least!
	 *       and lum_adjust is NOT a linear brightness adjustment as to human's visual sense.
         */
	int deltY=egi_color_getY(color)>>2; /* 1/4 of original Y  */
	if(deltY<50)deltY=75;

     	EGI_POINT       points[3];

	/* 1. Draw lower lines */
        if(type==1) {	/* For pressed button, bright. */
		fbset_color2(dev, egi_colorLuma_adjust(color, +deltY));
		//fbset_color2(dev, WEGI_COLOR_GRAYC);
	}
	else {		/* For released button, shadowy */
		fbset_color2(dev, egi_colorLuma_adjust(color, -deltY));
		//fbset_color2(dev, WEGI_COLOR_DARKGRAY);
	}
     	draw_filled_rect( dev,  x0, y0+height-w,  x0+width-1, y0+height-1 );
     	draw_filled_rect( dev,  x0+width-w, y0, x0+width-1, y0+height-w-1 );

	/* 2. Draw upper lines */
        if(type==1) {	/* For pressed button, shadowy */
		//fbset_color2(dev, WEGI_COLOR_DARKGRAY);
		fbset_color2(dev, egi_colorLuma_adjust(color, -deltY));
	}
	else {		/* For released button, bright */
		fbset_color2(dev, egi_colorLuma_adjust(color, +deltY));
		//fbset_color2(dev, WEGI_COLOR_GRAYC);
	}

     	draw_filled_rect( dev,  x0, y0,    x0+w-1, y0+height-w-1 );
     	draw_filled_rect( dev,  x0+w, y0,  x0+width-w-1, y0+w-1  );

     	/* covered triangles */
     	points[0]=(EGI_POINT){x0, y0+height-w};
     	points[1]=(EGI_POINT){x0+w-1, y0+height-w};
     	points[2]=(EGI_POINT){x0, y0+height-1};
     	draw_filled_triangle(dev, points);

     	points[0]=(EGI_POINT){x0+width-w, y0};
     	points[1]=(EGI_POINT){x0+width-1, y0};
     	points[2]=(EGI_POINT){x0+width-w, y0+w-1};
     	draw_filled_triangle(dev, points);
}


/*--------------------------------------------------------------------
Draw A Line with width, draw no circle at two points
x1,x1: starting point
x2,y2: ending point
w: width of the line ( W=2*N+1 )

NOTE: if you input w=0, it's same as w=1.

w=3 may be same as w=1, because of inaccuracy of fixed_point cal.

Midas Zhou
----------------------------------------------------------------------*/
void draw_wline_nc(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w)
{
	int i;
	int xr1,yr1,xr2,yr2;

	/* half width, also as circle rad */
//	int r=w>>1; /* so w=0 and w=1 is the same */
	int r=w>1 ? (w-1)/2 : 0;  /* w=0 and w=1 is the same, 1_pixel width line */

	/* x,y, difference */
	int ydif=y2-y1;
	int xdif=x2-x1;

	/* If Horizontal straight line */
	if( ydif == 0 ) {
		for(i=0; i<=r; i++) {
			/* Up Half */
			draw_line(dev, x1, y1+i, x2, y1+i);
			/* Low Half */
			if(i)
			  draw_line(dev, x1, y1-i, x2, y1-i);
		}
		/* Add 1 more line at Low Half if w is even. */
		if( (w&0x1)==0 )
			draw_line(dev, x1, y1-r-1, x2, y1-r-1);
	}
	/* If Vertial straight line */
	else if( xdif==0 ) {
		for(i=0; i<=r; i++) {
			/* Left Half */
			draw_line(dev, x1-i, y1, x1-i, y2);
			/* Right Half */
			if(i)
			  draw_line(dev, x1+i, y1, x1+i, y2);
		}
		/* Add 1 more line at Right Half if w is even. */
		if( (w&0x1)==0 )
			draw_line(dev, x1+r+1, y1, x1+r+1, y2);
	}


        int32_t fp16_len = mat_fp16_sqrtu32(ydif*ydif+xdif*xdif);

	/* if x1,y1 x2,y2 the same point */
	if(fp16_len==0) {
		draw_dot(dev,x1,y1);
		return;
	}

   if(fp16_len !=0 ) {

	/* draw multiple lines  */
	for(i=0;i<=r;i++) {
		/* draw UP_HALF multiple lines  */
		xr1=x1-(i*ydif<<16)/fp16_len;
		yr1=y1+(i*xdif<<16)/fp16_len;
		xr2=x2-(i*ydif<<16)/fp16_len;
		yr2=y2+(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);

		/* draw LOW_HALF multiple lines  */
		xr1=x1+(i*ydif<<16)/fp16_len;
		yr1=y1-(i*xdif<<16)/fp16_len;
		xr2=x2+(i*ydif<<16)/fp16_len;
		yr2=y2-(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);
	}

   } /* end of len !=0, if len=0, the two points are the same position */

}




/*--------------------------------------------------------------------
Draw A Line with width, with circle at two points.
x1,x1: starting point
x2,y2: ending point
w: width of the line ( W=2*N+1 )

NOTE:
1. If you input w=0, it's same as w=1.
2. As fixed point calculation will lose certain accuracy, some w value
   will fail to get desired effect. Example: w=3 has same effect of w=1.
w=3 may be same as w=1, because of inaccuracy of fixed_point cal.

Midas Zhou
----------------------------------------------------------------------*/
void draw_wline(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w)
{
	/* half width, also as circle rad */
//	int r=w>>1; /* so w=0 and w=1 is the same */
	int r=w>1 ? (w-1)/2 : 0;  /* so w=0 and w=1 is the same */

	int i;
	int xr1,yr1,xr2,yr2;

	/* x,y, difference */
	int ydif=y2-y1;
	int xdif=x2-x1;

	/* If Horizontal straight line */
	if( ydif == 0 ) {
		for(i=0; i<=r; i++) {
			/* Up Half */
			draw_line(dev, x1, y1+i, x2, y1+i);
			/* Low Half */
			if(i)
			  draw_line(dev, x1, y1-i, x2, y1-i);
		}
		/* Add 1 more line at Low Half if w is even. */
		if( (w&0x1)==0 )
			draw_line(dev, x1, y1-r-1, x2, y1-r-1);
	}
	/* If Vertial straight line */
	else if( xdif==0 ) {
		for(i=0; i<=r; i++) {
			/* Left Half */
			draw_line(dev, x1-i, y1, x1-i, y2);
			/* Right Half */
			if(i)
			  draw_line(dev, x1+i, y1, x1+i, y2);
		}
		/* Add 1 more line at Right Half if w is even. */
		if( (w&0x1)==0 )
			draw_line(dev, x1+r+1, y1, x1+r+1, y2);
	}

        int32_t fp16_len = mat_fp16_sqrtu32(ydif*ydif+xdif*xdif);

	/* if x1,y1 x2,y2 the same point */
	if(fp16_len==0) {
		draw_filled_circle(dev, x1, y1, r);
		return;
	}

   if(fp16_len !=0 )
   {
	/* draw multiple lines  */
	for(i=0;i<=r;i++)
	{
		/* draw UP_HALF multiple lines  */
		xr1=x1-(i*ydif<<16)/fp16_len;
		yr1=y1+(i*xdif<<16)/fp16_len;
		xr2=x2-(i*ydif<<16)/fp16_len;
		yr2=y2+(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);

		/* draw LOW_HALF multiple lines  */
		xr1=x1+(i*ydif<<16)/fp16_len;
		yr1=y1-(i*xdif<<16)/fp16_len;
		xr2=x2+(i*ydif<<16)/fp16_len;
		yr2=y2-(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);
	}
   } /* end of len !=0, if len=0, the two points are the same position */


	/* draw start/end circles */
	draw_filled_circle(dev, x1, y1, r);
	draw_filled_circle(dev, x2, y2, r);
}

/*--------------------------------------------------------------------
 Float method to draw A Line with width, with circle at two points.

@x1,x1: 	starting point
@x2,y2: 	ending point
@w: 		width of the line ( W=2*N+1 )
@roundEnd:	Add round ends if True.

NOTE:
1. if you input w=0, it's same as w=1.

2. TODO. When draw many short wlines to form a spline.
   With changing of curvatures at certain slope, it will appear
   ugly coarse shape. for little short wlines connected NOT
   smoothly.  too big u step??

Midas Zhou
---------------------------------------------------------------------*/
void float_draw_wline(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w, bool roundEnd)
{
	/* half width, also as circle rad */
	int r=w>>1; /* so w=0 and w=1 is the same */

	//int i;
	float i;
	int xr1,yr1,xr2,yr2;

	/* x,y, difference */
	int ydif=y2-y1;
	int xdif=x2-x1;
	float len=sqrt(ydif*ydif+xdif*xdif);
	float sina=ydif/len;
	float cosa=xdif/len;

	/* if x1,y1 x2,y2 the same point */
	if(x1==x2 && y1==y2) {
		draw_filled_circle(dev, x1, y1, r);
		return;
	}

   if(len !=0 )
   {
	/* draw multiple lines  */
	//for(i=0;i<=r;i++)
	for(i=00.0; i<=r;i+=0.5)
	{
		/* draw UP_HALF multiple lines  */
		xr1=x1-i*sina;
		yr1=y1+i*cosa;
		xr2=x2-i*sina;
		yr2=y2+i*cosa;

		draw_line(dev,round(xr1),round(yr1),round(xr2),round(yr2));

		/* draw LOW_HALF multiple lines  */
		xr1=x1+i*sina;
		yr1=y1-i*cosa;
		xr2=x2+i*sina;
		yr2=y2-i*cosa;

		draw_line(dev,round(xr1),round(yr1),round(xr2),round(yr2));
		//draw_line(dev,xr1,yr1,xr2,yr2);
	}
   } /* end of len !=0, if len=0, the two points are the same position */


	if(roundEnd) {
		/* draw start/end circles */
		draw_filled_circle(dev, x1, y1, r);
		draw_filled_circle(dev, x2, y2, r);
	}
}


/*--------------------------------------------------------------------
Draw a dash line.

NOTE:
1. If you input w=0, it's same as w=1.
2. As fixed point calculation will lose certain accuracy, some w value
   will fail to get desired effect. Example: w=3 has same effect of w=1.
   Call float_draw_wline() instead to improve it.

@dev:		FBDEV
@x1,y1: 	Start point
@x2,y2: 	End point
@sl:		Length of each solid part
@sv:		Length of each void part
@w: 		width of the line ( W=2*N+1 )

Midas Zhou
----------------------------------------------------------------------*/
void draw_dash_wline(FBDEV *dev,int x1,int y1, int x2,int y2, unsigned int w, int sl, int vl)
{
	float len;
	int i,k;
	int xs,ys,xe,ye;
	float s;	/* as ratio to whole length */

	if( sl<1 || vl<1 )
		draw_wline(dev,x1,y1,x2,y2,w);

	/* if x1,y1 x2,y2 the same point */
	if(x1==x2 && y1==y2) {
		draw_dot(dev,x1,y1);
		return;
	}

	/* Get number of total shots */
	len=sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
	k=len/(sl+vl);      /* Total number of full shots */

	/* Draw each shot */
	for(i=0; i<k+1; i++) {
		s=(i*(sl+vl))/len;
		xs=x1+round(s*(x2-x1));
		ys=y1+round(s*(y2-y1));

		s=(i*(sl+vl)+sl)/len;
		if(i==k) {  /* For the last incomplete shot */
			if(s>1.0)s=1.0;
		}
		xe=x1+round(s*(x2-x1));
		ye=y1+round(s*(y2-y1));

		//draw_wline(dev,xs,ys,xe,ye,w);
		float_draw_wline(dev,xs,ys,xe,ye,w,false);
	}
}


/*---------------------------------------------------------------------
Draw A Poly Line, with circle at each point.

points:	     input points for the polyline
num:	     total number of input points.
w: width of the line ( W=2*N+1 )

Midas Zhou
---------------------------------------------------------------------*/
void draw_pline(FBDEV *dev, EGI_POINT *points, int pnum, unsigned int w)
{
	int i=0;

	/* check input data */
	if( points==NULL || pnum<=0 ) {
		printf("%s: Input params error.\n", __func__);
		return ;
	}

	for(i=0; i<pnum-1; i++) {
		draw_wline(dev,points[i].x,points[i].y,points[i+1].x,points[i+1].y,w);
	}

}


/*---------------------------------------------------------------------
Draw A Poly Line, with no circle at each point.

points:	     input points for the polyline
num:	     total number of input points.
w: width of the line ( W=2*N+1 )

Midas Zhou
---------------------------------------------------------------------*/
void draw_pline_nc(FBDEV *dev, EGI_POINT *points, int pnum, unsigned int w)
{
	int i=0;

	/* check input data */
	if( points==NULL || pnum<=0 ) {
		printf("%s: Input params error.\n", __func__);
		return ;
	}

	for(i=0; i<pnum-1; i++) {
		draw_wline_nc(dev,points[i].x,points[i].y,points[i+1].x,points[i+1].y,w);
	}

}



/*------------------------------------------------
	           draw an oval
-------------------------------------------------*/
void draw_oval(FBDEV *dev,int x,int y) //(x.y) 是坐标
{
        FBDEV *fr_dev=dev;
        int *xx=&x;
        int *yy=&y;

	draw_line(fr_dev,*xx,*yy-3,*xx,*yy+3);
	draw_line(fr_dev,*xx-1,*yy-2,*xx-1,*yy+2);
	draw_line(fr_dev,*xx-2,*yy-1,*xx-2,*yy+1);
	draw_line(fr_dev,*xx+1,*yy-2,*xx+1,*yy+2);
	draw_line(fr_dev,*xx+2,*yy-2,*xx+2,*yy+2);
}


/*--------------------------------------------------
Draw an rectangle
Midas Zhou
--------------------------------------------------*/
void draw_rect(FBDEV *dev,int x1,int y1,int x2,int y2)
{
//        FBDEV *fr_dev=dev;

	draw_line(dev,x1,y1,x1,y2);
	draw_line(dev,x1,y2,x2,y2);
	draw_line(dev,x2,y2,x2,y1);
	draw_line(dev,x2,y1,x1,y1);
}


/*--------------------------------------------------
Draw an rectangle with pline
x1,y1,x2,y2:	two points define a rect.
w:		with of ploy line.

Midas Zhou
--------------------------------------------------*/
void draw_wrect(FBDEV *dev,int x1,int y1,int x2,int y2, int w)
{
	/* sort min and max x,y */
	int xl=(x1<x2?x1:x2);
	int xr=(x1>x2?x1:x2);
	int yu=(y1<y2?y1:y2);
	int yd=(y1>y2?y1:y2);

#if 0
	draw_wline_nc(dev,xl-w/2,yu,xr+w/2,yu,w);
	draw_wline_nc(dev,xl-w/2,yd,xr+w/2,yd,w);
	draw_wline_nc(dev,xl,yu-w/2,xl,yd+w/2,w);
	draw_wline_nc(dev,xr,yu-w/2,xr,yd+w/2,w);
#endif

	draw_wline_nc(dev,xl-(w-1)/2,yu,xr+(w-1)/2,yu,w);
	draw_wline_nc(dev,xl-(w-1)/2,yd,xr+(w-1)/2,yd,w);
	draw_wline_nc(dev,xl,yu-(w-1)/2,xl,yd+(w-1)/2,w);
	draw_wline_nc(dev,xr,yu-(w-1)/2,xr,yd+(w-1)/2,w);

	/* Draw one additional 1_width line inside */
//	if((w&0x1)==0)
//		draw_rect(dev, xl+w/2, yu+w/2, xr-w/2, yd-w/2);

}


/*--------------------------------------------------
Draw an rectangle with pline
x1,y1,x2,y2:	two points define a rect.
w:		with of ploy line.

Midas Zhou
--------------------------------------------------*/
void draw_roundcorner_wrect(FBDEV *dev,int x1,int y1,int x2,int y2, int r, int w)
{
	/* sort min and max x,y */
	int xl=(x1<x2?x1:x2);
	int xr=(x1>x2?x1:x2);
	int yu=(y1<y2?y1:y2);
	int yd=(y1>y2?y1:y2);

	/* Limit r */
	if(r<0)r=0;
	if( r > (xr-xl)/2 ) r=(xr-xl)/2;
	if( r > (yd-yu)/2 ) r=(yd-yu)/2;

	/* Draw corners */
	draw_warc(dev, xl+r, yu+r, r, -MATH_PI, -MATH_PI/2, w);     /* fbdev, x0, y0, r, float Sang, float Eang, w */
	draw_warc(dev, xr-r, yu+r, r, -MATH_PI/2, 0, w);
	draw_warc(dev, xl+r, yd-r, r, MATH_PI/2, MATH_PI, w);
	draw_warc(dev, xr-r, yd-r, r, 0, MATH_PI/2, w);

	/* Draw wlines */
	draw_wline_nc(dev, xl+r, yu, xr-r, yu, w);  /* H lines */
	draw_wline_nc(dev, xl+r, yd, xr-r, yd, w);
	draw_wline_nc(dev, xl, yu+r, xl, yd-r, w);  /* V lines */
	draw_wline_nc(dev, xr, yu+r, xr, yd-r, w);
}


/*-------------------------------------------------------------
    Draw a filled rectangle defined by two end points of its
    diagonal line. Both points are also part of the rectangle.

    Return:
		0	OK
		//ignore -1	point out of FB mem
    Midas
------------------------------------------------------------*/
int draw_filled_rect(FBDEV *dev,int x1,int y1,int x2,int y2)
{
	int xr,xl,yu,yd;
	int i,j;

        /* sort point coordinates */
        if(x1>x2) {
                xr=x1;
		xl=x2;
        }
        else {
                xr=x2;
		xl=x1;
	}

        if(y1>y2) {
                yu=y1;
		yd=y2;
        }
        else {
                yu=y2;
		yd=y1;
	}

	for(i=yd;i<=yu;i++)
	{
		for(j=xl;j<=xr;j++)
		{
                	draw_dot(dev,j,i); /* ignore range check */
		}
	}

	return 0;
}

/*-----------------------------------------------------------
    Draw a filled rectangle with round corners.

    Midas_Zhou
------------------------------------------------------------*/
int draw_blend_filled_roundcorner_rect( FBDEV *dev,int x1,int y1,int x2,int y2, int r,
						EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha)
{
	int i;
	int s;
	bool save_pixcolor_on;

	/* sort min and max x,y */
	int xl=(x1<x2?x1:x2);
	int xr=(x1>x2?x1:x2);
	int yu=(y1<y2?y1:y2);
	int yd=(y1>y2?y1:y2);

	/* Limit r */
	if(r<0)r=0;
	if( r > (xr-xl)/2 ) r=(xr-xl)/2;
	if( r > (yd-yu)/2 ) r=(yd-yu)/2;

        /* turn on FBDEV pixcolor and pixalpha_hold */
	save_pixcolor_on=dev->pixcolor_on;
        dev->pixcolor_on=true;
        dev->pixcolor=color;
        dev->pixalpha_hold=true;
        dev->pixalpha=alpha;


	/* Draw max. rect in mid */
	draw_filled_rect(dev, xl, yu+r, xr, yd-r);
	/* Draw upper/lower small rect */
	draw_filled_rect(dev, xl+r, yu, xr-r, yu+r-1);
	draw_filled_rect(dev, xl+r, yd-r+1, xr-r, yd);

	/* Draw 4 quarters of a circle */
	/* !!!Do NOT use draw_filled_pieSlice(), overlapped points! */
	for(i=1; i<r; i++) {
                s=round(sqrt(r*r-i*i));
                s-=1; /* diameter is always odd */

  		/* draw 4th quadrant */
		draw_line(dev, xr-r +1, yd-r+i, xr-r+s, yd-r+i);  /* +1 skip overlap */
		/* draw 3rd quadrant */
		draw_line(dev, xl+r -1, yd-r+i, xl+r-s, yd-r+i);  /* -1 skip overlap */
		/* draw 2nd quadrant */
		draw_line(dev, xl+r -1, yu+r-i, xl+r-s, yu+r-i);
		/* draw 1st quadrant */
		draw_line(dev, xr-r +1, yu+r-i, xr-r+s, yu+r-i);
	}

        /* turn off FBDEV pixcolor and pixalpha_hold */
        dev->pixcolor_on=save_pixcolor_on;
        dev->pixalpha_hold=false;
        dev->pixalpha=255;

	return 0;
}


/*--------------------------------------------
Draw a filled box.

Midas Zhou
--------------------------------------------*/
void draw_filled_box(FBDEV *dev, EGI_BOX *box)
{
	draw_filled_rect(dev, box->startxy.x,box->startxy.y, box->endxy.x, box->endxy.y);
}


/*------------------------------------------------------------------
Draw a filled rectangle blended with backcolor of FB.

Midas Zhou
-------------------------------------------------------------------*/
void draw_blend_filled_rect( FBDEV *dev, int x1, int y1, int x2, int y2,
			     	EGI_16BIT_COLOR color, uint8_t alpha )
{
	bool save_pixcolor_on;

        if(dev==NULL)
                return;

        /* turn on FBDEV pixcolor and pixalpha_hold */
	save_pixcolor_on=dev->pixcolor_on;
        dev->pixcolor_on=true;
        dev->pixcolor=color;
        dev->pixalpha_hold=true;
        dev->pixalpha=alpha;

	draw_filled_rect(dev,x1,y1,x2,y2);

        /* turn off FBDEV pixcolor and pixalpha_hold */
	dev->pixcolor_on=save_pixcolor_on;
	dev->pixalpha_hold=false;
	dev->pixalpha=255;
}


/*--------------------------------------------------------------------------
Same as draw_filled_rect(), but with color.
Midas Zhou
---------------------------------------------------------------------------*/
int draw_filled_rect2(FBDEV *dev, uint16_t color, int x1,int y1,int x2,int y2)
{
	int xr,xl,yu,yd;
	int i,j;

        /* sort point coordinates */
        if(x1>x2) {
                xr=x1;
		xl=x2;
        }
        else {
                xr=x2;
		xl=x1;
	}

        if(y1>y2) {
                yu=y1;
		yd=y2;
        }
        else {
                yu=y2;
		yd=y1;
	}

	for(i=yd;i<=yu;i++)
	{
		for(j=xl;j<=xr;j++)
		{
			//fb_color=color;
			fbset_color2(dev, color);
			/* TODO: dev->pixcolor_on */
                	draw_dot(dev,j,i); /* ignore range check */
		}
	}

	return 0;
}


/*--------------------------------------------------------------------------
Draw a short of arc with corresponding angle from Sang to Eang.

Angle direction: 	--- Right_hand Rule ---
Z as thumb, X->Y as positive rotation direction.

@x0,y0:		circle center
@r:		radius, in mid of width.
@Sang:		start angle, in radian.
@Eang:		end angle, in radian.
@w:		width of arc ( to be ajusted in form of 2*m+1 )

Midas Zhou
-----------------------------------------------------------------------------*/
void draw_warc(FBDEV *dev, int x0, int y0, int r, float Sang, float Eang, unsigned int w)
#if 0	/* ONLY for w=1:   with big w value, it looks ugly! */
{
	int n,i;
	double step_angle;
	double x[2]=,y[2];

	/* start point */
	x[0]=x0+r*cos(Sang);
	y[0]=y0+r*sin(Sang); /* Notice LCD -Y direction */

	/* get step angle, 1 pixel arc  */
//	step_angle=2.0*asin(0.5/r);
	step_angle=1.0*asin(0.5/r);
	n=(Eang-Sang)/step_angle;
	if(n<0) {
		n=-n;
		step_angle=-step_angle;
	}

	/* draw arcs */
	//for(i=1; i<n-1; i++) {
	for(i=1; i<=n; i++) {
		x[1]=x0+r*cos(Sang+i*step_angle);
		y[1]=y0+r*sin(Sang+i*step_angle);   /* Notice LCD -Y direction */
		draw_wline_nc(dev, round(x[0]), round(y[0]), round(x[1]), round(y[1]), w);
		x[0]=x[1];
		y[0]=y[1];
	}

	/* draw the last short */
	x[1]=x0+r*cos(Eang);  /* end point */
	y[1]=y0+r*sin(Eang);
	draw_wline_nc(dev, round(x[0]), round(y[0]), round(x[1]), round(y[1]), w);
}
#else   /* OK, for w>=1 */
{
	int i,n,m;
	int rad;
	double step_angle;
	double x[2]={0},y[2]={0};
	double dcos,dsin;
	double Stcos,Stsin,Edcos,Edsin;  /* for start/end angle */

	if( w<1 ) w=1;
	if( r<1 ) r=1;

	/* make m in form of 2*m+1, so 2*m and 2*m+1 have same effect!! */
	m=w/2;

	/* start/end angle sin/cos value */
	Stcos=cos(Sang);
	Stsin=sin(Sang); /* Notice LCD -Y direction */
	Edcos=cos(Eang);
	Edsin=sin(Eang);

	/* get step angle, 1 pixel arc */
	//step_angle=2.0*asin(0.5/(r+m));
	step_angle=asin(0.25/(r+m));
	n=(Eang-Sang)/step_angle;
	if(n<0) {
		n=-n;
		step_angle=-step_angle;
	}

	/* draw arcs */
	for(i=1; i<=n; i++) {
		dcos=cos(Sang+i*step_angle);
		dsin=sin(Sang+i*step_angle);
		/* draw 2*m+1 arcs, as width */
	        for( rad=r-m; rad<=r+m; rad++ ) {
			/* generate starting pioints */
			if(i==1) {
				x[0]=x0+rad*Stcos;
				y[0]=y0+rad*Stsin; /* Notice LCD -Y direction */
			}
			/* draw arc segments */
			x[1]=x0+rad*dcos;
			y[1]=y0+rad*dsin;
			//draw_wline_nc(dev, round(x[0]), round(y[0]), round(x[1]), round(y[1]), 1);
			float_draw_wline(dev, round(x[0]), round(y[0]), round(x[1]), round(y[1]), 1, false);
			x[0]=x[1];
			y[0]=y[1];
			/* For remaining of n=(Eang-Sang)/step_angle: the last shorts, end points */
			if(i==n) {
				x[1]=x0+rad*Edcos;
				y[1]=y0+rad*Edsin;
				//draw_wline_nc(dev, round(x[0]), round(y[0]), round(x[1]), round(y[1]), 1);
				float_draw_wline(dev, round(x[0]), round(y[0]), round(x[1]), round(y[1]), 1, false);
			}
		}
	}

}
#endif


/*--------------------------------------------------------------------------
Draw a slice of a pie chart.

NOTE: Because of calculation precision and roundoff, most of points are drawn
      twice!  It can NOT be used in a blend function!

Angle direction: 	--- Right_Hand Rule ---
	Z as thumb, X->Y as positive rotation direction.

@x0,y0:		circle center
@r:		radius
@Sang:		start angle, in radian.
@Eang:		end angle, in radian.

Midas Zhou
-----------------------------------------------------------------------------*/
void draw_filled_pieSlice(FBDEV *dev, int x0, int y0, int r, float Sang, float Eang )
{
	int 		n,i;
	double 		step_angle;
	EGI_POINT 	points[3];
	points[0].x=x0;
	points[0].y=y0;

	/* start point */
	points[1].x=round(x0+r*cos(Sang));
	points[1].y=round(y0+r*sin(Sang)); /* Notice LCD -Y direction */

	/* get step angle, 1 pixel arc */
//	step_angle=2.0*asin(0.5/r);
	step_angle=1.0*asin(0.5/r);
	n=(Eang-Sang)/step_angle;
	if(n<0) {
		n=-n;
		step_angle=-step_angle;
	}

	/* draw arcs */
	for(i=1; i<n-1; i++) {
		points[2].x=round(x0+r*cos(Sang+i*step_angle));
		points[2].y=round(y0+r*sin(Sang+i*step_angle));   /* Notice LCD -Y direction */
		draw_filled_triangle(dev, points);
		points[1].x=points[2].x;
		points[1].y=points[2].y;
	}

	/* draw the last short */
	points[2].x=round(x0+r*cos(Eang));  /* end point */
	points[2].y=round(y0+r*sin(Eang));
	draw_filled_triangle(dev, points);
}


/*----------------------------------------------
draw a circle,
	(x,y)	circle center
	r	radius  (>0)
Midas Zhou
-----------------------------------------------*/
void draw_circle(FBDEV *dev, int x, int y, int r)
{
	float i;
	int s;

	for(i=0;i<r;i+=0.5)  /* or o.25, there maybe 1 pixel deviation */
	{
//		s=round(sqrt(1.0*r*r-1.0*i*i));
		s=round(sqrt(r*r-i*i));
//		if(i==0)s-=1; /* erase four tips */
		draw_dot(dev,x-s,y+i);
		draw_dot(dev,x+s,y+i);
		draw_dot(dev,x-s,y-i);
		draw_dot(dev,x+s,y-i);
		/* flip X-Y */
		draw_dot(dev,x-i,y+s);
		draw_dot(dev,x+i,y+s);
		draw_dot(dev,x-i,y-s);
		draw_dot(dev,x+i,y-s);
	}

}


/*-----------------------------------------------------------------
OBSELET: replaced by draw_filled_annulus() or draw_filled_annulus2().

draw a circle formed by poly lines.
	(x,y)	circle center
	r	radius
	w	pline width

Note:	1. The final circle looks ugly if 'r' or 'w' is small.!!!!
	2. np=30, for rmax=20 with end circle
	   np=90, for rmax=70 with end circle
	   np=180,  ...!!! FAINT !!!...

Midas Zhou
------------------------------------------------------------------*/
void draw_pcircle(FBDEV *dev, int x0, int y0, int r, unsigned int w)
{
	int i;
	int np=90; /* number of segments in a 1/4 circle */
	double dx[90+1],dy[90+1];
	EGI_POINT points[90+1]; /* points on 1/4 circle */

	for(i=0; i<np+1; i++) {
		dx[i]=1.0*r*cos(90.0/np*i/180.0*MATH_PI); //cos(1.0*np*i*MATH_PI/2.0);
		dy[i]=1.0*r*sin(90.0/np*i/180.0*MATH_PI);  // sin(1.0*np*i*MATH_PI/2.0);
	}

	/* draw 4th quadrant */
	for(i=0; i<np+1; i++) {
		points[i].x=round(x0+dx[i]);
		points[i].y=round(y0+dy[i]);
	}
//	points[0].x -=1; /* erase tip */
//	points[np].y -=1;
	draw_pline(dev,points,np+1,w);

	/* draw 3rd quadrant */
	for(i=0; i<np+1; i++) {
		points[i].x = (x0<<1) - points[i].x;
	}
	draw_pline(dev,points,np+1,w);

	/* draw 2nd quadrant */
	for(i=0; i<np+1; i++) {
		points[i].y = (y0<<1) - points[i].y;
	}
	draw_pline(dev,points,np+1,w);

	/* draw 1st quadrant */
	for(i=0; i<np+1; i++) {
		points[i].x = (x0<<1) - points[i].x;
	}
	draw_pline(dev,points,np+1,w);

}


/*-------------------------------------------------
Draw a a triangle.

@points:  A pointer to 3 EGI_POINTs / Or an array;

Midas Zhou
-------------------------------------------------*/
void draw_triangle(FBDEV *dev, EGI_POINT *points)
{
	if(points==NULL)
		return;

	int i;

	for(i=0; i<3; i++) {
		 draw_line(dev, points[i].x, points[i].y,
				points[(i+1)%3].x, points[(i+1)%3].y );
	}
}


/*-----------------------------------------------------------------
Draw a a filled triangle.

Note:
  1. fround() to improve accuracy, but NOT speed.

@points:  A pointer to 3 EGI_POINTs / Or an array;

Midas Zhou
------------------------------------------------------------------*/
void draw_filled_triangle(FBDEV *dev, EGI_POINT *points)
{
	if(points==NULL)
		return;

	int i;
	int nl=0,nr=0; /* left and right point index */
	int nm; /* mid point index */

	float klr,klm,kmr;

	/* OR use INT type */
	float yu=0;
	float yd=0;
	float ymu=0;

	for(i=1;i<3;i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

	/* three points are collinear */
	if(nl==nr) {
		draw_pline_nc(dev, points, 3, 1);
		return;
	}

	/* get x_mid point index */
	nm=3-nl-nr;

	//printf("points[nl=%d] x=%d, y=%d \n", nl,points[nl].x,points[nl].y);
	//printf("points[nm=%d] x=%d, y=%d \n", nm,points[nm].x,points[nm].y);
	//printf("points[nr=%d] x=%d, y=%d \n", nr,points[nr].x,points[nr].y);


//	if(points[nr].x != points[nl].x) { /* Ruled out */
		klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);
//	}
//	else
//		klr=1000000.0; /* a big value */

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

	/* draw lines for two tri */
	for( i=0; i<points[nm].x-points[nl].x+1; i++)
	{
		yu=klr*i+points[nl].y;	//points[nl].y+klr*i;
		yd=klm*i+points[nl].y;	//points[nl].y+klm*i;
		//printf("part1: x=%d	yu=%d	yd=%d \n", points[nl].x+i, yu, yd);
		draw_line_simple(dev, points[nl].x+i, roundf(yu), points[nl].x+i, roundf(yd));
	}
	ymu=yu;
	for( i=0; i<points[nr].x-points[nm].x+1; i++)
	{
		yu=klr*i+ymu;          //yu=ymu+klr*i;
		yd=kmr*i+points[nm].y; //yd=points[nm].y+kmr*i;
		//printf("part2: x=%d	yu=%d	yd=%d \n", points[nm].x+i, yu, yd);
		draw_line_simple(dev, points[nm].x+i, roundf(yu), points[nm].x+i, roundf(yd));
	}

	/* Draw outline: NOTE: draw_triangle() gives a litter bigger outline. */
	//draw_triangle(dev, points);
}

/*-----------------------------------------------------------------
Draw a a filled triangle.

@dev: 		FB device
@points:  	A pointer to 3 EGI_POINTs / Or an array.
@color:		Color of the filled triangle.
@alpha:	  	Alpha value of the filled triangle.

Midas Zhou
------------------------------------------------------------------*/
void draw_blend_filled_triangle(FBDEV *dev, EGI_POINT *points, EGI_16BIT_COLOR color, uint8_t alpha)
{
	bool save_pixcolor_on;

	if(dev==NULL)
		return;

	/* turn on FBDEV pixcolor and pixalpha_hold */
	save_pixcolor_on=dev->pixcolor_on;
	dev->pixcolor_on=true;
	dev->pixcolor=color;
	dev->pixalpha_hold=true;
	dev->pixalpha=alpha;

	draw_filled_triangle(dev, points);

	/* turn off FBDEV pixcolor and pixalpha_hold */
	dev->pixcolor_on=save_pixcolor_on;
	dev->pixalpha_hold=false;
	dev->pixalpha=255;
}


/*-----------------------------------------------------------------------
draw a filled annulus.
	(x0,y0)	circle center
	r	radius in mid of w.
	w	width of annulus.

Note:
	1. When w=1, the result annulus is a circle with some
	   unconnected dots.!!!
	2. Distance is defined as pixel center to pixel center.
Midas
--------------------------------------------------------------------------*/
void draw_filled_annulus(FBDEV *dev, int x0, int y0, int r, unsigned int w)
{
	int j,k;
	int m;
	int ro=r+(w>>1); /* outer radium */
	int ri=r-(w>>1); /* inner radium */

        for(j=0; j<ro; j++) /* j<=ro,  j=ro erased here!!!  */
        {
		/* distance from Xcenter to the point on outer circle */
	        m=round(sqrt( ro*ro-j*j ));
		//m=mat_fp16_sqrtu32(ro*ro-j*j)>>16; /* Seems no effect */
		//m-=1; /* diameter is odd */

		/* distance from Xcenter to the point on inner circle */
                if(j<ri) {
                        k=round(sqrt( ri*ri-j*j));
			//k=mat_fp16_sqrtu32(ri*ri-j*j)>>16;
			//k-=1; /* diameter is odd */
		}
                else
                        k=0;

  		/* draw 4th quadrant */
		draw_line(dev,x0+k,y0+j,x0+m,y0+j);
		/* draw 3rd quadrant */
		if(j>=ri) /* exclude overlapped line between 4th/3rd quadrant */
			draw_line(dev,x0-1,y0+j,x0-m,y0+j);
		else
	   		draw_line(dev,x0-k,y0+j,x0-m,y0+j);
		/* draw 2nd quadrant */
		if(j>0)  /* exclude overlapped line between 3rd/2nd quadrant */
			draw_line(dev,x0-k,y0-j,x0-m,y0-j);
		/* draw 1st quadrant */
		if(j>0) { /* exclude overlapped line between 1st/4th quadrant */
			if(j>=ri)  /* exclude overlapped line between 1st/2nd quadrant */
				draw_line(dev,x0+1,y0-j,x0+m,y0-j);
			else
				draw_line(dev,x0+k,y0-j,x0+m,y0-j);
		}
	}
}


/*-----------------------------------------------------------------------
draw a filled annulus blended with backcolor of FB.

@dev: 		FB device
@(x0,y0):	circle center
@r:		radius in mid of w.
@w:		width of annulus.
@color:		Color of the filled circle
@alpha:		Alpha value of the filled circle

Note:
	1. When w=1, the result annulus is a circle with some
	   unconnected dots.!!!
Midas Zhou
--------------------------------------------------------------------------*/
void draw_blend_filled_annulus( FBDEV *dev, int x0, int y0, int r, unsigned int w,
			 	EGI_16BIT_COLOR color, uint8_t alpha )
{
	bool save_pixcolor_on;

	if(dev==NULL)
		return;

	/* turn on FBDEV pixcolor and pixalpha_hold */
	save_pixcolor_on=dev->pixcolor_on;
	dev->pixcolor_on=true;
	dev->pixcolor=color;
	dev->pixalpha_hold=true;
	dev->pixalpha=alpha;

	draw_filled_annulus( dev, x0, y0, r, w);

	/* turn off FBDEV pixcolor and pixalpha_hold */
	dev->pixcolor_on=save_pixcolor_on;
	dev->pixalpha_hold=false;
	dev->pixalpha=255;
}


/*------------------------------------------------
  draw a filled circle
  Midas Zhou
-------------------------------------------------*/
void draw_filled_circle(FBDEV *dev, int x, int y, int r)
{
	int i;
	int s;

	for(i=0;i<r;i++)
	{
		s=round(sqrt(r*r-i*i));
		s-=1; /* diameter is always odd */

		//if(i==0)s-=1; /* erase four tips */
		if(i!=0)	/* Otherwise it will call draw_line(dev,x-s,y,x+s,y) twice! */
			draw_line(dev,x-s,y+i,x+s,y+i);
		draw_line(dev,x-s,y-i,x+s,y-i);
	}
}



/*----------------------------------------------------------
draw a filled circle blended with backcolor of the FB
Midas Zhou

@dev: 		FB device
@color:		Color of the filled circle
@alpha:		Alpha value of the filled circle
@x,y:		Center coordinate of the circle
@r:		Radius of the circle

Midas Zhou
-----------------------------------------------------------*/
void draw_blend_filled_circle( FBDEV *dev, int x, int y, int r,
			       EGI_16BIT_COLOR color, uint8_t alpha )
{
	bool save_pixcolor_on;

	if(dev==NULL)
		return;

	/* turn on FBDEV pixcolor and pixalpha_hold */
	save_pixcolor_on=dev->pixcolor_on;
	dev->pixcolor_on=true;
	dev->pixcolor=color;
	dev->pixalpha_hold=true;
	dev->pixalpha=alpha;

	draw_filled_circle( dev, x, y, r);

	/* turn off FBDEV pixcolor and pixalpha_hold */
	dev->pixcolor_on=save_pixcolor_on;
	dev->pixalpha_hold=false;
	dev->pixalpha=255;
}



/*----------------------------------------------------------------------------
   copy a block of FB memory  to buffer
   x1,y1,x2,y2:	  LCD area corresponding to FB mem. block
   buf:		  data dest.

   NOTE:
 	1. FB.pos_rotate is not supported.

   Return
		2	area out of FB mem
		1	partial area out of FB mem boundary
		0	OK
		-1	fails
   Midas Zhou
----------------------------------------------------------------------------*/
   int fb_cpyto_buf(FBDEV *fb_dev, int px1, int py1, int px2, int py2, uint16_t *buf)
   {
	int i,j;
	int x1,y1,x2,y2;  /* maps to default screen coord. */
	int xl,xr; /* left, right */
	int yu,yd; /* up, down */
	int ret=0;
	long int location=0;
	int xres=fb_dev->vinfo.xres;
	int yres=fb_dev->vinfo.yres;
	int tmpx,tmpy;
	unsigned char *map=NULL; /* the pointer to map FB or back buffer */
	#ifdef LETS_NOTE
	uint32_t argb;	/* for LETS_NOTE */
	#endif

	/* check buf */
	if(buf==NULL)
	{
		printf("fb_cpyto_buf(): buf is NULL!\n");
		return -1;
	}

	/* <<<<<<  FB BUFFER SELECT  >>>>>> */
	#if defined(ENABLE_BACK_BUFFER) || defined(LETS_NOTE)
	map=fb_dev->map_bk; /* write to back buffer */
	#else
	map=fb_dev->map_fb; /* write directly to FB map */;
	#endif

	/* check FB.pos_rotate
	*  IF 90 Deg rotated: Y maps to (xres-1)-FB.X,  X maps to FB.Y
        */
	switch(fb_dev->pos_rotate) {
		case 0:			/* FB defaul position */
			x1=px1;	 y1=py1;
			x2=px2;  y2=py2;
			break;
		case 1:			/* Clockwise 90 deg */
			x1=(xres-1)-py1;  y1=px1;
			x2=(xres-1)-py2;  y2=px2;
			break;
		case 2:			/* Clockwise 180 deg */
			x1=(xres-1)-px1;  y1=(yres-1)-py1;
			x2=(xres-1)-px2;  y2=(yres-1)-py2;
			break;
		case 3:			/* Clockwise 270 deg */
			x1=py1;  y1=(yres-1)-px1;
			x2=py2;  y2=(yres-1)-px2;
			break;
		default:
			x1=px1;	 y1=py1;
			x2=px2;  y2=py2;
			break;
	}

	/* sort point coordinates */
	if(x1>x2){
		xr=x1;
		xl=x2;
	}
	else{
		xr=x2;
		xl=x1;
	}
	if(y1>y2){
		yu=y1;
		yd=y2;
	}
	else{
		yu=y2;
		yd=y1;
	}


#ifdef FB_DOTOUT_ROLLBACK /* -------------   ROLLBACK  ------------------*/

	/* ---------  copy mem --------- */
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{
			/* map i,j to LCD(Y,X) */
			if(i<0) /* map Y */
			{
				tmpy=yres-(-i)%yres; /* here tmpy=1-320 */
				tmpy=tmpy%yres;
			}
			else if(i > yres-1)
				tmpy=i%yres;
			else
				tmpy=i;

			if(j<0) /* map X */
			{
				tmpx=xres-(-j)%xres; /* here tmpx=1-240 */
				tmpx=tmpx%xres;
			}
			else if(j > xres-1)
				tmpx=j%xres;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* copy to buf */
			#ifdef LETS_NOTE
			#else
        		*buf = *((uint16_t *)(map+location));
			#endif

			 buf++;
		}
	}

#else  /* -----------------  NO ROLLBACK  ------------------------*/

	/* normalize x,y */
	if(yd<0)yd=0;
	if(yu>yres-1)yu=yres-1;

	if(xl<0)xl=0;
	if(xr>xres-1)xr=xres-1;

	/* ---------  copy mem --------- */
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{
#if 0 /* This shall never happen now! */
			if( i<0 || j<0 || i>yres-1 || j>xres-1 )
			{
				EGI_PDEBUG(DBG_FBGEOM,"WARNING: fb_cpyfrom_buf(): coordinates out of range!\n");
				ret=1;
			}
#endif
			/* map i,j to LCD(Y,X) */
			if(i>yres-1) /* map Y */
				tmpy=yres-1;
			else if(i<0)
				tmpy=0;
			else
				tmpy=i;

			if(j>xres-1) /* map X */
				tmpx=xres-1;
			else if(j<0)
				tmpx=0;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* copy to buf */
			#ifdef LETS_NOTE
        		argb = *((uint32_t *)(map+location));
			*buf=COLOR_24TO16BITS(argb&0xFFFFFF);
			#else
        		*buf = *((uint16_t *)(map+location));
			#endif

			 buf++;
		}
	}

#endif

	return ret;
   }


 /*----------------------------------------------------------------------------
   copy a block of buffer to FB memory.
   x1,y1,x2,y2:	  LCD area corresponding to FB mem. block
   buf:		  data source

   NOTE:
 	1. FB.pos_rotate is not supported.

   Return
		1	partial area out of FB mem boundary
		0	OK
		-1	fails
   Midas
   ----------------------------------------------------------------------------*/
   int fb_cpyfrom_buf(FBDEV *fb_dev, int px1, int py1, int px2, int py2, const uint16_t *buf)
   {
	int i,j;
	int x1,y1,x2,y2;  /* maps to default screen coord. */
	int xl,xr; /* left right */
	int yu,yd; /* up down */
	int ret=0;
	long int location=0;
	int xres=fb_dev->vinfo.xres;
	int yres=fb_dev->vinfo.yres;
	int tmpx,tmpy;
	unsigned char *map=NULL; /* the pointer to map FB or back buffer */
	#ifdef LETS_NOTE
	uint32_t argb;	/* for LETS_NOTE */
	#endif

	/* check buf */
	if(buf==NULL)
	{
		printf("fb_cpyfrom_buf(): buf is NULL!\n");
		return -1;
	}

	/* <<<<<<  FB BUFFER SELECT  >>>>>> */
	#if defined(ENABLE_BACK_BUFFER) || defined(LETS_NOTE)
	map=fb_dev->map_bk; /* write to back buffer */
	#else
	map=fb_dev->map_fb; /* write directly to FB map */;
	#endif

	/* check FB.pos_rotate
	*  IF 90 Deg rotated: Y maps to (xres-1)-FB.X,  X maps to FB.Y
        */
	switch(fb_dev->pos_rotate) {
		case 0:			/* FB defaul position */
			x1=px1;	 y1=py1;
			x2=px2;  y2=py2;
			break;
		case 1:			/* Clockwise 90 deg */
			x1=(xres-1)-py1;  y1=px1;
			x2=(xres-1)-py2;  y2=px2;
			break;
		case 2:			/* Clockwise 180 deg */
			x1=(xres-1)-px1;  y1=(yres-1)-py1;
			x2=(xres-1)-px2;  y2=(yres-1)-py2;
			break;
		case 3:			/* Clockwise 270 deg */
			x1=py1;  y1=(yres-1)-px1;
			x2=py2;  y2=(yres-1)-px2;
			break;
		default:
			x1=px1;	 y1=py1;
			x2=px2;  y2=py2;
			break;
	}

	/* sort point coordinates */
	if(x1>x2){
		xr=x1;
		xl=x2;
	}
	else{
		xr=x2;
		xl=x1;
	}

	if(y1>y2){
		yu=y1;
		yd=y2;
	}
	else{
		yu=y2;
		yd=y1;
	}


#ifdef FB_DOTOUT_ROLLBACK /* -------------   ROLLBACK  ------------------*/

	/* ------------ copy mem ------------*/
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{			/* map i,j to LCD(Y,X) */
			if(i<0) /* map Y */
			{
				tmpy=yres-(-i)%yres; /* here tmpy=1-320 */
				tmpy=tmpy%yres;
			}
			else if(i>yres-1)
				tmpy=i%yres;
			else
				tmpy=i;

			if(j<0) /* map X */
			{
				tmpx=xres-(-j)%xres; /* here tmpx=1-240 */
				tmpx=tmpx%xres;
			}
			else if(j>xres-1)
				tmpx=j%xres;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* --- copy to fb ---*/
			#ifdef LETS_NOTE
			#else
        		*((uint16_t *)(map+location))=*buf;
			#endif
			buf++;
		}
	}

#else  /* -----------------  NO ROLLBACK  ------------------------*/

	/* normalize x,y */
	if(yd<0)yd=0;
	if(yu>yres-1)yu=yres-1;

	if(xl<0)xl=0;
	if(xr>xres-1)xr=xres-1;

	/* ------------ copy mem ------------*/
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{
#if 0 /* This shall never happen now */
			if( i<0 || j<0 || i>yres-1 || j>xres-1 )
			{
				EGI_PDEBUG(DBG_FBGEOM,"WARNING: fb_cpyfrom_buf(): coordinates out of range!\n");
				ret=1;
			}
#endif ////////////////////////////////////////////////////////////
			/* map i,j to LCD(Y,X) */
			if(i>yres-1) /* map Y */
				tmpy=yres-1;
			else if(i<0)
				tmpy=0;
			else
				tmpy=i;

			if(j>xres-1) /* map X */
				tmpx=xres-1;
			else if(j<0)
				tmpx=0;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* --- copy to fb ---*/
			#ifdef LETS_NOTE
			argb=COLOR_16TO24BITS(*buf)+(255<<24);
			*((uint32_t *)(map+location))=argb;
			#else

        		*((uint16_t *)(map+location))=*buf;

			#endif

			buf++;
		}
	}
#endif

	return ret;
   }


#if 0 ///////////////////////////////////////////////////////
/*--------------------------------------------
Save whole data of FB to buffer.

@fb_dev:	pointer to FB dev.
@nb:		index of FBDEV.buffer[]

NOTE:
1. FB.pos_rotate is not supported.

Return
                0       OK
                <0 	fails
Midas Zhou
----------------------------------------------*/
int fb_buffer_FBimg(FBDEV *fb_dev, int nb)
{
	if(fb_dev==NULL || nb<0 || nb > FBDEV_BUFFER_PAGES-1 )
		return -1;

	int xres=fb_dev->vinfo.xres;
	int yres=fb_dev->vinfo.yres;

	/* callc buffer if it's NULL */
	if(fb_dev->buffer[nb]==NULL) {
		fb_dev->buffer[nb]=calloc(1,fb_dev->screensize); /* 2bytes color */
		if(fb_dev->buffer[nb]==NULL) {
			printf("%s: fail to calloc fb_dev->buffer[%d].\n",__func__, nb);
			return -2;
		}
	}

	if( fb_cpyto_buf(fb_dev, 0, 0, xres-1, yres-1, fb_dev->buffer[nb]) !=0 )
		return -2;

	else
		return 0;
}


/*----------------------------------------------------
Restor buffed data to FB.
@fb_dev:	pointer to FB dev.
@nb:		index of FBDEV.buffer[]
@clear:		if TRUE, clear buffer after restore.

NOTE:
1. FB.pos_rotate is not supported.


Return
                0       OK
                <0  	fails
Midas Zhou
------------------------------------------------------*/
int fb_restore_FBimg(FBDEV *fb_dev, int nb, bool clean)
{
	if(fb_dev==NULL || nb<0 || nb > FBDEV_BUFFER_PAGES-1 )
		return -1;

	int xres=fb_dev->vinfo.xres;
	int yres=fb_dev->vinfo.yres;

   	if ( fb_cpyfrom_buf(fb_dev, 0, 0, xres-1, yres-1, fb_dev->buffer[nb]) !=0 )
		return -2;

	if(clean) {
		free(fb_dev->buffer[nb]);
		fb_dev->buffer[nb]=NULL;
	}

	return 0;
}
#endif //////////////////////////////////////////////////////////////


/*--------------------------------------- Method 1 -------------------------------------------
1. draw an image through a map of rotation.
2.

n:		side pixel number of a square image. to be inform of 2*m+1;
x0y0:		left top coordinate of the square.
image:		16bit color buf of a square image
SQMat_XRYR:	Rotation map matrix of the square.
 		point(i,j) map to egi_point_coord SQMat_XRYR[n*i+j]
Midas
------------------------------------------------------------------------------------------*/
/*----------------------- Drawing for method 1  -------------------------*/
#if 0
void fb_drawimg_SQMap(int n, struct egi_point_coord x0y0, uint16_t *image,
						const struct egi_point_coord *SQMat_XRYR)
{
	int i,j,k,m;
	int s;
//	int mapx,mapy;
	uint16_t color;

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("fb_drawimg_SQMap(): the number of pixels on the square side must be n=2*m+1.\n");
	 	return;
	}

	/* map each image point index k to get rotated position index m,
	   then draw the color to index m position
	   only sort out points within the circle */
	for(i=-n/2-1;i<n/2;i++) /* row index,from top to bottom, -1 to compensate precision loss */
	{
		s=sqrt(n*n/4-i*i);
		for(j=n/2+1-s;j<=n/2+s;j++) /* +1 to compensate precision loss */
		{
			k=(i+n/2)*n+j;
			/*  for current point index k, get mapped point index m, origin left top */
			m=SQMat_XRYR[k].y*n+SQMat_XRYR[k].x; /* get point index after rotation */
			fbset_color(image[k]);
			draw_dot(&gv_fb_dev, x0y0.x+m%n, x0y0.y+m/n);
		}
	}
}
#endif


/*----------------------- Drawing for Method: revert rotation  --------------------------------
n:              pixel number for square side.
x0y0:		origin point coordinate of the square image,  in LCD coord system.
centxy:         the center point coordinate of the concerning square area of LCD(fb).
image:		original square shape image R5G6B5 data.
SQMat_XRYR:     after rotation
                square matrix after rotation mapping, coordinate origin is same as LCD(fb) origin.
--------------------------------------------------------------------------------------------------*/
void fb_drawimg_SQMap(int n, struct egi_point_coord x0y0, const uint16_t *image,
						const struct egi_point_coord *SQMat_XRYR)
{
	int i,j,k;

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("fb_drawimg_SQMap(): the number of pixels on the square side must be n=2*m+1.\n");
	 	return;
	}

	for(i=0;i<n;i++)
		for(j=0;j<n;j++)
		{
			/* since index i point map to  SQMat_XRYR[i](x,y), so get its mapped index k */
			k=SQMat_XRYR[i*n+j].y*n+SQMat_XRYR[i*n+j].x;
			//printf("k=%d\n",k);
			fbset_color(image[k]);
			draw_dot(&gv_fb_dev, x0y0.x+j, x0y0.y+i); /* ???? n-j */
		}
}



/*------------------------ Draw annulus mapping ------------------------------------------------
n:              pixel number for ouside square side. also is the outer diameter for the annulus.
ni:             pixel number for inner square side, also is the inner diameter for the annulus.
x0y0:		origin point coordinate of the square image,  in LCD coord system.
centxy:         the center point coordinate of the concerning square area of LCD(fb).
image:		original square shape image R5G6B5 data.
ANMat_XRYR:     after rotation
                square matrix after rotation mapping, coordinate origin is same as LCD(fb) origin.

Midas Zhou
----------------------------------------------------------------------------------------------*/
void fb_drawimg_ANMap(int n, int ni, struct egi_point_coord x0y0, const uint16_t *image,
		            			       const struct egi_point_coord *ANMat_XRYR)
{
	int i,j,m,k,t,pn;

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("fb_drawimg_ANMap(): WARNING: the number of pixels on the square side must be n=2*m+1.\n");
	 	//return;
	}

	/* Drawing only points of the annulus */
        for(j=0; j<=n/2;j++) /* row index,Y: 0->n/2 */
        {
                m=sqrt( (n/2)*(n/2)-j*j ); /* distance from Y to the point on outer circle */

                if(j<ni/2)
                        k=sqrt( (ni/2)*(ni/2)-j*j); /* distance from Y to the point on inner circle */
                else
                        k=0;

                for(i=-m;i<=-k;i++) /* colum index, X: -m->-n, */
                {
			/* since index i point map to  ANMat_XRYR[i](x,y), so get its mapped index t */

                        /* upper and left part of the annuls j: 0->n/2*/
			pn=(n/2-j)*n+n/2+i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2+i, x0y0.y+n/2-j);

                        /* lower and left part of the annulus -j: 0->-n/2*/
			pn=(n/2+j)*n+n/2+i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2+i, x0y0.y+n/2+j);

                        /* upper and right part of the annuls -i: m->n */
			pn=(n/2-j)*n+n/2-i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2-i, x0y0.y+n/2-j);

                        /* lower and right part of the annulus */
			pn=(n/2+j)*n+n/2-i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2-i, x0y0.y+n/2+j);
		}
	}
}



/*-----------------------------------------------------------------------------
Scale a block of pixel buffer in a simple way.
@ owid,ohgt:	old/original width and height of the pixel area
@ nwid,nhgt:   	new width and height
@ obuf:        	old data buffer for 16bit color pixels
@ nbuf;		scaled data buffer for 16bit color pixels

TODO:  interpolation mapping

Return
	1	partial area out of FB mem boundary
	0	OK
	<0	fails

Midas Zhou
------------------------------------------------------------------------------*/
int fb_scale_pixbuf(unsigned int owid, unsigned int ohgt, unsigned int nwid, unsigned int nhgt,
			const uint16_t *obuf, uint16_t *nbuf)
{
	int i,j;
	int imap,jmap;

	/* 1. check data */
	if(owid==0 || ohgt==0 || nwid==0 || nhgt==0) {
		printf("%s: pixel area is 0! fail to scale.\n",__func__);
		return -1;
	}
	if(obuf==NULL || nbuf==NULL) {
		printf("%s: old or new pixel buffer is NULL! fail to scale.\n",__func__);
		return -2;
	}

	/* 2. map pixel from old area to new area */
	for(i=1; i<nhgt+1; i++) { /* pixel row index */
		for(j=1; j<nwid+1; j++) { /* pixel column index */
			/* map i,j to old buf index imap,jmap */
			//imap= round( (float)i/(float)nhgt*(float)ohgt );
			//jmap= round( (float)j/(float)nwid*(float)owid );
			imap= i*ohgt/nhgt;
			jmap= j*owid/nwid;
			//PDEBUG("fb_scale_pixbuf(): imap=%d, jmap=%d\n",imap,jmap);
			/* get mapped pixel color */
			nbuf[ (i-1)*nwid+(j-1) ] = obuf[ (imap-1)*owid+(jmap-1) ];
		}
	}

	return 0;
}

/*-------------------------------------------------------------------
to get a new EGI_POINT by interpolation of 2 points

interpolation direction: from pa to pb
pn:	interpolate point.
pa,pb:	2 points defines the interpolation line.
off:	distance from pa to pn.
	>0  directing from pa to pb.
	<0  directiong from pb to pa.

return:
	2	pn extend from pa
	1	pn extend from pb
	0	ok
	<0	fails, or get end of

Midas Zhou
--------------------------------------------------------------------*/
int egi_getpoit_interpol2p(EGI_POINT *pn, int off, const EGI_POINT *pa, const EGI_POINT *pb)
{
	int ret=0;
	float cosang,sinang;

	/* distance from pa to pb */
	float s=sqrt( (pb->x-pa->x)*(pb->x-pa->x)+(pb->y-pa->y)*(pb->y-pa->y) );
	/* cosine and sine of the slop angle */
	if(s==0)
		return -1;
	else
	{
		cosang=1.0*(pb->x-pa->x)/s;
		sinang=1.0*(pb->y-pa->y)/s;
	}
	/* check if out of range */
	if(off>s)ret=1;
	if(off<0)ret=2;

	/* interpolate point */
	pn->x=roundf(pa->x+cosang*off);
	pn->y=roundf(pa->y+sinang*off);

	return ret;
}

/*--------------------------------------------------------
calculate number of steps between 2 points
step:	length of each step
pa,pb	two points

return:
	>0 	number of steps
	=0 	if pa==pb or s<step

Midas Zhou
---------------------------------------------------------*/
int egi_numstep_btw2p(int step, const EGI_POINT *pa, const EGI_POINT *pb)
{
        /* distance from pa to pb */
        float s=sqrt( (pb->x-pa->x)*(pb->x-pa->x)+(pb->y-pa->y)*(pb->y-pa->y) );

	if(step <= 0)
	{
		printf("egi_numstep_btw2p(): step must be greater than zero!\n");
		return -1;
	}

	if(s==0)
	{
		printf("egi_numstep_btw2p(): WARNING!!! point A and B is the same! \n");
		return 0;
	}

	return s/step;
}


/*--------------------------------------------------------
pick a random point within a box

pr:	pointer to a point.
box:	the limit box.

return:
	0	OK
	<0	fail
Midas
---------------------------------------------------------*/
int egi_randp_inbox(EGI_POINT *pr, const EGI_BOX *box)
{
	if(pr==NULL || box==NULL)
	{
		printf("egi_randp_inbox(): pr or box is NULL! \n");
		return -1;
	}

	EGI_POINT pa=box->startxy;
	EGI_POINT pb=box->endxy;

	pr->x=pa.x+(egi_random_max(pb.x-pa.x)-1);  /* if x=-10, -8<= egi_random_max(x) <=1 */
	pr->y=pa.y+(egi_random_max(pb.y-pa.y)-1); /* if x=10,  1<= egi_random_max(x) <=10 */

	return 0;
}


/*--------------------------------------------------------
pick a random point on a box's sides.

pr:	pointer to a point wihin the box.
box:	the box.

return:
	0	OK
	<0	fail
Midas Zhou
---------------------------------------------------------*/
int egi_randp_boxsides(EGI_POINT *pr, const EGI_BOX *box)
{
	if(pr==NULL || box==NULL)
	{
		printf("egi_randp_boxsides(): pr or box is NULL! \n");
		return -1;
	}

	EGI_POINT pa=box->startxy;
	EGI_POINT pb=box->endxy;

	if( egi_random_max(2) -1 ) /* on Width sides */
	{
		pr->x=pa.x+(egi_random_max(pb.x-pa.x)-1);  /* if x=-10, -8<= egi_random_max(x) <=1 */
		if( egi_random_max(2) -1 ) /* up or down side */
			pr->y=pa.y;
		else
			pr->y=pb.y;
	}
	else /* on Heigh sides */
	{
		pr->y=pa.y+(egi_random_max(pb.y-pa.y)-1);  /* if x=-10, -8<= egi_random_max(x) <=1 */
		if( egi_random_max(2) -1 ) /* left or right side */
			pr->x=pa.x;
		else
			pr->x=pb.x;
	}

	return 0;
}



/*----------------------------------------------
With param color
Midas Zhou
-----------------------------------------------*/
void draw_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color)
{
	fbset_color2(dev, color);
	draw_circle(dev, x,y, r);
}
void draw_filled_annulus2(FBDEV *dev, int x0, int y0, int r, unsigned int w, EGI_16BIT_COLOR color)
{
	fbset_color2(dev, color);
	draw_filled_annulus(dev, x0, y0, r,  w);
}
void draw_filled_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color)
{
	fbset_color2(dev, color);
	draw_filled_circle(dev, x, y,r);
}


/*-----------------------------------------------------------------------------
Draw a cubic spline passing through given points.
Reference: https://www.cnblogs.com/xpvincent/archive/2013/01/26/2878092.html

Note:
1.  A*M=B: Combinate A and B into  matrix MatAB[np*(np+1)], call GaussSolve to
    resolve it.
2. i_th segment of cubic spline expression:
    	y(x)=a[i]+b[i]*(x-x[i])+c[i]*(x-x[i])^2+d[i]*(x-x[i])^3
	where:
		i=[0, N-1]:  segment curve index number
		      x[i]:  start point.x of the segment curve.

                        --- !!! IMPORTANT !!! ---
3. Pxy[] all points MUST meets:  x[0]... x[i-1] < x[i] < x[i+1] ...x[np-1]
4. Changing a control point will affect the whole curve.

@np:		Number of input points.
@pxy:		Input points
@endtype:	Boundary type:
		1 --- Clampled	2 --- Not_A_Knot    Others --- Natural
@w:		Width of line.

Return:
	0	OK
	<0	Fails
Mias Zhou
-----------------------------------------------------------------------------*/
#include <egi_matrix.h>
int draw_spline(FBDEV *fbdev, int np, EGI_POINT *pxy, int endtype, unsigned int w)
{
	int ret=0;
	int k;
	int n=np-1;		 	/* segments of curves */
	EGI_MATRIX *MatAB=NULL;	 	/* Dimension [np*(np+1)] */
	EGI_MATRIX *MatM=NULL;	 	/* m[i]=2*c[i] */
	float *a=NULL;	 		/* Cubic spline coefficients: y=a+b*(x-xi)+c*(x-xi)^2+d*(x-xi)^3*/
	float *b=NULL,*c=NULL,*d=NULL;  /* To calloc together with a */
	float *h=NULL;			/* Step: h[i]=x[i+1]-x[i] */
	int ptx;
	float xs=0,ys=0,xe=0,ye=0;	/* start,end x,y */
	float dx=0.0;			/* dx=(x-xi) */
	int step;

	/* check np */
	if(np<2)
		return -1;

	/* Init matrix AB [6*7] */
   	MatAB=init_float_Matrix(np,np+1);
   	if(MatAB==NULL) {
                fprintf(stderr,"%s: Fail to init. MatAB!\n",__func__);
                return -1;
        }

	/* Init x step h[] */
	h=calloc(n, sizeof(float));
	if(h==NULL) {
                fprintf(stderr,"%s: Fail to calloc step h!\n",__func__);
                ret=-2;
		goto END_FUNC;
	}

	/* Init a,b,c,d */
	a=calloc(4*n, sizeof(float));
	if(a==NULL) {
                fprintf(stderr,"%s: Fail to calloc a,b,c,d!\n",__func__);
                ret=-3;
		goto END_FUNC;
	}
	b=a+n;
	c=b+n;
	d=c+n;

	/* Cal. step h[] */
	for(k=0; k<n; k++)
		h[k]=pxy[k+1].x-pxy[k].x;

	/* Fill A to  matrixAB */

	/* MatAB[]: Row 0 and np as for boundary conditions */
	switch( endtype )
	{
		/* 1. Clamped Spline */
		case 1:
			MatAB->pmat[0]=2*h[0];
			MatAB->pmat[1]=h[0];
			MatAB->pmat[np*(np+1)-3]=h[n-1];
			MatAB->pmat[np*(np+1)-2]=2*h[n-1];
			break;

		/* 2. Not_A_Kont Spline */
		case 2:
			MatAB->pmat[0]=-h[1];
			MatAB->pmat[1]=h[0]+h[1];
			MatAB->pmat[2]=-h[0];
			MatAB->pmat[np*(np+1)-4]=-h[n-1];
			MatAB->pmat[np*(np+1)-3]=h[n-1]+h[n-2];
			MatAB->pmat[np*(np+1)-2]=-h[n-2];
			break;

		/* 3. Natrual Spline */
		default:
			MatAB->pmat[0]=1;
			MatAB->pmat[np*(np+1)-2]=1;
			break;
	}

	/* MatAB[]: From Row (1) to Row (np-2), As k+1=(1 -> np-2), so k=(0 -> np-3) */
	for(k=0; k<np-2; k++) {
		/* Mat A [np*np] */
		MatAB->pmat[(1+k)*(np+1)+k]=h[k];			/* MatAB [np*(np+1)] */
		MatAB->pmat[(1+k)*(np+1)+k+1]=2.0*(h[k]+h[k+1]);
		MatAB->pmat[(1+k)*(np+1)+k+2]=h[k+1];
		/* Mat B [np*1]:  B[0] and B[np-1] all zeros */
		MatAB->pmat[(1+k)*(np+1)+np]=6.0*( (pxy[k+2].y-pxy[k+1].y)/h[k+1]-(pxy[k+1].y-pxy[k].y)/h[k] );
	}
	/* TEST---- */
//	Matrix_Print(MatAB);

	/* --- Solve matrix --- */
#if 0  /* OPTION 1: Guass Elimination */
	MatM=Matrix_GuassSolve(MatAB);
#else  /* OPTION 2: Thomas Algorithm (The Tridiagonal Matrix Algorithom) */
	MatM=Matrix_ThomasSolve(NULL,MatAB);
#endif
	if(MatM==NULL) {
                fprintf(stderr,"%s: Fail to solve the matrix equation!\n",__func__);
		ret=-4;
		goto END_FUNC;
	}


	/* TEST---- */
//	Matrix_Print(MatM);

	/* Calculate coefficients for each cubic curve */
	for(k=0; k<n; k++) {
		a[k]=pxy[k].y;
		b[k]=(pxy[k+1].y-pxy[k].y)/h[k] -h[k]*MatM->pmat[k]/3.0 -h[k]*MatM->pmat[k+1]/6.0;
		c[k]=MatM->pmat[k]/2.0;
		d[k]=(MatM->pmat[k+1]-MatM->pmat[k])/6.0/h[k];
	}

	/* Draw n segements of curves: Notice pxy[] is INT type! */
	for(k=0; k<n; k++) {
//		printf("k=%d\n",k); //getchar();
		/* Step */
		if(pxy[k].x <  pxy[k+1].x) step=1;
		else step=-1;

		/* Draw a curve/segment with many short lines: pxy[k]->pxy[k+1], step +/-1 */
		for( ptx=pxy[k].x; 	ptx != pxy[k+1].x; 	ptx +=step) {   /* NOTICE: All integer type! */
			/* Get two ends of a short line */
			if(xs==0 && xe==0) { /* Token as the first line of the [k]th segment */
				xs=ptx; 	dx=0;	/* dx = (x-xi) */
				ys=a[k]+b[k]*dx+c[k]*pow(dx,2)+d[k]*pow(dx,3);

				xe=xs+step;	dx+=step;
				ye=a[k]+b[k]*dx+c[k]*pow(dx,2)+d[k]*pow(dx,3);
			} else {	    /* Not first line */
				xs=xe; ys=ye;
				xe=xs+step;	dx+=step;
				ye=a[k]+b[k]*dx+c[k]*pow(dx,2)+d[k]*pow(dx,3);
			}

			/* Draw short line */
			draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w);
		}

		/* reste xs,xe as for token */
		xs=0; xe=0;
	}

END_FUNC:
	/* free */
	free(h);
	free(a);
	release_float_Matrix(&MatM);
	release_float_Matrix(&MatAB);

	return ret;
}


/*---------------------------------------------------
Draw a filled cubic spline, areas between spline and
line y=baseY will be filled with color/alpha.

see also draw_spline().

Return:
	0	OK
	<0	Fails

Midas Zhou
---------------------------------------------------*/
#include <egi_matrix.h>
int draw_filled_spline( FBDEV *fbdev, int np, EGI_POINT *pxy, int endtype, unsigned int w,
			int baseY, EGI_16BIT_COLOR color, EGI_8BIT_ALPHA alpha)
{
	int ret=0;
	int k;
	int n=np-1;		 	/* segments of curves */
	EGI_MATRIX *MatAB=NULL;	 	/* Dimension [np*(np+1)] */
	EGI_MATRIX *MatM=NULL;	 	/* m[i]=2*c[i] */
	float *a=NULL;	 		/* Cubic spline coefficients: y=a+b*(x-xi)+c*(x-xi)^2+d*(x-xi)^3*/
	float *b=NULL,*c=NULL,*d=NULL;  /* To calloc together with a */
	float *h=NULL;			/* Step: h[i]=x[i+1]-x[i] */
	int ptx;
	float xs=0,ys=0,xe=0,ye=0;	/* start,end x,y */
	float dx=0.0;			/* dx=(x-xi) */
	int step;
	int kn;
	bool save_pixcolor_on;

	/* check np */
	if(np<2)
		return -1;

	/* Init matrix AB [6*7] */
   	MatAB=init_float_Matrix(np,np+1);
   	if(MatAB==NULL) {
                fprintf(stderr,"%s: Fail to init. MatAB!\n",__func__);
                return -1;
        }

	/* Init x step h[] */
	h=calloc(n, sizeof(float));
	if(h==NULL) {
                fprintf(stderr,"%s: Fail to calloc step h!\n",__func__);
                ret=-2;
		goto END_FUNC;
	}

	/* Init a,b,c,d */
	a=calloc(4*n, sizeof(float));
	if(a==NULL) {
                fprintf(stderr,"%s: Fail to calloc a,b,c,d!\n",__func__);
                ret=-3;
		goto END_FUNC;
	}
	b=a+n;
	c=b+n;
	d=c+n;

	/* Cal. step h[] */
	for(k=0; k<n; k++)
		h[k]=pxy[k+1].x-pxy[k].x;

	/* Fill A to  matrixAB */

	/* MatAB[]: Row 0 and np as for boundary conditions */
	switch( endtype )
	{
		/* 1. Clamped Spline */
		case 1:
			MatAB->pmat[0]=2*h[0];
			MatAB->pmat[1]=h[0];
			MatAB->pmat[np*(np+1)-3]=h[n-1];
			MatAB->pmat[np*(np+1)-2]=2*h[n-1];
			break;

		/* 2. Not_A_Kont Spline */
		case 2:
			MatAB->pmat[0]=-h[1];
			MatAB->pmat[1]=h[0]+h[1];
			MatAB->pmat[2]=-h[0];
			MatAB->pmat[np*(np+1)-4]=-h[n-1];
			MatAB->pmat[np*(np+1)-3]=h[n-1]+h[n-2];
			MatAB->pmat[np*(np+1)-2]=-h[n-2];
			break;

		/* 3. Natrual Spline */
		default:
			MatAB->pmat[0]=1;
			MatAB->pmat[np*(np+1)-2]=1;
			break;
	}

	/* MatAB[]: From Row (1) to Row (np-2), As k+1=(1 -> np-2), so k=(0 -> np-3) */
	for(k=0; k<np-2; k++) {
		/* Mat A [np*np] */
		MatAB->pmat[(1+k)*(np+1)+k]=h[k];			/* MatAB [np*(np+1)] */
		MatAB->pmat[(1+k)*(np+1)+k+1]=2.0*(h[k]+h[k+1]);
		MatAB->pmat[(1+k)*(np+1)+k+2]=h[k+1];
		/* Mat B [np*1]:  B[0] and B[np-1] all zeros */
		MatAB->pmat[(1+k)*(np+1)+np]=6.0*( (pxy[k+2].y-pxy[k+1].y)/h[k+1]-(pxy[k+1].y-pxy[k].y)/h[k] );
	}
	/* TEST---- */
//	Matrix_Print(MatAB);

	/* --- Solve matrix --- */
#if 0  /* OPTION 1: Guass Elimination */
	MatM=Matrix_GuassSolve(MatAB);
#else  /* OPTION 2: Thomas Algorithm (The Tridiagonal Matrix Algorithom) */
	MatM=Matrix_ThomasSolve(NULL,MatAB);
#endif
	if(MatM==NULL) {
                fprintf(stderr,"%s: Fail to solve the matrix equation!\n",__func__);
		ret=-4;
		goto END_FUNC;
	}


	/* TEST---- */
//	Matrix_Print(MatM);

	/* Calculate coefficients for each cubic curve */
	for(k=0; k<n; k++) {
		a[k]=pxy[k].y;
		b[k]=(pxy[k+1].y-pxy[k].y)/h[k] -h[k]*MatM->pmat[k]/3.0 -h[k]*MatM->pmat[k+1]/6.0;
		c[k]=MatM->pmat[k]/2.0;
		d[k]=(MatM->pmat[k+1]-MatM->pmat[k])/6.0/h[k];
	}


        /* turn on FBDEV pixcolor and pixalpha_hold */
	save_pixcolor_on=fbdev->pixcolor_on;
        fbdev->pixcolor_on=true;
        fbdev->pixcolor=color;
        fbdev->pixalpha_hold=true;
        fbdev->pixalpha=alpha;

	/* Draw n segements of curves: Notice pxy[] is INT type! */
	for(k=0; k<n; k++) {
//		printf("k=%d\n",k); //getchar();
		/* Step */
		if(pxy[k].x <  pxy[k+1].x) step=1;
		else step=-1;

		/* Draw a curve/segment with many short lines: pxy[k]->pxy[k+1], step +/-1 */
		for( ptx=pxy[k].x; 	ptx != pxy[k+1].x; 	ptx +=step) {   /* NOTICE: All integer type! */
			/* Get two ends of a short line */
			if(xs==0 && xe==0) { /* Token as the first line of the [k]th segment */
				xs=ptx; 	dx=0;	/* dx = (x-xi) */
				ys=a[k]+b[k]*dx+c[k]*pow(dx,2)+d[k]*pow(dx,3);

				xe=xs+step;	dx+=step;
				ye=a[k]+b[k]*dx+c[k]*pow(dx,2)+d[k]*pow(dx,3);
			} else {	    /* Not first line */
				xs=xe; ys=ye;
				xe=xs+step;	dx+=step;
				ye=a[k]+b[k]*dx+c[k]*pow(dx,2)+d[k]*pow(dx,3);
			}

			/* Draw short line */
			//draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w);

			/* Draw areas */
			float tekxx=xe-xs;
        		float tekyy=ye-ys;
			int ky;
			float tek;

			if(tekxx==0)
				tek=1000000;
			else
				tek=tekyy/tekxx;

			fbset_color2(fbdev,color);
			for(kn=roundf(xs); kn<=roundf(xe); kn++) {
				ky=roundf(1.0*ys+(1.0*kn-xs)*tek);
				draw_line(fbdev, kn, ky, kn, baseY);
			}

		}

		/* reste xs,xe as for token */
		xs=0; xe=0;
	}

        /* turn off FBDEV pixcolor and pixalpha_hold */
      	fbdev->pixcolor_on=save_pixcolor_on;
        fbdev->pixalpha_hold=false;
        fbdev->pixalpha=255;

END_FUNC:
	/* free */
	free(h);
	free(a);
	release_float_Matrix(&MatM);
	release_float_Matrix(&MatAB);

	return ret;
}


/*--------------------------------------------------------------------
Draw a parametric cubic spline passing through given points.
Reference: https://blog.csdn.net/qq_35685675/article/details/105852749

Parametric equation:
   P(x/y)= s(t)=A*t^3+B*t^2+Ct+D
   Where:
   	  A=2*(P(i)-P(i+1))/t(i)^3 + (P(i+1)'+P(i)')/t(i)^2
	  B=3*(P(i+1)-P(i))/t(i)^2 - (2*P(i)'+P(i+1)')/t(i)
	  C=P(i)'
	  D=P(i)	i=0,1,2...np-2 ( OR 0,1,...n-1)

   So the target is to calculate P(i)' and solve s(t).

Note:
                        --- !!! IMPORTANT !!! ---
1. Changing a control point will affect the whole curve.

@np:		Number of input points.
@pxy:		Input points
@endtype:	Boundary type:
		1--- Clamped,   (0)Else--- Free.
@w:		Width of line.

Return:
	0	OK
	<0	Fails
Midas Zhou
--------------------------------------------------------------------*/
int draw_spline2(FBDEV *fbdev, int np, EGI_POINT *pxy, int endtype, unsigned int w)
{
	int ret=0;
	int i,k;
      	int n=np-1;              /* segments of curves */
	int *P;			/* P, point.x[] or point.y[] value */

	float *t=NULL;		/* chord lengths */
	//float *u=NULL;	/* param u, as accumulated chord length. NO USE! */

	/* ---For: matrix equation */
        float *a=NULL;          /* a(i)=m(i,i-1)=t(i)  i=1,..np-1 */
	float *c=NULL;		/* c(i)=t(i-1)*t(i)*( 3*(P(i+1)-P(i))/t(i)^2 + 3*(P(i)-P(i-1))/t(i-1)^2 ) i=1,..,np-2  */
        float *v=NULL;		/* v(i)=c(i)/l(i), i=0, np-2  */
        float *l=NULL;		/* l(0)=m11; l(i+1)=m(i+1,i+1)-v(i)*a(i+1) i=1,..np-2 */
	float *y=NULL;		/* Matrix middle solutions */
	float *x=NULL;		/* Matrix final solution, as derivative P(i)' */

	/* Middle var m(): m(i,i+1)=t(i-1), m(i,i)=2(t(i)+t(i-1)), m(i,i-1)=t(i)  i=1,...np-2 	*/

	/* Boundary params */
	float m[4]={0};		/* m[0]-m[3] as for: m(0,0), m(0,1), m(np-1,np-2), m(np-1,np-1) respectively */

	/* ---For: s(t)=A*t^3+B*t^2+C*t+D   A_D[0]-->x(t)  A_D[1]-->y(t) */
	float (*A)[2]=NULL;
	float (*B)[2]=NULL,(*C)[2]=NULL,(*D)[2]=NULL;		/* To allocate together with A */
	int size_A=2*sizeof(float);

	/* ---For: draw short lines   */
	float s;			/* to substitue t */
	float st=0;			/* st=s+step */
	float step;
	float xs=0,ys=0,xe=0,ye=0;      /* start,end x,y */

	float tmp3, tmp2;

	/* check np */
	if(np<2)
		return -1;

        /* Calloc P */
        P=calloc(np, sizeof(typeof(*P)));
        if(P==NULL) {
                fprintf(stderr,"%s: Fail to calloc P!\n",__func__);
                ret=-2;
                goto END_FUNC;
        }

        /* Calloc t */
        t=calloc(n, sizeof(float));
        if(t==NULL) {
                fprintf(stderr,"%s: Fail to calloc t!\n",__func__);
                ret=-2;
                goto END_FUNC;
        }

	#if 0 /* Calloc u */
        u=calloc(np, sizeof(float));
        if(u==NULL) {
                fprintf(stderr,"%s: Fail to calloc u!\n",__func__);
                ret=-2;
                goto END_FUNC;
        }
	#endif

        /* Calloc a,c,v,l,y,x */
        a=calloc(6*np, sizeof(float));
        if(a==NULL) {
                fprintf(stderr,"%s: Fail to calloc a,c,v,l,y,x!\n",__func__);
                ret=-2;
                goto END_FUNC;
        }
	c=a+np;
	v=c+np;
	l=v+np;
	y=l+np;
	x=y+np;

        /* Calloc A,B,C,D */
        A=calloc(4*n, size_A);
        if(A==NULL) {
                fprintf(stderr,"%s: Fail to calloc A,B,C,D!\n",__func__);
                ret=-2;
                goto END_FUNC;
        }
	B=A+n;
	C=B+n;
	D=C+n;

	/* Calculate each chord length */
	for(i=0; i<n; i++) {
		t[i]=sqrt( (pxy[i+1].x-pxy[i].x)*(pxy[i+1].x-pxy[i].x) + (pxy[i+1].y-pxy[i].y)*(pxy[i+1].y-pxy[i].y) );
		//printf("t[%d]=%f\n",i,t[i]);
	}

	#if 0 /* Parameter u, as accumulated chord length */
	u[0]=0.0;
	for(i=1; i<np; i++)
		u[i]=u[i-1]+t[i-1];
	#endif

   /* To calculate A,B,C,D for: X(t)/Y(t)=A*t^3+B*t^2+Ct+D : 0-X(t), 1-Y(t) */
   for(k=0; k<2; k++)
   {
	/* P[] .x OR .y */
	for(i=0; i<np; i++) {
		if(k==0)
			P[i]=pxy[i].x;
		else
			P[i]=pxy[i].y;
	}

	/* Boundary condition: m(0,0),m(1,0),m(np-1,np-2),m(np-1,np-1) */
	if(endtype==1) {
		/* Clamped, to input derivatinve value of start/end points */
		/* m(0,0)=m(np-1,np-1)=1 AND m(0,1)=m(np-1,np-2)=0 */
		m[0]=1.0; m[3]=1.0;  m[1]=0; m[2]=0;
		/* c[0]=P(0)' AND c[np-1]=P(np-1)' */
		c[0]=0.0;
		c[np-1]=0.0;
	}
	else {
	   	/* Free type */
		/* m(0,0)=m[0],m(0,1)=m[1], m(np-1,np-2)=m[2], m(np-1,np-1)=m[3]  */
		m[0]=1.0; m[3]=4.0;  m[1]=0.5;  m[2]=2.0;
		//m[0]=1.0; m[3]=0.5;  m[1]=2.0;  m[2]=4.0;
		c[0]=1.5*(P[1]-P[0])/t[0];
		c[np-1]=6*(P[np-1]-P[np-2])/t[np-2];
	}

	/* Check Matrix solution condtion,  MUST be diagonally dominat.  */
	if( !(fabsf(m[0]) > fabsf(m[1])) || !(fabsf(m[3]) > fabsf(m[2])) ) {
		printf("%s: Matrix has NO unique solution!\n",__func__);
		ret=-3;
		goto END_FUNC;
	}

	/* Matrix param: c[], a[];    c[0],c[np-1]--boundary condition */
	for(i=1; i<np-1; i++) {
		/* Matrix param a: a(i)=m(i,i-1)=t(i) */
		a[i]=t[i];

	   	/* Matrix param: c */
		c[i]=t[i-1]*t[i]*( 3*(P[i+1]-P[i])/t[i]/t[i] + 3*(P[i]-P[i-1])/t[i-1]/t[i-1] );
	}

	/* Boundary: a[np-1]=m(np-1,n-2) = m[2],   AND a[0] is useless. */
	a[np-1]=m[2];

	/*  c[0], c[np-1] from boundary  condition */

	/* Matrix param: v,l
         * l(0)=m(0,0); l(i+1)=m(i+1,i+1)-v(i)*a(i+1)=2(t(i+1)+t(i))-v(i)*a(i+1);  i=1,..np-2
	 */
	l[0]=m[0]; /* boundary condition: l[0]=m(0,0), m(0,0)=m[0] */
	v[0]=m[1]/l[0];	  /* v[0]=m(0,1)/l(0)=m[1]/l(0) */
	l[1]=2*(t[1]+t[0])-v[0]*a[1];
	for(i=1;i<np-2; i++) {  /* np-2 for index [i+1]=np-1 */
		v[i]=t[i-1]/l[i]; 	/* l(i)*v(i)=m(i,i+1) u(i)=m(i,i+1)/l(i)=t(i-1)/l(i) */
		l[i+1]=2*(t[i+1]+t[i])-v[i]*a[i+1];
	}
	/* Cal. v[np-2], v[np-1] is useless */
	v[np-2]=c[np-2]/l[np-2];
	/* boundary condition  m(np-1,np-2)=m[2], m(np-1,np-1)==m[3] */
	l[np-1]=m[3]-a[np-1]*v[np-2];   /*  */

	/* ----  Solve Matrix  ----*/
	/* Solution for middle Matrix */
	y[0]=c[0]/l[0];
	for(i=1; i<np; i++)
		y[i]=(c[i]-a[i]*y[i-1])/l[i];
	/* Final solution */
	x[np-1]=y[np-1];
	for(i=np-2; i>=0; i--)
		x[i]=y[i]-v[i]*x[i+1];

	/* Calculate A,B,C,D for: X(t)/Y(t)=A*t^3+B*t^2+Ct+D */
	for(i=0; i<n; i++) {  /* n segement of curves */
		//A[i][k]=2*(P[i]-P[i+1])/t[i]/t[i]/t[i]+(x[i+1]+x[i])/t[i]/t[i];
		//B[i][k]=3*(P[i+1]-P[i])/t[i]/t[i]-(2*x[i]+x[i+1])/t[i];
		tmp2=t[i]*t[i]; tmp3=tmp2*t[i];
		A[i][k]=2*(P[i]-P[i+1])/tmp3+(x[i+1]+x[i])/tmp2;
		B[i][k]=3*(P[i+1]-P[i])/tmp2-(2*x[i]+x[i+1])/t[i];
		C[i][k]=x[i];  /* x[i]=P'[i] */
		D[i][k]=P[i];

		//printf("A[%d][%d]=%f, B[%d][%d]=%f, C[%d][%d]=%f, D[%d][%d]=%f\n",
		//		i,k,A[i][k], i,k,B[i][k], i,k,C[i][k], i,k,D[i][k]);
	}

    } /* for(k) */

	/* ----- Draw N segments of curve ---------
	 * X(t)=A[0]*s^3+B[0]*s^2+C[0]*s+D[0]
	 * Y(t)=A[1]*s^3+B[1]*s^2+C[1]*s+D[1]
	 -----------------------------------------*/
	step=0.5; /* step of t: [0 t(i)].  MAX. 1 pixel */
	for(i=0; i<n; i++) {
		for(s=0; s < t[i]; s += step ) { /* t step=1.0 */
			if(s==0) {
				xs=D[i][0];
				ys=D[i][1];
				st=s+step;
				//xe=A[i][0]*st*st*st+B[i][0]*st*st+C[i][0]*st+D[i][0];
				//ye=A[i][1]*st*st*st+B[i][1]*st*st+C[i][1]*st+D[i][1];
				/* TODO: Horner's Method to evaluate polynomials */
				tmp2=st*st; tmp3=tmp2*st;
				xe=A[i][0]*tmp3+B[i][0]*tmp2+C[i][0]*st+D[i][0];
				ye=A[i][1]*tmp3+B[i][1]*tmp2+C[i][1]*st+D[i][1];
			} else {
				xs=xe; ys=ye;
				st += step;
				//xe=A[i][0]*st*st*st+B[i][0]*st*st+C[i][0]*st+D[i][0];
				//ye=A[i][1]*st*st*st+B[i][1]*st*st+C[i][1]*st+D[i][1];
				tmp2=st*st; tmp3=tmp2*st;
				xe=A[i][0]*tmp3+B[i][0]*tmp2+C[i][0]*st+D[i][0];
				ye=A[i][1]*tmp3+B[i][1]*tmp2+C[i][1]*st+D[i][1];
			}
			//printf("xs,ys: (%f,%f)  xe,ye: (%f,%f)\n",xs,ys,xe,ye);

                        /* Draw short line */
                        draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w);
		}
	}


END_FUNC:
	/* Free */
	free(P); free(t);
	//free(u);
	free(a); /*  a,c,v,l,y,x */
	free(A); /* A,B,C,D */

	return ret;
}


/*---------------------------------------------------------------------
Draw a Bezier curve.

Parametric equation:
   VP(u)= SUM{ Bern[i,n](u)*ws[i]*P[i] } / { Bern[i,n](u)*ws[i] }
	   ( i: [0 n], u: [0 1], n+1 points )
   where:
	   Bern[i,n] --- Bernstein polynomial
   	   VP 	     --- X/Y points on the curve
	   P[i]      --- X/Y control points
	   ws[i]     --- Weight value of control points
	   n+1       --- Number of control points

Note:
                        --- !!! IMPORTANT !!! ---
1. Changing a control point will affect the whole curve.
2. TODO: To callocate berns only ONCE!?? For real time editing.
3. With changing of curvatures at certain slope, it will appear ugly
   coarse shape, change ustep to a little value to improve it.


TODO:
1. How to deciding ustep, and different ustep for different segments.
2. Instead of calling  draw_wline(), To draw the solid width with
   curve data will be more efficient.

@np:            Number of input pxy[].
@pxy:           Control points
@ws		Weights of control points
@w:             Width of line.

Return:
        0       OK
        <0      Fails

Midas Zhou
------------------------------------------------------------------------*/
int draw_bezier_curve(FBDEV *fbdev, int np, EGI_POINT *pxy, float *ws, unsigned int w)
{
	double *berns=NULL;  	/* For bernstein polynomials  */
	int i;
	float 	chlen;
	float 	u;		/* Independent variable [0 1] */
	float 	ustep;
	int 	n=np-1;
	float 	xs=0,ys=0,xe=0,ye=0;	/* start,end x,y */
	float 	sumw=0.0;		/* sumw=SUM{ Bern[i,n](u)*ws[i] } */

	/* Check np */
	if( np<2 || pxy==NULL)
		return -1;

        /* Calloc berns */
        berns=calloc(np, sizeof(double));
      	if(berns==NULL) {
        	fprintf(stderr,"%s: Fail to calloc berns!\n",__func__);
                return -2;
        }

        /* Calculate each chord length */
	chlen=0.0;
        for(i=0; i<n; i++)
                chlen += sqrt( (pxy[i+1].x-pxy[i].x)*(pxy[i+1].x-pxy[i].x) + (pxy[i+1].y-pxy[i].y)*(pxy[i+1].y-pxy[i].y) );

	/* Estimate u step */
	ustep=1.0/chlen;  /* 0.5  more smooth */

	/* Cal. VPs and draw lines */
	xs=pxy[0].x;  ys=pxy[0].y;
	for(u=0.0+ustep; u<1.0; u += ustep)
	{
		/* u limit */
		if(u>1.0)u=1.0;

		/* Calculate bernstein polynomials */
		if( mat_bernstein_polynomials(np, u, berns)==NULL ) {
                        fprintf(stderr,"%s: Fail to calculate Bernstein polynomials!\n",__func__);
			free(berns);
			return -3;
		}

		/* IF: input weight values */
		if( ws != NULL )
	   	{
			/* Cal.  sumw=SUM{ Bern[i,n](u)*ws[i] } */
			sumw=0.0;
			for(i=0; i<np; i++) {
				sumw += berns[i]*ws[i];
			}

			/* Calculate corresponding (x,y) as per the current u value  */
			xe=0; ye=0;
			for(i=0; i<np; i++) {
				xe += berns[i]*ws[i]*pxy[i].x;
				ye += berns[i]*ws[i]*pxy[i].y;
			}
			/* common denominator */
			xe /= sumw;
			ye /= sumw;
		}
		/* Else: no weight values */
		else {
			/* Calculate corresponding (x,y) as per the current u value  */
			xe=0; ye=0;
			for(i=0; i<np; i++) {
				xe += berns[i]*pxy[i].x;
				ye += berns[i]*pxy[i].y;
			}
		}

		/* Draw short line */
                draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w);
                //float_draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w, false);
		xs=xe; ys=ye;
	}

	/* Free */
	free(berns);

	return 0;
}


/*-------------------------------------------------------------------------------------
Draw a B-spline curve.
Reference: https://pages.mtu.edu/~shene/COURSES/cs3621/NOTES

Parametric equation:
   VP(u)= SUM{ N[i,p](u)*ws[i]*P[i] }
	   ( Range: i [0 n], u [0 1], n+1 points )
   where:
	   U		--- knot vector {u0,u1,...um}, where u0<=u1<=...<=um.
	   N[i,p] 	--- B-spline basis function of degree p
   	   VP(u) 	--- parametric X/Y points on the curve.
	   P[i]  	--- X/Y control points
	   ws[i] 	--- Weight value of control points
	   n+1   	--- Number of control points

Note:
1. B-spline curve is a picewise curve with each component a curve of degree deg.
2. Knot vector are clamped type:
   2.1 The first and last knots have multiplicity of deg+1 of 0/1,
       as vu[]= {0,..,0,0, u1,u2,...uk, 1,...,1,1}.
   2.2 AND knot vector dimension is np+deg+1.
   2.3 For all open curves, include clamped type, the domain is [ u[deg], u[m-deg] ]

			--- !!! IMPORTANT !!! ---
3. Changing a control point P[i] only affect VP(u) on interval { vu[i]..vu[i+deg+1] }.

TODO:
1. How to deciding ustep, and different ustep for different segments.
2. Instead of calling  draw_wline(), To draw the solid width with
   curve data will be more efficient.

@np:            Number of input pxy[].
@pxy:           Control points
@ws:		Weights of control points
		If NULL, ignore as all equal 1.
		Else as NURBS.
@deg:		Degreen number, also as continuity.
		1 --C0 continuity, 2--C1 continuity, 3--C2 continuity, ....
//		uv[] dimension: np+deg+1
@w:             Width of line.

Return:
        0       OK
        <0      Fails

Midas Zhou
-------------------------------------------------------------------------------------*/
#define BSPLINE_CLAMPED_TYPE 	1   /* 1 -- clamped typ, 0 -- open type */
int draw_Bspline(FBDEV *fbdev, int np, EGI_POINT *pxy, float *ws, int deg, unsigned int w)
{
	int i,j;
	int k;
	float	*chord=NULL;		/* each chord length */
	float 	chlen;			/* Total length of all chord */
	float   *vu=NULL;		/* knot vector */
	int	mp;			/* Dimension of vu, for a clamped Bspline:
					 * Dimension is np+deg+1. the first and last knots have multiplicity of deg+1
					 * Knot vector in form of {0,..,0,0, u1,u2,...uk, 1,...,1,1}, u limits to [0 1]
					 */
	int	m;			/* m=mp-1 */
	float	*LN=NULL;		/* Dimension: deg+1 */
	float 	u;			/* Interpolation vu */
//	float   tmp;
	float 	ustep;
	int 	n=np-1;
	float 	xs=0,ys=0,xe=0,ye=0;	/* start,end x,y */
	float 	sumw=0.0;		/* sumw=SUM{ LN[i,n](u)*ws[i] } */
	bool	first_point;

	/* Check input  */
	if( np<2 || pxy==NULL || deg<0 )
		return -1;
	if( np < deg+1 )  /* At lease np=deg+1 */
		return -1;

	/* Get mp,m */
	mp=np+deg+1; m=mp-1;

        /* Calloc chord */
        chord=calloc(np-1, sizeof(typeof(*chord)));
      	if(chord==NULL) {
        	fprintf(stderr,"%s: Fail to calloc chord!\n",__func__);
                return -2;
        }

        /* Calloc vu: number of knots: np+deg+1 */
        vu=calloc(np+deg+1, sizeof(typeof(*vu)));
      	if(vu==NULL) {
        	fprintf(stderr,"%s: Fail to calloc vu!\n",__func__);
		free(chord);
                return -3;
        }

        /* Calloc LN, as for nonzero basis N polynomial */
        LN=calloc(deg+1, sizeof(typeof(*LN)));
      	if(LN==NULL) {
        	fprintf(stderr,"%s: Fail to calloc LN!\n",__func__);
		free(chord);
		free(vu);
                return -3;
        }

        /* Calculate each chord length */
	chlen=0.0;
        for(i=0; i<n; i++) {
                chord[i] = sqrt( (pxy[i+1].x-pxy[i].x)*(pxy[i+1].x-pxy[i].x) + (pxy[i+1].y-pxy[i].y)*(pxy[i+1].y-pxy[i].y) );
		chlen += chord[i];
	}

	/* Estimate u step */
#ifdef LETS_NOTE
	ustep=0.2/chlen;
#else
	ustep=1.0/chlen;  /* 0.25  more smooth */
#endif
	/* Create clamped knot vector vu[]: The first and last knots have multiplicity of deg+1 */
#if BSPLINE_CLAMPED_TYPE /* Clamped type */
	/* 1. Head: vu[0] -> uv[deg]  all 0, totally deg+1 elements */
	for(i=0; i<deg+1; i++) {
		vu[i]=0.75*ustep;
	}
	/* 2. Mid: uv[deg]=0.0 -> uv[np]=1.0 uniformly distributed, totally np-deg+1 elements, uv[np] is redundant as in 3.  */
	for(i=deg; i<=np; i++) {
		vu[i] = 1.0/(np-deg)*(i-deg);
	}
	/* 3. Tail: vu[np] -> uv[np+deg] all 1.0, totally deg+1 elements */
	for(i=np; i<np+deg+1; i++) {
		vu[i]=1.0;
	}

#else /* Open type (NOT clamped) type */
	for(i=0; i<mp; i++) {
		vu[i] = 1.0/mp*i;
	}
#endif

/* TEST --- */
        #if 0
	for(i=0; i<mp; i++)
		printf("vu[%d]=%f\n",vu[i]);
	#endif

        xs=pxy[0].x;  ys=pxy[0].y;
	k=0;  /* init knot span index */
	first_point=true;
	/* Note: for open curve(include clamped type(, the domain is [ u[deg], u[m-deg] ], so u start from 0.0+ustep  */
        for(u=vu[deg]+ustep; u < vu[m-deg+1]; u += ustep)
        {
		/* Limit u */
		//if(u>1.0)u=1.0;
		if(u>vu[m-deg]) break;

		/* To get the knot span index number, Notice u MUST >0, so ensure k-deg>=0 ! */
		for(; k<np+deg+1; k++) {  /* start from previous k! */
			if( u==vu[k+1] )  /* last multiplities */
				break;
			if( u >= vu[k] &&  u< vu[k+1] )
				break; /* k == index number */
		}
		//printf("k=%d\n",k);

		/* IF: input weight values */
		if( ws != NULL )
	   	{
			/* Cal basis functions */
			mat_bspline_basis(k, deg, u, vu, LN); /* For clamped type, all LN[] will be affected. NOT necessary to clear */

			/* Cal. sumw=SUM{ LN[i,n](u)*ws[i] }  */
			sumw=0.0;
			for(j=0; j<=deg; j++) {
				sumw += LN[j]*ws[k-deg+j];
			}

			/* Cal xe,ye, ONLY deg+1 nonzero basis values in LN */
			xe=0.0; ye=0.0;
			for(j=0; j<=deg; j++) {
				xe += LN[j]*ws[k-deg+j]*pxy[k-deg+j].x;
				ye += LN[j]*ws[k-deg+j]*pxy[k-deg+j].y;
			}
			xe /= sumw;
			ye /= sumw;

		}
		/* ws == NULL */
		else {
			/* Cal xe,ye, ONLY deg+1 nonzero basis values in LN */
			mat_bspline_basis(k, deg, u, vu, LN); /* For clamped type, all LN[] will be affected. NOT necessary to clear */
			xe=0.0; ye=0.0;
			for(j=0; j<=deg; j++) {
				xe += LN[j]*pxy[k-deg+j].x;
				ye += LN[j]*pxy[k-deg+j].y;
			}
		}

		/* Draw short line */
		if(!first_point)
                    draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w);
                    //float_draw_wline(fbdev, roundf(xs), roundf(ys), roundf(xe), roundf(ye), w, false);

		/* Make end_point as next start_point */
		xs=xe; ys=ye;
		first_point=false;
	}

	/* Free */
	free(chord);
	free(vu);
	free(LN);

	return 0;
}
