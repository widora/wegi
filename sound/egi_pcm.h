/*-------------------------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

 Based on:
	   www.itwendao.com/article/detail/420944.html

some explanation:
        sample: usually 8bits or 16bits, one sample data width.
        channel: 1-Mono. 2-Stereo
        frame: sizeof(one sample)*channels
        rate: frames per second
        period: Max. frame numbers hardware can be handled each time. (different value for PLAYBACK and CAPTURE!!)
                running show: 1536 frames for CAPTURE; 278 frames for PLAYBACK
        chunk: frames receive from/send to hardware each time.
        buffer: N*periods
        interleaved mode:record period data frame by frame, such as  frame1(Left sample,Right sample),frame2(), ......
        uninterleaved mode: record period data channel by channel, such as period(Left sample,Left ,left...),period(right,right..$

Midas Zhou
--------------------------------------------------------------------------------------------------------------*/
#ifndef __EGI_PCM_H__
#define __EGI_PCM_H__

#define ALSA_PCM_NEW_HW_PARAMS_API

#include <stdint.h>
#include <alsa/asoundlib.h>
//#include <sound/asound.h>
#include <stdbool.h>

/* EGI_PCMBUF */
typedef struct {
	snd_pcm_t 		*pcm_handle;	/* PCM handle */

	unsigned long	 	size;		/* in bytes(TODO inframes), size of pcmbuf[]*/
	unsigned char*   	pcmbuf;		/* PCM data, with following params */

	unsigned int		depth;		/* depth of sample */
	unsigned int 	 	nchanl;		/* Number of channels */
	unsigned int 	 	srate;		/* Sample rate */
	snd_pcm_format_t	sformat;        /* Sample format,
						 * Now only for S8,U8,S16 !!!
						 * Example: SND_PCM_FORMAT_S16_LE
						 */
	snd_pcm_access_t 	access_type;	/* access type, Exmple: SND_PCM_ACCESS_RW_INTERLEAVED */
	bool			noninterleaved;	/* Defaul as interleaved type */

} EGI_PCMBUF;

/* -------- ALSA Sample Format  --------
typedef enum _snd_pcm_format {
        SND_PCM_FORMAT_UNKNOWN = -1,
        SND_PCM_FORMAT_S8 = 0,
        SND_PCM_FORMAT_U8 = 1,
        SND_PCM_FORMAT_S16_LE =2,
        SND_PCM_FORMAT_S16_BE,
        SND_PCM_FORMAT_U16_LE,
        SND_PCM_FORMAT_U16_BE,
        SND_PCM_FORMAT_S24_LE,
        SND_PCM_FORMAT_S24_BE,
        SND_PCM_FORMAT_U24_LE,
        SND_PCM_FORMAT_U24_BE,
        SND_PCM_FORMAT_S32_LE,
        SND_PCM_FORMAT_S32_BE,
        SND_PCM_FORMAT_U32_LE,
        SND_PCM_FORMAT_U32_BE,
        SND_PCM_FORMAT_FLOAT_LE,
        SND_PCM_FORMAT_FLOAT_BE,
	....	...

} snd_pcm_format_t;
--------------------------------------------*/



/* --- SYS PCM functions --- */
int	egi_prepare_pcm_device(unsigned int nchan, unsigned int srate, bool bl_interleaved);
int 	egi_pcm_period_size(void);
void 	egi_close_pcm_device(void);
void 	egi_play_pcm_buff(void** buffer, int nf);
int  	egi_getset_pcm_volume(int *pvol, int *percnt);

snd_pcm_t*  	egi_open_playback_device(  const char *dev_name, snd_pcm_format_t sformat,
                                        snd_pcm_access_t access_type, bool soft_resample,
                                        unsigned int nchanl, unsigned int srate, unsigned int latency
                                    );

/* --- EGI_PCMBUF functions --- */
EGI_PCMBUF* 	egi_pcmbuf_create(  unsigned long size, unsigned int nchanl, unsigned int srate,
                                	  snd_pcm_format_t sformat, snd_pcm_access_t access_type );
void 	        egi_pcmbuf_free(EGI_PCMBUF **pcmbuf);
EGI_PCMBUF*     egi_pcmbuf_readfile(char *path);
int  		egi_pcmbuf_playback(const char* dev_name, const EGI_PCMBUF *pcmbuf, int vstep, unsigned int nf,
				  				int nloop, bool *sigstop, bool *sigsynch);


#endif
