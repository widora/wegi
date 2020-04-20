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



Midas Zhou
-------------------------------------------------------------------*/
#include "egi_input.h"
#include "egi_timer.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   /* open */
#include <unistd.h>  /* read */
#include <errno.h>
#include <stdbool.h>
#include <linux/input.h>

static int input_fd=-1;
static fd_set rfds;
static struct input_event  inevent;
static EGI_INEVENT_CALLBACK  inevent_callback; /* To be assigned */
/* TODO: mutex lock for inevent_callback */

static bool tok_loopread_running;       /* token for loopread is running if true */
static bool cmd_end_loopread;           /* command to end loopread if true */
static pthread_t thread_loopread;

static void *egi_input_loopread(void *arg);

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

	if(tok_loopread_running==true) {
		printf("%s: inevent loopread has been running already!\n",__func__);
		return -1;
	}

	cmd_end_loopread=false;

        /* start touch_read thread */
        if( pthread_create(&thread_loopread, NULL, (void *)egi_input_loopread, (void *)dev_name) !=0 )
        {
                printf("%s: Fail to create inevent loopread thread!\n", __func__);
                return -1;
        }

        /* reset token */
        tok_loopread_running=true;

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
	if(tok_loopread_running==false) {
		printf("%s: inevent loopread has been ceased already!\n",__func__);
		return -1;
	}

        /* Set indicator to end loopread */
        cmd_end_loopread=true;

        /* Wait to join touch_loopread thread */
        if( pthread_join(thread_loopread, NULL ) !=0 ) {
                printf("%s:Fail to join thread_loopread.\n", __func__);
                return -1;
        }

	/* reset cmd and token */
	cmd_end_loopread=false;
	tok_loopread_running=false;


	/* ccc */
	close(input_fd);

	return 0;
}



/*------------------------------------------------------
		A thread fucntion
Read input device in a loop, handle callback if
event reaches.

@devname:	Name of the input device

----------------------------------------------------*/
static void *egi_input_loopread( void* arg )
{
	struct timeval tmval;
	int retval;
	char *dev_name=(char *)arg;

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

	input_fd=open( dev_name, O_RDONLY|O_CLOEXEC);
	#if 0  /* to check in while() */
	if(input_fd<0) {
		printf("%s: Open input event: %s", __func__, strerror(errno));
		return (void *)-1;
	}
	#endif

	/* Loop input select read */
	while(1) {
		/* Check end_loopread command */
		if(cmd_end_loopread)
			break;

		/* Check fd, and try to re_open if necessary. */
		while(input_fd<0) {
			printf("%s: Input fd is invalid, input device may be offline! \n",__func__);
			tm_delayms(500);
			input_fd=open( dev_name, O_RDONLY|O_CLOEXEC);
		}

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
			printf("Time out\n");
			continue;
		}

		if(FD_ISSET(input_fd, &rfds)) {
			retval=read(input_fd, &inevent, sizeof(struct input_event));
			if(retval<0 && errno==ENODEV) {
				printf("%s: retval=%d, read inevent :%s\n",__func__, retval, strerror(errno));
				if(close(input_fd)!=0)
					printf("%s: close input_fd :%s\n",__func__, strerror(errno));
				printf("%s: close input_fd and reset it to %d\n", __func__, input_fd);
				input_fd=-1;
				tm_delayms(500);
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



