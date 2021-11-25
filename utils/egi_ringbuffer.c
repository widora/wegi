/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple ring buffer.

Journal:
2021-11-15:
	1. Create the file and functions:
		egi_ringbuffer_create() egi_ringbuffer_free()
		egi_ringbuffer_write()	egi_ringbuffer_read()
2021-11-19:
	1. Add  egi_ringbuffer_IsFull()

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <egi_debug.h>
#include <egi_ringbuffer.h>

/*-------------------------------------------
Create an EGI ring buffer

@buffsize:	Size of the ring buffer.

Return:
Pointer to EGI_RINGBUFFER	OK
NULL				Fails
-------------------------------------------*/
EGI_RINGBUFFER *egi_ringbuffer_create(size_t buffsize)
{
	EGI_RINGBUFFER *ringbuf=calloc(1, sizeof(EGI_RINGBUFFER));
	if(ringbuf==NULL) {
		egi_dperr("Fail to calloc ringbuf!");
		return NULL;
	}

	ringbuf->buffer=calloc(1, buffsize);
	if(ringbuf->buffer==NULL) {
		egi_dperr("Fail to calloc ringbuf->buffer!");
		free(ringbuf);
		return NULL;
	}
	ringbuf->buffsize=buffsize;

        if(pthread_mutex_init(&ringbuf->ring_mutex,NULL) != 0)
        {
		egi_dperr("Fail init ring_mutex!");
		free(ringbuf->buffer);
                free(ringbuf);
                return NULL;
        }

	return ringbuf;
}

/*-------------------------------------------
Free an EGI ring buffer.
---------------------------------------------*/
void egi_ringbuffer_free(EGI_RINGBUFFER **ringbuf)
{
	if(ringbuf==NULL || *ringbuf==NULL)
		return;

        /* Hope there is no other user */
        if(pthread_mutex_lock(&(*ringbuf)->ring_mutex) !=0 )
		egi_dperr("Fail lock ring_mutex");

#if 1   /*  "It shall be safe to destroy an initialized mutex that is unlocked. Attempting to
         *   destroy a locked mutex results in undefined behavior"  --- POSIX man/Linux man 3
         */
        if( pthread_mutex_unlock(&(*ringbuf)->ring_mutex) != 0)
		egi_dperr("Fail unlock ring_mutex");
#endif

        /* Destroy thread mutex lock  */
        if(pthread_mutex_destroy(&(*ringbuf)->ring_mutex) !=0 )
		egi_dperr("Fail destroy ring_mutex");

	/* Free mem */
	free((*ringbuf)->buffer);
	free(*ringbuf);

	*ringbuf=NULL;
}

/*-----------------------------------------------
Check if the RingBuffer is full, withou accessing
mutex lock.

@ringbuf:	Pointer to an ring buffer.

Return:
	Ture OR False
-----------------------------------------------*/
inline bool egi_ringbuffer_IsFull(const EGI_RINGBUFFER *ringbuf)
{
	return ( ringbuf && (ringbuf->datasize==ringbuf->buffsize) );
}

/*-------------------------------------------
Wrtie data into a ring buffer.

@ringbuf:	Pointer to an ring buffer.
@src:		Soure of data.
@len:		Length of source data.

Note:
1. Assume pw will NOT surpass pr!

Return:
	>0	Actual size of data written
	=0	Ringbuffer is full.
		OR len==0.
	<0	Fails.
-------------------------------------------*/
size_t egi_ringbuffer_write(EGI_RINGBUFFER *ringbuf, void *src, size_t len)
{
	int ret=0;
	size_t lenx=0;

	if(ringbuf==NULL || src==NULL)
		return -1;

        /* Get mutex lock */
        if(pthread_mutex_lock(&ringbuf->ring_mutex) !=0 ){
		egi_dperr("Fail to lock ring_mutex!");
		return -1;
        }
 /* ------ >>>  Critical Zone  */

	/* Check available space in the buffer. */

	/* Case_1. Ringbuffer is full! (pw==pr) */
	if( ringbuf->datasize == ringbuf->buffsize ) {
		/* Check */
		if(ringbuf->pw!=ringbuf->pr) {
			egi_dpstd("ERROR! datasize==buffsize while pw!=pr !\n");
			//return -2;
			ret=-2; goto END_FUNC;
		}
		else {
			//return 0;
			ret=0; goto END_FUNC;
		}
	}

	/* Case_2. Ringbuffer has enough space:  */
	if( ringbuf->datasize + len <= ringbuf->buffsize ) {
		/* 2.1 Pw is not necessary to cross the ring END/HEAD */
		if( ringbuf->buffsize - ringbuf->pw >= len ) {
			memcpy(ringbuf->buffer+ringbuf->pw, src, len);
			/* update pw */
			ringbuf->pw += len;
			//ringbuf->pw=ringbuf->pw%ringbuf->buffsize;
			if(ringbuf->pw==ringbuf->buffsize)
				ringbuf->pw=0;
			/* update datasize */
			ringbuf->datasize +=len;

			//return len;
			ret=len; goto END_FUNC;
		}
		/* 2.2 pw is to cross the ring END/HEAD */
		else {
			/* Write to buffer END/HEAD */
			lenx = ringbuf->buffsize-ringbuf->pw;
			memcpy(ringbuf->buffer+ringbuf->pw, src, lenx);
			/* Write remaining from HEAD/END */
			memcpy(ringbuf->buffer+0, src+lenx, len-lenx);
			/* Update pw */
			ringbuf->pw=len-lenx;
			/* Update datasize */
			ringbuf->datasize +=len;

			//return len;
			ret=len; goto END_FUNC;
		}
	}
	/* Case_3. Ringbuffer has NOT enough space: */
	else  { /* if( ringbuf->datasize + len > ringbuf->buffsize ) */
		/* 3.1 Pw is not necessary to cross the ring head/end */
		if( ringbuf->pw < ringbuf->pr ) {
			/* Write till to pr */
			lenx=ringbuf->pr-ringbuf->pw;
			memcpy(ringbuf->buffer+ringbuf->pw, src, lenx);

			/* Update pw */
			ringbuf->pw = ringbuf->pr;
			/* update datasize */
			ringbuf->datasize +=lenx;

			//return lenx;
			ret=lenx; goto END_FUNC;
		}
		/* 3.2 pw is to cross the ring head/end */
		else {  /* pw > pr */
			/* Write till to END/HEAD */
			lenx=ringbuf->buffsize-ringbuf->pw;
			memcpy(ringbuf->buffer+ringbuf->pw, src, lenx);
			/* Write remaining from HEAD/END to pr */
			memcpy(ringbuf->buffer+0, src+lenx, ringbuf->pr);

			/* Update pw */
			ringbuf->pw = ringbuf->pr;
			/* update datasize */
			ringbuf->datasize +=(lenx+ringbuf->pr);

			//return lenx+ringbuf->pr;
			ret=lenx+ringbuf->pr; goto END_FUNC;
		}
	}

END_FUNC:
/* ------- <<<  Critical Zone  */
        /* put mutex lock */
        pthread_mutex_unlock(&ringbuf->ring_mutex);

	return ret;
}

