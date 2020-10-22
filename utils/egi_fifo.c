/*----------------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A EGI FIFO buffer

1. Overrun occurs when pin catches up pout from loop_back, and underrun occurs when pout catches up
   pin and no more new data is available.

2. For overrun, we may take DIFFERENT stratigies:
   2.1 fifo->pin_wait !=0:
		When overrun occurs, pin will stop and wait for pout to catch up.
   2.2 fifo->pin_wait ==0:
		When overrun occurs, Pin will keep increasing and old data in buff will be overwritten
		and permantly lost before pout gets them.

3. When underrun occurs, pout will wait for pin to increase. so pout will alway lag behind pin.

4. Keep same sequence rule for push and pull:
      1. First, check whether pin/pout == fifo->buff_size,  If so, reset pin/pout to 0.
      2. Then, check whether overrun or underrun occurs.
      3. If we let pin increase even when overrun ocurrs, then after N times consecutive overrun,
	 total bytes of data loss MAY be N*(fifo->item_size) where N=1,2,3....
         ---So, when ahead==2, we acknowedge overrun and reset ahead to 0.
      4. Ahead=2 happens when pout remainds at buff_size(for example, the pulling thread gets stuck), and
	 pin keeps looping back, like this:
         Ahead increase from 0 to 1 when pin loop from buff_size to 0, then again when it increases to
	 be buff_size, the program  will first increase ahead to be 2 and then check if overrun occurs,
	 just as above mentioned.
      5. Ahead=-1 happens when pin remaids/stops at buff_size, and pout chase up, then pout changes from
	 buff_size to 0 and ahead changes from 0 to -1, which hanppes just before underrun check.

5. Ahead may be -1,0,1,2, most likely 0 or 1.


Midas Zhou
--------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_utils.h"
#include "egi_fifo.h"


/*--------------------------------------------------------------------
Malloc a EGI_FIFO structure and its buff.

efifo:		An empty pointer to EGI_FIFO.
buff_size: 	max data items to be hold in the buffer.
item_size:	data item size.

Return:
	!NULL	OK
	NULL	fails
---------------------------------------------------------------------*/
EGI_FIFO * egi_malloc_fifo(int buff_size, int item_size, int pin_wait)
{
	EGI_FIFO *efifo=NULL;

	/* check params */
//	if(efifo != NULL)
//		EGI_PLOG(LOGLV_WARN,"%s: input EGI_FIFO pointer is not NULL.\n",__func__);

	if( buff_size<=0 || item_size<=0 )
	{
		EGI_PLOG(LOGLV_ERROR,"%s: input buff_size or item_size is invalid.\n",
										__func__);
		return NULL;
	}


	/* malloc efifo */
	efifo=(EGI_FIFO *)malloc(sizeof(EGI_FIFO));
	if(efifo==NULL)
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to malloc efifo.\n",__func__);
		return NULL;
	}
	memset(efifo,0,sizeof(EGI_FIFO));

        /* buff mutex lock */
        if(pthread_mutex_init(&efifo->lock,NULL) != 0)
        {
                printf("%s: fail to initiate log_buff_mutex.\n",__func__);
		free(efifo);

                return NULL;
        }

	/* malloc buff, and memset inside... */
	efifo->buff=egi_malloc_buff2D(buff_size, item_size);
	if(efifo->buff==NULL)
	{
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to malloc efifo->buff.\n", __func__);

		free(efifo);
		efifo=NULL; /* ineffective though...*/

		return NULL;
	}

	/* set EGI_FIFO member */
	efifo->buff_size=buff_size;
	efifo->item_size=item_size;
	efifo->pin  = 0;
	efifo->pout = 0;
	efifo->ahead=0;
	efifo->pin_wait=pin_wait; /* 0:  pin will NOT wait for pout, keep pushing data.
				   * 1:  stop pushing when pin catches up with pout from loopback
				   */

	EGI_PLOG(LOGLV_INFO,"%s: An EGI_FIFO intialized successfully.\n", __func__);

	return efifo;
}


/*----------------------------------------------
Free a EGI_FIFO structure and its buff.
----------------------------------------------*/
void egi_free_fifo(EGI_FIFO *efifo )
{
	if( efifo != NULL)
	{
		/* free buff */
		if( efifo->buff != NULL )
		{
			printf("start egi_free_buff2D()...\n");
			egi_free_buff2D(efifo->buff, efifo->buff_size);
			//free(efifo->buff) and set efifo->buff=NULL in egi_free_buff2D() already
		}

		/* free itself */
		printf("%s:start free(efifo)...\n",__func__);
		free(efifo);
		efifo=NULL;	/* ineffective though...*/
	}
}


