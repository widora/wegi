/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Note:
	1. To make sample rate of effect sound same as of alsa config
	   value will improve sound mixing speed?
	   OR a fraction of Max HW sample rate?

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include "egi_pcm.h"

static EGI_PCMBUF *pcmbuf_launch;	/* Sound data for launching a missle */
static EGI_PCMBUF *pcmbuf_explode;	/* Sound data for exploding */

static void *  thread_sound_launch(void *arg);
static void *  thread_sound_explode(void *arg);


/*------------------------------------
    Load sound file to EGI_PCMBUFs

Return:
	0	OK
	<0	Fails
------------------------------------*/
int avg_load_sound(void)
{
	int ret=0;

	/* launching sound */
	#ifdef LETS_NOTE
	pcmbuf_launch=egi_pcmbuf_readfile("/home/midas-zhou/avenger/launch.wav");
	#else
	pcmbuf_launch=egi_pcmbuf_readfile("/mmc/avenger/launch.wav");
	#endif
	if(pcmbuf_launch==NULL) {
		printf("%s: Fail to load sound file for pcmbuf_launch.\n", __func__);
		ret--;
	}

	/* exploding sound */
	#ifdef LETS_NOTE
	pcmbuf_explode=egi_pcmbuf_readfile("/home/midas-zhou/avenger/explode.wav");
	#else
	pcmbuf_explode=egi_pcmbuf_readfile("/mmc/avenger/explode.wav");
	#endif
	if(pcmbuf_explode==NULL) {
		printf("%s: Fail to load sound file for pcmbuf_explode.\n", __func__);
		ret--;
	}

	return ret;
}


/* -----------------------------------------
      Sound effect for launching a missle
-------------------------------------------*/
static void *  thread_sound_launch(void *arg)
{
     pthread_detach(pthread_self());

     egi_pcmbuf_playback("default", (const EGI_PCMBUF *)pcmbuf_launch, 1024);
     usleep(10000);

     pthread_exit((void *)0);
}


/*------------------------------------------
      Sound effect for plane exploding
-------------------------------------------*/
static void *  thread_sound_explode(void *arg)
{
     pthread_detach(pthread_self());

     egi_pcmbuf_playback("default", (const EGI_PCMBUF *)pcmbuf_explode, 1024);
     usleep(10000);

     pthread_exit((void *)0);
}


/*-------------------------------------
 Create thread to play launching sound
--------------------------------------*/
void avg_sound_launch(void)
{
	pthread_t thread;

	if( pthread_create( &thread, NULL, thread_sound_launch, NULL ) !=0 )
		printf("%s:Fail to create pthread for sound launching!\n",__func__);

}


/*-------------------------------------
 Create thread to play launching sound
--------------------------------------*/
void avg_sound_explode(void)
{
	pthread_t thread;

	if( pthread_create( &thread, NULL, thread_sound_explode, NULL ) !=0 )
		printf("%s:Fail to create pthread for sound launching!\n",__func__);

}
