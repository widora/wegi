/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test libhelixaac

  Helix Fixed-point HE-AAC decoder ---  www.helixcommunity.org

To make a simple m3u8 radio player:
	test_heliaac.c 	---> radio_aacdecode
	test_http.c	---> http_aac
	test_decodeh264.c ---> h264_decode

Note:
1. ONLY support profile of AAC LC(Low Complexity).
2. Only support 2 channels NOW.
3. Non_support:
 XXX Profile 'main' or 'SSR' profile, LTP(Long Term Prediction)
 XXX coupling channel elements (CCE)
 XXX 960/1920-sample frame size
 XXX low-power mode SBR
 XXX downsampled (single-rate) SBR
 XXX parametric stereo

TODO:
1. To apply FIFO ring buffer.

Journal:
2021-05-03:
	1. Test msg queue: send params to WetRadio.
2022-07-29:
	1. 7.6 Case: ERR_AAC_INVALID_ADTS_HEADER, passing ADTS_Header(7Bs)
	   before try to sync again.
2022-09-26:
	1. Check frameLength with bytesLeft, to avoid segmentation fault.
	TODO: This maybe ADTS data error, check in egi_extract_AV_from_ts().

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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
	int trysyn_len=256;

	int bytesLeft;
	int frameLength;
	size_t checksize;
	int nchanl=2;
	unsigned char *pin=NULL;	/* Pointer to input aac data */
	int16_t *pout;			/* MAX. for 2 channels,  assume SBR enbaled */
	size_t outsize; 		/* outsize of decoded PCM data for each aacDec session, in bytes. */
	int skip_size=1024*64; //256; 	/* skip size */
	int findlen=1024;
	int npass;

	snd_pcm_t *pcm_handle;
	unsigned int samplerate;	/* output PCM sample rate */
	bool pcmdev_ready=false;
	EGI_FILEMMAP *fmap_aac=NULL;
	EGI_FILEMMAP *fmap_pcm=NULL;

