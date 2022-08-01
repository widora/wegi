/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Utility functions, mem.

Journal
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
2021-12-10:
	1. Add egi_host_littleEndian().
2021-12-20:
	1. Add egi_get_fileSize()
2022-01-04:
	egi_copy_file(): Option for append the dest file.
2022-02-16:
	1. Add egi_read_fileBlock()
	2. Add egi_copy_fileBlock()
2022-02-18:
	1. Add EGI_ID3V2TAG and related functions.
2022-04-10:
	1. Add egi_getset_backlightPwmDuty()
2022-04-11:
	1. Add egi_byteswap()
2022-07-02:
	1. Add egi_extract_aac_from_ts()
2022-07-06:
	1. Add egi_extract_aac_from_ts(): Save video data.
	2.  egi_extract_aac_from_ts() rename to egi_extract_AV_from_ts()
2022-07-14:
	1. egi_extract_AV_from_ts() 3.3.6/3.3.6a: fread/fwrite will fail
	   if datasize==0!
2022-07-20:
	1. Add egi_create_charList()
2022-07-25:
	1. egi_extract_AV_from_ts() 3.3.5.3: To omit PES_header_data for
	   audio type AAC.
2022-07-28:
	1. Add egi_append_file()

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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

/*---------------------------------------
Create a charList.
@items: Number of items in the list
@size:  Size for each item.
---------------------------------------*/
char** egi_create_charList(int items, int size)
{
	char **p=NULL;
	int i,j;

	if(items<1 || size <1)
		return NULL;

	p=calloc(items, sizeof(char *));
	if(p==NULL)
		return NULL;

	for(i=0; i<items; i++) {
		p[i]=calloc(size, sizeof(char));
		if(p[i]==NULL) {
			for(j=0; j<i; j++)
				free(p[j]);
			free(p);
			return NULL;
		}
	}

	return p;
}


/*---------------------------------------
Free a char pointer list and reset
it to NULL
@p: Pointer to PPointer(string list).
@n: Size of the list
----------------------------------------*/
inline void egi_free_charList(char ***p, int n)
{
	int i;
	if( p!=NULL && *p!=NULL ) {
		/* Free list items */
		for(i=0; i<n; i++) {
		   //if((*p)[i]) /* HK2022-07-18 */
			free((*p)[i]);
		}
		/* Free list pointers */
		free(*p);
		*p=NULL;
	}
}

/*------------------------------------
Check the Endianness of the host.
Return:
	True	Little endian
	False	Big endian
------------------------------------*/
inline bool egi_host_littleEndian(void)
{
	int idat=0x55667788;

	unsigned char *ch=(unsigned char *)&idat;

	return (*ch)==0x88;
}

