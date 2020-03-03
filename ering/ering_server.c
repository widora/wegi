/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Create an ERING mechanism for EGI IPC.

Test:
	ubus call ering.APP ering_cmd "{'msg':'Hello ERING'}"

TODO:
	1. APP cotrol command classification.
	2. Method_calling feedback data format.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <libubus.h>
#include "egi_ring.h"


int count=1;

/*----------------------------------------------------------
This is a ering_cmdret_handler_t type callback function.
Parse and process ering_cmd from the caller and prepare
ering_ret which will then be feeded back to the ering caller.

ering_cmd:	ering command received from any ering caller.
ering_ret:	return ering msg for the ering caller.
-----------------------------------------------------------*/
void request_parser(EGI_RING_CMD *ering_cmd, EGI_RING_RET *ering_ret)
{
	/* 1. check received ering_cmd */
	/* already clear ering_cmd in EGI_RING module */
	printf("Request from caller: ");
	if(ering_cmd->cmd_valid[ERING_CMD_ID])
		printf("id=%d, ",ering_cmd->cmd_id);
	if(ering_cmd->cmd_valid[ERING_CMD_DATA])
		printf("data=%d, ",ering_cmd->cmd_data);
	if(ering_cmd->cmd_valid[ERING_CMD_MSG])
		printf("msg='%s' \n",ering_cmd->cmd_msg);


	/* 2. execute request method.... */



	/* 3. feed back result ering_ret */
	/* already clear ering_ret in EGI_RING module */
	ering_ret->ret_valid[ERING_RET_CODE]=true;
	ering_ret->ret_code=count++;
	ering_ret->ret_valid[ERING_RET_DATA]=true;
	ering_ret->ret_data=555555;
	ering_ret->ret_valid[ERING_RET_MSG]=true;
	ering_ret->ret_msg=strdup("Request deferred!");
}

void main(void)
{

        /* init egi logger */
        tm_start_egitick();
        if(egi_init_log("/mmc/log_ering") != 0)
        {
                printf("Fail to init log_ering,quit.\n");
                return;
        }

	/* ERING uloop */
	ering_run_host("ering.APP", request_parser);
}
