/*------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

			--- Simple Buttons ---

1. Simple buttons are traditional rectangular buttons, or image icon buttons,
and their positions in the screen are fixed.
2. A simple button has onlyg two status: released or pressed.
3. There may be some illustrating effects on the button to show that status
   is just be transformed.
4. A simple button may be transfigured by adopting different image icons.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------*/
#include "egi_sbtn.h"
#include "egi_fbgeom.h"
#include "egi_FTsymbol.h"


/*--------------------------------------------------
Refresh button
1. Clear button and redraw frame.
2. Rewirte its tag.

@btn:   	A pointer to an EGI_RECTBTN.
@tag:		Tag of the button, or NULL.
@tagcolor:	Color of the tag words.

---------------------------------------------------*/
void egi_sbtn_refresh(EGI_RECTBTN *btn, char *tag)
{
        int pixlen;
        int bith;

        if(btn==NULL)
                return;

        /* Set font height/width */
        if(btn->fw < 4 || btn->fh < 4 ) {
                btn->fw=22; btn->fh=22;
        }

	/* 1. Apply pressedimg */
	if(btn->pressed && btn->pressimg != NULL ) {
		/* Transparent image NOT supported! */
		egi_subimg_writeFB(btn->pressimg, &gv_fb_dev, btn->pressimg_subnum, -1, btn->x0, btn->y0);
	}
	/* 2. Apply releaseimg */
	else if( !btn->pressed && btn->releaseimg != NULL ) {
		/* Transparent image NOT supported! */
		egi_subimg_writeFB(btn->releaseimg, &gv_fb_dev, btn->releaseimg_subnum, -1, btn->x0, btn->y0);
	}
	/* 3. Apply button frame */
	else {
	        /* Always clear before write, to fill interior area. */
        	draw_filled_rect2(&gv_fb_dev, btn->color, btn->x0, btn->y0, btn->x0+btn->width-1, btn->y0+btn->height-1);
	        /* Draw frame ( fbdev, type, x0, y0, width, height, w ) */
        	draw_button_frame( &gv_fb_dev, btn->pressed, btn->color, btn->x0, btn->y0, btn->width, btn->height, btn->sw);
	        /* Write TAG */
        	if(tag!=NULL) {
                	//bith=FTsymbol_get_symheight(egi_sysfonts.bold, tag, btn->fw, btn->fh);
	                pixlen=FTsymbol_uft8strings_pixlen( egi_sysfonts.bold, btn->fw, btn->fh,(const unsigned char *)tag);

        	        /* Write tag */
                	FTsymbol_uft8strings_writeFB(  &gv_fb_dev, egi_sysfonts.bold,                           /* FBdev, fontface */
                                               btn->fw, btn->fh,(const unsigned char *)tag,             /* fw,fh, pstr */
                                               320, 1, 0,                                       /* pixpl, lines, gap */
                                               btn->x0 +btn->offx +(btn->width-pixlen)/2,                   /* x0 */
                                         //btn->y0+(btn->height-btn->fh)/2+(btn->fh-bith)/2 +(btn->pressed?2:0),           /* y0 */
                                         //btn->y0+(btn->height-bith)/2 +(btn->pressed?2:0),           /* y0 */
                                          btn->y0+ +btn->offy +(btn->height-btn->fh)/2 +(btn->pressed?2:0),           /* y0 */
                                               btn->tagcolor, -1, 255,                               /* fontcolor, transcolor,opaque */
                                               NULL, NULL, NULL, NULL);                 	/* int *cnt, int *lnleft, int* penx, int* peny */
 		}
	}

}