/*-------------------------------------------
Reverse the order of bytes in data.
	!!! CAUTION !!!
The caller MUST ensure data mem space.

@n:	Size of data.
@data:  Pointer to data mem;
------------------------------------------*/
void egi_byteswap(int n, char *data)
{
	if(data==NULL || n<1)
		return;

	int k;
	char tmp;
	for(k=0; k<n/2; k++) {
		data[k]=tmp;
		data[k]=data[n-1-k];
		data[n-1-k]=tmp;
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

/*---------------------------------------------
	Return file size.

For 32-bits type off_t, mmap Max. file size <2G !
   off_t: 32bits, Max. fsize:  2^31-1
   off_t: 64bits, Max. fsize:  2^63-1

Return:
	0	Fails or zero size file.
	>0	OK
----------------------------------------------*/
unsigned long egi_get_fileSize(const char *fpath)
{
	struct stat buf;
	if( stat(fpath, &buf)!=0 )
		return 0;

	return (unsigned long)buf.st_size;
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
	if( fmap->fsize <= 0 && resize<1 ) {
//		printf("%s: file size is 0! Fail to mmap '%s'!\n", __func__, fpath);
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


/*-----------------------------------------------------------------
Read data from the fsrc to fdes.

Note:
1. FILE fsrc MUST have been properly open (with mode 'rb')
   and file position already set at right position.
2. Operation will end when 'size' or 'tnulls' whichever
   achieves first.
3. The Caller MUST ensure enough space for fdes.

@fsrc:   Source FILE.
@fdes:   Dest FILE.
@tnulls: Numbers if terminating mark '\0's to indicate END of
         the data block.
	 1 -- End with '\0'  2 -- End with '\0\0'  3 -- ...
	 0 -- Ignore.
@bs:   Size of data block to be copied.
	 if 0, ignore.

Return:
	>0	OK, Size of data copied. including terminating NULLs.
	<0	Fails,
		!!!! --- WARNING ---- !!!!
		Content of fdes MAY changed
		File position MAY changed!
-------------------------------------------------------------------*/
int egi_read_fileBlock(FILE *fsrc, char *fdes, int tnulls, size_t bs)
{
	size_t k;
	int  tcnt=0; /* Terminating mark counter */

	if(fsrc==NULL || fdes==NULL)
		return -1;

   	while(1) {
	   /* Read one byte each time */
	   if( fread(fdes+k, 1, 1, fsrc)!=1 ) {
		return -2;
	   }

	   /* Update k, as data bytes read out. */
	   k++;

	   /* Check tnulls */
	   if( tnulls>0 ) {
		if( fdes[k-1]=='\0' ) tcnt +=1;
		else  tcnt=0;  /* Reset */

		if( tcnt == tnulls )
			return k; 			/* OK */
	   }

	   /* Check bsize */
	   if(bs>0 && k==bs)
		return k;				/* OK */
   	}

}


/*------------------------------------------------------
Copy data from the fsrc to fdes.

Note:
1. FILE fsrc and fdes MUST have been properly opened
   (with mode 'rb'/'wb' etc.) and file positions are
   preset at right position.

@fsrc:   Source FILE.
@fdes:   Dest FILE.
@bs:     Size of data block to be copied.

Return:
	0	OK, Size of data copied.
	<0	Fails,
		!!!! --- WARNING ---- !!!!
		Content of fdes MAY changed
		File positions MAY changed!
-------------------------------------------------------*/
int egi_copy_fileBlock(FILE *fsrc, FILE *fdest, size_t bs)
{
	unsigned char buff[1024];
	int nb; /* blocks =bs/1024 */
	int nr; /* remaining =bs%1024 */
	int k;

	if( fsrc==NULL || fdest==NULL || bs==0 )
		return -1;

	/* Compute nb/nr */
	nb = bs/1024;
	nr = bs&(1024-1);

	/* Copy 1024-blocks */
	for(k=0; k<nb; k++) {
		if( fread(buff, 1024, 1, fsrc)!=1 )
			return -2;
		if( fwrite(buff, 1024, 1, fdest)!=1 )
			return -3;
	}

	/* Copy remaining data */
	if( fread(buff, nr, 1, fsrc)!=1 )
		return -4;
	if( fwrite(buff, nr, 1, fdest)!=1 )
		return -5;

	return 0;
}


/*---------------------------------------------
Copy file fsrc_path to fdest_path
@fsrc_path	source file path
@fdest_path	dest file path
@append		If to append to dest file.
Return:
	0	ok
	<0	fails
---------------------------------------------*/
int egi_copy_file(char const *fsrc_path, char const *fdest_path, bool append)
{
	unsigned char buff[1024];
	int fd_src, fd_dest;
	int len;

	fd_src=open(fsrc_path,O_RDONLY|O_CLOEXEC);  //O_RDWR
	if(fd_src<0) {
		perror("egi_copy_file() open source file");
		return -1;
	}

	fd_dest=open(fdest_path,O_WRONLY|O_CREAT|O_CLOEXEC|(append?O_APPEND:0),S_IRWXU|S_IRWXG); //O_RDWR
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


/*---------------------------------------------
Append data to end of file, and create the file
first if it doesn't exist.

@fpath		File path
@data		Pointer to data
#size		Size of data to append to.
Return:
	>0	Short written.
	0	OK
	<0	fails
---------------------------------------------*/
int egi_append_file(char const *fpath, void* data, size_t size)
{
	FILE *fil=NULL;
	int ret=0;

	/* Open the file */
	fil=fopen(fpath, "ab");
	if(fil==NULL) {
		perror("egi_append_file() open file");
		return -1;
	}

	/* Write data */
	ret=fwrite(data, 1, size, fil);
	if(ret>0 && ret==size)
		ret=0;
	//else: Fails Or short written.

	fclose(fil);
	return ret;
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


///////////////////////   ID3V2 Tag Functions   ///////////////////////
#include <arpa/inet.h>  /* ntohl */

/*-----------------------------------
Callocate an empty EGI_ID3V2TAG.
Return:
	!NULL   OK
	NULL	Fails
------------------------------------*/
EGI_ID3V2TAG *egi_id3v2tag_calloc(void)
{
	return calloc(1, sizeof(EGI_ID3V2TAG));
}


/*-----------------------------------
Free an EGI_ID3V2TAG.
------------------------------------*/
void egi_id3v2tag_free(EGI_ID3V2TAG **tag)
{
	if(tag==NULL || *tag==NULL)
		return;

	free( (*tag)->title );
	free( (*tag)->album );
	free( (*tag)->performer );
	free( (*tag)->comment );

	egi_imgbuf_free2(&(*tag)->imgbuf);

	free((*tag));
	*tag=NULL;
}


/*-----------------------------------------------------------
Extract ID3V2 tag from a (mp3) file. The ID3V2 Tag Header(10Bytes)
MUST be located in the very beginning of the file.


Note:
   0. ID3V2 Reference: https://id3.org
   1. Tag ID3V2 structrue:
       Tag_Header +Tag_Frame(TagFrameHeader+TagFrameBody) +Tag_Frame(TFH+TFB) +Tag_Frame(TFH+TFB) ...

   2. Struct of [Tag_Header]:
	Bytes  |   Content
	0-2	   TAG identifier "ID3"
	4-7        TAG version, eg 03 00
	5          Flags
        6-9	   Size of TAG (It doesnt contain Tag_Header itself)
		   The most significant bit in each Byte is set to 0 and ignored,
		   Only remaining 7 bits are used, as to avoid mismatch with audio frame header
                   which has the first synchro Byte 'FF'.

   3. Struct of [Tag_Frame_Header]:
	Bytes  |   Content
	0-3        Frame identifier
		   TRCK:  Track number
		   TIME:  time
		   WXXX:  URL
	           TCON:  Genre
		   TYER:  Year
	     	   TALB:  Album
		   TPE1:  Lead performer(s)/Soloist(s)
		   TIT2:  Title/songname/content description
		   APIC:  Attached picture

		   GEOB:  General encapsulated object
		   COMM:  Commnets
		   ... (more)
	4-7        Size (It doesnt include Tag_Frame_Header itself)
	8-9	   Flags

   4. Struct of [Tag_Frame_Body]:
	Depends on Frame identifer, see ID3V2 standard:
	Example for 'APCI':
		Text encoding(2Bytes)   $xx  (see Note)
		MIME type               <text string> $00

		Picture type(2Bytes)    $xx
					03 Cover(front)
					04 Cover(back)
					08 Artist/performer
					0C Lyricist/text writer
					...
		Description     	<text string according to encoding> $00 (00)
		Picture data    	<binary data>
					JPEG: starts with 'FF D8'
					PNG:  starts with ''

    5. Text string in the frame (See "ID3v2 frame overview"  --- Document id3v2.3 )
     "All numeric strings and URLs are always encoded as ISO-8859-1. Terminated strings are terminated with $00 if encoded with ISO-8859-1,
     and $00 00 if encoded as unicode If nothing else is said newline character is forbidden. In ISO-8859-1 a new line is represented,
     when allowed, with $0A only. Frames that allow different types of text encoding have a text encoding description byte directly after
     the frame size. If ISO-8859-1 is used this byte should be $00, if Unicode is used it should be $01.
     (Note: others are not mentioned in this standard:  $02 -- UTF-16BE(without BOM), $03 -- UTF8 )
     Strings dependent on encoding is represented as <text string according to encoding>, or <full text string according to encoding> if
     newlines are allowed. Any empty Unicode strings which are NULL-terminated may have the Unicode BOM followed by a Unicode NULL
     ($FF FE 00 00 or $FE FF 00 00)."


@fpath:	Full file path.

Return:
	!NULL	 Ok
	NULL	 Fails
-------------------------------------------------------*/
EGI_ID3V2TAG *egi_id3v2tag_readfile(const char *fpath)
{
	FILE *fil=NULL;
	int err=0;
	unsigned char ID3V2TagHead[10]={0};   /* Header of the ID3V2TAG (as whole) */
	unsigned int  ID3V2TagSize=0;         /* Total length of the ID3V2 tag body, MAX 256MB. excluding ID3V2TagHead. */

	unsigned char ID3V2FrameHead[10]={0}; /* Header of each tag frame */
	char 	      TagFrameID[5];          /* Tag frame identifier */
	unsigned int  tfbsize=0;   	       /* Size of a tag Frame_Body,
						* The value is reduced each time frame_data is read out for parsing,
						* and finanlly set to 0 when whole frame_body is read out.
						*/

	long  fpos;	           /* current file position */
	char  encodeType;          /* Text encode type: 0 -- ISO8859-1(ASCII), 1 -- Unicode  3-- UTF-8*/

	/* For text information frame */
	bool IsHostByteOrder;
	bool IsTextInfoFrame;
	char strText[1024];        /* ASCII OR UTF-8 */
	uint16_t uniTmp;           /* 16-bits unicode 2.o */
	wchar_t uniText[1024/2];   /* 32-bits Unicode */
	int len;
	int i;

	/* 1. Allocate struct ID3V2TAG */
	EGI_ID3V2TAG *mp3tag=egi_id3v2tag_calloc();
	if(mp3tag==NULL)
		return NULL;

	/* 2. Open (mp3) file */
	fil=fopen(fpath, "rb");
	if(fil==NULL) {
		egi_dperr("Fail to open '%s'", fpath);
		err=1;
		goto END_FUNC;
	}

	/* 3. Read ID3V2 Tag Header, 10Bytes. */
	if( fread(ID3V2TagHead, 10, 1, fil)!=1 ) {
		egi_dpstd("Fail to read ID3V2 Tag Header!\n");
		err=2;
		goto END_FUNC;
	}

	/* 4. Confirm ID3V2 identifier "ID3" */
	if( (ID3V2TagHead[0]=='I' || ID3V2TagHead[0]=='i')
	   && (ID3V2TagHead[1]=='D' || ID3V2TagHead[1]=='d')
	   && ID3V2TagHead[2]=='3' ) {

	   /* OK */
	   //egi_dpstd("ID3V2 tag is found!\n");
	}
	else {
	   egi_dpstd("No ID3V2 tag is found!\n");
		err=3;
		goto END_FUNC;
	}

	/* 5. Get total length of ID3V2 tag
        bits[6-9]  Size of TAG (It doesnt contain Tag_Header itself)
                   The most significant bit in each Byte is set to 0 and ignored,
                   Only remaining 7 bits are used, as to avoid mismatch with audio frame header
                   which has the first synchro Byte 'FF'.
	*/
	ID3V2TagSize= ((ID3V2TagHead[6]&0x7F)<<21) + ((ID3V2TagHead[7]&0x7F)<<14)
		+ ((ID3V2TagHead[8]&0x7F)<<7) + (ID3V2TagHead[9]&0x7F);

	egi_dpstd("ID3V2 tag body size: %d bytes\n", ID3V2TagSize);

	/* 6. Get current file position */
	fpos=ftell(fil);

	/* 7. Parse Tag Frame one by one */
	while( fpos>0 && fpos < ID3V2TagSize ) {

		/* W1. Process Tag Frame Header */
		/* W1.1 Read ID3V2 Tag Header, 10Bytes */
		if( fread(ID3V2FrameHead, 10, 1, fil)!=1 ) {
			//err=;
			break;
		}

		/* W1.2 bits[0-3]   Frame identifier */
		strncpy(TagFrameID, (char *)ID3V2FrameHead, 4);
		TagFrameID[4]='\0';
		//egi_dpstd("TagFrameID: %s\n", TagFrameID);

		/* W1.3 bits[4-7]  Tag Frame Body size (It doesnt include Tag_Frame_Header itself) */
		tfbsize=(ID3V2FrameHead[4]<<24) + (ID3V2FrameHead[5]<<16) + (ID3V2FrameHead[6]<<8) + ID3V2FrameHead[7];
/* TEST: -------- */
//		egi_dpstd("[ TagFrame ] TagFrameId: '%s', Frame body length: %d bytes\n", TagFrameID, tfbsize);

		/* W1.4 Notice TODO and TBD: There'are padding bytes when Frame body size is 0? */
		if(tfbsize<1) {
			egi_dpstd("Frame body length =0! All followed data are padding bytes?!\n");
			break;
		}

		/* W2. Process Tag Frame Body */
		/* W2.1 Get text encoding type from TextInfo frame, including COMM and APIC, which starts with Text_encoding.
		 *** NOTE: (see id3v2.3.0 Document)
		   >1. Text information frame body struct:
			  Text encoding    $xx
			  Information    <text string according to encoding>
		   >2. If nothing else is said a string is represented as ISO-8859-1 characters in the range $20 - $FF.
  		       Such strings are represented as <text string>, or <full text string> if newlines are allowed,
		       in the frame descriptions.
		   >3. Strings dependent on encoding is represented as <text string according to encoding>, or <full text
		       string according to encoding> if newlines are allowed.
		   >4. All Unicode strings use 16-bit unicode 2.0 (ISO/IEC 10646-1:1993, UCS-2). Unicode strings must
		       begin with the Unicode BOM ($FF FE or $FE FF) to identify the byte order.
		*/

		/*  All/Only text frame identifiers begin with "T", with the exception of the "TXXX" frame */
		if( (TagFrameID[0]=='T' && strncmp("TXXX",TagFrameID,4)!=0 ) /* Text inf frame */
		   || strncmp("COMM",TagFrameID,4)==0  			     /* Comment frame, begin with encodeType */
		   || strncmp("APIC",TagFrameID,4)==0			     /* Picture frame, begin with encodeType */
		   || strncmp("GEOB",TagFrameID,4)==0			     /* Encapsulated obj frame, begin with encodeType */
		  ) {
			/* Check if text information frame */
			if(TagFrameID[0]=='T')
				IsTextInfoFrame=true;
			else
				IsTextInfoFrame=false;

			if( fread(&encodeType, 1, 1, fil)!=1 ) {
				err=4;
				goto END_FUNC;
			}
//			egi_dpstd("encodeType: %d\n", encodeType);

			/* Reduce tfbsize */
			tfbsize -= 1;
		}
		else
			IsTextInfoFrame=false;

		/* W2.2 Parse Frame body */
		/* W2.2.1  -----> Case text information frames <------
		 * excluding 'COMM' and 'APIC' frames
 		 */
		if( IsTextInfoFrame ) {

			/* -----> Case Text_Information_Frame <-----
			 * TIT2: Title/songname/content description
			 * TALB: Album/Movie/Show title
			 * TPE1: Lead performer(s)/Soloist(s)
			 * TRCK: Track number/Position in set
			 */
			//if(strncmp("TIT2",TagFrameID,4)==0) {
			if(tfbsize>1024-1) {
				strcpy(strText,"<Too Big Size Text>");
				fseek(fil, tfbsize, SEEK_CUR);
				tfbsize=0; /* Reset tfbsize */
			}
			else {
				/* IF_1: 16-bits Unicode */
		   		if(encodeType==1) {
			  		/* --- NOTICE ---
					 *  All Unicode strings use 16-bit unicode 2.0 (ISO/IEC 10646-1:1993, UCS-2).
					 *  Unicode strings must begin with the Unicode BOM ($FF FE or $FE FF) to identify
					 *  the byte order.
					 */
					/* E1. Read Unicode Byte Order Mark */
				    	if( fread(&uniTmp, 1, 2, fil)!=2 ) {     /* Same as above */
						err=5; goto END_FUNC;
					}

					/* E2. Check byte order */
					/* TODO: Leave a pit here for BigEndian host. >>> */
					/* Byte order NOT same as host */
					if( uniTmp==0xFFFE ) { /* then original byte order is 'FEFF' */
						/* Big Endian */
						IsHostByteOrder=false;
						egi_dpstd(DBG_YELLOW"Byte order: BigEndia\n"DBG_RESET);
/* TEST -------- */
//						printf("Unicode Byte_Order_Mark: 'FEFF'. \n");
					}
					/* Byte order is the same as host */
					else  { /* uniTmp==0xFEFF,  then original byte order is 'FFFE' */
						/* Little Endian.  OK, NO need to convert endianess. */
						IsHostByteOrder=true;
						egi_dpstd(DBG_GREEN"Byte order: LittleEndia\n"DBG_RESET);
/* TEST -------- */
//						printf("Unicode Byte_Order_Mark: 'FFFE'. \n");
					}

					/* E3. Update tfbsize */
					tfbsize -=2;

					/* E4. Read all 16-bits Unicodes */
					memset(uniText, 0, sizeof(uniText));
					for(i=0; i<tfbsize/2; i++) {
						if( fread(&uniTmp,2,1,fil)!=1 ) {
							err=6; goto END_FUNC;
						}

						/* Endianess conversion */
						if(IsHostByteOrder)
							uniText[i]=(wchar_t)uniTmp;
						else
							//uniText[i]=(wchar_t)ntohl(&uniTmp);  //MidasHK_2022_03_17
							uniText[i]=(wchar_t)ntohl(uniTmp);

					}
					/* E5. Convert to UFT-8 */
					len=cstr_unicode_to_uft8(uniText, strText);
					if(len>0) {
						strText[len]='\0'; /* Necessary? --YES!!! */
/* TEST -------- */
						egi_dpstd("<--- %s ---> %s\n", TagFrameID, strText);
					}

					tfbsize=0; /* Reset tfbsize */
			   	}
				/* IF_2: UFT-8 encoding */
			   	else if(encodeType==3) {
					/* Read all UFT-8 string */
				    	if( fread(strText, tfbsize, 1, fil)!=1) {
						err=7; goto END_FUNC;
					}
					strText[tfbsize]='\0'; /* Necessary? --YES!!! */
/* TEST ------- */
					egi_dpstd("<--- %s ---> %s\n", TagFrameID, strText);

					tfbsize=0; /* Reset tfbsize */
				}
				/* IF_3: ASCII */
			   	else {
				    	if( fread(strText, tfbsize, 1, fil)!=1) {
						err=8; goto END_FUNC;
					}
					strText[tfbsize]='\0'; /* Necessary? --YES!!! */
/* TEST ------- */
					egi_dpstd("<--- %s ---> %s\n", TagFrameID, strText);

					tfbsize=0; /* Reset tfbsize */
			   	}
			}

			/*** Fill in EGI_ID3V2TAG struct
			 * TIT2: Title/songname/content description
			 * TALB: Album/Movie/Show title
			 * TPE1: Lead performer(s)/Soloist(s)
			 */
			if(strncmp("TIT2",TagFrameID,4)==0)
				mp3tag->title=strdup(strText);
			else if(strncmp("TALB",TagFrameID,4)==0)
				mp3tag->album=strdup(strText);
			else if(strncmp("TPE1",TagFrameID,4)==0)
				mp3tag->performer=strdup(strText);
		}
		/* W2.2.2  -----> Case COMM: Comment <------
			Text encoding           $xx
			Language                $xx xx xx
			Short content descrip.  <text string according to encoding> $00 (00)
						(Terminating string: ASIC $00,  Unicode $0000 )
			The actual text         <full text string according to encoding>
		*/
		else if(strncmp("COMM",TagFrameID,4)==0) {
			char lang[4];
			char strComm[1024];

			/* tfbsize: Text encoding already read out and tfbsize decreased. see 2.1 */

			/* C1. Read Language code */
                        if( fread(lang, 1, 3, fil)!=3 ) {
                        	err=9; goto END_FUNC;
                        }
			lang[4-1]='\0';

/* TEST ------- */
//			egi_dpstd("<--- Comments --->\n Language: %s\n", lang);

			/* C2. Update tfbsize */
			tfbsize -=3;

			/* C3. Read descriptor + actual text */
			if(tfbsize>1024-1) {
				strcpy(strComm,"<Too Big Size Comment>");
/* TEST ------- */
				egi_dpstd("----- Too big size comment!\n");
				fseek(fil, tfbsize, SEEK_CUR); /* Get to frame end. */
			}
			else {
				if( fread(strComm, tfbsize, 1, fil)!=1 ) {
					err=10; goto END_FUNC;
				}
				strComm[tfbsize]='\0';

/* TEST ------- */
				/* Descriptor */
//				egi_dpstd(" Descriptor: %s\n", strComm);
				/* Full comment text. Descriptor terminating: ASIC $00, (UTF-8 $00?)  Unicode $0000 */
//				egi_dpstd(" Content: %s\n", strComm+strlen(strComm)+(encodeType==1?2:1) );
			}

			/* C4. Fill in mp3tag */
			mp3tag->comment=strdup(strComm+strlen(strComm)+(encodeType==1?2:1));

			/* C5. fil seek pos OK */
			tfbsize=0; /* Reset tfbsize */
		}
		/* W2.2.3  ----->  Case APIC: Attached picture <-----
			Text encoding   $xx
			MIME type       <text string> $00
			Picture type    $xx
			Description     <text string according to encoding> $00 (00)
			Picture data    <binary data>
		*/
		else if(strncmp("APIC",TagFrameID, 4)==0) {

			/* tfbsize: Text encoding already read out and tfbsize decreased. see 2.1 */
			int ret;
			char mimeType[16]; /* image/, image/png, image/jpeg */
			char picType;
			char picDescript[128]; /* MAX. 64 */

			/* A1. Read MIME type */
			ret=egi_read_fileBlock(fil, mimeType, 1, 0);  /* fil, dest, tnulls, bsize */
			if(ret<0) {
				err=12; goto END_FUNC;
			}

			/* Update tfbsize */
			tfbsize -=ret;


			/* A2. Read picType */
			if( fread(&picType, 1, 1, fil)!=1 ) {
				err=13;	goto END_FUNC;
			}
			//egi_dpstd(" Picture type: %d\n", picType);

/* TEST ------- */
			egi_dpstd("<--- APIC ---> MIME Type: %s,  PicType: %02X\n", mimeType, picType);

/*------------  APIC Picture TYPE ---------------
$00     Other
$01     32x32 pixels 'file icon' (PNG only)
$02     Other file icon
$03     Cover (front)
$04     Cover (back)
$05     Leaflet page
$06     Media (e.g. lable side of CD)
$07     Lead artist/lead performer/soloist
$08     Artist/performer
$09     Conductor
$0A     Band/Orchestra
$0B     Composer
$0C     Lyricist/text writer
$0D     Recording Location
$0E     During recording
$0F     During performance
$10     Movie/video screen capture
$11     A bright coloured fish
$12     Illustration
$13     Band/artist logotype
$14     Publisher/Studio logotype
------------------------------------------------*/

			/* A3. Update tfbsize */
			tfbsize -=1;

			/* A4. Read Byte Order Mark if encoding in 16bits-Unicode */
			if( encodeType==1 ) {

                                       /* --- NOTICE ---
                                       *  All Unicode strings use 16-bit unicode 2.0 (ISO/IEC 10646-1:1993, UCS-2).
                                         *  Unicode strings must begin with the Unicode BOM ($FF FE or $FE FF) to identify
                                         *  the byte order.
                                         */
                                        /* Read Unicode Byte Order Mark */
                                        //if( fread(&uniTmp, 2, 1, fil)!=2 ) {
                        		if( fread(&uniTmp, 1, 2, fil)!=2 ) {     /* Same as above */
                                         	err=14; goto END_FUNC;
                                	}

                                        /* TODO: Leave a pit here for BigEndian host. >>> */
                                        /* Byte order NOT same as host */
                                        if( uniTmp==0xFFFE ) { /* then original byte order is 'FEFF' */
                                                /* Big Endian */
                                                IsHostByteOrder=false;
						egi_dpstd(DBG_YELLOW"Byte order: BigEndia\n"DBG_RESET);
/* TEST ------- */
//                                                printf("Unicode Byte_Order_Mark: 'FEFF'. \n");
                                        }
                                        /* Byte order is the same as host */
                                        else  { /* uniTmp==0xFEFF,  then original byte order is 'FFFE' */
                                                /* Little Endian.  OK, NO need to convert endianess. */
                                                IsHostByteOrder=true;
						egi_dpstd(DBG_GREEN"Byte order: LittleEndia\n"DBG_RESET);
/* TEST ------- */
//                                                printf("Unicode Byte_Order_Mark: 'FFFE'. \n");
                                        }

                                        /* Update tfbsize */
                                        tfbsize -=2;
			}

			/* A5. Read pic descritpion */
			ret=egi_read_fileBlock(fil, picDescript, encodeType==1?2:1, 0);  /* fil, dest, tnulls, bsize */
			if(ret<0) {
				err=15; goto END_FUNC;
			}

                        /* TODO: Convert to UFT-8  when  encodeType==1 and considering BOM. Refer to E4. */

			/* A6. Update tfbsize */
			tfbsize -=ret;
/* TEST ------- */
			egi_dpstd(" encodeType=%d, ret=%d, Description: %s\n", encodeType, ret, picDescript);

			/* A7. If tfbsize ERROR, too big value, out of ID3V2Tag zone. */
			fpos=ftell(fil);
			if(fpos+tfbsize > ID3V2TagSize)  {
				egi_dpstd("tfbsize ERROR, it's too big! and out of ID3V2Tag zone, Try to adjust to fit in...\n");
				tfbsize = ID3V2TagSize-fpos;
			}

			/* A8. Create picture file */
			FILE *fdest=fopen(TEMP_MP3APIC_PATH, "wb");
			if( fdest==NULL ) {
				err=16; goto END_FUNC;
			}
			if( egi_copy_fileBlock(fil, fdest, tfbsize)!=0 ) {
				fclose(fdest);
				err=17;	goto END_FUNC;
			}

			/* A9. Close tmp file */
			fclose(fdest);

			/* A10. Assign mp3tag->imgbuf */
			if(mp3tag->imgbuf==NULL)
				mp3tag->imgbuf = egi_imgbuf_readfile(TEMP_MP3APIC_PATH);
			else  /* More than 1 APIC frames! */
				egi_dpstd("   -----> More than 1 APIC found! <-----\n");

			tfbsize=0; /* Reset tfbsize */
		}
		/* W2.2.4  -----> Case SYLT: Synchronised lyrics/text  <-----
		 */
		// else if(strncmp("SYLT",TagFrameID, 4)==0) {
		//    --- Too complicated! Use TEXT tag seems OK! ---
		// }
		/* W2.2.5 -----> Case GEOB: General encapsulated object  <-----
			Text encoding           $xx
			MIME type               <text string> $00
			Filename                <text string according to encoding> $00 (00)
			Content description     $00 (00)
			Encapsulated object     <binary data>
		  */
		else if(strncmp("GEOB",TagFrameID, 4)==0) {
			/* tfbsize: Text encoding already read out and tfbsize decreased. see 2.1 */
			int ret;
			char mimeType[16]; /* image/, image/png, image/jpeg */

			/* Read MIME type */
			ret=egi_read_fileBlock(fil, mimeType, 1, 0);  /* fil, dest, tnulls, bsize */
			if(ret<0) {
				err=18; goto END_FUNC;
			}
			egi_dpstd("<--- GEOB --->  MIME_Type: %s\n", mimeType);

			/* Update tfbsize */
			tfbsize -=ret;

			/* TODO: Get object */

			/* Go to end of the Frame Body */
			if( fseek(fil, tfbsize, SEEK_CUR) !=0) {
				err=18; goto END_FUNC;
			}

			tfbsize=0; /* Reset tfbsize */
		}
		/* W2.2.6  -----> Case ELSE Tags <----- */
		else {
			/* Go to end of the Frame Body */
			if( fseek(fil, tfbsize, SEEK_CUR) !=0) {
				err=19; goto END_FUNC;
			}

			tfbsize=0; /* Reset tfbsize */
		}

		/* Prepare for next tag frame */
NEXT_FRAME:
		/* W3. Just move fil pos and clear tag data */
		if(tfbsize) {
			if( fseek(fil, tfbsize, SEEK_CUR) !=0) {
				err=20; goto END_FUNC;
			}
		}

		/* W4. Update fpos */
		fpos=ftell(fil);

/* TEST: ---------- */
		/* Check fpos relative to ID3V2TagSize */
//		egi_dpstd("NOW fpos=%ld of %u\n", fpos, ID3V2TagSize);

	} /* End while() */


END_FUNC:
	/* Close file */
	if(fil) fclose(fil);

	if(err) {
		egi_id3v2tag_free(&mp3tag);
		return NULL;
	}
	else
		return mp3tag;
}


////////////////////////////////////////////////////////////

/*--------------------------------------------
Adjust PWM duty cycle for backlighting.
Stroboflash frequency is set at 100KHz.

Refrence: http://blog.csdn.net/qianrushizaixian/article/details/46536005

@pwmnum:  PWM number.
          0 0R 1.
@pget:	  Pointer to pass out backlight PWM duty value read from ioctl.
@pset:    Pointer to pass in backlight PWM duty value to set to ioctl.
	  Percentage value of duty cycle. [0 100]
	  If getvalue <0, PWM is NOT enabled.
@adjust:  Adjust value, to be added on getvalue as setvaule.
	  If pset==NULL, then added on cfg.getduty.

Return:
        0       OK
        <0      Fail
----------------------------------------------*/
#include <sys/ioctl.h>
#include "/home/midas-zhou/Ctest/kmods/soopwm/sooall_pwm.h"
int egi_getset_backlightPwmDuty(int pwmnum, int *pget, int *pset, int adjust)
{
        int pwm_fd;
        struct pwm_cfg  cfg;
	int duty;
        int ret;

        /* Open PWM dev */
        pwm_fd = open("/dev/"PWM_DEV_NAME, O_RDWR);
        if (pwm_fd < 0) {
                egi_dperr("open pwm failed");
                return -1;
        }

        /* Ioctl to get duty */
       	cfg.no=pwmnum;
        if( ioctl(pwm_fd, PWM_GETDUTY, &cfg)<0 ) {
		egi_dperr("ioctl PWM_GETDUTY fails");
		close(pwm_fd);
               	return -1;
	}

	/* To pass out duty value */
	if(pget)
	     *pget=cfg.getduty;

	/* Need NOT to set */
	if( pset==NULL && adjust==0) {
		close(pwm_fd);
		return 0;
	}

	/* Set duty */
	if(pset)
	    duty=*pset;
	else if(pget)
	    duty= (*pget)>=0 ? (*pget)+adjust : 0+adjust;  /* <0 NOT enabled */
	else /* In case pget and pset are both NULL*/
	    duty=cfg.getduty+adjust;

        /* Limt set value */
        if(duty<0)duty=0;
	if(duty>100)duty=100;

        /* Set PWM parameters for old/classic mode. */
        cfg.no        =   pwmnum?1:0;    /* 0:pwm0, 1:pwm1 */
        cfg.clksrc    =   PWM_CLK_40MHZ; /* 40MHZ or 100KHZ */
        cfg.clkdiv    =   PWM_CLK_DIV4;  /* DIV2 40/2=20MHZ */
        cfg.old_pwm_mode =true;          /* TRUE: old mode; FALSE: new mode */
        cfg.idelval   =   0;
        cfg.guardval  =   0;
        cfg.guarddur  =   0;
        cfg.wavenum   =   0;             /* forever loop */
        cfg.datawidth =   100;           /* limit 2^13-1=8191 */
        cfg.threshold =   duty;          /* Duty cycle */

        /*** Check period: Min. BackLight PWM Frequence(stroboflash) >3125Hz?
         *  100KHz CLK: period=1000/100(KHZ)*(DIV(1-128))*datawidth   (us)
         *  40MHz CLK: period=1000/40(MHz)*(DIV)*datawidth            (ns)
         */
        if(cfg.old_pwm_mode == true) {
           if(cfg.clksrc == PWM_CLK_100KHZ) {
                egi_dpstd("Set PWM period=%d us\n",(int)(1000.0/100.0*(1<<cfg.clkdiv)*cfg.datawidth));
           }
           else if(cfg.clksrc == PWM_CLK_40MHZ) {
                /* PWM_CLK_40MHZ, PWM_CLK_DIV4, datawidth=100 --> period ==10us (100KHz) */
                egi_dpstd("Set PWM period=%d ns\n",(int)(1000.0/40.0*(1<<cfg.clkdiv)*cfg.datawidth));
          }
        }
        else  /* NEW mode */
                egi_dpstd("Set PWM senddata0: %#08x  senddata1: %#08x \n",cfg.senddata0,cfg.senddata1);

        /* Configure and enable PWM */
        if( ioctl(pwm_fd, PWM_CONFIGURE, &cfg)<0 || ioctl(pwm_fd, PWM_ENABLE, &cfg)<0 )
                ret=-1;

	/* Close pwm dev */
        close(pwm_fd);

        return ret;
}


/*-----------------------------------------------------------------------------------------------
Extract AAC audio data from a (MPEG-2) Transport Stream packet file fts, and it save to faac.

Reference:
  1. https://blog.csdn.net/leek5533/article/details/104993932
　2. ISO/IEC13818-1 Information technology — Generic coding of moving pictures and associated
     audio information: Systems


Abbreviations
   PAT --- Program Association Table, list Program_Number and related PMT PID.
   PMT --- Program Map Table, list Elementary Stream type and PID.
	   If ONLY one program, then PMT maybe omitted!??? <--------
   CAT --- Condition Access Table
   NIT --- Network Information Table

   PSI --- Program Specific Information. (PAT,PMT,CAT,...etc)
   PES --- Packetized Elementary Stream

Note:
1. Suppose there's ONLY one audio(and/or one video) stream in ts file.
   In this case PMT is NOT necessary.

	      ( TS packets ---> PES packets ---> ES packets )

   --- TS packet ---
   TS Header(4Bs) + AdaptationField(option)(XBs) + Payload(184-X Bs)

   --- PES packet >>>>> TS Playload ---
   TS Playlod = PES Header(6Bs) + OptionHeader(xBs) + Payload(xBs)

   --- ES unit  <<<<< PES Payloads ---
   An AAC Audio ES uint(a AAC frame):  ADTS_Header(7Bs) + AAC data(xBs)
   An H264 Video ES uint(a H264 frame):  Start_Code](3or4Bs) + NALU_Header(1Bs) + H264 data(xBs)


2. A Transport Stream packet consists of:
   TS Header(4Bs) + AdaptationField(option)(XBs) + Payload(184-X Bs)
   1.1 A Transport Stream(TS) is a multiplex of Elementary Streams.
   1.2 A TS packet has 188 bytes totally, or 204 bytes with CRC. (TS_204 NOT supported here).
   1.3 PAT and PMT has no adaption field, and are filled with stuffing data.
   1.4 TS PID may NOT be in sequence, as TS packets carrying Audio/Video ES data may be
 	mixed or interlaced.
	However TS PID for the same Audio(or Video) ES stream MUST be in sequence.

3. A PES packet consists of:
   PES Header(6Bs) + OptionHeader(xBs) + Payload(xBs)

   2.1 A PES packet is divided into stream data blocks, each block is packed into a TS packet as payload.
   2.2 A PES packet == a playload_unit
   2.3 (payload_unit_start_indicator==1 && packet_start_code_prefix) indicates start of a PES packet.

4. Typical strucutre of ES data:
   4.1 An ES unit(a Frame etc.) is divied into data blocks, each block is packed into a PES packet as payload.
	An AAC Audio ES uint(a AAC frame):  ADTS_Header(7Bs) + AAC data(xBs)

	(H264 Annexb byte-stream)
   	An H264 Video ES uint(a H264 frame):  Start_Code](3or4Bs) + NALU_Header(1Bs) + H264 data(xBs)

5. Audio data demuliplex/extraction procedure:
   1.Read TsHeader
     ---> 2.Read TsAdaptation Field
        ---> 3. Read Payload Data
	       3.1 Case: TsHead.PID==0 (PMT_PID):
			  Clear/init pcnt,...
			  Get pmt_PID, nit_PID. ---> Reel back to end of TsHeader
	       3.2 Case: TsHead.PID==pmt_PID: Parse PMT ---> Reel back to end of TsHeader
               3.3 Other case:  PES data
		   3.3.1 IF(TsHeader.payload_unit_start_indicator==1 && packet_start_code_prefix==1)
			 Read a new PES Header + PES palylaod data.
		   3.3.2 ELSE
			 Read PES playload data.
	           3.3.4 Save PES playload data as ES, addup to be a complet unit/frame.

   <---- Skip to next TsHeader from end of current TsHeader.

TODO:
1. Ffplay faout with msg: skip_data_stream_element: Input buffer exhausted before END element found
2. TS_204 NOT supportd yet.
3. In some case audio stream is NOT marked by stream_id = 110x xxxx.
   PID = 0x0101 default as audio stream??
4. If PES_video_length==0, it indicates that all palyload is PES data???
   and PES_audio_length SHOULD NOT be ommitted.???

Journal:
2022-06-30: Create the file.
2022-07-01/02: Extract ES and save to faout.
2022-07-02: Put to egi_utils.c

@fts:  TS packet file path.
@faac: Output AAC file path.
	If exits, it will be trauncated.
@fvd:  Output video file path.
	If exits, it will be trauncated.

Return:
	0 	OK
	<0	Fails
-----------------------------------------------------------------------------------------------*/
int egi_extract_AV_from_ts(const char *fts, const char *faac, const char *fvd)
{
     //////////////////  TS_HEADER, TS_ADPT_FIELD   ////////////////////
     typedef struct ts_header {   /* Total 4Bytes */
	/*** BitsMark
	 * bslbf:   Left bit first.
	 * tcimsbf: Two’s complement integer, msb (sign) bit first
	 * uimsbf:  Unsigned integer, most significant bit first
	 * vlclbf:  Variable length code, left bit first,
	 */
	uint8_t sync_byte; 		    	/* 8b bslbf,  0x47 */
	uint8_t transport_error_indicator;  	/* 1b bslbf, usually 0 */
	uint8_t payload_unit_start_indicator; 	/* 1b bslbf */
	uint8_t transport_priority;		/* 1b bslbf */
	uint16_t PID;			  	/* 13 uimsbf */
	uint8_t transport_scrambling_control;   /* 2 bslbf, 00 - no scrambling/encroption */
	uint8_t adaptation_field_control;	/* 2 bslfb
						 *  00- Reserved
					 	 *  01- No adaptation_field, playload only
						 *  10- Adapatation_field only, no payload
						 *  11- Adapatation_field followed by payload
						 */
	uint8_t continuity_counter;		/* 4 uimsbf */
     } TS_HEADER;

     typedef struct ts_adapt_field {
	/* This is from adaptation_field ONLY if it exists */
	uint8_t adaptation_field_length;

	uint8_t discontinuity_indicator;
	uint8_t random_access_indicator;
	/* more... */

     } TS_ADPT_FIELD;
     ///////////////////////////////////////////////////////////////

	FILE *fil=NULL;
	FILE *faout=NULL;    /* For audio output */
	FILE *fvout=NULL;    /* For video output */
	int err=0;

	TS_HEADER 	TsHeader;
	TS_ADPT_FIELD 	TsAdptField;
	uint8_t 	TsHeaderBytes[4];
	const int TS_PSIZE=188;  /* TS packet size, No CRC. TS_204 NOT supported here. */
	long  		fpos;    /* current fil file position */

	uint16_t 	PES_audio_length=0; /* Audio PES packet length  */
	uint16_t 	PES_video_length=0; /* Video PES packet length  */

        uint16_t 	program_map_PID=0; /* PID=0 will parsed as PID first, */
//	bool 	        IsAudioStream=false;
	int		TsAudioPID=0;	/* TS PID for audio */
	int 		TsVideoPID=0;	/* TS PID for video */
	int hcnt=0;     /* TsHeader counter */
	char data[204]; /* Form temp. data <TS_PSIZE */
	int datasize;
	int dacnt=0;	/* Data byter counter for one audio PES packet */
	int dvcnt=0;	/* Data byter counter for one audio PES packet */
	int pacnt=0;	/* Audio PES packet counter */
	int pvcnt=0;	/* Vidoe PES packet counter */

	if(fts==NULL || faac==NULL)
		return -1;

	/* Open ts file */
        fil=fopen(fts, "rb");
        if(fil==NULL) {
                egi_dperr("Fail to open '%s'", fts);
                err=-1; goto END_FUNC;
        }

	/* Open file for Audio output */
	faout=fopen(faac, "wb");
	if(faout==NULL) {
		egi_dperr("Fail to open/create faout!");
		err=-1; goto END_FUNC;
	}

	/* Open file for Video output */
	if(fvd) {
		fvout=fopen(fvd, "wb");
		if(fvout==NULL) {
			egi_dperr("Fail to open/create fvout!");
			err=-1; goto END_FUNC;
		}
	}


   /* Loop parse TS packets data */
   while(1) {

	/* 1. Read TsHeader */
        /* 1.1 Read TsHeaderBytes[], 4Bytes */
        if( fread(TsHeaderBytes, 4, 1, fil)!=1 ) {
		if(feof(fil)) {
			egi_dpstd("End of file!\n");
			break;
		}
		else{
	                egi_dperr("Fail to read TsHeaderBytes!");
        	        err=-2;  goto END_FUNC;
		}
        }
	/* 1.2 Fill in TsHeader */
	TsHeader.sync_byte = TsHeaderBytes[0];  /* 8b */
	TsHeader.transport_error_indicator = TsHeaderBytes[1]>>7; /* 1b */
	TsHeader.payload_unit_start_indicator = (TsHeaderBytes[1]&0b01000000)>>6; /* 1b */
	TsHeader.transport_priority = (TsHeaderBytes[1]&0b00100000)>>5;  /* 1b */
	TsHeader.PID = ((TsHeaderBytes[1]&0b00011111)<<8)+TsHeaderBytes[2];  /* 13b */
	TsHeader.transport_scrambling_control = TsHeaderBytes[3]>>6;   /* 2b */
	TsHeader.adaptation_field_control = (TsHeaderBytes[3]&0b00110000)>>4; /* 2b */
	TsHeader.continuity_counter = (TsHeaderBytes[3]&0b00001111); /* 4b */

	/* 1.3 Count TsHeader */
	hcnt++;

	/* 1.4 Check SYNC BYTE, Suppose its' TS_188, NOT TS_204 */
	if(TsHeader.sync_byte!=0x47) {
		egi_dpstd(DBG_RED"sync_byte error! or it's TS_204."DBG_RESET);
		err=-3; goto END_FUNC;
	}

#if 0  /* TEST: ------------ */
	egi_dpstd("TsHeader PID: 0x%04X,  payload_unit_start_indicator: %d, contiCounter: %d, adaptation_field_control: %d, %s + %s\n",
		TsHeader.PID, TsHeader.payload_unit_start_indicator, TsHeader.continuity_counter,
		TsHeader.adaptation_field_control,
		(TsHeader.adaptation_field_control&0b10)?"AdaptionField":"NoAdaptionField",
		(TsHeader.adaptation_field_control&0b01)?"Payload":"NoPayLoad" );
#endif


        /* 1.5 Get current file position as end of TsHeader, later to reel back here. */
        fpos=ftell(fil);

	/* 2. Read TsAdptField if adaptation_field exists. */
	if(TsHeader.adaptation_field_control&0b10) {
//		egi_dpstd(" --- Adaptation Field ---\n");
	   	/* Read adaptation_field_length */
           	if( fread(&TsAdptField.adaptation_field_length, 1, 1, fil)!=1 ) {
                	egi_dperr("Fail to read adaptation_field_length!");
                	err=-4; goto END_FUNC;
           	}
//	   	egi_dpstd("adaptation_field_length: %d\n", TsAdptField.adaptation_field_length);
//	   	egi_dpstd(" -----------\n");

		/* Skip adaptation field */
		if( fseek(fil, TsAdptField.adaptation_field_length, SEEK_CUR)!=0 ) {
			egi_dpstd("fseek to skip adaptation field error!\n");
			err=-4; goto END_FUNC;
		}
	}

/*** PID Table --------------------------------------------
	0x0000		  Program Associaton Table
	0x0001  	  Conditional Access Table
	0x0002  	  Transport Stream Descripton Table
	0x0003-0x000F 	  Reserved
	0x00010...0x1FFE  May be assigned as network_PID, Program_map_PID, elementary_PID,...
	0x1FFF 		  Null packet
----------------------------------------------------------*/

	/* 3. ===== Read and parse payload data of TS packets ====== */

	/* 3.1 Parse PAT(Program Association Table), reel back to fpos at end.
	 *    Notice that No adaptation field for a PAT ts.
	 */
	if(TsHeader.PID==0) {
	   //egi_dpstd(" --- PAT ---\n");

	   /*** Program Association Table
		  table_id   			8b	uimsbf
		  section_syntax_indicator	1b	bslbf
		  '0'				1b      bslbf
		  reserved			2b      bslbf
		  section_length		12b     uimsbf
		  transport_stream_id		16b	uimsbf
		  reserved                      2b	bslbf
		  version_number		5b	uimsbf
		  current_next_indicator	1b	bslbf   1--Current applicable, 0--Not yet applicable
		  section_number		8b	uimsbf
		  last_section_number		8b      uimsbf
		  for( i=0; i<N; i++) {
			program_number		16b     uimsbf
			reserved		 3b	bslbf
			if(program_number=='0')
			   network_PID(NIT) 	13b	uimsbf
			else
			   program_map_PID(PMT)	13b    	uimsbf
		  }
		  CRC_32			32b	uimsbf
	    ***/

		int i;
		uint8_t  table_id;
		uint16_t transport_stream_id;
		uint8_t	 current_next_indicator;
		uint8_t  last_section_number;
		uint8_t  bytes[2];
		uint16_t program_number;
		uint16_t sectPID;

		/* 3.1.1 Get table_id */
	        if( fread(&table_id, 1, 1, fil)!=1 ) {
        	        egi_dperr("Fail to read table_id!");
                	err=-4; goto END_FUNC;
        	}
		//egi_dpstd("table_id=0x%02X\n",table_id);

/*** Table_ID assignment values  --------------
 *	0x00	Program Association Section
 *      0x01    Conditional Access Section
 *	0x02    TS Program Map Section
 *	0x03    TS Description Section
 *	....
 ---------------------------------------------*/

		/* Check table_id */
		if(table_id!=0x00) {
			egi_dpstd(DBG_RED"Error, PAT table_id!=0x00"DBG_RESET);
			/* Skip following data */
			goto REEL_BACK;
		}

		/* 3.1.2 Clear IsAudioStream,dacnt...BEFORE next PES starts! */
		/* Not necessary? To clear it at 3.3.5.1 and 3.3.5a.1 */
//		dacnt=0; dvcnt=0;

		/* 3.1.3 Get transport_stream_id */
		if( fseek(fil, 2, SEEK_CUR)!=0 ) {  /* bypass 2bytes */
			egi_dpstd("fseek error!\n");
			err=-4;	goto END_FUNC;
		}
		if( fread(bytes, 2, 1, fil)!=1 ) {
                       	egi_dperr("Fail to read transport_stream_id!");
       	                err=-4; goto END_FUNC;
                }
		transport_stream_id=(bytes[0]<<8)+bytes[1];
		//egi_dpstd("transport_stream_id=%d\n",transport_stream_id);

		/* 3.1.3 Get current_next_indicator */
		if( fread(bytes, 1, 1, fil)!=1 ) {
                        egi_dperr("Fail to read current_next_indicator!");
                        err=-4; goto END_FUNC;
                }
		current_next_indicator=bytes[0]&0b1;
		//egi_dpstd("current_next_indicator=%d\n",current_next_indicator);

		/* 3.1.4 Get last_section_number */
		if( fseek(fil, 1, SEEK_CUR)!=0 ) {  /* bypass 1bytes */
			egi_dpstd("fseek error!\n");
			err=-4; goto END_FUNC;
		}
	        if( fread(&last_section_number, 1, 1, fil)!=1 ) {
        	        egi_dperr("Fail to read last_section_number!");
                	err=-4; goto END_FUNC;
        	}
		//egi_dpstd("last_section_number=%d, totally %d sections.\n",last_section_number,last_section_number+1);

		/* 3.1.5 Read all sections */
		for(i=0; i< last_section_number+1; i++) {
			/* Read program_number */
			if( fread(bytes, 2, 1, fil)!=1 ) {
                        	egi_dperr("Fail to read program_number!");
        	                err=-4; goto END_FUNC;
	                }
			program_number=(bytes[0]<<8)+bytes[1];
			//egi_dpstd("program_number: %d\n", program_number);
			/* Read sectPID */
			if( fread(bytes, 2, 1, fil)!=1 ) {
                                egi_dperr("Fail to read sectPID!");
                                err=-4; goto END_FUNC;
                        }
			/* Check reserved as 0b111 */
			//egi_dpstd("PAT Reserved mark: %d\n", (bytes[0]>>5));

			/* Get PID */
			sectPID = ((bytes[0]&0b00011111)<<8)+bytes[1];   /* 0x01F0 --> 0x1001 */
			if(program_number==0) {
				//egi_dpstd("network_PID: 0x%04X\n", sectPID);
			}
			else {
				//egi_dpstd("program_map_PID: 0x%04X\n", sectPID);
				program_map_PID=sectPID;
			}
		}


#if 0 /* TEST: ----------- */
	   	egi_dpstd(" --- PAT ---\n");
		egi_dpstd("table_id=0x%02X\n",table_id);
		egi_dpstd("transport_stream_id=%d\n",transport_stream_id);
		egi_dpstd("current_next_indicator=%d\n",current_next_indicator);
		egi_dpstd("last_section_number=%d, totally %d sections.\n",last_section_number,last_section_number+1);
		egi_dpstd("program_number: %d\n", program_number);
		egi_dpstd("PAT Reserved mark: %d\n", (bytes[0]>>5));
		egi_dpstd("network_PID: 0x%04X\n", sectPID);
	        egi_dpstd(" -----------\n");
#endif

		/* 3.1.6 Skip CRC to the end */
		if( fseek(fil, 4, SEEK_CUR)!=0 ) {
			egi_dpstd("CRC size error!\n");
			err=-4;  goto END_FUNC;
		}


REEL_BACK:
		/* 3.1.7 Reel back to end of current TsHeader */
		if( fseek(fil, fpos, SEEK_SET)!=0 ){
			egi_dperr("fseek fpos");
			err=-4; goto END_FUNC;
		}
	}

	/* 3.2 Parse Program_Map_Table(PMT): If only 1 program in TS, then PMT will NOT exist!??? */
#if 1
        else if(TsHeader.PID==program_map_PID) {
	        egi_dpstd(DBG_CYAN" --- PMT ---\n"DBG_RESET);
		//egi_dpstd("TsHeader.PID==progrem_map_PID\n");
		exit(0);
	}
#else
        else if(TsHeader.PID==program_map_PID) {
		//egi_dpstd("TsHeader.PID==progrem_map_PID\n");

	   /*** Program Association Table
		  table_id   			8b	uimsbf
		  section_syntax_indicator	1b	bslbf
		  '0'				1b      bslbf
		  reserved			2b      bslbf
		  section_length		12b     uimsbf
		  program_number		16b	uimsbf
		  reserved			2b	bslbf
		  version_number		5b	uimsbf
		  current_next_indicator	1b	bslbf
		  section_number		8b	uimsbf
		  last_section_number		8b	uimsbf
		  reserved			3b	bslbf
		  PCR_PID			13b	uimsbf
		  reserved			4b	bslbf
		  program_info_length		12b	uimsbf
		  for(i=0; i<N; i++)
			descriptor()
		  for(i=0; i<N1; i++) {
			stream_type		8b	uimsbf
			reserved		3b	bslbf
			elementary_PID		13b	uimsbf
			reserved		4b	bslbf
			ES_info_length		12b	uimsbf
			for(i=0; i<N2; i++)
				descriptor()
		  }
		  CRC_32			32b	rpchof
	    **/

		uint8_t  table_id;
		uint8_t  bytes[2];
		uint16_t program_number;

	        egi_dpstd(" --- PMT ---\n");

		/* M1. Get table_id */
	        if( fread(&table_id, 1, 1, fil)!=1 ) {
        	        egi_dperr("Fail to read table_id!");
                	err=-4; goto END_FUNC;
        	}
		egi_dpstd("table_id=0x%02X\n",table_id);

		/* Get program_number */
		if( fseek(fil, 2, SEEK_CUR)!=0 ) {  /* bypass 2bytes */
			egi_dpstd("fseek error!\n");
			err=-4; goto END_FUNC;
		}
	        if( fread(&bytes, 2, 1, fil)!=1 ) {
        	        egi_dperr("Fail to read program_number!");
                	err=-4; goto END_FUNC;
        	}
		program_number=(bytes[0]<<8)+bytes[1];
		egi_dpstd("program_number: %d\n", program_number);

	        egi_dpstd(" -----------\n");

		/* Reel back to end of current TsHeader */
		if( fseek(fil, fpos, SEEK_SET)!=0 ){
			egi_dperr("fseek fpos");
			err=-4; goto END_FUNC;
		}
	}
#endif

	/* 3.3 Parse Program_Elementary_Stream(PES) packet ----- */
	/* Note:
	 *  1. Suppose there's ONLY one programe with one Audio(and/or one Video)stream(s) in TS packets.
	 *     In this case, PMT is NOT necessary.
	 *  2. Suppose other PIDs as for PES.
	 */
        //if(TsHeader.PID==0x0101 || TsHeader.PID==0x0100 || TsHeader.PID==0x1001 ) {

	else {
	   /***  PES  Header
		  packet_start_code_prefix	24b	bslbf   // ==0x000001
		  stream_id			8b	uimsbf
			1011 1100	program_stream_map
			1011 1101	private_stream_1
			1011 1110	padding_stream
			1011 1111	private_stream_2
			110x xxxx	Audio stream number x xxxx
			1110 xxxx	Video stream number xxxx

		  PES_packet_length		16b	uimsbf
			....
	  ***/

		uint8_t bytes[4];
		bool IsPaddingStream=false;
		uint32_t packet_start_code_prefix;
		uint8_t stream_id;
		uint8_t PES_header_data_length; /* NOT PES_header length! */

#if 0		/* NOT HERE, Just after 3.3.5.2 */
	        egi_dpstd(" --- PES PID:0x%04x [%s] ---\n", TsHeader.PID,
			TsHeader.PID==TsAudioPID?"Audio":(TsHeader.PID==TsVideoPID?"Video":"Unkn") );
#endif

		/* 3.3.1 Skip adaptation field if exists....OK */
		/* NOW fil points to the TS payload data */

		/* 3.3.2 Get packet_start_code_prefix */
	        if( fread(bytes, 3, 1, fil)!=1 ) {
        	        egi_dperr("Fail to read packet_start_code_prefix!");
                	err=-4; goto END_FUNC;
        	}
//		egi_dpstd("packet_start_code_prefix: 0x%02X%02X%02X\n",bytes[0],bytes[1],bytes[2]);
		packet_start_code_prefix=(bytes[0]<<16)+(bytes[1]<<8)+bytes[2];

/* ------------> Start of a new EPS packet */
     	 if(packet_start_code_prefix==1) {

		/* 3.3.3 Get stream_id */
	        if( fread(&stream_id, 1, 1, fil)!=1 ) {
        	        egi_dperr("Fail to read PES stream_id!");
	                	err=-4; goto END_FUNC;
        	}

#if 0 /* TEST: --------- */
		egi_dpstd("PID:0x%04x, PES stream_id: ", TsHeader.PID);
		cstr_print_byteInBits((char)stream_id, "\n");
#endif

		if(stream_id==0b10111100) /* program_stream_map */
			egi_dpstd(DBG_GRAY"program_stream_map\n"DBG_RESET);
		else if(stream_id==0b10111110) {
			egi_dpstd(DBG_CYAN"...Padding stream....\n"DBG_RESET);
			IsPaddingStream=true;
		}

		/* 3.3.4 Get PES_packet_length, if found packet_start_code_prefix. */
		/* Read out PES_packet_length field, MAYBE invalid! */
	        if( fread(bytes, 2, 1, fil)!=1 ) {
       		        egi_dperr("Fail to read PES_packet_length!");
               		err=-4;  goto END_FUNC;
       		}

		/* 3.3.4a Audio/Video Stream id */
		if ( (stream_id>>4)==0b1110 ) {
//			egi_dpstd(DBG_BLUE"Video stream number %d\n"DBG_RESET, stream_id&0b00001111);
			/* Get TsVideoPID */
			TsVideoPID = TsHeader.PID;
		}
		else if ( (stream_id>>5)==0b110 ) {
//			egi_dpstd(DBG_GREEN"Audio stream number %d\n"DBG_RESET, stream_id&0b00011111);
			/* Get TsAudioPID */
			TsAudioPID = TsHeader.PID;
		}
		else {
			egi_dpstd("Unknown PES\n");
		}

		/* 3.3.5 Start of a Audio PES packet,  TsHeader payload_unit_start_indicator MUST be 1
		 * Note: start of PES packet identified by PUSI bit in TS header */
		if(  (TsHeader.PID==TsAudioPID  || TsHeader.PID==0x0101)   /* TODO PID 0x0101 default as audio stream? */
		     && TsHeader.payload_unit_start_indicator==1 && packet_start_code_prefix==1 ) {

			/* 3.3.5.1 Finish last TS for the PES packet */
			if(dacnt) {
			    if(dacnt!=PES_audio_length)
			        egi_dpstd(DBG_RED"Error! Last Audio PES packet dacnt=%dBs, while PES_audio_length=%dBs\n"DBG_RESET,
					dacnt, PES_audio_length);
			    else {
//			        egi_dpstd(DBG_GREEN"Last Audio PES packet dacnt=%dBs, while PES_audio_length=%dBs\n"DBG_RESET,
//					dacnt, PES_audio_length);
			    }
			    dacnt=0;
			    pacnt++; /* Finish a complet PES packet */
			}

			/* 3.3.5.2 Assign PES_audio_length NOW for the coming PES packet. */
			PES_audio_length=(bytes[0]<<8)+bytes[1];
//			egi_dpstd(DBG_GREEN"PES_audio_length: %d\n"DBG_RESET, PES_audio_length);

#if 1			/* 3.3.5.3 If stream_id NOT 'padding _stream' OR 'private_stream_2'
			   Then followig data is attached.
			   !!! NOTE: For audio AAC, following data can be omitted, BUT for video, it MUST be included in PES_video_length !!!
				1_Byte flag
					bits [7:6 fixed as 10][5:4 scrambling control][3 priority][2 alignment][1 copyright][0 original]
				1_Byte flag
					bits [7 PTS][6 DTS][5 ESCR][4 ESrate][3 DSM trickmode][2 additional info][1 CRC][0 Extension flags]
				1_Byte
					x=PES_header_data_length (bytes of data to follow, if preceding flag bits are set)

				x_Byte:  Following data, usually PTS,DTS data.
		 	*/
			if(stream_id!=0b10111110 && stream_id!=0b10111111) {
			        if( fread(bytes, 3, 1, fil)!=1 ) {
        			        egi_dperr("Fail to read packet_start_code_prefix!");
                			err=-4; goto END_FUNC;
        			}
				PES_header_data_length=bytes[2];
				egi_dpstd(" --- PES_header_data_length: %d ---\n", PES_header_data_length);
				/* Skip following header data */
				if(fseek(fil, PES_header_data_length, SEEK_CUR)!=0) {
        			        egi_dperr("Fail to fseek to skip PES_header_data_length!");
                			err=-4; goto END_FUNC;
				}

				/* Deduct PES_audio_length */
				PES_audio_length -= 3+PES_header_data_length;
			}
#endif

		}
		/* 3.3.5a  Start of a Vidoe PES packet */
		else if( TsHeader.PID==TsVideoPID
				&& TsHeader.payload_unit_start_indicator==1 && packet_start_code_prefix==1 ) {

			/* 3.3.5a.1 Finish last TS for the PES packet */
			if(dvcnt) {
			    /* For video stream, PES_video_length MAY be 0! */
			    if(PES_video_length>0) {
			   	if( dvcnt!=PES_video_length )
				        egi_dpstd(DBG_RED"Error! Last Video PES packet dvcnt=%dBs, while PES_video_length=%dBs\n"DBG_RESET,
						dvcnt, PES_video_length);
			    	else
			        	egi_dpstd(DBG_GREEN"Last Video PES packet dvcnt=%dBs, while PES_video_length=%dBs\n"DBG_RESET,
						dvcnt, PES_video_length);
			    }
			    dvcnt=0;
			    pvcnt++; /* Finish a complet PES packet */
			}

			/* 3.3.5a.2 Assign PES_video_length NOW for the coming PES packet. */
			PES_video_length=(bytes[0]<<8)+bytes[1];
//			egi_dpstd(DBG_GREEN"PES_video_length: %d\n"DBG_RESET, PES_video_length);

		}

	   }
/* <------------  Not start of a new PES packet, rewind back, to read it as PES data. */
	 else {
		fseek(fil, -3, SEEK_CUR);
	 }

#if 0 /* TEST: ------------ */
	        egi_dpstd(" --- PES PID:0x%04x [%s] ---\n", TsHeader.PID,
			TsHeader.PID==TsAudioPID?"Audio":(TsHeader.PID==TsVideoPID?"Video":"Unkn") );
#endif
		/* 3.3.6 Read Audio PES data */
		if( (TsHeader.PID == TsAudioPID || TsHeader.PID==0x0101 )   /* TODO PID 0x0101 default as audio stream? */
		    && PES_audio_length>0 ){
			/* TODO: padding or other type data */
			//printf("ftell(fil)=%ld, fpos=%ld, ftell-fpos=%ld\n", ftell(fil), fpos, ftell(fil)-fpos);
			datasize=TS_PSIZE-(ftell(fil)-(fpos-4));  /* 4-sizeof(TsHeader), NOW ftell() at end of PES Header */

//			egi_dpstd("Audio PES_audio_length=%d, datasize=%d, dacnt=%d\n", PES_audio_length,datasize,dacnt);
			/* Get rid of tail data ??? This indicates data error~ */
			if( dacnt+datasize > PES_audio_length ) {
				egi_dpstd(DBG_RED"Audio adjust datasize...\n"DBG_RESET);
				datasize=PES_audio_length-dacnt;
			}
			if( fread(data, datasize, 1, fil)!=1 ) {
				if(datasize>0) {  /* !!!datasize may be 0! */
					egi_dperr("Fail to read Audio PES data!\n");
					err=-4; goto END_FUNC;
				}
			}
			/* Write to faout */
			if( fwrite(data, datasize, 1, faout)!=1 ) {
				if(datasize>0) { /* !!!datasize may be 0! */
					egi_dperr("Fail to write Audio data to faout!");
					err=-4; goto END_FUNC;
				}
			}

			/* Count dacnt */
			dacnt +=datasize;

			/* Clear dacnt... XXX NOT here, at 3.1.2 AND 3.3.5.1 */
		}
		/* 3.3.6a Read Video PES data. !!!OK, PES_video_length may be 0!!! */
		else if( TsHeader.PID == TsVideoPID ) { //  && PES_video_length>0 ) {
			/* TODO: padding or other type data */
			//printf("ftell(fil)=%ld, fpos=%ld, ftell-fpos=%ld\n", ftell(fil), fpos, ftell(fil)-fpos);
			datasize=TS_PSIZE-(ftell(fil)-(fpos-4));  /* 4-sizeof(TsHeader), NOW ftell() at end of PES Header */

//			egi_dpstd("PES_video_length=%d, datasize=%d, dvcnt=%d\n", PES_video_length,datasize, dvcnt);

			#if 0 /* OK, PES_video_length MAY be 0!  Get rid of tail data ??? This indicates data error~ */
			if( dvcnt+datasize > PES_video_length ) {
				egi_dpstd(DBG_RED"Video adjust datasize...\n"DBG_RESET);
				datasize=PES_video_length-dvcnt;
			}
			#endif

			/* Read out */
		        if( fread(data, datasize, 1, fil)!=1 ) {
				egi_dperr("Fail to read Video PES data with size=%d, at file pos=%ld!\n", datasize, ftell(fil));
				//if(!feof(fil)) {
				if(datasize>0) {   /* !!! datasize may be 0! */
					err=-4; goto END_FUNC;
				}
			}
			/* If need to save video data */
			if(fvout) {
			    /* Write to fout */
			    if( fwrite(data, datasize, 1, fvout)!=1 ) {
				if(datasize>0) {  /* !!! datasize may be 0! */
					egi_dperr("Fail to write Video data to fvout!");
					err=-4; goto END_FUNC;
				}
			    }
			}

			/* Count dvcnt */
			dvcnt +=datasize;

			/* Clear dvcnt... XXX NOT here, at 3.1.2 AND 3.3.5a.1 */
		}

//	        egi_dpstd(" -----------\n");

		/* Reel back to end of current TsHeader */
		if( fseek(fil, fpos, SEEK_SET)!=0 ){
			egi_dperr("fseek fpos");
			err=-4; goto END_FUNC;
		}
	} /* end 3.3 */

	/* 4. Skip to next TS, from end of current TsHeader */
	if( fseek(fil, TS_PSIZE-4, SEEK_CUR)!=0 )
		break;

   } /* End while */

	/* Finish last PES packet */
	if(feof(fil)) {
	    /* Audio */
	    if(dacnt) {
	    	if(dacnt!=PES_audio_length)
	        	egi_dpstd(DBG_RED"Error! Last audo PES packet dacnt=%dBs, while PES_audio_length=%dBs\n"DBG_RESET,
				dacnt, PES_audio_length);
	    	else {
//	        	egi_dpstd(DBG_GREEN"Last audio PES packet dacnt=%dBs, while PES_audio_length=%dBs\n"DBG_RESET,
//				dacnt, PES_audio_length);
		}

	    	pacnt++; /* PES count */
	    }
	    /* Video */
	    if(dvcnt) {
	        /* For video stream, --- PES_video_length MAY be 0! ----- */
	        if(PES_video_length>0) {
	    		if(dvcnt!=PES_video_length)
	        		egi_dpstd(DBG_RED"Error! Last video PES packet dvcnt=%dBs, while PES_video_length=%dBs\n"DBG_RESET,
					dvcnt, PES_video_length);
	    		else
	        		egi_dpstd(DBG_MAGENTA"Last video PES packet dvcnt=%dBs, while PES_video_length=%dBs\n"DBG_RESET,
					dvcnt, PES_video_length);
		}
	    	pvcnt++; /* PES count */
	    }
	}


END_FUNC:
	egi_dpstd(DBG_MAGENTA"Totally %d TsHeader found, %d Audio and %d Video PES packets received.\n"DBG_RESET, hcnt, pacnt,pvcnt);

        /* Close file */
	if(fil) fclose(fil);
	if(faout) fclose(faout);
	if(fvout) fclose(fvout);

        if(err) {
	}

	return err;
}