//	unsigned char tailBuffer[2*2*NSAMPS_SHORT];  /* nchanl*2*NSAMPS_SHORT, for the remaining sample data. */

	int percent;
	char ch;
	int segFileCnt=0;  /* Counter for  segment files */
	int feedCnt=0;     /* Counter for decoder feeding/decoding */
	const char *title=NULL;  /* Title / Station name */
	if(argc>1) title=argv[1];

	/***  AAC Header: profile
	 * 0. AAC Main  --- NOT SUPPORTED!
	 * 1. AAC LC(Low Complexity)
	 * 2. AAC SSR(Scalable Sample Rate) --- NOT SUPPORTED!
	 * 3. AAC LTP(Long Term Prediction)  --- For HelixAAC, it's reserved!
	*/
	const char strprofile[][16]={ "AAC Main", "AAC LC", "AAC SSR", "AAC LTP" };

	/* input/output file */
	char *fin_path=argv[1];
	char *fout_path=argv[2];
	char *fin_path2;

	/* Allocate buffer for decoded PCM data */
	/* A typical raw AAC frame includes 1024 samples
  	   aac_pub/aacdec.h:		#define AAC_MAX_NSAMPS          1024
	  libhelix-aac/aaccommon.h:	#define NSAMPS_LONG		1024
	  libhelix-aac/aaccommon.h:	#define NSAMPS_SHORT		128
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
//	egi_log_silent(true);

  	/* Set termI/O 设置终端为直接读取单个字符方式 */
  	egi_set_termios();

  	/* Prepare vol 启动系统声音调整设备 */
  	egi_getset_pcm_volume(NULL,NULL,NULL);

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
fout_path=NULL;
RADIO_LOOP:

	//egi_fmap_free(&fmap_aac);  /* Before remove ... */

	/* 1. Flush codec before a new session */
	AACFlushCodec(aacDec); /* Not necessary? */

	/* 2. Check buffer file */
	if(access("/tmp/a.aac",R_OK)==0) { /* ??? F_OK: can NOT ensure the file is complete !??? */
		fin_path="/tmp/a.aac"; /* If AAC only */
		fin_path2="/tmp/a.ts";    /* If *.ts segment */

                /* Rename a._h264 to a.h264, as sync to h264_decode */
                if(access("/tmp/a._h264",F_OK)==0 && access("/tmp/a.h264",F_OK)!=0) /* HK2022-08-31 */
                	rename("/tmp/a._h264", "/tmp/a.h264"); //rename() will atuo. replace if already exists!!!
	}
	else if(access("/tmp/b.aac",R_OK)==0) {
		fin_path="/tmp/b.aac";
		fin_path2="/tmp/b.ts";

                /* Rename b._h264 to b.h264, as sync to h264_decode */
                if(access("/tmp/b._h264",F_OK)==0  && access("/tmp/b.h264",F_OK)!=0) /* HK2022-08-31 */
                	rename("/tmp/b._h264", "/tmp/b.h264");
	}
	else {
		printf("\rConnecting...  "); fflush(stdout);
		EGI_PLOG(LOGLV_INFO,"No downloaded stream file, wait...");
		usleep(500000);
		goto RADIO_LOOP;
	}

	/* 3. Mmap aac input file */
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

	/* 4. Count segment files */
	segFileCnt++;

	/* 5. Get pointer to AAC data */
	pin=(unsigned char *)fmap_aac->fp;
	bytesLeft=fmap_aac->fsize;
	checksize=fmap_aac->fsize;

	/* 6. Mmap output PCM file */
	if(fout_path) {
		fmap_pcm=egi_fmap_create(argv[2], 1024*2*sizeof(int16_t)*nchanl*2, PROT_WRITE, MAP_SHARED);
		if(fmap_pcm==NULL) {
			EGI_PLOG(LOGLV_INFO, "Fail to create fmap_pcm for '%s'!", argv[2]);
			exit(EXIT_FAILURE);
		}
		printf("fmap_pcm with fisze=%lld for '%s' created!\n", fmap_pcm->fsize,argv[2]);
	}

	EGI_PLOG(LOGLV_INFO, "Start to AACDecode() fmap_aac with size=%d Bs", bytesLeft);

	/* 7. Loop decoding */
	feedCnt=0;
	while(bytesLeft>= nchanl*2*NSAMPS_SHORT) {  /* 2*128 Too short cause decoder dump segfault! NSAMPS_SHORT=128 */
		//printf("bytesLeft=%d\n", bytesLeft);

		/* 7.1 Parse keyinput */
		ch=0;
  		read(STDIN_FILENO, &ch,1);
  		switch(ch) {
		        case 'q':       /* Quit */
		        case 'Q':
                		printf("\n\n");
				goto END_PROG;
                		break;
		        case '+':       /* Volume up */
	        	        egi_adjust_pcm_volume(NULL, 5);
        	        	egi_getset_pcm_volume(NULL, &percent,NULL);
		                printf("\rVol: %d%%",percent); fflush(stdout);
                		break;
			case '-':       /* Volume down */
		                egi_adjust_pcm_volume(NULL, -5);
                		egi_getset_pcm_volume(NULL, &percent,NULL);
		                printf("\rVol: %d%%",percent); fflush(stdout);
                		break;
			case '>':
				if(bytesLeft > skip_size+findlen ) {
					printf("---->\n");
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
				else
					printf("XXX ---->, skip_size=%d, findlen=%d \n", skip_size, findlen );
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
						printf("<----\n");
					}
				}
				break;
		}


