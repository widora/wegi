/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Utility functions, mem.


Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
-----------------------------------------------------------------*/
#ifndef __EGI_UTILS_H__
#define __EGI_UTILS_H__

#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define EGI_URL_MAX  1024 /* Max length for a URL address, 2048 is common for web browsers. */
#define EGI_PATH_MAX 256 /* Max length for a file path, 4096 for PATH_MAX in <limit.h>  */
#define EGI_NAME_MAX 128 /* Max length for a file name, 255 for NAME_MAX in <limit.h>, exclude '\0' */
#define EGI_SEARCH_FILE_MAX (1<<10) /* to be 2**n, Max number of files */
#define EGI_FEXTNAME_MAX 10 /* !!! exclude '.', length of extension name */
#define EGI_FEXTBUFF_MAX 16 /* Max items of separated extension names  */
#define EGI_LINE_MAX 4096   /* Max length for a line */


/* EGI File MMAP:  mmap with flags: PROT_READ|PROT_WRITE, MAP_PRIVATE */
typedef struct {
	int 		fd;	/* File descriptor */
	off_t 		fsize;  /* Size of file */
	int 		prot;   /* PROT_READ,PROT_WRITE,... OR | */
	int 		flags;  /* MAP_PRIVATE, MAP_SHARED,... OR | */
	char		*fp;	/* Protected */
	off_t		off;	/* Current offset, editable. init as 0. */
} EGI_FILEMMAP;
EGI_FILEMMAP *egi_fmap_create(const char *fpath, off_t resize, int prot, int flag);
int egi_fmap_msync(EGI_FILEMMAP* fmap);
int egi_fmap_free(EGI_FILEMMAP** fmap);
int egi_fmap_resize(EGI_FILEMMAP* fmap, off_t resize);

/* A file, functions as the system memo pad, for copy/paste operation */
#define EGI_SYSPAD_PATH "/tmp/.egi_syspad"

/* EGI SYSPAD buffer struct */
typedef struct {
	unsigned char*	data;
	size_t		size;
} EGI_SYSPAD_BUFF;
EGI_SYSPAD_BUFF *egi_sysPadBuff_create(size_t size);
void 		egi_sysPadBuff_free(EGI_SYSPAD_BUFF **padbuff);
EGI_SYSPAD_BUFF *egi_buffer_from_syspad(void);
int 		egi_copy_to_file( const char *fpath, const unsigned char *pstr, size_t size, char endtok);
int 		egi_copy_to_syspad(const unsigned char *pstr, size_t size);
int 		egi_copy_from_syspad(unsigned char *pstr);

/*** EGI ID3V2 Tag Packet
 * TIT2: Title/songname/content description
 * TALB: Album/Movie/Show title
 * TPE1: Lead performer(s)/Soloist(s)
 * COMM: Comment
 * APIC: Attached picture (png OR jpeg)
 xxxTRCK: Track number/Position in set
 */
#include "egi_imgbuf.h"
#define TEMP_MP3APIC_PATH "/tmp/tmp_mp3tag_pic.jpgpng" /* To save attached picture (APIC) */
typedef struct egi_ID3V2TAG{
	char *title;  		/* Tag TIT2 */
	char *album;  		/* Tag TALB */
	char *performer; 	/* Tag TPE1 */
	char *comment; 		/* Tag COMM */
	EGI_IMGBUF *imgbuf;  	/* Tag APIC */
} EGI_ID3V2TAG;
EGI_ID3V2TAG *egi_id3v2tag_calloc(void);
void egi_id3v2tag_free(EGI_ID3V2TAG **tag);
EGI_ID3V2TAG *egi_id3v2tag_readfile(const char *fpath);


/* Utils functions */
bool 	egi_host_littleEndian(void);
void 	egi_byteswap(int n, char *data);
void 	egi_free_char(char **p);
char**  egi_create_charList(int items, int size);
void 	egi_free_charList(char ***p, int n);
void** egi_create_array2D(int items, int subitems, int datasize);
void    egi_free_array2D(void ***p, int n);
int 	egi_mem_grow(void **ptr, size_t old_size, size_t more_size);
unsigned long egi_get_fileSize(const char *fpath);
int 	egi_search_str_in_file(const char *fpath, size_t off, const char *pstr);

int 	egi_shuffle_intArray(int *array, int size);
int 	egi_util_mkdir(char *dir, mode_t mode);

int 	egi_read_fileBlock(FILE *fsrc, char *fdes, int tnulls, size_t bs);
int 	egi_copy_fileBlock(FILE *fsrc, FILE *fdest, size_t bs);
int 	egi_copy_file(char const *fsrc_path, char const *fdest_path, bool append);
int 	egi_append_file(char const *fpath, void* data, size_t size); //ALSO for creating a file


unsigned char** egi_malloc_buff2D(int items, int item_size) __attribute__((__malloc__));
int 	egi_realloc_buff2D(unsigned char ***buff, int old_items, int new_items, int item_size);
void 	egi_free_buff2D(unsigned char **buff, int items);
char** 	egi_alloc_search_files(const char* path, const char* fext,  int *pcount );
/* Note: call egi_free_buff2D() to free it */

//unsigned char _base64_to_u6(char c); //private
int egi_decode_base64(int type, const char *buff, unsigned int size, char *data);
int egi_encode_base64(int type, const unsigned char *data, unsigned int size, char *buff);
int egi_encode_base64URL(const unsigned char *base64_data, unsigned int data_size, char *buff, unsigned int buff_size, bool notail);
int egi_encode_uft8URL(const unsigned char *ustr, char *buff, unsigned int buff_size);

int egi_lock_pidfile(const char *pid_lock_file);
int egi_valid_fd(int fd);

/* EGI_BITSTATUS: Bitwise status */
typedef struct egi_bit_status {
	unsigned int 	total;		/* Total number of effective bits in octbits[] */
		int	pos;		/* Current position, set -1 before posnext op. */
	unsigned char *octbits;		/* Arrays of bit_status holders, each with 8 bits. */
	//unsigned uint32_t *32bits;
} EGI_BITSTATUS;

EGI_BITSTATUS *egi_bitstatus_create(unsigned int total);
void 	egi_bitstatus_free(EGI_BITSTATUS **ebits);
int 	egi_bitstatus_getval(const EGI_BITSTATUS *ebits, unsigned int index);
void 	egi_bitstatus_print(const EGI_BITSTATUS *ebits);
int 	egi_bitstatus_set(EGI_BITSTATUS *ebits, unsigned int index);
int 	egi_bitstatus_reset(EGI_BITSTATUS *ebits, unsigned int index);
int 	egi_bitstatus_setall(EGI_BITSTATUS *ebits);
int 	egi_bitstatus_resetall(EGI_BITSTATUS *ebits);
int 	egi_bitstatus_count_ones(const EGI_BITSTATUS *ebits);
int 	egi_bitstatus_count_zeros(const EGI_BITSTATUS *ebits);
int 	egi_bitstatus_posfirst_zero(EGI_BITSTATUS *ebits,  int cp);  /* ebits->pos self increase */
int 	egi_bitstatus_posfirst_one(EGI_BITSTATUS *ebits,  int cp);   /* ebits->pos self increase */
int 	egi_bitstatus_checksum(void *data, size_t size);

/* MISC */
int egi_getset_backlightPwmDuty(int pwmnum, int *pget, int *pset, int adjust);
int egi_extract_AV_from_ts(const char *fts, const char *faac, const char *fvd);

#ifdef __cplusplus
 }
#endif

#endif
