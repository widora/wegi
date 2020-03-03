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
#ifndef __FF_PCM_H__
#define __FF_PCM_H__

#define ALSA_PCM_NEW_HW_PARAMS_API

#include <stdint.h>
#include <alsa/asoundlib.h>
#include <stdbool.h>

int prepare_ffpcm_device(unsigned int nchan, unsigned int srate, bool bl_interleaved);
void close_ffpcm_device(void);
void play_ffpcm_buff(void** buffer, int nf);
int ffpcm_getset_volume(int *pvol, int *percnt);


#endif
