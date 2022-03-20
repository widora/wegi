/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Journal:
	2021-11-15: Create the file.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include "egi_ringbuffer.h"

int main(void)
{
	char data[100];
	int inBytes=100; /* 每次写入数据量 */
	int outBytes=50;  /* 每次读出数据量 */
	int ret;

	EGI_RINGBUFFER *ringbuff=NULL;
	ringbuff=egi_ringbuffer_create(4096);

   while(1) {

	/* Write into the ringbuffer */
	if( (ret=egi_ringbuffer_write(ringbuff, data, inBytes)) <inBytes ) {
		printf("Ringbuffer overrun... write %d of %d Bytes.\n", ret,inBytes);
		/* Adjust speed */
		inBytes=50;
		outBytes=100;
		sleep(1);
	}

	/* Read out from the ringbuffer */
	if( (ret=egi_ringbuffer_read(ringbuff, data, outBytes)) <outBytes ) {
		printf("Ringbuffer underrun... read %d of %d Bytes.\n", ret,outBytes);
		/* Adjust speed */
		inBytes=100;
		outBytes=50;
		sleep(1);
	}

	printf("Ringbuff datasize=%zu, pw=%jd, pr=%jd\n", ringbuff->datasize, ringbuff->pw, ringbuff->pr);
   }

	egi_ringbuffer_free(&ringbuff);
	return 0;
}
