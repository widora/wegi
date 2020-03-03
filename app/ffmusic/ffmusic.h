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
#ifndef __FFMUSIC_H__
#define __FFMUSIC_H__

#include <signal.h>
#include <math.h>
#include <string.h>

#include "egi_common.h"
#include "sound/egi_pcm.h"

extern int ff_sec_Aelapsed;	/* in seconds, Audio playing time elapsed */
extern int ff_sec_Velapsed;	/* in seconds, Video playing time elapsed */
extern int ff_sec_Aduration;	/* in seconds, multimedia file Audio duration */
extern int ff_sec_Vduration;	/* in seconds, multimedia file Video duration */

/* ffplay control command signal
 * !!! Note: pthread lock not applied here, when you need to set
 *           ffmuz_mode, set 'cmd_mode' as last, as egi_thread_ffplay()
 *           will check 'cmd_mode' first!
 */
enum ffmuz_cmd {
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
/* set ffmuz_cmd as 'cmd_mode' before set ffmuz_mode.
 * !!! Note: pthread lock not applied here, set 'cmd_mode' at last,
             as thread_ffmusic() will check 'cmd_mode' first!
 */
enum ffmuz_mode
{
        mode_loop_all		=0,   	/* Default loop all files in the list */
	mode_play_once		=2,	/* play once */
        mode_repeat_one		=4,   	/* repeat current file */
        mode_shuffle		=8,     /* pick next file randomly */
	mode_audio_on		=16,	/* default audio on */
	mode_audio_off		=32,	/* disable audio */
	mode_video_on		=64,	/* default video on */
	mode_video_off		=128   /* disable video */
};
/* feedback stauts after ffmuz_cmd and ffmuz_mode execution */
enum ffmuz_status
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
typedef struct FFmusic_Context
{
	int	  ftotal;    	/* Total number of media files path in array 'fpath' */
	char     **fpath;   	/* 1. Array of media file paths, or address. #####
				 * 2. Corresponding subtitle files with extension name of '.srt'
			         *    will automatically be searched at the same path, and applied
				 *    if it exists.
				 */

	long start_tmsecs;  	/* start_playing time, default 0 */

	enum ffmuz_cmd     ffcmd;      /* 0 default none*/
	enum ffmuz_mode    ffmode;	/* 0 default mode_loop_all */
	enum ffmuz_status  ffstatus;	/* 0 default mode_stop */
}FFMUSIC_CONTEXT;


extern FBDEV ff_fb_dev;			/* For Pic displaying, defined in ffmusic.c */
extern FFMUSIC_CONTEXT *FFmuz_Ctx;	/* A global FFMUZ Context, defined in  */

/* Functions */
int 	init_ffmuzCtx(char *path, char *fext);
void 	free_ffmuzCtx(void);

/**
 *			A Thread Function
 *	Prepare/set  EGI_UI environment before start this thread/
 */
void * thread_ffplay_music(EGI_PAGE *page);



#endif
