/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef __EGI_H__
#define __EGI_H__

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include "sys_list.h"
#include "egi_image.h"
#include "egi_color.h"
#include "egi_filo.h"
//#include <freetype2/ft2build.h>

#define EGI_NOPRIM_COLOR -1 /* Do not draw primer color for an egi object */
#define EGI_TAG_LENGTH 30 /* ebox tag string length */
#define EGI_PAGE_MAXTHREADS 5 /* MAX. number of threads in a page routine job */

typedef struct egi_point_coord  EGI_POINT;
typedef struct egi_box_coords 	EGI_BOX;

typedef struct egi_page 	EGI_PAGE;
typedef struct egi_element_box 	EGI_EBOX;
typedef struct egi_touch_data 	EGI_TOUCH_DATA;
typedef struct egi_data_txt 	EGI_DATA_TXT;
typedef struct egi_data_btn 	EGI_DATA_BTN;
typedef struct egi_data_list 	EGI_DATA_LIST;
typedef struct egi_data_slider 	EGI_DATA_SLIDER;
typedef struct egi_data_pic 	EGI_DATA_PIC;


/* A group of pages that logically connected together that serves for an application.
   For example, One button in page A will bring forward page B,
   and one reaction in page B may trigger to switch over to page C.
*/
struct egi_pggroup {
	struct list_head actpages; /* list head for all activated pages */

	/* TODO:  PAGE switchover functions */

};

struct egi_point_coord
{
         int x;
         int y;
};

struct egi_box_coords
{
        struct egi_point_coord startxy;
        struct egi_point_coord endxy;
};


/* element box type */
enum egi_ebox_type
{
	type_page=1,
	type_txt=1<<1,
	type_btn=1<<2, 	  /* button, resize(enlarge) a movable btn will corrupt bkimg */
	type_list=1<<3,
	type_slider=1<<4, /* sliding bar */
	type_chart=1<<5,
	type_pic=1<<6, 	  /* still picture or motion picture, resize is OK!!! its ebox size is depended 
			    on its pic size.  */
};

/* element box status */
enum egi_ebox_status
{
	status_nobody=0,
	status_sleep,	     /* --- For movalbe EBOX only! ---
			      * WARNING!!! It's dangerous to make any modification to a sleeping EBOX!,
			      * for example: resize, renew egi_data etc.
			      */

	status_hidden,	      /* --- For movable EBOX only! ---
			       *  !!!!  NOW only applied for EBOX_TXT !!!!!
			       * Do NOT display! BUT refresh other data if necessary, size,bkimg ...etc.
			       * !!!We need to refresh bkimg however, when PAGE(wallpaper) updating. egi_txt.c
			       */
	status_active,
	status_page_exiting, /* to inform page runner */
};

/*
 	button shape type
	Not so necessary, you may use bkcolor of symbol to cut
	out the shape you want. unless you want a frame.
*/
enum egi_btn_type
{
	btnType_square=0,  /* default, or rectangle */
	btnType_circle,
};

/*  Slider type */
enum egi_slid_type
{
	slidType_horiz=0,	/* horizontal */
	slidType_vert=1,	/* vertical */
	slidType_circle=2,	/* circle   */
};


/* button touch status */
/* !!! SET str_touch_status[] in egi.c according !!! */
enum egi_touch_status
{
	unkown=0,  		/* during reading or fails */
	releasing=1,   		/* status transforming from pressed_hold to released_hold */
	pressing=2,      	/* status transforming from released_hold to pressed_hold */
	released_hold=3,
	pressed_hold=4,
	db_releasing=5, 	/* double click, the last releasing */
	db_pressing=6, 		/* double click, the last pressing */
	undefined=7,		/* as for limit */
};

/* egi button  return value */
enum egi_retval
{
	pgret_ERR=-2,			/* page routine return with failure */
	btnret_ERR=-1,			/* reation fails */

	btnret_OK=0,			/* trigger normal reaction */
	btnret_IDLE, 			/* trigger no reaction, just bypass btn reaction func */
	btnret_REQUEST_EXIT_PAGE, 	/* return to request the host page to exit */

	pgret_OK,			/* page return ok */

};


