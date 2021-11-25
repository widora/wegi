/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A simple ring buffer.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef __EGI_RINGBUFFER_H__
#define __EGI_RINGBUFFER_H__
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	char*	 buffer;	/* Mem. space allocated */
	size_t	 buffsize;	/* Size of the ring buffer */
	size_t   datasize;	/* Size of available data buffered */
	off_t	 pw;		/* Offset to write position (Write pointer) */
	off_t    pr;		/* Offset to read position (Read pointer) */

	pthread_mutex_t ring_mutex;
} EGI_RINGBUFFER;

EGI_RINGBUFFER *egi_ringbuffer_create(size_t buffsize);
void egi_ringbuffer_free(EGI_RINGBUFFER **ringbuf);
bool egi_ringbuffer_IsFull(const EGI_RINGBUFFER *ringbuf);
size_t egi_ringbuffer_write(EGI_RINGBUFFER *ringbuf, void *src, size_t len);
size_t egi_ringbuffer_read(EGI_RINGBUFFER *ringbuf, void *dest, size_t len);

#endif
