/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef __EGI_INPUT_H__
#define __EGI_INPUT_H__

#include <stdbool.h>
#include <linux/input.h>
#include <stdint.h>

typedef struct egi_keyboard_status {
	uint16_t keycode;
	bool	 press;
} EGI_KBD_STATUS;

/*** NOTE:
 *	1. Pointer memeber is NOT allowed! see ERING_MSG for mouse status.
 */
typedef struct egi_mouse_status {
        pthread_mutex_t mutex;      /* mutex lock */

	/* OR use bitwise to store status:
	 * 	 	!!! WARNING !!!
	 * 	Bitfields may NOT be protable!
	 */
        bool LeftKeyDown	:1;
        bool LeftKeyUp		:1;
        bool LeftKeyDownHold	:1;
        bool LeftKeyUpHold	:1;

        bool RightKeyDown	:1;
        bool RightKeyUp		:1;
        bool RightKeyDownHold	:1;
        bool RightKeyUpHold	:1;

        bool MidKeyDown		:1;
        bool MidKeyUp		:1;
        bool MidKeyDownHold	:1;
        bool MidKeyUpHold	:1;

	bool KeysIdle		:1;	/* All keys are uphold */
	/* MouseIdle: KeysIdle + mouseDX/YZ==0 */

        int  mouseX;	/* Of FB_POS */
        int  mouseY;
        int  mouseZ;

	/* Note: DX,DY,DZ is current increment value, which already added in above mouseX,mouseY,mouseZ
	 * If we use mouseDX/DY to guide the cursor, the cursor will slip away at four sides of LCD, as Limit Value
         * applys for mouseX/Y, while mouseDX/DY do NOT have limits!!!
	 */
	int  mouseDX;
	int  mouseDY;
	int  mouseDZ;

	bool cmd_end_loopread_mouse;   /* Request to end mouse loopread, espacially for mouse_callback function,
					*  which may be trapped in deadlock withou a signal.
					*/

	int  nch;	/* Used bytes in ch[] */
	char chars[32];	/* Input ASCII values.  NOT necessary to be null(0) terminated!  */
	/* TODO: Non_ASCII key values */

	bool request;  /* 0 -- No request, 1-- Mouse event request respond */
} EGI_MOUSE_STATUS;




/***  @inevnt: input event data */
typedef void (* EGI_INEVENT_CALLBACK)(struct input_event *inevent);  		/* input event callback function */

/***  @data: data from mousedev   @len:  data length */
typedef void (* EGI_MOUSE_CALLBACK)(unsigned char *data, int size, EGI_MOUSE_STATUS *mostatus); /* input mouse callback function */

/* For input events */
void 	egi_input_setCallback(EGI_INEVENT_CALLBACK callback);
int 	egi_start_inputread(const char *dev_name);
int 	egi_end_inputread(void);
int 	egi_read_kbdcode(EGI_KBD_STATUS *kbdstat);

/* For mice device */
void 	egi_mouse_setCallback(EGI_MOUSE_CALLBACK callback);
EGI_MOUSE_STATUS*  egi_start_mouseread(const char *dev_name, EGI_MOUSE_CALLBACK callback);
int 	egi_end_mouseread(void);
bool egi_mouse_checkRequest(EGI_MOUSE_STATUS *mostat);
bool egi_mouse_getRequest(EGI_MOUSE_STATUS *mostat);
int  egi_mouse_putRequest(EGI_MOUSE_STATUS *mostat);

/* Terminal IO settings */
void egi_set_termios(void);
void egi_reset_termios(void);

#endif
