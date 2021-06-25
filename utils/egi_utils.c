/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Utility functions, mem.

Jurnal
2021-02-05:
	1. Modify egi_fmap_resize(): Must re_mmap after resize the fiel.
2021-02-09:
	1. egi_fmap_create(): To fail if file size is 0!
2021-02-13:
	1. egi_fmap_create(): Add flock(,LOCK_EX) during mmaping.
2021-06-07:
	1. Add egi_lock_pidfile().
2021-06-22:
	1. Add egi_valid_fd().

Midas Zhou
midaszhou@yahoo.com
-----------------------------------------------------------------*/
#include "egi_utils.h"
#include "egi_debug.h"
#include "egi_log.h"
#include "egi_math.h"
#include "egi_cstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <libgen.h>
#include <sys/file.h>


/*---------------------------------
Free a char pointer and reset
it to NULL
----------------------------------*/
inline void egi_free_char(char **p)
{
	if( p!=NULL && *p!=NULL ) {
		free(*p);
		*p=NULL;
	}
}


/*--------------------------------------------------------
Reallocate more memory space for the pointed memory block.

@ptr:	    *Pointer to memory block.
@old_size:  Original size of the memory block, in bytes.
@more_size: More size for the memory block, in bytes.

Return:
	0	OK
	<0	Fails, the original memory block left untouched.
-----------------------------------------------------------*/
int egi_mem_grow(void **ptr, size_t old_size, size_t more_size)
{
	void *ptmp;

	if( (ptmp=realloc( *ptr, old_size+more_size)) == NULL )
		return -1;
	else
		*ptr=ptmp;

	/* realloc will not initiliaze added memory */
	memset( ptmp+old_size, 0, more_size);

	return 0;
}


