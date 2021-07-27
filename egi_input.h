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
	CONKEYID_NONE   =0,  /* Keep as 0! for EOF also. */
	CONKEYID_CTRL	=1,
	CONKEYID_ALT	=2,
	CONKEYID_SHIFT	=3,
	CONKEYID_SUPER	=4,
	CONKEYID_ASCII	=5,  /* ascii keys, NOT ASCII Codes!!! */
};
extern const char* CONKEY_NAME[];

/***  Keyboard control_keys status,   EGI_CONKEY_STATUS
 * 1.	 	!!! WARNING !!!
 * 	Bitfields may NOT be portable!
 * 2. They shall NOT be cleared by the user before readkbd, they are recorded status!
 *    only  PRESS/RELEASE KEY_EVs will make them change!
 * 3. An ASCII key will be deemed as a conkey ONLY WHEN preceded by a non_ASCII conkey!
 *    and ONLY one ASCII key is allowed in conkey group, as END of a conkey group!
 */
struct egi_controlkeys_status {

	int  nk;			/* Total number of CONKEYs pressed */

#ifndef XXXLETS_NOTE
     	bool press_leftctrl		: 1; 	/* Ture: pressed; 	False: released */
	bool press_rightctrl		: 1;
	bool press_leftalt		: 1;
        bool press_rightalt		: 1;
        bool press_leftshift		: 1;
	bool press_rightshift		: 1;
	bool press_leftsuper		: 1;
	bool press_rightsuper		: 1;
	bool press_asciikey		: 1;
	bool press_lastkey		: 1;  /* TEST: The last key from keyboard input, usually use this as a function key.
					       *  No repeating function required.
					       *  Press statu reset in egi_read_kbdcode().
					       */
	bool press_abskey		: 1;  /* TEST: EV_ABS value returns 0x7F(127) as releases? others as press.
					       *  Repeating function required.
					       *  Press statu need to be reset by Caller!
					       */
#else
     	bool press_leftctrl		; 	/* Ture: pressed; 	False: released */
	bool press_rightctrl		;
	bool press_leftalt		;
        bool press_rightalt		;
        bool press_leftshift		;
	bool press_rightshift		;
	bool press_leftsuper		;
	bool press_rightsuper		;
	bool press_asciikey		;
	bool press_lastkey		;  /* TEST: The last key from keyboard input, usually use this as a function key.
					       *  No repeating function required.
					       *  Press statu reset in egi_read_kbdcode().
					       */
	bool press_abskey		;  /* TEST: EV_ABS value returns 0x7F(127) as releases? others as press.
					       *  Repeating function required.
					       *  Press statu need to be reset by Caller!
					       */
#endif

/* Sequence of combined control keys; [0] the_first_pressed --->[4] the_last_pressed.
 * Note:
 *	1. NOW only 4 conkeys (SHIFT, ALT, CTRL, SUPER(win)) +1 ASCII_conkey.
 *	2. conkeyseq[] is always sorted in time sequence,  at end of egi_read_kbdcode().
 */
#define  CONKEYSEQ_MAX 	5		/* MAX.5:  SHIFT+ALT+CTRL+SUPER  + 1 ASCII_Conkey */
	char conkeyseq[CONKEYSEQ_MAX+1];/* +1 for EOF  */
	unsigned int asciikey;		/* For the ONLY ASCII_conkey, if !=0. EV_KEY */

	unsigned int lastkey;		/* The last key from keyboard input, EV_KEY
					 * 1. Usually use this as a function key when nk==0.
					 * 2. Usually the key shall NOT be EV_REP type.
					 * 3. NOW the SURFMAN will reset press_lastkey to false if same keycode and tm_lastkey as last time.
					 *    so as to ering lastkey press_statu to the surfuser ONLY ONCE.
					 * 4. For the SURFUSER, check press_lastkey==TRUE first, in case lastkey NOT cleared YET!
					 */
	/* ABS type keys.
	 * NOTE: struct input_event {  struct timeval time;  __u16 type;  __u16 code;  __s32 value; }
	 * input.h
		#define ABS_X                   0x00
		#define ABS_Y                   0x01
		#define ABS_MAX			0x3F (63)
		#define ABS_CNT			(ABS_MAX+1)
		//#define FF_MAX         	 0x7F  (Force Feedback Max )
	 */
	char abskey;			/* EV_ABS, ABS key code: ABS_X, ABS_Y, etc. !!! input_event __u16 code;
					 * ABS_MAX --> invalid.  0 --> ABS_X!
					 * EV_KEY: KEY_RESERVED ==0
					 * Repeating function required!
					 */
	int32_t absvalue;		/* ABS value: 0xff, 0x00 etc. !!! input_event __s32 value;
					 * If released, it returns 0x7F ???
					 * GamePad Conversion: (kstat.conkeys.absvalue -0x7F)>>7; then absvalue results in [1 -1].
					 */