/*  ebox action methods */
/*
 if you want add a new method, follow steps:
 	0. add a new method in this structure.
	1. egi.c:
		1.1 write a default method funciton for this new one: egi_ebox_(method_name) (EGI_EBOX *ebox).
		1.2 in egi_ebox_new(): assign default method for this new one.
	2. egi_XXX.c: 	EGI_METHOD XXXbox_method={}
 	3. egi_XXX.c:
		3.1 write an object-defined method function: egi_XXXbox_decorate(EGI_EBOX *ebox).
		3.2 in egi_XXXbox_new(): assign object-defined method to new XXX_type ebox.
 	4. modify relevant head files according.
 */
typedef struct egi_ebox_method EGI_METHOD;
struct egi_ebox_method
{
	int (*activate)(EGI_EBOX *);
	int (*refresh)(EGI_EBOX *);
	int (*decorate)(EGI_EBOX *);
	int (*play)(EGI_EBOX *); 	/* for motion pic ebox, or.. */
	int (*sleep)(EGI_EBOX *);
//TODO	int (*hiden)(EGI_EBOX *);	/* And apply mutex lock if necessary */
	int (*free)(EGI_EBOX *);
	int (*reaction)(EGI_EBOX *, EGI_TOUCH_DATA * touch_data); /* enum egi_touch_status */
};

/*
	-----  structs ebox, is basic element of egi. -----
	1. An abstract model of all egi objects.
	2. Common characteristic and parameters for all types of egi objects.
	3. or a holder for all egi objects.
*/
struct egi_element_box
{
	/* ID */
	unsigned int id; /* MUST >0, 0 as ignored */

	/* ebox type */
	enum egi_ebox_type type;

	/* anything changes that need to carry out refresh method */
	bool need_refresh;

	/*
		--- movable or stationary ---
	If an ebox is immovale, it cann't move or resize. if it does so, then its
	image will be messed up after refeshing.
	*/
	bool movable;

	/* ebox tag
	 * NOTE: If you want to show tag on a btn ebox,make sure that all words be within the box,
	 * any letters outside the box will NOT be refreshed by ebox!
	 */
	char tag[EGI_TAG_LENGTH+1];/* 1byte for /0,  a simple description of the ebox for later debug */
	EGI_16BIT_COLOR	tag_color;

	/* ebox statu */
	enum egi_ebox_status status;

	/* start point coordinate, left top point */
	int x0; //unsigned ??
	int y0; //unsigned ??

	/* for touch area detection
	 * 0. It is drived by x0,y0! touchbox to be updated by new x0y0 in refresh function.
	 * 1. Default/If 0, then use prime area(x0,y0,x0+width,y0+height) for touch detecting
	 * 2. Touchbox MAY or MAY NOT be linked with x0,y0 of a movable ebox.
	 * 3. If 2, touchbox refresh function MUST be realized by each ebox in its egi_xxx.c,
         *    currently only available in type_slider ebox.
         * 4. In a type_slider ebox, it is coupled/linked with ebox->x0,y0, they are concentric.
	 */
	EGI_BOX touchbox;

	/* box size, fit to different type
	 * 1. for txt box, H&W define the holding pad size. and if txt string extend out of
	 *    the ebox pad, it will not be updated/refreshed by the ebox, so keep box size bigger
	 *    enough to hold all text inside.
	 * 2. for button, H&W is same as symbol H&W.
	 */
	int height;
	int width;

	/*  frame type
	 <0: no frame
	 =0: simple type
	 >0: TBD, or see ebox refresh function
	IF >=100:  100+imgframe_type, shape applied!
	*/
	int  frame;

	/*  1. Usually for cutout shape of a ebox frame, also can be a background image.
	 *     Example: We may substitue frame_img->imgbuf, but keep frame_img->alpha!
	 *  2. Activated only frame>100 for ebox. TXT ebox applied
	 *  3. PAGE->ebox->frame_img is also for PAGE wallpaper.
	 *  4. No thread mutex for the img now!!!
	 */
	EGI_IMGBUF* frame_img;
	unsigned char frame_alpha; /* Set alpha value for the frame_img, NOT applied for PAGE now. */

	/* prime box color
	   >=0 normal 16bits color;
	   <0 no prime color, will not draw color. fonts floating on a page,for example.
	  Usually it shall be transparet, except for an egi page, or an floating txtbox?
	 */
	int prmcolor;