/*-----------------------------------------------------------
Mmap a file and return a EGI_FILEMMAP.
If the file dose NOT exis, then create it first.
If resize>0, then truncated to the size, extended part reads
as null bytes ('\0').

Note:
1. For 32-bits type off_t, mmap Max. file size <2G !
   off_t: 32bits, Max. fsize:  2^31-1
   off_t: 64bits, Max. fsize:  2^63-1
2. mmap with flags: PROT_READ, MAP_PRIVATE
3. The file must NOT be empty.

@fpath:	  File path
@resize:  resize
	  if>0, the file will be truncated to the size.
@prot:	  memory protection of the mapping (and must not conflict
	  with the open mode of the file).
	  Example: PROT_READ, PROT_WRITE
	  If PROT_WRITE is not set, then write to fmap will result
	  in segmentation fault!

@flag:	  MAP_SHARED or MAP_PRIVATE

Return:
	Pointer to EGI_FILEMMAP		OK
	NULL				Fails
----------------------------------------------------------*/
EGI_FILEMMAP * egi_fmap_create(const char *fpath, off_t resize, int prot, int flags)
{
        struct stat	sb;
	EGI_FILEMMAP *fmap=NULL;

	/* Calloc fmap */
	fmap=calloc(1,sizeof(EGI_FILEMMAP));
	if(fmap==NULL)
		return NULL;

        /* Open file */
        fmap->fd=open(fpath, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
        if(fmap->fd<0) {
                printf("%s: Fail to open input file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
		free(fmap);
                return NULL;
        }

        /* Get advisory lock */
        if( flock(fmap->fd,LOCK_EX) !=0 ) {
                EGI_PLOG(LOGLV_ERROR, "%s: Fail to flock '%s', Err'%s'.", __func__, fpath, strerror(errno));
                /* Go on .. */
        }

        /* Obtain file stat */
        if( fstat(fmap->fd, &sb)<0 ) {
                printf("%s: Fail call fstat for file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                goto END_FUNC;
        }

	/* Check size */
        fmap->fsize=sb.st_size;
	if( fmap->fsize <= 0 ) {
		printf("%s: file size is 0! Fail to mmap '%s'!\n", __func__, fpath);
		goto END_FUNC;
	}

	if( resize>0 ) {
		/* Resize the file */
		if( ftruncate(fmap->fd, resize) !=0 ) {
	                printf("%s: ftruncate '%s' fails. ERR:%s\n", __func__, fpath, strerror(errno));
        	        goto END_FUNC;
		}

		/* Obtain new size */
	        if( fstat(fmap->fd, &sb)<0 ) {
                	printf("%s: Fail to obtain expanded size for '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                	goto END_FUNC;
		}
		fmap->fsize=sb.st_size;
		//printf("%s: Succeed to create and/or ftruncate '%s' to %jd Bytes.\n", __func__, fpath, fmap->fsize);
        }

        /* MMAP Text file */
        fmap->fp=mmap(NULL, fmap->fsize, prot, flags, fmap->fd, 0);
        if(fmap->fp==MAP_FAILED) {
                        printf("%s: Fail to mmap file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                        goto END_FUNC;
         }

	/* FOR PRO_READ/MAP_PRIVATE only!:
   	 * Actually we can close(fd) now, instead of closing it in egi_fmap_free().
	 * Anyway, leave it to egi_fmap_free().. NOPE also for egi_fmap_resize()!
	 */

	/* Assign prot and flags */
	fmap->prot=prot;
	fmap->flags=flags;


END_FUNC:
        /* Unlock, advisory locks only */
        if( flock(fmap->fd,LOCK_UN) !=0 ) {
                printf("%s: Fail to un_flock '%s', Err'%s'\n.", __func__, fpath, strerror(errno));
                /* Go on .. */
        }

        /* Munmap file */
        if( fmap->fp==NULL || fmap->fp==MAP_FAILED ) {
	        if( close(fmap->fd) !=0 )
        	        printf("%s: Fail to close file '%s'. ERR:%s!\n", __func__, fpath, strerror(errno));
		free(fmap);
		return NULL;
	}
	else
		return fmap;
}

/*------------------------------------------------
Adjust size of the file that fmap underlies.

@fmap:		an EGI_FILEMMAP
@resize:	New size of fmap->fp.

Return:
	0	OK
	<0	Fail
--------------------------------------------------*/
int egi_fmap_resize(EGI_FILEMMAP* fmap, off_t resize)
{
	if(fmap==NULL || fmap->fp==NULL)
		return -1;

        /* Munmap file */
        if( munmap(fmap->fp,fmap->fsize) !=0 ) {
        	printf("%s: Fail to munmap fd=%d. Err'%s'!\n",__func__, fmap->fd, strerror(errno));
		return -1;
	}
	fmap->fp=NULL;

	/* Resize file */
        if( ftruncate(fmap->fd, resize) !=0 ) {
                printf("%s: ftruncate fails. ERR:%s\n", __func__, strerror(errno));
		return -2;
        }
        fmap->fsize=resize;

        /* Re_mmap the file */
        fmap->fp=mmap(NULL, fmap->fsize, fmap->prot, fmap->flags, fmap->fd, 0);
        if(fmap->fp==MAP_FAILED) {
                        printf("%s: Fail to re_mmap the file. ERR:%s\n", __func__, strerror(errno));
			return -3;
        }

	return 0;
}

/*-----------------------------------------------------------------
"msync()  flushes  changes made to the in-core copy of a file that
was mapped into memory using mmap(2) back to the filesystem.
Without use of this call, there is no guarantee that changes are
written back before munmap(2) is called." --man msync

Return:
	0	OK
	<0	Fails
---------------------------------------------------------------*/
int egi_fmap_msync(EGI_FILEMMAP* fmap)
{
	int ret;
	ret=msync(fmap->fp, fmap->fsize, MS_SYNC); //MS_INVALIDATE
	if(ret!=0) {
		printf("%s: msync Err'%s'.\n", __func__,strerror(errno));
		return ret;
	}
	else
		return 0;
}

/*--------------------------------------------
Release an EGI_FILEMMAP, unmap and close the
underlying file, free the struct.

Return:
	0	OK
	<0	Fail
--------------------------------------------*/
int egi_fmap_free(EGI_FILEMMAP** fmap)
{
	if(fmap==NULL || *fmap==NULL)
		return -1;

        /* Munmap file */
        if( munmap((*fmap)->fp,(*fmap)->fsize) !=0 ) {
        	printf("%s: Fail to munmap fd=%d. Err'%s'!\n",__func__, (*fmap)->fd, strerror(errno));
		return -2;
	}

    	/* Close file */
        if( close((*fmap)->fd) !=0 ) {
                printf("%s: Fail to close file fd=%d. Err'%s!'\n",__func__, (*fmap)->fd, strerror(errno));
		return -3;
	}

	free(*fmap);
	*fmap=NULL;

	return 0;
}


/*----------------------------------------------------------------------------
Search a string in a file. If find, return offset value from the mmap.

@fpath:		Full path to a text file.
@off:		Offset value from mmap, as start position for a search.
@pstr:		A pointer to a string.

Return:
	>=0	OK, offset value from mmap, as the beginning of the first located
		string.
	<0	Fails
-------------------------------------------------------------------------------*/
int egi_search_str_in_file(const char *fpath, size_t off, const char *pstr)
{
	int 		ret;
        int             fd;
        int             fsize=-1;  /* AS token */
        struct stat	sb;
        char *    	fp=NULL;
	const char *	pc=NULL;

        /* Open text file */
        fd=open(fpath, O_RDONLY, S_IRUSR);
        if(fd<0) {
                printf("%s: Fail to open input file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                return -2;
        }

        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
                printf("%s: Fail call fstat for file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                ret=-3;
                goto END_FUNC;
        }

	/* Check size */
        fsize=sb.st_size;
	if(fsize <=0) {
		ret=-3;
		goto END_FUNC;
	}

	/* Check off */
	if( off > fsize-1 ) {
		ret=-4;
		goto END_FUNC;
	}

        /* MMAP Text file */
        if(fsize >0) {
                fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
                if(fp==MAP_FAILED) {
                        printf("%s: Fail to mmap file '%s'. ERR:%s\n", __func__, fpath, strerror(errno));
                        ret=-5;
                        goto END_FUNC;
                }
        }

	/* Search substring */
	pc=strstr(fp,pstr);
	if(pc==NULL) {
                printf("%s: Substring '%s' NOT found in file '%s'!\n", __func__, pstr, fpath);
		ret=-6;
		goto END_FUNC;
	}
	else /* As offset value to the mmap */
		ret=pc-fp;

END_FUNC:
        /* Munmap file */
        if( fsize>0 && fp!=MAP_FAILED ) {
                if(munmap(fp,fsize) !=0 )
                        printf("%s: Fail to munmap file '%s'. ERR:%s!\n",__func__, fpath, strerror(errno));
        }

        /* Close File */
        if( close(fd) !=0 )
                printf("%s: Fail to close file '%s'. ERR:%s!\n",__func__, fpath, strerror(errno));


	return ret;
}


/*----------------------------------------------------------
Copy pstr pointed content to a file. If the file dose NOT
exist, then create it first. New content are writen/appended
to the end of the file.

@fpath: 	Full file path name.
@pstr:		Pointer to content for copying to the file
@size: 		Size for copy_to  operation, in bytes.
@endtok:	An end token. If =0, ignore.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------*/
int egi_copy_to_file( const char *fpath, const unsigned char *pstr, size_t size, char endtok)
{
	FILE *fil=NULL;
	int nwrite;
	int ret=0;

	if(pstr==NULL)
		return -1;

	if( (fil=fopen(fpath,"ae"))==NULL) {
		printf("%s: Fail to open or create '%s' for copy_to operation, %s!\n",__func__, fpath, strerror(errno));
		return -2;
	}

        nwrite=fwrite(pstr, 1, size, fil);
	if(endtok) {
		nwrite += fwrite(&endtok, 1, 1, fil);
		size += 1;
	}
        if(nwrite < size) {
                if(ferror(fil))
                        printf("%s: Fail to copy to '%s'.\n", __func__, fpath);
                else
                        printf("%s: WARNING! fwrite %d bytes of total %d bytes to '%s'.\n", __func__, nwrite, size, fpath);
                ret=-3;
        }
        EGI_PDEBUG(DBG_CHARMAP,"%d bytes copy_to '%s'.\n", nwrite, fpath);

        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close '%s': %s\n", __func__, fpath, strerror(errno));
                ret=-4;
        }

	return ret;
}


/*------------------------------------------------------------
Copy pstr pointed content to EGI SYSPAD, which is acutually
a file. It truncates the file to zero length first,
so old data will be lost.

@pstr:	Pointer to content for copying to EGI_SYSPAD
@size: 	Size for copy_to  operation, in bytes.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------*/
int egi_copy_to_syspad( const unsigned char *pstr, size_t size)
{
	FILE *fil=NULL;
	int nwrite;
	int ret=0;

	if(pstr==NULL)
		return -1;

	if( (fil=fopen(EGI_SYSPAD_PATH,"we"))==NULL) {
		printf("%s: Fail to open EGI SYSPAD '%s' for copy_to operation!\n",__func__, EGI_SYSPAD_PATH);
		return -2;
	}

        nwrite=fwrite(pstr, 1, size, fil);
        if(nwrite < size) {
                if(ferror(fil))
                        printf("%s: Fail to copy to EGI_SYSPAD '%s'.\n", __func__, EGI_SYSPAD_PATH);
                else
                        printf("%s: WARNING! fwrite %d bytes of total %d bytes to EGI SYSPAD.\n", __func__, nwrite, size);
                ret=-3;
        }
        EGI_PDEBUG(DBG_CHARMAP,"%d bytes copy_to EGI SYSPAD.\n", nwrite);

        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close EGI SYSPAD '%s': %s\n", __func__, EGI_SYSPAD_PATH, strerror(errno));
                ret=-4;
        }

	return ret;
}


/*----------------------------------------------
Copy content in EGI_SYSPAD to pstr pointed mem.

WARNING! The caller must ensure enough mem space
pointed by pstr.

@pstr:	Pointer to receiving buffer.

Return:
	0	OK
	<0	Fails
----------------------------------------------*/
int egi_copy_from_syspad(unsigned char *pstr)
{
	FILE *fil=NULL;
	int nread;
	int ret=0;
	int fsize;
	struct stat sb;

	if(pstr==NULL)
		return -1;

	if( (fil=fopen(EGI_SYSPAD_PATH,"re"))==NULL) {
		printf("%s: Fail to open EGI SYSPAD '%s' for copy_from operation!\n",__func__, EGI_SYSPAD_PATH);
		return -2;
	}

        /* Obtain file stat */
        if( fstat( fileno(fil), &sb )<0 ) {
                printf("%s: Fail call fstat for EGI_SYSPAD '%s': %s\n", __func__, EGI_SYSPAD_PATH, strerror(errno));
        	if( fclose(fil) !=0 )
                	printf("%s: Fail to close EGI SYSPAD '%s': %s\n", __func__, EGI_SYSPAD_PATH, strerror(errno));
                return -3;
        }
        fsize=sb.st_size;

	/* Read content */
        nread=fread(pstr, 1, fsize, fil);
        if(nread < fsize) {
                if(ferror(fil))
                        printf("%s: Fail to copy from EGI_SYSPAD '%s'.\n", __func__, EGI_SYSPAD_PATH);
                else
                        printf("%s: WARNING! fread %d bytes of total %d bytes from EGI SYSPAD.\n", __func__, nread, fsize);
                ret=-3;
        }
        EGI_PDEBUG(DBG_CHARMAP,"%d bytes cop_to EGI SYSPAD.\n", nread);

        /* Close fil */
        if( fclose(fil) !=0 ) {
                printf("%s: Fail to close EGI SYSPAD '%s': %s\n", __func__, EGI_SYSPAD_PATH, strerror(errno));
                ret=-4;
        }

	return ret;
}


/*-----------------------------------------------
Create an EGI_SYSPAD_BUFF struct

@size:	Size of buffer.

Return:
	A pointer to EGI_SYSPAD_BUFF	OK
	NULL				Fails
------------------------------------------------*/
EGI_SYSPAD_BUFF *egi_sysPadBuff_create(size_t size)
{
	EGI_SYSPAD_BUFF *padbuff=NULL;

	if(size<=0)
		return NULL;

	padbuff=calloc(1, sizeof(EGI_SYSPAD_BUFF));
	if(padbuff==NULL) {
		printf("%s: Fail to calloc padbuff!\n",__func__);
		return NULL;
	}

	padbuff->data=calloc(1, sizeof(typeof(*padbuff->data))*size);
	if(padbuff->data==NULL) {
		printf("%s: Fail to calloc padbuff->data!\n",__func__);
		free(padbuff);
		return NULL;
	}

	padbuff->size=size;

	return padbuff;
}

/*-----------------------------------------------
	Free an EGI_SYSPAD_BUFF struct
------------------------------------------------*/
void egi_sysPadBuff_free(EGI_SYSPAD_BUFF **padbuff)
{
	if(padbuff==NULL || *padbuff==NULL)
		return;

	free( (*padbuff)->data );
	free(*padbuff);

	*padbuff=NULL;
}


/*----------------------------------------------
Copy content in EGI_SYSPAD to an EG_SYSPAD_BUFF.

Return:
	An pointer to EGI_SYSPAD_BUFF	OK
	NULL				Fails
----------------------------------------------*/
EGI_SYSPAD_BUFF *egi_buffer_from_syspad(void)
{
	EGI_SYSPAD_BUFF *padbuff=NULL;
	int fd;
	char *fp=MAP_FAILED; /* as token */
	int fsize;
	struct stat sb;

        /* Mmap input file */
        fd=open(EGI_SYSPAD_PATH, O_CREAT|O_RDWR|O_CLOEXEC, S_IRWXU|S_IRWXG);
        if(fd<0) {
                printf("%s: Fail to open EGI_SYSPAD_PATH '%s': %s\n", __func__, EGI_SYSPAD_PATH, strerror(errno));
                return NULL;
        }
        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
                printf("%s: Fail to get fstat for EGI_SYSPAD_PATH: %s\n", __func__, strerror(errno));
		goto FUNC_END;
        }
	/* Get file size */
        fsize=sb.st_size;

        /* Only if fsize>0: mmap SYSPAD file */
        if(fsize >0) {
                fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
                if(fp==MAP_FAILED) {
                        printf("%s: Fail to mmap EGI_SYSPAD_PATH: %s\n", __func__, strerror(errno));
			goto FUNC_END;
		}
        }

	/* Create EGI_SYSPAD_BUFF */
	padbuff=egi_sysPadBuff_create(fsize);
	if(padbuff==NULL) {
		printf("%s: Fail to create padbuff!\n",__func__);
		goto FUNC_END;
	}

	/* TODO: verify uft-8 encoding for padbuff->data */
	/* Buffer content to padbuff */
	memcpy( padbuff->data, fp, fsize);

FUNC_END:
        /* munmap file */
        if( fp!=MAP_FAILED ) {
		if( munmap(fp,fsize) !=0 )
                	printf("%s: Fail to munmap SYSPAD file '%s': %s!\n",__func__, EGI_SYSPAD_PATH, strerror(errno));
	}
        if( close(fd) !=0 )
                printf("%s: Fail to close SYSPAD file '%s': %s!\n",__func__, EGI_SYSPAD_PATH, strerror(errno));

	return padbuff;
}


/*---------------------------------------------
Shuffle an integer array.
@size:	size of array.
@in:	Input array
@out:	Shuffled array.

Return:
	0	OK
	<0	Fails
---------------------------------------------*/
int egi_shuffle_intArray(int *array, int size)
{
	int i;
	int index;
	int *tmp=NULL;

	if(array==NULL || size < 1)
		return -1;

	/* Calloc tmp */
	tmp=calloc(1,size*sizeof(int));
	if(tmp==NULL) {
		printf("%s: Fail to calloc tmp.\n",__func__);
		return -2;
	}

	/* Copy original array to tmp */
	for(i=0; i<size; i++)
		tmp[i]=array[i];

	/* shuffle index of tmp[], then copy tmp[index] to array[] */
	for( i=0; i<size; i++ ) {
		/* if last one */
		if(i==size-1) {
			array[i]=tmp[0];
			break;
		}
		/* Shuffle index, and assign tmp[index] to array[i] */
		index=mat_random_range(size-i);
		array[i]=tmp[index];

		/* Fill up empty tmp[index] slot by moving followed members forward */
		if( index != size-i-1) /* if not the last one */
			memmove((char *)tmp+index*sizeof(int), (char *)tmp+(index+1)*sizeof(int), (size-i-index-1)*sizeof(int));
	}

	return 0;
}


/*------------------------------------------
This is from: https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix

1. Make dirs in a recursive way.
2. Compile with -D_GNU_SOURCE to activate strdupa().
3. No SPACE allowed in string dir.

Return:
	0	OK
	<0(-1) 	Fails
-------------------------------------------*/
int egi_util_mkdir(char *dir, mode_t mode)
{
        struct stat sb;

        if(dir==NULL)
                return -1;

        if( stat(dir, &sb)==0 )
                return 0;

        egi_util_mkdir( dirname(strdupa(dir)), mode);

        return mkdir(dir,mode);
}



/*---------------------------------------------
Copy file fsrc_path to fdest_path
@fsrc_path	source file path
@fdest_path	dest file path
Return:
	0	ok
	<0	fails
---------------------------------------------*/
int egi_copy_file(char const *fsrc_path, char const *fdest_path)
{
	unsigned char buff[1024];
	int fd_src, fd_dest;
	int len;

	fd_src=open(fsrc_path,O_RDWR|O_CLOEXEC);
	if(fd_src<0) {
		perror("egi_copy_file() open source file");
		return -1;
	}

	fd_dest=open(fdest_path,O_RDWR|O_CREAT|O_CLOEXEC,S_IRWXU|S_IRWXG);
	if(fd_dest<0) {
		perror("egi_copy_file() open dest file");
		return -1;
	}

	len=0;
	while( (len = read(fd_src, buff, 1024)) ) {
		write(fd_dest, buff, len);
	}

	close(fd_src);
	close(fd_dest);
	return 0;
}



/*---------------------------------------------------------------------
malloc 2 dimension buff.
char** buff to be regarded as char buff[items][item_size];

Allocates memory for an array of 'itmes' elements with 'item_size' bytes
each and returns a  pointer to the allocated memory.

Total 'items' memory blocks allocated, each block owns 'item_size' bytes.

return:
	!NULL	 	OK
	NULL		Fails
---------------------------------------------------------------------*/
unsigned char** egi_malloc_buff2D(int items, int item_size)
{
	int i,j;
	unsigned char **buff=NULL;

	/* check data */
	if( items <= 0 || item_size <= 0 )
	{
		printf("egi_malloc_buff2(): itmes or item_size is invalid.\n");
		return NULL;
	}

	buff=calloc(items, sizeof(unsigned char *));
	if(buff==NULL)
	{
		printf("egi_malloc_buff2(): fail to malloc buff.\n");
		return NULL;
	}

	/* malloc buff items */
	for(i=0;i<items;i++)
	{
		buff[i]=calloc(item_size, sizeof(unsigned char));
		if(buff[i]==NULL)
		{
			printf("egi_malloc_buff2(): fail to malloc buff[%d], free buff and return.\n",i);
			/* free mallocated items */
			for(j=0;j<i;j++)
			{
				free(buff[j]);
				//buff[j]=NULL;
			}
			free(buff);
			buff=NULL;
			return NULL;
		}
		/* clear data */
		//memset(buff[i],0,item_size*sizeof(unsigned char));
	}

	return buff;
}


/*---------------------------------------------------------------------------------------
reallocate 2 dimension buff.
NOTE:
  1. WARNING: Caller must ensure buff dimension size, old data will be lost after reallocation!!!!
     !!!! After realloc, the Caller is responsible to revise dimension size of buff accordingly.
  2. Be carefull to use when new_items < old_itmes, if realloc fails, then dimension of buff
     may NOT be able to reverted to the old_items.!!!!

buff:		2D buffer to be reallocated
old_items:	original item number as of buff[old_items][item_size]
new_items:	new item number as of buff[new_items][item_size]
item_size:	item size in byte.

return:
	0	 	OK
	<0		Fails
----------------------------------------------------------------------------------------*/
int egi_realloc_buff2D(unsigned char ***buff, int old_items, int new_items, int item_size)
{
	int i,j;
	unsigned char **tmp;

	/* check input data */
	if( buff==NULL || *buff==NULL) {
		printf("%s: input buff is NULL.\n",__func__);
		return -1;
	}
	if( old_items <= 0 || new_items <= 0 || item_size <= 0 ) {
		printf("%s: itmes or item_size is invalid.\n",__func__);
		return -2;
	}
	/* free redundant buff[] before realloc buff */
	if(new_items < old_items) {
		for(i=new_items; i<old_items; i++)
			free((*buff)[i]);
	}
	/* if items changed */
	if( new_items != old_items ) {
		tmp=realloc(*buff, sizeof(unsigned char *)*new_items);
		if(tmp==NULL) {
			printf("egi_realloc_buff2D(): fail to realloc buff.\n");
			return -3;
		}
		*buff=tmp;
	}
	/* need to calloc buff[] */
	if( new_items > old_items ) {
		for(i=old_items; i<new_items; i++) {
			(*buff)[i]=calloc(item_size, sizeof(unsigned char));

			if((*buff)[i]==NULL) {
				printf("egi_realloc_buff2D(): fail to calloc buff[%d], free buff and return.\n",i);
				for(j=i-1; j>old_items-1; j--)
					free( (*buff)[j] );

				/* revert item number to old_items !!! MAY BE TRAPED HERE!!! */
				tmp=NULL;
				while(tmp==NULL) {
					printf("egi_realloc_buff2D(): realloc fails,revert item number to old_items.\n");
					tmp=realloc( *buff, sizeof(unsigned char *)*old_items );
				}

				*buff=tmp;
				return -4;
			}
		}
	}

	return 0;
}


/*----------------------------------------------------
	free 2 dimension buff.
----------------------------------------------------*/
void egi_free_buff2D(unsigned char **buff, int items)
{
	int i;

	/* check data */
	if( items <= 0 ) {
		printf("%s: Param 'items' is invalid.\n",__func__);
		return;
	}

	/* free buff items and buff */
	if( buff == NULL ) {
		return;
	}
	else
	{
		for(i=0; i<items; i++) {
			free(buff[i]);
			//buff[i]=NULL;
		}

		free(buff);
		buff=NULL; /* ineffective */
	}
}


/*-----------------------------------------------------------------------------------------------------
1. Find out all files with specified type in a specified directory and push them into allocated fpbuff,
   then return the fpbuff pointer.
2. The fpbuff will expend its memory double each time when no space left for more file paths.
3. !!! Remind to free the fpbuff at last whatever the result !!!
4. Call egi_free_buff2D() to free result array of string.

@path:           Sear path without extension name.
@fext:		 File extension name, separated by ' ', '.', ',', or';'.
		 Example: "jpg", ".bmp, .doc", "avi; jpg .wav",....
@pcount:         Total number of files found, NULL to ignore.
		-1, search fails.

return value:
         Array of char string				 	OK
         NULL && pcount=-1;					Fails
	 NULL && pcount=0;					no file matches
--------------------------------------------------------------------------------------------*/
char ** egi_alloc_search_files(const char* path, const char* fext,  int *pcount )
{
        DIR 	*dir;
        struct 	dirent *file;
	char	fullpath[EGI_PATH_MAX+EGI_NAME_MAX];
	struct  stat sb;
        int 	num=0; 			/* file numbers */
	char 	*pt=NULL; 		/* pointer to '.', as for extension name */
	char 	**fpbuff=NULL; 		/* pointer to final full_path buff */
	int 	km=0; 			/* doubling memory times */
	char 	**ptmp;
	bool	ext_matched=false; 	/* extension name matched */
	int	len_path;		/* strlen of path */

	/* for extension name buffer */
	int k;
	char *strbuf;
	int nt=0;  /* total number of extension names as separated */
	char seps[]=" .,;"; /* separator for fext */
	char *spt;	  /* pointer to each separated fext string */
	char ext_list[EGI_FEXTBUFF_MAX][EGI_FEXTNAME_MAX]={0}; /* buffer for input extension names */


	/* 1. check input data */
	if( path==NULL || fext==NULL )
	{
		EGI_PLOG(LOGLV_ERROR, "ff_find_files(): Input path or extension name is NULL. \n");

		if(pcount!=NULL)*pcount=-1;
		return NULL;
	}

	/* 2. check input path leng */
	len_path=strlen(path);
	if( len_path > EGI_PATH_MAX-1 )
	{
		EGI_PLOG(LOGLV_ERROR, "egi_alloc_search_files(): Input path length > EGI_PATH_MAX(%d). \n",
											EGI_PATH_MAX);
		if(pcount!=NULL)*pcount=-1;
		return NULL;
	}

	/* get separated extension name list */
	strbuf=strdup(fext);
        spt=strtok(strbuf, seps);
        while(spt!=NULL) {
                snprintf(ext_list[nt], EGI_FEXTNAME_MAX, "%s", spt);
		nt++;
                spt=strtok(NULL,seps); /* get next */
        }
	free(strbuf);
#if 1
	printf("%s: ----------- ext_list[]: ",__func__);
	for(k=0; k<nt; k++)
		printf("%s ", ext_list[k]);
	printf("----------\n");
#endif

        /* 3. open dir */
        if(!(dir=opendir(path)))
        {
                EGI_PLOG(LOGLV_ERROR,"egi_alloc_search_files(): %s, error open dir: %s!\n",strerror(errno),path);

		if(pcount!=NULL) *pcount=-1;
                return NULL;
        }

	/* 4. calloc one slot buff first */
	fpbuff= calloc(1, sizeof(char *));
	if(fpbuff==NULL)
	{
		EGI_PLOG(LOGLV_ERROR,"egi_alloc_search_files(): Fail to callo fpbuff!.\n");

		if(pcount!=NULL) *pcount=-1;
		return NULL;
	}
	km=0; /* 2^0, first double */

        /* 5. get file paths */
        while((file=readdir(dir))!=NULL)
        {
		/* If NOT a regular file */
		snprintf(fullpath, sizeof(fullpath)-1,"%s/%s",path ,file->d_name);
		if( stat(fullpath, &sb)<0 || !S_ISREG(sb.st_mode) ) {
			continue;
		}

		/* 5.1 check number of files first, necessary to set limit????  */
                if(num >= EGI_SEARCH_FILE_MAX)/* break if fpaths buffer is full */
		{
			EGI_PLOG(LOGLV_WARN,"egi_alloc_search_files(): File fpath buffer is full! try to increase FFIND_MAX_FILENUM.\n");
                        break;
		}

		else if( num == (1<<km) )/* get limit of buff size */
		{
			/* double memory */
			km++;
			//ptmp=(char *)realloc((char *)fpbuff,(1<<km)*(EGI_PATH_MAX+EGI_NAME_MAX) );
			ptmp=realloc(fpbuff,(1<<km)*(sizeof(char *)));

			/* If doubling mem fails */
			if(ptmp==NULL)
			{
				EGI_PLOG(LOGLV_ERROR,"egi_alloc_search_files(): Fail to realloc mem for fpbuff.\n");
				/* break, and return old fpbuff data though*/
				break;
			}

			/* get new pointer to the buff */
			//fpbuff=( char(*)[EGI_PATH_MAX+EGI_NAME_MAX])ptmp;
			fpbuff=ptmp;
			ptmp=NULL;

			EGI_PLOG(LOGLV_INFO,"egi_alloc_search_files(): fpbuff[] is reallocated with capacity to buffer totally %d items of full_path.\n",
													1<<km );
		}

                /* 5.2 check name string length */
                if( strlen(file->d_name) > EGI_NAME_MAX-1 )
		{
			EGI_PLOG(LOGLV_WARN,"egi_alloc_search_files(): %s.\n	File path is too long, fail to store.\n",
									file->d_name);
                        continue;
		}

		/* TODO: skip mid '.' */
		pt=strstr(file->d_name,"."); /* get '.' pointer */
		if(pt==NULL){
			//printf("ff_find_files(): no extension '.' for %s\n",file->d_name);
			continue;
		}

		/* compare file extension  with items in ext_list[] */
		ext_matched=false;
		for(k=0; k<nt; k++) {
			if( strncmp(pt+1, ext_list[k], EGI_FEXTNAME_MAX)==0 ) {
				ext_matched=true;
				break;
			}
		}
		if(!ext_matched)
			continue;

		/* Clear buff and save full path of the matched files */
		fpbuff[num]=calloc(1, len_path+strlen(file->d_name)+2);
		sprintf(fpbuff[num], "%s/%s", path, file->d_name);
		//printf("egi_alloc_search_files2(): push %s ...OK\n",fpbuff[num]);

                num++;
        }

	/* 6. feed back count to the caller */
	if(pcount != NULL)
	        *pcount=num; /* return count */

	/* 7. free fpbuff if no file found */
	if( num==0 && fpbuff != NULL )
	{
		free(fpbuff);
		fpbuff=NULL;
	}

	/* 8. close dir */
         closedir(dir);

	/* 9. return pointer to the buff */
         return fpbuff;
}


/* Base64 element table */
#define BASE64_ETABLE_MAX	12
static const char BASE64_ETABLE[BASE64_ETABLE_MAX][65]=
{
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/+",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!-",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-!",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-.",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789:_",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_:"
};

static const char HEXBIT_TABLE[]="0123456789abcdef";


/*--------------------------------------------------------------------------------------------------
Encode input data to base64 string without any line breaks.

Ignore RFC2045 requirements:
 "
   (5)  (Soft Line Breaks) The Quoted-Printable encoding
      	REQUIRES that encoded lines be no more than 76
      	characters long.  If longer lines are to be encoded
      	with the Quoted-Printable encoding, "soft" line breaks
      	must be used.  An equal sign as the last character on a
      	encoded line indicates such a non-significant ("soft")
      	line break in the encoded text.
 "

@type: 	Type of BASE64_ETAB, index of it. default 0;
@data:	Input data.
@size:  Input data size.
@buff:	Buffer to hold enconded data.
	Note: The caller MUST allocate enought space for buff!

Return:
	<0	Fails
	>=0	Length of output base64 data.
--------------------------------------------------------------------------------------------------*/
int egi_encode_base64(int type, const unsigned char *data, unsigned int size, char *buff)
{
	int i,n,m;
	int ret=0;	/* Length of encoded base64 data */

	n=size/3;	/* every 3_byte data convert to 4_byte buff */
	m=size%3;	/* remaining byte */

	if(data==NULL || size==0 || buff==NULL)
		return -1;

	/* check type number, or use default as 0 */
	if( type<0 || type > BASE64_ETABLE_MAX-1 )
			type=0;

	/* Encode n*3_bytes of input data */
	for(i=0; i<n; i++) {
		buff[4*i+0]=BASE64_ETABLE[type][ data[3*i]>>2 ];					/*  first 6bits of data[0] */
		buff[4*i+1]=BASE64_ETABLE[type][ ((data[3*i]&0x3)<<4) + (data[3*i+1]>>4) ];		/*  2bits of data[0] AND 4bits of data[1] */
		buff[4*i+2]=BASE64_ETABLE[type][ ((data[3*i+1]&0x0F)<<2) + (data[3*i+2]>>6) ];	/*  4bits of data[1] AND 2bits of data[2] */
		buff[4*i+3]=BASE64_ETABLE[type][ data[3*i+2]&0x3F ];  				/*  last 6bits of data[2] */
	}
	ret = n*4;

	/* Encode remaining data */
	if(m==1) {
		buff[4*n+0]=BASE64_ETABLE[type][data[3*n]>>2];		/* first 6 bits of data[0] */
		buff[4*n+1]=BASE64_ETABLE[type][(data[3*n]&0x3)<<4];		/* last 2 bits of data[0]  */
		buff[4*n+2]='=';					/* padding */
		buff[4*n+3]='=';
		ret +=4;
	}
	else if(m==2) {
		buff[4*n+0]=BASE64_ETABLE[type][data[3*n]>>2];					/*  first 6bits of data[0] */
		buff[4*n+1]=BASE64_ETABLE[type][ ((data[3*n]&0x3)<<4) + (data[3*n+1]>>4) ];		/*  2bits of data[0] AND 4bits of data[1] */
		buff[4*n+2]=BASE64_ETABLE[type][ (data[3*n+1]&0x0F)<<2 ];				/*  last 4bits of data[1] */
		buff[4*n+3]='=';
		ret +=4;
	}

	return ret;
}


/*--------------------------------------------------------------------------------------------------
Encode base64 string into URL, by
	1. Replace '+', ASICC code 2B, with '%2B'
	2. Replace '/', ASIIC code 2F, with '%2F'
	3. Replace '=', ASIIC code 3D, with '%3D'

@base64_data:  	Input data in base64 encoding.
@data_size:  	Input data size.
@buff:  	Buffer to hold enconded URL data.
@buff_size:	Buff size.
        	Note: The caller MUST allocate enought space for buff!
		      If buff_size if less than length of encoded data + 1, then it fails.
@notail:	If true, get rid of '='

Return:
        <0      Fails
        >=0     Length of output base64URL data.
---------------------------------------------------------------------------------------------------*/
int egi_encode_base64URL(const unsigned char *base64_data, unsigned int data_size, char *buff,
								unsigned int buff_size, bool notail)
{
	int i,j;

	int ret=buff_size; /* Length of final encoded base64URL data */

	if(base64_data==NULL || data_size==0 || buff==NULL )
		return -1;

	j=0; /* buff index */

	for(i=0; i<data_size; i++)
	{
		switch(base64_data[i]) {
			case	'+':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='2'; buff[j+2]='B';
					j += 3;
					break;
			case	'/':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='2'; buff[j+2]='F';
					j += 3;
					break;
			case	'!':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='2'; buff[j+2]='1';
					j += 3;
					break;
			case	'-':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='2'; buff[j+2]='D';
					j += 3;
					break;
			case	'_':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='5'; buff[j+2]='F';
					j += 3;
					break;
			case	'.':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='2'; buff[j+2]='E';
					j += 3;
					break;
			case	':':
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='3'; buff[j+2]='A';
					j += 3;
					break;


			case	'=':
				 if(notail) {
					break; /* get rid of '=' */
				 }
				 else {
					buff_size -= 3;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]='%'; buff[j+1]='3'; buff[j+2]='D';
					j += 3;
					break;
				}
			default:
					buff_size -=1;
					if(buff_size<0) { ret=-2; goto END_ENCODE; }
					buff[j]=base64_data[i];
					j++;
					break;
		}

	}

	/* cal output URL  length */
	if(buff_size==0) {
		printf("%s: Buffer has no space for the closing NULL character. \n",__func__);
		ret=-3;
	}
	else {
		buff[j]='\0';
		buff_size -= 1;

		/* cal output URL  length */
		ret -= buff_size;
	}


END_ENCODE:
	if(ret<0)
		printf("%s: Buffer size is NOT enough, quit encoding. \n",__func__);

	return ret;
}



/*--------------------------------------------------------------------------------------------------
Encode UFT8 string into URL, by
        1. Replace '+', ASICC code 2B, with '%2B'
        2. Replace '/', ASIIC code 2F, with '%2F'
        3. Replace '=', ASIIC code 3D, with '%3D'
	4. Replace other uft8 chars with '%xx','%xx%xx' OR '%xx%xx%xx' etc.

@ustr:   	Input data in UFT8 encoding.
@data_size:     Input data size.
@buff:          Buffer to hold enconded URL data.
@buff_size:     Buff size.
                Note: The caller MUST allocate enought space for buff!
                      If buff_size if less than length of encoded data + 1, then it fails.

Return:
        <0      Fails
        >=0     Length of output.
---------------------------------------------------------------------------------------------------*/
int egi_encode_uft8URL(const unsigned char *ustr, char *buff, unsigned int buff_size)
{
        int i,j,k;
	int len;	/* in bytes, lenght of current UFT8 char */
	const unsigned char *ps=NULL;	    /* pointer to curretn UFT8 char */
	int usize; /* Get total number of uft8 chars in input string */

        int ret=buff_size; /* Length of final encoded URL data */

        if(ustr==NULL || buff==NULL )
                return -1;

	/* Get total number of uft8 chars in input string */
	usize=cstr_strcount_uft8(ustr);
        if( usize<1 )
                return -2;

        j=0; 	/* buff index */
	ps=ustr;

	for(i=0; i<usize; i++)
	{
		len=cstr_charlen_uft8(ps);
#if 0	/* --- TEST ---- */
		char strbuf[16]={0};
		printf("%s: i=%d len=%d\n",__func__,i,len);
		memset(strbuf,0,16);
		for(k=0; k<len; k++)
			strbuf[k]=*(ps+k);
		//snprintf(strbuf,len,"%s", ps);
		printf("%s: %s\n",__func__, strbuf);
#endif
		/* IF: ASCII chars */
		if(len==1) {
			/* case '+''/' */
			switch(*ps) {
				//case '+':
				case '/':
		                        buff_size -= 3;
		                        if(buff_size<0) { ret=-3; goto END_ENCODE; }
					buff[j]='%';
					buff[j+1]=HEXBIT_TABLE[(*ps)>>4];	/* LSB first */
					buff[j+2]=HEXBIT_TABLE[*(ps++)&0xF];
					j+=3;
					break;

				default: /* Other printable ASCII chars */
                                        buff_size -= 1;
                                        if(buff_size<0) { ret=-3; goto END_ENCODE; }
					buff[j]=*(ps++);
					j+=1;
					break;
			}
		}
		/* ELSE IF: len>1, local chars */
		else {
			buff_size -= 3*len; 	/* %xx %xx ... */
			if(buff_size<0) { ret=-3; goto END_ENCODE; }
			for(k=0; k<len; k++) {
				//printf("0x%x\n",*ps);
                        	buff[j]='%';
                                buff[j+1]=HEXBIT_TABLE[(*ps)>>4];
                                buff[j+2]=HEXBIT_TABLE[*(ps++)&0xF];
				j+=3;
			}
		}
	} /* END for() */

        /* cal output URL  length */
        if(buff_size==0) {
                printf("%s: Buffer has no space for the closing NULL character. \n",__func__);
                ret=-4;
        }
        else {
                buff[j]='\0';
                buff_size -= 1;

                /* cal output URL  length */
                ret -= buff_size;
        }


END_ENCODE:
        if(ret<0)
                printf("%s: Buffer size is NOT enough, quit encoding. \n",__func__);

	return ret;
}


/*-------------------------------------------------------
To create and/or lock pid_lock_file, to ensure
there is ONLY one instance running.

@pid_lock_file:  File for PID lock.

Return:
        FD of pid_lock_file     OK
        =0                      Instance already running.
        <0                      Fails
-------------------------------------------------------*/
int egi_lock_pidfile(const char *pid_lock_file)
{
        int fd;
        char strtmp[64];
        int len;

        fd=open(pid_lock_file, O_RDWR|O_CREAT, 0644);
        if(fd<0) {
                egi_dperr("Fail to open '%s'.",pid_lock_file);
                return -1;
        }

        /* Get advisory exclusive lock */
        if( flock(fd, LOCK_EX|LOCK_NB) !=0 ) {
                if(errno==EWOULDBLOCK) {
                        egi_dpstd("An instance already running with pid file locked!\n");
                        close(fd);
                        return 0;
                }
                else {
                        egi_dperr("Fail to flock '%s'.",pid_lock_file);
                        close(fd);
                        return -2;
                }
        }

        /* Write current pid into the file */
        if( ftruncate(fd, 0) !=0 ) {
                egi_dperr("Fail to ftruncate '%s'.", pid_lock_file);
                flock(fd, LOCK_UN);
                close(fd);
                return -3;
        }
        sprintf(strtmp, "%d\n", getpid());
        len=strlen(strtmp)+1;
        if( len != write(fd, strtmp, len) )
                egi_dpstd("WARNING: short write!\n");

        /* File locked, DO NOT close(fd) here! */

        return fd;
}

/*----------------------------------------------------
Validate a file descriptor.

Reference: https://blog.csdn.net/kangear/article/details/42805393

Return:
	0	OK, fd is valid.
	<0	Fails, fd is invalid. or its underlying
		device is closed/disconnected.
----------------------------------------------------*/
int egi_valid_fd(int fd)
{
        struct stat sbuf;

	if(fd<0)
		return -1;

	if( fcntl(fd, F_GETFL) >0 ) {  	   /*  Get file status flags */
        	if( fstat(fd, &sbuf)==0 ) {     /* Get file status */
                	if( sbuf.st_nlink >=1 ) {  /* Check links */
				return 0;
			}
		}
	}

	return -1;
}


/*--------------------------------------------
Create an EGI_BITSTATUS.

@total: Total number of effective bits needed.

Return:
	OK	An pointer to EGI_BITSTATUS.
	NULL	Fails
---------------------------------------------*/
EGI_BITSTATUS *egi_bitstatus_create(unsigned int total)
{

	EGI_BITSTATUS *ebits=calloc(1, sizeof(EGI_BITSTATUS));
	if(ebits==NULL)
		return NULL;

	ebits->octbits=calloc((total>>3)+1, 1);
	if(ebits->octbits==NULL) {
		free(ebits);
		return NULL;
	}

	ebits->total=total;

	return ebits;
}

/*--------------------------------------------
Free an EGI_BITSTATUS
---------------------------------------------*/
void egi_bitstatus_free(EGI_BITSTATUS **ebits)
{
	if(ebits==NULL) return;
	if(*ebits==NULL) return;

	free((*ebits)->octbits);
	free(*ebits);
	*ebits=NULL;
}

/*------------------------------------------------------
Get value of the specified bit.

@ebits: 	EGI_BITSTATUS*
@index:		position/index of the bit in all octbis[]
		starts from 0.
Return:
        0||1       OK
        <0         Fails
-------------------------------------------------------*/
inline int egi_bitstatus_getval(const EGI_BITSTATUS *ebits, unsigned int index)
{
        if(ebits==NULL)
                return -1;
	if(index > ebits->total-1)
		return -2;

	if( ebits->octbits[(index>>3)] & (1<<(index&0b111)) )
		return 1;
	else
		return 0;
}


/*------------------------------------------------------
Print bits value of an EGI_BITSTATUS.

@ebits: EGI_BITSTATUS*
Return:
        >=0       OK
        <0      Fails
-------------------------------------------------------*/
void egi_bitstatus_print(const EGI_BITSTATUS *ebits)
{
	int i;

        if(ebits==NULL)
                return;

	printf("Total bits: %d\n", ebits->total);

	for(i=0; i < ebits->total; i++) {
		if( (i&0b111) == 0 )
			printf("[%d]", i>>3);

		if( ebits->octbits[(i>>3)] & (1<<(i&0b111)) )
			printf("1");
		else
			printf("0");

		if( (i&0b111) == 0b111 )
			printf(" ");
	}
	printf("\n");
}

/*----------------------------------------------------------
Set/reset the  bit to be 1/0.

@ebits:		EGI_BITSTATUS
@index:		position/index of the bit in all octbis[]
		starts from 0.

Return:
	0	OK
	<0	Fails
---------------------------------------------------------*/
inline int egi_bitstatus_set(EGI_BITSTATUS *ebits, unsigned int index)
{
	if(ebits==NULL)
		return -1;
	if(index > ebits->total-1 )
		return -1;

	ebits->octbits[(index>>3)] |= 1<<(index&0b111);

	return 0;
}
inline int egi_bitstatus_reset(EGI_BITSTATUS *ebits, unsigned int index)
{
	if(ebits==NULL)
		return -1;
	if(index > ebits->total-1 )
		return -1;

	ebits->octbits[(index>>3)] &= ~(1<<(index&0b111));

	return 0;
}


/*---------------------------------------------
Set/reset all bits to be 1/0.
@ebits:		EGI_BITSTATUS*
Return:
	0	OK
	<0	Fails
---------------------------------------------*/
inline int egi_bitstatus_setall(EGI_BITSTATUS *ebits)
{
	int i;

	if(ebits==NULL)
		return -1;

	for(i=0; i < (ebits->total>>3)+1; i++)
		ebits->octbits[i]=0xFF;

	return 0;
}
/* Reset all bits to be 1 */
inline int egi_bitstatus_resetall(EGI_BITSTATUS *ebits)
{
	int i;

	if(ebits==NULL)
		return -1;

	for(i=0; i < (ebits->total>>3)+1; i++)
		ebits->octbits[i]=0x00;

	return 0;
}


/*------------------------------------------------------
Count all 1_bits in octbits[]
@ebits: EGI_BITSTATUS*
Return:
        >=0       OK
        <0      Fails
-------------------------------------------------------*/
inline int egi_bitstatus_count_ones(const EGI_BITSTATUS *ebits)
{
	int index;
	int count;

        if(ebits==NULL)
                return -1;

	for(count=0, index=0; index < ebits->total; index++) {
		if( ebits->octbits[(index>>3)] & (1<<(index&0b111)) )
			count++;
	}

	return count;
}

/*-------------------------------------------------------
Count all 0_bits in octbits[]
@ebits: EGI_BITSTATUS*
Return:
        >=0       OK
        <0      Fails
-------------------------------------------------------*/
inline int egi_bitstatus_count_zeros(const EGI_BITSTATUS *ebits)
{
	int index;
	int count;

        if(ebits==NULL)
                return -1;

	for(count=0, index=0; index < ebits->total; index++) {
		if( !(ebits->octbits[(index>>3)] & (1<<(index&0b111))) )
			count++;
	}

	return count;
}

/*---------------------------------------------------------
Search for the first ZERO bit starting from ebits->pos=cp.

@ebits:  EGI_BITSTATUS*
@cp:     Set as current ebits->pos

Return:
        =0     OK, found next ZERO as indicated by ebits->pos
        <0     Fails
	       No result, ebits->pos gets to the end.
----------------------------------------------------------*/
inline int egi_bitstatus_posfirst_zero(EGI_BITSTATUS *ebits,  int cp)
{
	unsigned char byte;

        if(ebits==NULL)
                return -1;

	/* Limit cp */
	if(cp<0) cp=0;

	/* Set cp as current pos */
	for(ebits->pos=cp; ebits->pos < ebits->total; ebits->pos++) {
		byte=ebits->octbits[(ebits->pos>>3)];
		if( byte==0xFF ) {
			ebits->pos += 8-1;  /* Skip to next byte */
			continue;
		}

		if( !(byte&( 1<<( ebits->pos&0b111 ) )) )
			return 0;
		else
			continue;  /* OK, find pos */
	}

	return -2;
}

/*---------------------------------------------------------
Search for the first ONE bit starting from ebits->pos=cp.

@ebits: EGI_BITSTATUS*

Return:
        =0     OK, found next ONE as indicated by ebits->pos
        <0     Fails
	       No result, ebits->pos gets to the end.
----------------------------------------------------------*/
inline int egi_bitstatus_posfirst_one(EGI_BITSTATUS *ebits,  int cp)
{
	unsigned char byte;

        if(ebits==NULL)
                return -1;

	/* Limit cp */
	if(cp<0) cp=0;

	/* Set cp as current pos */
	for(ebits->pos=cp; ebits->pos < ebits->total; ebits->pos++) {
		byte=ebits->octbits[(ebits->pos>>3)];
		if( byte==0x00 ) {
			ebits->pos += 8-1; /* Skip to next byte */
			continue;
		}

		if( byte&( 1<<( ebits->pos&0b111 ) ) )
			return 0;  /* Ok, find pos */
		else
		    continue;
	}

	return -2;
}


/*---------------------------------------------------------
Bits checksum:  to addup up all ONE bits in the data.

@data:	Data
@size:  Data size.

Return:
        >=0     OK, bits checksum
        <0     Fails
	       No result, ebits->pos gets to the end.
----------------------------------------------------------*/
int egi_bitstatus_checksum(void *data, size_t size)
{
	EGI_BITSTATUS ebits={0};

	if(data==NULL)
		return -1;

	ebits.total=size;
	ebits.octbits=data;

	/* TODO: divide data into 2^31 bits parts, 2^(31-8)=2^23 bytes.  */
	return egi_bitstatus_count_ones(&ebits);
}



