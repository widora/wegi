/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


A EGI FIFO buffer

Midas Zhou
-----------------------------------------------------------------*/
#ifndef __EGI_FIFO__
#define __EGI_FIFO__

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

/* DO NOT REFER EGI_FIFO MEMBER DIRECTLY IN MULT_THREAD CONDITION, THEY ARE UNSTABLE !!! */
typedef struct
{
	int	  	item_size;	/* size of each item data, in byte. */
	int   		buff_size;	/* total item number that the buff is capable of holding */
	unsigned char 	**buff;		/* data buffer [buff_size][item_size] */

	uint32_t 	pin;  		/* data pusher's position, as buff[pin].
					 * after push_in, pin++ to move to next slot position !!!!!!  */

	uint32_t 	pout; 		/* data puller's position!!!, as buff[pout]
					 * after pull_out, pout++ to move to next slot position !!!!!! */

	int		ahead; 		/* +1 when pin runs ahead of pout && cross start line [0]
					 * one more time then pout, -1 when pout cross start line
					 * one more time. In most case ahead will be 0 or 1, and rarely it
					 * may be -1 or 2.
 					 */

	pthread_mutex_t lock;		/* thread mutex lock */

	int		pin_wait;	/* 1.Set pin_wait==0: It keeps pushing data, pin never waits for pout,
					 * some data will be overwritten and lost before pout can get them.
					 * 2.Set pin_wait!=0: When pin catches up pout from a loop back, it will
					 * stop and wait for pin to catch up, so all data will be picked up
					 * by pout.
					 */
}EGI_FIFO;


EGI_FIFO * egi_malloc_fifo(int buff_size, int item_size, int pin_wait);
void egi_free_fifo(EGI_FIFO *efifo );
int egi_push_fifo(EGI_FIFO *fifo, unsigned char *data, int size, int *in, int *out, int *ahd );
int egi_pull_fifo(EGI_FIFO *fifo, unsigned char *data, int size, int *in, int *out, int *ahd );


#endif
