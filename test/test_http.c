/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to parse and download AAC/TS m3u8 file.

        test_heliaac.c  ---> radio_aacdecode
        test_http.c     ---> http_aac
        test_decodeh264.c ---> h264_decode

TS compound stream data to be saved as a.ts/b.ts
	To be deleted by radio_aacdecode after playing AAC.
AAC stream data to be saved as a.stream/b.stream (a.aac/b.aac).
	To be deleted by radio_aacdecode after playing.
H264 stream data to be saved as a.h264/b.h264.



Note:
1. A small TARGETDURATION is good for A/V synchronization in this case?
    Smaller TARGETDURATON results in more frequent calling of Easy_downloading,
    which is costly for CPU.
2. If there's only 1 segment buffer file, it means each segment SHOULD
   be downloaded within TARGETDURATION.
3. XXX Complete URL strings in m3u8 list may NOT be in alphabetical/numberical
   (timestamp)sequence, however the URL source name of the SHOULD be in sequence.
   Example: 'hostname' and 'port' may be variable.
   ...
   http://118.188.180.180:8080/fdfdfd/live/123-120.ts
   http://118.177.160.150:5050/fdfdfd/live/123-130.ts
   http://118.177.160.151:5050/fdfdfd/live/123-140.ts
   ...
   XXX   ----- !!! NOT NECESSARY, see SequenceNumber. !!! -----

4. Current and the next m3u8 list may have overlapped URL items.
   TODO: If reset regLastName[] when regName[]<regLastName[], then overlapped
	segments will be played twice!

		<<< MultiThread EasyDownlaoding >>>
5. This program supports downloading ONE remote file with MULTIPLE threads

		<<< MultiURLs EasyDownloading >>
6. This program supports downloading MULTIPLE remote files simultaneously
   with ONE thread for ONE file. (Enable_mURL_Download)
   Suggest to apply when segment TARGETDURATION < 5s.
7. If playing an ENDLIST, then parse_m3u8list() will try relentlessly to download all
   segments before ends the while() loop.

TODO:
1. For some m3u8 address, it sometimes exits accidently when calling https_easy_download().
2. Too big TARGETDURATION value(10) causes disconnection/pause between
   playing two segments, for it's too long time for downloading a segment?
3. If too big timeout value, then ignore following items and skip to NEXT m3u8 list.
4. TODO: mem leakage.
5. egi_append_file() cause segmentation fault if it's Read-only file system.?
6. tm_get_strtime2() cause curl error and segmentation fault?
7. If m3u8 address is redirected, change argURL accordingly.

Journal:
2021-09-29:
	1. main():  memset buff[] before https_curl_request().
	2. parse_m3u8list(): If https_easy_download() fails, return immediately!
2022-06-29:
	1. For case *.ts file in m3u8.
2022-07-02:
	1. Extract AAC from TS.
2022-07-03:
	1. Save as a._stream/b._stream(a._aac/b._aac), then rename to a.stream/b.stream(a._aac/b._aac).
2022-07-08:
	1. Extract NAUL data and save to a._h264/b._h264 then rename to a.h264/b.h264
2022-07-12:
	1. Compare segName sequence, NOT segLastURL.
2022-07-14:
	1. Reset segLastURL[] upon #EXT-X-DISCONTINUITY tags in m3u8
2022-07-15:
	1. parse_m3u8list(): Call m3u_parse_simple_HLS()
2022-07-18:
	1. Test multiple thread downloading with https_easy_mThreadDownload()
2022-07-20:
	1. Add decrypt_file() to decrypt segments.
2022-07-22:
	1. Test https_easy_mURLDownload().
2022-07-27:
	1. Rename a.stream/b.stream to a.aac/b.aac; a._stream/b._stream to a._aac/b._aac.
	2. Case F3 NOT necessary.
2022-07-28:
	1. Check start of a new round of program.
	2. Get sublist from Master List.
2022-09-23: Add option 'l' for loop playing VOD.
2022-12-25: https_curl_request(): If redirect URL then quto. renew strRequest and try again.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <egi_https.h>
#include <egi_timer.h>
#include <egi_debug.h>
#include <egi_log.h>
#include <egi_cstring.h>
#include <egi_utils.h>
#include <egi_aes.h>

#define MTHREAD_EASY_DOWN  1   /* 1 MultiThread Easy Download */

char strRequest[EGI_URL_MAX]; //Will be changed */
char newURL[EGI_URL_MAX];    //MUST provide if need redirection

/* For mURL_Download */
bool Enable_mURL_Download=false;   /* To enable downloading multiple files simultaneously
				    * 1. Auto. set TURE if mlist->maxDuraion<5.0
				    * 2. OR set in input option.
				    */
int  	nurls=2; /* MAX number of URLs(Files) downloading at same time
		  * 1. If mlist->maxDuraion<5.0, then nurls=6;
		  * 2. OR set in input option.
		  */
int     nseg;  /*  Actual number of URLs(Files) downloading at same time,
		* Usually =nurls, for the last group of URLs, nseq<=nurls
		*/
char**	strURL;
int  	uindx;
char**	filenames; /* For temporary files */

//bool Enable_mThread_Download=false;
int nthreads=1;		/* Threads for easy downloading ONE remote file.
			 * Default nthreads==1: To call https_easy_download()
			 * nthread>1:	To call https_easy_mThreadDownload()
			 */
int tss;		/* Start playing from index number tss of the list. (skipping tss segments)
			 * ONLY for PlayList with EXT-X-MEDIA-SEQUENCE ==0 !
			 */

