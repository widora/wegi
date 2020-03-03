/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#ifndef __EGI_RING_H__
#define __EGI_RING_H__

#include <stdio.h>
#include <stdint.h>

/* ERING APP METHOD ID */
/* to be defined in APP header file. */

/* Enum for ERING command policy order */
enum {
        ERING_CMD_ID=0,
        ERING_CMD_DATA,
        ERING_CMD_MSG,
        __ERING_CMD_MAX,
};

/* Enum for ERING returned data handling policy order */
enum {
        ERING_RET_CODE=0,
        ERING_RET_DATA,
        ERING_RET_MSG,
        __ERING_RET_MAX,
};

// #define ERING_MAX_STRMSG 	1024

typedef struct egi_ring_command EGI_RING_CMD; 	/* ering caller command */
typedef struct egi_ring_return  EGI_RING_RET;	/* ering host reply */

struct egi_ring_command
{
	char 	*caller;		/* name of the ering caller, for client_obj.name */

	bool	cmd_valid[__ERING_CMD_MAX]; /* to indicate whether corresponding cmd filed data is valid/availble */
	/***
	 * Complying with 'id','data' and 'msg' of ering_cmd_policy[]
	 */
	int16_t cmd_id;			/* command ID, to be coded by APPs */
	int32_t cmd_data;		/* command param */
	char 	*cmd_msg;		/* command string message, if any */

//	void (*cmd_handler)(EGI_RING_CMD *); /* handler for received command, provided by host APP */
};

struct egi_ring_return
{
	char 	*host;			/* name of the ering host (APP) */

	bool	ret_valid[__ERING_RET_MAX];  /* to indicate whether corresponding ret filed data is valid/availble */
	/***
	 * Complying with 'code','data' and 'msg' of ering_ret_policy[]
	 */
	int16_t  ret_code; 		/* returned code from the ering host, to be coded by APPs
			    		 * however, Do NOT code ret_code=0, which it for
					 * 'No Reply From the Host Yet!'
			    		 */
	int32_t  ret_data; 		/* returned data, or void* ret_data  ....TBD */
	char 	 *ret_msg;		/* returned string msg from the host, if any */

//	void	(*ret_handler)(EGI_RING_RET *); /* handler for returned msg, provided by caller */
};

/* ering command and return msg handler */
typedef void(*ering_cmdret_handler_t)(EGI_RING_CMD *, EGI_RING_RET *);

void 	ering_run_host(const char *ering_host, ering_cmdret_handler_t handler);
int 	ering_call_host(const char *ering_host, EGI_RING_CMD *ering_cmd,ering_cmdret_handler_t handler);
int 	ering_read_ret(EGI_RING_RET *eret);
void	ering_clear_ret(EGI_RING_RET *eret);
void	ering_clear_cmd(EGI_RING_CMD *ecmd);


#endif