/*-------------------------------------------
Read data out from a ring buffer.

@ringbuf:	Pointer to an ring buffer.
@dest:		Destination of data.
@len:		Expected length of data to read.

Note:
1. Assume pw will NOT surpass pr!

Return:
	>0	Actual size of data read out.
	=0	Ringbuffer is Empty.
		OR len==0.
	<0	Fails.
-------------------------------------------*/
size_t egi_ringbuffer_read(EGI_RINGBUFFER *ringbuf, void *dest, size_t len)
{
	int ret=0;
	size_t lenx=0;

	if(ringbuf==NULL || dest==NULL)
		return -1;

        /* Get mutex lock */
        if(pthread_mutex_lock(&ringbuf->ring_mutex) !=0 ){
		egi_dperr("Fail to lock ring_mutex!");
		return -1;
        }
 /* ------ >>>  Critical Zone  */

	/* Check available space in the buffer. */

	/* Case_1. Ringbuffer has Empty data! (pw==pr) */
	if( ringbuf->datasize==0 ) {
		/* Check */
		if(ringbuf->pw!=ringbuf->pr) {
			egi_dpstd("ERROR! datasize==0 while pw!=pr !\n");
			//return -2;
			ret=-2; goto END_FUNC;
		}
		else {
			//return 0;
			ret=0; goto END_FUNC;
		}
	}

	/* Case_2. Ringbuffer has enough data:  */
	if( ringbuf->datasize >= len ) {
		/* 2.1 Pr is not necessary to cross the ring END/HEAD */
		if( ringbuf->buffsize - ringbuf->pr >= len ) {
			memcpy(dest, ringbuf->buffer+ringbuf->pr, len);
			/* update pr */
			ringbuf->pr += len;
			//ringbuf->pr=ringbuf->pr%ringbuf->buffsize;
			if(ringbuf->pr==ringbuf->buffsize)
				ringbuf->pr=0;
			/* update datasize */
			ringbuf->datasize -=len;

			//return len;
			ret=len; goto END_FUNC;
		}
		/* 2.2 pr is to cross the ring END/HEAD */
		else {
			/* Read till to buffer END/HEAD */
			lenx = ringbuf->buffsize-ringbuf->pr;
			memcpy(dest, ringbuf->buffer+ringbuf->pr, lenx);
			/* Read remaining from HEAD/END */
			memcpy(dest+lenx, ringbuf->buffer+0, len-lenx);
			/* Update pr */
			ringbuf->pr=len-lenx;
			/* Update datasize */
			ringbuf->datasize -=len;

			//return len;
			ret=len; goto END_FUNC;
		}
	}
	/* Case_3. Ringbuffer has NOT enough data: */
	else  { /* if( ringbuf->datasize<len ) */
		/* 3.1 pr is NOT necessary to cross the ring head/end */
		if( ringbuf->pr < ringbuf->pw ) {
			/* Read till to pw */
			lenx=ringbuf->pw-ringbuf->pr;
			memcpy(dest, ringbuf->buffer+ringbuf->pr, lenx);

			/* Update pr */
			ringbuf->pr = ringbuf->pw;
			/* update datasize */
			ringbuf->datasize -=lenx;

			//return lenx;
			ret=lenx; goto END_FUNC;
		}
		/* 3.2 pr is to cross the ring head/end */
		else {  /* pr > pw */
			/* Write till to END/HEAD */
			lenx=ringbuf->buffsize-ringbuf->pr;
			memcpy(dest, ringbuf->buffer+ringbuf->pr, lenx);
			/* Write remaining from HEAD/END to pw */
			memcpy(dest, ringbuf->buffer+0, ringbuf->pw);

			/* Update pr */
			ringbuf->pr = ringbuf->pw;
			/* update datasize */
			ringbuf->datasize -=(lenx+ringbuf->pw);

			//return lenx+ringbuf->pw;
			ret=lenx+ringbuf->pw; goto END_FUNC;
		}
	}

END_FUNC:
/* ------- <<<  Critical Zone  */
        /* put mutex lock */
        pthread_mutex_unlock(&ringbuf->ring_mutex);

	return ret;
}
