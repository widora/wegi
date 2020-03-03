/*----------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A program to test BIGIOT client app.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "egi_iotclient.h"
#include "egi_timer.h"
#include "egi_fbgeom.h"
#include "egi_symbol.h"
#include "egi_log.h"



/*------------------------------------
 	    MAIN FUNCTION
------------------------------------*/
int main(void)
{
	int i,k;

	/* --- init logger --- */
#if 1
  	if(egi_init_log("/mmc/log_iot") != 0)
	{
		printf("Fail to init logger,quit.\n");
		return -1;
	}
#endif

        /* --- start egi tick --- */
//        tm_start_egitick();		/* WARNING: log thread tm_delayms() disable!!! */

        /* --- prepare fb device --- */
//       gv_fb_dev.fdfd=-1;
//       init_dev(&gv_fb_dev);

	/* --- load all symbol pages --- */
//	symbol_load_allpages();


/* <<<<<<<<<<<<<<<<<<<<<  TEST BIGIOT Client  <<<<<<<<<<<<<<<<<<*/

	egi_iotclient(NULL);


/* <<<<<<<<<<<<<<<<<<<<<  END TEST  <<<<<<<<<<<<<<<<<<*/

  	/* quit logger */
  	egi_quit_log();

        /* close fb dev */
//        munmap(gv_fb_dev.map_fb,gv_fb_dev.screensize);
//        close(gv_fb_dev.fdfd);

	return 0;
}