#if 0 /////////////////////  TEST : Pravite Info before Syncword  /////////////////////////
		uint8_t seqmark=0;
		int k;
		bool seqmark_error=false;

		npass=AACFindSyncWord(pin, trysyn_len);
		if(npass <0) {  /* TODO ??? ==0 also fails!?? */
			//EGI_PLOG(LOGLV_CRITICAL,"Fail to AACFindSyncWord, npass=%d!\n", npass);
			//pin +=trysyn_len;
			//bytesLeft -=trysyn_len;
			EGI_PLOG(LOGLV_CRITICAL,"PRE_AACFindSyncWord fails, npass=%d, bytesLeft=%d!\n",
				npass, bytesLeft);
			break;
		}
		else if( npass>=0 ) { //&& npass==8 ) {
			//EGI_PLOG(LOGLV_CRITICAL,"Succeed to AACFindSyncWord! npass=%d\n", npass);
			printf("npass=%d\n", npass);

			/* Leading 8Bytes what?
			 * Example:  84 80 05 2B 63 D1 5D C1
			 */
			printf("Leading bytes: ");
			for(k=0; k<npass; k++) {
				printf("%02X ", pin[k]);
			}
			printf("\n");

  #if 0 ///////////////////
			/* Check sequemark */
			if(seqmark==0)
				seqmark=pin[5];
			else if( seqmark!=pin[5] ) {
				seqmark +=2;
				if( seqmark+2 != pin[5] ) {
					printf("seqmark error!...................\n");
					seqmark_error=true;
				}
			}
  #endif ///////////////////

			printf("seqmark: 0x%02X\n", seqmark);

			/* Pass private bytes */
			pin +=npass; //NO change
			bytesLeft -= npass;

			/* NOW: pin pointers to the beginning(syncwords) of an aac frame */

		       /* Fixed header(28bits) + Var Header(28bits) = 7Bytes
			  Byte[bits]:  0[0:7]  1[8:15]  2[16:23]  3[24:31]  4[32:39]  5[40:47]  6[48:56] */

		        /* Get channel_configuration bits[23 : 25] bslbf: (msb)Left bit first
			   1: 1 channel: front-center，
			   2: 2 channels: front-left, front-right，
			   3: 3 channels: front-center, front-left, front-right，
			   4: 4 channels: front-center, front-left, front-right, back-center
			*/
			unsigned int chanConfig= ((pin[2]&0b1)<<3) + (pin[3]>>6);
			printf("chanConfig=%u \n", chanConfig);

			/* Get AAC frame length bits[30 : 42]    bslbf: (msb)Left bit first
			 * frameLength MAX. =188-4=184, when TS option_field =0byte
			 */
			unsigned int frameLength= ( (pin[3]&0b11)<<11 ) + (pin[4]<<3) + (pin[5]>>5);
			printf("frameLen=%u \n", frameLength);

			 /* TEST: print aac_frame_data + next_8bytes --------- */
			printf("pin[0]:%02X,pin[1]:%02X, pin[2]:%02X,pin[3]:%02X,\n", pin[0], pin[1], pin[2], pin[3]);
			printf("pin[4]:%02X,pin[5]:%02X, pin[6]:%02X \n", pin[4], pin[5], pin[6]);
			//if(bytesLeft==5607-8) {
			{
				int i;
				//for(i=0; i<bytesLeft; i++) {
				for(i=0; i<frameLength +8; i++) {
					printf("%02X ", pin[i]);
				}
				printf("\n");
			}
			//printf("seqmark: 0x%02X\n", seqmark);

#if 0			/* Check framelength ---- NOPE!	 'FFF' may appear in aac data  */
			int nxpass=AACFindSyncWord(pin+7, trysyn_len);
			if(nxpass < frameLength-7 +8) {
				printf(" ---- frameLength Error ----\n");
				/* Get to next syncword */
				pin += 7+nxpass;	/* 7Bytes header */
				bytesLeft -= 7+nxpass;
				exit(1);
			}

#endif
#if 0			/* Skip if seqmark_error, pass this frame. */
			if(seqmark_error) {
				pin += frameLength;
				bytesLeft -= frameLength;
				seqmark=0;
				AACFlushCodec(aacDec);
			}
#endif


		}
#endif  //////////////////////  END TEST  ////////////////////////////////

		/* 7.2 Check frameLength  HK2022-09-26 */
		frameLength= ( (pin[3]&0b11)<<11 ) + (pin[4]<<3) + (pin[5]>>5);
		if(frameLength>bytesLeft) {
			printf(DBG_RED"ERROR! frameLength(%d) > bytesLeft(%d)!\n"DBG_RESET, frameLength, bytesLeft);
			goto END_SESSION;
		}

		/* 7.3  int AACDecode(HAACDecoder hAACDecoder, unsigned char **inbuf, int *bytesLeft, short *outbuf) */
