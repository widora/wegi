/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#ifndef ___FBMP_OP_H__
#define ___FBMP_OP_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h> //stat()
#include <malloc.h>

#define STRBUFF 256   /* length of file path&name in g_BMP_file_name[][STRBUFF] */
#define MAX_FBMP_NUM 256 /* Max. number of bmp files in g_BMP_file_name[MAX_FBMP_NUM][] */
#define MIN_CHECK_SIZE 1000 /* for checking integrity of BMP file */

/* for BMP files */
char file_path[256];
char g_BMP_file_name[MAX_FBMP_NUM][STRBUFF]; /* BMP file directory and name */
int  g_BMP_file_num=0; /* BMP file name index */
int  g_BMP_file_total=0; /* total number of BMP files */


/*------------------------------------------
get file size
return:
	>0 ok
	<0 fail
-------------------------------------------*/
unsigned long get_file_size(const char *fpath)
{
	unsigned long filesize=-1;
	struct stat statbuff;
	if(stat(fpath,&statbuff)<0)
	{
		return filesize;
	}
	else
	{
		filesize = statbuff.st_size;
	}
	return filesize;
}


/* --------------------------------------------
 find out all BMP files in a specified directory
 return value:
	 0 --- OK
	<0 --- fails
----------------------------------------------*/
static int Find_BMP_files(char* path)
{
	DIR *d;
	struct dirent *file;
	int fn_len;

	g_BMP_file_total=0; //--reset total  file number
	g_BMP_file_num=0; //--reset file  index

	//----clear g_BMP_file_name[] -------
	bzero(&g_BMP_file_name[0][0],MAX_FBMP_NUM*STRBUFF);

	//-------- if open dir error ------
	if(!(d=opendir(path)))
	{
		printf("error open dir: %s !\n",path);
		return -1;
	}

	while((file=readdir(d))!=NULL) 
	{
	   	//------- find out all bmp files  --------
  		fn_len=strlen(file->d_name);
		if(strncmp(file->d_name+fn_len-4,".bmp",4)!=0 )
			 continue;
   		strncpy(g_BMP_file_name[g_BMP_file_num++],file->d_name,fn_len);
   		g_BMP_file_total++;
 	}

	 closedir(d);
	 return 0;
}


#endif

