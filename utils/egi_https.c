/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

		A libcurl http helper
Refer to:  https://curl.haxx.se/libcurl/c/

Note:
1. Unknown SSL protocol error:
   Old version of CURL and OpenSSL do not support some new SSL
   protocols(with elliptic-curve key etc.), Update to CURL 7.68
   and OpenSSL 1.1.1b...
2. Repeated call to curl_global_init and curl_global_cleanup should
   be avoided.
3. (MT7688 OPenwrt) A BLUETooth USB dongle (for mouse/keyboard etc.)
   MAY (clog/hamper) (CURL/http/wifi?) connecting/downloading!
   Just unplug it if such case arises!
4. Shorter segments will trigger more rounds of Easy_downloads, and more
   CPU/Resoruces consumed(?). In such case, it's a good idea to disable
   mthread_download.

Jurnal:
2021-02-09:
	1. https_easy_download(): Add input param 'int opt' to replace
           definition of SKIP_xxx_VERIFICATION.
2021-02-10:
	1. https_curl_request(): Add input param 'int opt' to replace
           definition of SKIP_xxx_VERIFICATION.
2021-02-13:
	1. https_easy_download(): Add flock(,LOCK_EX) during downloading...
	   			  Add fsync() after fwrite() all data!
				  Add truncate() if filesize not right.
2021-02-21:
	1. https_easy_download(): Re_truncate file to 0 if download fails.
2021-03-23:
	1. Reset curl to NULL after curl_easy_cleanup(curl) !!!
2021-09-29:
	1.https_curl_request()/_easy_download(): Append HTTP request header.
2021-11-11:
	1. curl_easy_strerror()
	2. Add https_easy_download2()
2021-11-14:
        1. Add https_easy_stream()
2021-11-20:
	1. https_easy_stream( ..nr, reqs..): additional request lines.
2021-11-24:
	1. https_curl_request(): add param timeout.
	2. https_easy_download(): add param timeout.

2021-11-30:
	1.Add CURLOPT_ACCEPT_ENCODING to enables automatic decompression of HTTP downloads.
		https_curl_request()
2021-12-02:
	1. https_easy_download(): Enable CURLOPT_ACCEPT_ENCODING.
2021-12-15:
	1. https_curl_request():
	   1.1 Add header append: Content-Type:charset=UTF-8
	   1.2 curl_slist_free_all(header)
2021-12-20:
	1. https_easy_download(): opt to save by appending to the file.
2021-12-21:
	1. https_curl_request(): opt to follow HTTP 3xx redirects.
2021-12-27:
	1. Check if curl_global_init() does NOT return CURLE_OK.
	2. Each time curl_easy_init() fails, call curl_global_cleanup() then.
2021-12-28:
	1. Set header to NULL after curl_slist_free_all(header)!
2022-01-01:
	1. https_easy_download():  Enable option CURLOPT_HTTPGET.
2022-03-21:
	1. Add fflush(fb) before fsync(fileno(fp));
2022-04-09:
	1. Add https_easy_buff[] and easy_callback_copyToBuffer().
2022-07-16:
	1. For MutiThread_Easy_Downloading:  EASY_MTHREADDOWN_CTX
	   easy_callback_mThreadWriteToFile()
	   https_easy_getFileSiz()
2022-07-17:
	1. Add  https_easy_mThreadDownload()
2022-07-21:
	1. Add https_easy_mURLDownload()

TODO:
XXX 1. fstat()/stat() MAY get shorter/longer filesize sometime? Not same as doubleinfo, OR curl BUG?
    compressed data size  ---OK, compression data.

XXX 2. After looping curl_global_init() +..+ curl_global_cleanup() with enough circle times, it will
   unexpectedly throw out 'Aborted' and exit just at calling curl_global_init()!

3. Set TCP KEEPALIVE option, and DO NOT clean up curl at end of each session,
   try to REUSE curl, to SAVE init time. ----> for streams only.
   MidasHK_2022_04_01

4. To check HTTP response status code! --- use void *data to pass out.
   Downloading session may still fail though function returns 0!
   Example: Status code 404, Resource NOT found.  MidasHK_2022_04_01


Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
--------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <curl/curl.h>
#include "egi_https.h"
#include "egi_log.h"
#include "egi_timer.h"
#include "egi_debug.h"
#include "egi_utils.h"

#define EGI_CURL_TIMEOUT	3  /* 15 in seconds. tricky for M3U, abt. SegDuration/3trys */

/*--------------------------------
This is from CURL lib/strerror.c
---------------------------------*/
const char *curl_easy_strerror(CURLcode error)
{
  switch (error) {
  case CURLE_OK:
    return "No error";

  case CURLE_UNSUPPORTED_PROTOCOL:
    return "Unsupported protocol";

  case CURLE_FAILED_INIT:
    return "Failed initialization";

  case CURLE_URL_MALFORMAT:
    return "URL using bad/illegal format or missing URL";

  case CURLE_NOT_BUILT_IN:
    return "A requested feature, protocol or option was not found built-in in"
      " this libcurl due to a build-time decision.";

  case CURLE_COULDNT_RESOLVE_PROXY:
    return "Couldn't resolve proxy name";

  case CURLE_COULDNT_RESOLVE_HOST:
    return "Couldn't resolve host name";

  case CURLE_COULDNT_CONNECT:
    return "Couldn't connect to server";

  case CURLE_FTP_WEIRD_SERVER_REPLY:
    return "FTP: weird server reply";

  case CURLE_REMOTE_ACCESS_DENIED:
    return "Access denied to remote resource";

  case CURLE_FTP_ACCEPT_FAILED:
    return "FTP: The server failed to connect to data port";

  case CURLE_FTP_ACCEPT_TIMEOUT:
    return "FTP: Accepting server connect has timed out";

  case CURLE_FTP_PRET_FAILED:
    return "FTP: The server did not accept the PRET command.";

  case CURLE_FTP_WEIRD_PASS_REPLY:
    return "FTP: unknown PASS reply";

  case CURLE_FTP_WEIRD_PASV_REPLY:
    return "FTP: unknown PASV reply";

  case CURLE_FTP_WEIRD_227_FORMAT:
    return "FTP: unknown 227 response format";

  case CURLE_FTP_CANT_GET_HOST:
    return "FTP: can't figure out the host in the PASV response";

  case CURLE_HTTP2:
    return "Error in the HTTP2 framing layer";

  case CURLE_FTP_COULDNT_SET_TYPE:
    return "FTP: couldn't set file type";

  case CURLE_PARTIAL_FILE:
    return "Transferred a partial file";

  case CURLE_FTP_COULDNT_RETR_FILE:
    return "FTP: couldn't retrieve (RETR failed) the specified file";

  case CURLE_QUOTE_ERROR:
    return "Quote command returned error";

  case CURLE_HTTP_RETURNED_ERROR:
    return "HTTP response code said error";

  case CURLE_WRITE_ERROR:
    return "Failed writing received data to disk/application";

  case CURLE_UPLOAD_FAILED:
    return "Upload failed (at start/before it took off)";

  case CURLE_READ_ERROR:
    return "Failed to open/read local data from file/application";

  case CURLE_OUT_OF_MEMORY:
    return "Out of memory";

  case CURLE_OPERATION_TIMEDOUT:
    return "Timeout was reached";

  case CURLE_FTP_PORT_FAILED:
    return "FTP: command PORT failed";

  case CURLE_FTP_COULDNT_USE_REST:
    return "FTP: command REST failed";

  case CURLE_RANGE_ERROR:
    return "Requested range was not delivered by the server";

  case CURLE_HTTP_POST_ERROR:
    return "Internal problem setting up the POST";

  case CURLE_SSL_CONNECT_ERROR:
    return "SSL connect error";

  case CURLE_BAD_DOWNLOAD_RESUME:
    return "Couldn't resume download";

  case CURLE_FILE_COULDNT_READ_FILE:
    return "Couldn't read a file:// file";

  case CURLE_LDAP_CANNOT_BIND:
    return "LDAP: cannot bind";

  case CURLE_LDAP_SEARCH_FAILED:
    return "LDAP: search failed";

  case CURLE_FUNCTION_NOT_FOUND:
    return "A required function in the library was not found";

  case CURLE_ABORTED_BY_CALLBACK:
    return "Operation was aborted by an application callback";

  case CURLE_BAD_FUNCTION_ARGUMENT:
    return "A libcurl function was given a bad argument";

  case CURLE_INTERFACE_FAILED:
    return "Failed binding local connection end";

  case CURLE_TOO_MANY_REDIRECTS :
    return "Number of redirects hit maximum amount";

  case CURLE_UNKNOWN_OPTION:
    return "An unknown option was passed in to libcurl";

  case CURLE_TELNET_OPTION_SYNTAX :
    return "Malformed telnet option";

  case CURLE_PEER_FAILED_VERIFICATION:
    return "SSL peer certificate or SSH remote key was not OK";

  case CURLE_GOT_NOTHING:
    return "Server returned nothing (no headers, no data)";

  case CURLE_SSL_ENGINE_NOTFOUND:
    return "SSL crypto engine not found";

  case CURLE_SSL_ENGINE_SETFAILED:
    return "Can not set SSL crypto engine as default";

  case CURLE_SSL_ENGINE_INITFAILED:
    return "Failed to initialise SSL crypto engine";

  case CURLE_SEND_ERROR:
    return "Failed sending data to the peer";

  case CURLE_RECV_ERROR:
    return "Failure when receiving data from the peer";

  case CURLE_SSL_CERTPROBLEM:
    return "Problem with the local SSL certificate";

  case CURLE_SSL_CIPHER:
    return "Couldn't use specified SSL cipher";

  case CURLE_SSL_CACERT:
    return "Peer certificate cannot be authenticated with given CA "
      "certificates";

  case CURLE_SSL_CACERT_BADFILE:
    return "Problem with the SSL CA cert (path? access rights?)";

  case CURLE_BAD_CONTENT_ENCODING:
    return "Unrecognized or bad HTTP Content or Transfer-Encoding";

  case CURLE_LDAP_INVALID_URL:
    return "Invalid LDAP URL";

  case CURLE_FILESIZE_EXCEEDED:
    return "Maximum file size exceeded";

  case CURLE_USE_SSL_FAILED:
    return "Requested SSL level failed";

  case CURLE_SSL_SHUTDOWN_FAILED:
    return "Failed to shut down the SSL connection";

  case CURLE_SSL_CRL_BADFILE:
    return "Failed to load CRL file (path? access rights?, format?)";

  case CURLE_SSL_ISSUER_ERROR:
    return "Issuer check against peer certificate failed";

  case CURLE_SEND_FAIL_REWIND:
    return "Send failed since rewinding of the data stream failed";

  case CURLE_LOGIN_DENIED:
    return "Login denied";

  case CURLE_TFTP_NOTFOUND:
    return "TFTP: File Not Found";

  case CURLE_TFTP_PERM:
    return "TFTP: Access Violation";

  case CURLE_REMOTE_DISK_FULL:
    return "Disk full or allocation exceeded";

  case CURLE_TFTP_ILLEGAL:
    return "TFTP: Illegal operation";

  case CURLE_TFTP_UNKNOWNID:
    return "TFTP: Unknown transfer ID";

  case CURLE_REMOTE_FILE_EXISTS:
    return "Remote file already exists";

  case CURLE_TFTP_NOSUCHUSER:
    return "TFTP: No such user";

  case CURLE_CONV_FAILED:
    return "Conversion failed";

  case CURLE_CONV_REQD:
    return "Caller must register CURLOPT_CONV_ callback options";

  case CURLE_REMOTE_FILE_NOT_FOUND:
    return "Remote file not found";

  case CURLE_SSH:
    return "Error in the SSH layer";

  case CURLE_AGAIN:
    return "Socket not ready for send/recv";

  case CURLE_RTSP_CSEQ_ERROR:
    return "RTSP CSeq mismatch or invalid CSeq";

  case CURLE_RTSP_SESSION_ERROR:
    return "RTSP session error";

  case CURLE_FTP_BAD_FILE_LIST:
    return "Unable to parse FTP file list";

  case CURLE_CHUNK_FAILED:
    return "Chunk callback failed";

  case CURLE_NO_CONNECTION_AVAILABLE:
    return "The max connection limit is reached";

  case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
    return "SSL public key does not match pinned public key";

  /* error codes not used by current libcurl */
  case CURLE_OBSOLETE20:
  case CURLE_OBSOLETE24:
  case CURLE_OBSOLETE29:
  case CURLE_OBSOLETE32:
  case CURLE_OBSOLETE40:
  case CURLE_OBSOLETE44:
  case CURLE_OBSOLETE46:
  case CURLE_OBSOLETE50:
  case CURLE_OBSOLETE57:
  case CURL_LAST:
  break;

  }

  return "Unknown error";
}

