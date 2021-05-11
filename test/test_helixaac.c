/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test libhelixaac

  Helix Fixed-point HE-AAC decoder ---  www.helixcommunity.org

To make a simple m3u8 radio player:
	test_heliaac.c 	---> radio_aacdecode
	test_http.c	---> http_aac


Note:
1. Only support 2 channels NOW.


Journal:
2021-05-03:
	1. Test msg queue: send params to WetRadio.


Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
//#include <aacdec.h>
#include <aaccommon.h>
#include <egi_utils.h>
#include <egi_pcm.h>
#include <egi_input.h>
#include <egi_log.h>
#include <egi_timer.h>
#include <egi_debug.h>

#include <egi_surface.h>

int main(int argc, char **argv)
{
	int err;
	HAACDecoder aacDec;
	AACDecInfo *aacDecInfo;
	AACFrameInfo aacFrameInfo={0};
	bool enableSBR; /* Special band replication */

	int bytesLeft;
	size_t checksize;
	int nchanl=2;
	unsigned char *pin=NULL;	/* Pointer to input aac data */
	int16_t *pout;			/* MAX. for 2 channels,  assume SBR enbaled */
	size_t outsize; 		/* outsize of decoded PCM data for each aacDec session, in bytes. */
	int skip_size=1024*256; 	/* skip size */
	int findlen=1024;
	int npass;

	snd_pcm_t *pcm_handle;
	unsigned int samplerate;	/* output PCM sample rate */
	bool pcmdev_ready=false;
	EGI_FILEMMAP *fmap_aac=NULL;
	EGI_FILEMMAP *fmap_pcm=NULL;

	int percent;
	char ch;



	/***  AAC Header: profile
	 * 0. AAC Main
	 * 1. AAC LC(Low Complexity)
	 * 2. AAC SSR(Scalable Sample Rate)
	 * 3. AAC LTP(Long Term Prediction)  --- For HelixAAC, it's reserved!
	*/
	const char strprofile[][16]={ "AAC Main", "AAC LC", "AAC SSR", "AAC LTP" };

	/* input/output file */
	char *fin_path=argv[1];
	char *fout_path=argv[2];

	/* Allocate buffer for decoded PCM data */
	/* A typical raw AAC frame includes 1024 samples
  	   aac_pub/aacdec.h:		#define AAC_MAX_NSAMPS          1024
	  libhelix-aac/aaccommon.h:	#define NSAMPS_LONG		1024
         */
  	pout=(int16_t *)calloc(1, 1024*2*nchanl*sizeof(int16_t)); /* MAX. for 2 channels,  assume SBR enbaled */
  	if(pout==NULL) {
		printf("Fail to calloc pout!");
		exit(EXIT_FAILURE);
	}

#if 0   /* Start egi log */
        if(egi_init_log("/mmc/test_helixaac.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
        }
        EGI_PLOG(LOGLV_TEST,"%s: Start logging...", argv[0]);
#endif

	/* Log silent */
	egi_log_silent(true);

  	/* Set termI/O 设置终端为直接读取单个字符方式 */
  	egi_set_termios();

  	/* Prepare vol 启动系统声音调整设备 */
  	egi_getset_pcm_volume(NULL,NULL);

	/* Open pcm playback device */
  	if( snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) <0 ) {  /* SND_PCM_NONBLOCK,SND_PCM_ASYNC */
        	printf("Fail to open PCM playback device!\n");
        	exit(EXIT_FAILURE);
  	}

	/* Init AAC Decoder */
	aacDec=AACInitDecoder();
	if(aacDec==NULL) {
		printf("Fail to init aacDec!\n");
		exit(EXIT_FAILURE);
	}
	else
		printf("Succeed to init aacDec!\n");

	/* AACDecInfo */
	aacDecInfo = (AACDecInfo *)aacDec;



/* TEST: -------- MSG QUEUE ----------- */
	#include <sys/msg.h>

#define MSGQTEXT_MAXLEN 64
	int msgid;
	int msgerr;

	struct smg_data {
		long msg_type;
		char msg_text[MSGQTEXT_MAXLEN];
	} msgdata;
	EGI_CLOCK eclock={0};

	/* Get System V message queue identifier */
	key_t mqkey=ftok(ERING_PATH_SURFMAN, SURFMAN_MSGKEY_PROJID); /* proj_id MUST >0 ! */
	msgid = msgget(mqkey, 0); //IPC_CREAT|0666); /* suppose SURFMAN created it */
	if(msgid<0) {
		egi_dperr("Fail msgget()!");
//		exit(1); /* Comment it for non_surf env. */
	}
	egi_dpstd("msgget succeed!\n");

	egi_clock_start(&eclock);

/* ----end: MSG QUEUE---- */

///////////// --- TEST: RADIO --- /////////////
printf("\nAAC RADIO Player\n");
fout_path=NULL;
RADIO_LOOP:

	//egi_fmap_free(&fmap_aac);  /* Before remove ... */

	/* Flush codec before a new session */
	AACFlushCodec(aacDec); /* Not necessary? */

	if(access("/tmp/a.stream",R_OK)==0)  /* ??? F_OK: can NOT ensure the file is complete !??? */
		fin_path="/tmp/a.stream";
	else if(access("/tmp/b.stream",R_OK)==0)
		fin_path="/tmp/b.stream";
	else {
		printf("\rConnecting...  "); fflush(stdout);
		EGI_PLOG(LOGLV_INFO,"No downloaded stream file, wait...");
		usleep(500000);
		goto RADIO_LOOP;
	}

	/* Mmap aac input file */
	EGI_PLOG(LOGLV_INFO, "Start to wait for flock and create fmap_aac for '%s'!", fin_path);
	fmap_aac=egi_fmap_create(fin_path, 0, PROT_READ, MAP_SHARED);
	if(fmap_aac==NULL) {
		EGI_PLOG(LOGLV_ERROR, "Fail to create fmap_aac for '%s'!", fin_path);

	        /* 'test_http.c' MAY also access the file, Remove the file anyway, maybe 0 length. */
        	if(remove(fin_path)!=0)
                	EGI_PLOG(LOGLV_ERROR, "Fail to remove '%s'.",fin_path);
        	else
                	EGI_PLOG(LOGLV_INFO, "OK, '%s' removed!", fin_path);

		//exit(EXIT_FAILURE);
		usleep(100000);
		goto RADIO_LOOP;
	}
	EGI_PLOG(LOGLV_INFO, "Finish creating fmap_aac for '%s', fsize=%lld!", fin_path,fmap_aac->fsize);

	/* Get pointer to AAC data */
	pin=(unsigned char *)fmap_aac->fp;
	bytesLeft=fmap_aac->fsize;
	checksize=fmap_aac->fsize;

	/* Mmap output PCM file */
	if(fout_path) {
		fmap_pcm=egi_fmap_create(argv[2], 1024*2*sizeof(int16_t)*nchanl*2, PROT_WRITE, MAP_SHARED);
		if(fmap_pcm==NULL) {
			printf("Fail to create fmap_pcm for '%s'!\n", argv[2]);
			exit(EXIT_FAILURE);
		}
		printf("fmap_pcm with fisze=%lld for '%s' created!\n", fmap_pcm->fsize,argv[2]);
	}

	EGI_PLOG(LOGLV_INFO, "Start to AACDecode() fmap_aac with size=%d Bs", bytesLeft);
	while(bytesLeft>0) {
		//printf("bytesLeft=%d\n", bytesLeft);

		/* Parse keyinput */
		ch=0;
  		read(STDIN_FILENO, &ch,1);
  		switch(ch) {
		        case 'q':       /* Quit */
		        case 'Q':
                		printf("\n\n");
				goto END_PROG;
                		break;
		        case '+':       /* Volume up */
	        	        egi_adjust_pcm_volume(5);
        	        	egi_getset_pcm_volume(&percent,NULL);
		                printf("\tVol: %d%%",percent);
                		break;
			case '-':       /* Volume down */
		                egi_adjust_pcm_volume(-5);
                		egi_getset_pcm_volume(&percent,NULL);
		                printf("\tVol: %d%%",percent);
                		break;
			case '>':
				if(bytesLeft > skip_size+findlen ) {
					pin += skip_size;
					bytesLeft -= skip_size;
					npass=AACFindSyncWord(pin, findlen);
					//printf("npass=%d\n",npass);
					if(npass<0) {
						printf("Fail to AACFindSyncWord!\n");
						goto END_PROG;
					}
					else {
						pin +=npass;
						bytesLeft -= npass;
						/* AACFlushCodec(aacDec); Not necessary */
					}
				}
				break;
			case '<':
				if(fmap_aac->fsize-bytesLeft > skip_size ) {
					pin -= skip_size;
					bytesLeft += skip_size;
					npass=AACFindSyncWord(pin, findlen);
					//printf("npass=%d\n",npass);
					if(npass<0) {
						printf("Fail to AACFindSyncWord!\n");
						goto END_PROG;
					}
					else {
						pin +=npass;
						bytesLeft -= npass;
						/* AACFlushCodec(aacDec); Not necessary */
					}
				}
				break;
		}

		/* int AACDecode(HAACDecoder hAACDecoder, unsigned char **inbuf, int *bytesLeft, short *outbuf) */
		err=AACDecode(aacDec, &pin, &bytesLeft, pout);
		if(err==ERR_AAC_NONE) {
			//EGI_PLOG(LOGLV_INFO, "AACDecode: %lld bytes of aac data decoded.\n", fmap_aac->fsize-bytesLeft );
			AACGetLastFrameInfo(aacDec, &aacFrameInfo);

		/* TEST: ------------ MSG QUEUE ------------------*/
		/* END: ---- MSG QUEUE ---- */

			/* Sep parameters for pcm_hanle, Assume format as SND_PCM_FORMAT_S16_LE, */
			if(!pcmdev_ready || nchanl != aacFrameInfo.nChans || samplerate!=aacFrameInfo.sampRateOut )
			{
				printf("\n--- Helix AAC FP Decoder ---\n");
				printf(" profile\t%s\n",
					strprofile[aacFrameInfo.profile] );
				printf(" nChans\t\t%d\n sampRateCore\t%d\n",
					aacFrameInfo.nChans, aacFrameInfo.sampRateCore );
				printf(" sampRateOut\t%d\n bitsPerSample\t%d\n outputSamps\t%d\n",  /* outputSamps=nchan*frames */
					aacFrameInfo.sampRateOut, aacFrameInfo.bitsPerSample, aacFrameInfo.outputSamps );
//				printf(" profile\t%d\n tnsUsed\t%d\n pnsUsed\t%d\n",
//					aacFrameInfo.profile,  aacFrameInfo.tnsUsed,  aacFrameInfo.pnsUsed );
				printf("----------------------------\n");
				/* In AACGetLastFrameInfo():
				 * aacFrameInfo->sampRateCore =  aacDecInfo->sampRate;
		                 * aacFrameInfo->sampRateOut =   aacDecInfo->sampRate * (aacDecInfo->sbrEnabled ? 2 : 1);
                		 * aacFrameInfo->bitsPerSample = 16;
		                 * aacFrameInfo->outputSamps =   aacDecInfo->nChans * AAC_MAX_NSAMPS * (aacDecInfo->sbrEnabled ? 2 : 1);
				 * AAC file format:
	        			AAC_FF_Unknown = 0,
				        AAC_FF_ADTS = 1,
				        AAC_FF_ADIF = 2,
				        AAC_FF_RAW =  3
				 */

#if 1		/* TEST: ------------ MSG QUEUE ------------------*/
		if(msgid>=0) {
				msgdata.msg_type=SURFMSG_AAC_PARAMS;
				msgdata.msg_text[MSGQTEXT_MAXLEN-1]='\0';
				snprintf(msgdata.msg_text, MSGQTEXT_MAXLEN-1, "%s Ch=%d %dHz %s",
					strprofile[aacFrameInfo.profile], aacFrameInfo.nChans, aacFrameInfo.sampRateOut,
									aacFrameInfo.sampRateOut>aacFrameInfo.sampRateCore?"SBR":" " );
				msgerr=msgsnd(msgid, (void *)&msgdata, MSGQTEXT_MAXLEN, 0); 	/* IPC_NOWAIT */
				if(msgerr!=0) {
					printf("Faill to msgsnd!\n");
				}
				else
					printf("Msg sent out type=%ld!\n", msgdata.msg_type);
		}
		/* END: ---- MSG QUEUE ---- */
#endif

				/* Assign nchanl and samplerate */
				nchanl= aacFrameInfo.nChans;
				samplerate=aacFrameInfo.sampRateOut;

				/* Reset pcm_handle params */
				snd_pcm_drain(pcm_handle);   /* NO USE?? To drain( finish playing) remained old data, before reset params! */
		 EGI_PLOG(LOGLV_WARN, "Reset params for pcm_handle: nchanl=%d, format=%d, frameCount=%d,sampRateCore=%d, sampRateOut=%d, SBR=%s",
						nchanl, aacDecInfo->format,  aacDecInfo->frameCount,
						aacFrameInfo.sampRateCore,aacFrameInfo.sampRateOut, aacDecInfo->sbrEnabled?"Yes":"No");
			        if( egi_pcmhnd_setParams(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, nchanl, samplerate) <0 ) {
        			        EGI_PLOG(LOGLV_ERROR, "Fail to set params for PCM playback handle.!\n");
	        		} else {
                			pcmdev_ready=true;
		        	}
				/* Need to flush Codec when SBR/sampRateOut changes ?? */
				AACFlushCodec(aacDec);
				/* TODO: When  samplerate changes, it needs to call snd_pcm_prepare() ??? */
				// snd_pcm_prepare(pcm_handle); /* MUST put after setParams! otherwise it will fail setParams()! */
  			}

/*** NOTE. Log before segmentation fault:
[2021-02-16 12:40:08] [LOGLV_WARN] Reset params for pcm_handle: nchanl=2, sampRateCore=24000, sampRateOut=24000, SBR=No
[2021-02-16 12:40:08] [LOGLV_CRITICAL] SBR is disabled
[2021-02-16 12:40:08] [LOGLV_WARN] Reset params for pcm_handle: nchanl=2, sampRateCore=24000, sampRateOut=48000, SBR=Yes
[2021-02-16 12:40:08] [LOGLV_CRITICAL] SBR is enabled
*/
			/* Check SBR */
			if( enableSBR != aacDecInfo->sbrEnabled ) {
				enableSBR=aacDecInfo->sbrEnabled;
				EGI_PLOG(LOGLV_CRITICAL, "SBR is %s. frameCount=%d,  bytesDecoded: %lld/%lld",
						enableSBR?"enabled":"disabled", aacDecInfo->frameCount, fmap_aac->fsize-bytesLeft,fmap_aac->fsize);
			}

			/* Playback. 2048 OR 1024 frames.   TODO: sbrEnabled may change?? */
	        	if(pcmdev_ready) {
		        	egi_pcmhnd_playBuff(pcm_handle, true, pout, aacDecInfo->sbrEnabled?2048:1024); // ENABLE_AAC_SBR?2048:1024);
			}

			printf("\r%lld/%lld",fmap_aac->fsize-bytesLeft,fmap_aac->fsize); fflush(stdout);
			EGI_PLOG(LOGLV_INFO,"##%lld/%lld",fmap_aac->fsize-bytesLeft,fmap_aac->fsize);

			/* Save PCM data to file */
			if(fout_path) {
				/* Get output pcm data size */
				outsize=aacFrameInfo.outputSamps*sizeof(int16_t); /* outputSamps including all channels! */

				/* Copy data to fmap_pcm */
				printf("memcpy outsize=%d, fmap_pcm->off=%lld...\n", outsize, fmap_pcm->off);
				memcpy(fmap_pcm->fp+fmap_pcm->off, (void *)pout, outsize);
				fmap_pcm->off += outsize;
				printf("fmap_pcm->off=%lld\n", fmap_pcm->off);

				/* Expend fmap_pcm */
				if(egi_fmap_resize(fmap_pcm, fmap_pcm->fsize+outsize )!=0)
					printf("Fail to resize fmap_pcm!\n");
				else
					printf("fmap_pcm resize to %lld bytes!\n", fmap_pcm->fsize);
			}
		}
		else if(err==ERR_AAC_INDATA_UNDERFLOW) {
			EGI_PLOG(LOGLV_WARN, "ERR_AAC_INDATA_UNDERFLOW! bytesLeft=%d \n", bytesLeft);
			if(fout_path)
				egi_fmap_free(&fmap_pcm);
			goto END_SESSION;
			//goto RADIO_LOOP;
			//continue;
		}
		else if(err==ERR_AAC_INVALID_ADTS_HEADER ) {
			int trysyn_len=256;

			EGI_PLOG(LOGLV_WARN, "ERR_AAC_INVALID_ADTS_HEADER! bytesLeft=%d, trysyn_len=%d \n", bytesLeft, trysyn_len);

			/* TODO: try to synch again, trysyn_len MUST NOT be too small!!! */

			/* Flush codec */
			AACFlushCodec(aacDec);

			if(bytesLeft > trysyn_len) {
				EGI_PLOG(LOGLV_INFO, "Try synch...\n");
				npass=AACFindSyncWord(pin, trysyn_len);
				//printf("npass=%d\n",npass);
				if(npass <= 0) {  /* TODO ??? ==0 also fails!?? */
					EGI_PLOG(LOGLV_CRITICAL,"Fail to AACFindSyncWord!\n");
					pin +=trysyn_len;
					bytesLeft -=trysyn_len;
					continue;
				}
				else {
					EGI_PLOG(LOGLV_CRITICAL,"Succeed to AACFindSyncWord! npass=%d\n", npass);
					//printf("npass=%d\n", npass);
					pin +=npass;
					bytesLeft -= npass;
					continue;
				}
			}
			else {
				goto END_SESSION;
			}
		}
		else {
			EGI_PLOG(LOGLV_WARN, "AAC ERR=%d, ", err);
			break; /* Break while()! or it may do while() forever as bytesLeft may not be changed! */
			/* Break and start a new session */
		}
	}

	//printf("\nFinish decoding, %d bytes left!\n", bytesLeft);

///////////// --- TEST: RADIO --- /////////////

END_SESSION:  /* End current radio aac seq file session */
	/* Free fmap_aac and close its file before remove() operation!??? */
	if( (err=egi_fmap_free(&fmap_aac)) !=0) {
		EGI_PLOG(LOGLV_ERROR, "Fail to free fmap_aac! err=%d.", err);
	}

	/* TEST: Re_check filesize again, if first mmap get wrong filesize! */
	/* Cause  */
	if(fin_path!=NULL) {
		int fd;
		struct stat sb;

	        /* Open file */
        	fd=open(fin_path, O_RDONLY);
		if(fd<0) {
			EGI_PLOG(LOGLV_ERROR, "Fail to open '%s'.", fin_path);
		}
		else {
		        /* Obtain file stat */
        		if( fstat(fd, &sb)<0 ) {
                		EGI_PLOG(LOGLV_ERROR, "Fail call fstat for file '%s'. Err'%s'.",fin_path, strerror(errno));
        		}
			else {
				/* Check size */
				if( checksize != sb.st_size )
					EGI_PLOG(LOGLV_ERROR, "Recheck fsize fails! checksize=%d, sb.st_size=%llu",
												checksize, sb.st_size);
			}

			/* Close file */
			close(fd);
		}
	}

	/* Remove file */
	if(remove(fin_path)!=0)
		EGI_PLOG(LOGLV_ERROR, "Fail to remove '%s'.",fin_path);
	else
		EGI_PLOG(LOGLV_INFO, "OK, '%s' removed!", fin_path);


  	goto RADIO_LOOP;


END_PROG:
	EGI_PLOG(LOGLV_CRITICAL,"--- ENG_PROG ---");
	/* Free source */
	AACFreeDecoder(aacDec);
	egi_fmap_free(&fmap_aac);
	if(fout_path)
		egi_fmap_free(&fmap_pcm);

  	/* Close pcm handle */
  	snd_pcm_close(pcm_handle);

  	/* Reset termI/O */
  	egi_reset_termios();

	return 0;
}
