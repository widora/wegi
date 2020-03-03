/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Based on:
          FFmpeg examples in code sources.
				       --- FFmpeg ORG
          dranger.com/ffmpeg/tutorialxx.c
 				       ---  by Martin Bohme
	  muroa.org/?q=node/11
				       ---  by Martin Runge
	  www.xuebuyuan.com/1624253.html
				       ---  by niwenxian


Midas Zhou
midaszhou@yahoo.com
--------------------------------------------------------------------*/
#ifndef __EGI_FFPLAY_H__
#define __EGI_FFPLAY_H__

#include <signal.h>
#include <math.h>
#include <string.h>

#include "egi_common.h"
#include "sound/egi_pcm.h"

/* ffplay control command signal
 * !!! Note: pthread lock not applied here, when you need to set
 *           ffplay_mode, set 'cmd_mode' as last, as egi_thread_ffplay()
 *           will check 'cmd_mode' first!
 */
enum ffplay_cmd {
        cmd_none=0,
        cmd_play,
        cmd_pause,
        cmd_quit, 	/* release all resource and quit ffplay */
        cmd_next,
        cmd_prev,
	cmd_mode,

        cmd_exit_display_thread,        /* stop display thread */
        cmd_exit_subtitle_thread,       /* stop subtitle thread */
	cmd_exit_audioSpectrum_thread   /* stop audio spectrum displaying thread */
};
/* set ffplay_cmd as 'cmd_mode' before set ffplay_mode
 * !!! Note: pthread lock not applied here, set 'cmd_mode' as last,
             as egi_thread_ffplay() will check 'cmd_mode' first!
 */
enum ffplay_mode
{
        mode_loop_all=0,   /* loop all files in the list */
        mode_repeat_one,   /* repeat current file */
        mode_shuffle,      /* pick next file randomly */
	mode_noaudio,	   /* disable audio */
	mode_audio
};
/* feedback stauts after ffplay_cmd and ffplay_mode execution */
enum ffplay_status
{
        status_stop=0,
        status_playing,
        status_pausing,
};

/** Variables/Parameters in FFPLAY Context
 *  Note:
 *  	1. Use these params to control/adjust ffplay and to
 *     	   get feedback status.
 *  	2. FFplay_Ctx is a commonly shared struct data.
 */
typedef struct FFplay_Context
{
	int	  ftotal;    	/* Total number of media files path in array 'fpath' */
	char     **fpath;   	/* 1. Array of media file paths, or address. #####
				 * 2. Corresponding subtitle files with extension name of '.srt'
			         *    will automatically be searched at the same path, and applied
				 *    if it exists.
				 */

	long start_tmsecs;  	/* start_playing time, default 0 */

	enum ffplay_cmd     ffcmd;      /* 0 default none*/
	enum ffplay_mode    ffmode;	/* 0 default mode_loop_all */
	enum ffplay_status  ffstatus;	/* 0 default mode_stop */
}FFPLAY_CONTEXT;

extern FFPLAY_CONTEXT *FFplay_Ctx;	/* a global ctx */

/* Functions */
int 	egi_init_ffplayCtx(char *path, char *fext);
void 	egi_free_ffplayCtx(void);

/**
 *			A Thread Function
 *	Prepare/set  EGI_UI environment before start this thread/
 */
void * egi_thread_ffplay(EGI_PAGE *page);



#endif
