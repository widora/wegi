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

typedef struct egi_unet_server_session  EGI_USERV_SESSION;
typedef struct egi_unix_server 		EGI_USERV;
typedef struct egi_unix_client 		EGI_UCLIT;
typedef struct ering_msg_data 		ERING_MSG;

/* EGI_USERV_SESSION */
struct egi_unet_server_session {
        int             sessionID;               /* ID, ref. index of EGI_USERV_SESSION.sessions[], NOT as sequence number! */
        int             csFD;                    /* C/S session fd, <=0 invalid.
						  * Reset to 0 when session ends. */
        struct          sockaddr_un addrCLIT;    /* Client address */
        bool            alive;                   /* flag on, if active/living, ?? race condition possible! check csFD. */
        int             cmd;                     /* Commad to session handler, NOW: 1 to end routine. */
};  /* At server side */

/* EGI_USERV */
struct egi_unix_server {
        int                     sockfd;		/* accept fd, NONBLOCK */
        struct sockaddr_un      addrSERV;       /* Self address */

#define USERV_MAX_BACKLOG       16
        int                     backlog;        /* BACKLOG for accept(), init. in inet_create_Userver() as MAX_UNET_BACKLOG */
        int                     cmd;            /* NOW: 1 to end acpthread! */

        /* Listen/accept sessions */
	pthread_t		acpthread;	/* Listen/accept thread */
        bool                    acpthread_on;   /* acpthread IS running! */

	/* TODO: mutex_lock for ccnt/sessions[] */
        int                     ccnt;           /* Clients/Sessions counter */
#define USERV_MAX_CLIENTS     	(8+1)		/* TODO: a list to manage all client */
						/* NOTE: To be bigger than SURFMAN_MAX_SURFACES! */
        EGI_USERV_SESSION       sessions[USERV_MAX_CLIENTS];  	/* Sessions */
};

/* EGI_UCLIT */
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


/* ----------	ERING MSG and Functions ---------- */

#ifdef LETS_NOTE
   #define ERING_MSG_DATALEN	256	/* In bytes, Fixed length of data in ERING_MSG. for stream data transfer. */
#else
   #define ERING_MSG_DATALEN	128	/* In bytes, Fixed length of data in ERING_MSG. for stream data transfer. */
#endif

enum ering_request_type {
        ERING_REQUEST_NONE      =0,
        ERING_REQUEST_EINPUT    =1,
        ERING_REQUEST_ESURFACE  =2,  /* Params: int x0, int y0, int w, int h, int pixsize */
};

enum ering_msg_type {
        ERING_SURFMAN_ERR       =0,
        ERING_SURFACE_BRINGTOP  =1, /* Surfman bring/retire client surface to/from the top */
        ERING_SURFACE_RETIRETOP =2,
        ERING_SURFACE_CLOSE     =3, /* Surfman request the surface to close.. */
        ERING_MOUSE_STATUS      =4,
};

enum ering_result_type {
        ERING_RESULT_OK         =0,
        ERING_RESULT_ERR        =1,
        ERING_MAX_LIMIT         =2,     /* Surfaces/... Max limit */
};

/* ERING_MSG */
struct ering_msg_data {
        struct msghdr   *msghead;
             /*** struct msghdr {
                        void         *msg_name;      --- optional address
                       //used on an unconnected socket to specify the target address for a datagram
                       socklen_t     msg_namelen;    --- size of address
                       struct iovec *msg_iov;        --- scatter/gather array, writev(), readv()
						         msg_iov[].iov_base AND msg_iov[].iov_len MAY be adjusted.
						     TO be allocated in ering_msg_init().
                       size_t        msg_iovlen;     --- elements in msg_iov
                       void         *msg_control;    --- ancillary data, see below
                       size_t        msg_controllen; --- ancillary data buffer len
                       int           msg_flags;      --- flags (unused)
                  }
             ***/

	int			type;		/* Type of data: Map to msg_iov[0] */
        unsigned char           *data;  	/* Data: Map to msg_iov[1]
						 * Data length is fixed as ERING_MSG_DATALEN.
						 */
};

ERING_MSG* ering_msg_init(void);
void 	ering_msg_free(ERING_MSG **emsg);
int 	ering_msg_send(int sockfd, ERING_MSG *emsg,  int msg_type, const void *data,  size_t len);
int 	ering_msg_recv(int sockfd, ERING_MSG *emsg);

#endif
