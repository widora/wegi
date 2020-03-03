/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "egi_shmem.h"

/*------------------------------------------------------------
Open shared memory and get msg_data with mmap.
If the named memory dosen't exist, then create it first.

@shmem:	A struct pointer to EGI_SHMEM.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------*/
int egi_shmem_open(EGI_SHMEM *shmem)
{
        int shmfd;

	/* check input */
	if(shmem==NULL||shmem->shm_name==NULL)
		return -1;

	if(shmem->shm_map >0 ) {   /* MAP_FAILED==(void *)-1 */
		printf("%s: shmem already mapped!\n",__func__);
		return -2;
	}

	/* open shm */
	shmfd=shm_open(shmem->shm_name, O_CREAT|O_RDWR, 0666);
        if(shmfd<0) {
                printf("%s: fail shm_open(): %s",__func__, strerror(errno));
		return -3;
        }

        /* resize */
        if( ftruncate(shmfd, shmem->shm_size) <0 ) { /* page size */
                printf("%s: fail ftruncate(): %s",__func__, strerror(errno));
		return -4;
	}

        /* mmap */
        shmem->shm_map=mmap(NULL, shmem->shm_size, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
        if( shmem->shm_map==MAP_FAILED ) {
                printf("%s: fail mmap(): %s",__func__, strerror(errno));
		return -5;
        }

	/* get msg_data pointer */
	shmem->msg_data=(typeof(shmem->msg_data))(shmem->shm_map);


	return 0;
}


/*------------------------------------------------------------
munmap msg_data in a EGI_SHMEM. but DO NOT unlink the shm_name.

@shmem:	A struct pointer to EGI_SHMEM.

Return:
	0	OK
	<0	Fails
-------------------------------------------------------------*/
int egi_shmem_close(EGI_SHMEM *shmem)
{
        /* check input */
        if(shmem==NULL || shmem->shm_name==NULL)
                return -1;

        if(shmem->shm_map <=0 ) {   /* MAP_FAILED==(void *)-1 */
                printf("%s: shmem already unmapped!\n",__func__);
                return -2;
        }

	/* reset msg_data */
	shmem->msg_data=NULL;

	/* unmap shm_map */
        if( munmap(shmem->shm_map, shmem->shm_size)<0 ) {
                printf("%s: fail munmap(): %s",__func__, strerror(errno));
		return -3;
	}

	return 0;
}


/*----------------------------------------------------------------------------
Removes a shared memory object name. once all processes have unmapped  the
object, de-allocates and destroys the contents of the associated memory region.

@name:	Name of the shared mem.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------*/
int egi_shmem_remove(const char *name)
{
	int err;

	err=shm_unlink(name);

	if(err!=0)
		printf("%s: %s\n",__func__, strerror(errno));

	return err;
}
