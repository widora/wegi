/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


	   (linux/input.h)
more details: https://www.kernel.org/doc/html/latest/input/input-programming.html

   	--- Event types ---
#define EV_SYN                  0x00
#define EV_KEY                  0x01
#define EV_REL                  0x02  -- Relative value
#define EV_ABS                  0x03  -- Absolute value
#define EV_MSC                  0x04
#define EV_SW                   0x05
#define EV_LED                  0x11
#define EV_SND                  0x12
#define EV_REP                  0x14
#define EV_FF                   0x15
#define EV_PWR                  0x16
#define EV_FF_STATUS            0x17
#define EV_MAX                  0x1f
#define EV_CNT                  (EV_MAX+1)

   	--- Relative Axes ---
#define REL_X                   0x00
#define REL_Y                   0x01
#define REL_Z                   0x02
#define REL_RX                  0x03
#define REL_RY                  0x04
#define REL_RZ                  0x05
#define REL_HWHEEL              0x06
#define REL_DIAL                0x07
#define REL_WHEEL               0x08
#define REL_MISC                0x09
#define REL_MAX                 0x0f
#define REL_CNT                 (REL_MAX+1)

	--- Button Value ---
...
#define BTN_MOUSE               0x110
#define BTN_LEFT                0x110
#define BTN_RIGHT               0x111
#define BTN_MIDDLE              0x112
#define BTN_SIDE                0x113
#define BTN_EXTRA               0x114
#define BTN_FORWARD             0x115
#define BTN_BACK                0x116
#define BTN_TASK                0x117


