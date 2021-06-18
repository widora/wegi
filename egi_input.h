/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


	----- Concept and Definition -----

CONKEY:    Control keys of SHIFT, CTRL, ALT, SUPER.. etc.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#ifndef __EGI_INPUT_H__
#define __EGI_INPUT_H__

#include <stdbool.h>
#include <linux/input.h>
#include <stdint.h>


/* Redefine name of some keys to replace input.h */
#define KEY_LEFTSUPER	125 //#define KEY_LEFTMETA            125
#define KEY_RIGHTSUPER  126 //#define KEY_RIGHTMETA           126

typedef struct egi_controlkeys_status 	EGI_CONKEY_STATUS;
typedef struct egi_keyboard_status	EGI_KBD_STATUS;
typedef struct egi_mouse_status		EGI_MOUSE_STATUS;

/*** ID number of control_keys as for conkeyseq[?]=KEY_xxx_ID
 *   Cross_check: const char* CONKEY_NAME[]!
 */
enum {
	KEY_NONE_ID	=0,  /* Keep as 0! for EOF also. */
	KEY_CTRL_ID	=1,
	KEY_ALT_ID	=2,
	KEY_SHIFT_ID	=3,
	KEY_SUPER_ID	=4,
	KEY_ASCII_ID	=5,
};
extern const char* CONKEY_NAME[];

/***  Keyboard control_keys status,   EGI_CONKEY_STATUS
 * 1.	 	!!! WARNING !!!
 * 	Bitfields may NOT be portable!
 * 2. They shall NOT be cleared by the user before readkbd, they are recorded status!
 *    only  PRESS/RELEASE KEY_EVs will make them change!
 */
struct egi_controlkeys_status {

	int  nk;			/* Total number of CONKEYs pressed */

     	bool press_leftctrl		: 1; /* press or keep pressed */
	bool press_rightctrl		: 1;
        bool press_leftalt		: 1;
        bool press_rightalt		: 1;
        bool press_leftshift		: 1;
	bool press_rightshift		: 1;
	bool press_leftsuper		: 1;
	bool press_rightsuper		: 1;
//	bool press_pageup		: 1;	/* !!! NOPE: To  TTY Escape Sequence  */
//	bool press_pagedown		: 1;

/* Sequence of combined control keys; [0] the_first_pressed --->[3] the_last_pressed.
 * Note:
 *	1. NOW only 4 conkeys: SHIFT, ALT, CTRL, SUPER(win)
 */
#define  CONKEYSEQ_MAX 	4
	char conkeyseq[CONKEYSEQ_MAX+1];/* +1 for EOF */

//	unsigned int press_funs		: 12; /* Press F1 ~ F12. --- NOPE! arch dependent. */
	bool press_F1			: 1;
	bool press_F2			: 1;
	bool press_F3			: 1;
	bool press_F4			: 1;
	bool press_F5			: 1;
	bool press_F6			: 1;
	bool press_F7			: 1;
	bool press_F8			: 1;
	bool press_F9			: 1;
	bool press_F10			: 1;
	bool press_F11			: 1;
	bool press_F12			: 1;
};

/*  Keyboard status,  EGI_KBD_STATUS  */
struct egi_keyboard_status {

	/*** Lastest key status as buffered
	 *  1.They will be cleared before each reading keyboard. See in egi_read_kbdcode().
	 *  2. Remind to cross check type and size in egi_read_kbdcode().
	 */
#define KBD_STATUS_BUFSIZE	4
	int	 		ks;	/* Total number of key events (press OR release) got in one read of KBD dev
					 * (each call of egi_read_kbdcode()), results are buffered in following arrays.
					 */
	struct timeval 		tmval[KBD_STATUS_BUFSIZE];
	uint16_t 		keycode[KBD_STATUS_BUFSIZE];
	bool	 		press[KBD_STATUS_BUFSIZE];

	/*** Control key status.
	 *   1. They user MUST NOT clear following date before reading keyboard, they are updated ONLY when EV_KEY happens. see egi_read_kbdcode().
	 *   2. They are ONLY recorded status values! Check keycode[0 ...] instead to see which KEYs are currently triggered!
	 */
	EGI_CONKEY_STATUS	conkeys;

        struct timeval tm_leftctrl;	/* Last update time, sizeof(struct timeval)=8Bytes */
        struct timeval tm_rightctrl;
        struct timeval tm_leftalt;
        struct timeval tm_rightalt;
        struct timeval tm_leftshift;
        struct timeval tm_rightshift;
        struct timeval tm_leftsuper;
        struct timeval tm_rightsuper;
//        struct timeval tm_pageup;
//        struct timeval tm_pagedown;

};

/*** NOTE:
 *	1. Pointer memeber is NOT allowed! see ERING_MSG for mouse status.
 */
struct egi_mouse_status {
        pthread_mutex_t mutex;      /* mutex lock, 24bytes */

	/* OR use bitwise to store status:
	 * 	 	!!! WARNING !!!
	 * 	Bitfields may NOT be portable!
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

	EGI_CONKEY_STATUS conkeys;

	bool request;  /* 0 -- No request, 1-- Mouse event request respond */
};


/***  @inevnt: input event data */
typedef void (* EGI_INEVENT_CALLBACK)(struct input_event *inevent);  		/* input event callback function */

/***  @data: data from mousedev   @len:  data length */
typedef void (* EGI_MOUSE_CALLBACK)(unsigned char *data, int size, EGI_MOUSE_STATUS *mostatus); /* input mouse callback function */

/* For input events */
void 	egi_input_setCallback(EGI_INEVENT_CALLBACK callback);
int 	egi_start_inputread(const char *dev_name);
int 	egi_end_inputread(void);
int 	egi_read_kbdcode(EGI_KBD_STATUS *kbdstat, const char *kdev);

/* For mice device */
void 	egi_mouse_setCallback(EGI_MOUSE_CALLBACK callback);
EGI_MOUSE_STATUS*  egi_start_mouseread(const char *dev_name, EGI_MOUSE_CALLBACK callback);
int 	egi_end_mouseread(void);
bool 	egi_mouse_checkRequest(EGI_MOUSE_STATUS *mostat);
bool 	egi_mouse_getRequest(EGI_MOUSE_STATUS *mostat);
int  	egi_mouse_putRequest(EGI_MOUSE_STATUS *mostat);

/* Terminal IO settings */
void 	egi_set_termios(void);
void 	egi_reset_termios(void);

#endif
