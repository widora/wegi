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

Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------------------------*/
#include "egi_debug.h"
#include "egi_input.h"
#include "egi_timer.h"
#include "egi_fbdev.h"
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
		while(mouse_fd<0) {
			mouse_fd=open( dev_name, O_RDWR|O_CLOEXEC);
			if(mouse_fd<0) {
				printf("%s: Open '%s' fail: %s.\n",__func__, dev_name, strerror(errno));
				tm_delayms(250);
			}
			else if(mouse_fd>0) {
				printf("%s: succeed to open '%s'.\n",__func__, dev_name);
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
                	        	printf("-Leftkey Down!\n");
                        		break;
		                case 0b10:
	        	                mostatus.LeftKeyUp=true;
					mostatus.LeftKeyDown=false; /* MAY transfer from 0b01 ! */
        	        	        mostatus.LeftKeyDownHold=false;
                	        	printf("-Leftkey Up!\n");
	                	        break;
	        	        case 0b11:
        	        	        mostatus.LeftKeyDownHold=true;
		                        mostatus.LeftKeyDown=false;
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

		/* Call back */
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
		printf("%s: Fail cfgetispeed, Err'%s'\n", __func__, strerror(errno));
	old_termospeed=cfgetospeed(&old_termioset);
	if(old_termospeed<0)
		printf("%s: Fail cfgetispeed, Err'%s'\n", __func__, strerror(errno));

	/* Setup with new settings */
        new_termioset=old_termioset;
        new_termioset.c_lflag &= (~ICANON);      /* disable canonical mode, no buffer */
        new_termioset.c_lflag &= (~ECHO);        /* disable echo */
        new_termioset.c_cc[VMIN]=0; //1;
        new_termioset.c_cc[VTIME]=0; //0

	/* TEST: set IO speed with tentative values. */
        if( cfsetispeed(&new_termioset,B57600)<0 )  /* 4k */
		printf("%s: Fail cfsetiseepd, Err'%s'\n", __func__, strerror(errno));

        if( cfsetospeed(&new_termioset,B57600)<0 )
		printf("%s: Fail cfsetoseepd, Err'%s'\n", __func__, strerror(errno));

	/* Set parameters to the terminal. TCSANOW -- the change occurs immediately.*/
        if( tcsetattr(STDIN_FILENO, TCSANOW, &new_termioset)<0 )
		printf("%s: Fail tcsetattr, Err'%s'\n", __func__, strerror(errno));

        printf("NEW input speed:%d, output speed:%d\n",cfgetispeed(&new_termioset), cfgetospeed(&new_termioset) );

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

        printf("OLD input speed:%d, output speed:%d\n",cfgetispeed(&old_termioset), cfgetospeed(&old_termioset) );
}


