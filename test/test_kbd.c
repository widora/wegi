/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI read keyboard status.

Note:
1. A keyboard device MAY create more than 1 evdev in /dev/input/.

Journal:
2021-06-12:
	1. Create test_kbd.c
2021-06-21:
	1. Test conkeys and multi_media keys.
2021-07-03:
	1. Test conkeys.abskey and absvalue.

Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <linux/input.h>
#include <stdint.h>

#include "egi_input.h"

/*** Refrence:
struct input_event {
        struct timeval time;
        __u16 type;
        __u16 code;
        __s32 value;
};

struct input_id {
        __u16 bustype;
        __u16 vendor;
        __u16 product;
        __u16 version;
};

*/


int main(void)
{
	int i=0;
	EGI_KBD_STATUS kstat={0};

	/* Init ABS KEY.  Noticed ABS_X ==0!!!  */
	kstat.conkeys.abskey=ABS_MAX;
	printf("Init abskey=ABS_MAX=%d \n", ABS_MAX);

	/* Test struct size */
	printf("Sizeof(pthread_mutex_t)=%d\n", sizeof(pthread_mutex_t));
	printf("Sizeof(struct timeval)=%d\n", sizeof(struct timeval));
	printf("Sizeof(EGI_CONKEY_STATUS)=%d\n", sizeof(EGI_CONKEY_STATUS));
	printf("Sizeof(EGI_KBD_STATUS)=%d\n", sizeof(EGI_KBD_STATUS));
	printf("Sizeof(EGI_MOUSE_STATUS)=%d\n", sizeof(EGI_MOUSE_STATUS));

	while(1) {
		//if(egi_read_kbdcode(&kstat, "/dev/input/event0")==0) {
		if(egi_read_kbdcode(&kstat, NULL)==0) {

		#if 1 /* Print keys */
		       if(kstat.ks) {
				printf(" ------ ks=%d:  ", kstat.ks);
				for(i=0; i<kstat.ks; i++) {
					printf("%s_%u ", kstat.press[i] ? "press":"release", kstat.keycode[i]);
				}
				printf("\n");
			}
		#endif

			/* Function keys */
			if( kstat.conkeys.press_F1 )	printf(" F1 \n");
			if( kstat.conkeys.press_F2 )    printf(" F2 \n");
			if( kstat.conkeys.press_F3 )    printf(" F3 \n");
			if( kstat.conkeys.press_F4 )    printf(" F4 \n");
			if( kstat.conkeys.press_F5 )    printf(" F5 \n");
			if( kstat.conkeys.press_F6 )    printf(" F6 \n");
			if( kstat.conkeys.press_F7 )    printf(" F7 \n");
			if( kstat.conkeys.press_F8 )    printf(" F8 \n");
			if( kstat.conkeys.press_F9 )    printf(" F9 \n");
			if( kstat.conkeys.press_F10 )    printf(" F10 \n");
			if( kstat.conkeys.press_F11 )    printf(" F11 \n");
			if( kstat.conkeys.press_F12 )    printf(" F12 \n");

			/* ALT+TAB, in sequence. */
			if( kstat.conkeys.conkeyseq[0] == CONKEYID_ALT && kstat.conkeys.asciikey == KEY_TAB )
					printf("ALT+TAB\n");

			/* CTRL+ALT+A  */
#if 1
			if( kstat.conkeys.conkeyseq[0] == CONKEYID_CTRL  && kstat.conkeys.conkeyseq[1] == CONKEYID_ALT
			    && kstat.conkeys.asciikey == KEY_A )
			{
				printf("CTRL+ALT+A\n");
			}
#else
			if( ( kstat.conkeys.press_leftctrl || kstat.conkeys.press_rightctrl )
			    && ( kstat.conkeys.press_leftalt || kstat.conkeys.press_rightalt )
			    && kstat.keycode[0]==KEY_A)
			{
				printf("CTRL+ALT+A\n");
			}
#endif

			/* Multi_media function keys: KEY_VOLUMEUP, KEY_VOLUMEDOWN,KEY_MUTE,KEY_PLAYPAUSE,KEY_BACK,KEY_FORWARD    */
			switch( kstat.conkeys.lastkey ) {
				/* Gamepad keys */
				case KEY_J:  /* Gamepad L(left) */
					printf("%s KEY_J\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_K:  /* Gamepad R(right) */
					printf("%s KEY_K\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_D:  /* Gamepad X */
					printf("%s KEY_D\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_F:  /* Gamepad A */
					printf("%s KEY_F\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_G:  /* Gamepad B */
					printf("%s KEY_G\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_H:  /* Gamepad Y */
					printf("%s KEY_H\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_APOSTROPHE: /* Select */
					printf("%s KEY_APOSTROPHE\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_GRAVE:    /* Start */
					printf("%s KEY_GRAVE\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;

				/* Media control keys */
				case KEY_VOLUMEUP:
					printf("%s KEY_VOLUMEUP\n",kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_VOLUMEDOWN:
					printf("%s KEY_VOLUMEDOWN\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_PLAYPAUSE:
					printf("%s KEY_PLAYPAUSE\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_MUTE:
					printf("%s KEY_MUTE\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_PREVIOUSSONG:
					printf("%s KEY_PREVIOUSSONG\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_NEXTSONG:
					printf("%s KEY_NEXTSONG\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_BACK:
					printf("%s KEY_BACK\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				case KEY_FORWARD:
					printf("%s KEY_FORWARD\n", kstat.conkeys.press_lastkey? "Press":"Release");
					break;
				default:
					break;
			}

			/* ABS keys */
			switch( kstat.conkeys.abskey ) {
				case ABS_X:
					printf("ABS_X: %d\n", (kstat.conkeys.absvalue -0x7F)>>7); /* NOW: abs value [1 -1] and same as FB Coord. */
					break;
				case ABS_Y:
					printf("ABS_Y: %d\n", (kstat.conkeys.absvalue -0x7F)>>7);
					break;
				default:
					break;
			}

			/* Reset lastkey:  Every EV_KEY read ONLY ONCE */
			kstat.conkeys.lastkey =0;	/* Note: KEY_RESERVED ==0 */

			/* Reset abskey: Every EV_ABS read ONLY ONCE */
			kstat.conkeys.abskey =ABS_MAX; /* NOTE: ABS_X ==0!!! */

		}
	} /* End while() */

}