/*---------------------------------------------------
 A default callback function for https_curl_request()
----------------------------------------------------*/
char https_easy_buff[1024*1024];
static size_t easy_callback_copyToBuffer(void *ptr, size_t size, size_t nmemb, void *userp)
{
        int blen=strlen(https_easy_buff);
        int plen=strlen(ptr);
        int xlen=plen+blen;

        /* MUST check buff[] capacity! */
        if( xlen > sizeof(https_easy_buff)-1) {   // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
                /* "If src contains n or more bytes, strncat() writes n+1 bytes to dest
                 * (n from src plus the terminating null byte." ---man strncat
                 */
                fprintf(stderr,"\033[0;31;40m strlen(buff)=%d, strlen(buff)+strlen(ptr)=%d >=sizeof(buff)-1! \e[0m\n",
                                                        blen, xlen);
                strncat(userp, ptr, sizeof(https_easy_buff)-1-blen); /* 1 for '\0' by strncat */
                //strncat(buff, ptr, sizeof(buff)-1);
        }
        else {
            #if 1 /* strcat */
                //strcat(userp, ptr);
		strncat(userp, ptr, size*nmemb); /* HK2022-07-19  string data MAY have terminating NULL! */
                //buff[sizeof(buff)-1]='\0';  /* <---------- return data maybe error!!??  */
            #else /* memcpy */
                memcpy(userp, ptr, size*nmemb); // NOPE for compressed data?!!!
                userp[size*nmemb]='\0';
            #endif
        }

        return size*nmemb; /* Return size*nmemb OR curl will fails! */
}


/*------------------------------------------------------------------------------
			HTTPS request by libcurl

Note: You must have installed ca-certificates before call curl https, or
      define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION

@trys:			Max. try times.
			If trys==0; unlimit.
@timeout:		Timeout in seconds.
@request:	request string
@reply_buff:	Buffer for returned reply string, the Caller must ensure enough space.

		>>>> Applicable ONLY IF user get_callback is NOT NULL! <<<<

				!!! --- CAUTION --- !!!
		Insufficient memspace causes segmentation_fault and other strange things.
		In the get_callback() function, it MUST check available space left
		if reply_buff[], for each chunck session.
		Example:
		   static size_t get_callback(void *ptr, size_t size, size_t nmemb, void *userp)
		   {
			int xlen=strlen(ptr)+strlen(reply_buff);
        		if( xlen >= sizeof(reply_buff)-1) {
				strncat(userp, ptr, sizeof(reply_buff)-1-strlen(reply_buff));
				....  // OR: CURL_RETDATA_BUFF_SIZE-1-strlen(..)
			}
			else {
				strcat(userp, ptr);
				....
			}
		   }

@data:		TODO: if any more data needed
@get_callback:   If NULL use easy_callback_copyToBuffer() and https_easy_buff[]

		!!! CURL will disable egi tick timer? !!!
Return:
	0	Ok
	<0	Fails
--------------------------------------------------------------------------------*/
int https_curl_request( int opt, unsigned int trys, unsigned int timeout, const char *request, char *reply_buff,
			void *data, curlget_callback_t get_callback )
{
	int i;
	int ret=0;
  	CURL *curl;
  	CURLcode res=CURLE_OK;
	double doubleinfo=0;
	long   longinfo=0;
	struct curl_slist *header=NULL;

 /* Try Max. sessions. TODO: tm_delay() not accurate, delay time too short!  */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init ...",__func__);
	if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
		EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
	}

	EGI_PLOG(LOGLV_INFO, "%s: start curl_easy_init...",__func__);
	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
  		curl_global_cleanup();
		return -1;
	}

	/* Set curl options */
	EGI_PLOG(LOGLV_INFO, "%s: start to set curl options ...",__func__);
	curl_easy_setopt(curl, CURLOPT_URL, request);		 	 /* set request URL */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); //1L		 /* 1 print more detail */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); //EGI_CURL_TIMEOUT);	 /* set timeout */
	if(get_callback) {
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_callback);     /* set write_callback */
	    curl_easy_setopt(curl, CURLOPT_WRITEDATA, reply_buff); 		 /* set data dest for write_callback */
	}
	else {
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, easy_callback_copyToBuffer);
	    memset(https_easy_buff,0,sizeof(https_easy_buff));
	    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (char *)https_easy_buff); 		 /* set data dest for write_callback */
	}

	/* User options */
        if(HTTPS_ENABLE_REDIRECT & opt) {
        	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); /* Enable redirect */
                curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
//               curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
        }

	/* TODO:  unsupported compression method may produce corrupted data! */
	/* "" --- Containing all built-in supported encodings. */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
//	header=curl_slist_append(header,"User-Agent: Mozilla/5.0(Linux; Android 8.0)");
	header=curl_slist_append(header,"Accept-Charset: utf-8");
