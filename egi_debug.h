/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-------------------------------------------------------------------*/

#ifndef __EGI_DEBUG_H__
#define __EGI_DEBUG_H__

#include <stdio.h>

//#define EGI_DEBUG

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
#define DBG_TEST	(1<<20)

#define ENABLE_EGI_DEBUG

/* default debug flags */
#define DEFAULT_DBG_FLAGS   (DBG_NONE|DBG_FFPLAY|DBG_PAGE )//|DBG_TOUCH ) // |DBG_BTN)  //DBG_TOUCH )

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




#endif
