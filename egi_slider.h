/*---------------- egi_slider.h--------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

egi slider tyep ebox functions

Midas Zhou
------------------------------------------------------*/
#ifndef __EGI_SLIDER_H__
#define __EGI_SLIDER_H__


#include "egi.h"
#include "egi_symbol.h"

EGI_DATA_BTN *egi_sliderdata_new(
                                /* for btnbox */
                                int id, enum egi_btn_type btn_shape,
                                EGI_SYMPAGE *icon, int icon_code,
                                EGI_SYMPAGE *font,

                                /* for slider */
				enum egi_slid_type   slid_type, /* slider type */
                                EGI_POINT pxy,
                                int swidth, int slen,
                                int value,       /* usually to be 0 */
                                EGI_16BIT_COLOR val_color, EGI_16BIT_COLOR void_color,
                                EGI_16BIT_COLOR slider_color
                            );

EGI_EBOX * egi_slider_new(
        char *tag, /* or NULL to ignore */
        EGI_DATA_BTN *egi_data,
        int width, int height, /* for frame drawing and prmcolor filled_rectangle */
 	int twidth, int theight, /* for touch area,which is coupled with ebox->x0,y0 */
        int frame,
        int prmcolor /* 1. Let <0, it will draw slider, instead of applying gemo or icon.
                        2. prmcolor geom applys only if prmcolor>=0 and egi_data->icon != NULL */
);


int egi_slider_activate(EGI_EBOX *ebox);
int egi_slider_refresh(EGI_EBOX *ebox);
int egi_slider_setpsval(EGI_EBOX *ebox, int psval);
int egi_slider_reset(EGI_EBOX *slider, int psval, int sl, EGI_POINT sxy);
EGI_DATA_SLIDER *egi_slider_getdata(EGI_EBOX *slider);
void egi_free_data_slider(EGI_DATA_BTN *data_btn);


#endif
