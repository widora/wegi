/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to parse and download AAC/TS m3u8 file.
AAC stream data to be saved as a.stream/b.stream.
H264 stream data to be save as a.h264/b.h264.

Note:
1. A small TARGETDURATION is good for A/V synchronization in this case?
2. As there's only 1 segment buffer file, it means each segment SHOULD
   be downloaded within TARGETDURATION.
3. XXX Complete URL strings in m3u8 list may NOT be in alphabetical/numberical
   (timestamp)sequence, however the URL source name of the SHOULD be in sequence.
   Example: 'hostname' and 'port' may be variable.
   ...
   http://118.188.180.180:8080/fdfdfd/live/123-120.ts
   http://118.177.160.150:5050/fdfdfd/live/123-130.ts
   http://118.177.160.151:5050/fdfdfd/live/123-140.ts
   ...
   XXX   ----- NOT NECESSARY! -----

4. Current and the next m3u8 list may have overlapped URL items.
   TODO: If reset regLastName[] when regName[]<regLastName[], then overlapped
	segments will be played twice!

		<<< MultiThread EasyDownlaoding >>>
5. This program supports downloading ONE remote file with MULTIPLE threads

		<<< MultiURLs EasyDownloading >>
6. This program supports downloading MULTIPLE remote files simultaneously
   with ONE thread for ONE file. (Enable_mURL_Download)
   Suggest to apply when segment TARGETDURATION < 5s.

TODO:
1. For some m3u8 address, it sometimes exits accidently when calling https_easy_download().
2. Too big TARGETDURATION value(10) causes disconnection/pause between
   playing two segments, for it's too long time for downloading a segment?
3. If too big timeout value, then ignore following items and skip to NEXT m3u8 list.

Journal:
2021-09-29:
	1. main():  memset buff[] before https_curl_request().
	2. parse_m3u8list(): If https_easy_download() fails, return immediately!
2022-06-29:
	1. For case *.ts file in m3u8.
2022-07-02:
	1. Extract AAC from TS.
2022-07-03:
	1. Save as a._stream/b._stream, then rename to a.stream/b.stream
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

char strRequest[256+64];

/* For mURL_Download */
bool Enable_mURL_Download=false;   /* To enable downloading multiple files simultaneously
				    * Auto. set TURE if mlist->maxDuraion<5.0
				    */
int  	nurls=6; /* Number of URLs(Files) downloading at same time */
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



/* #EXT-X-ENDLIST */
bool isEndList;	 /* The last list for the remote file, no more media segments. */
/* #EXT-X-MEDIA-SEQUENCE */
int lastSeqNum;  /* Last media sequence number */

int curl_nwrite;   /* Bytes downloaded */
int mbcnt; 	   /* MBytes downloaded */

size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *userp);
void parse_m3u8list(char *strm3u);
char segLastURL[1024];  /* The last URL for (aac or ts) segment */
char* segName; /* segment file name at URL (aac or ts) */
char segLastName[1024];  /* Segment file name of the URL (aac or ts) */
char* dirURL; /* Dir URL of input m3u8 address */
char* hostName;
char* protocolName;

/* Functions */
int decrypt_file(const char *fin, const char *fkey, const char *strIV);


