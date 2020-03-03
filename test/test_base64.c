/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test egi_xxx_base64() functions.
Print out encoded base64URL string.

Usage:	test_base64 file

Midas Zhou
-------------------------------------------------------------------*/
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include "egi_utils.h"

int main(int argc, char **argv)
{
	FILE *fp=NULL;
	int fd;
	struct stat sb;
	size_t fsize;
	unsigned char *fmap=NULL;
	char *buff=NULL;	/* For base64 data */
	char *URLbuff=NULL;  /* For base64URL data */
	int ret=0;
	int opt;
	bool enuft8url=false; /* If true: call egi_encode_uft8URL() and return */
	bool notail=false; /* If true:  No '='s at tail */
	bool enurl=false;
	int  base64type=0; /* BASE64 ETABLE index */

	if(argc<2) {
                printf("usage:  %s [-h] [-f] [-n] [-u] [-t type] fpath \n", argv[0]);
		exit(1);
	}

        /* parse input option */
        while( (opt=getopt(argc,argv,"hfnut:"))!=-1)
        {
                switch(opt)
                {
                       	case 'h':
                           printf("usage:  %s [-h] [-n] [-t type] fpath \n", argv[0]);
                           printf("         -h   	this help \n");
			   printf("	    -f		uft8URL encode \n");
                           printf("         -n   	No '='s at tail.\n");
                           printf("         -u   	encode to URL \n");
                           printf("         -t type   	BASE64 ETABLE index.\n");
                           printf("         fpath       file path \n");
                           return 0;
                       	case 'n':
                           notail=true;
                           break;
			case 'f':
			   enuft8url=true;
			   break;
			case 'u':
			   enurl=true;
			   break;
		       	case 't':
			   base64type=atoi(optarg);
			   break;
                       	default:
                           break;
                }
        }


	fd=open(argv[optind],O_RDONLY);
	if(fd<0) {
		perror("open");
		return -1;
	}
	/* get size */
	if( fstat(fd,&sb)<0) {
		perror("fstat");
		return -2;
	}
	fsize=sb.st_size;

	/* allocate buff */
	buff=calloc((fsize+2)/3+1, 4); /* 3byte to 4 byte, 1 more*/
	if(buff==NULL) {
		printf("Fail to calloc buff\n");
		fclose(fp);
		return -3;
	}

        /* mmap file */
        fmap=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        if(fmap==MAP_FAILED) {
                perror("mmap");
                return -2;
        }

	/* Encode to uft8URL */
	if(enuft8url)
	{
		if( realloc(buff,fsize*2)==NULL )
			goto CODE_END;

		ret=egi_encode_uft8URL(fmap, buff, fsize*2);
		printf("%s\n",buff);
		goto CODE_END;
	}

	/* Encode to base64 */
	ret=egi_encode_base64(base64type,fmap, fsize, buff);
//	printf("\nEncode base64: input size=%d, output size ret=%d, result buff contest:\n%s\n",fsize, ret,buff);
	if(!enurl) {
		printf("%s",buff);
		goto CODE_END;
	}

	/* allocate URLbuff */
	URLbuff=calloc(ret, 2); /*  !!! NOTICE HERE:  2*ret, 2 times base64 data size for URLbuff */
	if(URLbuff==NULL) {
		printf("Fail to calloc URLbuff\n");
		munmap(fmap, fsize);
		fclose(fp);
		free(buff);
		return -3;
	}

	/* Encode base64 to URL */
	ret=egi_encode_base64URL((const unsigned char *)buff, strlen(buff), URLbuff, ret*2, notail); /* TRUE notail '=' */
//	printf("\nEncode base64 to URL: input size strlen(buff)=%d, output size ret=%d, URLbuff contest:\n%s\n",
//										strlen(buff), ret, URLbuff);
	printf("%s", URLbuff);




CODE_END:
	munmap(fmap,fsize);
	close(fd);
	free(buff);
	free(URLbuff);

	return 0;
}