/*---------------------------------------------------------------------
Push data into buff.
1. Keep difference between pin and pout within buff_size.
2. If overrun occurs, reset pin to the same value as pout and
   start a new chase, also reset ahead to 0.

fifo:	  	a EGI_FIFO holding the buff.
data:	  	data to push to fifo.
size:	  	size of the data to push to fifo, in byte.
in,out:		pusher/puller position corresponding to the data[] slot
		!!!! BEFORE pin++/pout++ !!!!
ahd:    	current ahead mark.

Return:
	1	overrun;
	0	ok
	<0	fails
---------------------------------------------------------------------*/
int egi_push_fifo(EGI_FIFO *fifo,  unsigned char *data, int size,
				   int *in, int *out, int *ahd )
{

	/* check input param */
	if( fifo==NULL || fifo->buff==NULL )
	{
		EGI_PDEBUG(DBG_FIFO,"Input fifo or fifo buff is NULL! \n");
		return -1;
	}
	if( data==NULL)
	{
		EGI_PDEBUG(DBG_FIFO,"Input data is NULL! \n");
		return -2;
	}
	if( size <= 0 || size > fifo->item_size )
	{
		EGI_PDEBUG(DBG_FIFO,"Data size exceeds FIFO buffer item_size, or size<=0.\n");
		return -2;
	}

        /* get mutex lock */
        if(pthread_mutex_lock(&fifo->lock) != 0)
        {
                EGI_PLOG(LOGLV_CRITICAL,"%s: fail to get mutex lock.\n",__func__);
                return -3;
        }

 	/* >>>>>>>>>>>>>>>>>>>>   entering critical zone  >>>>>>>>>>>>>>>>>>>>> */

	/* Check if get end of the buff, loop back and increase 'ahead' */
	if( fifo->pin == fifo->buff_size )
	{
		fifo->pin=0;
		(fifo->ahead)++;

		/* this case shall be rare! */
		/* rare case that fifo->ahead=-2 before (fifo->ahead)++ */
		if( fifo->ahead > 1 || fifo->ahead < 0 )
			EGI_PLOG(LOGLV_WARN,"Pusher: fifo->ahead=%d, pin=%d, pout=%d \n",
								fifo->ahead, fifo->pin, fifo->pout);
	}

	/* Check if overrun occurs */
	/* NOTE:
         *    Start point: ahead=1, pin=item_size & pout=item_size.
	 *    Result: pin loopback and become 0(<pout), ahead=2. !!!
	 *    If pout stops at item_size and pin goes on and on, then the value of ahead will keep increasing,
	 *    So, It's an overrun!!! and we shall reset it when ahead==2 !!!!
	 */
	if( ((fifo->pin == fifo->pout) && (fifo->ahead)>0)  /* usually ahead==1 !!! */
	     || fifo->ahead > 1 ) /* in case ahead==2 */
	{

		/* reset fifo->pin to the save value of pout, start a new chase. */
		if(fifo->pout==fifo->buff_size) /* while pout==buff_size, assign 0 to pin instead of buff_size. or SEG FAULT!!! */
		{
			fifo->pin=0;
			EGI_PLOG(LOGLV_WARN, "FIFO Pusher: Overrun occurs! assign 0 to pin(%d), while pout==buff_size, ahead=%d \n",
											fifo->pin,fifo->ahead);
		}
		else
			fifo->pin=fifo->pout;


		/* going on or stop pushing depends on fifo->pin_wait */
		if(fifo->pin_wait)
		{
//			EGI_PLOG(LOGLV_WARN,"FIFO Pusher: Overrun occurs! pin=%d, pout=%d, ahead=%d \n",
//									fifo->pin, fifo->pout, fifo->ahead );
			pthread_mutex_unlock(&fifo->lock);
			return 1;
		}
		else {
			/* if going on, reset fifo then */
			fifo->ahead=0;
		}
	}

	/* Now, fifo->pin is no more than buff_size-1. */

	/* ----- FOR TEST ---------------- */
	if(fifo->pin==0)
	{
		if( ((*data)<<(32-8)) != 0 )
		{
			EGI_PLOG(LOGLV_ERROR,"FIFO ERROR!!! push data=%d is NOT N*512 when pin==0! pout=%d, ahead-%d\n",
								*data, fifo->pout, fifo->ahead );
			exit(0);
		}
	}
	/* ----- END OF TEST ------------ */

	/* Clear slot and push data */
	memset((fifo->buff)[fifo->pin],0,fifo->item_size);
	memcpy((fifo->buff)[fifo->pin],data,size); /* Not item_size however */
	/* pass out params */
	if(in != NULL)	*in=fifo->pin;
	if(out != NULL)	*out=fifo->pout;
	if(ahd != NULL) *ahd=fifo->ahead;

	/* increase pin at last */
	(fifo->pin) += 1;
	//printf("fifo->pin=%d\n",fifo->pin);

	pthread_mutex_unlock(&fifo->lock);
 	/* <<<<<<<<<<<<<<<<<<<<<<<<   leaving critical zone  <<<<<<<<<<<<<<<<<<<<<<<<  */


	return 0;
}