	/* pointer to back image
	   1.For a page, it is a wallpaper.??
	   2.For other eboxes, it's an image backup for its holder(page)
           3.For MOVABLE icons/txt boxes, the size should not be too large,!!!
	   It will restore bkimg to fb and copy bkimg from fb everytime before you
	   draw the moving ebox. It's sure not efficient when bkimg is very large, you would
	   rather refresh the whole FB memory.
	   4.Not applicable for an immovable ebox.
	   5. TODO: An alternative solution is to create a background FB buffer, from where image data
	      will be copied to restore the backbground first before an ebox changes/moves.
      */
	uint16_t *bkimg;
	bool bkimg_valid;  /* Only ebox btn is applied now.
			    * 1. To indicate that ebox->bkimg shall copy to or from FB.
			    * 2. If ebox position is out of its container PAGE box, bkimg_valid reset
			    *    to false, see in egi_btnbox_refresh().
			    */

	/* tracking box coordinates for image backup
		to be used in object method	*/
	struct egi_box_coords bkbox;

	/* data pointer to different types of struct egi_data_xxx */
	void *egi_data;

	/* private data, for concept ebox */
	void *prvdata;

	/* child defined method, or use default method as followed */
	EGI_METHOD method;

	/* <<< ------------- DEFAULT METHODS -------------- >>> */
	/*  --- activate:
	   A._for a status_sleep ebox:
	   	1. re-activate(wake up) it:
		   1.1 file offset value of egi_data_txt.foff will be reset.
		   1.2 then refresh() to display it on screen.
		2. reset status.
	   B._for a status_no-body ebox:
	   	1. initialization job for the ebox and ebox->egi_data.
	   	2. malloc bkimg for an movable ebox,and save the backgroud img.
	   	3. refresh to display the ebox on screen.
	   	4. reset the status as active.
	*/
	int (*activate)(EGI_EBOX *);

	/* --- refresh:
		0. a sleep ebox will not be refreshed.
		1. restore backgroud from bkimg and store new position backgroud to bkimg.
		2. update ebox->egi_data and do some job here ---------.
		3. read txt file into egi_data_txt.txt if it applys, file offset of egi_data_txt.foff
		   will be applied and updated.
		4. redraw the ebox and txt content according to updated data.

		5. each type of ebox may have its own refresh method?  type_list ebox, 
	*/
	int (*refresh)(EGI_EBOX *);

	/* --- sleep:
	   1. Remove the ebox image from the screen and restore the bkimg.
	   2. and set status as status_sleep. A sleeping EBOX do NOT refresh()!
	   3. if an immovale ebox sleeps, it should not disappear ??!!!
	   4. A sleeping ebox will not react to touching.
	*/
	int (*sleep)(EGI_EBOX *);

	/* --- hide:
	   1. Remove the ebox image from the screen and restore the bkimg.
	   2. and set status as status_hidden. A hidden EBOX only refresh/update its data, NOT image!
	   3. If an immovale ebox sleeps, it should not disappear ??!!!
	   4. A hidden ebox will not react to touching.
	*/
	int (*hide)(EGI_EBOX *);

	/* --- free:
	*/
	int (*free)(EGI_EBOX *);

	/* reaction to touch_data
	 * Several types of touch status can trigger page routine to call a button(ebox) reaction,
	 * so always check touch_data at the begin of the reaction function codes, to see if it's the
	 * expected touch_status for the button(ebox).
	 */
	int (*reaction)(EGI_EBOX *, EGI_TOUCH_DATA * );

	/* --- decorate:
	    additional drawing/imgs decoration function for the ebox
	*/
	int (*decorate)(EGI_EBOX *);

	struct list_head node; /* list node to a father ebox */

	/* its PAGE container, an EGI_PAGE usually */
	EGI_PAGE *container;   /* May be NULL for an ebox! */

	/* the father ebox */
	EGI_EBOX *father;
};


/* egi touch data structure */
struct egi_touch_data
{
        /* need semaphore lock ?????
        TODO:         */

        /* flag, whether the data is updated after read out */
        bool updated;

        /* the last touch status */
        enum egi_touch_status   status;

        /* the latest touched point coordinate */
        struct egi_point_coord coord;

        /* the sliding deviation of coordXY from the beginnig touch point,
        in LCD coordinate */
        int     dx;
        int     dy;
};


/* egi data for a txt type ebox
 * Note: well, you may use symbol_strings_writeFB() to display txt,
	 but it cann't set/renew color/font independently for each line.
 */
