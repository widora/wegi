/*------------------------------------------------------------------
 Based on:
	   www.itwendao.com/article/detail/420944.html

  usage:  cat test.pcm | ./alsa_play    (test.pcm should be of 2 channels and 16bits per sample)

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


-------------------------------------------------------------------*/

#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>

//int alsa_playPCM()
int main(void)
{
//	long loops;
	int rc;
	int size;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir=0;
	snd_pcm_uframes_t frames;
	char *buffer;

	//----- open PCM device for playblack
	rc=snd_pcm_open(&handle,"default",SND_PCM_STREAM_PLAYBACK,0);
	if(rc<0)
	{
		fprintf(stderr,"unable to open pcm device: %s\n",snd_strerror(rc));
		exit(1);
	}

	//----- allocate a hardware parameters object
	snd_pcm_hw_params_alloca(&params);

	//----- fill it in with default values
	snd_pcm_hw_params_any(handle, params);

	//<<<<<<<<<<<<<<<<<<<       set hardware parameters     >>>>>>>>>>>>>>>>>>>>>>
	//----- interleaved mode
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	//----- signed 16-bit little-endian format
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	//----- two channels
	snd_pcm_hw_params_set_channels(handle, params, 2);

	//----- sampling rate
	val = 44100;
	snd_pcm_hw_params_set_rate_near(handle,params,&val, &dir);

	rc=snd_pcm_hw_params(handle,params);
	if(rc<0)
	{
		fprintf(stderr,"unable to set hw parameter: %s\n",snd_strerror(rc));
		exit(1);
	}

	size=1024; //--- or any 4N
	frames=size/4; // 4 = 2_Bytes_per_chan(16bit) * 2_Channels

	buffer=(char *) malloc(size);
	printf("malloc buffer with size=%d\n",size);


	while(rc=read(0,buffer,size)) // redirect pcmFiLE
	{
		if(rc==0) // no data
		{
			fprintf(stderr,"end of file on input\n");
			break;
		}
		else if (rc != size) //short read
		{
			fprintf(stderr,"short read: read %d bytes\n",rc);
		}

		rc=snd_pcm_writei(handle,buffer,frames); //write to hw to playback 
		if (rc == -EPIPE)
		{
			//EPIPE means underrun
			fprintf(stderr,"underrun occurred\n");
			snd_pcm_prepare(handle);
		}

		else if(rc<0)
		{
			fprintf(stderr,"error from writei():%s\n",snd_strerror(rc));
		}

		else if (rc != (int)frames)
		{
			fprintf(stderr,"short write, write %d of total %d frames\n",rc,(int)frames);
		}
	}

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);

	return 0;
}

