/*--------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example to parse and download AAC m3u8 file.
AAC stream data to be saved as a.stream/b.stream.
H264 stream data to be save as a.h264/b.h264.

Note:
1. A small TARGETDURATION is good for A/V synchronization in this case?
2. As there's only 1 segment buffer file, it means each segment SHOULD
   be downloaded within TARGETDURATION.
3. Complete URL strings in m3u8 list may NOT be in alphabetical/numberical
   (timestamp)sequence, however the URL source name of the SHOULD be in sequence.
   Example: 'hostname' and 'port' may be variable.
   ...
   http://118.188.180.180:8080/fdfdfd/live/123-120.ts
   http://118.177.160.150:5050/fdfdfd/live/123-130.ts
   http://118.177.160.151:5050/fdfdfd/live/123-140.ts
   ...
4. Current and the next m3u8 list may have overlapped URL items.
   TODO: If reset regLastName[] when regName[]<regLastName[], then overlapped
	segments will be played twice!

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

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
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

int main(int argc, char **argv)
{
	char buff[CURL_RETDATA_BUFF_SIZE];
	char strRequest[256+64];

/* TODO test */
//	cstr_split_nstr(strm3u, split, )

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

	/* Init strRequest */
        strcat(strRequest,argv[1]);
        EGI_PLOG(LOGLV_CRITICAL,"Http request head:\n %s\n", strRequest);

	/* Get dirURL */
	/* URL, **protocol, **hostname, *port, **path, **filename, **dir, **dirURL */
	cstr_parse_URL(strRequest, &protocolName, &hostName, NULL, NULL, NULL, NULL, &dirURL);
	printf("protocolName: %s\n hostName: %s\n dirULR: %s\n", protocolName,hostName,dirURL);

while(1) {
	/* Reset buff */
	memset(buff, 0, sizeof(buff));

	/* Https GET request */
	EGI_PLOG(LOGLV_INFO,"Start https curl request...");
        if( https_curl_request( HTTPS_SKIP_PEER_VERIFICATION|HTTPS_SKIP_HOSTNAME_VERIFICATION|HTTPS_ENABLE_REDIRECT, 5,3, /* HK2022-07-03 */
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

	/* Parse content */
	EGI_PLOG(LOGLV_INFO,"Start parse m3u8list...");
	parse_m3u8list(buff);

	/* Sleep of TARGETDURATION --- If too small, it will fetch the same segment? */
	//sleep(7/2);
	usleep(500000);
}

	free(dirURL);
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
        strcat(userp, ptr);

        return session_size;
}

/*-----------------------------------------------
 A callback function for write data to file.

 TODO: Check buff space.
------------------------------------------------*/
size_t download_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
        size_t written;
	int tmp;

        written = fwrite(ptr, size,  nmemb, (FILE *)stream);
        //printf("%s: written size=%zd\n",__func__, written);

	if(written<0) {
		EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  written<0!  xxxxx");
		written=0;
	}
	curl_nwrite += written;

#if 1
	tmp=curl_nwrite/1024/1024;
	if(mbcnt != tmp) {
		mbcnt = tmp;
		EGI_PLOG(LOGLV_CRITICAL,"download ~%dMBytes", mbcnt);
	}
#else
	EGI_PLOG(LOGLV_CRITICAL," curl_nwritten=%d ", curl_nwrite);
#endif

        return written;
}


/*---------------------------------------------
Parse m3u8 content

Note:
1. Content in strm3u will be changed!
--------------------------------------------*/
#if 0 /////////////////////////////////////////////////////////////////
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

#else ///////////////// EGI_M3U8_LIST* m3u_parse_simple_HLS()  ///////////////////////////////