//	header=curl_slist_append(header,"Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Perform the request, res will get the return code */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);
	if(  (res=curl_easy_perform(curl)) !=CURLE_OK ) {
	       egi_dpstd("curl_easy_perform() failed! res=%d, Err'%s'\n",res, curl_easy_strerror(res));
		ret=-2;
		printf("%s: tm_delayms..\n",__func__);
		tm_delayms(200);
		printf("%s: try again...\n",__func__);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

	/* 				--- Check session info. ---
	 ***  Note: curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 */
/* TEST: ---> */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Start to get response code...", __func__);
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &longinfo) ) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: get response code '%ld'.",__func__, longinfo);
		if(longinfo==0) {
			egi_dpstd("No response from the peer/server!\n");
			ret=-2;
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
	}

#if 0 ///////////////// Decompressed data lenght MAY NOT same as doubleinfo size ///////////////
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &doubleinfo) ) {
		printf("%s: CURLINFO_CONTENT_LENGTH_DOWNLOAD = %.0f \n", __func__, doubleinfo);
		if( (int)doubleinfo > CURL_RETDATA_BUFF_SIZE )
		{
		  EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length > CURL_RETDATA_BUFF_SIZE!",
												   __func__);
			ret=-3;
			break;	/* BUFF overflow! */
		}
		/* Content length MAY BE invalid!!! */
		else if( (int)doubleinfo < 0 ) {
		  	EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length < 0!",  __func__);
			ret=-4;
			tm_delayms(200);
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
		else {
			ret=0;
		  	/* ----- OK! End Session ----- */
		}
	}
	else { 	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-5;
		tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}
#endif //////////////////////

CURL_CLEANUP:
	/* always cleanup */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL;
  	curl_global_cleanup();

	/* if succeeds --- OK --- */
	if(ret==0)
		break;

 } /* End: try Max.3 times */

	if(ret!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail after try %d times!", __func__, i);


	return ret;
}



/*-----------------------------------------------
 A default callback for https_easy_download().
------------------------------------------------*/
static size_t easy_callback_writeToFile(void *ptr, size_t size, size_t nmemb, void *stream)
{
   size_t chunksize=size*nmemb;
   size_t written=0;

   if(stream) {
        written = fwrite(ptr, size,  nmemb, (FILE *)stream);

#if 0 /*TEST: ---------- */
	egi_dpstd(DBG_MAGENTA"get data %zdBs\n"DBG_RESET, written);
	//sleep(2);
#endif

        if(written<0) {
                EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  written<0!  xxxxx");
                written=0;
        }
	else if(written!=chunksize) {
                EGI_PLOG(LOGLV_ERROR,"xxxxx  Curl callback:  fwrite error!  write %uz of datasize %uz!  xxxxx", written, chunksize);
	}
   }


   //return written;
   return chunksize; /* if(retsize != size*nmemeb) curl will fails!? */
}

/*----------------------------------------------------------------------------
			HTTPS download by libcurl
Note:
1. You must have installed ca-certificates before call curl https, or
   define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.
2. --- !!! WARNING !!! ---
   Filesize of fila_save will be truncated to 0 if it fails!
   The caller MAY need to remove file_save file in case https_easy_download fails!
   OR the file will remain!

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION
@trys:			Max. try times.
			If trys==0; unlimit.
@file_url:		file url for downloading.
@file_save:		file path for saving received file.
@data:			TODO: if more data needed
@write_callback:	Callback for writing received data

		!!! CURL will disable egi tick timer? !!!

Return:
	0	ok
	<0	fails
-------------------------------------------------------------------------------*/
int https_easy_download( int opt, unsigned int trys, unsigned int timeout,
			 const char *file_url, const char *file_save,   void *data,
			 curlget_callback_t write_callback )
{
	int i;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	long longinfo=0;
	FILE *fp;	/* FILE to save received data */
	int sizedown;
	size_t ofsize;
	struct curl_slist *header=NULL;

	/* check input */
	if(file_url==NULL || file_save==NULL)
		return -1;

	/* Open file to save */
	if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {
	    fp=fopen(file_save,"ab");
	    /* Get original file size */
	    ofsize=egi_get_fileSize(file_save);
	}
	else
	    fp=fopen(file_save,"wb");
	if(fp==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: open file '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -1;
	}

	/* Put lock, advisory locks only. TODO: it doesn't work???  */
	if( flock(fileno(fp),LOCK_EX) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

 /* Try Max. tru times */
 for(i=0; i<trys || trys==0; i++)
 {
#if 1	/* Global init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
/* TEST: ------------ */
		exit(1);

  		//curl_global_cleanup();
		if(fclose(fp)!=0)   /* HK2022-07-17 */
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -1;
        }
#endif

	/* Init easy curl */
	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
		ret=-2;
  		//curl_global_cleanup();
		goto CURL_END;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, file_url);

#if 0   /* Setopt TCP KEEPALIVE */
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
#endif

	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
   	/*  1L --disable progress meter, 0L --enable and disable debug output */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/* Redirection */
        if(HTTPS_ENABLE_REDIRECT & opt) {
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); /* Enable redirect */
                curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
        }

/* TEST: ---- HK2022-07-11 */
	/* Set recive buffer size */
//	curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, (32L*1024));
	/* Set Max speed */
	//curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE,(1024L*1024));

	/*  set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); //EGI_CURL_TIMEOUT);

	/* TODO:  unsupported compression method may produce corrupted data! */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  /* enables automatic decompression of HTTP downloads */

	/* Force the HTTP request to get back to using GET. in case the serve NOT provide Access-Control-Allow-Methods! */
//	curl_easy_setopt(curl,CURLOPT_HTTPGET, 1L);

	/* For name lookup timed out error: use ipv4 as default */
	//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	if(write_callback)
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	else /* default */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, easy_callback_writeToFile);

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
	//header=curl_slist_append(header,"User-Agent: Mozilla/5.0(Linux; Ubuntu/16.04)");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	//curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");

	/* Set data destination for write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);
	/* Perform the request, res will get the return code */
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		//if(res ==  CURLE_URL_MALFORMAT )
		//	EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() failed because of URL malformat!", __func__);
		//else if( res == CURLE_OPERATION_TIMEDOUT )  /* res==28 */
		//	EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() Timeout!", __func__);
		//else
		EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));
		ret=-3;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

	/* NOW: CURLE_OK, reset ret! */
	ret=0;

	/* 				--- Check session info. ---
	 *** Note:
	 *   1. curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 *   2. Testing results: CURLINFO_SIZE_DOWNLOAD and CURLINFO_CONTENT_LENGTH_DOWNLOAD get the same doubleinfo!
	 */
/* TEST: ---> */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Start to get response code...", __func__);
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &longinfo) ) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: get response code '%ld'.",__func__, longinfo);
		if(longinfo==0) {
			egi_dpstd("No response from the peer/server!\n");
			ret=-2;
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
	}

	/* TEST: CURLINFO_SIZE_DOWNLOAD */
/* TEST: ---> */
	EGI_PLOG(LOGLV_CRITICAL, "%s: Start to get sizedown...", __func__);
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &doubleinfo) ) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_size_download=%d!",  __func__, (int)doubleinfo);
			sizedown=(int)doubleinfo;
	}

#if 0 /////////////////////   Decompressed, st_size MAY NOT as doubleinfo size ///////////////////////////////

	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &doubleinfo) ) {
		//printf("%s: CURLINFO_CONTENT_LENGTH_DOWNLOAD = %.0f \n", __func__, doubleinfo);
		if( (int)doubleinfo > CURL_RETDATA_BUFF_SIZE )
		{
		  EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length=%d > CURL_RETDATA_BUFF_SIZE!",
  									      __func__, (int)doubleinfo);
			ret=-4;
			break;	/* BUFF overflow! */
		}
		/* Content length MAY BE invalid!!! */
		else if( (int)doubleinfo < 0 ) {
		  	EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length < 0!",  __func__);
			ret=-5;
			//tm_delayms(200);
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
		else {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curl download content length =%d!",  __func__, (int)doubleinfo);
			ret=0;
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}
#endif //////////////////////////////////////////////

CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	egi_dpstd("Curl clean up before try again... the file will be truncated to 0 or ofsize!\n");

	/* If OK, it should NOT carry out following jobs ... */
	if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {
		truncate(file_save, ofsize);
		fseek(fp, ofsize, SEEK_SET); /* truncate will NOT reset file offset! */
	}
	else {
		truncate(file_save, 0);
		rewind(fp);	/* truncate will NOT reset file offset! */
	}

	tm_delayms(200);

	/* Always cleanup before loop back to init curl */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();
 } /* End: try Max times */

	/* Check result, If it fails, truncate filesize to 0 or ofsize before quit!    ??? NOT happens!! */
	if(ret!=0) {
		EGI_PLOG(LOGLV_ERROR, "%s:  Download fails, re_truncate file size to 0 or ofsize",__func__);

		/* Resize/truncate filesize to 0! */
		if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {
		     if(truncate(file_save, ofsize)!=0)
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to ofsize!",__func__);
		}
		else if( truncate(file_save, 0)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to 0!",__func__);
		}

		/* Make sure to flush metadata again! */
		fflush(fp); // fflush first :>>>
		if( fsync(fileno(fp)) !=0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: [download fails] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
		}

		goto CURL_END;
	}

	/* NOW: ret==0 */

	#if 1 /* MidasHK_2022-04-02 XXX  Make sure to flush metadata before recheck file size! necessary? */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start fsync file...\n", __func__);
	fflush(fp); // fflush first :>>>
	if( fsync(fileno(fp)) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
	}
        #endif

