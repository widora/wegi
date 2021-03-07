/*----------------------------------------------------------------
Referece: https://blog.csdn.net/21cnbao/article/details/103470878


1. struct sockaddr_un
	#define UNIX_PATH_MAX   108
	struct sockaddr_un {
        	__kernel_sa_family_t sun_family; // AF_UNIX
	        char sun_path[UNIX_PATH_MAX];    //pathname
	};

2. struct msghdr
	struct msghdr {
               void         *msg_name;       // optional address
               socklen_t     msg_namelen;    // size of address
               struct iovec *msg_iov;        // scatter/gather array, writev() readv()
               size_t        msg_iovlen;     // # elements in msg_iov
               void         *msg_control;    // ancillary data, see below
               size_t        msg_controllen; // ancillary data buffer len
               int           msg_flags;      // flags (unused)
           };

3. struct cmsghdr and CMSG_xxx
	struct cmsghdr {
           socklen_t cmsg_len;    // data byte count, including header
           int       cmsg_level;  // originating protocol
           int       cmsg_type;   // protocol-specific type
           // followed by unsigned char cmsg_data[];
       };

       struct cmsghdr *CMSG_FIRSTHDR(struct msghdr *msgh);
       struct cmsghdr *CMSG_NXTHDR(struct msghdr *msgh, struct cmsghdr *cmsg);
       size_t CMSG_ALIGN(size_t length);
       size_t CMSG_SPACE(size_t length);
       size_t CMSG_LEN(size_t length);
       unsigned char *CMSG_DATA(struct cmsghdr *cmsg);

----------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/un.h> /* unix std socket */
#include <sys/socket.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "egi_unet.h"

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

void send_fd(int socket, int *fds, int n);
int *recv_fd(int socket, int n);

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

 /* ----- As Userver ----- */
 if( SetAsServer ) {
	EGI_USERV *userv;
	userv=unet_create_Userver("/tmp/ering_test");
	if(userv==NULL)
		exit(EXIT_FAILURE);
	else
		printf("Succeed to create userv!\n");


	return 0;
 }
 /* ----- As Uclient ----- */
 else {
	EGI_UCLIT *uclit;
	uclit=unet_create_Uclient("/tmp/ering_test");
	if(uclit==NULL)
		exit(EXIT_FAILURE);
	else
		printf("Succeed to create uclit!\n");

	return 0;
 }


#if 0
 /////////////////////////////	  As Client	///////////////////////////////
 if( !SetAsServer ) {
	int sfd, fds[2];
	struct sockaddr_un addr;

	sfd=socket(AF_UNIX, SOCK_STREAM, 0);
	if(sfd<0)
		perror("scocket");

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family=AF_UNIX;
	strncpy(addr.sun_path, "/tmp/.egi_fd_pass.socket", sizeof(addr.sun_path) -1);

#define SIZE 256
	//fds[0]=memfd_create("shma",0);
        /*  Allow  sealing  operations  on  this  file.   See the discussion of the F_ADD_SEALS and F_GET_SEALS operations in fcntl(2) */
	fds[0]=syscall(SYS_memfd_create,"shma",MFD_ALLOW_SEALING);
	if(fds[0]<0)
		perror("memfd_create");
	else
		printf("Opened fd %d in parent\n", fds[0]);

	ftruncate(fds[0], SIZE);
	void *ptr0=mmap(NULL,SIZE,PROT_READ|PROT_WRITE, MAP_SHARED, fds[0], 0);
	memset(ptr0,'A', SIZE);
	munmap(ptr0,SIZE);

	//fds[1]=memfd_create("shmb",0);
	fds[1]=syscall(SYS_memfd_create,"shmb",MFD_ALLOW_SEALING);
	if(fds[1]<0)
		perror("memfd_create fds[1]");
	else
		printf("Opened fd %d in parent\n", fds[1]);

	ftruncate(fds[1],SIZE);
	void *ptr1=mmap(NULL,SIZE,PROT_READ|PROT_WRITE,MAP_SHARED, fds[1],0);
	memset(ptr1,'B',SIZE);
	munmap(ptr1,SIZE);

	printf("Connecting the server ....\n");
	if(connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) <0 )
		perror("connect sfd");

	send_fd(sfd, fds, 2);

	return 0;
 }
 /////////////////////////////	  As Server	///////////////////////////////
 else  {
	int i;
	ssize_t nbytes;
	char buffer[128];
	int sfd, cfd, *fds;
	struct sockaddr_un addr;

	sfd=socket(AF_UNIX, SOCK_STREAM, 0);
	if(sfd==-1)
		perror("socket");

	/* If old socket name exists, remove it first */
	if(unlink("/tmp/.egi_fd_pass.socket")==-1 && errno != ENOENT)
		perror("unlink socket file");

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family=AF_UNIX;
	strncpy(addr.sun_path, "/tmp/.egi_fd_pass.socket", sizeof(addr.sun_path)-1);

	if( bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un))<0 )
		perror("bind");

	if( listen(sfd,5)<0 )
		perror("listen");

	printf("Waiting client...\n");
	cfd=accept(sfd, NULL, NULL);
	if(cfd<0)
		perror("accept");

	fds = recv_fd(cfd,2);
	printf("Receive fds: %d, %d\n", fds[0], fds[1]);
	for( i=0; i<2; i++) {
		printf("Reading from passed fd %d\n",fds[i]);
		while((nbytes=read(fds[i],buffer,sizeof(buffer)))>0)
			write(1,buffer,nbytes);
		printf("\n");
		*buffer=0;
	}

	if(close(cfd)<0)
		perror("Fail to close client socket");

	return 0;
 }

 #endif ////////////////

}


void send_fd(int socket, int *fds, int n)
{
	struct msghdr msg={0};
	struct cmsghdr *cmsg;
	char buf[CMSG_SPACE(n*sizeof(int))], data; /* CMSG_SPACE counts in header length */
	struct iovec io={.iov_base=&data, .iov_len=1 };
	memset(buf, 0, sizeof(buf));

	msg.msg_iov=&io;
	msg.msg_iovlen=1;
	msg.msg_control=buf;
	msg.msg_controllen=sizeof(buf);

	cmsg=CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(n*sizeof(int)); /* CMSG_LEN alignment */

	memcpy((int *)CMSG_DATA(cmsg), fds, n*sizeof(int));

	if(sendmsg(socket, &msg, 0) <0)
		perror("sendmsg");

}


int *recv_fd(int socket, int n)
{
	int *fds=malloc(n*sizeof(int));
	struct msghdr msg={0};
	struct cmsghdr *cmsg;

	char buf[CMSG_SPACE(n*sizeof(int))], data;
	memset(buf,'0',sizeof(buf));
	struct iovec io={.iov_base=&data, .iov_len=1 };

	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	if(recvmsg(socket, &msg, 0) <0)
		perror("recvmsg");

	cmsg = CMSG_FIRSTHDR(&msg);

	memcpy(fds, (int *)CMSG_DATA(cmsg), n*sizeof(int));

	return fds;
}
