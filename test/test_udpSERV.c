/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <egi_inet.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	/* Create UDP server */
	EGI_UDP_SERV *userv=inet_create_udpServer(NULL, 8765, 4);
	if(userv==NULL)
		exit(1);

	/* Set callback */
	userv->callback=inet_udpServer_TESTcallback;

	/* Process UDP server routine */
	inet_udpServer_routine(userv);

	/* Free and destroy */
	inet_destroy_udpServer(&userv);

	return 0;
}
