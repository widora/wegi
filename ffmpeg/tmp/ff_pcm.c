/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Note:
1. Mplayer will take the pcm device exclusively?? ---NOPE.
2. ctrl_c also send interrupt signal to MPlayer to cause it stuck.
  Mplayer interrrupted by signal 2 in Module: enable_cache
  Mplayer interrrupted by signal 2 in Module: play_audio


Midas Zhou
-------------------------------------------------------------------*/
#include "ff_pcm.h"
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
		EGI_PLOG(LOGLV_ERROR,"%s(): unable to open pcm device: %s\n",__func__,snd_strerror(rc));
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

/*----------------------------------------------
  close pcm device and free resources

----------------------------------------------*/
void close_ffpcm_device(void)
{
	if(g_ffpcm_handle != NULL) {
		snd_pcm_drain(g_ffpcm_handle);
		snd_pcm_close(g_ffpcm_handle);
	}
}


/*-----------------------------------------------------------------------
send buffer data to pcm play device with NONINTERLEAVED frame format !!!!
PCM access mode MUST have been set properly in open_pcm_device().

buffer ---  point to pcm data buffer
nf     ---  number of frames

Return:
	>0    OK
	<0   fails
------------------------------------------------------------------------*/
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
		EGI_PLOG(LOGLV_ERROR,"%s: error from writen():%s\n",__func__, snd_strerror(rc));
        }
        else if (rc != nf)
        {
                //fprintf(stderr,"short write, write %d of total %d frames\n",rc, nf);
		EGI_PLOG(LOGLV_ERROR,"%s: short write, write %d of total %d frames\n", __func__,rc,nf);
        }
}

/*------------------------------------------------------------------------
Get current volume value from the first available channel, and then set all
volume to the given value.

pgetvol: 	(0-100), pointer to a value to pass the volume percentage.
		If NULL, ignore.

psetvol: 	(0-100), volume value of percentage*100.
		If NULL, ignore.

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------------*/
int ffpcm_getset_volume(int *pgetvol, int *psetvol)
{
	int ret;
	long min=-1;
	long max=-2;
	long vrange;
	long vol;
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t* elem;
	snd_mixer_selem_channel_id_t chn;
	//snd_mixer_selem_channel_id_t vchn;
	const char *card="default";
	const char *selem_name="PCM";


	/* open an empty mixer */
	ret=snd_mixer_open(&handle,0); /* 0 unused param*/
	if(ret!=0){
		printf("Open mixer fails: %s",snd_strerror(ret));
		ret=-1;
		goto FAILS;
	}

	/* Attach an HCTL specified with the CTL device name to an opened mixer */
	ret=snd_mixer_attach(handle,card);
	if(ret!=0){
		printf("Mixer attach fails: %s",snd_strerror(ret));
		ret=-2;
		goto FAILS;
	}

	/* Register mixer simple element class */
	ret=snd_mixer_selem_register(handle,NULL,NULL);
	if(ret!=0){
		printf("Mixer simple element class register fails: %s",snd_strerror(ret));
		ret=-3;
		goto FAILS;
	}

	/* Load a mixer element	*/
	ret=snd_mixer_load(handle);
	if(ret!=0){
		printf("Load mixer element fails: %s",snd_strerror(ret));
		ret=-4;
		goto FAILS;
	}

	/* alloc snd_mixer_selem_id_t */
	snd_mixer_selem_id_alloca(&sid);
	/* Set index part of a mixer simple element identifier */
	snd_mixer_selem_id_set_index(sid,0);
	/* Set name  part of a mixer simple element identifier */
	snd_mixer_selem_id_set_name(sid,selem_name);

	/* Find a mixer simple element */
	elem=snd_mixer_find_selem(handle,sid);
	if(elem==NULL){
		printf("Find mixer simple element fails.\n");
		ret=-5;
		goto FAILS;
	}

	/* Get range for playback volume of a mixer simple element */
	snd_mixer_selem_get_playback_volume_range(elem,&min,&max);
	if( min<0 || max<0 ){
		printf("Get range of volume fails.!\n");
		ret=-6;
		goto FAILS;
	}
	//printf("Get playback range Min:%ld - Max:%ld.\n",min,max);
	vrange=max-min+1;

	/* get volume , ret=0 Ok */
        for (chn = 0; chn < 32; chn++) {
                if (!snd_mixer_selem_has_playback_channel(elem, chn))
                        continue;
		ret=snd_mixer_selem_get_playback_volume(elem, chn, &vol);
		if(ret<0) {
			printf("Get palyback volume error on channle %d.\n",chn);
			ret=-7;
			goto FAILS;
		}
		else {
			if(pgetvol!=NULL) {
				*pgetvol=vol*100/vrange;
				//printf("Get palyback volume: %ld[%d] on channle %d.\n",vol,*pgetvol,chn);
			}
			break; /* break */
		}
                if (chn == 0 && snd_mixer_selem_has_playback_volume_joined(elem))
                        break;
        }

	/* set volume, ret=0 OK */
	//snd_mixer_selem_set_playback_volume_all(elem, val);
	//snd_mixer_selem_set_playback_volume(elem, chn, val);

	/* set volume, ret=0 OK */
	if(psetvol != NULL) {
		vol=(*psetvol)%100*vrange/100;
	        snd_mixer_selem_set_playback_volume_all(elem, vol );
		//printf("Set playback all volume to: %ld.\n",vol);
	}

	ret=0;
 FAILS:
	snd_mixer_close(handle);
	return ret;
}

