/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Journal:
2021-02-18:
	1. Add egi_dperr() and egi_dpstd().
2022-01-02:
	1. Define DGB_color...

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
-------------------------------------------------------------------*/

#ifndef __EGI_DEBUG_H__
#define __EGI_DEBUG_H__

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
 extern "C" {
#endif

/* debug flags */
#define DBG_NONE	(0<<0)
#define	DBG_EGI 	(1<<0)
#define	DBG_TXT		(1<<1)
#define DBG_BTN		(1<<2)
#define DBG_LIST        (1<<3)
#define DBG_PIC		(1<<4)
#define DBG_SLIDER	(1<<5)
#define DBG_PAGE	(1<<6)
#define DBG_COLOR	(1<<7)
#define DBG_SYMBOL	(1<<8)
#define DBG_OBJTXT	(1<<9)
#define DBG_FBGEOM	(1<<10)
#define DBG_TOUCH	(1<<11)
#define DBG_BJP		(1<<12)
#define DBG_FFPLAY	(1<<13)
#define DBG_IOT		(1<<14)
#define DBG_FILO	(1<<15)
#define DBG_FIFO	(1<<16)
#define DBG_ERING	(1<<17)
#define DBG_PCM		(1<<18)
#define DBG_IMAGE	(1<<19)
#define DBG_CHARMAP	(1<<20)
#define DBG_UNIHAN	(1<<21)
#define DBG_TEST	(1<<21)

#define ENABLE_EGI_DEBUG

/* default debug flags */
#define DEFAULT_DBG_FLAGS   (DBG_NONE|DBG_UNIHAN | DBG_CHARMAP) //DBG_FFPLAY|DBG_PAGE ) //|DBG_TOUCH ) // |DBG_BTN)  //DBG_TOUCH )

#ifdef ENABLE_EGI_DEBUG
	/* define egi_pdebug() */
	//#define egi_pdebug(flags, fmt, args...)
	#define EGI_PDEBUG(flags, fmt, args...)			\
		do {						\
			if( flags & DEFAULT_DBG_FLAGS)		\
			{					\
				fprintf(stderr,"%s(): ",__func__); 	\
				fprintf(stderr,fmt, ## args);	\
			}					\
		} while(0)
#else
	#define EGI_PDEBUG(flags, fmt,args...)   /* blank space */

#endif


/* --- egi_printf() to replace printf() and make blank ---- */
#if 1
 #define egi_printf(flags, fmt, args...)			\
	do {						\
		fprintf(stderr,fmt, ## args);	\
	} while(0)
#else
 #define egi_printf(flags, fmt, args...)
#endif

#if 0 /*  8 Color Console, Color Attribute Setting */
  #define DBG_RED 	"\033[0;31;40m"
  #define DBG_GREEN 	"\033[0;32;40m"
  #define DBG_YELLOW 	"\033[0;33;40m"
  #define DBG_BLUE 	"\033[0;34;40m"
  #define DBG_MAGENTA 	"\033[0;35;40m"
  #define DBG_CYAN	"\033[0;36;40m"
 #define DBG_GRAY	"\033[0;37;40m"
#else /* ----- 256 color console : env TERM=xterm-256color ---- */
  #define DBG_RED        "\e[38;5;196;48;5;0m"      /* forecolor 196, backcolor 0 */
  #define DBG_GREEN      "\e[38;5;34;48;5;0m"
  #define DBG_YELLOW     "\e[38;5;220;48;5;0m"   /* forecolor 220, backcolor 0 */
  #define DBG_BLUE       "\e[38;5;27;48;5;0m"
  #define DBG_MAGENTA    "\e[38;5;201m"
  #define DBG_CYAN       "\e[38;5;37;48;5;0m"
  #define DBG_GRAY       "\e[38;5;249;48;5;0m"     /* forecolor 249, backcolor 0 */
#endif
#define DBG_RESET	"\e[0m"		/* Reset color to default */

/* Print to stderr and stdout */
/* printf() format:
   off_t 	%jd
   size_t	%zu
   ssize_t      %zd
 */
#define egi_dperr(fmt, args...)			\
	do {						\
		fprintf(stderr,"%s(): "fmt" Err'%s'.\n",__func__, ## args, strerror(errno)); 	\
	} while(0)

#define egi_dpstd(fmt, args...)			\
	do {						\
		fprintf(stdout,"%s(): "fmt, __func__, ## args); 	\
	} while(0)


#ifdef __cplusplus
 }
#endif

#endif
