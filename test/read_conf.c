/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Test EGI CSTRING functions

Midas Zhou
-----------------------------------------------------------------*/

#include <stdio.h>
#include "egi_timer.h"
#include "egi_cstring.h"

int main(void)
{
	char value[64]={0};

        /* --- start egi tick --- */
        tm_start_egitick();

 while(1)
 {
	/* IOT_CLIENT */
	if( egi_get_config_value("IOT_CLIENT", "server_ip", value)==0)
		printf("Found KEY server ip:%s\n",value);
	else
		printf("[IOT_CLIENT]: Key [server_ip] value is NOT found in config file!\n");

	/* MPLAYER */
	if( egi_get_config_value("EGI_MPLAYER", "playlist", value)==0)
		printf("Found KEY playlist:%s\n",value);
	else
		printf("[EGI_MPLAYER]: Key [playlist] value is NOT found in config file!\n");

	/* FFPLAY */
	if( egi_get_config_value("EGI_FFPLAY", "video_dir", value)==0)
		printf("Found KEY video_dir:%s\n",value);
	else
		printf("[EGI_FFPLAY]: Key [video_dir] value is NOT found in config file!\n");

	tm_delayms(500);
 }


	return 0;

}
