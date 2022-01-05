/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Usage:
	./test_http2 -m -s 60 -l http(s)://......m3u8

Refrence:
1. RFC_8216(2017)     HTTP Live Streaming
2. All download segments are saved to vall.mp4, and will be removed
   when starts for a new session.
3. vall.mp4 will be converted to vall.avi by FFmpeg, and it will be
   truncated to zero by NEXT ffmpeg converting operation.

TODO:
1. For some m3u8 address, it sometimes exits accidently when calling
   https_easy_download().

Journal:
2021-12-18:
	Create the file by copying test_http.c
2021-12-20:
	1. parse_m3u8list(): to download and play segment vides.
2021-12-24:
	1. Redirect if the first m3u8list item is m3u8 URL, NOT a media segemnt URL.
2021-12-31:
	1. Substitue 'cstr_parse_simple_HLS()' with 'm3u_parse_simple_HLS()'
2022-01-01:
	1. Download and decrypt M3U8ENCRYPT_AES_128 segments.
2022-01-02:
	1. Options for user to set recordTime etc.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <egi_https.h>
#include <egi_timer.h>
#include <egi_debug.h>
#include <egi_log.h>
#include <egi_utils.h>
#include <egi_cstring.h>
#include <egi_procman.h>
#include <egi_aes.h>


size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp);
void parse_m3u8list(char *strm3u);

char strRequest[EGI_URL_MAX];
char *dirURL;  /* The last dirURL */
char *m3uURL;
char buff[CURL_RETDATA_BUFF_SIZE];

char strcmd[512];
int brightness;

char latest_msURI[EGI_URL_MAX];  /* The latest segment URI */
int  latest_Seqnum;		/* The latest segment sequence number */
bool loop_ON;
int loopcnt;
int dscnt;	   /* Downloaded segments counter */
float durations;   /* Duration for all download segments, in seconds. */
int recordTime;    /* User preset record time, in seconds. if durations>=recordTime, then stop downloading. */
bool saveToMMC;
bool saveAVI;
int playRounds=1;

void print_help(const char *name)
{
        printf("Usage: %s [-hlmas:r:b:] URL\n", name);
        printf("-h    This help \n");
        printf("-l    Loop downloading and playing\n");
	printf("-m    To save to mmc \n");
	printf("-a    Save .avi video. otherwise remove it before next conversion operation.\n");
	printf("-s n  Record time, in seconds\n");
	printf("-r n  Repeat playing rounds, default 1. If 0, skip playing.\n");
	printf("-b n  Adjust brightness for mplayer, default 0\n");
}

/*---------------------------
	    MAIN
----------------------------*/
int main(int argc, char **argv)
{
        int opt;
        while( (opt=getopt(argc,argv,"hlmas:r:b:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                print_help(argv[0]);
                                exit(0);
                                break;
			case 'l':
				loop_ON=true;
				break;
			case 'm':
				saveToMMC=true;
				break;
			case 'a':
				saveAVI=true;
				break;
                        case 's':
                                recordTime=atoi(optarg);
                                break;
			case 'r':
				playRounds=atoi(optarg);
				if(playRounds<0) playRounds=0;
				break;
			case 'b':
				brightness=atoi(optarg);
				break;
		}
	}
	if(argv[optind]==NULL) {
		printf("Please provide URL for m3u8!\n");
		exit(0);
	}

#if 0	/* Start egi log */
  	if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  	}
	EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

	/* For http,  conflict with curl?? */
	printf("start egitick...\n");
	tm_start_egitick();

	/* prepare GET request string */
        memset(strRequest,0,sizeof(strRequest));
        strcat(strRequest, argv[optind]);
        EGI_PLOG(LOGLV_CRITICAL,"Http request head:\n %s\n", strRequest);

#if 0 /* TEST: ------------------ */
	/* Download media segment files */
	if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
				 3, 20,   /* A rather small value, near to targetduration */
               			 strRequest, "/tmp/test.dat", NULL, NULL) ==0 )  /* Use default callback write func. */
        {
		printf("Ok, succeed to download from '%s', and save to '/tmp/test.dat'!\n", strRequest);
	}
	else
		printf("Fail to download segment from '%s'!\n", strRequest);

	exit(0);
