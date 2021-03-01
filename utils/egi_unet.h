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
        int             csFD;                    /* C/S session fd, <=0 invalid.
						  * Reset to 0 when session ends. */
        struct          sockaddr_un addrCLIT;    /* Client address */
        bool            alive;                   /* flag on, if active/living, ?? race condition possible! check csFD. */
        int             cmd;                     /* Commad to session handler, NOW: 1 to end routine. */
};  /* At server side */


/* EGI_USERV */
typedef struct egi_unix_server EGI_USERV;
struct egi_unix_server {
        int                     sockfd;		/* accept fd, NONBLOCK */
        struct sockaddr_un      addrSERV;       /* Self address */

#define USERV_MAX_BACKLOG        16
        int                     backlog;        /* BACKLOG for accept(), init. in inet_create_Userver() as MAX_UNET_BACKLOG */
        int                     cmd;            /* NOW: 1 to end acpthread! */

        /* Listen/accept sessions */
	pthread_t		acpthread;	/* Listen/accept thread */
        bool                    acpthread_on;   /* acpthread IS running! */

	/* TODO: mutex_lock for ccnt/sessions[] */
        int                     ccnt;           /* Clients/Sessions counter */
#define USERV_MAX_CLIENTS     	8		/* TODO: a list to manage all client */
        EGI_USERV_SESSION       sessions[USERV_MAX_CLIENTS];  	/* Sessions */
};

/* EGI_UCLIT */
typedef struct egi_unix_client EGI_UCLIT;
struct egi_unix_client {
        int                     sockfd;
        struct sockaddr_un      addrSERV;
};

EGI_USERV* unet_create_Userver(const char *svrpath);
int unet_destroy_Userver(EGI_USERV **userv );
// static void* userv_listen_thread(void *userv);

EGI_UCLIT* unet_create_Uclient(const char *svrpath);
int unet_destroy_Uclient(EGI_UCLIT **uclit );

int unet_sendmsg(int sockfd,  struct msghdr *msg);
int unet_recvmsg(int sockfd,  struct msghdr *msg);


/* Signal handle functions */
void unet_signals_handler(int signum);
__attribute__((weak)) void unet_sigpipe_handler(int signum);
__attribute__((weak)) void unet_sigint_handler(int signum);
int unet_register_sigAction(int signum, void(*handler)(int));
int unet_default_sigAction(void);

#endif