#if 0 /////////////////////   Decompressed, st_size MAY NOT as doubleinfo size ///////////////////////////////
	/*** Check date length again. and try to adjust it.
	 *  1. fstat()/stat() MAY get shorter/longer filesize sometime? Not same as doubleinfo, OR curl BUG?
	 *  2. However, written bytes sum_up in write_callback function gets same as stat file size!
	 */
        struct stat     sb;

        //if( fstat(fileno(fp), &sb)<0 ) {
        if( stat(file_save, &sb)<0 ) {
                EGI_PLOG(LOGLV_ERROR, "%s: Fail call stat() for file '%s'. ERR:%s\n", __func__, file_save, strerror(errno));
        } else {
		/* !!! WARNING !!!    [st_size > doubleinfo] OR [st_size < doubleinfo], all possible! */

		if( sb.st_size != (int)doubleinfo ) {

	#if 0 ////////   A. Try to adjust final file size according to doubleinfo   ////////////////
			EGI_PLOG(LOGLV_ERROR,"%s: stat file size(%lld) is NOT same as content length downloaded (%d)! Try adjust...",
									__func__, sb.st_size, (int)doubleinfo);

			/* Resize/truncate to the same as curl downloaded size, If it's shorter, then to be paddled with 0! */
			if( truncate(file_save, (int)doubleinfo )!=0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: [fsize error] Fail to truncate file size to the same as download size!",__func__);
			}

			/* Make sure to flush metadata again! */
			fflush(fp);
			if( fsync(fileno(fp)) !=0 ) {
				EGI_PLOG(LOGLV_ERROR,"%s: [fsize error] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
			}

	#else  /////////  B. Just let it fail! Resize file_save to 0!   ///////////////
			EGI_PLOG(LOGLV_ERROR,"%s: stat file size(%lld) is NOT same as content length downloaded size(%d)! sizedown(%d). Fails.",
									__func__, sb.st_size, (int)doubleinfo, sizedown);
			ret=-10;

			/* Retry if i<3 ???? re_enter into for() ... OK ???  */
			if(i<3) {
							/* Most(all) case: i=0! */
				EGI_PLOG(LOGLV_CRITICAL,"%s: i=%d, i<3, filesize != sizedown, retry...", __func__, i);
				goto CURL_CLEANUP; 	/* To truncate file to 0 and reset offset. */
			}

			/* Resize/truncate filesize to 0! */
			if( truncate(file_save, 0)!=0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: [fsize error] Fail to truncate file size to 0!",__func__);
			}
			/* Make sure to flush metadata again! */
			fflush(fp);
			if( fsync(fileno(fp)) !=0 ) {
				EGI_PLOG(LOGLV_ERROR,"%s: [fsize error] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
			}
	#endif  /////////////////////

		}
		/* (sb.st_size == (int)doubleinfo) */
		else
			EGI_PLOG(LOGLV_CRITICAL,"%s: File size(%lld) is same as content length downloaded size(%d)!",
									__func__, sb.st_size, (int)doubleinfo);
	}
#endif ////////////////////////////////////////

CURL_END:

	/* Unlock, advisory locks only */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start unflock file...\n",__func__);
	if( flock(fileno(fp),LOCK_UN) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to un_flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

	/* Close file */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start fclose(fp)...\n",__func__);
	if(fclose(fp)!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));

	/* Clean up */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start curl clean...\n",__func__);
	if(curl != NULL) {
		curl_slist_free_all(header);
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	return ret;
}

/*-------------------------------------------------------------------------------
Download https stream data, and save them into separated files! Size of each file
can be controlled by the Caller in write_callbck.

Note:
1. Assume that the remote object/file is a stream in chunk data.
2. Set a big timeout value to avoid frequent breakout and clean_up of easy_perform,
   and a new round(new file) of easy_perform may receive repeated data at first.

-----------------------------------------------------------------------------*/
int https_easy_download2( int opt, unsigned int trys,
			  const char *file_url, const char *file_save,   void *data,
			  curlget_callback_t write_callback )
{
	int i;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	long filetime;
	FILE *fp;	/* FILE to save received data */
	int sizedown;
	struct curl_slist *header=NULL;

	/* check input */
	if(file_url==NULL || file_save==NULL)
		return -1;

	/* Open file to save */
	fp=fopen(file_save,"wb");
	if(fp==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: open file '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -1;
	}

	/* Put lock, advisory locks only. TODO: it doesn't work???  */
	if( flock(fileno(fp),LOCK_EX) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

 /* Try Max. trys.. */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
        }

	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
  		curl_global_cleanup();
		ret=-2;
		goto CURL_END;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, file_url);
	//curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

#if 0	/* Setopt TCP KEEPALIVE */
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
#endif

	/* Just request head, without body */
//	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
//	curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL); /* use selfdefined data as headerdata */

	/* Callback when get Header data  */
//	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

	/* Setopt for file time */
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L
   	/*  1L --disable progress meter, 0L --enable and disable debug output  */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/*  1L --enable redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	/*  set timeout. 0--never timeout, Give it a value, to prevent is from stucking in! */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60); //EGI_CURL_TIMEOUT);

	/* TODO:  unsupported compression method may produce corrupted data! */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	/* Set CA bundle */
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");

	/* For name lookup timed out error: use ipv4 as default */
	//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);


	/* Set data destination for write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);

/* Perform to download data. */
while(1) {
	/* then download body */
	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));

		ret=-3;

		if(res == CURLE_OPERATION_TIMEDOUT)
			continue;

		else if(res == CURLE_WRITE_ERROR)
			goto CURL_END;
	}
	/* NOW: CURLE_OK, all data received. reset ret! */
	else
		ret=0;


	/* 				--- Check session info. ---
	 *** Note:
	 *   1. curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 *   2. Testing results: CURLINFO_SIZE_DOWNLOAD and CURLINFO_CONTENT_LENGTH_DOWNLOAD get the same doubleinfo!
	 */
	/* CURLINFO_FILETIME */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime) ) {
		if(filetime<0)
			EGI_PLOG(LOGLV_CRITICAL, "%s: CURLINFO_FILETIME  <0!", __func__ );
		else {
			time_t ftime=(time_t)filetime;
			egi_dpstd(">>>>>>>> File time: %s\n", ctime(&ftime));
		}
	}
	else
		EGI_PLOG(LOGLV_CRITICAL, "%s: Fail to easy_getinfo CURLINFO_FILETIME!", __func__ );

	/* TEST: CURLINFO_SIZE_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &doubleinfo) ) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_size_download=%d!",  __func__, (int)doubleinfo);
			sizedown=(int)doubleinfo;
	}

	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &doubleinfo) ) {
		//printf("%s: CURLINFO_CONTENT_LENGTH_DOWNLOAD = %.0f \n", __func__, doubleinfo);
		if( (int)doubleinfo > CURL_RETDATA_BUFF_SIZE )
		{
		  EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length=%d > CURL_RETDATA_BUFF_SIZE!",
  									      __func__, (int)doubleinfo);
			ret=-4;
			break;	/* BUFF overflow! */
		}
		/* Content length MAY BE invalid!!! */
		else if( (int)doubleinfo < 0 ) {
		  	EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length < 0!",  __func__);
			ret=-5;
			//tm_delayms(200);
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
		else {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curl download content length =%d!",  __func__, (int)doubleinfo);
			ret=0;
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}

} /* END WHILE */


CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	egi_dpstd("Curl clean up before try again... the file will be truncated to 0!\n");
	/* If OK, it should NOT carry out following jobs ... */
	truncate(file_save, 0);
	rewind(fp);	/* truncate will NOT reset file offset! */
	tm_delayms(200);

	/* Always cleanup before loop back to init curl */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();

 } /* End: try Max. TIMEOUTs */


