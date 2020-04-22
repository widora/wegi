/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef __EGI_INPUT_H__
#define __EGI_INPUT_H__

#include <linux/input.h>

/***  @inevnt: input event data */
typedef void (* EGI_INEVENT_CALLBACK)(struct input_event *inevent);  		/* input event callback function */

/***  @data: data from mousedev   @len:  data length */
typedef void (* EGI_MOUSE_CALLBACK)(unsigned char *data, int size);  		/* input mouse callback function */

/* For input events */
void 	egi_input_setCallback(EGI_INEVENT_CALLBACK callback);
int 	egi_start_inputread(const char *dev_name);
int 	egi_end_inputread(void);

/* For mice device */
void 	egi_mouse_setCallback(EGI_MOUSE_CALLBACK callback);
int 	egi_start_mouseread(const char *dev_name);
int 	egi_end_mouseread(void);

#endif
