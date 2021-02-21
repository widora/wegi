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

#define ERING_PATH "/tmp/ering_test"

/*** Note
 *  Openwrt has no wrapper function of memfd_create() in the C library.
int unet_destroy_Uclient(EGI_UCLIT **uclit ) *  and NOT include head file  <sys/memfd.h>
 *  syscall(SYS_memfd_create, ...) needs following option keywords.
 *  "However,  when  using syscall() to make a system call, the caller might need to handle architecture-dependent details; this
 *   requirement is most commonly encountered on certain 32-bit architectures." man-syscall
 */
#ifndef  MFD_CLOEXEC
 /* flags for memfd_create(2) (unsigned int) */
 #define MFD_CLOEXEC             0x0001U
 #define MFD_ALLOW_SEALING       0x0002U
#endif


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


#if 0 //////////////    TEST 1:  Basic loop test    //////////////////////////////
 /* ----- As Userver ----- */
 if( SetAsServer ) {

	EGI_USERV *userv;

    while(1) { /////////// LOOP TEST ///////////////

	userv=unet_create_Userver(ERING_PATH);
	if(userv==NULL)
		exit(EXIT_FAILURE);
	else
		printf("Succeed to create userv at '%s'!\n", ERING_PATH);

	/* Userver routine */
	//while(1) {
		usleep(200000);
	//}

	unet_destroy_Userver(&userv);
    }

	return 0;
 }

 /* ----- As Uclient ----- */
 else {
	EGI_UCLIT *uclit;

    while(1) { /////////// LOOP TEST ///////////////

	uclit=unet_create_Uclient(ERING_PATH);
	if(uclit==NULL) {
		//exit(EXIT_FAILURE);
	}
	else
		printf("Succeed to create uclit to '%s'!\n", ERING_PATH);

	/* Uclient routine */
	//while(1) {
		usleep(10000);
		//sleep(1);
	//}

	unet_destroy_Uclient(&uclit);
    }

	return 0;
 }

#endif

#if 0 //////////////    TEST 2:  Send/Recv Msg     //////////////////////////////
 struct msghdr msg={0};
 struct cmsghdr *cmsg;
 char data[1024];
 struct iovec iov={.iov_base=&data, .iov_len=sizeof(data) };

 /* ----- As Userver ----- */
 if( SetAsServer ) {
	int nrcv;
	EGI_USERV *userv;

    while(1) { /////////// LOOP TEST ///////////////

	userv=unet_create_Userver(ERING_PATH);
	if(userv==NULL)
		continue;
	else
		printf("Succeed to create userv at '%s'!\n", ERING_PATH);

	/* Wait for a client */
	while(userv->ccnt<1) { usleep(1000); }

	/* Prepare MSG buffer */
	memset(data, 0, sizeof(data));
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	unet_recvmsg(userv->sessions[0].csFD, &msg);

	/* Read MSG */
	if(nrcv>0)
		printf("Recv MSG: %s.\n", data);

	/* Free Userver */
	unet_destroy_Userver(&userv);

   }

	return 0;
 }

 /* ----- As Uclient ----- */
 else {
	EGI_UCLIT *uclit;

        time_t t;
        struct tm *tmp;

    while(1) { /////////// LOOP TEST ///////////////

	uclit=unet_create_Uclient(ERING_PATH);
	if(uclit==NULL) {
		continue;
	}
	else
		printf("Succeed to create uclit to '%s'!\n", ERING_PATH);

	/* Prepare MSG */
	memset(data,0,sizeof(data));
	strcat(data, "Hello, ERING MSG!");

        t = time(NULL);
        tmp = localtime(&t);
        if (tmp == NULL)  perror("localtime");

        if (strftime(data, sizeof(data), "%T", tmp) == 0) {
                printf( "strftime returned 0");
		strcat(data, "strftime returned 0");
	}

	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	/* Send msg */
	unet_sendmsg(uclit->sockfd, &msg);


	/* Free Uclient */
	unet_destroy_Uclient(&uclit);

	usleep(5000);
    }

	return 0;
 }

#endif