#endif /* TEST: END */

	loopcnt=0;
  while(1) {
	/* m3u URL */
	m3uURL = strRequest;

	/* Get the last subdir URL */
	/* (myURL, protocol, host, port, path, filename, dir,dirURL) */
	cstr_parse_URL(m3uURL, NULL, NULL, NULL, NULL, NULL, NULL, &dirURL);

	/* 1. Reset buff */
	//memset(buff, 0, sizeof(buff));
	buff[0]=0; /* OK??? */

	/* 2. Https GET request */
	EGI_PLOG(LOGLV_INFO,"Start https curl request...");
	system("free");
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_ENABLE_REDIRECT,
				5,30, strRequest, buff, NULL, curl_callback) !=0 )
	{
		EGI_PLOG(LOGLV_ERROR, "Fail to call https_curl_request()! try again...");
		/* Try again */
		sleep(1);
		continue;
		//EGI_PLOG(LOGLV_ERROR, "Try https_curl_request() again...");
		//exit(EXIT_FAILURE);
	}
        printf("        --- Http GET Reply ---\n %s\n",buff);
	EGI_PLOG(LOGLV_INFO,"Http curl get: %s\n", buff);


	/* 3. Parse content */
	EGI_PLOG(LOGLV_INFO,"Start parse m3u8list...\n");
	parse_m3u8list(buff);

	/* 4. Check loop */
	if( (!loop_ON) && loopcnt>0 )
		exit(0);

	/* Sleep of TARGETDURATION */
//	sleep(7/2);

  }

	exit(EXIT_SUCCESS);
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        int blen=strlen(buff); // strlen((char*)userp)
        int plen=strlen(ptr);
        int xlen=plen+blen;

        /* MUST check buff[] capacity! */
        if( xlen > sizeof(buff)-1) {   // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
                /* "If src contains n or more bytes, strncat() writes n+1 bytes to dest
                 * (n from src plus the terminating null byte." ---man strncat
                 */
		egi_dperr(DBG_RED"strlen(buff)=%d, strlen(buff)+strlen(ptr)=%d >=sizeof(buff)-1!\n"DBG_RESET,
			   blen,xlen);

                //exit(0);  /* YES! */

                strncat(userp, ptr, sizeof(buff)-1-blen); /* 1 for '\0' by strncat */
                //strncat(buff, ptr, sizeof(buff)-1);

	    	EGI_PLOG(LOGLV_TEST, "%dBytes data strncat to userp!", sizeof(buff)-1-blen);
        }
        else {
            #if 1 /* strcat */
                strcat(userp, ptr);
                //buff[sizeof(buff)-1]='\0';  /* <---------- return data maybe error!!??  */
	    	EGI_PLOG(LOGLV_TEST, "%dBytes data strcat to userp!", plen);
            #else /* memcpy */
                memcpy(userp, ptr, size*nmemb); // NOPE for compressed data?!!!
                userp[size*nmemb]='\0';
            #endif

        }

        return size*nmemb; /* Return size*nmemb OR curl will fails! */
}