#define TMP_KEY_FPATH  "/tmp/ts_aesKey.dat"

/* Loop playing VOD */
bool loopVOD=false;

/* #EXT-X-PLAYLIST-TYPE:VOD */
bool isVOD;	 /* NOTE: sometimes VOD tags is missing, so use isEndList instead. */
/* #EXT-X-ENDLIST */
bool isEndList;	 /* The last list for the remote file, no more media segments. */
/* #EXT-X-MEDIA-SEQUENCE */
int lastSeqNum;  /* Last media sequence number */

int curl_nwrite;   /* Bytes downloaded */
int mbcnt; 	   /* MBytes downloaded */

size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp);
void parse_m3u8list(char *strm3u);
char segLastURL[1024];  /* The last URL for (aac or ts) segment */
char segLastName[1024];  /* Segment file name of the URL (aac or ts) */
char* dirURL; /* Dir URL of input m3u8 address */
char* hostName;
char* protocolName;

/* Functions */
int decrypt_file(const char *fin, const char *fkey, const char *strIV);


void print_help(const char* cmd)
{
        printf("Usage: %s [-hn:m:s:] file\n", cmd);
        printf("        -h   This Help \n");
	printf("	-l   Loop playing VOD\n");
        printf("        -n:  Threads to download a file, Default 1. Ineffective if -m option is set.\n");
        printf("        -m:  To download m files simultaneously, one thread for one file. min=2.\n");
        printf("        -s:  Skip first N segments in the list.\n");
	printf("  !!! Note:  If segment maxDruation<5.0, then '-m 6' will apply automatically, and user options will be ignored.\n");
	printf("\n");
}


/*----------------------------
            MAIN
----------------------------*/
int main(int argc, char **argv)
{
	char buff[CURL_RETDATA_BUFF_SIZE];
	int opt;
	char *argURL;

        while( (opt=getopt(argc,argv,"hln:m:s:"))!=-1 ) {
                switch(opt) {
                        case 'h':
				print_help( argv[0]);
                                exit(0);
                                break;
			case 'l':
				loopVOD=true;
				break;
                        case 'n':
                                nthreads=atoi(optarg);
				if(nthreads<1)
					nthreads=1;
                                break;
			case 'm':
				Enable_mURL_Download=true;
				nurls=atoi(optarg);
				if(nurls<2) nurls=2;
				break;
			case 's':  /* Skip first N segments in the list */
				tss=atoi(optarg);
				if(tss<0)
					tss=0;
				break;
                }
        }
        if( optind < argc ) {
                argURL=argv[optind];
        } else {
                printf("No URL!\n");
                exit(1);
        }


/* TODO test */
//	cstr_split_nstr(strm3u, split, )

#if 0	/* Start egi log */
  	if(egi_init_log("/mmc/test_http.log") != 0) {
                printf("Fail to init logger,quit.\n");
                return -1;
  	}
	EGI_PLOG(LOGLV_INFO,"%s: Start logging...", argv[0]);
#endif

#if 1	/* For http,  conflict with curl?? */
	printf("start egitick...\n");
	tm_start_egitick();
#endif

	/* prepare GET request string */
        memset(strRequest,0,sizeof(strRequest));

	/* Init strRequest */
        strcat(strRequest,argURL);
        EGI_PLOG(LOGLV_CRITICAL,"Http request head:\n %s\n", strRequest);

#if 0	/* Get dirURL */
	/* URL, **protocol, **hostname, *port, **path, **filename, **dir, **dirURL */
	cstr_parse_URL(strRequest, &protocolName, &hostName, NULL, NULL, NULL, NULL, &dirURL);
	printf("protocolName: %s\n hostName: %s\n dirULR: %s\n", protocolName,hostName,dirURL);
#endif

         char tmbuf[128];
         tm_get_strtime2(tmbuf, " <---- Start Playing... --->\n");
         egi_append_file("/mmc/test_http.log", tmbuf, strlen(tmbuf));

while(1) {
	/* Reset buff */
	memset(buff, 0, sizeof(buff));

	/* Reset newURL */
	newURL[0]=0;

	/* Https GET request. Redirect URL at newURL, strRequest will be changed also! */
	EGI_PLOG(LOGLV_INFO,"Start https curl request...");
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_ENABLE_REDIRECT, 3,30, /* HK2022-07-03 */
				strRequest, buff, newURL, curl_callback) !=0 )
	{
		EGI_PLOG(LOGLV_ERROR, "Fail to call https_curl_request()! try again...");
		/* Try again */
		sleep(1);
		continue;
		//EGI_PLOG(LOGLV_ERROR, "Try https_curl_request() again...");
		//exit(EXIT_FAILURE);
	}
        //printf("        --- Http GET Reply ---\n %s\n",buff);
	if(newURL[0])
		printf(DBG_GREEN"Redirected URL: %s\n"DBG_RESET, newURL);

	EGI_PLOG(LOGLV_INFO,"Http curl get: %s\n", buff);

#if 0 /* TEST: --------------- */
	printf("\n%s\n",buff);
	EGI_M3U8_LIST *mlist=m3u_parse_simple_HLS(buff);
	if(mlist) printf("mlist->ns=%d\n", mlist->ns);
	exit(0);
