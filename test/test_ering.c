/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
midaszhou@yahoo.com
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "egi_unet.h"
#include "egi_timer.h"
#include "egi_color.h"
#include "egi_debug.h"
#include "egi_math.h"

#define ERING_PATH "/tmp/ering_test"


int main(int argc, char **argv)
{
	bool SetAsServer=true;
	int opt;

        /* Parse input option */
        while( (opt=getopt(argc,argv,"hsc"))!=-1 ) {
                switch(opt) {
                        case 's':
				SetAsServer=true;
                                break;
                        case 'c':
                                SetAsServer=false;
                                break;
                        case 'h':
				printf("%s: [-hsc]\n", argv[0]);
				break;
		}
	}


#if 1   //////////////    TEST: ering request     /////////////////

 /* ---------- As EGI_SURFMAN ( The Server ) --------- */

 if( SetAsServer ) {
	EGI_USERV *userv;

 	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	int data[16]={0};
	struct iovec iov={.iov_base=&data, .iov_len=sizeof(data) };

    while(1) { /////////// LOOP TEST ///////////////

	/* Create an EGI surface manager */
	userv=unet_create_Userver(ERING_PATH);
	if(userv==NULL) {
		//exit(EXIT_FAILURE);
		continue;
	}
	else
		printf("Succeed to create userv at '%s'!\n", ERING_PATH);

	/* Wait for a client */
	while(userv->ccnt<1) { usleep(1000); }

	/* Prepare MSG iov data buffer for recvmsg */
	memset(data,0,sizeof(data));
	msg.msg_iov=&iov;  /* iov holds data */
	msg.msg_iovlen=1;

	/* Wait to receive ering request */
	if( unet_recvmsg(userv->sessions[0].csFD, &msg) <=0 ) {
		printf("unet_recvmsg() fails!\n");
		unet_destroy_Userver(&userv);
		continue;
	}

	/* Check request:   ERING_REQUEST_ESURFACE */
	if( data[0] != ERING_REQUEST_ESURFACE ) {
		printf("ering_request =%d! Can not handle it.\n", data[0]);
		continue;
	}

	/* ---------   Handle ERING_REQUEST_ESURFACE   ------- */
	int w=data[3];
	int h=data[4];
	int pixsize=data[5];

	printf("Request for an surface of %dx%dx%dbpp, origin at (%d,%d)\n", w, h, pixsize, data[1], data[2]);

	/* Prepare MSG buffer */
	int memfd;
	union {
	  /* Wrapped in a union in ordert o ensure it's suitably aligned */
		char buf[CMSG_SPACE(sizeof(memfd))]; /* CMSG_SPACE(): space with header and required alignment. */
		struct cmsghdr align;
	} u;


	/* Create memfd, the file descriptor is open with (O_RDWR) and O_LARGEFILE. */
	size_t memsize=sizeof(EGI_SURFACE)+w*h*(pixsize+1); /* 1bytes for alpha */
	memfd=egi_create_memfd("eface_test", memsize);
	if(memfd<0) {
		data[0]=ERING_RESULT_ERR;
		unet_sendmsg(userv->sessions[0].csFD, &msg);
		continue;
	}
	printf("Create memfd=%d\n", memfd);

	/* Create and init. EGI_SURFACE */
	EGI_SURFACE *eface=egi_mmap_surface(memfd);
	if(eface==NULL) {
		close(memfd);
		continue;
	}

	/* Assign members */
	eface->x0=data[1];
	eface->y0=data[2];
	eface->width=data[3];
	eface->height=data[4];
	eface->pixsize=data[5];
	eface->memsize=memsize;

        /* Unmap surface */
        if( munmap(eface, eface->memsize)!=0 )
                printf("Fail to unmap eface!\n");

	/* Prepare reply data */
	/* MSG iov data */
	memset(data, 0, sizeof(data));
	data[0]=ERING_RESULT_OK;
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	/* MSG control */
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level= SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;	/* For file descriptor transfer. */
	cmsg->cmsg_len	= CMSG_LEN(sizeof(memfd)); /* cmsg_len: with required alignment, take cmsg_data length as argment. */

	/* Put memfd to cmsg_data */
	// *(int *)CMSG_DATA(cmsg)=memfd; /* !!! CMSG_DATA() is constant ??!!! */
	memcpy((int *)CMSG_DATA(cmsg), &memfd, sizeof(memfd));

	/* Reply to the request */
	if( unet_sendmsg(userv->sessions[0].csFD, &msg) <=0 ) {
		printf("unet_sendmsg() fails!\n");
		unet_destroy_Userver(&userv);
		continue;
	}

	/* Close memfd */
	if(close(memfd)<0)
		printf("Fail to close memfd, Err'%s'.!\n", strerror(errno));


	/* Free Userver */
	unet_destroy_Userver(&userv);

	//exit(0);

   } /* End while() */

	exit(0);
 }

 /* ---------- As EGI_SURFACE Application ( The client ) --------- */
 else {

	EGI_UCLIT *uclit;

    while(1) { /////////// LOOP TEST ///////////////

	/* Link to ering */
	uclit=unet_create_Uclient(ERING_PATH);
	if(uclit==NULL) {
		//exit(EXIT_FAILURE);
		continue;
	}
	else
		printf("Succeed to create uclit to '%s'!\n", ERING_PATH);

	/* ering request surface */
	EGI_SURFACE *eface=NULL;
	eface=ering_request_surface( uclit->sockfd, mat_random_range(320/2), mat_random_range(240/2),
					   80+mat_random_range(320/2), 60+mat_random_range(240/2), sizeof(EGI_16BIT_COLOR));

	if(eface==NULL) {
		//exit(EXIT_FAILURE);
		unet_destroy_Uclient(&uclit);
		continue;
	}

	/* Test EGI_SURFACE */
	printf("An EGI_SURFACE is registered in EGI_SURFMAN!\n"); /* Egi surface manager */
	printf("Memsize: %uBytes  Geom: %dx%dx%dbpp  Origin at (%d,%d). \n",
			eface->memsize, eface->width, eface->height, eface->pixsize, eface->x0, eface->y0);

	/* Unmap surface */
	egi_munmap_surface(&eface);

	/* Free uclit */
	unet_destroy_Uclient(&uclit);

	usleep(20000);

   } /* End while() */

	exit(0);
 }

#endif


}