/*----------------------------------------------------------------------
Pull data from buff.
1. Keep difference between pin and pout within buff_size.
2. When underrun occurs, pout get the same positon as of pin,
   then it stops and waits pin to run ahead.

fifo:	a EGI_FIFO holding the buff.
data:	pointer to data pulled out from the fifo.
size:	size of the data to be extraced from the fifo, in byte.
	if size<=0 or >fifo->itemsize, then re_asign it to fifo->itemsize.
in,out:		pusher/puller position corresponding to the data[] slot
		!!!! BEFORE pin++/pout++ !!!!
ahd:    	current ahead mark.

Return:
	1	underrun;
	0	ok
	<0	fails
-------------------------------------------------------------------------*/
int egi_pull_fifo(EGI_FIFO *fifo, unsigned char *data, int size,
				   int *in, int *out, int *ahd )
{

	int item_size=size;

	/* check input param */
	if( fifo==NULL || fifo->buff==NULL )
	{
		EGI_PDEBUG(DBG_FIFO,"FIFO buff is NULL! \n");
		return -1;
	}
	/* check size */
	if( size <= 0 || size > fifo->item_size )
	{
		EGI_PDEBUG(DBG_FIFO,"Data size exceeds FIFO buffer item_size, or size<=0. reset to fifo->item_size.\n");
		item_size=fifo->item_size;
	}

        /* get mutex lock */
        if(pthread_mutex_lock(&fifo->lock) != 0)
        {
                EGI_PLOG(LOGLV_CRITICAL,"%s: fail to get mutex lock.\n",__func__);
                return -3;
        }

 	/* >>>>>>>>>>>>>>>>>>>>   entering critical zone  >>>>>>>>>>>>>>>>>>>>> */

	/* Check if get end of the buff, loop back and decrease 'ahead' */
	if( fifo->pout == fifo->buff_size )
	{
		fifo->pout=0;
		(fifo->ahead)--;

		/* this case shall never be expected */
		if(fifo->ahead > 1 || fifo->ahead < 0 )
			EGI_PLOG(LOGLV_WARN,"Puller: fifo->ahead=%d, pin=%d, pout=%d \n",
								fifo->ahead, fifo->pin, fifo->pout);
	}

	/* Check if underrun occurs */
	/* NOTE:
	 * Start Point: ahead=0, pin=item_size, pout=item_size.
	 * Result: pout loopback and becomes 0. ahead becomes -1.
	 * So, It's an underrun!!! and we shall stop pout.
	 */

	if( ( (fifo->pout >= fifo->pin) && (fifo->ahead == 0) )  /* usually pout==pin and ahead==0 */
		|| fifo->ahead < 0  )
	{
		EGI_PDEBUG(DBG_FIFO,"FIFO Puller: Output rate exceeds input, underrun occurrs!\n");
		/* return to wait pin */
		pthread_mutex_unlock(&fifo->lock);
		return 1;
	}

	/* Now, fifo->pout is no more than buff_size-1. */

	/* pull data */
	memcpy(data,(fifo->buff)[fifo->pout],item_size); /* Not fifo->item_size however */
	/* pass out params */
	if(in != NULL)	*in=fifo->pin;
	if(out != NULL)	*out=fifo->pout;
	if(ahd != NULL) *ahd=fifo->ahead;

	/* increase pin at last */
	(fifo->pout) += 1;
	//printf("fifo->pout=%d\n",fifo->pout);

	pthread_mutex_unlock(&fifo->lock);
 	/* <<<<<<<<<<<<<<<<<<<<<<<<   leaving critical zone  <<<<<<<<<<<<<<<<<<<<<<<<  */

	return 0;
}