#endif

	/* Parse content */
	EGI_PLOG(LOGLV_INFO,"Start parse m3u8list...");
	parse_m3u8list(buff);

	/* Check ENDLIST, Sometimes VOD tag is missing.  */
	if(isEndList) {
		char tmbuf[128];
		tm_get_strtime2(tmbuf, " <----End list---> ");
		egi_append_file("/mmc/test_http.log", tmbuf, strlen(tmbuf));
		//system("echo  ----Endlist--- >>/mmc/test_http.log");

		if(!loopVOD) break;
	}

	/* Sleep of TARGETDURATION --- If too small, it will fetch the same segment? */
	//sleep(7/2);
	//usleep(500000);
	sleep(1);
}

	/* Free and release */
	free(dirURL); free(hostName); free(protocolName);

	exit(EXIT_SUCCESS);
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        size_t session_size=size*nmemb;

        //printf("%s: session_size=%zd\n",__func__, session_size);
	EGI_PLOG(LOGLV_INFO, "%s: ptr: %s\n", __func__, (char *)ptr);

	/* Just copy to user */
//        strcat(userp, ptr);
	strncat(userp, ptr, session_size);  /* HK2022-07-19  string data MAY have terminating NULL! */

        return session_size;
}

/*-----------------------------------------------
 A callback function for write data to file.

 TODO: Check buff space.
------------------------------------------------*/
size_t download_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
        size_t written;

        written = fwrite(ptr, size,  nmemb, (FILE *)stream);
        //egi_dpstd("written size=%zd\n", written);
	if(written<0) {
		EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  written<0!  xxxxx");
		written=0;
	}
	curl_nwrite += written;

#if 0
	int tmp;
	tmp=curl_nwrite/1024/1024;
	if(mbcnt != tmp) {
		mbcnt = tmp;
		EGI_PLOG(LOGLV_CRITICAL,"download ~%dMBytes", mbcnt);
	}
#else
	egi_dpstd(DBG_GREEN" curl_nwritten=%d\n"DBG_RESET, curl_nwrite);
#endif

        return written;
}