//		AACDecInfo *aacDecInfo = (AACDecInfo *)aacDec;
//		printf("AAC format: %d\n", aacDecInfo->format); /* 0=Unknown, 1=AAC_FF_ADTS, 2=AAC_FF_ADIF, 3=AAC_FF_RAW */

		printf("AACDecode bytesLeft=%d.\n", bytesLeft);
		err=AACDecode(aacDec, &pin, &bytesLeft, pout);
		printf("err=%d, bytesLeft=%d.\n", err, bytesLeft);
		feedCnt++;

		/* 7.4 Case: ERR_AAC_NONE */
		if(err==ERR_AAC_NONE) {
			//EGI_PLOG(LOGLV_INFO, "AACDecode: %lld bytes of aac data decoded.\n", fmap_aac->fsize-bytesLeft );
			AACGetLastFrameInfo(aacDec, &aacFrameInfo);

			/* Unsupported profile */
			if(aacFrameInfo.profile != 1 && aacFrameInfo.profile<4 ) {
				printf("Profile '%s' NOT supported, it ONLY supports '%s' NOW!\n",
					strprofile[aacFrameInfo.profile], strprofile[1]);
				exit(1);
			}

		/* TEST: ------------ MSG QUEUE ------------------*/
		/* END: ---- MSG QUEUE ---- */

			/* Sep parameters for pcm_hanle, Assume format as SND_PCM_FORMAT_S16_LE, */
			if(!pcmdev_ready || nchanl != aacFrameInfo.nChans || samplerate!=aacFrameInfo.sampRateOut )
			{
				printf("\n   %s\n", title);
   			        printf(" ----------------------------\n");
                		printf("       AAC Radio Player\n");
				//printf("     Helic AAC FP Decoder\n");
		                printf(" ----------------------------\n");
				printf(" Helix AAC FP Decoder(fmt=%d)\n", aacDecInfo->format);
				printf(" profile\t%s\n",
					strprofile[aacFrameInfo.profile]);
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
				else {
					//printf("Msg sent out type=%ld!\n", msgdata.msg_type);
				}
		}
		/* END: ---- MSG QUEUE ---- */
#endif

				/* Assign nchanl and samplerate */
				nchanl= aacFrameInfo.nChans;
				samplerate=aacFrameInfo.sampRateOut;

				/* Reset pcm_handle params */
//				snd_pcm_drain(pcm_handle);   /* NO USE?? To drain( finish playing) remained old data, before reset params! */
		 EGI_PLOG(LOGLV_WARN, "Reset params for pcm_handle: nchanl=%d, format=%d, frameCount=%d,sampRateCore=%d, sampRateOut=%d, SBR=%s",
						nchanl, aacDecInfo->format,  aacDecInfo->frameCount,
						aacFrameInfo.sampRateCore,aacFrameInfo.sampRateOut, aacDecInfo->sbrEnabled?"Yes":"No");
			        if( egi_pcmhnd_setParams(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, nchanl, samplerate) <0 ) {
        			        EGI_PLOG(LOGLV_ERROR, "Fail to set params for PCM playback handle.!\n");
	        		} else {
                			pcmdev_ready=true;
		        	}

				/* Need to flush Codec when SBR/sampRateOut changes ?? */
//				AACFlushCodec(aacDec);

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

			if( (feedCnt&(4-1)) ==0 )
				printf("\rPlaying [seg%d] %lld/%lld", segFileCnt, fmap_aac->fsize-bytesLeft,fmap_aac->fsize); fflush(stdout);

//			EGI_PLOG(LOGLV_INFO,"##%lld/%lld",fmap_aac->fsize-bytesLeft,fmap_aac->fsize);

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
		/* 7.5 Case: ERR_AAC_INDATA_UNDERFLOW */
		else if(err==ERR_AAC_INDATA_UNDERFLOW) {
			EGI_PLOG(LOGLV_WARN, "ERR_AAC_INDATA_UNDERFLOW! bytesLeft=%d \n", bytesLeft);

			#if 0
			if(fout_path)
				egi_fmap_free(&fmap_pcm);
			goto END_SESSION;
			#endif

                        pin +=1;
                        bytesLeft -=1;

			/* Flush codec */
			AACFlushCodec(aacDec);

			continue;
		}
		/* 7.6 Case: ERR_AAC_INVALID_ADTS_HEADER */
		else if(err==ERR_AAC_INVALID_ADTS_HEADER ) {

			EGI_PLOG(LOGLV_WARN, "ERR_AAC_INVALID_ADTS_HEADER! bytesLeft=%d, trysyn_len=%d \n", bytesLeft, trysyn_len);

#if 1			/* Try to synch again, trysyn_len MUST NOT be too small!!!
			 *  Re_synch is NO use if the data format is unsupported.
			 */

			/* Flush codec */
			AACFlushCodec(aacDec);

			if(bytesLeft > trysyn_len) {
				EGI_PLOG(LOGLV_INFO, "Try synch...\n");
				npass=AACFindSyncWord(pin+7, trysyn_len);  /* Skip ADTS_Header(7Bs)...HK2022_07_29 */
				if(npass <= 0) {  /* TODO ??? ==0 also fails, npass==0 will trap into dead looping. */
					EGI_PLOG(LOGLV_CRITICAL,"Fail to AACFindSyncWord, npass=%d!\n", npass);
					pin +=trysyn_len+7;   /* ADTS_Header(7Bs) */
					bytesLeft -=trysyn_len+7;
					EGI_PLOG(LOGLV_CRITICAL,"Fail to AACFindSyncWord, npass=%d, bytesLeft=%d!\n",
								npass, bytesLeft);

					/* Flush codec */
//					AACFlushCodec(aacDec);

					/* HK2022-07-20: If continues, AACDecode() will throw out segmentation fault! */
					break; //continue;
				}
				else {
					EGI_PLOG(LOGLV_CRITICAL,"Succeed to AACFindSyncWord! npass=%d\n", npass);
					//printf("npass=%d\n", npass);
					pin +=npass+7;   /* ADTS_Header(7Bs) */
					bytesLeft -= npass+7;

					/* Flush codec */
//					AACFlushCodec(aacDec);

					continue;
				}
			}
			else {
				goto END_SESSION;
			}
#endif /* END try sync */

		}
		/* 7.7 Case: Other errors */
		else {
			EGI_PLOG(LOGLV_WARN, "AAC ERR=%d, bytesLeft=%d", err,bytesLeft);
			break; /* Break while()! or it may do while() forever as bytesLeft may not be changed! */
			/* Break and start a new session */
		}

	} /* While() */