	/*** Function keys:
	 * 1. Function key is EV_REP type.
	 */
#ifndef XXXLETS_NOTE
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

#else	/* ------*/
	bool press_F1			;
	bool press_F2			;
	bool press_F3			;
	bool press_F4			;
	bool press_F5			;
	bool press_F6			;
	bool press_F7			;
	bool press_F8			;
	bool press_F9			;
	bool press_F10			;
	bool press_F11			;
	bool press_F12			;
#endif

};

/*  Keyboard status,  EGI_KBD_STATUS  */
struct egi_keyboard_status {

	/*** Lastest key status for one round of select/read input devices.
	 *  1.They will be cleared before each reading keyboard. See in egi_read_kbdcode().
	 *  2. Remind to cross check type and size in egi_read_kbdcode(), Sec.5.
	 */
#define KBD_STATUS_BUFSIZE	8  //4
	int	 		ks;	/* Total number of EV_KEY events (press OR release) got in one read of KBD dev
					 * (each call of egi_read_kbdcode()), results are buffered in following arrays.
					 */
	struct timeval 		tmval[KBD_STATUS_BUFSIZE];
	uint16_t 		keycode[KBD_STATUS_BUFSIZE];  /* For EV_KEY only;   EV_ABS to conkeys.abskey. */
	bool	 		press[KBD_STATUS_BUFSIZE];


//	int			kabs;	/* Total nmber of EV_ABS events */

	/*** Control key status.
	 *   1. The user MUST NOT clear following date before reading keyboard, they are updated ONLY when EV_KEY happens. see egi_read_kbdcode().
	 *   2. They are ONLY recorded status values! Check keycode[0 ...] instead to see which KEYs are currently triggered!
	 */
	EGI_CONKEY_STATUS	conkeys;

	/* Last update time for conkeys, sizeof(struct timeval)=8Bytes */
        struct timeval tm_leftctrl;
        struct timeval tm_rightctrl;
        struct timeval tm_leftalt;
        struct timeval tm_rightalt;
        struct timeval tm_leftshift;
        struct timeval tm_rightshift;
        struct timeval tm_leftsuper;
        struct timeval tm_rightsuper;

	struct timeval tm_lastkey;	/* TEST: use this as a function key. */

};

/*** NOTE:
 *	1. Pointer memeber is NOT allowed! see ERING_MSG for mouse status.
 */
struct egi_mouse_status {
        pthread_mutex_t mutex;      /* mutex lock, 24bytes */

#ifndef XXXLETS_NOTE
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
#else

        bool LeftKeyDown	;
        bool LeftKeyUp		;
        bool LeftKeyDownHold	;
        bool LeftKeyUpHold	;

        bool RightKeyDown	;
        bool RightKeyUp		;
        bool RightKeyDownHold	;
        bool RightKeyUpHold	;

        bool MidKeyDown		;
        bool MidKeyUp		;
        bool MidKeyDownHold	;
        bool MidKeyUpHold	;

	bool KeysIdle		;	/* All keys are uphold */
#endif


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
	/* NOW: From ptty STDIN */
	int  nch;	/* Used bytes in ch[] */
	char chars[32];	/* Input ASCII values.  NOT necessary to be null(0) terminated!  */

	/* NOW: From event dev by egi_read_kbdcode() */
	EGI_CONKEY_STATUS conkeys; /* NOTE: Unlike chars[], conkeys has NO buffer!
				    * If you hit(press/release) a key with very short time, the press_status
				    * MAY miss to SURFUSER, when the SURFUSER is too busy(FLAG_MEVENT NOT cleared) and
				    * SURFMAN will skip/cancel that ERING session.
				    */

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

/* For keyboard device */


/* Terminal IO settings */
void 	egi_set_termios(void);
void 	egi_reset_termios(void);

#endif
