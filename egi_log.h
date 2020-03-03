/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-------------------------------------------------------------------*/

#ifndef __EGI_LOG__
#define __EGI_LOG__

#include <stdio.h>
#include <stdlib.h>

#define EGI_LOGFILE_PATH "/mmc/egi_log"	/* default */

#define ENABLE_LOGBUFF_PRINT 	/* enable to print log buff content */

#define EGI_LOG_MAX_BUFFITEMS	128 	/* MAX. number of log buff items */
#define EGI_LOG_MAX_ITEMLEN	512 //256 	/* Max length for each log string item */
#define EGI_LOG_WRITE_SLEEPGAP	10  	/* in ms, sleep gap between two buff write session in egi_log_thread_write() */
#define EGI_LOG_QUITWAIT 	55 	/* in ms, wait for other thread to finish pushing inhand log string,
				     	 * before quit the log process */

/* LOG LEVEL */
/* NOTE: to accord with egi_loglv_to_string()! */
enum egi_log_level
{
	LOGLV_NONE	  =(1<<0),
	LOGLV_INFO        =(1<<1),
	LOGLV_ASSERT      =(1<<2),
	LOGLV_WARN        =(1<<3),
	LOGLV_CRITICAL	  =(1<<4),
	LOGLV_ERROR       =(1<<5),
	LOGLV_TEST        =(1<<15),
};

#define ENABLE_EGI_PLOG

/* Only log levels included in DEFAULT_LOG_LEVELS will be effective in EGI_PLOG() */
#define DEFAULT_LOG_LEVELS   (LOGLV_NONE|LOGLV_TEST|LOGLV_INFO|LOGLV_WARN|LOGLV_ERROR|LOGLV_CRITICAL|LOGLV_ASSERT)

/* Only log level gets threshold(>=) that will be written to log file directly, without putting to buffer */
#define LOGLV_NOBUFF_THRESHOLD		LOGLV_WARN


/* --- logger functions --- */
int egi_push_log(enum egi_log_level log_level, const char *fmt, ...) __attribute__(( format(printf,2,3) ));
//static void egi_log_thread_write(void);
int egi_init_log(const char *fpath); //void);
//static int egi_stop_log(void);
//static int egi_free_logbuff(void)
int egi_quit_log(void);


#ifdef ENABLE_EGI_PLOG
   /* define egi_plog(), push to log_buff
    * Let the caller to put FILE and FUNCTION, we can not ensure that two egi_push_log()
    * will push string to the log buff exactly one after the other,because of concurrency
    * race condition.
    * egi_push_log(" From file %s, %s(): \n",__FILE__,__FUNCTION__);
    */
	#define EGI_PLOG(level, fmt, args...)                 \
        	do {                                            \
                	if(level & DEFAULT_LOG_LEVELS)          \
               	 	{                                       \
				egi_push_log(level,fmt, ## args);	\
                	}                                       \
        	} while(0)

#else
	#define EGI_PLOG(level, fmt, args...)	/* leave as blank */

#endif


#endif