//	printf("\nFinish decoding, %d bytes left!\n", bytesLeft);

///////////// --- TEST: RADIO --- /////////////

END_SESSION:  /* End current radio aac seq file session */

	/* 8. Free fmap_aac and close its file before remove() operation!??? */
	if( (err=egi_fmap_free(&fmap_aac)) !=0) {
		EGI_PLOG(LOGLV_ERROR, "Fail to free fmap_aac! err=%d.", err);
	}

	/* 9. TEST: Re_check filesize again, if first mmap get wrong filesize! */
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

	/* 10. Remove buffer files: a.aac/b.aac, a.ts/b.ts */
	if(remove(fin_path)!=0)
		EGI_PLOG(LOGLV_ERROR, "Fail to remove '%s'.",fin_path);
	else
		EGI_PLOG(LOGLV_INFO, "OK, '%s' removed!", fin_path);

	if(remove(fin_path2)!=0)
		EGI_PLOG(LOGLV_ERROR, "Fail to remove '%s'.",fin_path2);
	else
		EGI_PLOG(LOGLV_INFO, "OK, '%s' removed!", fin_path2);


  	goto RADIO_LOOP;


END_PROG:
	EGI_PLOG(LOGLV_CRITICAL,"--- ENG_PROG ---");
	/* 11. Free source */
	AACFreeDecoder(aacDec);
	egi_fmap_free(&fmap_aac);
	if(fout_path)
		egi_fmap_free(&fmap_pcm);

  	/* 12. Close pcm handle */
  	snd_pcm_close(pcm_handle);

  	/* 13. Reset termI/O */
  	egi_reset_termios();

	return 0;
}
