/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

test EGI_PCMBUF functions.

Midas Zhou
----------------------------------------------------------------------*/
#include <malloc.h>
#include <pthread.h>
#include "egi_common.h"
#include "egi_pcm.h"

static void *  thread_play_pcmbuf(void *arg);

int main(int argc, char** argv)
{


	mallopt(M_MMAP_THRESHOLD,512*1024);

#if 0 /* ------------------ LOOP TEST  --------------------- */
 	EGI_PCMBUF	*pcmbuf=NULL;
	if(argc<2) {
		printf("Usage: %s wav_file\n",argv[0]);
		exit(1);
	}
	while(1) {
		printf("pcmbuf readfile...\n");
		pcmbuf=egi_pcmbuf_readfile(argv[1]);
		if(pcmbuf==NULL)
			exit(1);

		printf("pcmbuf playback...\n");
		egi_pcmbuf_playback("default", pcmbuf, 2048);

		egi_pcmbuf_free(&pcmbuf);

		usleep(100000);
	}
#endif /* ------- END LOOP TEST ------- */


#if 1 /* ------------------ THREAD TEST  --------------------- */
	int i;
	EGI_PCMBUF	*pcmbuf[2]={NULL,NULL};
	pthread_t	thread_pcm[2];

	if(argc<3) {
		printf("Usage: %s wav_file1 wav_file2\n",argv[0]);
		exit(1);
	}

	/* Read wav file into  EGI_PCMBUF */
	for(i=0; i<2; i++) {
		pcmbuf[i]=egi_pcmbuf_readfile(argv[1+i]);
		if(pcmbuf==NULL)
			exit(1);
	}

	/* create thread */
	   //int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	   //                       void *(*start_routine) (void *), void *arg);
	for(i=0; i<2; i++) {
		if( pthread_create( &thread_pcm[i], NULL, thread_play_pcmbuf, (void *)pcmbuf[i] ) !=0 )
			exit(1);
	}

	/* joint threads */
	for(i=0; i<2; i++) {
		pthread_join(thread_pcm[i],NULL);
	}

#endif /* ------- END THREAD TEST ------- */


	return 0;
}




static void *  thread_play_pcmbuf(void *arg)
{
     EGI_PCMBUF *pcmbuf=(EGI_PCMBUF *)arg;

     while(1) {
	egi_pcmbuf_playback("default", (const EGI_PCMBUF *)pcmbuf, 512);
	usleep(10000);
     }

     return (void *)0;
}