/*----------------------------
            MAIN
----------------------------*/
int main(int argc, char **argv)
{
	char buff[CURL_RETDATA_BUFF_SIZE];
	int opt;
	char *argURL;
        while( (opt=getopt(argc,argv,"hn:s:"))!=-1 ) {
                switch(opt) {
                        case 'h':
                                printf("%s: -hn:s:\n", argv[0]);
                                exit(0);
                                break;
                        case 'n':
                                nthreads=atoi(optarg);
				if(nthreads<1)
					nthreads=1;
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



while(1) {
	/* Reset buff */
	memset(buff, 0, sizeof(buff));

	/* Https GET request */
	EGI_PLOG(LOGLV_INFO,"Start https curl request...");
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_ENABLE_REDIRECT, 3,30, /* HK2022-07-03 */
				strRequest, buff, NULL, curl_callback) !=0 )
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

#if 0 /* TEST: --------------- */
	printf("\n%s\n",buff);
	EGI_M3U8_LIST *mlist=m3u_parse_simple_HLS(buff);
	if(mlist) printf("mlist->ns=%d\n", mlist->ns);
	exit(0);
#endif

	/* Parse content */
	EGI_PLOG(LOGLV_INFO,"Start parse m3u8list...");
	parse_m3u8list(buff);

	/* Check ENDLIST */
	if(isEndList)
		break;

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

	/* 1A. Get isEndList before releasing mlist. */
	isEndList = mlist->isEndList;

	/* 1B. Set Enable_mURL_Download */
	if(mlist->maxDuration <5.0)
		Enable_mURL_Download=true;
	else
		Enable_mURL_Download=false;

	/* 1C. Create charList for mURL_Download */
	if( Enable_mURL_Download ) {
		strURL=egi_create_charList(nurls,EGI_URL_MAX);
		filenames=egi_create_charList(nurls, EGI_PATH_MAX); //+EGI_NAME_MAX);
		if(strURL==NULL || filenames==NULL)
			exit(1);
	}


	/* 2. Skip if all old itmes */
	if(lastSeqNum >= mlist->seqnum+mlist->ns-1) {
		egi_dpstd(DBG_CYAN" <----- Old List ----->\n"DBG_RESET);
		m3u_free_list(&mlist);
		return;
	}

	/* 3. Get startIndx <------------ */
	if(mlist->seqnum==0) {
		startIndx=0;   /* Reset */
		lastSeqNum=0;  /* Reset */

		/* Skip segments */
		if(tss>0) {
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

	printf("startIndx=%d\n", startIndx);

	/* 4. Proceed each URI */
	for(k=startIndx; k < mlist->ns;  k++) {
		egi_dpstd(DBG_BLUE"startIndx=%d, k=%d/%d\n"DBG_RESET, startIndx, k, mlist->ns);
		ps=mlist->msURI[k];
		printf(DBG_YELLOW"Sequence %d, ps: %s\n"DBG_RESET, mlist->seqnum+k, ps);

		/* 4.1 Check segment type */
		Is_AAC=Is_TS=false;
		if( strstr(ps,"aac") && strstr(ps,"//") )
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
			EGI_PLOG(LOGLV_INFO, "sub URL: '%s'.",ps);

			/* Get full segment URL */
			free(segURL); segURL=NULL;
			segURL=egi_URI2URL(strRequest, ps);
			egi_dpstd(DBG_BLUE"Stream URL: %s\n"DBG_RESET, segURL);

	///////////////*   https_easy_mURLDownload()   */////////////////
	if(Enable_mURL_Download && Is_TS ) {
		/* M1. Get URLs */
		strURL[uindx][0]=0;
		strcat(strURL[uindx],segURL);

		/* M2. Count URLs */
		uindx ++;
		if(uindx!=nurls)
			continue;
		else
			uindx=0;

		/* M3. mURL Downloading */
   		ret=https_easy_mURLDownload(nurls, (const char**)strURL, "/tmp/4seg.ts",   /* n, **urls, file_save */
			    HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
                                                         | HTTPS_ENABLE_REDIRECT, 1, timeout); /* opt, trys, timeout */
		if(ret) {
			/* Rewind k */
			k -=nurls;
			continue;
		}

		/* M4. Create c.ts */
		FILE *fil=fopen("/tmp/c.ts", "wb");
		if(fil==NULL) {
			egi_dperr("fopen /tmp/c.ts");
			exit(2);
		}

		/* M5. Decrypt files and merge into 'c.ts' */
		int i;
		for(i=0; i<nurls; i++) {
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

			/* Extract segName from URL */
			free(segName); segName=NULL;
			cstr_parse_URL(segURL, NULL, NULL, NULL, NULL, &segName, NULL, NULL);
			//printf(DBG_GREEN"segName: %s  <<<<<<  segLastName: %s\n"DBG_RESET, segName, segLastName);

			/* F1. Check and download a.stream */
			if( access("/tmp/a.stream",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading a.stream/a.ts from: %s",  segURL);
				if(Enable_mURL_Download && Is_TS ) {  /* mURL Download */
					rename("/tmp/c.ts", "/tmp/a.ts");
					ret=0;
				}
				else if(nthreads==1) // || !Enable_mThread_Download)  /* Easy download */
		                	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3, timeout,  //3,3
								 segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts", NULL, download_callback);
				else /* Multi_thread easy download */
			        	ret=https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, nthreads, 0, timeout, /* threads, trys, timeout */
                                    				segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts");

				if(ret!=0)
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download a._stream/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._stream":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/a._stream(or a.ts.");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_1. End parsing m3u8list, and starts a new http request...\n");
					continue; //break; //return;
				} else {

					if(Is_TS)  {
					    /* Decrypt file, mURL_Download already decrypted. */
					    if(mlist->keyURI[0]!=NULL && !Enable_mURL_Download)
						  decrypt_file("/tmp/a.ts", TMP_KEY_FPATH, mlist->strIV[k]);

					    /* Extract AAC from TS */
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._stream", "/tmp/a._h264");
					}

					/* Rename to a.stream */
					printf("rename a._stream to a.stream\n");
					rename("/tmp/a._stream","/tmp/a.stream");

					/* Rename to a.h264,  ---by test_helixaac  */
					//if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264", "/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.stream updated! curl_nwrite=%d", curl_nwrite);
				}
			}

			/* F2. Check and download b.stream */
			else if( access("/tmp/b.stream",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading b._stream/b.ts from: %s",  segURL);
				if(Enable_mURL_Download && Is_TS ) {  /* mURL Download */
					rename("/tmp/c.ts", "/tmp/b.ts");
					ret=0;
				}
			        else if(nthreads==1) // || !Enable_mThread_Download)  /* Easy download */
		                	ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT , 3, timeout, //3,3
								 segURL, Is_AAC?"/tmp/b._stream":"/tmp/b.ts", NULL, download_callback);
			   	else /* Multi_thread easy download */
			        	ret=https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, nthreads, 0, timeout, /* threads, trys, timeout */
                                    				segURL, Is_AAC?"/tmp/b._stream":"/tmp/b.ts");

			        if(ret!=0)
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download b._stream/b.ts from '%s'.", segURL);

					remove(Is_AAC?"/tmp/b._stream":"/tmp/b.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/b._stream(or b.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_2. End parsing m3u8list, and starts a new http request...");
					continue; //break; //return;

				} else {

					if(Is_TS)  {
					    /* Decrypt file, mURL_Download already decrypted. */
					    if(mlist->keyURI[0]!=NULL && !Enable_mURL_Download)
						  decrypt_file("/tmp/b.ts", TMP_KEY_FPATH, mlist->strIV[k]);

					    /* Extract AAC from TS */
					    egi_extract_AV_from_ts("/tmp/b.ts", "/tmp/b._stream", "/tmp/b._h264");
					}

					/* Rename to a.stream */
					printf("rename b._stream to b.stream\n");
					rename("/tmp/b._stream","/tmp/b.stream");

                                        /* Rename to b.h264 ---by test_helixaac */

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, b.stream updated! curl_nwrite=%d", curl_nwrite);
				}
			}
			/* F3.!! In some cases, time stamp is NOT consistent with former URL list, strcmp() will fail!???  */
			else if( access("/tmp/a.stream",F_OK)!=0  && access("/tmp/b.stream",F_OK)!=0 ) {
				// && strcmp(segName, segLastName) > 0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_ERROR, "Case 3: a.stream & b.stream both missing! downloading a._stream/a.ts from: %s",  segURL);
				if(Enable_mURL_Download && Is_TS ) {  /* mURL Download */
					rename("/tmp/c.ts", "/tmp/a.ts");
					ret=0;
				}
				else if(nthreads==1) // || !Enable_mThread_Download) /* Easy download */
			                ret=https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3,timeout, //3,3
								 segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts", NULL, download_callback);
				else /* Multi_thread easy download */
			        	ret=https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, 2, nthreads, timeout, /* threads, trys, timeout */
                                    				segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts");

				if(ret!=0)
				{
                        		EGI_PLOG(LOGLV_ERROR, "Case 3: Fail to easy_download a._stream/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._stream":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Case 3: Finsh remove /tmp/a._stream(or a.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_3. End parsing m3u8list, and starts a new http request...");
					continue; //break; //return;
				} else {

					if(Is_TS)  {
					    /* Decrypt file, mURL_Download already decrypted. */
					    if(mlist->keyURI[0]!=NULL && !Enable_mURL_Download)
						  decrypt_file("/tmp/a.ts", TMP_KEY_FPATH, mlist->strIV[k]);

					    /* Extract AAC from TS */
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._stream", "/tmp/a._h264");
					}

					/* Rename to a.stream */
					printf("rename2 a._stream to a.stream\n");
					rename("/tmp/a._stream","/tmp/a.stream");

					/* Rename to a.h264,  ---by test_helixaac  */
                                        //if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264","/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok2, a.stream updated! curl_nwrite=%d", curl_nwrite);
				}
			}
			/* F4. Both OK */
			else if(access("/tmp/a.stream",F_OK)==0 && access("/tmp/b.stream",F_OK)==0) {
				EGI_PLOG(LOGLV_CRITICAL, "a.stream and b.stream both available!");

				/* Rewind back */
				do {
					if(mlist->tsec[k]>4.0)
				   		sleep(1);
					else
				   		usleep(200000);
				} while( access("/tmp/a.stream",F_OK)==0 && access("/tmp/b.stream",F_OK)==0);

				/* Rewind k */
				if(!Enable_mURL_Download)
					k--;
			}

			/* F5. Sequence reset?  */
			//else if( strcmp(segName, segLastName) <= 0 ) {
			//	EGI_PLOG(LOGLV_CRITICAL, DBG_YELLOW"------ Old Sequence -----\n"DBG_RESET);
			//	EGI_PLOG(LOGLV_CRITICAL, "segName: %s\n", segName);
			//	EGI_PLOG(LOGLV_CRITICAL, "segLastName: %s\n", segLastName);
			//}

		}
		else /* NOT .aac OR .ts */ {
			egi_dpstd(DBG_YELLOW"Only support AAC/TS stream, fail to support URL: '%s'.\n"DBG_RESET,ps);
		}

	} /* END for(K) */

	/* Renew lastSeqNum */
	printf("Renew lastSeqNum....\n");
	lastSeqNum = mlist->seqnum+mlist->ns-1;

	/* Free charList */
	egi_free_charList(&strURL, nurls);
	egi_free_charList(&filenames, nurls);

	/* Free mlist */
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

			/* 1. Check and download a.stream */
			//if( access("/tmp/a.stream",F_OK)!=0 && strcmp(segURL, segLastURL) > 0) {
			if( access("/tmp/a.stream",F_OK)!=0 && strcmp(segName, segLastName) > 0) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading a.stream/a.ts from: %s",  segURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3, timeout,  //3,3
							 segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download a._stream/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._stream":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/a._stream(or a.ts.");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_1. End parsing m3u8list, and starts a new http request...\n");
					return;
				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._stream", "/tmp/a._h264");

					/* Rename to a.stream */
					rename("/tmp/a._stream","/tmp/a.stream");

					/* Rename to a.h264,  ---by test_helixaac  */
					//if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264", "/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.stream updated! curl_nwrite=%d", curl_nwrite);
					strncpy( segLastName, segName, sizeof(segLastName)-1 );
				}
			}

			/* 2. Check and download b.stream */
			//else if( access("/tmp/b.stream",F_OK)!=0 && strcmp(segURL, segLastURL) > 0) {
			else if( access("/tmp/b.stream",F_OK)!=0 && strcmp(segName, segLastName) > 0) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading b._stream/b.ts from: %s",  segURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT , 3, timeout, //3,3
							 segURL, Is_AAC?"/tmp/b._stream":"/tmp/b.ts", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download b._stream/b.ts from '%s'.", segURL);

					remove(Is_AAC?"/tmp/b._stream":"/tmp/b.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/b._stream(or b.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_2. End parsing m3u8list, and starts a new http request...\n");
					return;

				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/b.ts", "/tmp/b._stream", "/tmp/b._h264");

					/* Rename to a.stream */
					rename("/tmp/b._stream","/tmp/b.stream");

                                        /* Rename to b.h264 ---by test_helixaac */
                                        //if(access("/tmp/b.h264",F_OK)!=0 )
                                        //        rename("/tmp/b._h264", "/tmp/b.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, b.stream updated! curl_nwrite=%d", curl_nwrite);
					strncpy( segLastName, segName, sizeof(segLastName)-1 );
				}
			}
			/* 3.!! In some cases, time stamp is NOT consistent with former URL list, strcmp() will fail!???  */
			else if( access("/tmp/a.stream",F_OK)!=0  && access("/tmp/b.stream",F_OK)!=0
				 && strcmp(segName, segLastName) > 0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_ERROR, "Case 3: a.stream & b.stream both missing! downloading a._stream/a.ts from: %s",  segURL);
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3,timeout, //3,3
							 segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts", NULL, download_callback) !=0 )
				{
                        		EGI_PLOG(LOGLV_ERROR, "Case 3: Fail to easy_download a._stream/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._stream":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Case 3: Finsh remove /tmp/a._stream(or a.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_3. End parsing m3u8list, and starts a new http request...\n");

					return;
				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/a.ts", "/tmp/a._stream", NULL);

					/* Rename to a.stream */
					rename("/tmp/a._stream","/tmp/a.stream");

					/* Rename to a.h264,  ---by test_helixaac  */
                                        //if(access("/tmp/a.h264",F_OK)!=0 )
					//	rename("/tmp/a._h264","/tmp/a.h264");

					/* Update segLastURL */
					EGI_PLOG(LOGLV_INFO, "Ok, a.stream updated! curl_nwrite=%d", curl_nwrite);
					strncpy( segLastName, segName, sizeof(segLastName)-1 );
				}
			}
			/* 4. Both OK */
			else if(access("/tmp/a.stream",F_OK)==0 && access("/tmp/b.stream",F_OK)==0)
				EGI_PLOG(LOGLV_CRITICAL, "a.stream and b.stream both available!\n");
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