void parse_m3u8list(char *strm3u)
{
	int k;
	char *ps;
	char segURL[1024]={0}; /* URL item in m3u8 list */
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

	/* 2. Skip if all old itmes */
	if(lastSeqNum >= mlist->seqnum+mlist->ns-1) {
		egi_dpstd(DBG_CYAN" <----- Old List ----->\n"DBG_RESET);
		m3u_free_list(&mlist);
		return;
	}

	/* 3. Get startIndx */
	if(mlist->seqnum==0) {
		startIndx=0;   /* Reset */
		lastSeqNum=0;  /* Reset */
	}
	else if(lastSeqNum < mlist->seqnum) {
		startIndx=0;
	}
	else if(lastSeqNum >= mlist->seqnum) {
		startIndx=lastSeqNum-mlist->seqnum +1;
	}

	printf("startIndx=%d\n", startIndx);

#if 0   ////////////* TEST: ------------ Proceed each URI */////////////
	for(k=startIndx; k < mlist->ns;  k++) {
		ps=mlist->msURI[k];
		printf("ps: %s\n", ps);
	}

	/* Renew lastSeqNum */
	lastSeqNum = mlist->seqnum+mlist->ns-1;

	/* Free mlist */
	m3u_free_list(&mlist);

	sleep(8);
return;
#endif  ///////////////////////////////////////////////////////////////

	/* 4. Proceed each URI */
	for(k=startIndx; k < mlist->ns;  k++) {
		ps=mlist->msURI[k];
		printf(DBG_YELLOW"Sequence %d, ps: %s\n"DBG_RESET, mlist->seqnum+k, ps);
//		sleep(1);

		/* 4.1 Check segment type */
		Is_AAC=Is_TS=false;
		if( strstr(ps,"aac") && strstr(ps,"//") )
			Is_AAC=true;
		else if( strstr(ps, ".ts") )
			Is_TS=true;
		else
			Is_TS=true;

		/* 4.2 Parse AAC or TS segment file */
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
			//printf(DBG_GREEN"segName: %s  <<<<<<  segLastName: %s\n"DBG_RESET, segName, segLastName);

			/* F1. Check and download a.stream */
			//if( access("/tmp/a.stream",F_OK)!=0 && strcmp(segName, segLastName) > 0) {
			if( access("/tmp/a.stream",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading a.stream/a.ts from: %s",  segURL);
		#if 0  /* Easy download */
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3, timeout,  //3,3
							 segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts", NULL, download_callback) !=0 )
		#else /* Multi_thread easy download */
			        if( https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, 2, 0, timeout, /* threads, trys, timeout */
                                    			segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts") !=0 )
		#endif
				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download a._stream/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._stream":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/a._stream(or a.ts.");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_1. End parsing m3u8list, and starts a new http request...\n");
					break; //return;
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
				}
			}

			/* F2. Check and download b.stream */
			//else if( access("/tmp/b.stream",F_OK)!=0 && strcmp(segName, segLastName) > 0) {
			else if( access("/tmp/b.stream",F_OK)!=0 ) {
				/* Download AAC */
				curl_nwrite=0; mbcnt=0;
				EGI_PLOG(LOGLV_INFO, "Downloading b._stream/b.ts from: %s",  segURL);
		#if 0  /* Easy download */
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT , 3, timeout, //3,3
							 segURL, Is_AAC?"/tmp/b._stream":"/tmp/b.ts", NULL, download_callback) !=0 )
		#else /* Multi_thread easy download */
			        if( https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, 2, 0, timeout, /* threads, trys, timeout */
                                    			segURL, Is_AAC?"/tmp/b._stream":"/tmp/b.ts") !=0 )
		#endif

				{
                        		EGI_PLOG(LOGLV_ERROR, "Fail to easy_download b._stream/b.ts from '%s'.", segURL);

					remove(Is_AAC?"/tmp/b._stream":"/tmp/b.ts");
					EGI_PLOG(LOGLV_INFO, "Finish remove /tmp/b._stream(or b.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_2. End parsing m3u8list, and starts a new http request...\n");
					break; //return;

				} else {
					/* Extract AAC from TS */
					if(Is_TS)
					    egi_extract_AV_from_ts("/tmp/b.ts", "/tmp/b._stream", "/tmp/b._h264");

					/* Rename to a.stream */
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
		#if 0 /* Easy download */
		                if( https_easy_download( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
										      | HTTPS_ENABLE_REDIRECT, 3,timeout, //3,3
							 segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts", NULL, download_callback) !=0 )
		#else /* Multi_thread easy download */
			        if( https_easy_mThreadDownload( HTTPS_SKIP_PEER_VERIFICATION | HTTPS_SKIP_HOSTNAME_VERIFICATION
								     | HTTPS_ENABLE_REDIRECT, 2, 0, timeout, /* threads, trys, timeout */
                                    			segURL, Is_AAC?"/tmp/a._stream":"/tmp/a.ts") !=0 )
		#endif
				{
                        		EGI_PLOG(LOGLV_ERROR, "Case 3: Fail to easy_download a._stream/a.ts from '%s'.", segURL);
					remove(Is_AAC?"/tmp/a._stream":"/tmp/a.ts");
					EGI_PLOG(LOGLV_INFO, "Case 3: Finsh remove /tmp/a._stream(or a.ts).");

					/* !!! Break NOW, in case next acc url NOT available either, as downloading time goes by!!! */
					EGI_PLOG(LOGLV_INFO, "Case_3. End parsing m3u8list, and starts a new http request...\n");
					break; //return;
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
				}
			}
			/* F4. Both OK */
			else if(access("/tmp/a.stream",F_OK)==0 && access("/tmp/b.stream",F_OK)==0)
				EGI_PLOG(LOGLV_CRITICAL, "a.stream and b.stream both available!\n");

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

	}

	/* Renew lastSeqNum */
	lastSeqNum = mlist->seqnum+mlist->ns-1;

	/* Free mlist */
	m3u_free_list(&mlist);
}

#endif
