/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A test program for ffmusic

Usage:    test_ffmuz  audio_file
Example:  test_ffmuz  /music/*.mp3

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "page_ffmusic.h"
#include "ffmusic_utils.h"
#include "ffmusic.h"

int main(int argc, char **argv)
{
//	char* mypath;

	if(argc<2){
                fprintf(stderr, "Usage: %s file\n", argv[0]);
                return -1;
        }
//	mypath=strdup(argv[1]);

//	FFMUSIC_CONTEXT  myFFmuzCtx={ .ftotal=1,  .fpath=&mypath, .ffmode=mode_play_once };
	FFMUSIC_CONTEXT  myFFmuzCtx={ .ftotal=argc-1,  .fpath=argv+1, .ffmode=mode_play_once };
	FFmuz_Ctx=&myFFmuzCtx;

	thread_ffplay_music(NULL);

//	free(mypath);

	return 0;
}