#if 1   //////////////    TEST 3:  Send/Recv Msg_control    /////////////////
 struct msghdr msg={0};
 struct cmsghdr *cmsg;
 char data[1024]={0};
 struct iovec iov={.iov_base=&data, .iov_len=sizeof(data) };

 /* ----- As Userver ----- */
 if( SetAsServer ) {
	EGI_USERV *userv;
	char  readbuf[16];
	int nread;

    while(1) { /////////// LOOP TEST ///////////////

	userv=unet_create_Userver(ERING_PATH);
	if(userv==NULL) {
		//exit(EXIT_FAILURE);
		continue;
	}
	else
		printf("Succeed to create userv at '%s'!\n", ERING_PATH);

	/* Wait for a client */
	while(userv->ccnt<1) { usleep(1000); }

	/* Prepare MSG buffer */
	int memfd;
	union {
	  /* Wrapped in a union in ordert o ensure it's suitably aligned */
		char buf[CMSG_SPACE(sizeof(memfd))]; /* CMSG_SPACE(): space with header and required alignment. */
		struct cmsghdr align;
	} u;

	/* Create memfd, the file descriptor is open with (O_RDWR) and O_LARGEFILE. */
	memfd=syscall(SYS_memfd_create,"shma", 0);//MFD_ALLOW_SEALING);
	if(memfd<0) {
		printf("Fail to create memfd!\n");
		//continue;
	}
	printf("Create memfd=%d\n", memfd);

	if(ftruncate(memfd, 32) <0 )
		perror("ftruncate");

	/* Write something to memfd */
	write(memfd, "Hello\0", 6);
//	sync();
//	fsync(memfd);

	/* MSG iov data */
	memset(data, 0, sizeof(data));
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
	// *(int *)CMSG_DATA(cmsg)=memfd; /* !!! CMSG_DATA() is constant !!! */
	memcpy((int *)CMSG_DATA(cmsg), &memfd, sizeof(memfd));

	/* Send msg to client */
	if( unet_sendmsg(userv->sessions[0].csFD, &msg) <=0 ) {
		printf("unet_sendmsg() fails!\n");
		unet_destroy_Userver(&userv);
		continue;
	}

	/* Wait for client */
	usleep(500000);

	/* Read memfd, if any change by client */
	lseek(memfd,0, SEEK_SET); /* !!! */
	nread=read(memfd, readbuf, 15);
	if(nread<0)
		perror("read memfd");
	else
		printf("read memfd %d bytes!\n", nread);
	readbuf[nread]='\0';
	printf("Readbuf: %s\n", readbuf);

	/* Close memfd */
	if(close(memfd)<0)
		printf("Fail to close memfd, Err'%s'.!\n", strerror(errno));

	/* Free Userver */
	unet_destroy_Userver(&userv);

   } /* End while() */

	exit(0);
 }

 /* ----- As Uclient ----- */
 else {
	EGI_UCLIT *uclit;
	char  writebuf[16];

    while(1) { /////////// LOOP TEST ///////////////

	uclit=unet_create_Uclient(ERING_PATH);
	if(uclit==NULL) {
		//exit(EXIT_FAILURE);
		continue;
	}
	else
		printf("Succeed to create uclit to '%s'!\n", ERING_PATH);

	/* Prepare MSG buffer */
	int memfd;
	union {
	  /* Wrapped in a union in ordert o ensure it's suitably aligned */
		char buf[CMSG_SPACE(sizeof(memfd))]; /* CMSG_SPACE(): space with header and required alignment. */
		struct cmsghdr align;
	} u;

	/* MSG iov data buffer */
	memset(data,0,sizeof(data));
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	/* MSG control buffer */
        msg.msg_control = u.buf;
        msg.msg_controllen = sizeof(u.buf);

	/* Wait to receive msg */
	if( unet_recvmsg(uclit->sockfd, &msg) <=0 ) {
		printf("unet_recvmsg() fails!\n");
		unet_destroy_Uclient(&uclit);
		continue;
	}

	/* Get memfd */
	cmsg = CMSG_FIRSTHDR(&msg);
	memfd = *(int *)CMSG_DATA(cmsg);
	//memcpy(&memfd, (int *)CMSG_DATA(cmsg), n*sizeof(int));

	printf("Get memfd=%d\n",memfd);

	/* Read memfd fails! */
	lseek(memfd, 0, SEEK_SET);
	read(memfd, writebuf, 6);
	printf("read memfd: %s\n", writebuf);

#if 1	/* Test memfd: write to memfd  ---OK */
	tm_get_strtime(writebuf);
	printf("writebuf: %s\n", writebuf);
	lseek(memfd, 0, SEEK_SET);
	if(write(memfd, writebuf, strlen(writebuf))<0)
		perror("write memfd");
	//fsync(memfd);
#else   /* Test memfd: Use mmap ---OK */
	void *ptr=mmap(NULL, 32, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, 0);
	printf("memfd content: %s\n", (char *)ptr);
        tm_get_strtime(ptr);
	munmap(ptr, 32);
#endif

	/* Close memfd */
	if(close(memfd)<0)
		printf("Fail to close memfd, Err'%s'.!\n", strerror(errno));

	/* Free Uclient */
	unet_destroy_Uclient(&uclit);

	usleep(5000);

   } /* End while() */

	exit(0);
 }

#endif


}