#if 0	/* Check result, If it fails, truncate filesize to 0 before quit!    ??? NOT happens!! */
	if(ret!=0) {
		EGI_PLOG(LOGLV_ERROR, "%s: Download fails, re_truncate file size to 0...",__func__);

		/* Resize/truncate filesize to 0! */
		if( truncate(file_save, 0)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: [download fails] Fail to truncate file size to 0!",__func__);
		}

		/* Make sure to flush metadata again! */
		fflush(fp);
		if( fsync(fileno(fp)) !=0 ) {
			EGI_PLOG(LOGLV_ERROR,"%s: [download fails] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
		}

		goto CURL_END;
	}
#endif //////////////////////

	/* NOW: ret==0 */

	/* Make sure to flush metadata before recheck file size! necessary? */
	fflush(fp);
	if( fsync(fileno(fp)) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
	}


#if 0  ///////////////// NOT for stream data!  ////////////
	/*** Check date length again. and try to adjust it.
	 *  1. fstat()/stat() MAY get shorter/longer filesize sometime? Not same as doubleinfo, OR curl BUG?
	 *  2. However, written bytes sum_up in write_callback function gets same as stat file size!
	 */
        struct stat     sb;

        //if( fstat(fileno(fp), &sb)<0 ) {
        if( stat(file_save, &sb)<0 ) {
                EGI_PLOG(LOGLV_ERROR, "%s: Fail call stat() for file '%s'. ERR:%s\n", __func__, file_save, strerror(errno));
        } else {
		/* !!! WARNING !!!    [st_size > doubleinfo] OR [st_size < doubleinfo], all possible! */

		if( sb.st_size != (int)doubleinfo ) {

	#if 0 ////////   A. Try to adjust final file size according to doubleinfo   ////////////////
			EGI_PLOG(LOGLV_ERROR,"%s: stat file size(%lld) is NOT same as content length downloaded (%d)! Try adjust...",
									__func__, sb.st_size, (int)doubleinfo);

			/* Resize/truncate to the same as curl downloaded size, If it's shorter, then to be paddled with 0! */
			if( truncate(file_save, (int)doubleinfo )!=0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: [fsize error] Fail to truncate file size to the same as download size!",__func__);
			}

			/* Make sure to flush metadata again! */
			fflush(fp);
			if( fsync(fileno(fp)) !=0 ) {
				EGI_PLOG(LOGLV_ERROR,"%s: [fsize error] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
			}

	#else  /////////  B. Just let it fail! Resize file_save to 0!   ///////////////
			EGI_PLOG(LOGLV_ERROR,"%s: stat file size(%lld) is NOT same as content length downloaded size(%d)! sizedown(%d). Fails.",
									__func__, sb.st_size, (int)doubleinfo, sizedown);
			ret=-10;

			/* Retry if i<3 ???? re_enter into for() ... OK ???  */
			if(i<3) {
							/* Most(all) case: i=0! */
				EGI_PLOG(LOGLV_CRITICAL,"%s: i=%d, i<3, filesize != sizedown, retry...", __func__, i);
				goto CURL_CLEANUP; 	/* To truncate file to 0 and reset offset. */
			}

			/* Resize/truncate filesize to 0! */
			if( truncate(file_save, 0)!=0 ) {
				EGI_PLOG(LOGLV_ERROR, "%s: [fsize error] Fail to truncate file size to 0!",__func__);
			}
			/* Make sure to flush metadata again! */
			fflush(fp);
			if( fsync(fileno(fp)) !=0 ) {
				EGI_PLOG(LOGLV_ERROR,"%s: [fsize error] Fail to fsync '%s', Err'%s'", __func__, file_save, strerror(errno));
			}
	#endif  /////////////////////

		}
		/* (sb.st_size == (int)doubleinfo) */
		else
			EGI_PLOG(LOGLV_CRITICAL,"%s: File size(%lld) is same as content length downloaded size(%d)!",
									__func__, sb.st_size, (int)doubleinfo);
	}
#endif  //////////////////////////////////////////////////


CURL_END:
	/* Unlock, advisory locks only */
	if( flock(fileno(fp),LOCK_UN) !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to un_flock '%s', Err'%s'", __func__, file_save, strerror(errno));
		/* Go on .. */
	}

	/* Close file */
	if(fclose(fp)!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));

	/* Clean up */
	if(curl != NULL) {
		curl_slist_free_all(header); header=NULL;
		curl_easy_cleanup(curl); curl=NULL;
  		curl_global_cleanup();
	}

	return ret;
}


/*--------------------------------------------------------------------
Download https data as continuous stream.

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION
@trys:			Max. try times.
			If trys==0; unlimit.
@timeout:		Timeout in seconds.
			A proper timeout value matching with RingBuffer
			makes stream play smoothly.
			Too small value may result in receiving repeated data.
			Too big value cause to break stream playing often.
@stream_url:		URL of the stream
@nr:			Items of reqs[].
@reqs			Pointers to addtional HTTP request lines
@data:			Data destination for write_callback
@write_callback:	Callback for writing received data
@header_callback	Callback for each head line replied.

Note:
1. Assume that the remote object/file is a stream in chunk data.

----------------------------------------------------------------------*/
int https_easy_stream( int opt, unsigned int trys, unsigned int timeout,
			 const char *stream_url, int nr, const char **reqs, void *data,
			 curlget_callback_t write_callback, curlget_callback_t header_callback )
{
	int i,k;
	int ret=0;
  	CURL *curl=NULL;
  	CURLcode res;
	double doubleinfo=0;
	long longinfo=0;
//	long filetime;
	int sizedown;
	struct curl_slist *header=NULL;

	/* Check input */
	if(stream_url==NULL)
		return -1;

 /* Try Max. trys.. */
 for(i=0; i<trys || trys==0; i++)
 {
	/* init curl */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
  		curl_global_cleanup();
		return -1;
        }

	curl = curl_easy_init();
	if(curl==NULL) {
		EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
		ret=-2;
  		curl_global_cleanup();
		goto CURL_END;
	}

	/***   ---  Set options for CURL  ---   ***/
	/* set download file URL */
	curl_easy_setopt(curl, CURLOPT_URL, stream_url);
	//curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

#if 0	/* Setopt TCP KEEPALIVE */
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
#endif

	/* Just request head, without body */
//	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
//	curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL); /* use selfdefined data as headerdata */

	/* Callback when get Header data  */
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

	/* Setopt for file time */
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

	/*  1L --print more detail,  0L --disbale */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); //1L
   	/*  1L --disable progress meter, 0L --enable and disable debug output  */
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	/*  1L --enable redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	/*  set timeout. 0--never timeout. give it a value, to prevent it from stucking in. */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

	/* TODO:  unsupported compression method may produce corrupted data! */
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	/* Set CA bundle */
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");

	/* For name lookup timed out error: use ipv4 as default */
	//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* set write_callback to write/save received data  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* Append HTTP request header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);

	/* User input request header lines */
	if(nr>0) {
	   for(k=0; k<nr; k++) {
		//header=curl_slist_append(header,"Icy-MetaData:1");
		header=curl_slist_append(header, reqs[k]);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER,header);
	   }
	}

	/* Param opt set */
	if(HTTPS_SKIP_PEER_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Set data destination for write_callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

	EGI_PLOG(LOGLV_CRITICAL, "%s: Try [%d]th curl_easy_perform()... ", __func__, i);

/* Perform to download data. */
while(1) {

	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

	/* Receive stream data without break. */
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));
		ret=-3;

		if(res == CURLE_OPERATION_TIMEDOUT)
			continue;
		else if(res == CURLE_WRITE_ERROR)
			goto CURL_END;
	}
	/* NOW: CURLE_OK. All stream data received! */
	else
		ret=0;

	/* 				--- Check session info. ---
	 *** Note:
	 *   1. curl_easy_perform() may result in CURLE_OK, but curl_easy_getinfo() may still fail!
	 *   2. Testing results: CURLINFO_SIZE_DOWNLOAD and CURLINFO_CONTENT_LENGTH_DOWNLOAD get the same doubleinfo!
	 */

	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &longinfo) ) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: get response code '%ld'.",__func__, longinfo);
		if(longinfo==0) {
			egi_dpstd("No response from the peer/server!\n");
			ret=-2;
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
	}
	/* TEST: CURLINFO_SIZE_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &doubleinfo) ) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_size_download=%d!",  __func__, (int)doubleinfo);
			sizedown=(int)doubleinfo;
	}


#if 0 /////////////////////  Decompressed data MAY NOT as doubleinfo size ///////////////////////////////

	/* CURLINFO_FILETIME */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime) ) {
		if(filetime<0)
			EGI_PLOG(LOGLV_CRITICAL, "%s: CURLINFO_FILETIME  <0!", __func__ );
		else {
			time_t ftime=(time_t)filetime;
			egi_dpstd(">>>>>>>> File time: %s\n", ctime(&ftime));
		}
	}
	else
		EGI_PLOG(LOGLV_CRITICAL, "%s: Fail to easy_getinfo CURLINFO_FILETIME!", __func__ );

	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD */
	if( CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &doubleinfo) ) {
		//printf("%s: CURLINFO_CONTENT_LENGTH_DOWNLOAD = %.0f \n", __func__, doubleinfo);
		if( (int)doubleinfo > CURL_RETDATA_BUFF_SIZE )
		{
		  EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length=%d > CURL_RETDATA_BUFF_SIZE!",
  									      __func__, (int)doubleinfo);
			ret=-4;
			break;	/* BUFF overflow! */
		}
		/* Content length MAY BE invalid!!! */
		else if( (int)doubleinfo < 0 ) {
		  	EGI_PLOG(LOGLV_ERROR,"%s: Curl download content length < 0!",  __func__);
			ret=-5;
			//tm_delayms(200);
			goto CURL_CLEANUP; //continue; /* retry ... */
		}
		else {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curl download content length =%d!",  __func__, (int)doubleinfo);
			ret=0;
		  	break; 	/* ----- OK ----- */
		}
	}
	else {	/* Getinfo fails */
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to easy getinfo CURLINFO_CONTENT_LENGTH_DOWNLOAD!", __func__);
		ret=-6;
		//tm_delayms(200);
		goto CURL_CLEANUP; //continue; /* retry ... */
	}