/*-----------------------------------
Parse m3u8 playlist(HLS playlist).

Note:
1. Content in strm3u will be changed!
-----------------------------------*/
void parse_m3u8list(char *strm3u)
{
	int k;
	EGI_M3U8_LIST *list;
	char strURL[EGI_URL_MAX]; /* For msURI[], media segment URL */
	#define MAX_PLAYSEQ  12 /* To play Max. number of segements */
//	char *sfname[8];  	/* segment file names */

	/* 1. Parse HLS file, and get list of URL for media segments. */
	list=m3u_parse_simple_HLS(strm3u);
	if(list==NULL) {
		printf("Fail to parse simple HLS!\n");
		return;
	}
	else
		printf("Totally %d media segments included!\n", list->ns);

	/* 2. Check if the list is STILL a m3u8 URI, then LOOP BACK to main to parse again
	 *   TODO: To select with bandwith etc. NOW take the first item.
	 */
	if(list->isMasterList) {
		printf("It's a master playlist!\n");
	   	for(k=0; k < list->ns; k++) {
			printf("list[%d]:\nmsURI: %s\n", k, list->msURI[k]);
	   	}

                /* Assemble strRequest for the new m3u8 file. */
                strRequest[0]=0;
                strcat(strRequest, dirURL);
                strcat(strRequest, list->msURI[0]);  /* Select the first playlist */
                printf("Redirected strRequest: %s\n", strRequest);

/* Return back <------------- */
		m3u_free_list(&list);
		return;
	}

	/* NOW: it a media segment list! */
	printf("Max. segment duration: %fs\n", list->maxDuration);
   	for(k=0; k < list->ns; k++) {
		printf("---list[%d]:\nduration: %f\nmsURI: %s\nencrypt: %d\nkeyURI: %s\nIV: %s\n",
			k, list->tsec[k], list->msURI[k], list->encrypt[k], list->keyURI[k], list->strIV[k]);
   	}


#if 0 /* Loop TEST: -------------- */
	if(list->maxDuration>0.0) {
		printf("Sleep....\n");
		sleep((int)(list->maxDuration/3));
	}


/* Return back <------------- */
	m3u_free_list(&list);
	return;
#endif


#if 1 /* Download all segments in the playlist  */
	/* 2A. Init.  durations/dscnt will be reset at 6. */
	if( durations<0.001 ) {
		if(saveToMMC)
		    remove("/mmc/vall.mp4");
		else
		    remove("/tmp/vall.mp4");
	}

	/* 3. Download segment files */
	for(k=0; k< list->ns; k++) {

	   /* Check if msURI is already downloaded */
	   /* NOTE: list seqnum==0 means restart of ALL numbering! */
	   if( (list->seqnum!=0) && (latest_Seqnum >= list->seqnum+k) ) {
		egi_dpstd(DBG_RED"  xxxxxxxx list_seqnum=%d+(%d) <= lastest_seqnum=%d, obsolete msURI! xxxxxx\n"DBG_RESET,
			 list->seqnum,k,latest_Seqnum );
		egi_dpstd(DBG_BLUE" latest msURI: %s\n"DBG_RESET, latest_msURI);
		sleep((int)(list->maxDuration/1.5));
		continue;
	   }
	   else /* Update laste_msURI AND latest_Seqnum */ {
		if( strlen(list->msURI[k]) > EGI_URL_MAX-1 )
			egi_dpstd(DBG_RED"strlen of msURI >EGI_URL_MAX -1\n"DBG_RESET);
		strncpy(latest_msURI, list->msURI[k], EGI_URL_MAX-1);
		latest_msURI[EGI_URL_MAX-1]=0;

		/* Update latest_Seqnum */
		latest_Seqnum = list->seqnum+k;
	   }

	    /* Assemble URL for the media segement file */
	    strURL[0]=0;
	    if( cstr_is_absoluteURL(list->msURI[k])==false )
		strcat(strURL, dirURL);
	    strcat(strURL, list->msURI[k]);
	    printf("strURL: %s\n", strURL);

	    /* F1. NOT encrypted:  Download media segment files to one file! */
	    if(list->encrypt[k]==M3U8ENCRYPT_NONE) {
		if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_DOWNLOAD_SAVE_APPEND,
					 1, 10,   /* A rather small value, near to targetduration */
                			 //strURL, tsSavePath[k], NULL, download_callback) ==0 )
                			 strURL, saveToMMC?"/mmc/vall.mp4":"/tmp/vall.mp4", NULL, NULL) ==0 )  /* Use default callback write func. */
                {
			printf("Ok, succeed to download segment from '%s'!\n", strURL);
			/* Update dscnt/durations */
			dscnt++;
			durations +=list->tsec[k];
		}
		else
			printf("Fail to download segment from '%s'!\n", strURL);
	    }
	    /* F2. M3U8ENCRYPT_AES_128: Download media segment and decrypt it, then merge to a file. */
	    else if(list->encrypt[k]==M3U8ENCRYPT_AES_128) {
		/* F2.1  Download meida segment file */
		if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
					 1, 60,   /* A rather small value, near to targetduration */
               			 strURL, "/tmp/segment.ts", NULL, NULL) ==0 )  /* Use default callback write func. */
				/* Segment.ts always at /tmp/ */
                {
			printf("Ok, succeed to download segment from '%s'!\n", strURL);
			//dscnt++; NOT here!
		}
		else
			printf("Fail to download segment from '%s'!\n", strURL);

		/* F2.2  Download key: TODO if keyURI is NOT a complete/absolute URL address. */
		if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION,
					 1, 10,   /* A rather small value, near to targetduration */
                			 list->keyURI[k], "/tmp/aesKey.dat", NULL, NULL) ==0 )  /* Use default callback write func. */
		{
			/* F2_D1. Read key */
			unsigned char ukey[16]={0};
			FILE *fil=fopen("/tmp/aesKey.dat", "rb");
			fread(ukey, 16, 1, fil);
			fclose(fil);

			/* F2_D2. Prepare uiv */
			unsigned char *uiv=egi_str2hex(list->strIV[k], 16);

			/* F2_D3. Decrypt segment.ts */
        		size_t outsize;
			EGI_FILEMMAP *fmap=NULL;
			fmap=egi_fmap_create("/tmp/segment.ts", 0, PROT_READ|PROT_WRITE, MAP_SHARED);
		        if(fmap==NULL || fmap->fsize==0 ) {
                		printf("Fail to mmap segment.ts OR fsize==0!\n");
				egi_fmap_free(&fmap);
        		}
			else {
				/* Decrytp segment.ts */
        			if( AES_cbc128_encrypt( (unsigned char *)fmap->fp, fmap->fsize,  /* indata, insize, */
                                		(unsigned char *)fmap->fp, &outsize,      /* outdata, outsize */
		                                ukey, uiv, AES_DECRYPT)!=0 ) {            /* ukey, uiv, encode */
			                printf(DBG_RED"AES_cbc128_encrypt() fails!\n"DBG_RESET);
        			}
        			else {
					/* Resize segment.ts */
					if(outsize!=fmap->fsize)
	                			egi_fmap_resize(fmap, outsize);

					/* Append to all.ts */
					if(saveToMMC)
						//egi_system("cat /tmp/segment.ts >>/mmc/vall.mp4");
						egi_copy_file("/tmp/segment.ts", "/mmc/vall.mp4", true);
					else
						//egi_system("cat /tmp/segment.ts >>/tmp/vall.mp4");
						egi_copy_file("/tmp/segment.ts", "/tmp/vall.mp4", true);

					/* Update dscnt/durations */
					dscnt++;
					durations += list->tsec[k];
				}
			}

			/* F2_D4. Free uiv and fmap */
			free(uiv);
			egi_fmap_free(&fmap);
		}
		else
			printf("Fail to download AES key frome '%s'!\n", list->keyURI[k]);
	    }
	    /* F3. M3U8ENCRYPT_AES_SAMPLE or undefined! */
	    else {
		printf("Sorry, M3U8ENCRYPT_SAMPLE_AES or other encryption methods are NOT supported now!\n");
		sleep(1);
	    }

	    /* F4. Check duration */
	    if( recordTime>0 && (int)durations >= recordTime )
		break;

		egi_dpstd(DBG_YELLOW"Totally %d segments downloaded, with durations sumup: %.1fs \n"DBG_RESET, dscnt, durations);

	} /* End for() */
	//printf("\033[0;32;40m  durations(%.1fs) > recordTime(%ds), OK! \e[0m\n", durations, recordTime);

 	/* Covert and play video */
    	if( dscnt>0 && (recordTime==0 || (recordTime>0 && (int)durations >= recordTime)) ) {
		printf("\033[0;32;40m  durations(%.1fs) > recordTime(%ds), OK! \e[0m\n", durations, recordTime);

        	/* 4. Convert video to MPEG */
		printf("Start to convert video format...\n");
        	//egi_system("ffmpeg -y -i /tmp/vall.mp4 -f mpeg -s 320x200 -r 20 -q:v 0 -b:v 1200k /tmp/vall.avi");
		if(saveToMMC) /* mpeg do not support -r <20 */
	            egi_system("ffmpeg -y -i /mmc/vall.mp4 -f mpeg -vf 'scale=320:-1' -r 20 -q:v 0 /mmc/vall.avi");
		else
	            egi_system("ffmpeg -y -i /tmp/vall.mp4 -f mpeg -vf 'scale=320:-1' -r 20 -q:v 0 /tmp/vall.avi");

		/* 4a. Save result AVI */
		if(saveAVI && saveToMMC)
			egi_copy_file("/mmc/vall.avi", "/mmc/save_vall.avi", true);

		/* 5. Play video */
		printf("Start to play video...\n");
		for(k=0; k<playRounds; k++) {
			if(saveToMMC)
        		    egi_system("/tmp/MPlayer -ac mad -vo fbdev -vfm ffmpeg -framedrop -fs /mmc/vall.avi");
			else {
			    sprintf(strcmd,"/tmp/MPlayer -ac mad -vo fbdev -vfm ffmpeg -framedrop -fs -brightness %d /tmp/vall.avi",
					brightness);
			    egi_system(strcmd);
			}
			printf("Finish playing video!\n");
		}

		/* 6. Reset durations */
		durations=0.0;
		dscnt=0;

		/* 7. loop counter */
		loopcnt++;
    	}

#endif

	/* 7. Free and release */
	m3u_free_list(&list);
}
