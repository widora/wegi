/*---------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
---------------------------------------------------------------------*/
#ifndef __EGI_UNET_H__
#define __EGI_UNET_H__

#include <stdio.h>
#include <stdbool.h>
#include <sys/un.h>
#include <sys/socket.h>


/* EGI_USERV_SESSION */
typedef struct egi_unet_server_session EGI_USERV_SESSION;
struct egi_unet_server_session {
        int             sessionID;               /* ID, ref. index of EGI_USERV_SESSION.sessions[], NOT as sequence number! */
        int             csFD;                    /* C/S session fd */
        struct          sockaddr_un addrCLIT;    /* Client address */
        bool            alive;                   /* flag on, if active/living, ?? race condition possible! check csFD. */
        int             cmd;                     /* Commad to session handler, NOW: 1 to end routine. */
};  /* At server side */


/* EGI_USERV */
typedef struct egi_unix_server EGI_USERV;
struct egi_unix_server {
        int                     sockfd;
        struct sockaddr_un      addrSERV;       /* Self address */

#define MAX_UNET_BACKLOG        16
        int                     backlog;        /* BACKLOG for accept(), init. in inet_create_Userver() as MAX_UNET_BACKLOG */
        int                     cmd;            /* NOW: 1 to end acpthread! */

        /* Listen/accept sessions */
	pthread_t		acpthread;	/* Listen/accept thread */
        bool                    acpthread_on;   /* acpthread IS running! */
        int                     ccnt;           /* Clients/Sessions counter */

#define MAX_UNET_CLIENTS     	8
        EGI_USERV_SESSION       sessions[MAX_UNET_CLIENTS];  	/* Sessions */
};

/* EGI_UCLIT */
typedef struct egi_unix_client EGI_UCLIT;
struct egi_unix_client {
        int                     sockfd;
        struct sockaddr_un      addrSERV;
};

EGI_USERV* unet_create_Userver(const char *svrpath);
int unet_destroy_Userver(EGI_USERV **userv );

EGI_UCLIT* unet_create_Uclient(const char *svrpath);
int unet_destroy_Uclient(EGI_UCLIT **uclit );


#endif