#endif /////////////////////////////////////////////////////////////////////


} /* END WHILE */

CURL_CLEANUP:
	/* If succeeds, break loop! */
	if(ret==0)
		break;

	/* If OK, it should NOT carry out following jobs ... */
	egi_dpstd("Curl clean up before try again...\n");

	/* Always cleanup before loop back to init curl */
	curl_slist_free_all(header);
	header=NULL;
	curl_easy_cleanup(curl);
	curl=NULL; /* OR curl_easy_cleanup() will crash at last! */
  	curl_global_cleanup();

 } /* End: try Max. TIMEOUTs */


CURL_END:
	/* Clean up */
	if(curl != NULL) {
		curl_slist_free_all(header); header=NULL;
		curl_easy_cleanup(curl); curl=NULL;
  		curl_global_cleanup();
	}

	return ret;
}

/* ==========================  Mutli_Thread Easy Downloading  ================= */

/***Note:
 1. Reference: https://blog.csdn.net/tao_627/article/details/39272471
 2. Functions to download a remote file with multi_instances of CURL easy_download,
    each instance downloads part of the file.
 3. ONLY if the server allows range/partial downloading.

 */


static pthread_mutex_t mutex_mtd=PTHREAD_MUTEX_INITIALIZER;  /* Mutex for MThreadDownload */

typedef struct  easy_MultiThreadDownload_Context
{
	int		index;	/* Thread index 0,1,2... */
	pthread_t  	tid;    /* Thread ID */
	CURL 		*curl;	/* Related curl handler, to clean up at thread_easy_download() */

	FILE		*fil;   /* Relate FILE for download data. Just refer.  */
	off_t 		woff;	/* Offset position for writing data */

	size_t		szleft;  /* Data size need to download/write */
} EASY_MTHREADDOWN_CTX;


/*-----------------------------------------------
 A default callback for easy mthread download.
------------------------------------------------*/
static size_t callback_mThreadWriteToFile(void *ptr, size_t size, size_t nmemb, void *arg)
{
	size_t chunksize=size*nmemb;
   	size_t written=0;

	EASY_MTHREADDOWN_CTX *mctx=(EASY_MTHREADDOWN_CTX *)arg;
	if(mctx==NULL)
		return -1;

	egi_dpstd(DBG_GREEN">>>>> thread[%d] writing data size[%zuBytes]*nmemb[%zu] >>>>>\n"DBG_RESET, mctx->index, size, nmemb);

        if(pthread_mutex_lock(&mutex_mtd) !=0 ) {
                return -1;
        }
/* ------ >>>  Critical Zone  */

	if(mctx->szleft==0)
		written=0;
	else if(chunksize > mctx->szleft) {
		fseek(mctx->fil, mctx->woff, SEEK_SET);
		written=fwrite(ptr, 1, mctx->szleft, mctx->fil);
		mctx->szleft=0;
		//mctx->woff No use.
	}
	else {
		fseek(mctx->fil, mctx->woff, SEEK_SET);
		written=fwrite(ptr, size, nmemb, mctx->fil);
		mctx->szleft -= chunksize;
		mctx->woff += chunksize;
	}

/* ------- <<<  Critical Zone  */
        pthread_mutex_unlock(&mutex_mtd);

	return chunksize; /* if(retsize != size*nmemeb) curl will fails*/
}


/*---------------------------------------
A thread function for mThreadDownload.
--------------------------------------*/
void *thread_easy_download(void *arg)
{
	int ret=0;
  	CURLcode res;
	EASY_MTHREADDOWN_CTX *mctx=(EASY_MTHREADDOWN_CTX *)arg;
        if(mctx==NULL) {
		ret=-1;
		goto END_THREAD;
	}

        if( (res=curl_easy_perform(mctx->curl)) !=CURLE_OK ) {
        	EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() failed! res=%d, Err'%s'\n",
						__func__, res, curl_easy_strerror(res));
		ret=-2;
		goto END_THREAD;
        }

END_THREAD:
	curl_easy_cleanup(mctx->curl);
	mctx->curl=NULL;

	return (void *)ret;
}


/*-------------------------------------------------------
To get remote file size by easy_perform.

Note:
1. To avoid nesting of calling curl_global_init() and
   curl_global_cleanup(), the Caller SHOULD take care of
   curl_global_init before call this functioin.

@file_url:	File URL.
@opt:		Http options, Use '|' to combine.

Return:
	<0	Fails
	>=0	OK
---------------------------------------------------------*/
size_t https_easy_getFileSize(const char *file_url, int opt)
{
	int ret=0;
	double fsize;
  	CURL *curl=NULL;
  	CURLcode res;
	unsigned int timeout=30;

#if 0   /* Init curl.... Nope! to avoid nesting of calling curl_global_init/curl_global_cleanup....*/
        EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...\n",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
                curl_global_cleanup();
                return -1;
        }
#endif

	/* Easy init */
        curl = curl_easy_init();
        if(curl==NULL) {
                EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl!",__func__);
//              curl_global_cleanup();
		return -2;
        }

	/* Curl Options */
	curl_easy_setopt(curl, CURLOPT_URL, file_url);
	curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);

        /* Redirection */
        if(HTTPS_ENABLE_REDIRECT & opt) {
        	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); /* Enable redirect */
                curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
        }
	/* Verification */
        if(HTTPS_SKIP_PEER_VERIFICATION & opt)
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Curl easy perform */
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		/* Check Curl config: #ifdef CURL_DISABLE_VERBOSE_STRINGS! */
		EGI_PLOG(LOGLV_ERROR,"%s: res=%d, curl_easy_perform() failed! Err'%s'", __func__, res, curl_easy_strerror(res));
		if(res == CURLE_OPERATION_TIMEDOUT)
			EGI_PLOG(LOGLV_ERROR,"%s: %ds timeout!", __func__, timeout);
		ret=-3; goto CURL_END;
	}
	else {
		res=curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &fsize);
		if(res == CURLE_OK) {
		  	EGI_PLOG(LOGLV_CRITICAL,"%s: Curlinfo_content_length_download=%d!\n",  __func__, (int)fsize);
			ret=0;
		}
		else {
			EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_getinfo failed, Err'%s'", __func__, curl_easy_strerror(res));
			ret=-4;
		}
	}

CURL_END:
	/* Clean up */
	if(curl != NULL) {
		//curl_slist_free_all(header);
		curl_easy_cleanup(curl);
//  		curl_global_cleanup();
	}

	if(ret==0)
		return fsize;
	else //ret<0
		return ret;
}


