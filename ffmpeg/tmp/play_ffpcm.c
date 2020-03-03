/*-----------------------------------------------------------------------
Note:
1. Mplayer will take the pcm device exclusively??
2. ctrl_c also send interrupt signal to MPlayer to cause it stuck.
  Mplayer interrrupted by signal 2 in Module: enable_cache
  Mplayer interrrupted by signal 2 in Module: play_audio

Midas Zhou
-----------------------------------------------------------------------*/
#include "play_ffpcm.h"
#include "egi_debug.h"
#include "egi_log.h"
#include "egi_timer.h"

/* definition of global varriable: g_ffpcm_handle */
static snd_pcm_t *g_ffpcm_handle;
static bool g_blInterleaved;

/*-------------------------------------------------------------------------------
 Open an PCM device and set following parameters:

 1.  access mode:		(SND_PCM_ACCESS_RW_INTERLEAVED)
 2.  PCM format:		(SND_PCM_FORMAT_S16_LE)
 3.  number of channels:	unsigned int nchan
 4.  sampling rate:		unsigned int srate
 5.  bl_interleaved:		TRUE for INTERLEAVED access,
				FALSE for NONINTERLEAVED access
Return:
a	0  :  OK
	<0 :  fails

Midas Zhou
-----------------------------------------------------------------------------------*/
int prepare_ffpcm_device(unsigned int nchan, unsigned int srate, bool bl_interleaved)
{
	int rc;
        snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
        int dir=0;

	/* save interleave mode */
	g_blInterleaved=bl_interleaved;

	/* open PCM device for playblack */
	rc=snd_pcm_open(&g_ffpcm_handle,"default",SND_PCM_STREAM_PLAYBACK,0);
	if(rc<0)
	{
		EGI_PLOG(LOGLV_ERROR,"%s(): unable to open pcm device: %s\n",__FUNCTION__,snd_strerror(rc));
		//exit(-1);
		return rc;
	}

	/* allocate a hardware parameters object */
	snd_pcm_hw_params_alloca(&params);

	/* fill it in with default values */
	snd_pcm_hw_params_any(g_ffpcm_handle, params);

	/* <<<<<<<<<<<<<<<<<<<       set hardware parameters     >>>>>>>>>>>>>>>>>>>>>> */
	/* if interleaved mode */
	if(bl_interleaved)  {
		snd_pcm_hw_params_set_access(g_ffpcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	}
	/* otherwise noninterleaved mode */
	else {
		/* !!!! use noninterleaved mode to play ffmpeg decoded data !!!!! */
		snd_pcm_hw_params_set_access(g_ffpcm_handle, params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
	}

	/* signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(g_ffpcm_handle, params, SND_PCM_FORMAT_S16_LE);

	/* set channel	*/
	snd_pcm_hw_params_set_channels(g_ffpcm_handle, params, nchan);

	/* sampling rate */
	snd_pcm_hw_params_set_rate_near(g_ffpcm_handle, params, &srate, &dir);
	if(dir != 0)
		printf(" Actual sampling rate is set to %d HZ!\n",srate);

	/* set HW params */
	rc=snd_pcm_hw_params(g_ffpcm_handle,params);
	if(rc<0) /* rc=0 on success */
	{
		fprintf(stderr,"unable to set hw parameter: %s\n",snd_strerror(rc));
		return rc;
	}

	/* get period size */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	printf("snd pcm period size = %d frames\n",(int)frames);

	return rc;
}

/*----------------------------------------------------
  close pcm device and free resources

----------------------------------------------------*/
void close_ffpcm_device(void)
{
	if(g_ffpcm_handle != NULL) {
		snd_pcm_drain(g_ffpcm_handle);
		snd_pcm_close(g_ffpcm_handle);
	}
}



/*-----------------------------------------------------------------
send buffer data to pcm play device with NONINTERLEAVED frame format !!!!
PCM access mode MUST have been set properly in open_pcm_device().

buffer ---  point to pcm data buffer
nf     ---  number of frames

Return:
	>0    OK
	<0   fails
------------------------------------------------------------------*/
void  play_ffpcm_buff(void ** buffer, int nf)
{
	int rc;

	/* write interleaved frame data */
	if(g_blInterleaved)
	        rc=snd_pcm_writei(g_ffpcm_handle,buffer,(snd_pcm_uframes_t)nf );
	/* write noninterleaved frame data */
	else
       	        rc=snd_pcm_writen(g_ffpcm_handle,buffer,(snd_pcm_uframes_t)nf ); //write to hw to playback
        if (rc == -EPIPE)
        {
            /* EPIPE means underrun */
            //fprintf(stderr,"snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n");
            EGI_PDEBUG(DBG_FFPLAY,"[%lld]: snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n",
            						tm_get_tmstampms() );
	    snd_pcm_prepare(g_ffpcm_handle);
        }
	else if(rc<0)
        {
        	//fprintf(stderr,"error from writen():%s\n",snd_strerror(rc));
		EGI_PLOG(LOGLV_ERROR,"%s: error from writen():%s\n",__FUNCTION__, snd_strerror(rc));
        }
        else if (rc != nf)
        {
                //fprintf(stderr,"short write, write %d of total %d frames\n",rc, nf);
		EGI_PLOG(LOGLV_ERROR,"%s: short write, write %d of total %d frames\n",
								__FUNCTION__, rc, nf);
        }

}