/*---------------------------------------------
Parse m3u8 content, with m3u_parse_simple_HLS()

Note:
XXX 1. Content in strm3u will be changed!
    OK, unchanged with m3u_parse_simple_HLS()
--------------------------------------------*/
void parse_m3u8list(char *strm3u)
{
	int k;
	int ret;
	char *ps;
//	char segURL[1024]={0}; /* URL item in m3u8 list */
	char *segURL=NULL; /* Segment file full URL */
	char* segName; /* segment file name at URL (aac or ts) */
	bool Is_AAC=false; /* *.aac segment */
	bool Is_TS=false;  /* *.ts segment */
	long timeout=30;
	int startIndx;
	EGI_M3U8_LIST* mlist=NULL;


	/* 1. Get mlist */
	mlist=m3u_parse_simple_HLS(strm3u);
	if(mlist==NULL) {
		egi_dpstd(DBG_RED"m3u_parse_simple_HLS() returns NULL!\n"DBG_RESET);
		return;
	} else {
		egi_dpstd(DBG_MAGENTA"Total items=%d, seqnum=%d(lastSeqNum=%d), maxDuration=%f\n"DBG_RESET,
							mlist->ns, mlist->seqnum, lastSeqNum, mlist->maxDuration);
	}

	/* 1a. If Master List, then update strRequest */
	if(mlist->isMasterList) {
	   if(mlist->ns>0) {
		 /* Get first sublist */
		 char *sublist=egi_URI2URL(strRequest, mlist->msURI[0]);
		 /* Update strRequest */
		 strRequest[sizeof(strRequest)-1]='\0';
                 strncpy(strRequest,sublist, sizeof(strRequest)-1);
		 /* Free sublist */
		 free(sublist);
	   }
	   else {
		egi_dpstd(DBG_RED"M3U8 master list has NO sublist inside!\n"DBG_RESET);
		sleep(1);
	   }
	   goto END_FUNC;
	}


	/* 1A. Get isEndList before releasing mlist. */
	isEndList = mlist->isEndList;
	isVOD = (mlist->type==M3U8PLTYPE_VOD);

	/* 1B. Set Enable_mURL_Download */
	if(mlist->maxDuration <5.0) {
		/* Set nurls */
		Enable_mURL_Download=true;
		nurls=6;
	}
	//else  /* As user input option */
	//	Enable_mURL_Download=false;

	/* 1C. Create charList for mURL_Download */
	if( Enable_mURL_Download ) {
		strURL=egi_create_charList(nurls,EGI_URL_MAX);
		filenames=egi_create_charList(nurls, EGI_PATH_MAX); //+EGI_NAME_MAX);
		if(strURL==NULL || filenames==NULL) {
			m3u_free_list(&mlist);
			exit(1);
		}
	}

	/* 2. Skip if all old items, except for start of a new round with mlist->seqnum==0.  */
	if(mlist->seqnum!=0 && lastSeqNum >= mlist->seqnum+mlist->ns-1) {
		egi_dpstd(DBG_CYAN" <----- All Obsolete List? ----->\n"DBG_RESET);

#if 1 /* TEST: ------------------ */
			const char *strnote="<----- All Obsolete List? ----->\n";
			egi_append_file("/mmc/test_http.log", (void*)strnote, strlen(strnote)+1);
			egi_append_file("/mmc/test_http.log", strm3u, strlen(strm3u));
			egi_append_file("/mmc/test_http.log", "\n", 1);
#endif
		goto END_FUNC;
		// If server seqnumb error! then Go on to rest lastSeqNum==mlist->seqnum+mlist->ns-1 ..... see 5.0

	}

	/* 3. Get startIndx of mlist->msURI[], to succeed the last played segment. */
	if(mlist->seqnum==0) {
		       printf("mlist->seqnum==0\n");
#if 1 /* TEST: ------------------ */
		#if 1
		        EGI_PLOG(LOGLV_CRITICAL,"<---- New Round ---->");
			const char *strnote="<----- New round ----->\n";
			egi_append_file("/mmc/test_http.log", (void*)strnote, strlen(strnote));
		#else  /* TODO: Segfault */
			char tmbuf[128];
			tm_get_strtime2(tmbuf, " <---- New round --->\n");
			egi_append_file("/mmc/test_http.log", tmbuf, strlen(tmbuf));
		#endif
			egi_append_file("/mmc/test_http.log", strm3u, strlen(strm3u));
			egi_append_file("/mmc/test_http.log", "\n", 1);
#endif

		/* Reset startIndx and lastSeqNum */
		startIndx=0;
		lastSeqNum=0;

		/* Skip segments */
		if(tss>0 && mlist->ns > tss ) {
			startIndx=tss;
			lastSeqNum=tss;
		}
	}
	else if(lastSeqNum < mlist->seqnum) {
		startIndx=0;
	}
	else if(lastSeqNum >= mlist->seqnum) {
		startIndx=lastSeqNum-mlist->seqnum +1;
	}

	printf("Playlsit seqnum=%d, startIndx=%d\n", mlist->seqnum, startIndx);

	/* 4. Process each URI */
	for(k=startIndx; k < mlist->ns;  k++) {
		egi_dpstd(DBG_BLUE"startIndx=%d, k=%d/(%d-1)\n"DBG_RESET, startIndx, k, mlist->ns);
		ps=mlist->msURI[k];
		printf(DBG_YELLOW"Sequence %d, ps: %s\n"DBG_RESET, mlist->seqnum+k, ps);

		/* 4.1 Check segment type */
		Is_AAC=Is_TS=false;
		if( strstr(ps,".aac") && strstr(ps,"//") )
			Is_AAC=true;
		//else if( strstr(ps, ".ts") )
		//	Is_TS=true;
		/* ---- Suppose others are all TS ---- */
		else
			Is_TS=true;

		/* 4.1A  Download key to file.
		 * 1. TODO if keyURI is NOT a complete/absolute URL address.
		 * 2. For VOD: only keyURI[0] is provided.
		 */
		if( mlist->keyURI[k]!=NULL ) {
			 EGI_PLOG(LOGLV_CRITICAL,"Start downloading key data for decryption...");
			 char *keyURI=egi_URI2URL(strRequest, mlist->keyURI[k]);
			 https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION |
					      HTTPS_ENABLE_REDIRECT, 1, 10,   /* A rather small value, near to targetduration */
                			      keyURI, TMP_KEY_FPATH, NULL, NULL);  /* Use default callback write func. */
			free(keyURI);
		}

		/* 4.2 Parse AAC or TS segment file */
		if( Is_AAC || Is_TS ) {
			//EGI_PLOG(LOGLV_INFO, "sub URL: '%s'.",ps);

			/* 4.2.1 Get full path segment URL: segURL */
			free(segURL); segURL=NULL;
			segURL=egi_URI2URL(strRequest, ps);
			egi_dpstd(DBG_BLUE"Stream URL: %s\n"DBG_RESET, segURL);

#if 1 /* TEST: ------------------ */
			egi_append_file("/mmc/test_http.log", DBG_BLUE, strlen(DBG_BLUE));
			egi_append_file("/mmc/test_http.log", segURL, strlen(segURL));
			egi_append_file("/mmc/test_http.log", "\n", 1);
			egi_append_file("/mmc/test_http.log", DBG_RESET, strlen(DBG_RESET));
#endif

	///////////////*   4.2.2 https_easy_mURLDownload()   */////////////////
	if(Enable_mURL_Download && Is_TS ) {

		/* M1. Get URLs */
		strURL[uindx][0]=0;
		strcat(strURL[uindx],segURL);

		/* M2. Count URLs */
		uindx ++;
		/* For the last segment, we may NOT get nurls number of URLs */
		if( k==mlist->ns-1 && uindx!=nurls ) {
			nseg=uindx;
			//uindx=0;
		}
		/* Continue to read urls */
		else if(uindx!=nurls)
			continue;
		else  { /* Ok, Get all URLs */
			/* Set actual segments */
			nseg=nurls;
			/*Reset uindx */
			//uindx=0;
		}

#if 1 /* TEST: ------------- */
			int m=0;
			for(m=0; m<nseg; m++) {
				egi_dpstd(DBG_YELLOW"mURLs[%d]: '%s'\n"DBG_RESET, m, strURL[m]);
			}
#endif

		/* M2a. Clear/Reset uindx */
		uindx=0;

		/* M3. mURL Downloading */
   		ret=https_easy_mURLDownload(nseg, (const char**)strURL, "/tmp/4seg.ts",   /* n, **urls, file_save */
			    HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
                                                         | HTTPS_ENABLE_REDIRECT, 1, timeout); /* opt, trys, timeout */
		if(ret) {
			/* Rewind k */
			k -=nseg;
			if(k<-1)k=-1;

			if(isVOD || isEndList) /* Sometimes VOD tag is missing... so check isEndlist instread */
				continue;  /* VOD ok, continue... */
			else
				break; /* To break loop, playlist maybe already obsolete for the timeout! */
		}


		/* M4. Create c.ts */
		FILE *fil=fopen("/tmp/c.ts", "wb");
		if(fil==NULL) {
			egi_dperr("fopen /tmp/c.ts");
			exit(2);
		}

		/* M5. Decrypt files and merge into 'c.ts' */
		int i;
		for(i=0; i<nseg; i++) {
			sprintf(filenames[i], "/tmp/__%d__.ts", i);
			if(access(filenames[i],F_OK)==0) {
	  			if(mlist->keyURI[0]!=NULL)
	       				decrypt_file(filenames[i], TMP_KEY_FPATH, mlist->strIV[k]);
				egi_copy_file(filenames[i], "/tmp/c.ts", true); /* src,dest,append */
			}
		}

		/* M6. Close fil */
		fclose(fil);
	}
	//////////////////////////////////////////////////////////////////

			/* 4.2.3 Extract segName from URL */
			free(segName); segName=NULL;
			cstr_parse_URL(segURL, NULL, NULL, NULL, NULL, &segName, NULL, NULL);
			//printf(DBG_GREEN"segName: %s  <<<<<<  segLastName: %s\n"DBG_RESET, segName, segLastName);

CHECK_BUFF_FILES:	/* 4.2.4 Check and download buffer files */
			/* 4.2.4_F1. Check and download a.aac */
			if( access("/tmp/a.aac",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading a.aac/a.ts from: %s",  segURL);
				if(Enable_mURL_Download && Is_TS ) {  /* mURL Download */
					if(access("/tmp/c.ts",F_OK)==0)
						rename("/tmp/c.ts", "/tmp/a.ts");
					ret=0;
				}
				else if(nthreads==1) // || !Enable_mThread_Download)  /* Easy download */
		                	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3, timeout,  //3,3
								 segURL, Is_AAC?"/tmp/a._aac":"/tmp/a.ts", NULL, download_callback);
				else /* Multi_thread easy download */
			        	ret=https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, nthreads, 0, timeout, /* threads, trys, timeout */
                                    				segURL, Is_AAC?"/tmp/a._aac":"/tmp/a.ts");

				if(ret!=0)
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download a._aac/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._aac":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/a._aac(or a.ts.");

					/* !!! Break NOW, in case playlist is already obsolete, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_1. End parsing m3u8list, and starts a new http request...\n");
					if(mlist->isEndList)continue; else break;
				} else {

					if(Is_TS)  {
					    /* Decrypt file, mURL_Download already decrypted. */
					    if(mlist->keyURI[0]!=NULL && !Enable_mURL_Download) {
						  printf("Start decrypt a.ts...\n");
						  decrypt_file("/tmp/a.ts", TMP_KEY_FPATH, mlist->strIV[k]);
					    }

					    /* Extract AAC from TS */
					    printf("Start extract AV from ts...\n");
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._aac", "/tmp/a._h264");
					}

					/* Rename to a.aac */
					printf("rename a._aac to a.aac\n");
					rename("/tmp/a._aac","/tmp/a.aac");

					/* Rename to a.h264,  ---by test_helixaac  */
					//if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264", "/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.aac updated! curl_nwrite=%d", curl_nwrite);
				}
			}

			/* 4.2.4_F2. Check and download b.aac */
			else if( access("/tmp/b.aac",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading b._aac/b.ts from: %s",  segURL);
				if(Enable_mURL_Download && Is_TS ) {  /* mURL Download */
					if(access("/tmp/c.ts",F_OK)==0)
						rename("/tmp/c.ts", "/tmp/b.ts");
					ret=0;
				}
			        else if(nthreads==1) // || !Enable_mThread_Download)  /* Easy download */
		                	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT , 3, timeout, //3,3
								 segURL, Is_AAC?"/tmp/b._aac":"/tmp/b.ts", NULL, download_callback);
			   	else /* Multi_thread easy download */
			        	ret=https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, nthreads, 0, timeout, /* threads, trys, timeout */
                                    				segURL, Is_AAC?"/tmp/b._aac":"/tmp/b.ts");

			        if(ret!=0)
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download b._aac/b.ts from '%s'.", segURL);

					remove(Is_AAC?"/tmp/b._aac":"/tmp/b.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/b._aac(or b.ts).");

					/* !!! Break NOW, in case playlist is already obsolete, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_2. End parsing m3u8list, and starts a new http request...");
					if(mlist->isEndList)continue; else break;

				} else {

					if(Is_TS)  {
					    /* Decrypt file, mURL_Download already decrypted. */
					    if(mlist->keyURI[0]!=NULL && !Enable_mURL_Download)
						  decrypt_file("/tmp/b.ts", TMP_KEY_FPATH, mlist->strIV[k]);

					    /* Extract AAC from TS */
					    egi_extract_AV_from_ts("/tmp/b.ts", "/tmp/b._aac", "/tmp/b._h264");
					}

					/* Rename to a.aac */
					printf("rename b._aac to b.aac\n");
					rename("/tmp/b._aac","/tmp/b.aac");

                                        /* Rename to b.h264 ---by test_helixaac */

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, b.aac updated! curl_nwrite=%d", curl_nwrite);
				}
			}
		#if 0	/*  F3.!! In some cases, time stamp is NOT consistent with former URL list, strcmp() will fail!???  */
			/* TODO: NOT necesasry ??? */
			else if( access("/tmp/a.aac",F_OK)!=0  && access("/tmp/b.aac",F_OK)!=0 ) {
				// && strcmp(segName, segLastName) > 0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_ERROR, "Case 3: a.aac & b.aac both missing! downloading a._aac/a.ts from: %s",  segURL);
				if(Enable_mURL_Download && Is_TS ) {  /* mURL Download */
					if(access("/tmp/c.ts",F_OK)==0)
						rename("/tmp/c.ts", "/tmp/a.ts");
					ret=0;
				}
				else if(nthreads==1) // || !Enable_mThread_Download) /* Easy download */
			                ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3,timeout, //3,3
								 segURL, Is_AAC?"/tmp/a._aac":"/tmp/a.ts", NULL, download_callback);
				else /* Multi_thread easy download */
			        	ret=https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, 2, nthreads, timeout, /* threads, trys, timeout */
                                    				segURL, Is_AAC?"/tmp/a._aac":"/tmp/a.ts");

				if(ret!=0)
				{
                        		EGI_PLOG(LOGLV_ERROR, "Case 3: Fail to easy_download a._aac/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._aac":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Case 3: Finsh remove /tmp/a._aac(or a.ts).");

					/* !!! Break NOW, in case playlist is already obsolete, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_3. End parsing m3u8list, and starts a new http request...");
					if(mlist->isEndList)continue; else break;
				} else {

					if(Is_TS)  {
					    /* Decrypt file, mURL_Download already decrypted. */
					    if(mlist->keyURI[0]!=NULL && !Enable_mURL_Download)
						  decrypt_file("/tmp/a.ts", TMP_KEY_FPATH, mlist->strIV[k]);

					    /* Extract AAC from TS */
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._aac", "/tmp/a._h264");
					}

					/* Rename to a.aac */
					printf("rename2 a._aac to a.aac\n");
					rename("/tmp/a._aac","/tmp/a.aac");

					/* Rename to a.h264,  ---by test_helixaac  */
                                        //if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264","/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok2, a.aac updated! curl_nwrite=%d", curl_nwrite);
				}
			}
	          #endif  /* F3. END */
			/* 4.2.4_F4. Both OK */
			else if(access("/tmp/a.aac",F_OK)==0 && access("/tmp/b.aac",F_OK)==0) {
				EGI_PLOG(LOGLV_CRITICAL, "a.aac and b.aac both available!");

				/* Wait for consuming.... */
				do {
					if(mlist->tsec[k]>4.0)
				   		sleep(1);
					else
				   		usleep(200000);
				} while( access("/tmp/a.aac",F_OK)==0 && access("/tmp/b.aac",F_OK)==0);

				if(Enable_mURL_Download) {
					//egi_dpstd("Go to CHECK_BUFF_FILES...\n");
					goto CHECK_BUFF_FILES;
				}
				/* Rewind k */
				else {
					k--;
				}
			}

			/* F5. Sequence reset?  */
			//else if( strcmp(segName, segLastName) <= 0 ) {
			//	EGI_PLOG(LOGLV_CRITICAL, DBG_YELLOW"------ Old Sequence -----\n"DBG_RESET);
			//	EGI_PLOG(LOGLV_CRITICAL, "segName: %s\n", segName);
			//	EGI_PLOG(LOGLV_CRITICAL, "segLastName: %s\n", segLastName);
			//}

		}
		/* 4.3 NOT .aac OR .ts */
		else  {
			egi_dpstd(DBG_YELLOW"Only support AAC/TS stream, fail to support URL: '%s'.\n"DBG_RESET,ps);
		}

	} /* END for(K) */