/*----------------------------------------------------------------------------
Download a remote file by multi_instanes of easy_download.

Note:
1. You must have installed ca-certificates before call curl https, or
   define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.
2. --- !!! WARNING !!! ---
   Filesize of fila_save will be truncated to 0 if it fails!
   The caller MAY need to remove file_save file in case https_easy_download fails!
   OR the file will remain!

3. 		----  !!! CAUTION !!!!  ---
   3.1 Licurl is thread safe, except for signals and SSL/TLS handlers.
   3.2 You should set CURLOPT_NOSIGNAL option to 1 for all handers! In this case,
       timeout are NOT honored during the DNS lookup.
   3.3 A multi-threaded example with SSL: curl-xxx/docs/examples/threaded-ssl.c


@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION
@nthreads:		Number of threads for downloading file.
@trys: (TODO)		Max. try times for each thread.
			If trys==0; unlimit.
@timeout:		Timeout in seconds.
@file_url:		file url for downloading.
@file_save:		file path for saving received file.

		!!! CURL will disable egi tick timer? !!!

Return:
	0	ok
	<0	fails
-------------------------------------------------------------------------------*/
int https_easy_mThreadDownload(int opt, unsigned int nthreads, unsigned int trys, unsigned int timeout,
				const char *file_url, const char *file_save )
{
	int ret=0;
	int i,j;
	//CURLcode res;
	EASY_MTHREADDOWN_CTX *mctx=NULL;
	FILE *fp=NULL;	/* File to save */
	size_t ofsize;
	struct curl_slist *header=NULL;
	size_t fsize;
	size_t tsize;  /* tsize=fsize/nthread; File part size for each downloading thread */
	char strRange[128];

	/* 1. Check input */
	if(file_url==NULL || file_save==NULL)
		return -1;

	/* 2. Open file to save */
	//if(opt&HTTPS_DOWNLOAD_SAVE_APPEND) {   Need to revise woff ---->
	//	fp=fopen(file_save, "ab");
	//	/* Get original file size */
	//	ofsize=egi_get_fileSize(file_save);
	//}
	//else
	fp=fopen(file_save, "wb");
	if(fp==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: open file '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -2;
	}

	/* 3. BLANK  */

	/* 4. Create EASY_MTHREADDOWN_CTX for each thread */
	mctx=calloc(nthreads, sizeof(EASY_MTHREADDOWN_CTX));
	if(mctx==NULL) {
                EGI_PLOG(LOGLV_ERROR, "%s: Fail to callocate mtx!",__func__);
		fclose(fp);
		return -4;
	}

#if 1	/* 5. Global init curl.. BEFORE https_easy_getFileSize() */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
                //curl_global_cleanup();
		if(fclose(fp)!=0)
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));
		free(mctx);
		return -5;
        }
#endif

	/* 6. Get remote file size */
	fsize=https_easy_getFileSize(file_url, opt);
	if(fsize<0) {
		EGI_PLOG(LOGLV_ERROR,"%s: https_easy_getFileSize() fails!\n", __func__);
		ret=-6; goto CURL_END;
	}
	else if(fsize==0) {
		EGI_PLOG(LOGLV_ERROR,"%s: https_easy_getFileSize()==0!\n", __func__);
		ret=0; goto CURL_END;
	}
	/* 6.1 Part size for each thread */
	tsize=fsize/nthreads;

	/* 7. Common header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");

	/* 8. Create downloading threads */
	for(i=0; i<nthreads; i++) {
		/* 8.1 Init curl */
		CURL *curl=curl_easy_init();
		if(curl==NULL) {
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl for mctx[%d]!",__func__, i);
			for(j=0; j<i; j++)  /* TODO:  This MAY cause crash if threads alreay running?  */
				curl_easy_cleanup(mctx[j].curl);
			ret=-8;
			goto CURL_END;
		}
		/* 8.2 Assign/init memebers of mctx[i] */
		/* fp, curl, szleft, woff */
		mctx[i].index =i;
		mctx[i].fil =fp;
		mctx[i].curl =curl; /* mctx[] is Owner */
		if(i==nthreads-1) {
			/* The last thread need to download all remaining data size. */
			mctx[i].szleft =fsize-tsize*(nthreads-1);
			mctx[i].woff =i*tsize;
		}
		else {
			mctx[i].szleft =tsize;
			mctx[i].woff =i*tsize;
		}

		/* 8.3 Set strRange */
		strRange[sizeof(strRange)-1]=0;
		snprintf(strRange, sizeof(strRange)-1, "%jd-%jd", mctx[i].woff, mctx[i].woff+mctx[i].szleft-1);
		curl_easy_setopt (curl, CURLOPT_RANGE, strRange);

		/* 8.4 Set download file URL */
		curl_easy_setopt(curl, CURLOPT_URL, file_url);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);

		/* For multiple threads! */
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL,1L);

		/* Redirection */
		if(HTTPS_ENABLE_REDIRECT & opt) {
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); /* Enable redirect */
			curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
		}

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, (32L*1024));

		/* Set Max speed */
		//curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE,(1024L*1024));

		/* Low speed limit in bytes per second */
		//curl_easy_setopt(curl, CURLOPT_LOW_SPEED, 1L);
		//culr_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);

		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
		/* TODO:  unsupported compression method may produce corrupted data! */
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  /* enables automatic decompression */
		/* Force the HTTP request to get back to using GET. in case the serve NOT provide Access-Control-Allow-Methods! */
		curl_easy_setopt(curl,CURLOPT_HTTPGET, 1L);
		/* For name lookup timed out error: use ipv4 as default */
		//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_mThreadWriteToFile);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)(&mctx[i]));

		/* Append HTTP request header */
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

		/* Peer/hostname verification */
		if(HTTPS_SKIP_PEER_VERIFICATION & opt)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

#if 0 //////////////   Critical Zone  ///////////////
	       if(pthread_mutex_lock(&mutex_mtd) !=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to get mutex_tmd!",__func__);
			for(j=0; j<i; j++)
				curl_easy_cleanup(mctx[j].curl);
                	ret=-8;
			goto CURL_END;
        	}
/* ------ >>>  Critical Zone  */



/* ------- <<<  Critical Zone  */
	        pthread_mutex_unlock(&mutex_mtd);

#endif ////////////////////////////////////////////

		/* 8.5 Create thread */
		if(pthread_create(&mctx[i].tid, NULL, thread_easy_download, (void *)(&mctx[i]))!=0) {
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to create thread for mctx[%d]!", __func__, i);
			for(j=0; j<i; j++)  /* TODO: This MAY cause crash if threads alreay running? */
				curl_easy_cleanup(mctx[j].curl);
                	ret=-8;
			goto CURL_END;
		}
		EGI_PLOG(LOGLV_CRITICAL,"%s: Thread mctx[%d] is created!\n", __func__, i);

	} /* END for(i) */

	/* 9. Join all downloading threads */
	void *tdret=(void *)-1;
	for(i=0; i<nthreads; i++) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: Wait to join thread mctx[%d] ...\n", __func__, i);
		if( pthread_join(mctx[i].tid, &tdret)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: pthread_join mctx[%d] fails, Err'%s' ", __func__, i, strerror(errno));
		}
		else {
			EGI_PLOG(LOGLV_CRITICAL, "%s: pthread_join mctx[%d] with tdret=%d.\n", __func__, i, (int)tdret);
		}

		/* Check error */
		if((int)tdret)
			ret=-9;
	}


CURL_END:
	/* P1. Close file */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start fclose(fp)...\n",__func__);
	if(fclose(fp)!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));

	/* P2. Free mctx[] and its content */
	/* P2.1 Clear up thread curl ----> ALSO see in thread function. */
	for(i=0; i<nthreads; i++) {
	    if(mctx[i].curl)
		    curl_easy_cleanup(mctx[i].curl);
	}
	/* P2.2 Free mctx */
	free(mctx);

	/* P3. Clean up main curl */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start curl clean...\n",__func__);
	curl_slist_free_all(header); //header=NULL;
	curl_global_cleanup();

	return ret;
}


/* ==========================  Mutli_URL Easy Downloading  ================= */

typedef struct  easy_MultiURL_Download_Context
{
	//int		index;	/* Thread index 0,1,2... */
	pthread_t  	tid;    /* Thread ID */
	CURL 		*curl;	/* Related curl handler, to clean up at thread_easy_download() */

        //const char      *url;   /* URL for the remote file */
        //const char      *file_save;  /* Fath to save the file */
} EASY_MURLDOWN_CTX;


/*---------------------------------------
A thread function for mURLDownload.
--------------------------------------*/
void *thread_easy_download2(void *arg)
{
	int ret=0;
  	CURLcode res;
	CURL *curl=arg;
	if(curl==NULL) {
		ret=-1;
		goto END_THREAD;
	}

        if( (res=curl_easy_perform(curl)) !=CURLE_OK ) {
        	EGI_PLOG(LOGLV_ERROR,"%s: curl_easy_perform() failed! res=%d, Err'%s'\n",
						__func__, res, curl_easy_strerror(res));
		ret=-2;
		goto END_THREAD;
        }

END_THREAD:
	curl_easy_cleanup(curl);

	return (void *)ret;
}



