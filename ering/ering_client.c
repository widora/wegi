/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Create an ERING mechanism for EGI IPC.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include "egi_ring.h"
#include "egi_log.h"

/*----------------------------------------------------------
This is a ering_cmdret_handler_t type callback function
Parse and process ering_ret feeded back from the ering host.

For caller: ering_cmd,ering_ret are passed out params from
	    the EGI_RING module. Here ering_cmd is just copied
	    from caller's input ering_cmd.

-----------------------------------------------------------*/
void result_parser(EGI_RING_CMD *ering_cmd, EGI_RING_RET *ering_ret)
{
	printf("Call to host: ");
        if(ering_cmd->cmd_valid[ERING_CMD_ID])
                printf("id=%d,  ",ering_cmd->cmd_id);
        if(ering_cmd->cmd_valid[ERING_CMD_DATA])
                printf("data=%d,  ",ering_cmd->cmd_data);
        if(ering_cmd->cmd_valid[ERING_CMD_MSG])
                printf("msg='%s' \n",ering_cmd->cmd_msg);

	printf("Return from host: ");
        if( ering_ret->ret_valid[ERING_RET_CODE])
		printf("code=%d, ",ering_ret->ret_code);
        if( ering_ret->ret_valid[ERING_RET_DATA])
		printf("data=%d, ",ering_ret->ret_data);
        if( ering_ret->ret_valid[ERING_RET_MSG])
		printf("msg='%s' \n",ering_ret->ret_msg);
}


void main(void)
{
	#define ERING_HOST_APP		"ering.APP"
	#define ERING_HOST_STOCK	"ering.STOCK"
	#define ERING_HOST_FFPLAY	"ering.FFPLAY"

	#define ERING_STOCK_DISPLAY	1
	#define ERING_STOCK_HIDE	2
	#define ERING_STOCK_QUIT	3
	#define ERING_STOCK_SAVE	4

	int ret;
	int i=0;

	/* init ering_cmd */
	EGI_RING_CMD ering_cmd=
	{
		.caller="UI_stock",
		.cmd_valid={1,1,1},	/* switch valid data */
		.cmd_id=5,
		.cmd_data=56789,
		.cmd_msg=strdup("Hello EGI RING!"),
	};


	/* init egi logger */
	tm_start_egitick();
        if(egi_init_log("/mmc/log_ering") != 0)
        {
                printf("Fail to init log_ering,quit.\n");
                return;
        }

	/* loop test */
	while(1) {
		i++;
		ering_cmd.cmd_data=i;
		if(ret=ering_call_host(ERING_HOST_APP, &ering_cmd, result_parser))
			printf("Fail to call '%s', error: %s. \n", ERING_HOST_APP, ering_strerror(ret));
		else
			printf("i=%d, Ering call session completed!'\n", i);

		if( (i&(64-1))==0 ) {
			EGI_PLOG(LOGLV_INFO, "%s: Finish %d rounds of ering calls.\n",__func__, i);
		}

		tm_delayms(50);
  	}


}