END_FUNC:
	/* 5. Update or Reset lastSeqNum. */
	if( mlist->isMasterList==false ) {
	   if(isEndList==false) {
		lastSeqNum = mlist->seqnum+mlist->ns-1;
		printf("End sub playlist, lastSeqNum is %d\n", lastSeqNum);
	   }
	   else {
		printf("End all playlists, lastSeqNum is %d\n", lastSeqNum);
		lastSeqNum=0;
	   }
	}

	/* 6. Free temp var. In case break in mid of processing...*/
	free(segURL); segURL=NULL;
	free(segName); segName=NULL;

	/* 7. Free charList */
	egi_free_charList(&strURL, nurls);
	egi_free_charList(&filenames, nurls);

	/* 8. Free mlist */
	printf("Free list...\n");
	m3u_free_list(&mlist);
	printf("free OK\n");
}

/*------------------------------------------------------
To decrypt a file with AES_cbc128 encryption.

Note:
1. Playlist_Type_VOD has ONLY one EXT-X-KEY tag in a PlayList:
   #EXT-X-KEY:METHOD=AES-128,URI="https://iqiyi.shanshanku.com/20211012/N3PjvZ3A/1200kb/hls/key.key"
   #EXTINF:2.085,
   https://......ts
   #EXTINF:2.085,
   https://......ts
   #EXTINF:2.085,
   ...

2. Playlist_type_EVENT(or ) has one EXT-X-KEY tag for each media segment.
   #EXT-X-KEY:METHOD=AES-128,IV=0x30303030303030303030303055555555,URI="https://...",KEYFORMAT="identity"
   #EXTINF:5.004
   https://......ts
   #EXT-X-KEY:METHOD=AES-128,IV=0x30303030303030303030303055555555,URI="https://...",KEYFORMAT="identity"
   #EXTINF:5.004
   https://......ts
   ...


@fin:	 Input file.	!!! --- CAUTION --- !!!
	 To be overwritten by decrypted data.
@fkey:	 File with ukey.
@strIV:  String with initial vector.

Return:
	0	OK
	<0	Fails
-----------------------------------------------------*/
int decrypt_file(const char *fin, const char *fkey, const char *strIV)
{
	int ret=0;
	EGI_FILEMMAP *fmap=NULL; /* in/out file */
	FILE* fil=NULL;
	unsigned char ukey[16]={0};
	unsigned char *uiv=NULL;
        size_t outsize;

egi_dpstd(DBG_YELLOW"Decrypting ...\n"DBG_RESET);

	/* 1. Mmap fin  */
	fmap=egi_fmap_create(fin, 0, PROT_READ|PROT_WRITE, MAP_SHARED);
        if(fmap==NULL || fmap->fsize==0 ) {
             	egi_dpstd(DBG_RED"Fail to mmap '%s' OR fsize==0!\n"DBG_RESET, fin);
		return -1;
        }

	/* 2. Read key */
	fil=fopen(fkey, "rb");  /* For VOD, it may already exist! */
	if(fil==NULL) {
             	egi_dpstd(DBG_RED"Fail to open fkey '%s'!\n"DBG_RESET, fkey);
		ret=-2; goto END_FUNC;
	}
	if( fread(ukey, 16, 1, fil) !=1 ) {
             	egi_dpstd(DBG_RED"Fail to read ukey from '%s'!\n"DBG_RESET, fkey);
		ret=-2; goto END_FUNC;
	}
	fclose(fil);

	/* 3. Prepare uiv */
	/* NOTICE: if strIV==NULL, egi_str2hex() will return all 0s */
	uiv=egi_str2hex((char *)strIV, 16);

	/* 4. Decrypt file with AES_cbc128_encrypt */
	if( AES_cbc128_encrypt( (unsigned char *)fmap->fp, fmap->fsize,  /* indata, insize, */
       				(unsigned char *)fmap->fp, &outsize,     /* outdata, outsize */
                       		ukey, uiv, AES_DECRYPT)!=0 )             /* ukey, uiv, encode */
	{
		egi_dpstd(DBG_RED"AES_cbc128_encrypt() fails!\n"DBG_RESET);
		ret=-4; goto END_FUNC;
       	}
	/* 5. Resize decrypted file, which MAY be smaller in size. */
	else {
		if(outsize!=fmap->fsize)
           		egi_fmap_resize(fmap, outsize);
	}

END_FUNC:
	//if(fil) fclose(fil); Already closed
	if(uiv) free(uiv);
	egi_fmap_free(&fmap);
	return ret;
}






