/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-------------------------------------------------------------------*/
#ifndef __EGI_PROCMAN_H__
#define __EGI_PROCMAN_H__

#include <signal.h>

extern const char *app_name;

int egi_common_sigAction(int signum, void(*handler)(int) );

void __attribute__((constructor)) app_common_constructor(void);
void __attribute__((destructor)) app_common_destructor(void);
int egi_assign_AppSigActions(void);
int egi_process_activate_APP(pid_t *apid, char* app_path);

#endif
