/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


                        <<   Glossary  >>

Sample:         A digital value representing a sound amplitude,
                sample data width usually to be 8bits or 16bits.
Channels:       1-Mono. 2-Stereo, ...
Channel_layout: For FFMPEG: It's a 64bits integer,with each bit set for one channel:
		0x00000001(1<<0) for AV_CH_FRONT_LEFT
		0x00000002(1<<1) for AV_CH_FRONT_RIGHT
		0x00000004(1<<2) for AV_CH_FRONT_CENTER
			 ... ...
Frame_Size:     Sizeof(one sample)*channels, in bytes.
		(For ffmpeg: frame_size=number of frames, may refer to encoded/decoded data size.)
Rate:           Frames per second played/captured. in HZ.
Data_Rate:      Frame_size * Rate, usually in kBytes/s(kb/s), or kbits/s.
Period:         Period of time that a hard ware spends in processing one pass data/
                certain numbers of frames, as capable of the HW.
Period Size:    Representing Max. frame numbers a hard ware can be handled for each
                write/read(playe/capture), in HZ, NOT in bytes??
                (different value for PLAYBACK and CAPTURE!!)
		Also can be set in asound.conf ?
		WM8960 running show:
   		1536 frames for CAPTURE(default); 278 frames for PLAYBACK(default), depends on HW?
Chunk(period):  frames from snd_pcm_readi() each time for shine enconder.
Buffer:         Usually in N*periods

Interleaved mode:       record period data frame by frame, such as
                        frame1(Left sample,Right sample), frame2(), ......
Uninterleaved mode:     record period data channel by channel, such as
                        period(Left sample,Left ,left...), period(right,right...),
                        period()...
snd_pcm_uframes_t       unsigned long
snd_pcm_sframes_t       signed long


		--- FFMPEG SAMPLE FORMAT ---

enum AVSampleFormat {
    AV_SAMPLE_FMT_NONE = -1,
    AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
    AV_SAMPLE_FMT_S16,         ///< signed 16 bits
    AV_SAMPLE_FMT_S32,         ///< signed 32 bits
    AV_SAMPLE_FMT_FLT,         ///< float
    AV_SAMPLE_FMT_DBL,         ///< double

    AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
    AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
    AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
    AV_SAMPLE_FMT_FLTP,        ///< float, planar
    AV_SAMPLE_FMT_DBLP,        ///< double, planar

    AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
};

#define AV_CH_LAYOUT_MONO              (AV_CH_FRONT_CENTER)  4
#define AV_CH_LAYOUT_STEREO            (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT) 1+2=3


Note:
0. For permission mode: set 'rw' for users to access device files at /dev/snd/
   Example:
	crw-rw-rw-    1 root     root      116,   0 May  6 11:59 controlC0
	crw-rw-rw-    1 root     root      116,  24 May  6 11:59 pcmC0D0c
	crw-rw-rw-    1 root     root      116,  16 May  6 11:59 pcmC0D0p
	crw-rw-rw-    1 root     root      116,  33 May  6 11:59 time

1. Mplayer will take the pcm device exclusively?? ---NOPE.
2. ctrl_c also send interrupt signal to MPlayer to cause it stuck.
   Mplayer interrrupted by signal 2 in Module: enable_cache
   Mplayer interrrupted by signal 2 in Module: play_audio
3. Balance between lantency, frames per write, conf rate.

TODO:
1. To play PCM data with format other than SND_PCM_FORMAT_S16_LE.

Journal:
	1. Modify egi_play_pcm_buff():  param (void** buffer) to (void* buffer)
	2. Modify egi_pcmhnd_playBuff(): param (void** buffer) to (void* buffer)

2021-04-13:
	1. Replace 'elem in egi_getset_pcm_volume()' by 'g_volmix_elem'.
	2. Add egi_pcm_mute()

2021-04-19:
	1.egi_pcmbuf_playback(): If input pcmbuf->pcm_handle is NULL, then create/open it
	  first, and close it after playback.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <stdint.h>
#include <alsa/asoundlib.h>
//#include <sound/asound.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>
#include <sndfile.h>
#include "egi_pcm.h"
#include "egi_log.h"
#include "egi_debug.h"
#include "egi_timer.h"
#include "egi_cstring.h"

/* --- For system PCM PLAY/CAPTURE -- */
static snd_pcm_t *g_ffpcm_handle;	/* for PCM playback */
static snd_mixer_t *g_volmix_handle; 	/* for volume control */
static snd_mixer_elem_t* g_volmix_elem; /* mixer elememnt */
static bool g_blInterleaved;		/* Interleaved or Noninterleaved */
static char g_snd_device[256]; 		/* Pending, use "default" now. */
static int period_size;			/* period size of HW, in frames. */