//typedef struct egi_data_txt EGI_DATA_TXT;
struct egi_data_txt
{
	unsigned int id; /* unique id number for txt, MUST>0, default 0 for ignored  */
	int offx; 	/* offset from ebox x0,y0 */
	int offy;
	uint16_t color; /* txt color */
	int nl;  	/* number of txt lines, make it as big as possible?? */


	/* for ASCII alphabet txt */
	struct symbol_page *font;
	char **txt; 	/*multiline txt data, will be memallocated by egi_txtdata_new()  */
	int llen; 	/* in byte, number of chars for each line. The total pixel number
	   	   	 * of those chars should not exceeds ebox->width, ---xxx-- or it may display in
			 * a roll-back way.
	 	   	 */

	/* for FreeType uft-8 encoding txt */
	FT_Face font_face; /* A pointer already */
	unsigned char *utxt;	/* txt in uft-8, !!! will NOT be memallocated by egi_utxtdata_new()!!!
				 * The EBOX will NOT take care of its allocation/free operation!!!
				 */
	int pixpl;	/* in pixels, pixels per line, length of a line. */
	int fw;		/* nominal font width in pixels, including min. horizontal gaps between wchars. */
	int fh;		/* nominal font height in pixels, including min. vertical gaps between lines. */
	int gap;	/* adjusting gap between lines */

	/* For txt file operaton */
	char *fpath; 	/* txt file path if applys */
	long foff; 	/* current seek position of the txt file.
			 * sizeof(long) is the time_size of filo_off
			 */
	long count;     /* count of chars loaded to txt[] for displaying */
	int forward; 	/* flag, read file forward/backward when refresh egi_txt
		      	 * >0 Read Forward (as default)
			 * =0 STOP
		      	 * <0 Read Backward
		      	 */
	EGI_FILO *filo_off; /* a FILO buff for push/pop offset position for txt file
			     *
			     */
};


/* egi data for a botton type ebox
 * !!! a slider type ebox also has egi_data->egi_data_btn !!!
*/
//typedef struct egi_data_btn EGI_DATA_BTN;

struct egi_data_btn
{
	//char tag[32]; 	  /* short description of the button */
	unsigned int id; 	  /* unique id number for btn, MUST >0, default 0 as for ignored
				   *
				   */
	enum egi_btn_type shape;  /* button shape type, square or circle */
	struct symbol_page *icon; /* button icon */
	uint32_t icon_code; 	  /* SYM_SUB_COLOR(16)+CODE(16) code number of the symbol in the symbol_page */
	struct symbol_page *font; /* button tag font */
//	uint16_t font_color; 	use ebox->tag_color instead  /* tag font color, defaul is black */
	int opaque; 		  /* opaque value for the icon, <0 no effect, 0-255 as alpha value */
	enum egi_touch_status status; /* ??? button status, pressed or released */
	bool showtag;             /* to show tag on button or not, default FALSE, */

	/* Move touch_effect() to concept EBOX ???? */
	void (*touch_effect)(EGI_EBOX *, EGI_TOUCH_DATA *);
                                  /*  1. If not NULL, to be called when the btn is touched in page routine,
				   *     depends on touch status.
				   *  2. Default set in egi_btn.c, or to be re-define later.
				   *  3. The ebox must be movable, if touch_effect() is applied.
				   */

	void *prvdata;		  /* private data for derivative ebox, slider etc. */
};

/* egi data for a list type ebox */
//typedef struct egi_data_list EGI_DATA_LIST;
struct egi_data_list
{
        /* total number of items in a list, part of them may be shown in the displaying_window */
        int inum;

	/* the displaying_window size, or number of items displayed in the ebox.*/
	int nwin;

	/* item index for the starting item in displaying_window
	  diplaying item index: pw, pw+1, pw+2,....pw+nwin-1 */
	int pw;

        /* a list of type_txt ebox
	  each one is for one item of the list
	*/
        EGI_EBOX **txt_boxes;

	/*
          sympg icon for each list item
          NULL means no icon
        */
        struct symbol_page **icons;
        int *icon_code;

        /* ----- for all icons -------
	offset of icon from each item ebox */
        int iconoffx;
        int iconoffy;

        /*
           color for each txt line in a list item.
	  to be decided on egi_data_txt .....
	*/

	/* refresh motion type
	   0:	no motion
	   1:	sliding from left
	*/
	int motion;
};