/*----------------------------------------------------------------------------
Easy_download multiple remote files at the same time.

Note:
1. You must have installed ca-certificates before call curl https, or
   define SKIP_PEER/HOSTNAME_VERIFICATION to use http instead.
2. --- !!! WARNING !!! ---
   Filesize of fila_save will be truncated to 0 if it fails!
   The caller MAY need to remove file_save file in case https_easy_download fails!
   OR the file will remain!

3. 		----  !!! CAUTION !!!!  ---
   3.1 Licurl is thread safe, except for signals and SSL/TLS handlers.
   3.2 You should set CURLOPT_NOSIGNAL option to 1 for all handers! In this case,
       timeout are NOT honored during the DNS lookup.
   3.3 A multi-threaded example with SSL: curl-xxx/docs/examples/threaded-ssl.c


@n:             	Numbers of files/segments to download
@urls:          	URLs for above files/segments.
@file_save(TODO):	file path for saving received file.

@opt:			Http options, Use '|' to combine.
			HTTPS_SKIP_PEER_VERIFICATION
			HTTPS_SKIP_HOSTNAME_VERIFICATION
@trys: (TODO)		Max. try times for each thread.
			If trys==0; unlimit.
@timeout:		Timeout in seconds.

		!!! CURL will disable egi tick timer? !!!

Return:
	0	ok
	<0	fails
-------------------------------------------------------------------------------*/
int https_easy_mURLDownload( unsigned int n, const char **urls, const char *file_save,
			     int opt, unsigned int trys, unsigned int timeout )
{
	int ret=0;
	int i,j;
	//CURLcode res;
	FILE *fp=NULL;	/* Final mergered file. */

	/* For each URL thread */
	char **filenames=NULL;  /* Save file for each URL */
	FILE **fils=NULL;
	CURL **curls=NULL;
	pthread_t *tids=NULL;

	size_t ofsize;
	struct curl_slist *header=NULL;

	/* 1. Check input */
	if(n<1 || urls==NULL || file_save==NULL)
		return -1;

	/* 2. Open file to save */
	fp=fopen(file_save, "wb");
	if(fp==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: open file '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -2;
	}

	/* 3. Callocate fils  */
	fils=calloc(n, sizeof(FILE *));
	if(fils==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to calloc fils, Err'%s'", __func__, strerror(errno));
		fclose(fp);
		return -3;
	}

	/* 4. Create filenames[] */
	filenames=egi_create_charList(n, EGI_PATH_MAX+EGI_NAME_MAX);
	if(filenames==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to calloc filenames, Err'%s'", __func__, strerror(errno));
		ret=-4; goto CURL_END;
	}

	/* 5. Calloc curls[] */
	curls=calloc(n, sizeof(CURL *));
	if(curls==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to calloc curls, Err'%s'", __func__, strerror(errno));
		ret=-5; goto CURL_END;
	}

	/* 6. Calloc tids[] */
	tids=calloc(n, sizeof(pthread_t));
	if(tids==NULL) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to calloc tids, Err'%s'", __func__, strerror(errno));
		ret=-6; goto CURL_END;
	}

	/* 7. Open files for write dowloaded data */
	for(i=0; i<n; i++) {
		sprintf(filenames[i], "/tmp/__%d__.ts", i);
		fils[i]=fopen(filenames[i], "wb");
		if(fils[i]==NULL) {
			EGI_PLOG(LOGLV_ERROR,"%s: open file '%s', Err'%s'", __func__, filenames[i], strerror(errno));
			ret=-5; goto CURL_END;
		}
	}

	/* 8. Global init curl.. BEFORE https_easy_getFileSize() */
	EGI_PLOG(LOGLV_INFO, "%s: start curl_global_init and easy init...",__func__);
        if( curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK ) {
                EGI_PLOG(LOGLV_ERROR, "%s: curl_global_init() fails!",__func__);
		free(fils);
		if(fclose(fp)!=0)
			EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));
		return -6;
        }

	/* 9. Common header */
	header=curl_slist_append(header,"User-Agent: CURL(Linux; Egi)");

	/* 10. Create downloading threads */
	for(i=0; i<n; i++) {
		/* 10.1 Init each curl */
		curls[i]=curl_easy_init();
		if(curls[i]==NULL) {
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to init curl[%d]!",__func__, i);
			for(j=0; j<i; j++)  /* TODO  This MAY cause crash if threads alreay running? */
				curl_easy_cleanup(curls[j]);
			ret=-8;
			goto CURL_END;
		}

		/* 10.2 Set download file URL */
		curl_easy_setopt(curls[i], CURLOPT_URL, urls[i]);
		curl_easy_setopt(curls[i], CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curls[i], CURLOPT_NOPROGRESS, 1);

		/* For multiple threads! */
		curl_easy_setopt(curls[i], CURLOPT_NOSIGNAL,1L);

		/* Redirection */
		if(HTTPS_ENABLE_REDIRECT & opt) {
			curl_easy_setopt(curls[i], CURLOPT_FOLLOWLOCATION, 1L); /* Enable redirect */
			curl_easy_setopt(curls[i], CURLOPT_AUTOREFERER, 1L);
			curl_easy_setopt(curls[i], CURLOPT_MAXREDIRS, 3);
		}

		curl_easy_setopt(curls[i], CURLOPT_BUFFERSIZE, (32L*1024));

		/* Set Max speed */
		//curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE,(1024L*1024));

		/* Low speed limit in bytes per second */
		//curl_easy_setopt(curl, CURLOPT_LOW_SPEED, 1L);
		//culr_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);

		curl_easy_setopt(curls[i], CURLOPT_TIMEOUT, timeout);
		/* TODO:  unsupported compression method may produce corrupted data! */
		curl_easy_setopt(curls[i], CURLOPT_ACCEPT_ENCODING, "");  /* enables automatic decompression */
		/* Force the HTTP request to get back to using GET. in case the serve NOT provide Access-Control-Allow-Methods! */
		curl_easy_setopt(curls[i],CURLOPT_HTTPGET, 1L);
		/* For name lookup timed out error: use ipv4 as default */
		//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

		curl_easy_setopt(curls[i], CURLOPT_WRITEFUNCTION, easy_callback_writeToFile);
		curl_easy_setopt(curls[i], CURLOPT_WRITEDATA, (void *)fils[i]);

		/* Append HTTP request header */
		curl_easy_setopt(curls[i], CURLOPT_HTTPHEADER, header);

		/* Peer/hostname verification */
		if(HTTPS_SKIP_PEER_VERIFICATION & opt)
			curl_easy_setopt(curls[i], CURLOPT_SSL_VERIFYPEER, 0L);
		if(HTTPS_SKIP_HOSTNAME_VERIFICATION & opt)
			curl_easy_setopt(curls[i], CURLOPT_SSL_VERIFYHOST, 0L);

		/* 10.3 Create thread to download each URL */
		if(pthread_create(&tids[i], NULL, thread_easy_download2, (void *)curls[i])!=0) {
			EGI_PLOG(LOGLV_ERROR, "%s: Fail to create thread for curls[%d]!", __func__, i);
			for(j=0; j<i; j++)  /* TODO: This MAY cause crash if threads alreay running? */
				curl_easy_cleanup(&curls[j]);
                	ret=-8;
			goto CURL_END;
		}
		EGI_PLOG(LOGLV_CRITICAL,"%s: Thread_easy_download with curls[%d] is created!\n", __func__, i);

	} /* END for(i) */

	/* 11. Join all downloading threads */
	void *tdret=(void *)-1;
	for(i=0; i<n; i++) {
		EGI_PLOG(LOGLV_CRITICAL,"%s: Wait to join thread for curls[%d] ...\n", __func__, i);
		if( pthread_join(tids[i], &tdret)!=0 ) {
			EGI_PLOG(LOGLV_ERROR, "%s: pthread_join curls[%d] fails, Err'%s' ", __func__, i, strerror(errno));
		}
		else {
			EGI_PLOG(LOGLV_CRITICAL, "%s: pthread_join curls[%d] with tdret=%d.\n", __func__, i, (int)tdret);
		}

		/* Check error */
		if((int)tdret)
			ret=-9;
	}

CURL_END:
	/* P1. Free tids */
	if(tids) free(tids);

	/* P2. curls[i] cleanup at thread function */
	/* P3. Free curls */
	if(curls) free(curls);

	/* P4. Close the final merged file */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start fclose(fp)...\n",__func__);
	if(fclose(fp)!=0)
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to fclose '%s', Err'%s'", __func__, file_save, strerror(errno));

	/* P5. Close(and Remove) temp. fils */
	for(i=0; i<n; i++) {
	    if(fils[i]) {
		int fd=fileno(fils[i]);
		fflush(fils[i]); fsync(fd);

		fclose(fils[i]);

		/* P5.1 Remove file if fails */
		if(ret) {
			egi_dpstd("Remove '%s'...\n", filenames[i]);
	                if(remove(filenames[i])!=0)
        	                EGI_PLOG(LOGLV_ERROR,"%s: Fail to remove '%s'.",__func__, filenames[i]);
		}
	    }
	}

	/* P6. Free fils */
	if(fils) free(fils);

	/* P7. Free filenames */
	if(filenames) egi_free_charList(&filenames, n);

	/* P8. Clean up main curl env. */
	EGI_PLOG(LOGLV_CRITICAL,"%s: Start curl clean...\n",__func__);
	curl_slist_free_all(header);
	curl_global_cleanup();

	EGI_PLOG(LOGLV_CRITICAL,"%s: Curl clean OK!\n",__func__);
	return ret;
}
