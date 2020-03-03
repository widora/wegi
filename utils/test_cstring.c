/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include "egi_utils.h"
#include "egi_timer.h"
#include "egi_cstring.h"



int main(void)
{

        /* --- start egi tick --- */
        tm_start_egitick();

#if 0 //////////////////  1. TEST: egi_alloc_search_file() //////////////

	int i;
	char (*path)[EGI_PATH_MAX+EGI_NAME_MAX];
	int total;

	while(1)
	{
		path=egi_alloc_search_files("/mmc/","mp3, wav, png", &total);
		printf("Find %d files.\n", total);
  		for(i=0;i<total;i++)
			printf("%s\n",path+i);

	  	free(path);
  		tm_delayms(500);
	}
#endif

#if 1 //////////////////  1. TEST: egi_alloc_search_file2() //////////////

	int i;
	char **path;
	int total;

	while(1)
	{
		path=egi_alloc_search_files("/mmc/",".mp3,    .avi,  .png", &total);
		printf("Find %d files.\n", total);
//  		for(i=0;i<total;i++)
//			printf("%s\n",path[i]);

		/* free path */
		egi_free_buff2D((unsigned char **) path, total);

  		tm_delayms(100);
	}
#endif



#if 0 //////////////////  2. TEST: cstr_dup_repextname()  ////////////////
int i;
char *newpath=NULL;

char fpath[][128]={
"/tmp/df/xx.1234",
"/adb/.dsf/sd.f/wave.mp3",
"/home/yahoo.df/../okok.mp4",
"/meida/.music./.OLD./..last.wav",
"/meida/.music./.OLD./.l..ast...wav",
};

char extname[]=".src";

while(1) {

for(i=0; i<5; i++) {
	newpath=cstr_dup_repextname(&fpath[i][0], extname);
	printf("Old path: %s \n", fpath[i]);
	printf("New path: %s \n", newpath);
	free(newpath);newpath=NULL;
}
tm_delayms(100);

}//end while()
#endif


	return 0;
}