/* egi data for a slider type ebox
  0.  As a slider, the egi_data_slider is referred by egi_data_btn->prvdata.
      and this data will NOT be allocated/created explicitly, usually it will be
      created in EGI_DATA_BTN *egi_sliderdata_new(), as private data of egi_data_btn.
  1.  Adjusting range to be  0 - maxval;
  2.  The hosting ebox's W/H defines the MAX. outline of a sliding bar,
      It covers/surrounds all geometries, it's all the limit size for the slot.
      Usually the Height of hosting ebox and slider to be the same.

  3.  The egi_data_slider.slider  defines the the sliding button on the bar.
      It MAY have reaction methods for EGI_TOUCH_DATA.

  4.  The geometry of the slot bar and/or the slider will be replaced by icons,
      if they'are defined as so.
*/
//typedef struct egi_data_slider EGI_DATA_SLIDER;
struct egi_data_slider
{
	/* starting end point of a slider axis line.
	 *  For Horizontal type, start point is the left end point, with smaller X value.
	 *  For Vertical type, start point is the lower end point, with bigger Y value,
	 */
	EGI_POINT sxy;

	/* position/shape type of the sliding bar */
	enum egi_slid_type ptype;    /*** NOW only 2 types applied, H and V.
				      * 0 -- Horizontal,
				      * 1 -- Vertical,
				      * 2 -- Circle
				      */

	/* width and length of a bar slot */
	int	sw;  /* at least>0 */
	int	sl;

	/* indicating value, part of ls */
	int 	val; /* range: 0-(ls-1) */

	/* color for the valued bar and remainded/void bar */
	EGI_16BIT_COLOR color_valued;
	EGI_16BIT_COLOR color_void;
	EGI_16BIT_COLOR color_slider; /* only if it applys */

	/* icon for the slot bar, if not NULL */
//	struct symbol_page *slot_icon;
//	int slot_code;
	/* bkimg for slot bar, to cover slider and slot image */
	uint16_t *slot_bkimg;
	struct egi_box_coords bkbox;

	/* Icon for the slider block:
	 * if ebox->prmcolor <0 && data_btn->icon ==NULL
	 *  then it will draw D=3*sw circle as for the slider
	 */

};

/*
  egi data for a picture displaying ebox
   also for a motion picture(movie)
  1. first priority: imgbuf, if imgbuf is not NULL.
  2. then fpath file: load jpg file to imgbuf, if fpath is not NULL.
  3. method refresh() only reload imgbuf, not fpath file.
  4. use egi_picbox_loadfjpg() to load/reload jpg file to imgbuf.
     It also frees and reallocates memory for imgbuf according to pic size.

*/
//typedef struct egi_data_pic EGI_DATA_PIC;
struct egi_data_pic
{
	/* image data for a picture, in R5G6B5 pixel format */
	EGI_IMGBUF *imgbuf;

	/* window origin coordinate relating to the image coord system*/
	int imgpx;
	int imgpy;

	/* offset from host ebox
	   offy will be auto. adjusted according to title font height if applys.
	*/
	int offx; /* for left and right side space between host ebox*/
	int offy; /* for up and down side space between host ebox */

	/* image background/canvas color,  <0 as transparent,  */
	int bkcolor;

	/* title of the picture,
	   1. for only one line!
	 */
	char *title;
	/* font for the title */
	struct symbol_page *font;

	/* file path for a picture if applys */
	char *fpath;
};


/* egi data for a picture type ebox */
struct egi_data_chart
{

};


/* an egi_page takes hold of whole tft-LCD screen */
struct egi_data_page
{
	/* eboxes */
	EGI_EBOX *boxlist;
};


struct egi_page
{
	/* Mutex for PAGE resource access */
	pthread_mutex_t pgmutex;

	/* egi page is based on egi_ebox */
	EGI_EBOX *ebox;   /* NOTE:
			   * 1. Here ebox->need_refresh indicates that the PAGE elements(wallpaper etc.),
			   *    NOT its child eboxes in the list, need to refresh! need_refresh tokens
			   *    in the child eboxes will decide wheather they need refresh or not.
	 		   */

	bool page_update; /* 1. To inform child eboxes that its container PAGE's elements(not children's)
			   *    is just updated, so it needs to update its bkimg(ignore fb_cpyfrom_buf())
			   *      ...etc.
			   *    It is implemented in egi_page_refresh().
			   *
			   *    !!! NOW !!! It synchronizes with page->ebox->need_refresh
			   */

