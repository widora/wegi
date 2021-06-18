/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test EGI read keyboard status.

Journal:
2021-06-12:
	1. Create test_kbd.c

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

	/* Test struct size */
	printf("Sizeof(pthread_mutex_t)=%d\n", sizeof(pthread_mutex_t));
	printf("Sizeof(struct timeval)=%d\n", sizeof(struct timeval));
	printf("Sizeof(EGI_CONKEY_STATUS)=%d\n", sizeof(EGI_CONKEY_STATUS));
	printf("Sizeof(EGI_KBD_STATUS)=%d\n", sizeof(EGI_KBD_STATUS));
	printf("Sizeof(EGI_MOUSE_STATUS)=%d\n", sizeof(EGI_MOUSE_STATUS));

	while(1) {
		if(egi_read_kbdcode(&kstat, "/dev/input/event0")==0) {

			#if 0 /* Print keys */
			printf(" ------ ks=%d:  ", kstat.ks);
			for(i=0; i<kstat.ks; i++) {
				printf("%s_%u ", kstat.press[i] ? "press":"release", kstat.keycode[i]);
			}
			printf("\n");
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

#if 0			/* ALT+TAB, in sequence. */
			if( kstat.conkeys.press_tab && kstat.conkeys.press_leftalt ) {
					if( timercmp(&kstat.tm_tab, &kstat.tm_leftalt, >) )
						printf("ALT+TAB\n");
					else
						printf("TAB+ALT\n");
			}
#endif

			/* CTRL+ALT+A //DEL */
			if( ( kstat.conkeys.press_leftctrl || kstat.conkeys.press_rightctrl )
			    && ( kstat.conkeys.press_leftalt || kstat.conkeys.press_rightalt )
			    && kstat.keycode[0]==KEY_A)  //KEY_DELETE )
			{
				printf("CTRL+ALT+A\n");
			}

		}
	} /* End while() */

}



