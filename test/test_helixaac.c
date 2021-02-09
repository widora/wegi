/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Test libhelixaac

  Helix Fixed-point HE-AAC decoder ---  www.helixcommunity.org

Midas Zhou
midaszhou@yahoo.com
-------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <aacdec.h>
#include <egi_utils.h>
#include <egi_pcm.h>
#include <egi_input.h>
#include <egi_log.h>

int main(int argc, char **argv)
{
	int err;
	HAACDecoder aacDec;
	AACFrameInfo aacFrameInfo={0};
	bool ENABLE_AAC_SBR=true; /* Special band replication */

	int bytesLeft;
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

///////////// --- TEST: RADIO --- /////////////
printf("\nAAC RADIO Player\n");
fout_path=NULL;
RADIO_LOOP:

	egi_fmap_free(&fmap_aac);
//	AACFlushCodec(aacDec);

	if(access("/tmp/a.stream",R_OK)==0)
		fin_path="/tmp/a.stream";
	else if(access("/tmp/b.stream",R_OK)==0)
		fin_path="/tmp/b.stream";
	else {
		printf("\rConnecting...  "); fflush(stdout);
		usleep(500000);
		goto RADIO_LOOP;
	}

	/* Mmap aac input file */
	fmap_aac=egi_fmap_create(fin_path, 0, PROT_READ, MAP_PRIVATE);
	if(fmap_aac==NULL) {
		printf("Fail to create fmap_aac for '%s'!\n", argv[1]);
		exit(EXIT_FAILURE);
	}
	//printf("fmap_aac for '%s' created!\n", argv[1]);

	/* Get pointer to AAC data */
	pin=(unsigned char *)fmap_aac->fp;
	bytesLeft=fmap_aac->fsize;

	/* Mmap output PCM file */
	if(fout_path) {
		fmap_pcm=egi_fmap_create(argv[2], 1024*2*sizeof(int16_t)*nchanl*2, PROT_WRITE, MAP_SHARED);
		if(fmap_pcm==NULL) {
			printf("Fail to create fmap_pcm for '%s'!\n", argv[2]);
			exit(EXIT_FAILURE);
		}
		printf("fmap_pcm with fisze=%lld for '%s' created!\n", fmap_pcm->fsize,argv[2]);
	}

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
						//AACFlushCodec(aacDec);
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
						//AACFlushCodec(aacDec);
					}
				}
				break;
		}

		/* int AACDecode(HAACDecoder hAACDecoder, unsigned char **inbuf, int *bytesLeft, short *outbuf) */
		err=AACDecode(aacDec, &pin, &bytesLeft, pout);
		if(err==0) {
			//printf("AACDecode: %lld bytes of aac data decoded.\n", fmap_aac->fsize-bytesLeft );
			AACGetLastFrameInfo(aacDec, &aacFrameInfo);


#if 0
			printf(" sampRateOut\t%d\n sampRateCore\t%d\n",
				aacFrameInfo.sampRateOut, aacFrameInfo.sampRateCore);
			printf(" profile\t%d\n tnsUsed\t%d\n pnsUsed\t%d\n",
				aacFrameInfo.profile,  aacFrameInfo.tnsUsed,  aacFrameInfo.pnsUsed );
			printf("bitRate=%d, nChans=%d, sampRateCore=%d, sampRateOut=%d, bitsPerSample=%d, outputSamps=%d\n",
							aacFrameInfo.bitRate, aacFrameInfo.nChans, aacFrameInfo.sampRateCore,
							aacFrameInfo.sampRateOut, aacFrameInfo.bitsPerSample, aacFrameInfo.outputSamps );
#endif

			/* Sep parameters for pcm_hanle, Assume format as SND_PCM_FORMAT_S16_LE, */
			if(!pcmdev_ready || nchanl != aacFrameInfo.nChans || samplerate!=aacFrameInfo.sampRateOut )
			{
				printf("\n--- Helix AAC FP Decoder ---\n");
				printf(" profile\t%s\n",
					strprofile[aacFrameInfo.profile] );
				printf(" nChans\t\t%d\n sampRateCore\t%d\n",
					aacFrameInfo.nChans, aacFrameInfo.sampRateCore );
				printf(" sampRateOut\t%d\n bitsPerSample\t%d\n outputSamps\t%d\n",
					aacFrameInfo.sampRateOut, aacFrameInfo.bitsPerSample, aacFrameInfo.outputSamps );
//				printf(" profile\t%d\n tnsUsed\t%d\n pnsUsed\t%d\n",
//					aacFrameInfo.profile,  aacFrameInfo.tnsUsed,  aacFrameInfo.pnsUsed );
				printf("----------------------------\n");

				/* Assign nchanl and samplerate */
				nchanl= aacFrameInfo.nChans;
				samplerate=aacFrameInfo.sampRateOut;

			        if( egi_pcmhnd_setParams(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, nchanl, samplerate) <0 ) {
        			        printf("Fail to set params for PCM playback handle.!\n");
	        		} else {
                			pcmdev_ready=true;
		        	}
  			}

			/* Playback */
	        	if(pcmdev_ready)
		              egi_pcmhnd_playBuff(pcm_handle, true, pout, ENABLE_AAC_SBR?2048:1024);

			printf("\r%lld/%lld",fmap_aac->fsize-bytesLeft,fmap_aac->fsize); fflush(stdout);

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
			printf("ERR_AAC_INDATA_UNDERFLOW!\n");
			continue;
		}
		else if(err==ERR_AAC_INVALID_ADTS_HEADER ) {
			/* try to synch again, trysyn_len MUST NOT be too small!!! */
			int trysyn_len=256;
			if(bytesLeft > trysyn_len) {
				printf("Try synch...\n");
				npass=AACFindSyncWord(pin, trysyn_len);
				//printf("npass=%d\n",npass);
				if(npass<=0) {  /* TODO ??? ==0 also fails!?? */
					printf("Fail to AACFindSyncWord!\n");
					pin +=trysyn_len;
					bytesLeft -=trysyn_len;
				}
				else {
					//printf("npass=%d\n", npass);
					pin +=npass;
					bytesLeft -= npass;
					//AACFlushCodec(aacDec);
					continue;
				}
			}
			else
				goto END_PROG;
		}
		else {
			printf("err=%d\n", err);
			//printf("Unrecognizable acc file!\n");
			continue;
			//exit(1);
		}
	}

	//printf("\nFinish decoding, %d bytes left!\n", bytesLeft);

///////////// --- TEST: RADIO --- /////////////
if(remove(fin_path)!=0) {
  printf("Fail to remove '%s'.\n",fin_path);
}
goto RADIO_LOOP;


END_PROG:
	/* Free source */
	AACFreeDecoder(aacDec);
	egi_fmap_free(&fmap_aac);
	egi_fmap_free(&fmap_pcm);

  	/* Close pcm handle */
  	snd_pcm_close(pcm_handle);

  	/* Reset termI/O */
  	egi_reset_termios();

	return 0;
}
