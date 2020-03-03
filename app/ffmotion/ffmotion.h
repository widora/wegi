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
#ifndef __FFMOTION_H__
#define __FFMOTION_H__

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
 *           ffmotion_mode, set 'cmd_mode' as last, as egi_thread_ffplay()
 *           will check 'cmd_mode' first!
 */
enum ffmotion_cmd {
        cmd_none=0,
        cmd_play,
        cmd_pause,
        cmd_quit, 	/* release all resource and quit ffplay */
        cmd_next,
        cmd_prev,
	cmd_mode,
	cmd_seek,

	cmd_rotate_clock,
	cmd_rotate_cclock,

        cmd_exit_display_thread,        /* stop display thread */
        cmd_exit_subtitle_thread,       /* stop subtitle thread */
//	cmd_exit_audioSpectrum_thread   /* stop audio spectrum displaying thread */
};
/* set ffmotion_cmd as 'cmd_mode' before set ffmotion_mode.
 * !!! Note: pthread lock not applied here, set 'cmd_mode' at last,
             as thread_ffmusic() will check 'cmd_mode' first!
 */
enum ffmotion_mode
{
        mode_loop_all=0,   /* loop all files in the list */
        mode_repeat_one,   /* repeat current file */
        mode_shuffle,      /* pick next file randomly */
	mode_noaudio,	   /* disable audio */
	mode_audio
};
/* feedback stauts after ffmotion_cmd and ffmotion_mode execution */
enum ffmotion_status
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
typedef struct FFmotion_Context
{
	int	 utotal;	/* Total number of URL addresses */
	char	 **url;		/* Array of URL address for net media files */
	int	 ftotal;    	/* Total number of media files path in array 'fpath' */
	char     **fpath;   	/* 1. Array of media file paths, or address. #####
				 * 2. Corresponding subtitle files with extension name of '.srt'
			         *    will automatically be searched at the same path, and applied
				 *    if it exists.
				 */
	int  pos_rotate;		/* Rotation position for FB displaying 0-3
					 * Just to synchronize gv_fb_dev with ff_fb_dev if necessary.
					 */
	long start_tmsecs;  		/* start_playing time, default 0 */
	int  sec_Vduration;		/* Audio duration in seconds */
	int  sec_Aduration;		/* Video duration in seconds */
	int  seek_pval;			/* Seek position, in percentage */

	enum ffmotion_cmd     ffcmd;    /* 0 default none*/
	enum ffmotion_mode    ffmode;	/* 0 default mode_loop_all */
	enum ffmotion_status  ffstatus;	/* 0 default mode_stop */
}FFMOTION_CONTEXT;


extern FBDEV ff_fb_dev;			/* For Pic displaying, defined in ffmusic.c */
extern FFMOTION_CONTEXT *FFmotion_Ctx;	/* A global FFMOTION Context, defined in  */


/**
 *			A Thread Function
 *	Prepare/set  EGI_UI environment before start this thread/
 */
void * thread_ffplay_motion(EGI_PAGE *page);



#endif
