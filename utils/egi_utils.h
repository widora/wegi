/*--------------------------------------------------------------------------------------------
Utility functions, mem.


Midas Zhou
-----------------------------------------------------------------------------------------------*/
#ifndef __EGI_UTILS_H__
#define __EGI_UTILS_H__

#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>

#define EGI_URL_MAX  256 /* Max length for a URL address */
#define EGI_PATH_MAX 256 /* Max length for a file path, 4096 for PATH_MAX in <limit.h>  */
#define EGI_NAME_MAX 128 /* Max length for a file name, 255 for NAME_MAX in <limit.h> */
#define EGI_SEARCH_FILE_MAX (1<<10) /* to be 2**n, Max number of files */
#define EGI_FEXTNAME_MAX 10 /* !!! exclude '.', length of extension name */
#define EGI_FEXTBUFF_MAX 16 /* Max items of separated extension names  */

void 	egi_free_char(char **p);
int 	egi_util_mkdir(char *dir, mode_t mode);
int 	egi_copy_file(char const *fsrc_path, char const *fdest_path);
unsigned char** egi_malloc_buff2D(int items, int item_size) __attribute__((__malloc__));
int 	egi_realloc_buff2D(unsigned char ***buff, int old_items, int new_items, int item_size);
void 	egi_free_buff2D(unsigned char **buff, int items);
char** 	egi_alloc_search_files(const char* path, const char* fext,  int *pcount );
/* Note: call egi_free_buff2D() to free it */

int egi_encode_base64(int type, const unsigned char *data, unsigned int size, char *buff);
int egi_encode_base64URL(const unsigned char *base64_data, unsigned int data_size, char *buff, unsigned int buff_size, bool notail);
int egi_encode_uft8URL(const unsigned char *ustr, char *buff, unsigned int buff_size);


#endif