(With reference to:  https://wiki.osdev.org/PS/2_Mouse )

				--- 1. PS2 DATA ---

		Bit7	  Bit6	    Bit5     Bit4    Bit3    Bit2      Bit1 	 Bit0
	--------------------------------------------------------------------------------
Byte 1:	    Y_overflow	X_overflow  Y_sign  X_sign    1	    Mid_Btn  Right_Btn	Left_Btn
Byte 2:					X_Movement
Byte 3:					Y_Movement


				--- 2. Intellimouse #1 ---

		Bit7	  Bit6	    Bit5     Bit4    Bit3    Bit2      Bit1 	 Bit0
	--------------------------------------------------------------------------------
Byte 1:	    Y_overflow	X_overflow  Y_sign  X_sign    1	    Mid_Btn  Right_Btn	Left_Btn
Byte 2:					X_Movement
Byte 3:					Y_Movement
Byte 4:					Z_Movement (wheel)  (in 2's complement. valid value are -8 to +7)

SETUP MAGIC:  { 0xf3, 200, 0xf3, 100, 0xf3, 80 }


				--- 3. Intellimouse #2 ---

		Bit7	  Bit6	    Bit5     Bit4    Bit3    Bit2      Bit1 	 Bit0
	--------------------------------------------------------------------------------
Byte 1:	    Y_overflow	X_overflow  Y_sign  X_sign    1	    Mid_Btn  Right_Btn	Left_Btn
Byte 2:					X_Movement
Byte 3:					Y_Movement
Byte 4:	          0        0       5th_Btn  4th_Btn  		Z_Movement(Bit0-3)

SETUP MAGIC:  { 0xf3, 200, 0xf3, 200, 0xf3, 80 }



Journal
2021-03-14:
	1. egi_mouse_loopread(): When select() time out, instead of return and wait,
	   go on to calculate mostatus with sustained data of mouse_data[0] &= 0x0F.
	   So that if the mouse keeps idle after a click (in such case, no data from the
           input event after the click), the loopread can still generate mostatus! For example,
	   to deduce a LeftKeyDownHold status after the click.

	TODO: it continue generating mostatu at idle time.....

2021-03-18:
	1. egi_mouse_loopread(): Adjust mostatus.DX/DY as per mostatus.mouseX/mouseY variation,
	   with consideration of mouse movement limit at four sides.
2021-03-26:
	1. egi_mouse_loopread(): Add var. status_sustain to check if idle DOWNHOLD happens when select() returns 0.
2021-04-03:
	1. egi_mouse_loopread():
		1.A. Reset mostatus.RightKeyDownHold=false if status=0b00.
	 	1.B. Clear mouse_data after set mouse type to Intellimouse.
		1.C. Case 2 0b01: mostatus.RightKeyDownHold=true --modified to--> mostatus.RightKeyDownHold=false
		1.D. Case 2 0b10: mostatus.LeftKeyDownHold=false --modified to-->  mostatus.RightKeyDownHold=false;
2021-05-10:
	XXX 1. egi_mouse_loopread(): Cancel while(checkRequest) {} at end of loop.

TODO: If mouse is disconnected, the mouse_callback() will be hung up.

2021-06-11:
	1. Add EGI_KBD_STATUS
	2. Add egi_read_kbdcode().
2021-06-12:
	1. Add Control_key status in EGI_KBD_STATUS and revise egi_read_kbdcode().
2021-06-15:
	1. Add EGI_CONKEYS_STAUTS, and revise egi_read_kbdcode().

2021-06-17:
	1. egi_read_kbdcode(): Sort kstat->conkeys.conkeyseq[] according to input timeval.
2021-06-18:
	1. egi_read_kbdcode():
	   1.1 Close(fd) will also flush buffer of event, so male fd[] static.
	   1.2 Check integerity of fd[] before ioctl and read.
2021-06-21:
	1. Add member 'asciikey' to EGI_CONKEY_STATUS, as the only ASCII type conkey!
	   and modify egi_read_kbdcode() accordingly.
2021-06-22:
	1. egi_mouse_loopread(): DO NOT STOP loopreading even if the mouse is disconnected,
	   			 replace while(mouse_fd<0){} with if(mouse_fd<0){}.
	2. egi_read_kbdcode(): Call egi_valid_fd() to check/validate set_fds in each round.
2021-06-23:
	1. egi_read_kbdcode():
	   1.1 Add concept and definition.
	   1.2 Clear KBD STATUS when it detects an input device becomes disconnected/invalid! <-----
2021-06-24:
	1.egi_read_kbdcode():  Add ascii_conkey to conkeseq[].
2021-07-03:
	1. Add member 'abskey' and 'absvalue' for EGI_CONKEY_STATUS.
	   Modify egi_read_kbdcode() accordingly.
2021-07-07:
	1. (Itme 11.) If abskey value keeps as IDLE(0x7F) aft. readkbd, reset baskey=ABS_MAX to make it invalid!.
	2. (Item 12.) If press_lastkey keeps FALSE, clear laskey code!
Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------------------------*/
#include "egi_debug.h"
#include "egi_input.h"
#include "egi_timer.h"
#include "egi_fbdev.h"
#include "egi_utils.h"
#include <stdio.h>
#include <string.h>
#include <termio.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <stdbool.h>
#include <linux/input.h>

/* 1. Inpute device: for generic input events */
static int input_fd=-1;
static struct input_event  inevent;
static EGI_INEVENT_CALLBACK  inevent_callback; 	/* To be assigned */
/* ??? mutex lock for inevent_callback */

static bool tok_loopread_event_running;         /* token for loopread is running if true */
static bool cmd_end_loopread_event;            	/* command to end loopread if true */
static pthread_t thread_loopread_event;
static void *egi_input_loopread(void *arg);

#if 0 ///////////////////
static pthread_cond_t   cond_request;   /* To indicate that mouse event request  */
static pthread_mutex_t  mutex_lockCond; /* mutex lock for pthread cond */
static int              flag_cond=-1;   /* predicate for pthread cond, set in egi_touch_loopread()
                                         * flag_cond:  == 0, request=false,  ==1 request=true.
                                         * flag_cond:  == -1, idle.
					 */
#endif/////////////////




/* 2. Input device: for mouse */
#if 1 /* set mouse type to Intellimouse #1 */
static unsigned char setup_data[6]={0xf3,200,0xf3,100,0xf3,80};
#else /* set mouse type to Intellimouse #2 */
static unsigned char setup_data[6]={0xf3,200,0xf3,200,0xf3,80};
#endif

static int mouse_fd=-1;
static unsigned char mouse_data[4];
static EGI_MOUSE_CALLBACK  mouse_callback; 	/* To be assigned */
/* ??? mutex lock for mouse_callback */

static bool tok_loopread_mouse_running;		/* token for loopread is running if true */
static bool cmd_end_loopread_mouse;           	/* command to end loopread if true */
static pthread_t thread_loopread_mouse;
static void *egi_mouse_loopread(void *arg);

/* Mouse status and data */
static EGI_MOUSE_STATUS mostatus={ .LeftKeyUpHold=true, .RightKeyUpHold=true, .MidKeyUpHold=true, .KeysIdle=true, };
/* Keyboard status and data */
static EGI_KBD_STATUS kbdstatus;

/* Conkey ID to its Name */
const char* CONKEY_NAME[]=
{
        [CONKEYID_NONE]   = "None",
        [CONKEYID_CTRL]   = "CTRL",
        [CONKEYID_ALT]    = "ALT",
        [CONKEYID_SHIFT]  = "SHIFT",
        [CONKEYID_SUPER]  = "SUPER",
        [CONKEYID_ASCII]  = "ASCII",
};

/*---------------------------------------------------
Call this function before loopread input device
----------------------------------------------------*/
void egi_input_setCallback(EGI_INEVENT_CALLBACK callback)
{
	inevent_callback=callback;
}


/*-------------------------------
Run inevent loopread in a thread

Return:
	0	OK
	<0	Fails
------------------------------*/
int egi_start_inputread(const char *dev_name)
{

	if(tok_loopread_event_running==true) {
		printf("%s: inevent loopread has been running already!\n",__func__);
		return -1;
	}

	cmd_end_loopread_event=false;

        /* start touch_read thread */
        if( pthread_create(&thread_loopread_event, NULL, (void *)egi_input_loopread, (void *)dev_name) !=0 )
        {
                printf("%s: Fail to create inevent loopread thread!\n", __func__);
                return -1;
        }

        /* reset token */
        tok_loopread_event_running=true;

	return 0;
}


/*-------------------------------
End inevent loopread.

Return:
	0	OK
	<0	Fails
------------------------------*/
int egi_end_inputread(void)
{
	if(tok_loopread_event_running==false) {
		printf("%s: inevent loopread has been ceased already!\n",__func__);
		return -1;
	}

        /* Set indicator to end loopread */
        cmd_end_loopread_event=true;

        /* Wait to join touch_loopread thread */
        if( pthread_join(thread_loopread_event, NULL ) !=0 ) {
                printf("%s:Fail to join thread_loopread_event.\n", __func__);
                return -1;
        }

	/* reset cmd and token */
	cmd_end_loopread_event=false;
	tok_loopread_event_running=false;

	/* close fd */
	close(input_fd);
	input_fd=-1;

	return 0;
}


/*---------------------------------------------------
Call this function before loopread mice/mouseX device
----------------------------------------------------*/
void egi_mouse_setCallback(EGI_MOUSE_CALLBACK callback)
{
	mouse_callback=callback;
}


/*----------------------------------------------
Run loopread mice/mouseX data in a thread.

@dev_name:	Mouse dev name
		NULL, default "/dev/input/mice"

Return:
	0	OK
	<0	Fails
-----------------------------------------------*/
EGI_MOUSE_STATUS* egi_start_mouseread(const char *dev_name, EGI_MOUSE_CALLBACK callback )
{
	if(tok_loopread_mouse_running==true) {
		printf("%s: inevent loopread has been running already!\n",__func__);
		return NULL;
	}

	//cmd_end_loopread_mouse=false;
	mostatus.cmd_end_loopread_mouse=false;

	/* Set callback */
	mouse_callback=callback;

        /* Init mostatus mutex */
        if(pthread_mutex_init(&mostatus.mutex,NULL) != 0)
        {
                printf("%s: Fail to initiate mostatus.mutex.\n",__func__);
                return NULL;
        }

        /* Start loopread_mouse thread */
        if( pthread_create(&thread_loopread_mouse, NULL, (void *)egi_mouse_loopread, (void *)dev_name) !=0 )
        {
                printf("%s: Fail to create mouse loopread thread!\n", __func__);
	        if(pthread_mutex_destroy(&mostatus.mutex) !=0 )
	                printf("%s: Fail to destroy mostatus.mutex!\n",__func__);
                return NULL;
        }

        /* reset token */
        tok_loopread_mouse_running=true;


	return &mostatus;
	//return 0;
}


/*-------------------------------
End inevent loopread.

Return:
	0	OK
	<0	Fails
------------------------------*/
int egi_end_mouseread(void)
{
	if(tok_loopread_mouse_running==false) {
		printf("%s: inevent loopread has been ceased already!\n",__func__);
		return -1;
	}

        /* Set indicator to end loopread */
        //cmd_end_loopread_mouse=true;
	/* Set mostatus, otherwise it may be trapped in mouse_callback. */
	mostatus.cmd_end_loopread_mouse=true;

        /* Wait to join touch_loopread thread */
        if( pthread_join(thread_loopread_mouse, NULL ) !=0 ) {
                printf("%s: Fail to join thread_loopread_event.\n", __func__);
                return -1;
        }

        /* Destroy mostatus mutex */
        if(pthread_mutex_destroy(&mostatus.mutex) !=0 )
                printf("%s: Fail to destroy mostatus.mutex!\n",__func__);

	/* reset cmd and token */
	//cmd_end_loopread_mouse=false;
	mostatus.cmd_end_loopread_mouse=false;
	tok_loopread_mouse_running=false;

	/* close fd */
	close(mouse_fd);
	mouse_fd=-1;

	return 0;
}


/*---------------------------------------------------------------------
		A thread fucntion
Read input device in a loop, handle callback if
event reaches.

NOTE:
1. A mouse KeyHoldDown event will NOT be polled independently, it's only
   updated together with other events, such as mouse moving/rolling
   etc.

@devname:	Name of the input device

----------------------------------------------------------------------*/
static void *egi_input_loopread( void* arg )
{
	fd_set rfds;
	struct timeval tmval;
	int retval;
	const char *dev_name=(const char *)arg;

	if(dev_name==NULL )
		return (void *)-1;

	/* -------------- Buffer ??? ---------------------------
	FILE *fp;
	fp=open("/dev/input/event0", "r");
	if(fp==NULL) {
		printf("%s: Open input event: %s", __func__, strerror(errno));
		return -1;
	}

	while(1) {
		//Read input event in buffer way
		fread((void *)&inevent, sizeof(inevent), 1, fp);
		if(ferror(fp)) {
			printf("%s: fread input event:%s\n",__func__, strerror(errno));
			return -2;
		}

		printf("Event_type: %d, Event_code: %d, Event_value: %d, Event_Time: %dsec, %dusec\n",
				inevent.type, inevent.code, inevent.value, (int)inevent.time.tv_sec, (int)inevent.time.tv_usec );
	}

	------------------------------------------ */

	/* Loop input select read */
	while(1) {
		/* Check end_loopread command */
		if(cmd_end_loopread_event)
			break;

		/* Check fd, and try to re_open if necessary. */
		while(input_fd<0) {
			input_fd=open( dev_name, O_RDWR|O_CLOEXEC);
			if(input_fd<0) {
				printf("%s: Open '%s' fail: %s.\n",__func__, dev_name, strerror(errno));
				tm_delayms(250);
			}
			else if(input_fd>0) {
				printf("%s: succeed to open '%s'.\n",__func__, dev_name);
				/* set mouse type to Intellimouse
				 * If the eventX device is mapped to /dev/input/mice or mouseX etc.., it will fail!?
				 * Instead, you should set the type to /dev/input/mice or mouseX etc..
				 */
				retval=write(input_fd, setup_data, sizeof(setup_data));
				if(retval<=0) {
					printf("%s: Fail to set as Intellimouse! retval=%d. \n",__func__, retval);
				} else {
					/* Read out reply to discard  */
					printf("%s: Succeed to set as Intellimouse! \n",__func__);
					read(input_fd, &inevent, sizeof(struct input_event));
				}
			}
		}

		/* To select fd */
		FD_ZERO(&rfds);
		FD_SET( input_fd, &rfds);
		tmval.tv_sec=1;
		tmval.tv_usec=0;

		retval=select( input_fd+1, &rfds, NULL, NULL, &tmval );
		if(retval<0) {
			printf("%s: select event: errno=%d, %s \n",__func__, errno, strerror(errno));
			continue;
		}
		else if(retval==0) {
			//printf("Time out\n");
			continue;
		}
		/* Read input fd and trigger callback */
		if(FD_ISSET(input_fd, &rfds)) {
			retval=read(input_fd, &inevent, sizeof(struct input_event));
			if(retval<0 && errno==ENODEV) {  /* Device is offline */
				printf("%s: retval=%d, read inevent :%s\n",__func__, retval, strerror(errno));
				if(close(input_fd)!=0)
					printf("%s: close input_fd :%s\n",__func__, strerror(errno));
				input_fd=-1;	/* close() will not reset it */
				printf("%s: Device '%s' is offline. close input_fd and reset it to %d. \n", __func__, dev_name, input_fd);
				tm_delayms(100);
				continue;
			}
			else if(retval<0) {
				printf("%s: read inevent errno=%d : %s\n",__func__, errno, strerror(errno));
				continue;
			}

			/* Call back */
			if(inevent_callback!=NULL) {
				//inevent_callback(inevent.type, inevent.code, inevent.value);
				inevent_callback(&inevent);
			}
		}
	}

	close(input_fd);
	input_fd=-1;
	return (void *)0;
}


/*--------------------------------------------------------
		  A thread function
Read mouse input data in loop.

Note:
1. If gv_fb_dev is NOT initialized! then gv_fb_dev.pos_xres
   and gv_fb_dev.pos_yres HAVE TO be set manually!


TODO:
1. There is chance that the mice dev MAY miss/omit events!?
   Example: LeftKeyDownHold transfers to LeftKeyDown directly,
	    and omit LeftKeyUp status.



@arg:	devname for the mouse device.
	if NULL, "/dev/input/mice"
---------------------------------------------------------*/
static void *egi_mouse_loopread( void* arg )
{
	fd_set rfds;
	struct timeval tmval;
	int retval;
	int tmp;
        unsigned char old_mouse_data[4]={0};
	bool status_sustain=false;
        int status=0;/* 0b00(0): Released_hold
                        0b01(1): Key Down / Raising edge
                        0b10(2): Key Up / Falling edge
                        0b11(3): Pressed_hold
                     */

	const char *dev_name="/dev/input/mice";
	if(arg!=NULL)
		dev_name=(const char *)arg;


	/* Loop input select read */
	while(1) {
		/* Check end_loopread command */
		//if(cmd_end_loopread_mouse)
		if(mostatus.cmd_end_loopread_mouse)
			break;

		/* Check fd, and try to re_open if necessary. */
#if 0
		while(mouse_fd<0) {
#else
		if(mouse_fd<0) {
#endif
			mouse_fd=open( dev_name, O_RDWR|O_CLOEXEC);
			if(mouse_fd<0) {
				printf("%s: Open '%s' fail: %s.\n",__func__, dev_name, strerror(errno));
				tm_delayms(50);
			}
			else if(mouse_fd>0) {
				printf("%s: Succeed to open '%s'.\n",__func__, dev_name);
				memset(mouse_data,0,sizeof(mouse_data));
				/* set mouse type to Intellimouse */
				retval=write( mouse_fd, setup_data,sizeof(setup_data));
				if(retval<=0) {
					printf("%s: Fail to set to as Intellimouse! retval=%d.\n",__func__, retval);
				} else {
					printf("%s: Succeed to set as Intellimouse!\n",__func__);
					/* Read out reply to discard  */
					read( mouse_fd, mouse_data, sizeof(mouse_data));
				}
				/* Clear mouse_data! */
				memset(mouse_data,0,sizeof(mouse_data));
			}
		}


		/* To select fd */
		FD_ZERO(&rfds);
		FD_SET( mouse_fd, &rfds);
		tmval.tv_sec=0;
		tmval.tv_usec=50000;

		//egi_dpstd("Start select mouse..\n");
		retval=select( mouse_fd+1, &rfds, NULL, NULL, &tmval );
		if(retval<0) {
			printf("%s: select event: errno=%d, %s \n",__func__, errno, strerror(errno));
			continue;
		}
		else if(retval==0) {
			//printf("Time out\n");
			if(status_sustain==false) {  /* Check if its DOWNHOLD idle */
				/* Keep status, clear Movement */
				/* EXAMPLE: If previous status is Click down. .....HOLD */
				mouse_data[0] &= 0x0F;
				mouse_data[1]=0;
				mouse_data[2]=0;
				mouse_data[3]=0;

				status_sustain=true;
				//continue; GO ON ....
			}
			else  /* Sustained status: DOWNHOLD etc. */
				continue;
		}
		/* Reset, status is changing. */
		status_sustain=false;

		/* Read input fd and trigger callback */
		if( retval>0 && FD_ISSET(mouse_fd, &rfds) ) {
			retval=read(mouse_fd, mouse_data, sizeof(mouse_data));

			/* For /dev/input/eventX and /dev/input/mouseX, ENODEV means device is offline.
			 * For /dev/input/mice, it alway exists, so ENODEV case never happens.
			 */
			if(retval<0 && errno==ENODEV) {
				printf("%s: read input mouse to get retval=%d, :%s \n",__func__, retval, strerror(errno));
				if(close(mouse_fd)!=0)
					printf("%s: close mouse_fd :%s\n",__func__, strerror(errno));
				mouse_fd=-1;	/* close() will not reset it */
				printf("%s: Device '%s' is offline. close mouse_fd and reset it to %d. \n", __func__, dev_name, mouse_fd);
				tm_delayms(100);
				continue;
			}
			else if(retval<0) {
				printf("%s: read mouse to get retval=%d, errno=%d : %s\n",__func__, retval, errno, strerror(errno));
				continue;
			}
			else if(retval==0) {
				printf("%s: read mouse to get retval=0, errno=%d : %s\n",__func__, errno, strerror(errno));
				continue;
			}

		}  /* END FD_ISSET */

		/* NOW: retval>0 OR TIME_OUT */

		/* Get mostatus mutex lock:  Waiting for test_surfman to egi_mouse_putRequest(pmostat) */
	        if( pthread_mutex_lock(&mostatus.mutex) !=0 )
			printf("%s: Fail to mutex lock mostatus.mutex!\n",__func__);
/*  --- >>>  Critical Zone  */

			/* ------ Cal. mouse status/data --------- */

		        /* 1. Renew status for Left Key */
        		status=((old_mouse_data[0]&0x1)<<1) + (mouse_data[0]&0x1);
	        	switch( status ) {
        	        	case 0b01:
	        	                mostatus.LeftKeyDown=true;
        	        	        mostatus.LeftKeyUpHold=false;
        	        	        mostatus.LeftKeyDownHold=false;
                	        	printf("-Leftkey Down!\n");
                        		break;
		                case 0b10:
	        	                mostatus.LeftKeyUp=true;
					mostatus.LeftKeyDown=false; /* MAY transfer from 0b01 ! */
        	        	        mostatus.LeftKeyDownHold=false;  /* <--------- */
                	        	printf("-Leftkey Up!\n");
	                	        break;
	        	        case 0b11:
        	        	        mostatus.LeftKeyDownHold=true;
		                        mostatus.LeftKeyDown=false;
					mostatus.LeftKeyUpHold=false;
		                        printf("-Leftkey DownHold!\n");
                		        break;
		                default:  /* 0b00: Leftkey UpHold */
					/* TODO: Necesssary? May miss status transitions sometimes, restore KeyUp status  */
					if(mostatus.LeftKeyDown || mostatus.LeftKeyDownHold ) {
			                        mostatus.LeftKeyUp=true;
                	  		      	printf("--Leftkey Up!\n");
					}
					else
			                        mostatus.LeftKeyUp=false;
                		        mostatus.LeftKeyUpHold=true;
                		        mostatus.LeftKeyDown=false;     /* May miss some status???  */
					mostatus.LeftKeyDownHold=false;
		                        break;
        		}
		        /* 2. Renew status for Right Key */
       			status=(old_mouse_data[0]&0x2) + ((mouse_data[0]&0x2)>>1);
	        	switch( status ) {
        	        	case 0b01:
	        	                mostatus.RightKeyDown=true;
        	        	        mostatus.RightKeyUpHold=false;
        	        	        mostatus.RightKeyDownHold=false;
                	        	printf("-Rightkey Down!\n");
                        		break;
		                case 0b10:
	        	                mostatus.RightKeyUp=true;
        	        	        mostatus.RightKeyDown=false;
        	        	        mostatus.RightKeyDownHold=false;
                	        	printf("-Rightkey Up!\n");
	                	        break;
	        	        case 0b11:
        	        	        mostatus.RightKeyDownHold=true;
		                        mostatus.RightKeyDown=false;
		                        printf(" ---Rightkey DownHold!\n");
                		        break;
		                default:  /* 0b00: UpHold */
                		        mostatus.RightKeyUpHold=true;
					mostatus.RightKeyUp=false;
                		        mostatus.RightKeyDown=false;
					mostatus.RightKeyDownHold=false;
		                        break;
        		}

		        /* 3. Renew status for Mid Key */
        		status=((old_mouse_data[0]&0x4)>>1) + ((mouse_data[0]&0x4)>>2);
	        	switch( status ) {
        	        	case 0b01:
	        	                mostatus.MidKeyDown=true;
        	        	        mostatus.MidKeyUpHold=false;
                	        //	printf("-Midkey Down!\n");
                        		break;
		                case 0b10:
	        	                mostatus.MidKeyUp=true;
					mostatus.MidKeyDown=false;
        	        	        mostatus.MidKeyDownHold=false;
                	        //	printf("-Midkey Up!\n");
	                	        break;
	        	        case 0b11:
        	        	        mostatus.MidKeyDownHold=true;
		                        mostatus.MidKeyDown=false;
		                        //printf(" ---Midkey DownHold!\n");
                		        break;
		                default:  /* 0b00: UpHold */
                		        mostatus.MidKeyUpHold=true;
		                        mostatus.MidKeyUp=false;
                		        mostatus.MidKeyDown=false;     /* May miss some status???  */
		                        break;
        		}

		        /* 4. Renew status for KeysIdle */
			mostatus.KeysIdle=(mostatus.LeftKeyUpHold) && (mostatus.RightKeyUpHold) && (mostatus.MidKeyUpHold);

	        	/* 5. Get mouse X */
			mostatus.mouseDX = (mouse_data[0]&0x10) ? mouse_data[1]-256 : mouse_data[1];
			tmp=mostatus.mouseX;
	        	mostatus.mouseX += mostatus.mouseDX;
        		if( mostatus.mouseX > gv_fb_dev.pos_xres -5)
		                mostatus.mouseX=gv_fb_dev.pos_xres -5;
		        else if( mostatus.mouseX<0)
		                mostatus.mouseX=0;
			/* Adjust DX accordingly! */
			mostatus.mouseDX = mostatus.mouseX-tmp;

		        /* 6. Get mouse Y: Notice LCD Y direction!  Minus for down movement, Plus for up movement!
		         * !!! For eventX: Minus for up movement, Plus for down movement!
		         */
			mostatus.mouseDY = -( (mouse_data[0]&0x20) ? mouse_data[2]-256 : mouse_data[2] );
			tmp=mostatus.mouseY;
		        mostatus.mouseY +=mostatus.mouseDY;

			/* Limit mouseY */
		        if( mostatus.mouseY > gv_fb_dev.pos_yres -5)
                		mostatus.mouseY=gv_fb_dev.pos_yres -5;
		        else if(mostatus.mouseY<0)
                		mostatus.mouseY=0;
			/* Adjust DY accordingly! */
			mostatus.mouseDY = mostatus.mouseY-tmp;

		        /* 7. Get mouse Z */
			mostatus.mouseDZ = (mouse_data[3]&0x80) ? mouse_data[3]-256 : mouse_data[3] ;
		        mostatus.mouseZ += mostatus.mouseDZ;

		        /* 8. Renew old mouse data */
		        memcpy(old_mouse_data, mouse_data, sizeof(old_mouse_data));

		/* ------ END: Cal. mouse status/data --------- */

/*  --- <<<  Critical Zone  */
	  	/* Put mutex lock */
		pthread_mutex_unlock(&mostatus.mutex);

		/* Call back. mostauts.request is set in the callback! */
		if(mouse_callback!=NULL) {
			/* If callback trapped here, then cmd_end_loopread_mouse signal will NOT be responded!
			 * mostauts.request is set in the callback!
			 */
			//egi_dpstd("RKDHold=%s\n", mostatus.RightKeyDownHold ? "TRUE" : "FALSE" );
			mouse_callback(mouse_data, sizeof(mouse_data), &mostatus);
		}

		#if 0 /* --- TEST ----  */
		/* Parse mouse data */
		printf("Left:%d, Right:%d, Middle:%d,  dx=%d, dy=%d, (wheel)dz=%d\n",
				mouse_data[0]&0x1, (mouse_data[0]&0x2) > 0, (mouse_data[0]&0x4) > 0,   /* 3 KEYL L.R.M */
				(mouse_data[0]&0x10) ? mouse_data[1]-256 : mouse_data[1],  /* dx */
				(mouse_data[0]&0x20) ? mouse_data[2]-256 : mouse_data[2],  /* dy */
					(mouse_data[3]&0x80) ? mouse_data[3]-256 : mouse_data[3]   /* dz, 2's complement */
				);
		#endif

		/* Wait for MEVETN request to be proceed, TODO: use pthread_cond_wait() instead. */
		/* Note:
		 * 1. This is to ensure each MEVENT request is parsed/responded.
		 * 2. Request usually is parsed/responded by another thread, other than mouse_callback(), Example: test_surfman.c */
		while( egi_mouse_checkRequest(&mostatus) && !mostatus.cmd_end_loopread_mouse ) { tm_delayms(5); };
	}

	close(mouse_fd);
	mouse_fd=-1;
	return (void *)0;
}


/*-------------------------------------------------
Return TURE if mostat.request==TRUE
Return FALSE if it fails or mostat.request==FALSE
Unlock moutex.
--------------------------------------------------*/
bool egi_mouse_checkRequest(EGI_MOUSE_STATUS *mostat)
{
	bool request;

	if(mostat==NULL)
		return false;

	/* Get mostatus mutex lock */
        if( pthread_mutex_lock(&mostat->mutex) !=0 ) {
		printf("%s: Fail to mutex lock mostatus.mutex!\n",__func__);
		return false;
	}

	request=mostat->request;

  	/* Put mutex lock */
	pthread_mutex_unlock(&mostat->mutex);

	return request;
}


/*---------------------------------------------------------
Return TURE if mostat.request==TRUE and keep mutex locked!
Return FALSE if  mostat.request==FALSE or it fails.
--------------------------------------------------------*/
bool egi_mouse_getRequest(EGI_MOUSE_STATUS *mostat)
{
	if(mostat==NULL)
		return false;

	/* Get mostatus mutex lock */
        if( pthread_mutex_lock(&mostat->mutex) !=0 ) {
		printf("%s: Fail to mutex lock mostatus.mutex!\n",__func__);
		return false;
	}

	if(mostat->request) {
		return true;
		/* --- Keep mostatus mutex locked! --- */
	}
	else {  /* Put mutex lock */
		pthread_mutex_unlock(&mostat->mutex);
		return false;
	}
}




/*-----------------------------------------------
Reset mostat.request to FALSE, and unlock mutex.

Return:
	0	OK
	<0	Fails
------------------------------------------------*/
int egi_mouse_putRequest(EGI_MOUSE_STATUS *mostat)
{
	if(mostat==NULL)
		return -1;

	/* Reset request */
	mostat->request=false;

  	/* Put mutex lock */
	pthread_mutex_unlock(&mostat->mutex);

	return 0;
}


/* ===================	Terminal IO setttings =============== */

static struct termios old_termioset;
static speed_t	      old_termispeed;
static speed_t	      old_termospeed;

/*------------------------------------
Set terminal IO attributes, and try to
set io speed as expected.
-------------------------------------*/
void egi_set_termios(void)
{
	 struct termios new_termioset;

#if 0   /* TEST: Call cfmakeraw() */
        tcgetattr(STDIN_FILENO, &old_termioset);
        new_termioset=old_termioset;

        cfsetispeed(&new_termioset,B57600);  /* 4097k */
        cfsetospeed(&new_termioset,B57600);

        /* cfmakeraw(): Input is available character by character, echoing is disabled,
         * and all special processing of terminal input and output characters is disabled.
         * Mouse event is disabled!???
         */
        cfmakeraw(&new_termioset);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termioset);
	return;
#else

	/* Save old settings */
        tcgetattr(STDIN_FILENO, &old_termioset);
	old_termispeed=cfgetispeed(&old_termioset);
	if(old_termispeed<0)
		egi_dperr("Fail cfgetispeed");
	old_termospeed=cfgetospeed(&old_termioset);
	if(old_termospeed<0)
		egi_dperr("Fail cfgetospeed");

	/* Setup with new settings */
        new_termioset=old_termioset;
        new_termioset.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_termioset.c_lflag &= (~ECHO);        /* disable echo */
        new_termioset.c_cc[VMIN]=0; //1;
        new_termioset.c_cc[VTIME]=0; //0


#if 0	/* TEST: set IO speed with tentative values. */
        if( cfsetispeed(&new_termioset,B57600)<0 )  /* 4k */
		egi_dperr("Fail cfsetiseed");

        if( cfsetospeed(&new_termioset,B57600)<0 )
		egi_dperr("Fail cfsetoseed");
#endif

	/* Set parameters to the terminal. TCSANOW -- the change occurs immediately.*/
        if( tcsetattr(STDIN_FILENO, TCSANOW, &new_termioset)<0 )
		egi_dperr("Fail tcsetattr");

        egi_dpstd("NEW input speed:%d, output speed:%d\n",cfgetispeed(&new_termioset), cfgetospeed(&new_termioset) );

#endif
}


/*------------------------------------------
Reset terminal IO attributes to old_termioset
-------------------------------------------*/
void egi_reset_termios(void)
{
	/* Restore IO speed */
        cfsetispeed(&old_termioset,old_termispeed);  /* 4k */
        cfsetospeed(&old_termioset,old_termospeed);

        /* Restore old terminal settings */
        tcsetattr(STDIN_FILENO, TCSANOW, &old_termioset);

        egi_dpstd("OLD input speed:%d, output speed:%d\n",cfgetispeed(&old_termioset), cfgetospeed(&old_termioset) );
}


/*--------------------------------------------------------------
Reference: https://blog.csdn.net/lanmanck/article/details/8423669

 * Copyright 2002 Red Hat Inc., Durham, North Carolina.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL RED HAT AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This is a simple test program that reads from /dev/input/event*,
 * decoding events into a human readable form.
 *
 *
 * Authors:
 *   Rickard E. (Rik) Faith <faith@redhat.com>

 ---------------------------------------------------------
 Modified to select /dev/input/event* to read KEY_EV only.
 Midas Zhou   <midaszhou@yahoo.com>

-----------------------------------------------------------------
Read keyboard EV_KEY event to get key codes. If kdev is NULL, then
scan and poll all /dev/input/event* files.

                ----- Concept and Definition -----

CONKEYs:    	Control keys of SHIFT, CTRL, ALT, SUPER, which
            	are NEITHER included in ASCII codes NOR represeted by Escape Sequence!

FUNCKEYs:   	Function keys: F1....F12

ASCIIKEYs:  	Keys defined in ASCII code, or definey by as TTY Escape Sequences, which
		including PgUp,PgDn,Insert,Home,End,Left,Right,Up,Down....

CONKEY_GROUP:  	Any combination of one or more of SHIFT/CTRL/ALT/SUPER, with each key appears once only,
		and there may be an ASCIIKEY attached at last.

ASCII_CONKEY: 	The only asciikey in a conkey group. AND it's the end a conkey_group!
		An input asciikey will be an ascii_conkey ONLY IF the conkey_group is NOT empty!
		OR it will be ignored.

LASTKEY:      	The lastkey in the last input keycode at each egi_read_kbdcode() session.
	TEST: -----    It's to be parsed as a functional key ONLY WHEN conkey_group is empty!

Other keys: 	For other keys defined in the keybaord, media_control keys etc.
            	TEST: conkey.lastkey......


Note:
0. Cat /proc/bus/input/devices to see corresponsing eventX device.
1. If a device has EV_KEY type event, then we assume it's a keyboard.

2.			--- WARNING! ---
   Static memebers(fd[]) are in the function!

3.		 	--- IMPORTANT! ---
   If 2 or more keys are pressed, then the ONLY repeating  keycode
   is that of the LAST pressed key! This will help to analyze sequence
   of combined_keys!

4.			--- IMPORTANT! ---
   Each round for kbdread may get more than 1 keyinput event, so need to buffer codes/timeval of those
   events in kstat->keycode[]/press[]/tmval[] first, then parse those data one by one, and
   store only final status.
   Example: If kbdread get KEY_A PRESS and RELEASE events both,  then only final RELEASE status will
   be recorded.
   So, a big KBD_STATUS_BUFSIZE helps to absorb unnecessary intermediate status.

5. Control_key_status in kstat will NOT be cleared before kbdread, they will be updated
   after kbdread.

6. XXX TODO: It MAY miss/omit key events if CPU load is too hight!!! ?
   Example: when press and release key 'A' quickly, it may miss/omit PRESS event, and get RELEASE only!
   ---OK, Make fd[] static to ensure unbroken event data for kbdread! and always check/validate them before add to fd_set.

7. For some keyboards: pressing down Left_SHIFT and Right_SHIFT at the same
   time will block further key input.

 @kstat: 	A pointer to EGI_KEY_STATUS.
 @kdev:		Specified input devcie path for keyboard.
		If null, ignore. then scan and poll /dev/input/eventX.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------*/
int egi_read_kbdcode(EGI_KBD_STATUS *kstat, const char *kdev)
{
    	int           i, j, k;
    	char          devname[64];           /* RATS: Use ok, but could be better */
    	unsigned char mask[EV_MAX/8 + 1]; /* RATS: Use ok */
    	static int    fd[16] = {0};	  /* Max. fd for input devies, static!!! */
	int	      nk;		  /* Total input devices, MAX. sizeof(fd), OR 1 if kdev!=NULL. */

    	struct timeval	tmout;
    	int           rc;

	struct input_event event;

	bool  type_ev_key;		/* Has EV_KEY event */
	bool  type_ev_rep;		/* OK, NOT necessary for a keybaord. */
	bool  finish_read_key;		/* KBD_STATUS_BUFSIZE limit! */

    	/* Select fds */
	int sfd;
    	int maxfd;
    	fd_set rfds;


	if(kstat==NULL)
		return -1;

	/* TEST:  Previous abskey value / press_lastkey. NO repeating function required!  */
	int32_t prev_absvalue;
	prev_absvalue = kstat->conkeys.absvalue;
	bool prev_press_lastkey;
	prev_press_lastkey = kstat->conkeys.press_lastkey;

	/* If input device specified */
	if(kdev)
		nk=1;
	else
		nk=sizeof(fd)/sizeof(typeof(fd[0]));


	/* 1. Clear fd set */
    	maxfd=0;
    	FD_ZERO(&rfds);

	/* 2. To scan all possible keyboard input devices. */
	for (i = 0; i < nk; i++) {
		if( kdev ) {
			strncpy(devname, kdev, sizeof(devname)-1);
			devname[sizeof(devname)-1]=0;
		}
		else
	        	sprintf(devname, "/dev/input/event%d", i);

		/* All fd[] are static */
        	if ( fd[i]>0 || (fd[i] = open(devname, O_RDONLY|O_NONBLOCK, 0)) >= 0 ) {

			/* Check file descriptor, in case input device disconnected. */
			if( egi_valid_fd(fd[i])!=0 ) {
				egi_dperr("'%s' is invalid or disconnected!", devname);
				/* Close fd[i] */
				if(close(fd[i])!=0)
					egi_dperr("close(fd)");
				fd[i]=-1;
				/* Clear KBD STATUS */
				memset(kstat,0,sizeof(EGI_KBD_STATUS));
				continue;
			}

			#if 0 /* If fd[] is invalid, icotl() also fails */
            		if( ioctl(fd[i], EVIOCGBIT(0, sizeof(mask)), mask) <0 ) {
				egi_dpstd("Fail ioctl to '%s', evdev invalid or disconnected! \n", devname);
				/* Close fd[i] */
				if(close(fd[i])!=0)
					egi_dperr("close(fd)");
				fd[i]=-1;
				/* Clear KBD STATUS */
				memset(kstat,0,sizeof(EGI_KBD_STATUS));
				continue;
			}
			#endif

			/* Get mask of evdev */
            		ioctl(fd[i], EVIOCGBIT(0, sizeof(mask)), mask);

#if 0 /* Print out input device information */
			struct input_id	   devID;
		    	int           version;
		    	char          buf[256] = { 0, };  /* RATS: Use ok */

			ioctl(fd[i], EVIOCGID, &devID);
	        	ioctl(fd[i], EVIOCGVERSION, &version);
        		ioctl(fd[i], EVIOCGNAME(sizeof(buf)), buf);

			printf("devID.product=%u\n", devID.product);
            		printf("%s\n", devname);
            		printf("    evdev version: %d.%d.%d\n",
                   		version >> 16, (version >> 8) & 0xff, version & 0xff);
            		printf("    devname: %s\n", buf);
            		printf("    features:");
#endif

			/* Clear token */
			type_ev_key=false;
			type_ev_rep=false;

			/* Check input device/event type */
            		for (j = 0; j < EV_MAX; j++) {
				// #define test_bit(bit) (mask[(bit)/8] & (1 << ((bit)%8)))
				if( mask[j/8] & (1 << (j%8)) ) {
		                        const char *type = "unknown";
	        	            	switch(j) {
			                    case EV_KEY: type = "keys/buttons";
						type_ev_key=true;
						break;
			                    case EV_REL: type = "relative";     break;
			                    case EV_ABS: type = "absolute";     break;
			                    case EV_MSC: type = "reserved";     break;
			                    case EV_LED: type = "leds";         break;
			                    case EV_SND: type = "sound";        break;
			                    case EV_REP: type = "repeat";
						type_ev_rep=true;
						break;
			                    case EV_FF:  type = "feedback";     break;
                    			}
					//printf(" %s \n", type);
                		}
            		}
#if 0 /* Print out input device information */
            		printf("\n");
#endif
			/* XXX If EV_KEY+EV_REP: Add fd into selection AS keyboard. */
			/* If EV_KEY: Add fd into selection AS keyboard. */
			if( type_ev_key ) { // && type_ev_rep ) {
				FD_SET(fd[i],&rfds);
				if(fd[i] > maxfd)
					maxfd=fd[i];
//				printf("KBD fd[%d]: '%s', product id=%u\n", i, devname, devID.product);
			}
		}
//		else
//			egi_dpstd("Fail to open '%s', or it doesn't exist.\n", devname);

	} /* END for() */

	/* 3. If no fd in set. */
	if(maxfd==0)
		return -2;

	/* 4. Clear token */
	finish_read_key=false;

	/* 5. Init/Clear temp. vars: kstat->ks, keycode[], press[], tmval[]. !!! Keep kstat->control_key_status. */
	kstat->ks=0;
	memset(kstat->keycode, 0, sizeof(typeof(*kstat->keycode))*KBD_STATUS_BUFSIZE);
	memset(kstat->press, 0, sizeof(typeof(*kstat->press))*KBD_STATUS_BUFSIZE);
	memset(kstat->tmval, 0, sizeof(typeof(*kstat->tmval))*KBD_STATUS_BUFSIZE);

	/* 5.1 Init kstat->conkey.abskey ---NOPE!  */

	/* 6. Select and read fd[] */
        tmout.tv_sec=0;
        tmout.tv_usec=10000;
	sfd=select(maxfd+1, &rfds, NULL, NULL, &tmout);

   	if(sfd>0) {

	   for(i=0; i<nk; i++) {

		/* Skip invalid fd[] */
		if( fd[i]<0 )
			continue;

		if( !FD_ISSET(fd[i], &rfds) )
			continue;

	    	/* Read out fds[i], break if get KEY_CODE. (at least one KEY_CODE) */
            	while( (rc = read(fd[i], &event, sizeof(struct input_event))) > 0) {
	#if 0 /* Print event time/type  */
			egi_dpstd("fd[%d]: ",i);
			egi_dpstd("%-24.24s.%06lu type 0x%04x; code 0x%04x;"
                       		" value 0x%08x; ",
                      		ctime(&event.time.tv_sec),
                       		event.time.tv_usec,
                 		event.type, event.code, event.value);
	#endif
                	switch (event.type) {
	                   case EV_KEY:
	#if 0 /* Print key code/value */
        	            	if (event.code > BTN_MISC) {
               		        	printf("Button %d %s", event.code & 0xff,
	                       	       		event.value ? "press" : "release");
				} else {
                       			printf("Key %d (0x%x) %s\n", event.code & 0xff,
	                       			event.code & 0xff, event.value ? "press" : "release");
               	    		}
	#endif
				/* Buffer codes/timeval of keyevent. */
				kstat->keycode[kstat->ks]=event.code & 0xff;
				kstat->press[kstat->ks]=event.value;
				kstat->tmval[kstat->ks]=event.time;
				kstat->ks++;
				if(kstat->ks==KBD_STATUS_BUFSIZE) {
					egi_dpstd("KBD_STATUS_BUFSIZE limit!\n");
					finish_read_key=true;
				}

				break;

	                case EV_ABS:
				kstat->conkeys.abskey = event.code & 0xff;
				kstat->conkeys.absvalue = event.value; 	/* NOTE! */
				if(kstat->conkeys.absvalue==0x7F)
					kstat->conkeys.press_abskey=false; /* 0x7F as release? GamePad */
				else
					kstat->conkeys.press_abskey=true;
				//egi_dpstd("%s abskey=%d, absvalue=%d\n", kstat->conkeys.press_abskey?"Press":"Release",
				//		kstat->conkeys.abskey, kstat->conkeys.absvalue);
				//egi_dpstd("EV_ABS: abskey=%d %s\n", kstat->conkeys.abskey, kstat->conkeys.abskey==0?"ABS_X":"");
						/* 0: ABS_X, 1: ABS_Y, ABS_MAX(0x3F): IDLE */
				break;

			#if 0 /*  Discard other event...  */
	                   case EV_REL: printf("Rel"); break;
	                   case EV_MSC: printf("Misc"); break;
        	           case EV_LED: printf("Led");  break;
                	   case EV_SND: printf("Snd");  break;
	                   case EV_REP: printf("Rep");  break;
        	           case EV_FF:  printf("FF");   break;
			#endif

			   default:
				//printf("NOT EV_KEY!\n");
				break;
                	}
	#if 0 /* Flush to print keys */
                	printf("\n");
	#endif
		        /* If got keycode, exit loop. */
     			if(finish_read_key) {
	     			//printf("Got keycode!\n");
	     			break;
    			}

	    	} /* END while( read(fd[i]..) */

	        /* If got keycode, exit for() loop. */
     		if(finish_read_key)
	     		break;

           } /* END for() */

        } /* END if(sfd>0) */

#if 0	/* XXX 7.Check ks XXX DO NOT, we need EV_ABS key also... */
	if(kstat->ks==0) {
 		//egi_dpstd("No input key detected!\n");
		//NOPE!!!  kstat->conkeys.lastkey = 0; /* Keep lastkey!!! */
		return 0;
	}
#endif

 /* To process buffered EV_KEY event data */
 if( kstat->ks >0 ) {

	/* 8. Parse and get key status! */
	for(i=0; i< kstat->ks; i++) {
		egi_dpstd("keycode[%d]:%d\n", i, kstat->keycode[i]);

		/* 8.1 If an ascii_conkey is already pressed, then STOP/END reading conkey_group!!!
		 * If not to release the same asciikey, then break to get rid of all followed keycodes! */
		if( kstat->conkeys.press_asciikey && kstat->press[i] )
		{
			egi_dpstd("Conkey ends with an asciikey already, ignore!\n");
			/* DO NOT 'break' here,  In case the same key with consecutive status: press/press/release...
  			 * then the last 'release' status will be neglected!
			 */
			continue;
		}

		/* 8.2 Switch keycode[] */
		switch(kstat->keycode[i]) {

			/* ----- 8.2.1  Prase conkeys ----- */

			case KEY_LEFTCTRL:
				kstat->conkeys.press_leftctrl=kstat->press[i];
				kstat->tm_leftctrl=kstat->tmval[i];
				break;
			case KEY_RIGHTCTRL:
				kstat->conkeys.press_rightctrl=kstat->press[i];
				kstat->tm_rightctrl=kstat->tmval[i];
				break;
			case KEY_LEFTALT:
				kstat->conkeys.press_leftalt=kstat->press[i];
				kstat->tm_leftalt=kstat->tmval[i];
				break;
			case KEY_RIGHTALT:
				kstat->conkeys.press_rightalt=kstat->press[i];
				kstat->tm_rightalt=kstat->tmval[i];
				break;
			case KEY_LEFTSHIFT:
				kstat->conkeys.press_leftshift=kstat->press[i];
				kstat->tm_leftshift=kstat->tmval[i];
				break;
			case KEY_RIGHTSHIFT:
				kstat->conkeys.press_rightshift=kstat->press[i];
				kstat->tm_rightshift=kstat->tmval[i];
				break;
			case KEY_LEFTSUPER:
				kstat->conkeys.press_leftsuper=kstat->press[i];
				kstat->tm_leftsuper=kstat->tmval[i];
				break;
			case KEY_RIGHTSUPER:
				kstat->conkeys.press_rightsuper=kstat->press[i];
				kstat->tm_rightsuper=kstat->tmval[i];
				break;

			/* ----- 8.2.2  Prase function keys ----- */
			/* KEY_F1(59) ~ KEY_F10(68), KEY_F11(87), KEY_F12(88)  */
			/* MAYBE put at lastkey */

			case KEY_F1:
				kstat->conkeys.press_F1=kstat->press[i];
				break;
			case KEY_F2:
				kstat->conkeys.press_F2=kstat->press[i];
				break;
			case KEY_F3:
				kstat->conkeys.press_F3=kstat->press[i];
				break;
			case KEY_F4:
				kstat->conkeys.press_F4=kstat->press[i];
				break;
			case KEY_F5:
				kstat->conkeys.press_F5=kstat->press[i];
				break;
			case KEY_F6:
				kstat->conkeys.press_F6=kstat->press[i];
				break;
			case KEY_F7:
				kstat->conkeys.press_F7=kstat->press[i];
				break;
			case KEY_F8:
				kstat->conkeys.press_F8=kstat->press[i];
				break;
			case KEY_F9:
				kstat->conkeys.press_F9=kstat->press[i];
				break;
			case KEY_F10:
				kstat->conkeys.press_F10=kstat->press[i];
				break;
			case KEY_F11:
				kstat->conkeys.press_F11=kstat->press[i];
				break;
			case KEY_F12:
				kstat->conkeys.press_F12=kstat->press[i];

			/* ----- 8.2.3  Prase ascii_conkey ----- */

			/* Assume default all ASCII keys (NOT ASCII codes!!!), include PgDn,PgUp,Home,End,Insert.. */
			default:
			    /* IF release the already pressed asciikey */
			    if( kstat->conkeys.asciikey !=0 ) {  /* Only one asciikey allowed */
				if( kstat->keycode[i] == kstat->conkeys.asciikey && !kstat->press[i] ) {
					egi_dpstd("Release ascii_conkey %u\n", kstat->conkeys.asciikey);
					kstat->conkeys.asciikey=0; /* Release the same ascii key */
					kstat->conkeys.press_asciikey=false;
				}
			    }
			    /* Else: an asciikey preceded by a non_ASCII conkey */
			    else if( kstat->press[i] ) {
				/* An ASCII key will be deemed as a conkey ONLY WHEN preceded by a non_ASCII conkey */
				if( kstat->conkeys.press_leftctrl || kstat->conkeys.press_rightctrl
				    || kstat->conkeys.press_leftalt || kstat->conkeys.press_rightalt
				    || kstat->conkeys.press_leftshift || kstat->conkeys.press_rightshift
				    || kstat->conkeys.press_leftsuper || kstat->conkeys.press_rightsuper )
				{
					kstat->conkeys.asciikey =  kstat->keycode[i];
					kstat->conkeys.press_asciikey = kstat->press[i];
					egi_dpstd("Press ascii_conkey %u\n", kstat->conkeys.asciikey);
				}
			  }

			  break;
		}

	} /* End for() */



	/* 8.A Save the lastkey to kstat->conkeys.lastkey, NOPTE!  XXX here ks>=1 assured, otherwise it returns when ks==0, see 7.
	 *  Functional keys(media contorl keys, etc.) usually has NO repeating event...
	 *  A lastkey will be functional ONLY WHEN conkey_group is empty!
	 */
		kstat->conkeys.lastkey = kstat->keycode[kstat->ks-1];    /* ks-1: The last index! */
		kstat->conkeys.press_lastkey = kstat->press[kstat->ks-1];
		kstat->tm_lastkey=kstat->tmval[kstat->ks-1];

	/* kstat->conkeys.abskey saved at above 6. at case EV_ABS */

				/* ----- 9. Update conkeyseq[] ----- */

	/* 9.1 Clear conkeyseq[] */
	for(i=0; i<CONKEYSEQ_MAX+1; i++)  /* +1 for EOF */
		kstat->conkeys.conkeyseq[i]=CONKEYID_NONE;

	/* 9.2 Update conkeyseq[] with current buffered key data */
	struct timeval conkeytm[CONKEYSEQ_MAX+1]={0};  /* +1 as EOF */
	i=0; /* CONKEY counter */

	/* 9.2.1  Check KEY_CTRL */
	if( kstat->conkeys.press_leftctrl || kstat->conkeys.press_rightctrl ) {
		kstat->conkeys.conkeyseq[i] = CONKEYID_CTRL;

		if( kstat->conkeys.press_leftctrl && !kstat->conkeys.press_rightctrl )
			conkeytm[i] = kstat->tm_leftctrl;
		else if( kstat->conkeys.press_rightctrl && !kstat->conkeys.press_leftctrl )
			conkeytm[i] = kstat->tm_rightctrl;
		else if( kstat->conkeys.press_rightctrl && kstat->conkeys.press_leftctrl ) {
			if( timercmp(&kstat->tm_leftctrl, &kstat->tm_rightctrl, >) ) { /* Right press first */
				printf("Right CTRL press first!\n");
				conkeytm[i] = kstat->tm_rightctrl;
			}
			else {
				printf("Left CTRL press first!\n");
				conkeytm[i] = kstat->tm_leftctrl;
			}
		}

		i++; /* Count CONKEYs */
	}
	/* 9.2.2 Check KEY_ALT */
	if( kstat->conkeys.press_leftalt || kstat->conkeys.press_rightalt ) {
		kstat->conkeys.conkeyseq[i] = CONKEYID_ALT;

		if( kstat->conkeys.press_leftalt && !kstat->conkeys.press_rightalt )
			conkeytm[i] = kstat->tm_leftalt;
		else if( kstat->conkeys.press_rightalt && !kstat->conkeys.press_leftalt )
			conkeytm[i] = kstat->tm_rightalt;
		else if( kstat->conkeys.press_rightalt && kstat->conkeys.press_leftalt ) {
			if( timercmp(&kstat->tm_leftalt, &kstat->tm_rightalt, >) ) /* Right press first */
				conkeytm[i] = kstat->tm_rightalt;
			else
				conkeytm[i] = kstat->tm_leftalt;
		}

		i++; /* Count CONKEYs */
	}
	/* 9.2.3 Check KEY_SHIFT */
	if( kstat->conkeys.press_leftshift || kstat->conkeys.press_rightshift ) {
		kstat->conkeys.conkeyseq[i] = CONKEYID_SHIFT;

		if( kstat->conkeys.press_leftshift && !kstat->conkeys.press_rightshift )
			conkeytm[i] = kstat->tm_leftshift;
		else if( kstat->conkeys.press_rightshift && !kstat->conkeys.press_leftshift )
			conkeytm[i] = kstat->tm_rightshift;
		else if( kstat->conkeys.press_rightshift && kstat->conkeys.press_leftshift ) {
			if( timercmp(&kstat->tm_leftshift, &kstat->tm_rightshift, >) ) /* Right press first */
				conkeytm[i] = kstat->tm_rightshift;
			else
				conkeytm[i] = kstat->tm_leftshift;
		}

		i++; /* Count CONKEYs */
	}
	/* 9.2.4 Check KEY_SUPER */
	if( kstat->conkeys.press_leftsuper || kstat->conkeys.press_rightsuper ) {
		kstat->conkeys.conkeyseq[i] = CONKEYID_SUPER;

		if( kstat->conkeys.press_leftsuper && !kstat->conkeys.press_rightsuper )
			conkeytm[i] = kstat->tm_leftsuper;
		else if( kstat->conkeys.press_rightsuper && !kstat->conkeys.press_leftsuper )
			conkeytm[i] = kstat->tm_rightsuper;
		else if( kstat->conkeys.press_rightsuper && kstat->conkeys.press_leftsuper ) {
			if( timercmp(&kstat->tm_leftsuper, &kstat->tm_rightsuper, >) ) /* Right press first */
				conkeytm[i] = kstat->tm_rightsuper;
			else
				conkeytm[i] = kstat->tm_leftsuper;
		}

		i++; /* Count CONKEYs */
	}

	/* 9.2.5 NOW: i is the total number of conkeys pressed(execpt ascii_conkey),   i <= 4! (CONKEYSEQ_MAX), */
	kstat->conkeys.nk=i;

#if 0 /* TEST: -------------print conkeyseq[], NOT in time sequence. */
	printf("CONKEYs in conkeyseq[]: ");
	for(k=0; k < kstat->conkeys.nk; k++) {
		printf("%s ", CONKEY_NAME[ (int)(kstat->conkeys.conkeyseq[k]) ] );
	}
	printf("\n");
/* TEST: end */
#endif

	/* 9.2.6 InsertSort conkeyseq[] according to conkeytm[]: conkeyseq[0] the_first_pressed ---> conkeseq[3] the_last_pressed */
	struct timeval tmptm;
	int tmpseq;
	for(j=1; j<i; j++) {
		tmptm=conkeytm[j];
		tmpseq=kstat->conkeys.conkeyseq[j];
		for(k=j; k>0 && timercmp(&conkeytm[k-1], &tmptm, >); k--) {   /*  */
			/* Swap conkeyseq[] and conkeytm[] */
			kstat->conkeys.conkeyseq[k] = kstat->conkeys.conkeyseq[k-1];
			conkeytm[k]=conkeytm[k-1];
		}

		kstat->conkeys.conkeyseq[k] = tmpseq;
		conkeytm[k]=tmptm;
	}

	/* 10. If all CONKEYs released, then the only ascii_conkey is also invalid! */
	if(i==0) {
        	kstat->conkeys.asciikey = 0;
        	kstat->conkeys.press_asciikey = false;
	}
	/* Add ascii_conkey to conkeseq[] */
	else if( kstat->conkeys.press_asciikey ) {
		kstat->conkeys.conkeyseq[kstat->conkeys.nk] = CONKEYID_ASCII;
		kstat->conkeys.nk += 1;
	}

} /* END of if( kstat->ks >0 ) */


/* TEST: ---- 11. If abskey value keeps as IDLE(0x7F) aft. readkbd, reset baskey=ABS_MAX to make it invalid!
	 TODO: Check abskey CODE also!
 */
	if( prev_absvalue==0x7F && kstat->conkeys.absvalue==0x7F ) {
		//egi_dpstd("abskey released alread!\n");
		kstat->conkeys.abskey = ABS_MAX;
	}

	/* 12. If press_lastkey keeps FALSE, clear laskey code!  TODO: Check lastkey CODE also! ... here see lastkey as a func. key. */
	if( prev_press_lastkey==false  &&  kstat->conkeys.press_lastkey==false )
		kstat->conkeys.lastkey = 0;

/* TEST: ----------- print sorted CONKEYs */
  if(kstat->conkeys.nk>0 || kstat->conkeys.asciikey )
  {
	egi_dpstd("Sorted CONKEYs: ");
	for(k=0; k < kstat->conkeys.nk; k++) {
		printf("%s ", CONKEY_NAME[ (int)(kstat->conkeys.conkeyseq[k]) ] );
	}
	if( kstat->conkeys.asciikey )
		printf("key_%u", kstat->conkeys.asciikey);
	printf("\n");
  }
/* TEST: ----------  EV_KEY: lastkey.   ONLY for press statu OR the last release statu.  */
	if(kstat->conkeys.press_lastkey || prev_press_lastkey )  /* now_pressed OR keep_pressed OR now_release(prev_pressed) */
		egi_dpstd("%s lastkey=%d\n", kstat->conkeys.press_lastkey?"Press":"Release", kstat->conkeys.lastkey);

/* TEST: ----------- EV_ABS: abskey. Press statu need to be reset by Caller! */
	if(kstat->conkeys.abskey!=ABS_MAX)  /* ABS_MAX as IDLE */
		egi_dpstd("%s abskey=%d, absvalue=%d\n", kstat->conkeys.press_abskey?"Press/pressed":"Release",
					kstat->conkeys.abskey, kstat->conkeys.absvalue);

    	return 0;
}