/*-------------------------------------------------------------------------------
 Open an PCM device and set following parameters:

@nchan:			number of channels
@srate:			sample rate
@bl_interleaved:	TRUE/FALSE for interleaved/non-interleaved data

 1.  access mode:		(SND_PCM_ACCESS_RW_INTERLEAVED)
 2.  PCM format:		(SND_PCM_FORMAT_S16_LE)
 3.  number of channels:	unsigned int nchan
 4.  sampling rate:		unsigned int srate
 5.  bl_interleaved:		TRUE for INTERLEAVED access,
				FALSE for NONINTERLEAVED access
Return:
	0  :  OK
	<0 :  fails
-----------------------------------------------------------------------------------*/
int egi_prepare_pcm_device(unsigned int nchan, unsigned int srate, bool bl_interleaved)
{
	int rc;
        snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
        int dir=0;

	/* save interleave mode */
	g_blInterleaved=bl_interleaved;

	/* set PCM device */
	sprintf(g_snd_device,"default");

	/* open PCM device for playblack */
	rc=snd_pcm_open(&g_ffpcm_handle,g_snd_device,SND_PCM_STREAM_PLAYBACK,0);
	if(rc<0)
	{
		EGI_PLOG(LOGLV_ERROR,"%s(): unable to open pcm device '%s': %s\n",
							__func__, g_snd_device, snd_strerror(rc) );
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
		printf("%s: Actual sampling rate is set to %d HZ!\n",__func__, srate);

	/* set HW params */
	rc=snd_pcm_hw_params(g_ffpcm_handle,params);
	if(rc<0) /* rc=0 on success */
	{
		EGI_PLOG(LOGLV_ERROR,"unable to set hw parameter: %s\n",snd_strerror(rc));
		return rc;
	}

	/* get period size */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	period_size=(int)frames;
//	EGI_PDEBUG(DBG_NONE, "snd pcm period size = %d frames\n", (int)frames);
	EGI_PLOG(LOGLV_CRITICAL,"%s: snd pcm period size = %d frames.", __func__, (int)frames);
	printf("%s: snd pcm period size = %d frames.\n", __func__, (int)frames);

	return rc;
}

/*----------------------------------------------
	Return period size of the PCM HW
----------------------------------------------*/
int egi_pcm_period_size(void)
{
	return period_size;
}


/*----------------------------------------------
  close pcm device and free resources
  together with volmix.
----------------------------------------------*/
void egi_close_pcm_device(void)
{
	if(g_ffpcm_handle != NULL) {
		snd_pcm_drain(g_ffpcm_handle);
		snd_pcm_close(g_ffpcm_handle);
		g_ffpcm_handle=NULL;
	}

	if(g_volmix_handle !=NULL) {
		snd_mixer_close(g_volmix_handle);
		g_volmix_handle=NULL;
	}
}


/*-----------------------------------------------------------------------
send buffer data to pcm play device with NONINTERLEAVED frame format !!!!
PCM access mode MUST have been set properly in open_pcm_device().

@buffer    	point to pcm data buffer
@nf     	number of frames

------------------------------------------------------------------------*/
void  egi_play_pcm_buff(void * buffer, int nf)
{
	int rc;

	/* write interleaved frame data */
	if(g_blInterleaved)
	        rc=snd_pcm_writei(g_ffpcm_handle,buffer,(snd_pcm_uframes_t)nf );
	/* write noninterleaved frame data */
	else
       	        rc=snd_pcm_writen(g_ffpcm_handle, buffer,(snd_pcm_uframes_t)nf ); //write to hw to playback
        if (rc == -EPIPE)
        {
            /* EPIPE means underrun */
            //fprintf(stderr,"snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n");
            EGI_PDEBUG(DBG_PCM,"[%lld]: snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n",
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

/*--------------------------------------------------------------------
Playback buff data through pcm_handle, which has been setup with adequate
parameters.

@pcm_handle	PCM handle
@Interleave	Interleaved or noninterleaved frame data
@buffer    	point to pcm data buffer
@nf     	number of frames

Return:
#	>0    OK
#	<0   fails
--------------------------------------------------------------------*/
void  egi_pcmhnd_playBuff(snd_pcm_t *pcm_handle, bool Interleaved, void *buffer, int nf)
{
	int rc;

	/* write interleaved frame data */
	if(Interleaved)
	        rc=snd_pcm_writei(pcm_handle, buffer, (snd_pcm_uframes_t)nf );
	/* write noninterleaved frame data */
	else
       	        rc=snd_pcm_writen(pcm_handle, buffer, (snd_pcm_uframes_t)nf ); //write to hw to playback
        if (rc == -EPIPE)
        {
            /* EPIPE means underrun */
            //fprintf(stderr,"snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n");
            EGI_PLOG(LOGLV_ERROR,"%s: snd_pcm_writen() or snd_pcm_writei(): underrun occurred",
			            						__func__);
	    snd_pcm_prepare(pcm_handle);
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


/*-------------------------------------
Mute/Demut EGI PCM.

Note: This is for SYS PCM! It will mute/demute
      all current applications.

Return:
	0	OK
	<0	Fails
------------------------------------*/
int egi_pcm_mute(void)
{
	int ret=0;

#if 0 /* Use ALSA mixer switch */
	static int control=1;

	/* Check volmix_hande */
	if( g_volmix_handle==NULL || g_volmix_elem==NULL) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: A g_volmix_handle OR g_volmix_elem is NULL!",__func__);
		return -1;
	}

	//if( snd_mixer_selem_has_playback_switch(g_volmix_elem) ) {
	//if( snd_mixer_selem_has_playback_switch_joined(g_volmix_elem) ) {
	if( snd_mixer_selem_has_common_switch(g_volmix_elem) ) {
		control = !control;
		if( (ret=snd_mixer_selem_set_playback_switch_all(g_volmix_elem, control)) !=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: _set_playback_switch_all: %s",__func__, snd_strerror(ret));
			return -2;
		}
		else
			return 0;
	}
	else {
		EGI_PLOG(LOGLV_ERROR, "%s: Mixer selem has NO playback switch! ",__func__);
		return -3;
	}

#else  /* Get Set volume */
	static int save_vol=0;
	int zero=0;

	/* Mute */
	if(save_vol==0) {
		ret=egi_getset_pcm_volume(&save_vol, &zero);
		egi_dpstd("Get vol=%d\n", save_vol);
	}
	/* DeMute */
	else {
		ret=egi_getset_pcm_volume(NULL, &save_vol);
		egi_dpstd("Set vol=%d\n", save_vol);
		save_vol=0;
	}

	return ret;
#endif
}


/*-------------------------------------------------------------------------------
Get current volume value from the first available channel, and then set all
volume to the given value.

!!! --- WARNING: Static variabls applied, for one running thread only --- !!!

pgetvol: 	[0-100], pointer to a value to pass the volume percentage value.
		If NULL, ignore.
		Forced to [0-100] if out of range.

psetvol: 	[0-100], pointer to a volume value of percentage*100, which
		is about to set to the mixer.
		If NULL, ignore.
		Forced to [0-100] if out of range.

Return:
	0	OK
	<0	Fails
------------------------------------------------------------------------------*/
int egi_getset_pcm_volume(int *pgetvol, int *psetvol)
{
	int ret;
	static long  min=-1; /* selem volume value limit */
	static long  max=-2;
	//static long  vrange;
	int	     pvol;   /* vol in percentage*100 */
	long int     vol;

	/* Min dBmute */
	const long int  dBmute=-114*100; 	/* in dB*100, Set Mute level */

	static long  dBmin=-114*100;	/* in dB*100 selem volume value limit */
	static long  dBmax=-114*100; 	/* in dB*100 */
	static long  dBvrange;		/* in dB*100 */
	static long  dBvol;     	/* in dB*100 */

	static snd_mixer_selem_id_t *sid;
//	static snd_mixer_elem_t* elem;  // replaced by g_volmix_elem
	static snd_mixer_selem_channel_id_t chn;
	const char *card="default";
	static char selem_name[128]={0};
	static char dBvol_type[128];	 /* linear or nonlinear */
	static bool linear_dBvol=false;  /* default as nonlinear */
	static bool has_selem=false;
	static bool finish_setup=false;

    /* First time init */
    if( !finish_setup || g_volmix_handle==NULL )
    {
	if( g_volmix_handle==NULL)
		EGI_PLOG(LOGLV_CRITICAL,"%s: A g_volmix_handle is about to open...",__func__);

	/* must reset token first */
	has_selem=false;
	finish_setup=false;

        /* read egi.conf and get selem_name and volume adjusting method */
        if ( !has_selem ) {
		/* To get simple mixer name */
		if(egi_get_config_value("EGI_SOUND","selem_name",selem_name) != 0) {
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to get config value 'selem_name'.",__func__);
			has_selem=false;
	                return -1;
		}
		else {
			EGI_PLOG(LOGLV_INFO,"%s: Succeed to get config value selem_name='%s'",
										__func__, selem_name);
			has_selem=true;
		}

		/* To get dB volume type, volume adjusted accordingly */
		memset(dBvol_type,0,sizeof(dBvol_type));
		if(egi_get_config_value("EGI_SOUND","dBvol_type", dBvol_type) != 0) {
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to get config value 'dBvol_type'.",__func__);
			/* Ingore, use default dBvol type. */
		}
		else {
			EGI_PLOG(LOGLV_INFO,"%s: Succeed to get config value dBvol_type='%s'",
										__func__, dBvol_type);
			if( strcmp(dBvol_type,"linear")==0 )
				linear_dBvol=true;
			else
				linear_dBvol=false;
		}

	}

	//printf(" --- Selem: %s --- \n", selem_name);

	/* open an empty mixer for volume control */
	ret=snd_mixer_open(&g_volmix_handle,0); /* 0 unused param*/
	if(ret!=0){
		EGI_PLOG(LOGLV_ERROR, "%s: Open mixer fails: %s",__func__, snd_strerror(ret));
		ret=-1;
		goto FAILS;
	}

	/* Attach an HCTL specified with the CTL device name to an opened mixer */
	ret=snd_mixer_attach(g_volmix_handle,card);
	if(ret!=0){
		EGI_PLOG(LOGLV_ERROR, "%s: Mixer attach fails: %s", __func__, snd_strerror(ret));
		ret=-2;
		goto FAILS;
	}

	/* Register mixer simple element class */
	ret=snd_mixer_selem_register(g_volmix_handle,NULL,NULL);
	if(ret!=0){
		EGI_PLOG(LOGLV_ERROR,"%s: snd_mixer_selem_register(): Mixer simple element class register fails: %s",
									__func__, snd_strerror(ret));
		ret=-3;
		goto FAILS;
	}

	/* Load a mixer element	*/
	ret=snd_mixer_load(g_volmix_handle);
	if(ret!=0){
		EGI_PLOG(LOGLV_ERROR,"%s: Load mixer element fails: %s",__func__, snd_strerror(ret));
		ret=-4;
		goto FAILS;
	}

	/* alloc snd_mixer_selem_id_t */
	snd_mixer_selem_id_alloca(&sid);
	/* Set index part of a mixer simple element identifier */
	snd_mixer_selem_id_set_index(sid,0);
	/* Set name part of a mixer simple element identifier */
	snd_mixer_selem_id_set_name(sid,selem_name);

	/* Find a mixer simple element */
	g_volmix_elem=snd_mixer_find_selem(g_volmix_handle,sid);
	if(g_volmix_elem==NULL){
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to find mixer simple element '%s'.",__func__, selem_name);
		ret=-5;
		goto FAILS;
	}

	/* Get range for playback volume of a mixer simple element */
	snd_mixer_selem_get_playback_volume_range(g_volmix_elem, &min, &max);
	if( min<0 || max<0 ){
		EGI_PLOG(LOGLV_ERROR,"%s: Get range of volume fails.!",__func__);
		ret=-6;
		goto FAILS;
	}
	// vrange=max-min+1;
	EGI_PLOG(LOGLV_CRITICAL,"%s: Get playback volume range Min.%ld - Max.%ld.", __func__, min, max);

	/* Get dB range for playback volume of a mixer simple element
	 * dBmin and dBmax in dB*100
	 */
	ret=snd_mixer_selem_get_playback_dB_range(g_volmix_elem, &dBmin, &dBmax);
	if( ret !=0 ){
		EGI_PLOG(LOGLV_ERROR,"%s: Get range of playback dB range fails.!",__func__);
		ret=-6;
		goto FAILS;
	}
	EGI_PLOG(LOGLV_CRITICAL,"%s: Get playback volume dB range Min.dB%ld - Max.dB%ld.",
										__func__, dBmin, dBmax);
	/* Reset dBmin to dBmute */
	if(dBmin < dBmute)
		dBmin=dBmute;
	dBvrange=dBmax-dBmin+1;

	/* get volume , ret=0 Ok */
        for (chn = 0; chn < 32; chn++) {
		/* Master control channel */
                if (chn == 0 && snd_mixer_selem_has_playback_volume_joined(g_volmix_elem)) {
			EGI_PLOG(LOGLV_CRITICAL,"%s: '%s' channle 0, Playback volume joined!",
										__func__, selem_name);
			finish_setup=true;
                        break;
		}
		/* Other control channel */
                if (!snd_mixer_selem_has_playback_channel(g_volmix_elem, chn))
                        continue;
		else {
			finish_setup=true;
			break;
		}
        }

	/* Fail to find a control channel */
	if(finish_setup==false) {
		ret=-8;
		goto FAILS;
	}

     }	/*  ---------   Now we finish first setup  ---------  */


	/* try to get playback volume value on the channel */
	snd_mixer_handle_events(g_volmix_handle); /* handle events first */

	ret=snd_mixer_selem_get_playback_volume(g_volmix_elem, chn, &vol); /* suppose that each channel has same volume value */
	if(ret<0) {
		EGI_PLOG(LOGLV_ERROR,"%s: Get playback volume error on channle %d.",
										__func__, chn);
		ret=-7;
		goto FAILS;
	}
//	EGI_PLOG(LOGLV_CRITICAL,"%s: Get playback vol=%ld.", __func__, vol);

	/* try to get playback dB value on the channel */
	ret=snd_mixer_selem_get_playback_dB(g_volmix_elem, chn, &dBvol); /* suppose that each channel has same volume value */
	if(ret<0) {
		EGI_PLOG(LOGLV_ERROR,"%s: Get playback dB error on channle %d.",
										__func__, chn);
		ret=-7;
		goto FAILS;
	}
//	EGI_PLOG(LOGLV_CRITICAL,"%s: Get playback dBvol=%ld.", __func__, dBvol);

	/* To get volume in percentage */
	if( pgetvol!=NULL ) {
		//printf("%s: Get volume: %ld[%d] on channle %d.\n",__func__,vol,*pgetvol,chn);

	#if 0 /* ---- (Option 1) : in volume_percentage*100 ---  */
		/*** Convert vol back to percentage value.
		 *  	--- NOT GOOD! ---
		 *      pv=percentage*100; v=volume value in vrange.
		 *	pv/100=lg(v^2)/lg(vrange^2)
		 *	pv=lg(v^2)*100/lg(vrange^2)
		 */
		 //printf("get alsa vol=%ld\n",vol);
		 // *pgetvol=log10(vol)*100.0/log10(vrange);  /* in percent*100 */
		*pgetvol=vol*100/vrange;  /* actual volume value to percent. value */

	#else /* ---- (Option 2) : in dB_percentage *100 ---
		       *  lg(pv)/lg(100) = (dBvol-dBmin)/(dBmax-dBmin) = (dBvol-dBmin)/dBvrange
		       *   2*(dBvol-dBmin)=lg(pv)*dBvrange  --->  pv=pow(10, 2*(dBvol-dBmin)/dBvrange )
		       */
		if(dBvol<dBmin+1)
			dBvol=dBmin+1; /* to avoid dBvol-dBmin==0 */

		if(linear_dBvol) {
			/* 1. Linear type dB curve */
			*pgetvol=(dBvol-dBmin)*100/dBvrange;
			printf("get alsa dBvol=%ld  percent.=%d\n", dBvol, *pgetvol);
		}
		else {
			/* 2. Nonlinear type dB curve */
			*pgetvol=pow(10,2.0*(dBvol-dBmin)/dBvrange);  /* in percent*100 */
		}
	#endif
	}

	/* set volume, ret=0 OK */
	//snd_mixer_selem_set_playback_volume_all(elem, val);
	//snd_mixer_selem_set_playback_volume(elem, chn, val);
	//snd_mixer_selem_set_playback_volume_range(elem,0,32);
	// set_volume_mute_value()

	/* set volume in percentage, ret=0 OK */
	if(psetvol != NULL) {
		/* limit input to [0-100] */
		//printf("%s: min=%ld, max=%ld vrange=%ld\n",__func__, min,max, vrange);
//		printf("%s: dBmin=%ld, dBmax=%ld dBvrange=%ld\n",__func__, dBmin,dBmax, dBvrange);
		if(*psetvol > 100)
			*psetvol=100;
		if(*psetvol < 0 )
			*psetvol=0;

		pvol=*psetvol;  /* vol: in percent*100 */

	#if 0 /* ----- (OPTION 1) : Call snd_mixer_selem_set_playback_volume_all() ------ */
		/*** Covert percentage value to lg(pv^2) related value, so to sound dB relative.
		 *  	--- NOT GOOD! ---
		 *      pv=percentage*100; v=volume value in vrange.
		 *	pv/100=lg(v^2)/lg(vrange^2)
		 *	v=10^(pv*lg(vrange)/100)
		 */
		if(pvol<1)pvol=1; /* to avoid 0 */
		//vol=pow(10, vol*log10(vrange)/100.0);
		vol=pvol*vrange/100;  /* percent. vol to actual volume value  */
//		printf("%s: Percent. vol=%d%%, dB related vol=%ld\n", __func__, *psetvol, vol);

		/* normalize vol, Not necessay now?  */
		if(vol > max) vol=max;
		else if(vol < min) vol=min;
	        snd_mixer_selem_set_playback_volume_all(g_volmix_elem, vol);
		//printf("Set playback all volume to: %ld.\n",vol);

	#else /* ----- (Option 2) : Call snd_mixer_selem_set_playback_dB_all() ----
		       *  lg(pv)/lg(100) = (dBvol-dBmin)/(dBmax-dBmin) = (dBvol-dBmin)/dBvrange
		       *   2*(dBvol-dBmin)=lg(pv)*dBvrange  --->  dBvol=0.5*lg(pv)*dBvrange+dBmin;
		       */
		//dBvol=dBmin+dBvrange*vol/100;
		if(pvol==0)
			dBvol=dBmin;
		else {
			if(linear_dBvol) {
				/* 1. Linear type dB curve */
				dBvol=pvol*dBvrange/100+dBmin;
			}
			else {
				/* 2. Nonlinear tyep dB curve */
				dBvol=0.5*log10(pvol)*dBvrange+dBmin;
			}
		}
//		printf("%s: set %s dBvol=%ld\n", __func__, linear_dBvol?"linear":"nonlinear" ,dBvol);
		/* dB_all(elem, vol, dir)  vol: dB*100 dir>0 round up, otherwise down */
	        snd_mixer_selem_set_playback_dB_all(g_volmix_elem, dBvol, 1);
	#endif

	}

	return 0;

 FAILS:
	snd_mixer_close(g_volmix_handle);
	g_volmix_handle=NULL;

	return ret;
}


/*---------------------------------------------------
Step up/down pcm volume to range [0 100]

@vdelt:  adjusting value for volume.

return
	0	OK
	<0	Fails
---------------------------------------------------*/
int egi_adjust_pcm_volume(int vdelt)
{
	int vol;

	if(egi_getset_pcm_volume(&vol, NULL) !=0)
		return -1;

	vol += vdelt;

	if(egi_getset_pcm_volume(NULL,&vol) !=0)
		return -2;

	return 0;
}

/*---------------------------------------------------------------------------------
Set params for PCM handler.

Note:
1. TBD&TODO: snd_pcm_drain() remaining data before reset params !???

@pcm_handle:		PCM PLAYBACK handler.
@sformat:	 	Sample format, Example: SND_PCM_FORMAT_S16_LE
@access_type:		PCM access type, Exmple: SND_PCM_ACCESS_RW_INTERLEAVED
//@soft_resample:		0 = disallow alsa-lib resample stream, 1 = allow resampling
@nchanl:		Number of channels
@srate:		 	Sample rate
@latency:		required overall latency in us

Return:
	0	OK
	<0	Fails
-----------------------------------------------------------------------------------*/
int egi_pcmhnd_setParams( snd_pcm_t *pcm_handle, snd_pcm_format_t sformat, snd_pcm_access_t access_type,
                        unsigned int nchanl, unsigned int srate )
{
	int rc=0;
        int dir=0;
        snd_pcm_hw_params_t *params=NULL;

        /* Allocate a hardware parameters object */
        snd_pcm_hw_params_alloca(&params);
	if(params==NULL) {
                printf("%s: Fail to allocate params!\n",__func__);
		return -1;
        }

        /* Fill it in with default values */
        if( (rc=snd_pcm_hw_params_any(pcm_handle, params)) !=0) {
                printf("%s: Err'%s'.\n",__func__, snd_strerror(rc));
		return rc;
        }

        /* Set access type  */
        if( (rc=snd_pcm_hw_params_set_access(pcm_handle, params, access_type)) !=0) {
                printf("%s: Err'%s'.\n",__func__, snd_strerror(rc));
		return rc;
        }

        /* Set format */
        if( (rc=snd_pcm_hw_params_set_format(pcm_handle, params, sformat)) !=0) {
                printf("%s: Err'%s'.\n",__func__, snd_strerror(rc));
		return rc;
        }

        /* Set channels  */
        if( (rc=snd_pcm_hw_params_set_channels(pcm_handle, params, nchanl)) !=0) {
                printf("%s: Err'%s'.\n",__func__, snd_strerror(rc));
		return rc;
        }

        /* sampling rate */
        if( (rc=snd_pcm_hw_params_set_rate_near(pcm_handle, params, &srate, &dir)) !=0) {
                printf("%s: Err'%s'.\n",__func__, snd_strerror(rc));
		return rc;
        }
        if(dir != 0)
                printf("%s: Actual sampling rate is set to %d HZ!\n",__func__, srate);

        /* Set HW params */
        if ( (rc=snd_pcm_hw_params(pcm_handle, params)) !=0 ) {
                printf("%s: Err'%s'.\n",__func__, snd_strerror(rc));
		return rc;
        }

	return 0;
}


/*-----------------------------------------------------------------------------------
Open a PLAYBACK device and set params.

@dev_name:		name of PCM device, Example: "default", "plughw:0,0"
@sformat:	 	Sample format, Example: SND_PCM_FORMAT_S16_LE
@access_type:		PCM access type, Exmple: SND_PCM_ACCESS_RW_INTERLEAVED
@soft_resample:		0 = disallow alsa-lib resample stream, 1 = allow resampling
@nchanl:		Number of channels
@srate:		 	Sample rate
@latency:		required overall latency in us

Return:
	A poiter to snd_pcm_t	OK
	NULL			Fails
------------------------------------------------------------------------------------*/
snd_pcm_t*  egi_open_playback_device(  const char *dev_name, snd_pcm_format_t sformat,
					snd_pcm_access_t access_type, bool soft_resample,
					unsigned int nchanl, unsigned int srate, unsigned int latency
				    )
{
	snd_pcm_t *pcm_handle=NULL;
        snd_pcm_hw_params_t *params;
        int dir=0;
	snd_pcm_uframes_t frames;
	int rc;

        /* open pcm play device */
        if( snd_pcm_open(&pcm_handle, dev_name, SND_PCM_STREAM_PLAYBACK, 0) <0 ) {
                printf("%s: Fail to open PCM device '%s'.\n",__func__, dev_name);
                return NULL;
        }

#if 1 //////////////////////////////////////////////////////////////////////////////
        /* allocate a hardware parameters object */
        snd_pcm_hw_params_alloca(&params);

        /* fill it in with default values */
        snd_pcm_hw_params_any(pcm_handle, params);

        /* set access type  */
        snd_pcm_hw_params_set_access(pcm_handle, params, access_type);

        /* HW signed 16-bit little-endian format */
        snd_pcm_hw_params_set_format(pcm_handle, params, sformat); //SND_PCM_FORMAT_S16_LE);

        /* set channel  */
        snd_pcm_hw_params_set_channels(pcm_handle, params, nchanl);

        /* sampling rate */
        snd_pcm_hw_params_set_rate_near(pcm_handle, params, &srate, &dir);
        if(dir != 0)
                printf("%s: Actual sampling rate is set to %d HZ!\n",__func__, srate);

        /* set HW params */
        if ( (rc=snd_pcm_hw_params(pcm_handle,params)) != 0 )
	{
		printf("%s: unable to set hw parameter: %s\n",__func__, snd_strerror(rc));
		snd_pcm_close(pcm_handle);
		return NULL;
	}

	/* get period size */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	printf("%s: snd pcm period size is %d frames.\n", __func__, (int)frames);

#else  ///////////////////////////////////////////////////////////////////////////////
        /* set params for pcm play handle */
        if( snd_pcm_set_params( pcm_handle, sformat, access_type,
                                nchanl, srate, soft_resample, latency )  <0 ) {
                printf("%s: Fail to set params for pcm handle.!\n",__func__);
                snd_pcm_close(pcm_handle);
                return NULL;
        }


#endif


	return pcm_handle;
}


/*--------------------------------------------------------------
Create an EGI_PCMBUF with given paramters

@size:	   	in bytes, size of buff in EGI_PCMBUF
@nchanl:   	number of channles
@srate:	   	sample rate
@sformat:  	sample format,Example: SND_PCM_FORMAT_S16_LE
@access_type:   access type,Exmple: SND_PCM_ACCESS_RW_INTERLEAVED

Return:
	A poiter to EGI_PCMBUF	OK
	NULL			Fails
---------------------------------------------------------------*/
EGI_PCMBUF* egi_pcmbuf_create(  unsigned long size, unsigned int nchanl, unsigned int srate,
				snd_pcm_format_t sformat, snd_pcm_access_t access_type )
{
	EGI_PCMBUF* pcmbuf=NULL;

	if( size==0 || nchanl==0 ||srate==0)
		return NULL;

	pcmbuf=calloc(1, sizeof(EGI_PCMBUF));
	if(pcmbuf==NULL)
		return NULL;

	pcmbuf->pcmbuf=calloc(1, size);
	if(pcmbuf->pcmbuf==NULL) {
		free(pcmbuf);
		return NULL;
	}

	/* Assign members */
	pcmbuf->size=size;
	pcmbuf->nchanl=nchanl;
	pcmbuf->srate=srate;
	pcmbuf->sformat=sformat;
	pcmbuf->access_type=access_type;

	/* For depth, Only support U8,S8,S16 NOW... TODO */
	if(sformat<2)
		pcmbuf->depth=1;
	else
		pcmbuf->depth=2;

	return pcmbuf;
}


/*--------------------------------------
	Free an EGI_PCMBUF
--------------------------------------*/
void egi_pcmbuf_free(EGI_PCMBUF **pcmbuf)
{
	if(pcmbuf==NULL || *pcmbuf==NULL)
		return;

	if( (*pcmbuf)->pcmbuf != NULL )
		free((*pcmbuf)->pcmbuf);

	if( (*pcmbuf)->pcm_handle != NULL )
		snd_pcm_close((*pcmbuf)->pcm_handle);

	free(*pcmbuf);
	*pcmbuf=NULL;
}

/* -------------- For Libsndfile ------------------*/

#define SF_CONTAINER(x)         ((x) & SF_FORMAT_TYPEMASK)
#define SF_CODEC(x)                     ((x) & SF_FORMAT_SUBMASK)
#define CASE_NAME(x)            case x : return #x ; break ;

static const char* str_of_major_format(int format)
{
        switch (SF_CONTAINER (format))
        {       CASE_NAME (SF_FORMAT_WAV) ;
                CASE_NAME (SF_FORMAT_AIFF) ;
                CASE_NAME (SF_FORMAT_AU) ;
                CASE_NAME (SF_FORMAT_RAW) ;
                CASE_NAME (SF_FORMAT_PAF) ;
                CASE_NAME (SF_FORMAT_SVX) ;
                CASE_NAME (SF_FORMAT_NIST) ;
                CASE_NAME (SF_FORMAT_VOC) ;
                CASE_NAME (SF_FORMAT_IRCAM) ;
                CASE_NAME (SF_FORMAT_W64) ;
                CASE_NAME (SF_FORMAT_MAT4) ;
                CASE_NAME (SF_FORMAT_MAT5) ;
                CASE_NAME (SF_FORMAT_PVF) ;
                CASE_NAME (SF_FORMAT_XI) ;
                CASE_NAME (SF_FORMAT_HTK) ;
                CASE_NAME (SF_FORMAT_SDS) ;
                CASE_NAME (SF_FORMAT_AVR) ;
                CASE_NAME (SF_FORMAT_WAVEX) ;
                CASE_NAME (SF_FORMAT_SD2) ;
                CASE_NAME (SF_FORMAT_FLAC) ;
                CASE_NAME (SF_FORMAT_CAF) ;
                CASE_NAME (SF_FORMAT_WVE) ;
                CASE_NAME (SF_FORMAT_OGG) ;
                default :
                        break ;
          } ;

        return "BAD_MAJOR_FORMAT" ;
} /* ----------- str_of_major_format ------------- */

static const char* str_of_minor_format (int format)
{
        switch (SF_CODEC (format) )
	{
                CASE_NAME (SF_FORMAT_PCM_S8) ;
                CASE_NAME (SF_FORMAT_PCM_16) ;
                CASE_NAME (SF_FORMAT_PCM_24) ;
                CASE_NAME (SF_FORMAT_PCM_32) ;
                CASE_NAME (SF_FORMAT_PCM_U8) ;
                CASE_NAME (SF_FORMAT_FLOAT) ;
                CASE_NAME (SF_FORMAT_DOUBLE) ;
                CASE_NAME (SF_FORMAT_ULAW) ;
                CASE_NAME (SF_FORMAT_ALAW) ;
                CASE_NAME (SF_FORMAT_IMA_ADPCM) ;
                CASE_NAME (SF_FORMAT_MS_ADPCM) ;
                CASE_NAME (SF_FORMAT_GSM610) ;
                CASE_NAME (SF_FORMAT_VOX_ADPCM) ;
                CASE_NAME (SF_FORMAT_G721_32) ;
                CASE_NAME (SF_FORMAT_G723_24) ;
                CASE_NAME (SF_FORMAT_G723_40) ;
                CASE_NAME (SF_FORMAT_DWVW_12) ;
                CASE_NAME (SF_FORMAT_DWVW_16) ;
                CASE_NAME (SF_FORMAT_DWVW_24) ;
                CASE_NAME (SF_FORMAT_DWVW_N) ;
                CASE_NAME (SF_FORMAT_DPCM_8) ;
                CASE_NAME (SF_FORMAT_DPCM_16) ;
                CASE_NAME (SF_FORMAT_VORBIS) ;
                default :
                        break ;
         } ;

        return "BAD_MINOR_FORMAT" ;
} /* -------------------  str_of_minor_format  ---------------*/




/*------------------------------------------------------------------
Read a sound file by libsndfile, then create an EGI_PCMBUF and
load data to it.

Note: 1. Now only support for PCM_S8, PCM_U8, and PCM_16
	 depth max.=2. see in sf_readf_short(...)
      2. For wav files, the format for ALSA is SND_PCM_FORMAT_S16_LE.
      3. For wav files, access type is SND_PCM_ACCESS_RW_INTERLEAVED.
      4. pcmbuf->pcm_hand will NOT be created/opened here.

@path:	path of a sound file.

Return:
        A poiter to EGI_PCMBUF  OK
        NULL                    Fails
-------------------------------------------------------------------*/
EGI_PCMBUF* egi_pcmbuf_readfile(char *path)
{
        SNDFILE *snf;
        SF_INFO  sinfo={.format=0 };
	int ret;

	EGI_PCMBUF *pcmbuf=NULL;
	unsigned int  depth;
	unsigned long size;
        snd_pcm_format_t sformat;

	/* open wav file */
        snf=sf_open(path, SFM_READ, &sinfo); /* SFM_READ, SFM_WRITE, SFM_RDWR */
        if(snf==NULL) {
                printf("%s: Fail to open wav file, %s \n",__func__, sf_strerror(NULL));
                return NULL;
        }

#if 1 /* ------------  verbos  ----------- */
        printf("Open sound File '%s':\n", path);
        printf("Frames:         %"PRIu64"\n", sinfo.frames);
        printf("Sample Rate:    %d\n", sinfo.samplerate);
        printf("Channels:       %d\n", sinfo.channels);
        printf("Formate:        %s %s\n", str_of_major_format(sinfo.format),
                                          str_of_minor_format(sinfo.format) );
        printf("Sections:       %d\n", sinfo.sections);
        printf("Seekable:       %d\n", sinfo.seekable);
        printf("COPYRIGHT:      %s\n", sf_get_string(snf,SF_STR_COPYRIGHT) );
        printf("TITLE:          %s\n", sf_get_string(snf,SF_STR_TITLE) );
        printf("ARTIST:         %s\n", sf_get_string(snf,SF_STR_ARTIST) );
        printf("COMMENT:        %s\n", sf_get_string(snf,SF_STR_COMMENT) );
#endif

	/* get ALSA format */
	switch( (sinfo.format) & SF_FORMAT_SUBMASK ) {
		case SF_FORMAT_PCM_S8:
			sformat=SND_PCM_FORMAT_S8;
			depth=1;
			break;
		case SF_FORMAT_PCM_U8:
			sformat=SND_PCM_FORMAT_U8;
			depth=1;
			break;
		case SF_FORMAT_PCM_16:
			sformat=SND_PCM_FORMAT_S16;
			depth=2;
			break;
		default:
			sformat=SND_PCM_FORMAT_UNKNOWN;
	}
	if(sformat==SND_PCM_FORMAT_UNKNOWN) {
		printf("%s: Unsupported format!\n",__func__);
		sf_close(snf);
		return NULL;
	}

	/* get size of pcm data, in bytes */
	size=depth*(sinfo.channels)*(sinfo.frames);

	/* create EGI_PCMBUF */
	printf("%s: pcmbuf create with size=%ld, depth=%d ...\n",__func__, size, depth);
	pcmbuf=egi_pcmbuf_create( size, sinfo.channels, sinfo.samplerate,
	                                  sformat, SND_PCM_ACCESS_RW_INTERLEAVED );
	if(pcmbuf==NULL) {
		sf_close(snf);
		return NULL;
	}

        /* read interleaved data and load to pcmbuf */
#if 0
	printf("%s: sf_readf_short...\n",__func__);
        ret=sf_readf_short(snf, (short int *)pcmbuf->pcmbuf, sinfo.frames/(2/depth) );
	if(ret<0) {
		printf("%s: Fail to call sf_readf_short()!\n",__func__);
		sf_close(snf);
		egi_pcmbuf_free(&pcmbuf);
		return NULL;
	}
        else if( ret != sinfo.frames/(2/depth) ) {
		printf("%s: WARNING!!! sf_readf_short() return uncomplete frames!\n",__func__);
        }
	else
	 	printf("%s: ...finish sf_read_short()!\n",__func__);
#else
	printf("%s: sf_readf_raw...\n",__func__);
	ret=sf_read_raw(snf, (void *)pcmbuf->pcmbuf, (sf_count_t)(sinfo.frames*sinfo.channels*depth) ) ;
	if(ret<0) {
		printf("%s: Fail to call sf_read_raw()!\n",__func__);
		sf_close(snf);
		egi_pcmbuf_free(&pcmbuf);
		return NULL;
	}
        else if( ret != sinfo.frames*sinfo.channels*depth ) {
		printf("%s: WARNING!!! sf_read_raw() return uncomplete frames!\n",__func__);
        }
	else
	 	printf("%s: ...finish sf_read_raw()!\n",__func__);
#endif
	sf_close(snf);
	return pcmbuf;
}



/*--------------------------------------------------------------------
Playback a EGI_PCMIMG

@dev_name:	PCM device
@pcmbuf:	An EGI_PCMBUF holding pcm data.
		If pcmbuf->pcm_handle == NULL, then create/open it first, and close it after playback.
		Else if pcmbuf->pcm_handle != NULL, DO NOT close it after playback.
@vstep:		Volume step up/down, each step is 1/16 strength of original PCM value.
		So Min. value of vstep is -16.  If vstep<-16, then the sign of PCM value
		will reverse!
		vstep=0 as neutral.
		Note:
		1. For SND_PCM_FORMAT_S16 only!
		2. Shift operation is compiler depended!
		   !!! Suppose that Left/Right bit_shifting are both arithmetic here !!!
		3. Volume step_up operation may cause sound distorted !!!
		TODO: real time volume adjust, use pointer to vstep!
@nf:		frames for each write to HW.
		take a small proper value
@nloop:         loop times:
                	<=0 : loop forever
                	>0 :  nloop times

@sigstop:       Quit if Ture
                If NULL, ignore.
@sigsynch:	If true, synchronize to play from the beginning of the pcmbuf.
		and reset *sigsynch to false then.
		If nf is a big value, then it maybe nonsense!
		If NULL, ignore.
@sigtrigger	If not NULL, wait till it's True, then start playing.
		It resets to False after playing 1 loop, then wait for True again,
		until finish playing nloop or forever.
		If NULL, ignore.
Return:
	0	OK
	<0	Fails
----------------------------------------------------------------------*/
int  egi_pcmbuf_playback( const char* dev_name, EGI_PCMBUF *pcmbuf, int vstep,
			  unsigned int nf , int nloop, bool *sigstop, bool *sigsynch, bool* sigtrigger)
{
	int		i;
	int 		ret;
	int 		frames;
	unsigned int 	wf;
	unsigned char 	*pbuf=NULL;	/* pointer to pcm data position */
	int16_t  	*vbuf=NULL;	/* for modified pcm data */
	int count; 			/* for loop count */
	snd_pcm_t 	*pcm_handle=NULL;	/* Only a ref. pointer to pcmbuf->pcm_handle */
	bool		without_pcm_handle=false;  /* TURE if pcmbuf->pcm_handle is NULL! */


	/* Check sigstop before re_check in while() */
	if( sigstop!=NULL && *sigstop==true)
		return 1;

	/* check input data */
	if(pcmbuf==NULL || pcmbuf->pcmbuf==NULL) {
		printf("%s: Input pcmbuf is empty!\n",__func__);
		return -1;
	}

	/* Allocate vpbuf */
	if( vstep!=0 && pcmbuf->sformat==SND_PCM_FORMAT_S16 )  {  /* pcmbuf->depth==2, For PCM_S16 format only */
		vbuf=calloc(nf, pcmbuf->nchanl*2);
		if(vbuf==NULL) {
			printf("%s:Fail to calloc vbuf!\n",__func__);
			return -1;
		}
	}
	else if ( vstep!=0 && pcmbuf->sformat!=SND_PCM_FORMAT_S16 ) {
		EGI_PLOG(LOGLV_WARN,"%s: Soft volume adjustment only supports SND_PCM_FORMAT_S16!",__func__);
	}

	/* Open pcm device if input pcmbuf contain NULL */
	//egi_dpstd("open playback device...\n");
	if(pcmbuf->pcm_handle==NULL) {

		without_pcm_handle=true;

		pcmbuf->pcm_handle=egi_open_playback_device( dev_name, pcmbuf->sformat,     /* dev_name, sformat */
        	                                     pcmbuf->access_type, true,	    /* access_type, bool soft_resample */
						     pcmbuf->nchanl, pcmbuf->srate, /* nchanl, srate */
                        	                     50000			    /* latency (us), syssrate, */
                                	    	    );
		if(pcmbuf->pcm_handle==NULL) {
			if(vbuf!=NULL) /* free vbuf first */
				free(vbuf);
			return -2;
		}
	}
	/* Set ref. pointer */
	pcm_handle = pcmbuf->pcm_handle;

	/* Write data to PCM for PLAYBACK */
	//printf("%s: start pcm write...\n",__func__);

   	count=0; /* for nloop count */
   	do {
		/* check sigtrigger */
		if( sigtrigger != NULL)
			while( *sigtrigger == false ){usleep(1000);}; /* !!!FAINT!!! must put some codes here! { }; */

		wf=nf;	/* reset frames for each iwrite */
		frames=pcmbuf->size/pcmbuf->depth/pcmbuf->nchanl; /* Total frames */
		//pos=0;
		pbuf=pcmbuf->pcmbuf;

		/* If pcmbuf size too small */
		if(frames<nf)wf=frames;

		while( frames >0 ) //!=0 )
		{
			/* check sigstop */
			if( sigstop!=NULL && *sigstop==true)
				break;

			/* check sigsynch */
			if( sigsynch!=NULL && *sigsynch==true) {
				*sigsynch=false;
				break;
			}

			/* vstep operation for volume adjustment, !!! Suppose that Left/Right bit_shifting are both arithmetic here !!! */
			if( vstep != 0 && pcmbuf->sformat==SND_PCM_FORMAT_S16 ) {   /* !! pcmbuf->depth==2, for PCM_S16 format only */
				memcpy(vbuf, pbuf, wf*pcmbuf->nchanl*2);
				for(i=0; i < wf*pcmbuf->nchanl; i++)
					vbuf[i] += (vbuf[i]>>4)*vstep; /* whatever vstep > or < 0 */
			}
			else  /* No volume step_up/down operation, just assign pbuf to vbuf */
				vbuf=(int16_t *)pbuf;

	        	if(!pcmbuf->noninterleaved) { /* write interleaved frame data */
//				printf("%s:snd_pcm_writei()...\n",__func__);
                		ret=snd_pcm_writei( pcm_handle, (void *)vbuf, (snd_pcm_uframes_t)wf );  /* pbuf */
			}
		        else {  /* TODO: TEST,   write noninterleaved frame data */
				printf("%s: noninterleaved snd_pcm_writen()...\n",__func__);
                		//ret=snd_pcm_writen( pcm_handle, (void **)&(pcmbuf->pcmbuf), (snd_pcm_uframes_t)frames );
	        	        ret=snd_pcm_writen( pcm_handle, (void **)&vbuf, (snd_pcm_uframes_t)wf );  /* pbuf */
			}
//			printf("%s: pcm written ret=%d\n", __func__, ret);

		        if (ret == -EPIPE) {
        		    /* EPIPE means underrun */
//	        	    fprintf(stderr,"snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n");
	        	    //EGI_PDEBUG(DBG_PCM,"snd_pcm_writen() or snd_pcm_writei(): underrun occurred\n" );
		            snd_pcm_prepare(pcm_handle);
		    	    continue;
        		}
	        	else if(ret<0) {
        	    		fprintf(stderr,"snd_pcm_writei():%s\n",snd_strerror(ret));
		    		snd_pcm_close(pcm_handle);
		    		return -3;
		    		//continue;
	        	}
        		else if (ret!=wf) {
                	 	fprintf(stderr,"snd_pcm_writei(): short write, write %d of total %d frames\n",
                        	                                                        ret, wf );
			}

			/* move pbuf forward */
			frames -= ret;
			if( frames<nf && frames>0 )wf=frames; /* for the last write session */

			pbuf += ret*(pcmbuf->nchanl)*(pcmbuf->depth);
	   	} /* end while() */

	  	/* check sigstop */
	  	if( sigstop!=NULL && *sigstop==true)
			break;

		/* check sigtrigger */
		if( sigtrigger != NULL)
			*sigtrigger=false;

	  	/* loop count */
	  	count++;
  	} while ( nloop<=0 || count<nloop );  /* end loops */

	/* Close private pcm handle */
	if( without_pcm_handle ) {
		snd_pcm_drain(pcmbuf->pcm_handle);
		snd_pcm_close(pcmbuf->pcm_handle);
		pcmbuf->pcm_handle=NULL;
	}
	/* Else: If input pcmbuf contains valid pcm_handle, DO NOT close it! */

	/* Free vbuf */
	if( vstep !=0 && pcmbuf->sformat==SND_PCM_FORMAT_S16 )    //pcmbuf->depth==2   NOTE: vbuf may be referred to pbuf!
		free(vbuf);

	return 0;
}