	bool slide_off;	  /* to turn off PAGE sliding detection and disable slide_handler() */

	/* wallpaper for the page, JPG or PNG file.  */
	char *fpath;

	/* image buff, load fpath file image to it. */
//	EGI_IMGBUF *imgbuf;  !!! Apply ebox->frame_img instead  !!!

	/* --- child list:
	 *  maintain a list for all child ebox, there should also be layer information
 	 *  multi_layer operation is applied ???
	 */
	struct list_head list_head; /* list head for child eboxes */

	/* --- !!! page routine function : threads pusher and job pusher ----
         *  1. detect pen_touch and trigger buttons.
	 *  2. refresh page (wallpaper and ebox in list).
	 *  3. Default func is defined in egi_page.c, or to be re_defined.
	 */
	int (*routine)(EGI_PAGE *page);

	/* if a touch sliding handler is defined */
	int (*slide_handler)(EGI_PAGE *page, EGI_TOUCH_DATA *touch_data);

	/* if NOT NULL, always do the miscelaneous job when refresh the page in the routine func */
	int (*page_refresh_misc)(EGI_PAGE *page);

	/* NOTE:
	 *  1. Runners are launched in PAGE routine(),  not in page activate or refresh methods.
	 *     and they will be jointed in egi_page_free().
	 *  2. A runner is a pthread job in a PAGE.
	 *  3. !!!! WARNING !!! There is a time delay between finish_creating_runner and runner_start_running.
	 *  4. A runner function shall avoid sleep inside, or as short as possible, in order to keep a quick
	 *     response to Signal SiSUSPEND etc..
	*/
	pthread_t	threadID[EGI_PAGE_MAXTHREADS];
	void * 	  	(*runner[EGI_PAGE_MAXTHREADS])(EGI_PAGE *page);

	bool 		thread_running[EGI_PAGE_MAXTHREADS];    /* indicating running status
								 * Must be set by the owner thread
								 * others may read it.
								 */
	bool		thread_suspending[EGI_PAGE_MAXTHREADS]; /* indicating suspending status
								 * Must be set by the owner thread
								 * others may read it.
								 */
	bool 		thread_SigSUSPEND[EGI_PAGE_MAXTHREADS]; /* a signal to PAUSE/SUSPEND the thread
								 * MUST be set by other threads.
								 * the owner thread will check it while waiting
								 * for the mutex condition to change.
								 */
	/* TBD&TODO: Use enum runner_signal for more signals????!! */

	/* common mutex/cond for all runners in the PAGE, initilized in routine() */
	pthread_mutex_t runner_mutex;
	pthread_cond_t  runner_cond;
	/* TBD: Whether?/When?/Where? to destroy above mutex and cond variables!?!?! */
};


/* for common ebox */
const char*	egi_str_touch_status(enum egi_touch_status touch_status);
int 		egi_random_max(int max);
void*		egi_alloc_bkimg(EGI_EBOX *ebox, int width, int height);
void*		egi_realloc_bkimg(EGI_EBOX *ebox, int width, int height);
bool		egi_point_inbox(int px,int py, EGI_EBOX *ebox);
int 		egi_get_boxindex(int x,int y, EGI_EBOX *ebox, int num);
EGI_EBOX 	*egi_hit_pagebox(int x, int y, EGI_PAGE *page, enum egi_ebox_type);
void 		egi_ebox_settag(EGI_EBOX *ebox, const char *tag);
enum 		egi_ebox_status egi_get_ebox_status(const EGI_EBOX *ebox);

EGI_EBOX*	egi_ebox_new(enum egi_ebox_type type);//, void *egi_data);

int 	egi_ebox_activate(EGI_EBOX *ebox);
void 	egi_ebox_needrefresh(EGI_EBOX *ebox);
void 	egi_ebox_set_touchbox(EGI_EBOX *ebox, EGI_BOX box);
int 	egi_ebox_refresh(EGI_EBOX *ebox);
int 	egi_ebox_forcerefresh(EGI_EBOX *ebox);
int 	egi_ebox_decorate(EGI_EBOX *ebox);
int 	egi_ebox_sleep(EGI_EBOX *ebox);
int 	egi_ebox_free(EGI_EBOX *ebox);

/* for egi page */
int 	egi_page_disappear(EGI_EBOX *ebox);

#endif