#if 0 ////////////////////////////////   OBSOLETE   ///////////////////////////////////

/*---------------------------------------------
Parse m3u8 content, with m3u_parse_simple_HLS()

Note:
	1. Content in strm3u will be changed!
--------------------------------------------*/
void parse_m3u8list(char *strm3u)
{
	int k;
	const char *delim="\r\n";
	char *ps;
	char segURL[1024]={0}; /* URL item in m3u8 list */
	bool Is_AAC=false; /* *.aac segment */
	bool Is_TS=false;  /* *.ts segment */
	long timeout=30;

	/* Get m3u8 params: VERSION,TARGETDURATION,SEQUENCE,EXTINF .....  */

	/* Parse AAC URL List */
        ps=strtok(strm3u, delim);
        for(k=0; ps!=NULL; k++) {
		printf(">>>> Ps[]: %s\n",ps);

/*------- Example: discontinuity ----------
#EXTM3U
#EXT-X-TARGETDURATION:10
#EXT-X-VERSION:6
#EXT-X-MEDIA-SEQUENCE:160086
#EXT-X-DISCONTINUITY-SEQUENCE:26732  <----------
#EXT-X-PROGRAM-DATE-TIME:2022-07-14T07:20:08.647Z
#EXTINF:10
https://.....
#EXTINF:10
https://....
#EXTINF:8
https://....
#EXT-X-DISCONTINUITY   <------------
#EXTINF:10
https://.....
#EXTINF:10
https://....
....
-------------------------------------------*/

		/* Discontinuity token. #EXT-X-DISCONTINUITY */
		if( !strstr(ps, "#EXT-X-DISCONTINUITY-SEQUENCE")
		    &&strncmp(ps,"#EXT-X-DISCONTINUITY",strlen("#EXT-X-DISCONTINUITY") )==0 ) {
			egi_dpstd(DBG_MAGENTA"----- DX-DISCONTINUITY -----\n"DBG_RESET);
			/* Reset segLastName then */
			segLastName[0]=0;

			/* Fetch next item */
			ps=strtok(NULL, delim);
			continue;
		}
		/* Skip # line */
		else if(ps[0]=='#') {
			/* Fetch next item */
			ps=strtok(NULL, delim);
			continue;
		}

		/* Check type */
		Is_AAC=Is_TS=false;
		if( strstr(ps,"aac") && strstr(ps,"//") )
			Is_AAC=true;
		else if( strstr(ps, ".ts") )
			Is_TS=true;
		else
			Is_TS=true;

		if( Is_AAC || Is_TS ) {
			EGI_PLOG(LOGLV_INFO, "sub URL: '%s'.",ps);
			memset(segURL,0,sizeof(segURL));

			/* To assemble and get complete URL for .ts file HK2022-07-05 */
		        /* Given full URL */
			if(strstr(ps, "http:") || strstr(ps, "https:")) {
				strncat(segURL, ps, sizeof(segURL)-1);
			}
			/* Given hostname+path */
			else if( ps[0]=='/' && (strlen(ps)>1 && ps[1]=='/') ) {
				strcat(segURL, protocolName);  /* including "//" */
				strncat(segURL, ps+2, sizeof(segURL)-1-strlen(segURL));
			}
			/* Given path */
			else if( ps[0]=='/' && (strlen(ps)>1 && ps[1]!='/') ) {
				 strcat(segURL, protocolName);
				 strncat(segURL, hostName, sizeof(segURL)-1-strlen(segURL));
				 strncat(segURL, ps, sizeof(segURL)-1-strlen(segURL));
			}
			/* Given file/resource name */
			//else if(strstr(ps, "http:")==NULL && strstr(ps, "https:")==NULL ) {
			else {
				strcat(segURL,dirURL);
				strncat(segURL, ps, sizeof(segURL)-1-strlen(segURL));
			}
			egi_dpstd(DBG_BLUE"Stream URL: %s\n"DBG_RESET, segURL);

			/* Extract segName for URL */
			free(segName); segName=NULL;
			cstr_parse_URL(segURL, NULL, NULL, NULL, NULL, &segName, NULL, NULL);
			printf(DBG_GREEN"segName: %s  <<<<<<  segLastName: %s\n"DBG_RESET, segName, segLastName);

			/* 1. Check and download a.aac */
			//if( access("/tmp/a.aac",F_OK)!=0 && strcmp(segURL, segLastURL) > 0) {
			if( access("/tmp/a.aac",F_OK)!=0 && strcmp(segName, segLastName) > 0) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading a.aac/a.ts from: %s",  segURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3, timeout,  //3,3
							 segURL, Is_AAC?"/tmp/a._aac":"/tmp/a.ts", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download a._aac/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._aac":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/a._aac(or a.ts.");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_1. End parsing m3u8list, and starts a new http request...\n");
					return;
				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._aac", "/tmp/a._h264");

					/* Rename to a.aac */
					rename("/tmp/a._aac","/tmp/a.aac");

					/* Rename to a.h264,  ---by test_helixaac  */
					//if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264", "/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.aac updated! curl_nwrite=%d", curl_nwrite);
					strncpy( segLastName, segName, sizeof(segLastName)-1 );
				}
			}

			/* 2. Check and download b.aac */
			//else if( access("/tmp/b.aac",F_OK)!=0 && strcmp(segURL, segLastURL) > 0) {
			else if( access("/tmp/b.aac",F_OK)!=0 && strcmp(segName, segLastName) > 0) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading b._aac/b.ts from: %s",  segURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT , 3, timeout, //3,3
							 segURL, Is_AAC?"/tmp/b._aac":"/tmp/b.ts", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download b._aac/b.ts from '%s'.", segURL);

					remove(Is_AAC?"/tmp/b._aac":"/tmp/b.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/b._aac(or b.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_2. End parsing m3u8list, and starts a new http request...\n");
					return;

				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/b.ts", "/tmp/b._aac", "/tmp/b._h264");

					/* Rename to a.aac */
					rename("/tmp/b._aac","/tmp/b.aac");

                                        /* Rename to b.h264 ---by test_helixaac */
                                        //if(access("/tmp/b.h264",F_OK)!=0 )
                                        //        rename("/tmp/b._h264", "/tmp/b.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, b.aac updated! curl_nwrite=%d", curl_nwrite);
					strncpy( segLastName, segName, sizeof(segLastName)-1 );
				}
			}
			/* 3.!! In some cases, time stamp is NOT consistent with former URL list, strcmp() will fail!???  */
			else if( access("/tmp/a.aac",F_OK)!=0  && access("/tmp/b.aac",F_OK)!=0
				 && strcmp(segName, segLastName) > 0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_ERROR, "Case 3: a.aac & b.aac both missing! downloading a._aac/a.ts from: %s",  segURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3,timeout, //3,3
							 segURL, Is_AAC?"/tmp/a._aac":"/tmp/a.ts", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Case 3: Fail to easy_download a._aac/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._aac":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Case 3: Finsh remove /tmp/a._aac(or a.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_3. End parsing m3u8list, and starts a new http request...\n");

					return;
				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._aac", NULL);

					/* Rename to a.aac */
					rename("/tmp/a._aac","/tmp/a.aac");

					/* Rename to a.h264,  ---by test_helixaac  */
                                        //if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264","/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.aac updated! curl_nwrite=%d", curl_nwrite);
					strncpy( segLastName, segName, sizeof(segLastName)-1 );
				}
			}
			/* 4. Both OK */
			else if(access("/tmp/a.aac",F_OK)==0 && access("/tmp/b.aac",F_OK)==0)
				EGI_PLOG(LOGLV_CRITICAL, "a.aac and b.aac both available!\n");
			/* 5. Sequence reset?  */
			//else if( strcmp(segURL, segLastURL) <= 0 ) {
			else if( strcmp(segName, segLastName) <= 0 ) {
				EGI_PLOG(LOGLV_CRITICAL, DBG_YELLOW"------ Sequence Error? -----\n"DBG_RESET);
				EGI_PLOG(LOGLV_CRITICAL, "segName: %s\n", segName);
				EGI_PLOG(LOGLV_CRITICAL, "segLastName: %s\n", segLastName);

				/* XXXTODO: If the next m3u8 list have overlapped URL items, then they will replay!
				 * NOPE: Reset segLastName[] upon #EXT-X-DISCONTINUITY tags.
				 */
				//segLastName[0]=0;
				//k--; continue;
			}
		}
		else /* NOT .aac OR .ts */ {
			egi_dpstd(DBG_YELLOW"Only support AAC/TS stream, fail to support URL: '%s'.\n"DBG_RESET,ps);
		}

		/* Get next URL */
		ps=strtok(NULL, delim);
	}

  /* TEST:  If whole list items  <= segLastName[] ---------------*/
  if( strcmp(segName, segLastName) <= 0 ) {
	printf(" ------ whole list items  < segLastName[] --------\n" );
	exit(0);
  }

}
#endif //////////////////////////////////////////////////////////////////////
