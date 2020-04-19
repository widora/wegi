/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-------------------------------------------------------------------*/
#ifndef __EGI_INPUT_H__
#define __EGI_INPUT_H__

typedef void (* EGI_INEVENT_CALLBACK)(int inevt_type, int inevt_code, int inevt_value);  /* input event callback function */

void 	egi_input_setCallback(EGI_INEVENT_CALLBACK callback);
int 	egi_start_inputread(const char *dev_name);
int 	egi_end_inputread(void);

#endif
