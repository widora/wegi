/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
------------------------------------------------------------------*/
#ifndef __EGI_SHMEM__
#define __EGI_SHMEM__

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
	const char* shm_name;
	char* shm_map;
	size_t shm_size;

	/* DATA shm_map, struct pointer to shm_map */
	struct {
		//pthread_mutex_t shm_mutex;	/* Ensure no race in first mutex create */
        	bool    active;			/* status to indicate that data is being processed.
						 * The process occupying the data may quit abnormally, so other processes shall confirm
						 * this status by other means.
						 */
		bool 		sigstop;	/* siganl to stop data processing */
        	int     	signum;		/* signal number */
		char    	msg[64];
		void*		data;		/* more data, pointer value, !!! AS FOR VALUE, NOT FOR ADDR !!! */
	}* msg_data;

	/* Offset to somewhere in mem block */
	unsigned int	offset;		/* data block offset from shm_map */

} EGI_SHMEM;


int egi_shmem_open(EGI_SHMEM *shmem);
int egi_shmem_close(EGI_SHMEM *shmem);
int egi_shmem_remove(const char *name);

#endif
