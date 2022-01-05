/* -----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Char and String Functions.

Journal
2021-01-24:
	1. Add cstr_hash_string().
2021-02-09:
	1. Modify cstr_split_nstr().
2021-07-17:
	1. Add cstr_get_peword(), cstr_strlen_eword().
2021-07-20:
	1. cstr_strlen_eword():	Include any trailing SPACEs/TABs.
2021-11-25:
	1. cstr_parse_html_tag(): Parse Tag image, <img src=... alt=..>
	2. Add cstr_get_html_attribute().
2021-11-28/29:
	1. cstr_parse_html_tag(): param attributes, and include nested tag.
2021-11-29:
	1. cstr_unicode_to_uft8(): Put a terminating null byte after conversion.
2021-12-01:
	1. Add cstr_replace_string()
2021-12-02:
	1. Add cstr_decode_htmlSpecailChars(char *strHtml)
2021-12-06:
	1. Add cstr_extract_html_text()
2021-12-08:
	1. cstr_parse_html_tag(): to distinguish similar TAG starts, as '<a...>' to '<alt...>'.
	   strstr() NOT enough, need to check/confirm NEXT char to be ' ' OR '>'.
2021-12-15:
	1. cstr_parse_html_tag(): add param attrisize of attributes mem space.
2021-12-16:
	1. Add cstr_parse_simple_html()
2021-12-19/20:
	1. Add cstr_parse_URL()
2021-12-23:
	1. Add cstr_parse_simple_HLS()
2021-12-27:
	1. cstr_extract_html_text(): Add param 'txtsize'
2021-12-29:
	1. Add egi_str2hex()
	2. Add egi_hex2str()
2021-12-31:
	1. Add struct EGI_M3U8_LIST.
	2. Add m3u_free_list();
	3. Modify 'cstr_parse_simple_HLS()' to 'm3u_parse_simple_HLS()'
2022-01-03:
	1. m3u_parse_simple_HLS(): Check and update segment Sequence Number.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include "egi_cstring.h"
#include "egi_log.h"
#include "egi_utils.h"
#include "egi_debug.h"


/*-----------------------------------------------
Conver string back into original data.

Note:
1. Here the string is presentation of the original
data in hex format, every two bytes (chars) for one
original byte data.

	!!! --- CAUTION --- !!!
2. No terminal NULL for the returned data!

	!!! --- CAUTION --- !!!
3.DO NOT forget to free the pointer after use!

@str:	Pointer to string
@size:  Size of data expected.
	If size>strlen(str)/2, padding with zeros.


Return:
	!NULL	OK
	NULL	Fails
------------------------------------------------*/
unsigned char* egi_str2hex(char *str, size_t size)
{
	int i;

	if(str==NULL || size==0)
		return NULL;

   	/* Check size */
 	int len=strlen(str);
   	if(len&1) {
		egi_dpstd("Strlen MUST be multiples of 2!\n");
		return NULL;
   	}

	/* Calloc */
	unsigned char *hex=(unsigned char *)calloc(1, size);
	if(hex==NULL)
		return NULL;

	/* Truncate len as per wanted size */
	if(len>2*size) len=2*size;

	/* Convert:
	   %2h -- 2_digits short type;
	   %2hh -- 2_digits short short type (=char)
	  */
	for(i=0; i<len; i+=2)
	        sscanf(str+i,"%2hhx",hex+i/2);

	return hex;
}


/*----------------------------------------------
Present the data in form of hex and stored in a
string ending with a terminal NULL. Each one byte
data will be presented by two bytes(chars).

Note:
1. The Caller MUST ensure enough space/data in
   hex[]! eg. size of hex[] MUST >=len.

	!!! --- CAUTION --- !!!
2. Input hex[] MAY have NO terminal NULL at all!

	!!! --- CAUTION --- !!!
3. DO NOT forget to free the pointer after use!

@hex:	Data in hex.
@len:	Length of data need to be converted.
	Each one byte will be presented by two chars
	as a HEX.

Return:
	!NULL	OK
	NULL	Fails
----------------------------------------------*/
char* egi_hex2str(unsigned char *hex, size_t len)
{
	int i;

	if(hex==NULL || len==0)
		return NULL;

	/* Calloc */
	char *str=(char *)calloc(1, 2*len+1); /* +1 EOF */
	if(str==NULL)
		return NULL;

	/* Sprintf to str */
	for(i=0; i<len; i++)
		sprintf(str+2*i, "%02x", hex[i]);

	return str;
}


/*----------------------------------------
Create an EGI_TXTGROUP.

@offs_capacity:  Initial offs[] array size.
		 If 0, reset to 8
@buff_capacity:  Initial buff mem size.
		 If 0, reset to 1024Bytes
Return:
	!NULL	OK
	NULL	Fails
-----------------------------------------*/
EGI_TXTGROUP *cstr_txtgroup_create(size_t offs_capacity, size_t buff_capacity)
{
	EGI_TXTGROUP *txtgroup=calloc(1, sizeof(EGI_TXTGROUP));
	if(txtgroup==NULL) {
		printf("%s: Fail to calloc offs!\n", __func__);
		return NULL;
	}

	if(offs_capacity==0)
		offs_capacity=8;
	if(buff_capacity==0)
		buff_capacity=1024;

	txtgroup->offs=calloc(offs_capacity, sizeof(typeof(*txtgroup->offs)));
	if(txtgroup->offs==NULL) {
		printf("%s: Fail to calloc txtgroup->offs!\n", __func__);
		free(txtgroup);
		return NULL;
	}

	txtgroup->buff=calloc(buff_capacity, sizeof(typeof(*txtgroup->buff)));
	if(txtgroup->offs==NULL) {
		printf("%s: Fail to calloc txtgroup->buff!\n", __func__);
		free(txtgroup->offs);
		free(txtgroup);
		return NULL;
	}

	txtgroup->offs_capacity=offs_capacity;
	txtgroup->buff_capacity=buff_capacity;

	return txtgroup;
}


/*----------------------------------------
	Free an EGI_TXTGROUP.
-----------------------------------------*/
void  cstr_txtgroup_free(EGI_TXTGROUP **txtgroup)
{
	if(txtgroup==NULL || *txtgroup==NULL)
		return;

	free((*txtgroup)->buff);
	free((*txtgroup)->offs);
	free((*txtgroup));
	*txtgroup=NULL;
}

/*------------------------------------------------
Push a string into txtgroup. txt MUST be terminated
with '\0'.

@txtgroup:	Pointer to an EGI_TXTGROUP
@txt:		A string of chars

Return:
	0	OK
	<0	Fails
--------------------------------------------------*/
int cstr_txtgroup_push(EGI_TXTGROUP *txtgroup, const char *txt)
{
	int txtlen;

	if(txtgroup==NULL || txt==NULL)
		return -1;

	txtlen=strlen(txt);

	/* Check offs[] space */
	if( txtgroup->offs_capacity < txtgroup->size+1 ) {
		 if( egi_mem_grow( (void **)&txtgroup->offs, txtgroup->offs_capacity*sizeof(typeof(*txtgroup->offs)),
                                                                    TXTGROUP_OFF_GROW_SIZE*sizeof(typeof(*txtgroup->offs))) != 0 )
                 {
                                printf("%s: Fail to mem grow txtgroup->offs!\n", __func__);
				return -2;
		 }
		 txtgroup->offs_capacity+=TXTGROUP_OFF_GROW_SIZE;
		 printf("%s: txtgroup->offs_capacity grows to be %d \n", __func__, txtgroup->offs_capacity);
	}
	/* Check buff[] space */
	while( txtgroup->buff_capacity < txtgroup->buff_size+txtlen+1 )  {   /* +1 '\0' */
		if( egi_mem_grow( (void **)&txtgroup->buff, txtgroup->buff_capacity*sizeof(typeof(*txtgroup->buff)),
                                                                    TXTGROUP_BUFF_GROW_SIZE*sizeof(typeof(*txtgroup->buff))) != 0 )
                {
                                printf("%s: Fail to mem grow txtgroup->buff!\n", __func__);
				return -3;
		}
		txtgroup->buff_capacity+=TXTGROUP_BUFF_GROW_SIZE;
		 printf("%s: txtgroup->buff_capacity grows to be %d \n", __func__, txtgroup->buff_capacity);
	}

	/* Push txt */
	memcpy(txtgroup->buff+txtgroup->buff_size, txt, txtlen);

	/* Update: buff_size, offs, size */
	txtgroup->buff_size += txtlen+1;  /* +1 '\0' */
	txtgroup->offs[++txtgroup->size]=txtgroup->buff_size;

	return 0;
}

/*-----------------------------------------------------------
Compare 'len' chars of two strings to judge their dictionary
order.

@str1,str2:	Strings for comparison.
@len:		Comparing length.

Return:
	0	DICTODER_IS_SAME    ( str1 and str2 same order)
        1      	DICTODER_IS_AHEAD   ( str1 is aheah of str2 )
	2	DICTODER_IS_AFTER   ( str1 is behind str2 )
	<0	fails
-----------------------------------------------------------*/
int cstr_compare_dictOrder(const char *str1, const char *str2, size_t len)
{
	return 0;
}


/*---------------------------------------
To check if the string is an absolute URL
,only by checking "://".
---------------------------------------*/
bool cstr_is_absoluteURL(const char *URL)
{
	if(URL==NULL)
		return false;

	if(strstr(URL, "://")!=NULL)
		return true;
	else
		return false;
}


/*----------------------------------------------------------------
Parse input URL string, and to extract protocol,hostname and
port number.
All param pointers MUST be cleared before calling the function!
OR their holding memory space will be lost/leaked!

URL components:
1.	Example:
		http(s)://www.abcde.com:1234/doc/test?id=3&lan=en#p1

	Protocol(scheme): 	http(s)://
	Hostname: 	www.abcde.com
	Port: 		1234
	Path: 		/doc/test
	Query string:   id=3&lan=en  ( ? ---qurey_string separator)
	Fragment:	p1	     ( # ---fragment separator )

2.	!!! --- CAUTION --- !!!
	Example:  https://www.letgo.xx/pic/food
	food ---> The server will see it as the resource name
	first, if NOT found, then parse it as the last directory!
	You'd better explicitly add '/' at the end if it's
        just a directory!


Note:
1. If no "://" found in the URL string, then it fails!
   It means the input URL MUST be an absolute URL(full address URL).

TODO:
1. to separate and extract query_string&params from path.


@URL:		Input URL string, a full address URL.
@protocl: 	PPointer to pass out protocol.
@hostname:	PPointer to pass out hostname.
@port:		PPointer to pass out port.
		*post=0, as default for the protocol.
@path:		Absolute full path = dir+(resource_name)
@filename:	The resource object(file) name.
@dir:		Directory part of the path, see above.
	(TODO:  if the last char in URL is NOT '/', then
		the last components of URL will be parsed
		as resource name!
		see in 'URL components". )
@dirURL:	The URL for the dir
		Example:  https://www.letgo.xx/pic/food/apple.jpg
		         dirURL: https://www.letgo.xx/pic/food/

	!!! --- CAUTION --- !!!
DO NOT forget to free *protocl and *hostname.


Return:
	0	Ok
	<0	Fails
-----------------------------------------------------------------*/
int cstr_parse_URL(const char *URL, char **protocol, char **hostname,
		unsigned int *port, char **path, char **filename, char **dir, char **dirURL)
{
	char *ps=NULL;
	char *pe=NULL;
	char *pc=NULL;  /* Point to the ':' before port number */

	/* Check URL */
	if(URL==NULL)
		return -1;

	/* Reset init pointers to NULL anyway. */
	if(protocol)	*protocol=NULL;
	if(hostname)	*hostname=NULL;
	if(port)	*port=0;	/* As default */
	if(path)	*path=NULL;
	if(filename)	*filename=NULL;
	if(dir)		*dir=NULL;
	if(dirURL)	*dirURL=NULL;

	/* 1. Extract protocol */
	ps=(char *)URL;
	pe=strstr(URL,"://");
	if(pe==NULL)
		return -2;

	pe +=strlen("://");
	if(protocol) *protocol=strndup(ps, pe-ps);

	/* If port number specified in the URL */
	ps=pe; /* Now ps points to end of '://'  <---------- */
	pc=strstr(ps,":");
	if(pc!=NULL) {
		/* 2. Extract port number */
		if(port) *port=atoi(pc+1);

		/* 3. Eextract hostname */
		if(hostname) *hostname=strndup(ps, pc-ps);

		/* Extract path and dir */
		ps=pc+strlen(":");
		pe=strstr(ps, "/"); /* the first '/' */
		if(pe!=NULL) {
			/* 4. Extract path */
			if(path) *path=strndup(pe, strlen(URL)-(pe-URL));

			/* 5. Extract dir */
			/* Locate the last '/' */
			pc=(char *)URL+strlen(URL);
			while(*pc != '/') pc--;
			if(dir) *dir=strndup(pe, pc+1 -pe); /* +1 to include the last '/' */

			/* 6. Extract filename */
			if(filename) *filename=strdup(pc+1);

			/* 7. Extract dirURL */
			if(dirURL) *dirURL=strndup((char *)URL, pc+1 -(char *)URL);
		}
		/* If no subdir */
		else {
			/* 4. Extract dir */
			if(path) *path=strdup("/");

			/* 5. Extract dir */
			if(dir) *dir=strdup("/");

			/* 6. Extract filename */
			// No filename, default!

			/* 7. Extract dirURL */
			if(dirURL) {
				*dirURL=calloc(1, strlen(URL)+1+1);
				if(*dirURL) {
					strcat(*dirURL, URL);
					strcat(*dirURL, "/");
				}
			}
		}

		/* TODO: to separate and extract query string and params from path */

/* -------> Return */
		return 0;
	}

	/* NOW: no ':' and port number in the URL string */

	/* 2a. Extract portnumber */
	if(port) *port=0; // TODO: Default port number according to protocols

	/* Now ps points to end of '://'  <---------- */
	pe=strstr(ps, "/"); /* the first '/' */
	/* If '/' exists in the URL */
	if(pe!=NULL) { /* With subdir */
		/* 3a. Extract hostname */
		if(hostname) *hostname=strndup(ps, pe-ps);

		/* 4a. Extract path */
		if(path) *path=strndup(pe, strlen(URL)-(pe-URL));

		/* 5a. Extract dir */
		/* Locate the last '/' */
		pc=(char *)URL+strlen(URL);
		while(*pc != '/') pc--;
		if(dir) *dir=strndup(pe, pc+1 -pe); /* +1 to include the last '/' */

		/* 6a. Extract filename */
		if(filename) *filename=strdup(pc+1);

		/* 7a. Extract dirURL */
		if(dirURL) *dirURL=strndup((char *)URL, pc+1 -(char *)URL);

		/* TODO: to separate and extract query string&params from path */
	}
	/* Else: no subdir, no '/' in the URL */
	else {
		/* 3b. Extract hostname */
		if(hostname) *hostname=strdup(ps);

		/* 4b. Extract path */
		if(path) *path=strdup("/");

		/* 5b. Extract dir */
		if(dir) *dir=strdup("/");

		/* 6b. Extract filename */
		// No filename, default!

		/* 7b. Extract dirURL */
		if(dirURL) {
			*dirURL=calloc(1, strlen(URL)+1+1);
			if(*dirURL) {
				strcat(*dirURL, URL);
				strcat(*dirURL, "/");
			}
		}

		/* TODO: to separate and extract query string &params from path */
	}

	return 0;
}


/*-------------------------------------
	Free an EGI_M3U8_LIST
--------------------------------------*/
void m3u_free_list(EGI_M3U8_LIST **list)
{
	if(list==NULL || *list==NULL)
		return;

	/* Free items */
	if( (*list)->ns > 0 ) {
		free((*list)->tsec);
		free((*list)->encrypt);
		egi_free_charList( &(*list)->msURI, (*list)->ns);
		egi_free_charList( &(*list)->keyURI, (*list)->ns);
		egi_free_charList( &(*list)->strIV, (*list)->ns);
	}

	/* Free self */
	free(*list);
	*list=NULL;
}

/*-----------------------------------------------------------------------
Parse HTTP Live streaming playlist file. and return a list of URL/URI for
media segments. They MAY be the final URL for a specified media file(usually
with extension.ts), OR just another m3u8 file (usually with extention .m3u/.m3u8)!

	--- Refrecne: RFC8216 HTTP Live Streaming ---
Note:
1. HLS playlist files MUST be encoded in UTF-8.
2. Each line of a HLS playlist is a URI, is blank, or starts with
   the character '#'.
3. All lines are terminated by either a single line feed character or
   a carriage return character followed by a line feed character.
4. There are ONLY two tags that withou '-X-' tokens, they are:
   'EXTM3U' and 'EXTINF'
5. Tags begin with #EXT, other lines begin with '#' are comments.
6. Each media segment is specified by a series of media segment tags
   followed by a URI.
   Media segment tags include:
   #EXTINF  <---- This tag is REQUIRED for each Media Segment!
   #EXT-X-BYTERANGE
   #EXT-X-DISCONTINUITY
   #EXT-X-KEY
   #EXT-X-MAP
   #EXT-X-PROGRAM-DATE-TIME
   #EXT-X-DATERANGE
7. A server MAY offer multiple Media Playlist files to provide different
   encodings of the same presentation(with different bandwidth etc.)
   Usually with tags of:
   #EXT-X-STREAM-INF
   #EXT-X-I-FRAME-STREAM-INF (containing the I-frames of a multimedia presentation.)
8. If NO #EXT-X-MEDIA-SEQUENCE tag found in the playlist, then it MUST set to be 0,
   which implys restart of ALL sequence numbering.

PARAMs:
@strHLS:   Pointer to a m3u playlist file content, encoded in UFT-8.
			!!! --- CAUTION --- !!!
	   Content in strHLS will be modified/changed by strtok().
Return:
	!NULL	OK
	NULL	Fails
------------------------------------------------------------------------*/
EGI_M3U8_LIST* m3u_parse_simple_HLS(char *strHLS)
{
	const char *delimNL="\r\n"; /* delim for new line.  DO NOT add SPACE here! */
	char *ps=NULL;
	char *saveps; /* For strtok_r(..) to save ps content */
	// char *pt, *savept, *pm, char *delimpt defined in section

	EGI_M3U8_LIST *list;
	bool isMasterList;
	int ns=0;		/* MediaSegment counter */
	char *ptmp=NULL;

	int lcnt;	/* Line counter */
	int mscnt;	/* Media segment counter */

	if(strHLS==NULL)
		return NULL;

	/* Init vars */
	lcnt=0;
	mscnt=0;
	ns=0;

	/* IF: A master playlist */
	if(strstr(strHLS, "#EXT-X-STREAM-INF:")) {
		isMasterList=true;

		/* Compute total segmemnt items */
		ps=strHLS;
		while( (ps=strstr(ps, "#EXT-X-STREAM-INF:")) ){
			/* ONLY if it appears as beginning of a line */
			if( ps!=strHLS )
				ns +=1;

			/* Pass it */
			ps += strlen("#EXT-X-STREAM-INF:");
		}
	}
	/* ELSE: A media segment list */
	else {
		isMasterList=false;

		/* Compute total segmemnt items */
		ps=strHLS;
		while( (ps=strstr(ps, "#EXTINF:")) ){
			/* ONLY if it appears as beginning of a line */
			if( ps!=strHLS )
				ns +=1;

			/* Pass it */
			ps += strlen("#EXTINF:");
		}
	}

	if(ns==0) {
		egi_dpstd("NO media segment address found!\n");
	   	return NULL;
	}

	/* Create m3u8 list */
	list=calloc(1, sizeof(EGI_M3U8_LIST));
	if(list==NULL)
		return NULL;

	/* Calloc items */
	list->ns=ns;
	list->isMasterList=isMasterList;
	list->encrypt=calloc(ns, sizeof(typeof(*list->encrypt)));
	list->msURI=calloc(ns, sizeof(char*));
	if(list->msURI==NULL || list->encrypt==NULL ) {
		m3u_free_list(&list);
		return NULL;
	}

	if(!isMasterList) {
		list->tsec=calloc(ns, sizeof(float*));
		list->keyURI=calloc(ns, sizeof(char*));
		list->strIV=calloc(ns, sizeof(char*));

		/* Check result */
		if(list->tsec==NULL || list->keyURI==NULL || list->strIV==NULL) {
			m3u_free_list(&list);
			return NULL;
		}
	}

	/* ALWAYS:
 	 * Init(calloc) list->seqnum =0! in case tag #EXT-X-MEDIA-SEQUENCE is NOT found, which implys
	 *  restart of ALL segment sequence numbering!
	 */

        /* Parse segment URL List */
        ps=strtok_r((char *)strHLS, delimNL, &saveps);
        while(ps!=NULL) {
	   egi_dpstd("line[]: %s\n", ps);

	   /* Check if a legal playlist */
	   if(lcnt==0 && strncmp("#EXTM3U",ps,7)!=0) {
		egi_dperr("The first tag line in a HLS playlist file MUST be #EXTM3U!\n");
		m3u_free_list(&list);
		return NULL;
	   }

	   /* W1. Tag lines starts with #EXT-X- */
	   if( strncmp("#EXT-X-", ps, 7)==0 ) {
		egi_dpstd("EXT-X- line: %s\n", ps+7);

		/* W1.1 #EXT-X-STREAM-INF:<attribute-list>  specifies a Variant Stream
		 *  Example: #EXT-X-STREAM-INF:BANDWIDTH=533579,CODECS="avc1.66.30,mp4a.40.34",RESOLUTION=320x240
		 */
		if(strncmp("#EXT-X-STREAM-INF:", ps, 18)==0) {
			egi_dpstd("Stream INF: %s\n", ps+18);
			/* TODO:  */

		}
		/* W1.2  #EXT-X-KEY:<attribute-list>.  If media segments are encrypted. */
		else if(strncmp("#EXT-X-KEY:", ps, 11)==0) {
			/* EXAMPLE: #EXT-X-KEY:METHOD=AES-128,IV=0x30303030303030303030303061c93924,URI="https://...",KEYFORMAT="identity"
		  	 */

			/* Extract KEY items */
			char *pt;
			char *pm=ps+strlen("#EXT-X-KEY:");
			const char *ptdelim=",\r\n "; /* Delim for key/value */
			char *savepm;
			pt=strtok_r(pm, ptdelim, &savepm);
			while(pt!=NULL) {
				/* METHOD */
				if(strncmp(pt,"METHOD=", 7)==0) {
					egi_dpstd("Encryption method: %s\n", pt+7);
					if( strncmp(pt+7, "AES-128", 7)==0 )
						list->encrypt[mscnt]=M3U8ENCRYPT_AES_128;
					else if( strncmp(pt+7, "SAMPLE-AES", 10)==0 )
						list->encrypt[mscnt]=M3U8ENCRYPT_SAMPLE_AES;
				}
				/* IV: TODO: An EXT-X-KEY tag with a KEYFORMAT of "identity" that does not have an IV attribute indicates
				 * that the Media Sequence Number is to be used as the IV
				 */
				else if(strncmp(pt,"IV=",3)==0) {
				   egi_dpstd("IV: %s\n", pt+3);
				   if(mscnt>=ns)
					egi_dpstd("mscnt>=ns! Media segments number ERROR!\n");
				   else
					list->strIV[mscnt]=strdup(pt+strlen("IV=")+2); /* +2 to get rid of 0x */
				}
				/* URI: uri for key value */
				else if(strncmp(pt,"URI=",4)==0) {
				   egi_dpstd("keyURI: %s\n", pt+4);
				   if(mscnt>=ns)
					egi_dpstd("mscnt>=ns! Media segments number ERROR!\n");
				   else {
					pt +=strlen("URI=");
					cstr_squeeze_string(pt, strlen(pt), '"'); /* Get rid if " */
					list->keyURI[mscnt]=strdup(pt);
				   }
				}
				/* KEYFORMAT */
				else if(strncmp(pt, "KEYFORMAT=", 10)==0) {
					egi_dpstd("Keyformat: %s\n", pt+strlen("KEYFORMAT="));
				}

		   		/* Next line */
	   			pt=strtok_r(NULL, ptdelim, &savepm);

			} /* End while */
		}
		/* W1.3  #EXT-X-TARGETDURATION:<s> specifies the maximum Media Segment duratio */
		else if(strncmp("#EXT-X-TARGETDURATION:", ps, 22)==0) {
			list->maxDuration=strtof(ps+22, NULL);
		}
		/* W1.4 #EXT-X-MEDIA-SEQUENCE:<number>  It indicates the Media Sequence Number of
   		 * the first Media Segment that appears in a Playlist file. IF NOT has this tag, then
  		 * the sequence number is 0!
		 */
		else if(strncmp("#EXT-X-MEDIA-SEQUENCE:", ps,22)==0) {
			list->seqnum=atoi(ps+22);
		}
	   }
	   /* W2. Two tags without '-X-': EXTINF and EXTM3U  */
 	   else if( strncmp("#EXT",ps, 4)==0 ) {
		/* EXTM3U: Start of playlist. */
		if(strncmp("#EXTM3U",ps,7)==0) {
			egi_dpstd("Start line with #EXTM3U, OK!\n");
		}
		/* #EXTINF:<duration>,[<title>]. */
		else if(strncmp("#EXTINF:",ps,8)==0) {
			egi_dpstd("EXTINF line: %s\n", ps+7);

		        if(mscnt>=ns)
				egi_dpstd("mscnt>=ns! Media segments number ERROR!\n");
			else
				/* Read segment duration */
				list->tsec[mscnt]=strtof(ps+strlen("#EXTINF:"), NULL);
		}

	   }
	   /* W3. Otherwise it's an URI */
	   else if( lcnt>0 && (ptmp=cstr_trim_space(ps)) ){
		egi_dpstd("media segment URI: %s\n", ptmp);

	        if(mscnt>=ns)
			egi_dpstd("mscnt>=ns! Media segments number ERROR!\n");
		else {
			/* Push to list */
			list->msURI[mscnt]=strdup(ptmp);

			/* Count mscnt */
			mscnt++;
		}

	   }

	   /* Count lcnt */
	   lcnt++;

	   /* Next line */
	   ps=strtok_r(NULL, delimNL, &saveps);

	} /* End while() */

	/* Finally */
	return list;
}


/*----------------------------------------------------------------------
Decode HTML entity names in a string into special chars(uft-8 coding).

		!!! --- CAUTION  --- !!!
1. Assume that length of each special char is short than its entity name!
2. Assume each entity name starts with '&' and ends with ';'.

@strHtml: string containing special entity names!

TODO:
1. To decode entity numbers, such as &#39; (&apos; does NOT work in IE)
2. To decode complex expression, such as ???
	....&amp;quot;item 6&amp;quot;... ==> &"item 6"&


Return:
	Pointer to decoded string (strHtml)	OK
	NULL	Fails
----------------------------------------------------------------------*/
char* cstr_decode_htmlSpecailChars(char *strHtml)
{
	if(strHtml==NULL)
		return NULL;

	char *ps=NULL; /* Start of searching position */
	char *pe=NULL; /* End of searching position */
	char *buff=NULL;
	char box[8+1];  /* For MAX long entity name */
	int i,k;
	int srclen=strlen(strHtml);
	int newlen;
	int xlen;
	bool checkOK; /* Check if an entity name possible, OR EOF */

	/* Entity name list. TODO: More! */
	struct compare_list {
		const char *ename; /* entity name */
		const char *uchar; /* uft8 code */
	} list[]={
		{"&lsquo;","‘"}, {"&rsquo;","’"}, {"&ldquo;","“"}, {"&rdquo;","”"},
		{"&spades;","♠"}, {"&clubs;","♣"}, {"&hearts;","♥"}, {"&diams;","♦"},
		{"&quot;","\""}, {"&amp;","&"}, {"&apos;","'"}, {"&lt;","<"},  {"&gt;",">"},
		{"&ndash;","-"},{"&hellip;","…"}, {"&nbsp;"," "},
	};

	int list_size=sizeof(list)/sizeof(struct compare_list);

	/* Malloc buff */
	buff=malloc(srclen+1);
	if(buff==NULL)
		return NULL;
	buff[srclen]=0;

	/* Init vars */
	newlen=0;
	ps=strHtml;
	pe=ps;
	box[8]=0; /* Terminating null */

	while( *pe ) {

	   /* W1. IF '&': as start of an entity name */
	   if( *pe=='&' ) {

		/* P1. Check if an ';' appears within 8 chars, as END of an entity name. */
		checkOK=false;
		for(i=1; i<8; i++) {
			/* A new '&' will reset. Example: "&xx&gt;" */
			if( *(pe+i)=='&' ) {
				//checkOK=false;
				break;
			}
			/* Find ';' OR just EOF. */
			else if( *(pe+i)==0 || *(pe+i)==';' ) {
				checkOK=true;
				break;
			}
		}

		/* P2. NO entity name in the coming first 8chars. */
		if(checkOK==false) {
			/* Copy skipping string to buff */
			strncpy(buff+newlen, ps, pe+i-ps);
			newlen += pe+i-ps;

			/* Renew ps and pe */
			pe += i;
			ps=pe;

			continue;
		}
		/* P3. JUST the end of string */
		else if( *(pe+i)==0 ) {
			pe += i;
			break;   /* BREAK WHILE(), goto X.  */
		}
		//ELSE: pe,ps, newlen: TO BE RESET LATER.


		/* ---- POSSIBLY AN ENTITY NAME ---- */

		/* 1.1 To hold entity_name_like string.
		 * NOW: *(pe+i) point to end ';', +1 to include it.
		 */
		strncpy(box, pe, i+1); /* MAX 8 +'0' */
		box[i+1]='\0';
		/* 1.2 Check if it's an entity name */
		for( k=0; k<list_size; k++) {
		   /* Tranverse entity names in list */
		   if( strstr(box, list[k].ename)==box ) {
			/* Copy skipping string to buff */
			strncpy(buff+newlen, ps, pe-ps);
			newlen += pe-ps;

			/* Copy uchar to buff */
			xlen=strlen(list[k].uchar);
			strncpy(buff+newlen, list[k].uchar, xlen);
			newlen += xlen;

			/* Renew ps and pe */
			pe += strlen(list[k].ename);
			ps=pe;

			break;
		   }
		}
		/* 1.3 Check if NOT found in list[]. XXX THIS SKIP BY 1.0  */
		if( k==list_size ) {
                        /* Copy skipping string to buff */
                        strncpy(buff+newlen, ps, pe+(i+1)-ps);
                        newlen += pe+(i+1)-ps;

		    	/* renew ps and pe */
			//pe +=i+1;
			pe += (i+1);
			ps=pe;
		}

		/* 1.4 Go on while()..  */
		continue;
	   }
	   /* W2. NOT '&' */
	   else {
		/* Move on pe */
		pe++;
		continue;
	   }

	} /* End while */

	/* X. The last */
	if( pe!=ps ) {
		/* Copy skipping string to buff */
		strncpy(buff+newlen, ps, pe-ps);
		newlen += pe-ps;
	}
	/* Y. Set NULL end */
	buff[newlen]='\0';

	/* Replace strHtml[] with buff[] */
	strncpy(strHtml, buff, newlen);
	strHtml[newlen]='\0';

	/* Free buff */
	free(buff);

	/* Ok */
	return strHtml;
}


/*----------------------------------------------------------
Replace the object string(obj) in the source(src) with
the substitue(sub).

@src:	Pointer poiner to the source string.

			!!! --- CAUTION --- !!!
	IF (sublen > objlen ):
	   Space of *src MUST be allocated by malloc/calloc,
	   since it will be reallocated in the function!
	ELSE
	   src MUST NOT point to a const string, Example: char* psrc="sdfsf..".

@obj:	The object/target string.
@sub:	The substitue string.

Return:
	Pointer to a new string:	OK
	NULL				Fails
-----------------------------------------------------------*/
char*   cstr_replace_string(char **src, const char *obj, const char *sub)
{
	if(src==NULL || *src==NULL || obj==NULL || sub==NULL)
		return NULL;

	int srclen=strlen(*src);
	int objlen=strlen(obj);
	int sublen=strlen(sub);

	char *buff=NULL;
	int  newlen;  /* buff len */
	char *newstr=NULL;
	char *ps=NULL; /* Start of searching position */
	char *pe=NULL; /* End of searching position */

	/* No need to reallocate *src! */
	if(sublen<=objlen) {
		buff=malloc(srclen+1);
		if(buff==NULL)
			return NULL;
		buff[srclen]=0;

		newlen=0;
		ps=*src;
		while( (pe=strstr(ps, obj)) ) {
			/* Copy skipping string to buff */
			strncpy(buff+newlen, ps, pe-ps);
			newlen += pe-ps;

			/* Copy sub to buff */
			strncpy(buff+newlen, sub, sublen);
			newlen += sublen;

			/* Renwe ps */
			ps=pe+objlen;
		}

		/* The last skipping string */
		if( (*src) + srclen > ps ) {
			strncpy(buff+newlen, ps, (*src)+srclen-ps);
			newlen += (*src)+srclen-ps;
		}

		/* Set the EOF null */
		buff[newlen]=0;
//		printf("buff: %s\n", buff);

		/* Replace *src with buff[] */
		strncpy(*src, buff, newlen);
		(*src)[newlen]=0;

		/* Ok */
		free(buff);
		return (*src);
	}
	/* TODO: Need to reallocate as mem space is NOT enough! */
	else {


		return newstr;
	}

}

/*-----------------------------------------------------------
Clear all specified char and squeeze the string.

@buff:  String buffer.
@size:  Total size of the buff.
 	!!! DO NOT use strlen(buff) to get the size, if the
	spot is '\0'.
@spot:  The specified character to be picked out.

Return:
        Total number of spot chars that have been cleared out.
------------------------------------------------------------*/
int cstr_squeeze_string(char *buff, int size, char spot)
{
        int i;
        int olen; /* Length of original buff, including spots in middle of the buff,
		   * but spots at the end of buff will NOT be counted in.
		   */
        int nlen; /* Length of new buff */

        for(i=0,nlen=0,olen=0; i<size; i++) {
                if( buff[i] != spot ) {
                        buff[nlen++]=buff[i];
			olen=i+1;
		}
                /* Original buff length, including middle spots.    In case the spot char is '\0'... */
//                if( buff[i] != spot)// && spot != '\0' )
//                        olen=i+1;
        }

        /* clear original remaining chars, which already have been shifted forward. */
        memset(buff+nlen,0,olen-nlen);

        return olen-nlen; /* Buff is squeezed now. */
}


/*-----------------------------------------------------------
Clear all unprintable chars in a string.

@buff:  String buffer.
@size:  Total size of the buff.

Return:
        Total number of spot chars that have been cleared out.
------------------------------------------------------------*/
int cstr_clear_unprintChars(char *buff, int size)
{
        int i;
        int olen; /* Length of original buff, including spots in middle of the buff,
		   * but spots at the end of buff will NOT be counted in.
		   */
        int nlen; /* Length of new buff */

        for(i=0,nlen=0,olen=0; i<size; i++) {
                if( isprint(buff[i]) ) {
                        buff[nlen++]=buff[i];
			olen=i+1;
		}
        }

        /* clear original remaining chars, which already have been shifted forward. */
        memset(buff+nlen,0,olen-nlen);

        return olen-nlen; /* Buff is squeezed now. */
}


/*-----------------------------------------------------------
Clear all control chars in a string.

@buff:  String buffer.
@size:  Total size of the buff.
 	!!! DO NOT use strlen(buff) to get the size, if the
	spot is '\0'.

Return:
        Total number of spot chars that have been cleared out.
------------------------------------------------------------*/
int cstr_clear_controlChars(char *buff, int size)
{
        int i;
        int olen; /* Length of original buff, including spots in middle of the buff,
		   * but spots at the end of buff will NOT be counted in.
		   */
        int nlen; /* Length of new buff */

        for(i=0,nlen=0,olen=0; i<size; i++) {
                if( iscntrl(buff[i]) ) {
                        buff[nlen++]=buff[i];
			olen=i+1;
		}
        }

        /* clear original remaining chars, which already have been shifted forward. */
        memset(buff+nlen,0,olen-nlen);

        return olen-nlen; /* Buff is squeezed now. */
}



/*------------------------------------------------------------------
Duplicate a file path string, then replace the extension name
with the given one.
	!!! --- Don't forget to free it! --- !!!

fpath:		a file path string.
new_extname:	new extension name for the fpath.

NOTE:
	1. The old extension name MUST have only one '.' char.
	   or the last '.' will be deemed as start of extension name!
	2. The new_extname MUST start with '.' !
	3. Remember to free the new_fpath later after call!

Return:
	pointer	to new string	OK
        NULL			Fail, or not found.
-------------------------------------------------------------------*/
char * cstr_dup_repextname(char *fpath, char *new_extname)
{
	char *pt=NULL;
	char *ptt=NULL;
	char *new_fpath;
	int  len,fplen;

	if(fpath==NULL || new_extname==NULL)
	return NULL;

	pt=strstr(fpath,".");
	if(pt==NULL)
		return NULL;

	/* Make sure its the last '.' in fpath */
	for( ; ( ptt=strstr(pt+1,".") ) ; pt=ptt)
	{ };

	/* duplicate and replace */
	len= (long)pt - (long)fpath +1;
	fplen= len + strlen(new_extname) +2;

	new_fpath=calloc(1, fplen*sizeof(char));
	if(new_fpath==NULL)
		return NULL;

	//snprintf(new_fpath,len,fpath);
	strncpy(new_fpath,fpath,len);
	strcat(new_fpath,new_extname);

	return new_fpath;
}



/*----------------------------------------------------------
Get pointer to the first char after Nth split char. content
of original string NOT changed.
Instead, you may use strtok().

TODO: TEST 

str:	source string.
split:  split char (or string).
n:	the n_th split
	if n=0, then return split=str;

Note:
1. Example str:"data,12.23,94,343.43"
	split=",", n=2 (from 0...)
	then get pointer to '9',
2. If "data,23,34,,,34"
	split=",", n=4
	then get pointer to ','??

Return:
	pointer	to a char	OK, spaces trimed.
        NULL			Fail, or not found.
----------------------------------------------------------*/
char * cstr_split_nstr(const char *str, const char *split, unsigned n)
{
	int i;
	char *pt;
	int slen;

	if(str==NULL || split==NULL)
		return NULL;

	pt=(char *)str;

	if(n==0)return pt;

	slen=strlen(split);

	for(i=1;i<=n;i++) {
		pt=strstr(pt,split);
		if(pt==NULL)return NULL;
		//pt=pt+1;
		if( strlen(pt) >= slen )
			pt += slen;
		else
			return NULL;
	}

	return pt;
}


/*--------------------------------------------------------------------
Skip all Spaces&Tabs at front, and trim all SPACES & '\n's & '\r's
at end of a string, return a pointer to the first non_space char.

Return:
	pointer	to a char	OK, spaces trimed.
        NULL			Input buf invalid
--------------------------------------------------------------------*/
char * cstr_trim_space(char *buf)
{
	char *ps, *pe;

	if(buf==NULL)return NULL;

	/* skip front spaces */
	ps=buf;
	for(ps=buf; *ps==' '|| *ps=='	'; ps++)
	{ };

	/* eat up back spaces/returns, replace with 0 */
	for(  pe=buf+strlen(buf)-1;
	      *pe==' '|| *pe=='	' || *pe=='\n' || *pe=='\r' ;
	      (*pe=0, pe-- ) )
	{ };

	/* if all spaces, or end token
	 * if ps==pe, means there is only ONE char in the line.
	*/
	if( (long)ps > (long)pe ) { //ps==pe || *ps=='\0' ) {
//		printf("%s: Input line buff contains all but spaces. ps=%p, pe=%p \n",
//					__func__,ps,pe);
			return NULL;
	}

	//printf("%s: %s, [0]:%c, [9]:%c, [10]:%c\n",__func__,ps,ps[0],ps[9],ps[10]);
	return ps;
}


/*-------------------------------------------------------------
Copy one line from src to buff, including '\n', with Max. bufsize
OR get to the end of src, whichever happens first. put '\0' at
end of the dest.

@src:	Source
@dest:	Destination
@size:  Size of dest buffer.

Return:
	>0	Ok, bytes copied, without '\0'.
	=0	src[0] is '\0'
	<0	Fails
-------------------------------------------------------------*/
int cstr_copy_line(const char *src, char *dest, size_t size)
{
	int i;

	if(src==NULL || dest==NULL)
		return -1;

	/* Max. size-1, leave 1byte for '\0' */
        for (i = 0; i < size-1 && src[i] != '\n' && src[i] != '\0'; i++)
                   dest[i] = src[i];

	/* put '\n' at end */
	if( src[i] == '\n' && i < size-1 ) {
		dest[i]=src[i];
		i++;
	}

	/* Clear left space, i Max. = size-1  */
	memset( dest+i,0, size-i );

	return i;
}



/*-----------------------------------------------------------------------
Get length of a character with UTF-8 encoding.

@cp:	A pointer to a character with UTF-8 encoding.
	OR any ASCII value, include '\0'.

Note:
1. UTF-8 maps to UNICODE according to following relations:
				(No endianess problem for UTF-8 conding)

	   --- UNICODE ---	      --- UTF-8 CODING ---
	U+  0000 - U+  007F:	0XXXXXXX
	U+  0080 - U+  07FF:	110XXXXX 10XXXXXX
	U+  0800 - U+  FFFF:	1110XXXX 10XXXXXX 10XXXXXX
	U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX
	(TODO: consider >U+1FFFF unicode conversion)

2. If illegal coding is found...

Return:
	>=0	OK, string length in bytes.
	<0	Fails
------------------------------------------------------------------------*/
inline int cstr_charlen_uft8(const unsigned char *cp)
{
	if(cp==NULL)
		return -1;

	if( *cp == '\0' )
		return 0;
	else if( *cp < 0b10000000 )
		return 1;	/* 1 byte */
	else if( *cp >= 0b11110000 )
		return 4;	/* 4 bytes */
	else if( *cp >= 0b11100000 )
		return 3;	/* 3 bytes */
	else if( *cp >= 0b11000000 )
		return 2;	/* 2 bytes */
	else {
		egi_dpstd("Unrecognizable starting byte for UFT8!\n");
		return -2;	/* unrecognizable starting byte */
	}
}


/*-----------------------------------------------------------------------
Get length of a character which is located BEFORE *cp, all in UTF-8 encoding.
Bytes that needs to move from cp to pointer to the previous character.

@cp:	A pointer to a character with UTF-8 encoding.
	OR any ASCII value, include '\0'.

Note:
1. UTF-8 maps to UNICODE according to following relations:
				(No endianess problem for UTF-8 conding)

	   --- UNICODE ---	      --- UTF-8 CODING ---
	U+  0000 - U+  007F:	0XXXXXXX
	U+  0080 - U+  07FF:	110XXXXX 10XXXXXX
	U+  0800 - U+  FFFF:	1110XXXX 10XXXXXX 10XXXXXX
	U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX
        (TODO: consider >U+1FFFF unicode conversion)

2. The caller MUST ensure that the pointer will NOT move out of range!

Return:
	>0	OK, length in bytes.
	<0	Fails
------------------------------------------------------------------------*/
inline int cstr_prevcharlen_uft8(const unsigned char *cp)
{
	int i;

	if(cp==NULL)
		return -1;

	/* ------------------------------
	U+  0000 - U+  007F:	0XXXXXXX
	--------------------------------*/
	if( *(cp-1) < 0b10000000 )
		return 1;

	/* ----------------------------------------------------------
        U+  0080 - U+  07FF:    110XXXXX 10XXXXXX
        U+  0800 - U+  FFFF:    1110XXXX 10XXXXXX 10XXXXXX
        U+ 10000 - U+ 1FFFF:    11110XXX 10XXXXXX 10XXXXXX 10XXXXXX
	----------------------------------------------------------- */
	for( i=1; i<5; i++) {
		if( *(cp-i) >= 0b10000000 ) {
			if( *(cp-i-1) >= 0b11000000 && i<4 )
				return i+1;
		}
	}

	return -2;	/* unrecognizable */
}


/*--------------------------------------------------
Get length of a string with UTF-8 encoding.

@cp:	A pointer to a string with UTF-8 encoding.

Return:
	>0	OK, string length in bytes.
	<0	Fails
-------------------------------------------------*/
int cstr_strlen_uft8(const unsigned char *cp)
{
	int len=0;
	int sum=0;

	while( *(cp+sum) != '\0'  &&  (len=cstr_charlen_uft8(cp+sum))>0 )
	{
		sum+=len;
	}

	return sum;
}


/*-----------------------------------------------------------------------
Get total number of characters in a string with UTF-8 encoding.

@cp:	A pointer to a string with UTF-8 encoding.

Note:
1. Count all ASCII or localt characters, both printables and unprintables.
2. UTF-8 maps to UNICODE according to following relations:
				(No endianess problem for UTF-8 conding)

	   --- UNICODE ---	      --- UTF-8 CODING ---
	U+  0000 - U+  007F:	0XXXXXXX
	U+  0080 - U+  07FF:	110XXXXX 10XXXXXX
	U+  0800 - U+  FFFF:	1110XXXX 10XXXXXX 10XXXXXX
	U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX
	(TODO: consider >U+1FFFF unicode conversion)

3. If illegal coding is found...
4. ZERO_WIDTH_NO_BREAK SPACE ( U+FEFF, unicode 65279) also counted in.

Return:
	>=0	OK, total numbers of character in the string.
	<0	Fails
------------------------------------------------------------------------*/
int cstr_strcount_uft8(const unsigned char *pstr)
{
	const unsigned char *cp;
	int size;	/* in bytes, size of the character with UFT-8 encoding*/
	int count;

	if(pstr==NULL)
		return -1;

	cp=pstr;
	count=0;
	while(*cp) {
		size=cstr_charlen_uft8(cp);
		if(size>0) {
			#if 0 /* Check ZERO_WIDTH_NO_BREAK SPACE */
			wchar_t wcstr=0;
			if( size==3 ) {
				char_uft8_to_unicode(cp, &wcstr);
				if(wcstr==65279)
					printf("%s: ZERO_WIDTH_NO_BREAK_SPACE found!\n",__func__);
			}
			#endif
			count++;
			cp+=size;
			continue;
		}
		else {
		   printf("%s: WARNGING! unrecognizable starting byte for UFT-8. Try to skip it.\n",__func__);
		   	cp++;
		}
	}

	return count;
}


/*------------------------------------------------------
Get pointer to the first char of an English word.
An English word is separated by any UFT-8 char which is NOT
an alphanumeric character.

@cp:	A pointer to a string with UTF-8 encoding, terminated
	with a '\0'.

Return:
	!NULL		OK
	NULL		Fails

----------------------------------------------------*/
char* cstr_get_peword(const unsigned char *cp)
{
	char *p;
	int len;

	if(cp==NULL)
		return NULL;

	p= (char *)cp;
	while(*p){
		/* Start of an English word: an English alphabet or an number */
		if( isalnum(*p) )
			return p;

		/* Shift p to next UFT8 char. */
		len=cstr_charlen_uft8((const unsigned char *)p);
		if(len<0) {
			egi_dpstd("UFT8 coding error, skip 1 byte!\n");
			len =1;
		}
		p+=len;
	}

	return NULL;
}


/*------------------------------------------------------
Get byte length of an English word.
An English word is separated by any UFT-8 char which is NOT
an alphanumeric character.
However, any trailing SPACEs/TABs are included in the eword!
This is to make left side of any word editor aligned.

@cp:	A pointer to a string with UTF-8 encoding, terminated
	with a '\0'.

Return:
	>=0	OK, length in bytes.
	<0	Fails
----------------------------------------------------*/
int cstr_strlen_eword(const unsigned char *cp)
{
	char *p;
//	int len;

	if(cp==NULL)
		return 0;

	p=(char *)cp;
	while(*p && isalnum(*p)) {
#if 0		/* XXX Shift p to next UFT8 char. */
		len=cstr_charlen_uft8((const unsigned char *)p);
		if(len<0) {
			egi_dpstd("UFT8 coding error, skip 1 byte!\n");
			len =1;
		}
		p+=len;

#else		/* !!! An alphanumeric character is ALWAYS 1 byte! */
		p+=1;
#endif
	}

	/* Include any trailing SPACEs/TABs */
	if(*p && p!=(char *)cp) {
		while( *p && isblank(*p) )
			p+=1;
	}

	return p-(char *)cp;
}


/*----------------------------------------------------------------------
Convert a character from UFT-8 to UNICODE.

@src:	A pointer to character with UFT-8 encoding.
@dest:	A pointer to mem space to hold converted characters in UNICODE.
	The caller shall allocate enough space for dest.
	!!!!TODO: Dest is now supposed to be in little-endian ordering. !!!!

1. UTF-8 maps to UNICODE according to following relations:
				(No endianess problem for UTF-8 conding)

      --- UTF-8 CODING ---	   	  --- UNICODE ---
 0XXXXXXX				U+  0000 - U+  007F
 110XXXXX 10XXXXXX			U+  0080 - U+  07FF
 1110XXXX 10XXXXXX 10XXXXXX		U+  0800 - U+  FFFF
 11110XXX 10XXXXXX 10XXXXXX 10XXXXXX	U+ 10000 - U+ 1FFFF
 (TODO: consider >U+1FFFF unicode conversion)

2. If illegal coding is found...

Return:
	>0 	OK, bytes of src consumed and converted into unicode.
                MAY be interrupted by invalid encoding in src.
	<=0	Fails, or unrecognizable uft-8 .
-----------------------------------------------------------------------*/
inline int char_uft8_to_unicode(const unsigned char *src, wchar_t *dest)
{
//	unsigned char *cp; /* tmp to dest */
	unsigned char *sp; /* tmp to src  */

	int size=0;	/* in bytes, size of the character with UFT-8 encoding*/

	if(src==NULL || dest==NULL )
		return 0;

	if( *src=='\0' )
		return 0;

//	cp=(unsigned char *)dest;
	sp=(unsigned char *)src;

	size=cstr_charlen_uft8(src);

       /* size<0 fails, But we want to have error printed in switch(default), so go on... */

#if 0 /////////////////////////////////////////////////////
	if(size=<0) {
		return 0;	/* ERROR */
	}

	/* U+ 0000 - U+ 007F:	0XXXXXXX */
	else if(size==1) {
	//	*dest=0;*/
	//	*cp=*src;	/* The LSB of wchar_t */
		*dest=*src;
	}

	/* U+ 0080 - U+ 07FF:	110XXXXX 10XXXXXX */
	else if(size==2) {
		/*dest=0;*/
		*dest= (*(sp+1)&0x3F) + ((*sp&0x1F)<<6);
	}

	/* U+ 0800 - U+ FFFF:	1110XXXX 10XXXXXX 10XXXXXX */
	else if(size==3) {
		*dest= (*(sp+2)&0x3F) + ((*(sp+1)&0x3F)<<6) + ((*sp&0xF)<<12);
	}

	/* U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX */
	else if(size==4) {
		*dest= (*(sp+3)&0x3F) + ((*(sp+2)&0x3F)<<6) +((*(sp+2)&0x3F)<<12) + ((*sp&0x7)<<18);
	}

	else {
		printf("%s: Unrecognizable uft-8 character! \n",__func__);
	}
#endif /////////////////////////////////////////////////////////////////////////////////////////

	switch(size)
	{
		/* U+ 0000 - U+ 007F:	0XXXXXXX */
		case	1:
			*dest=*src;
			break;

		/* U+ 0080 - U+ 07FF:	110XXXXX 10XXXXXX */
		case	2:
			*dest= (*(sp+1)&0x3F) + ((*sp&0x1F)<<6);
			break;

		/* U+ 0800 - U+ FFFF:	1110XXXX 10XXXXXX 10XXXXXX */
		case	3:
			*dest= (*(sp+2)&0x3F) + ((*(sp+1)&0x3F)<<6) + ((*sp&0xF)<<12);
			break;

		/* U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX */
		case	4:
			*dest= (*(sp+3)&0x3F) + ((*(sp+2)&0x3F)<<6) +((*(sp+2)&0x3F)<<12) + ((*sp&0x7)<<18);
			break;

		default: /* if size<=0 or size>4 */
			printf("%s: size=%d, Unrecognizable uft-8 character! \n",__func__,size);
			break;
	}

	return size;
}



/*-------------------------------------------------------------------------
Convert a character from UNICODE to UFT-8 encoding.
(MAX. size of dest[] is defined as UNIHAN_UCHAR_MAXLEN=4 in egi_unihan.h)

@src:	A pointer to characters in UNICODE.
@dest:	A pointer to mem space to hold converted character in UFT-8 encoding.
	The caller shall allocate enough space for dest.
	!!!!TODO: Now little-endian ordering is supposed. !!!!

1. UNICODE maps to UFT-8 according to following relations:
				(No endianess problem for UTF-8 conding)

	   --- UNICODE ---	      --- UTF-8 CODING ---
	U+  0000 - U+  007F:	0XXXXXXX
	U+  0080 - U+  07FF:	110XXXXX 10XXXXXX
	U+  0800 - U+  FFFF:	1110XXXX 10XXXXXX 10XXXXXX
	U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX
	(TODO: consider >U+1FFFF unicode conversion)

2. If illegal coding is found...

Return:
	>0 	OK, bytes of dest in UFT-8 encoding.
	<=0	Fail, or unrecognizable unicode, dest[] keeps intact.
--------------------------------------------------------------------------*/
inline int char_unicode_to_uft8(const wchar_t *src, char *dest)
{
	uint32_t usrc=*src;
//	uint32_t udest;

	int size=0;	/* in bytes, size of the returned dest in UFT-8 encoding*/

	if(src==NULL || dest==NULL )
		return 0;

	if( usrc > (uint32_t)0x1FFFF ) {
		printf("%s: Unrecognizable unicode as 0x%x > 0x1FFFF! \n",__func__, usrc );
		return -1;
	}
	/* U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX */
	else if(  usrc >  0xFFFF ) {
 	     //udest = (uint32_t)0xF0808080+(usrc&0x3F)+((usrc&0xFC0)<<2)+((usrc&0x3F000)<<4)+((usrc&0x1C0000)<<6);
	     dest[0]=0xF0+(usrc>>18);
	     dest[1]=0x80+((usrc>>12)&0x3F);
	     dest[2]=0x80+((usrc>>6)&0x3F);
	     dest[3]=0x80+(usrc&0x3F);
	     size=4;
	}
	/* U+ 0800 - U+ FFFF:	1110XXXX 10XXXXXX 10XXXXXX */
	else if( usrc > 0x7FF) {
 	  	//udest = 0xE08080+(usrc&0x3F)+((usrc&0xFC0)<<2)+((usrc&0xF000)<<4);
	    	dest[0]=0xE0+(usrc>>12);
	    	dest[1]=0x80+((usrc>>6)&0x3F);
	    	dest[2]=0x80+(usrc&0x3F);

		size=3;
	}
	/* U+ 0080 - U+ 07FF:	110XXXXX 10XXXXXX */
	else if( usrc > 0x7F ) {
		//udest = 0xC080+(usrc&0x3F)+((usrc&0x7C0)<<2);
	    	dest[0]=0xC0+(usrc>>6);
	    	dest[1]=0x80+(usrc&0x3F);

		size=2;
	}
	/* U+ 0000 - U+ 007F:	0XXXXXXX */
	else {
		//udest = usrc;
		dest[0]=usrc&0x7F;
		size=1;
	}

	return size;
}


/*--------------------------------------------------
Convert unicode of a char in DBC case(half_width) to
SBC case (full_width).

The conversion takes place according to following table:

        ASCII   Unicode(DBC)   Unicode(SBC)
        ------------------------------------
        SPACE   U+0020          U+3000
         !      U+0021          U+FF01
         "      U+0022          U+FF02
         #      U+0023          U+FF03

         ...    ...             ...

         |      U+007C          U+FF5C
         }      U+007D          U+FF5D
         ~      U+007E          U+FF5E


@dbc:   Unicode of the char in DBC case.
	Range from 0x20 to 0x7E only!

Return:
        Unicode of the char in SBC case.
--------------------------------------------------*/
inline wchar_t char_unicode_DBC2SBC(wchar_t dbc)
{
        /* 1. Out of range, SBC is same as DBC. */
        if( dbc < 0x0020 || dbc > 0x007E )
                return dbc;

        /* 2. SPACE */
        if(dbc==0x0020)
                return 0x3000;
        /* 3. Other ASCIIs */
        else
                return dbc+0xFEE0;
}

/*--------------------------------------------------
Convert an ASCII char in DBC case(half_width) to
SBC case (full_width), and pass out in UFT-8 encoding.

The conversion takes place according to following table:

        ASCII   Unicode(DBC)   Unicode(SBC)
        ------------------------------------
        SPACE   U+0020          U+3000
         !      U+0021          U+FF01
         "      U+0022          U+FF02
         #      U+0023          U+FF03

         ...    ...             ...

         |      U+007C          U+FF5C
         }      U+007D          U+FF5D
         ~      U+007E          U+FF5E


@dbc:   Unicode of the char in DBC case.
        Range from 0x20 to 0x7E only!

@dest:  Pointer to a string in UFT-8 encoding.
        The caller MUST ensure enough space!

Return:
        >0      OK, bytes of dest in UFT-8 encoding.
        <=0     Fail, or unrecognizable unicode and
                dest[] keeps intact.
--------------------------------------------------*/
inline int char_DBC2SBC_to_uft8(char dbc, char *dest)
{
	wchar_t wcode;

        if(dest==NULL)
                return -1;

        /* 1. Out of range, SBC is same as DBC. */
        if( dbc < 0x0020 || dbc > 0x007E ) {
                dest[0]=dbc;
                return 1;
        }

        /* 2. SPACE */
        if(dbc==0x0020) {
		wcode=0x3000;
                return char_unicode_to_uft8(&wcode, dest);
	}

        /* 3. Other ASCIIs */
        else {
                wcode=dbc+0xFEE0;
                return char_unicode_to_uft8(&wcode, dest);
        }
}


/*---------------------------------------------------------------------
Convert a string in UNICODE to UFT-8 by calling char_unicode_to_uft8()

@src:	Input string in UNICODE, it shall ends with L'\0'.
@dest:  Output string in UFT-8.
	The caller shall allocate enough space for dest.

Return:
	>0 	OK, converted bytes of dest in UFT-8 encoding.
	<=0	Fail, or unrecognizable unicode
---------------------------------------------------------------------*/
int cstr_unicode_to_uft8(const wchar_t *src, char *dest)
{
	wchar_t *ps=(wchar_t *)src;

	int ret=0;
	int size=0;	/* in bytes, size of the returned dest in UFT-8 encoding*/

	if(src==NULL || dest==NULL )
		return -1;

	while( *ps !=L'\0' ) {  /* wchar t end token */
		ret = char_unicode_to_uft8(ps, dest+size);
		if(ret<=0) {
			*(dest+size)=0; /* terminating byte. */
			return size;
		}

		size += ret;
		ps++;
	}

	/* End token */
	*(dest+size)=0;

	return size;
}


/*---------------------------------------------------------------------
Convert a string in UFT-8 encoding to UNICODEs by calling char_uft8_to unicode()

@src:	Input string in UFT-8 encoding.
@dest:  Output UNICODE array
	The caller shall allocate enough space for dest.

Return:
	>0 	OK, total number of UNICODE returned.
		MAY be interrupted by invalid encoding in src.
	<=0	Fails
---------------------------------------------------------------------*/
int cstr_uft8_to_unicode(const unsigned char *src, wchar_t *dest)
{
	unsigned int off;
	int chsize;
	int count;	/* Counter for UNICODEs */

	if( src==NULL || dest==NULL)
		return -1;

	off=0;
	count=0;
	while( (chsize=char_uft8_to_unicode(src+off, dest+count)) >0 ) {
		off += chsize;
		count ++;
	}

	return count;
}

/*-------------------------------------------------------------
	See in "egi_gb2unicode.h"
int   cstr_gb2312_to_unicode(const char *src,  wchar_t *dest)
-------------------------------------------------------------*/

/*---------------------------------------------------------------------------
Count lines in an text file. 	To check it with 'wc' command.
For ASCII or UFT-8 files.

@fpath: Full path of the file


Return:
	>=0	Total lines in the file.
	<0	Fails
----------------------------------------------------------------------------*/
int egi_count_file_lines(const char *fpath)
{
	int fd;
	char *fp;
	char *cp;
	size_t fsize;
	struct stat sb;
	int lines;

	if(fpath==NULL)
		return -1;

        /* Open file */
        fd=open(fpath,O_RDONLY);
        if(fd<0) {
		printf("%s: Fail to open '%s'!\n",__func__, fpath);
                return -2;
	}

        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
		printf("%s: Fail to call fstat()!\n", __func__);
                return -2;
	}

        /* Mmap file */
        fsize=sb.st_size;
        fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        if(fp==MAP_FAILED) {
		printf("%s: Fail to call mmap()!\n", __func__);
                return -3;
        }

	/* Count new lines */
	cp=fp;
	lines=0;
	while(*cp) {
		if( *cp == '\n' )
			lines++;
		cp++;
	}

        /* close file and munmap */
        if( munmap(fp,fsize) != 0 )
		printf("%s: WARNING: munmap() fails! \n",__func__);
        close(fd);


	return lines;
}




/*----------------------------------------------------------------------------------
Search given SECTION and KEY string in EGI config file, copy VALUE
string to the char *value if found.

@sect:		Char pointer to a given SECTION name.
@key:		Char pointer to a given KEY name.
@pvalue:	Char pointer to a char buff that will receive found VALUE string.

NOTE:
1. A config file should be edited like this:
# comment comment comment
# comment comment commnet
        # comment comment
  [ SECTION1]
KEY1 = VALUE1 #  !!!! all chars after '=' (including '#' and this comment) will be parsed as value of KEY2 !!!
KEY2= VALUE2

##########   comment
	####comment
##
[SECTION2 ]
KEY1=VALUE1
KEY2= VALUE2
...

1. Lines starting with '#' are deemed as comment lines.
2. Lines starting wiht '[' are deemed as start/end/boundary of a SECTION.
3. Non_comments lines containing a '=' are parsed as assignment for KEYs with VALUEs.
4. All spaces beside SECTION/KEY/VALUE strings will be ignored/trimmed.
5. If there are more than one section with the same name, only the first
   one is valid, and others will be all neglected.
6. If KEY value is found to be all SPACES, then it will deemded as failure. (see Return value 3 )

		[[ ------  LIMITS -----  ]]
7. Max. length for one line in a config file is 256-1. ( see. line_buff[256] )
8. Max. length of a SECTION/KEY/VALUE string is 64-1. ( see. str_test[64] )


TODO:	If key value includes BLANKS, use "".--- OK, sustained.


Return:
	3	VALE string is NULL
	2	Fail to find KEY string
	1	Fail to find SECTION string
	0	OK
	<0	Fails
------------------------------------------------------------------------------------*/
//#define EGI_CONFIG_LMAX		256	/* Max. length of one line in a config file */
//#define EGI_CONFIG_VMAX		64      /* Max. length of a SECTION/KEY/VALUE string  */
int egi_get_config_value(const char *sect, const char *key, char* pvalue)
{
	int lnum=0;
	int ret=0;

	FILE *fil;
	char line_buff[EGI_CONFIG_LMAX];
	char str_test[EGI_CONFIG_VMAX];
	char *ps=NULL, *pe=NULL; /* start/end char pointer for [ and  ] */
	char *pt=NULL;
	int  found_sect=0; /* 0 - section not found, !0 - found */

	/* check input param */
	if(sect==NULL || key==NULL || pvalue==NULL) {
		printf("%s: One or more input param is NULL.\n",__func__);
		return -1;
	}

	/* open config file */
	fil=fopen( EGI_CONFIG_PATH, "re");
	if(fil==NULL) {
		printf("%s: Fail to open EGI config file '%s', %s\n", __func__, EGI_CONFIG_PATH, strerror(errno));
		return -2;
	}
	//printf("Succeed to open '%s', with file descriptor %d. \n",EGI_CONFIG_PATH, fileno(fil));

	fseek(fil,0,SEEK_SET);

	/* Search SECTION and KEY line by line */
	while(!feof(fil))
	{
		lnum++;

		memset(line_buff,0,sizeof(line_buff));
		fgets(line_buff,sizeof(line_buff),fil);
		//printf("line_buff: %s\n",line_buff);

		/* 0. cut the return key '\r' '\n' at end .*/
		/*   OK, cstr_trim_space() will do it */

		/* 1.  Search SECTION name in the line_buff */
		if(!found_sect)
		{
			/* Ignore comment lines starting with '#' */
			ps=cstr_trim_space(line_buff);
			/* get rid of all_space/empty line */
			if(ps==NULL) {
//				printf("config file:: Line:%d is an empty line!\n", lnum);
				continue;
			}
			else if( *ps=='#') {
//				printf("Comment: %s\n",line_buff);
				continue;
			}
			/* search SECTION name between '[' and ']'
			 * Here we assume that [] only appears in a SECTION line, except comment line.
			 */
			ps=strstr(line_buff,"[");
			pe=strstr(line_buff,"]");
			if( ps!=NULL && pe!=NULL && pe>ps) {
				memset(str_test,0,sizeof(str_test));
				strncpy(str_test,ps+1,pe-ps+1-2); /* strip '[' and ']' */
				//printf("SECTION: %s\n",str_test);

				/* check if SECTION name matches */
				if( strcmp(sect,cstr_trim_space(str_test))==0 ) {
//					printf("Found SECTION:[%s] \n",str_test);
					found_sect=1;
				}
			}
		}
		/* 2. Search KEY name in the line_buff */
		else /* IF found_sect */
		{
			ps=cstr_trim_space(line_buff);
			/* bypass blank line */
			if( ps==NULL )continue;
			/* bypass comment line */
			else if( *ps=='#' ) {
//				printf("Comment: %s\n",line_buff);
				continue;
			}
			/* If first char is '[', it reaches the next SECTION, so give up. */
			else if( *ps=='[' ) {
//				printf("Get bottom of the section, fail to find key '%s'.\n",key);
				ret=2;
				break;
			}

			/* find first '=' */
			ps=strstr(line_buff,"=");
			/* assure NOT null and '=' is NOT a starting char
			 * But, we can not exclude spaces before '='.
			 */
			if( ps!=NULL && ps != line_buff ) {
				memset(str_test,0,sizeof(str_test));
				/* get key name string */
				strncpy(str_test, line_buff, ps-line_buff);
//				printf(" key str_test: %s\n",str_test);
				/* assure key name is not NULL */
				if( (ps=cstr_trim_space(str_test))==NULL) {
				   printf("%s: Key name is NULL in line_buff: '%s' \n",__func__, line_buff);
				   continue;
				}
				/* if key name matches */
				else if( strcmp(key,ps)==0 ) {
					//printf("found KEY:%s \n",str_test);
					ps=strstr(line_buff,"="); /* !!! again, point ps to '=' */
			           pt=cstr_trim_space(ps+1); /* trim ending spaces and replace \r,\n with \0 */
					/* Assure the value is NOT null */
					if(pt==NULL) {
					   printf("%s: Value of key [%s] is NULL in line_buff: '%s' \n",
										__func__, key, line_buff);
					   ret=3;
					   break;
					}
					/* pass VALUE to pavlue */
					else {
					   strcpy(pvalue, pt);
					   //printf("%s: Found  Key:[%s],  Value:[%s] \n",
					   //					__func__, key,pvalue);
					   ret=0; /* OK, get it! */
					   break;
					}
				}
			}
		}

	} /* end of while() */

	if(!found_sect) {
		printf("%s: Fail to find given SECTION:[%s] in config file.\n",__func__,sect);
		ret=1;
	}
#if 1
	/* log errors */
	if(ret !=0 ) {
		EGI_PLOG(LOGLV_ERROR,"%s: Fail to get value of key:[%s] in section:[%s] in config file.\n",
										      __func__, key, sect);
	}
	else {
		EGI_PLOG(LOGLV_CRITICAL,"%s: Get value of key:[%s] in section:[%s] in config file.",
										      __func__, key, sect);
	}
#endif
	fclose(fil);
	return ret;
}


/*---------------------------------------------------------------------------------
Update KEY value of the given SECTION in EGI config file, if the KEY
(and SECTION) does NOT exist, then create it (both) .

Note:
1. If EGI config file does not exist, then create it first.
2. If SECTION does not exist, then it will be put at the end of the config file.
3. If KEY does not exist, then it will be put just after SECTION line.
4. xxxxx A new line starting with "#####" also deemed as end of last SECTION,
   though it is not so defined in egi_get_config_value().  ---- NOPE! see the codes.

@sect:		Char pointer to a given SECTION name.
@key:		Char pointer to a given KEY name.
@pvalue:	Char pointer to a char buff that will receive found VALUE string.

Return:
	2	Fail to find KEY string
	1	Fail to find SECTION string
	0	OK
	<0	Fails
--------------------------------------------------------------------------------*/
int egi_update_config_value(const char *sect, const char *key, const char* pvalue)
{
	int lnum=0;

	int fd;
	char *fp;
	size_t fsize;
	off_t off;
	off_t off_newkey;  /* new key insert point, only when SECTION is found and KEY is NOT found. */
	struct stat sb;
	char line_buff[EGI_CONFIG_LMAX];
	char str_test[EGI_CONFIG_VMAX];
	char *ps=NULL, *pe=NULL; /* start/end char pointer for [ and  ] */
	char *pt=NULL;
	char *buff=NULL;
	size_t buffsize;
	int  found_sect=0; /* 0 - section not found, !0 - found */
	int  found_key=0; /* 0 - key not found, !0 - found */
	int  ret=0;
	int  nb;

	/* Check input param */
	if(sect==NULL || key==NULL || pvalue==NULL) {
		printf("%s: One or more input param is NULL.\n",__func__);
		return -1;
	}

        /* Open config file */
        fd=open(EGI_CONFIG_PATH, O_CREAT|O_RDWR|O_CLOEXEC, S_IRWXU|S_IRWXG);
        if(fd<0) {
                printf("%s: Fail to open EGI config file '%s', %s\n",__func__, EGI_CONFIG_PATH, strerror(errno));
                return -2;
        }

        /* Obtain file stat */
        if( fstat(fd,&sb)<0 ) {
		fprintf(stderr,"%s: Fail to call fstat()!\n", __func__);
                return -2;
	}
        fsize=sb.st_size;
	/* In case that the EGI config file is just created! otherwise mmap() will fail. */
	if(fsize==0) {
		printf("%s: fsize of '%s' is zero! create it now...\n",__func__, EGI_CONFIG_PATH);
		/* Insert file name at the beginning */
                memset(line_buff, 0, sizeof(line_buff));
       	        snprintf( line_buff, sizeof(line_buff)-1, "##### egi.conf #####\n");
               	if( write(fd, line_buff, strlen(line_buff)) < strlen(line_buff) ) {
                       	printf("%s: Fail to restore saved buff to config file.\n",__func__);
                       	ret=-7;
               	}
		/* Note: Use ftruncate() will put '\0' into the file first, and put SECT and KEY after then, that will lead to
		 *  failure in locating SECT and KEY later */

	        /* Obtain file stat again...*/
        	if( fstat(fd,&sb)<0 ) {
			fprintf(stderr,"%s: Fail to call fstat()!\n", __func__);
                	return -2;
		}
        	fsize=sb.st_size;
	}

        /* Mmap file */
        fp=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        if(fp==MAP_FAILED) {
		fprintf(stderr,"%s: Fail to call mmap()!\n", __func__);
                return -3;
        }

	/* Search SECTION and KEY */
	off=0;
	off_newkey=0;
	while( ( nb=cstr_copy_line(fp+off, line_buff, sizeof(line_buff)) )  > 0 )   /* memset 0 in func */
	{
		off += nb;
		lnum++;

		/* 1. To locate SECTION  first */
		if(!found_sect)
                {
                        /* Ignore comment lines starting with '#' */
                        ps=cstr_trim_space(line_buff);
                        /* get rid of all_space/empty line */
                        if(ps==NULL) {
//                              printf("config file:: Line:%d is an empty line!\n", lnum);
                                continue;
                        }
                        else if( *ps=='#') {
//                              printf("Comment: %s\n",line_buff);
                                continue;
                        }
                        /* search SECTION name between '[' and ']'
                         * Here we assume that [] only appears in a SECTION line, except comment line.
                         */
                        ps=strstr(line_buff,"[");
                        pe=strstr(line_buff,"]");
                        if( ps!=NULL && pe!=NULL && pe>ps) {
                                memset(str_test,0,sizeof(str_test));
                                strncpy(str_test,ps+1,pe-ps+1-2); /* strip '[' and ']' */
                                //printf("SECTION: %s\n",str_test);

                                /* check if SECTION name matches */
                                if( strcmp(sect,cstr_trim_space(str_test))==0 ) {
                                        printf("%s: SECTION:[%s] is found in line_%d: '%s'. \n",__func__, sect, lnum, line_buff);
                                        found_sect=1;

					/* set new key insert position, in case that KEY will NOT be found */
					off_newkey=off;
                                }
                        }
                }
		/* 2. Search KEY name in the line_buff */
		else if(!found_key) /* else: SECTION is found */
		{
			ps=cstr_trim_space(line_buff);
			/* bypass blank line */
			if( ps==NULL )continue;
			/* bypass comment line */
			else if( *ps=='#' ) {
//				printf("Comment: %s\n",line_buff);

				/*  A new line starting with "#####" also deemed as end of last SECTION */
				//if( ps == strstr(ps,"#####") )
				//	break;
				//else
					continue;
			}
			/* If first char is '[', it reaches the next SECTION, so give up. */
			else if( *ps=='[' ) {
//				printf("Get bottom of the section, fail to find key '%s'.\n",key);
				break;
			}

			/* find first '=' */
			ps=strstr(line_buff,"=");
			/* assure NOT null and '=' is NOT a starting char
			 * But, we can not exclude spaces before '='.
			 */
			if( ps!=NULL && ps != line_buff ) {
				memset(str_test,0,sizeof(str_test));
				/* get key name string */
				strncpy(str_test, line_buff, ps-line_buff);
//				printf(" key str_test: %s\n",str_test);
				/* assure key name is not NULL */
				if( (ps=cstr_trim_space(str_test))==NULL) {
				   	printf("%s: Key name is NULL in line_buff: '%s' \n",__func__, line_buff);
				   	continue;
				}
				/* if key name matches */
				else if( strcmp(key,ps)==0 ) {
					found_key=1;  /* We found the KEY, even it's value may be NULL! */

					ps=strstr(line_buff,"="); /* !!! again, point ps to '=' */
			           	pt=cstr_trim_space(ps+1); /* trim ending spaces and replace \r,\n with \0 */
					if(pt==NULL)
					   	printf("%s: Value of key [%s] is NULL in line_buff: '%s' \n",
										__func__, key, line_buff);
					else
						printf("%s: Value of key [%s] is found in line_%d: '%s'\n", __func__, key, lnum, line_buff);

					break;
				}
			}

		}  /* End else: search KEY */

	}  /* End while() */


	/* If SECTION NOT found, insert them at the end of the file. */
	if( !found_sect ) {
		memset(line_buff, 0, sizeof(line_buff));
		snprintf( line_buff, sizeof(line_buff)-1, "[%s]\n %s = %s\n", sect, key, pvalue );

		lseek( fd, 0, SEEK_END); /* go to end */
		if( write( fd, line_buff, strlen(line_buff) ) < strlen(line_buff) ) {
			printf("%s: Fail to write line_buff to config file.\n",__func__);
			ret=-4;
		}
	}
	/* Else , insert OR replace key and value at end of the SECTION, see above */
	else  {

		/* If not found key, set off_newkey just after SECTION */
		if( !found_key )
			off = off_newkey;
			//off -= nb;
		/* Else if found key, then keep off as to start of the next line,  we will replace the old line. */

		/* allocate buff for content after insertion point */
		buffsize=fsize-off;
		buff=calloc(1, buffsize);
		if(buff==NULL) {
			printf("%s: Fail to calloc buff!\n",__func__);
			ret=-5;
		}
		else {
			/* save content after insertion point */
			memcpy(buff, fp+off, buffsize);

			/* Insert key and value */
	                memset(line_buff, 0, sizeof(line_buff));
        	        snprintf( line_buff, sizeof(line_buff)-1, " %s = %s\n", key, pvalue );

			/* If found_key, rewind one line back here to the start of the old line, just to overwrite it. */
			if( found_key )
				off -= nb;

	                lseek( fd, off, SEEK_SET); /* go to insertion point */
                	if( write(fd, line_buff, strlen(line_buff)) < strlen(line_buff) ) {
                        	printf("%s: Fail to write line_buff to config file.\n",__func__);
                        	ret=-6;
                	}
			/* Restore saved content */
                	if( write(fd, buff, buffsize) < buffsize ) {
                        	printf("%s: Fail to restore saved buff to config file.\n",__func__);
                        	ret=-7;
                	}
			/* Truncate old remaining */
			if( fsize > off+strlen(line_buff)+buffsize )
				ftruncate(fd, off+strlen(line_buff)+buffsize );

			/* Free temp. buffer */
			free(buff);
		}
	}


        /* close file and munmap */
        if( munmap(fp,fsize) != 0 )
		fprintf(stderr,"%s: WARNING: munmap() fails! \n",__func__);
        close(fd);

	/* ---  Seems NOT necessary!? --- */
	/* Synch it immediately, in case any other process may access the EGI config file */
	//syncfs(fd); This function is not applicable?
        //sync();

	/* Return  value */
	return ret;
}


/*---------------------------------------------------------------------------------------
Get pointer to the beginning of HTML element content, which is defined between start tag
and end tag. It returns only the first matched case!


Note:
0. For UFT-8 encoding only! ...Indentation.
1. Input tag MUST be closed type, it appeares as <X> ..... </X> in HTML string.
   Void elements, such as <hr />, <br /> are not applicable for the function!
   If input html string contains only single <X> OR </X> tag, a NULL pointer will
   be returned.

1A.Input tag of "img" is also accepted. <img src="dog.jpg", alt="D.O.G">

2. The parameter *content has independent memory space if it is passed to the caller,
   so it's the caller's responsibility to free it after use!
3. The returned char pointer is a reference pointer to a position in original HTML string,
   so it needs NOT to be freed.
4. Default indentation of two SPACEs is inserted.


5. Limits:
   5.1 Length of tag.
   5.2 Nested conditions such as  <x>...<y>...</y>...</x> are NOT considered!

 TODO:  1. parse attributes between '<' and '>', such as 'id','Title','Class', and 'Style' ...
        2. there are nested setions such as  <x>...<y>...</y>...</x>:
		<figure class="section img">
			<a class="img-wrap" style="padding-bottom: 51.90%;" data-href="https://02imgmini.eastday.com/mobile/20191127/20191127132714_845bbcb4a1eed74b6f2f678497454157_1.jpeg" data-size="499x259">
				<img width="100%" alt="" src="https://....zz.jpeg" data-weight="499" data-width="499" data-height="259">
			</a>
		</figure>


@str_html:	Pointer to a html string with UFT-8 encoding.
@tag:		Tag name
		Example: "p" as paragraph tag
			 "h1","h2".. as heading tag

@attributes:    Pass out all attributes string in a start tag <X .....>,
		and put an end token '\0'.
		If NULL, ignore.
			!!! CAUTION !!!
		The caller MUST ensure enough sapce!
		For slef_closing tag, such as <img ...>, attributes are SAME as content!
@attrisize:     Size of attributes mem space, including space for'\0'.
@content:	Pointer to pass element content.
		Example: for <p>xyz...123</p>, content is 'xyz...123'/
		For slef_closing tag, such as <img ...>, content is SAME as attributes string.

		If input is NULL, ignore.
		if fails, pass NULL to the caller then.
		!!! --- Note: the content has independent memory space, and
		do NOT forget to free it. --- !!!

@length:	Pointer to pass length of the element content, in bytes.
		if input is NULL, ignore.
		if fails, pass NULL to the caller then.

Return:
	Pointer to taged content in str_html:		Ok
	NULL:						Fails
--------------------------------------------------------------------------------------*/
char* cstr_parse_html_tag(const char* str_html, const char *tag, char *attributes, int attrisize, char **content, int *length)
{
	int lenTag;
	char stag[16]; 	/* start tag */
	char etag[16]; 	/* end tag   */
	char *pst=NULL;	/* Pointer to the beginning of start tags in str_html
			 * then adjusted to the beginning of content later.
			 */
	char *pet=NULL;  	/* Pointer to the beginning of end tag in str_html */
	char *pattr=NULL; /* Pointer to start position of attributes in a start tag. */
	char *ptmp=NULL;
	//const char *pst2=NULL; /* For nested tags */
	int nestCnt=0;  /* Counter for inner nested tag pair */

	char str_indent[32];			/* UFT-8, indentation string */
	/* For LOCALE SPACE INDENTATION */
	const wchar_t wstr_indent[]={12288,12288, L'\0'}; /* sizelimit 32!! UNICODE, indentation string */
	int  len_indent; 	//strlen(str_indent);
	int  len_content=0;	/* length of content, in bytes. NOT include len_indent */

	char *pctent=NULL; /* allocated mem to hold copied content */


	/* Check input data */
	if(str_html==NULL || tag==NULL )
		return NULL;

	/* Check tag length  2021_12_08_HK_ */
	lenTag=strlen(tag);
	if( lenTag > 16-4 || lenTag<1 )
		return NULL;


	/* Set indentation string in UFT-8 */
	memset(str_indent,0, sizeof(str_indent));
	cstr_unicode_to_uft8(wstr_indent, str_indent);
	len_indent=strlen(str_indent);

	/* Reset content and length to NULL First!!! */
	if(content != NULL)
		*content=NULL;
	if(length != NULL)
		*length=0;


	/* Init. start/end tag */
	memset(stag,0, sizeof(stag));
	strcat(stag,"<");
	strcat(stag,tag);   /* Example: '<p' */
//	strcat(stag," ");   /* Separator! 2021_12_06_ZHK_  XXX  NOPE! Tell differenc: '<a' and '<alt'  */
	int lenStag=strlen(stag);
	/* Note: attributes may exists between '<x' and '>',
	   Example: <p class="section txt">
	 */
	//printf("stag: %s\n", stag);

	memset(etag,0, sizeof(etag));
	strcat(etag,"</");
	strcat(etag,tag);
	strcat(etag,">");   /* Example: '</p' */
	//int lenEtag=strlen(etag);
	//printf("etag: %s\n", etag);

	/* Locate start and end tag in html string, nested elements NOT considered! */
	pst=(char *)str_html;
	while( (pst=strstr(pst, stag)) ) {
		/* Get start positon of start_tag: <x, BUT NOT similar start as '<TAGa ..' to '<TAGalt ..' 2021_12_08_ZHK_*/
		if( pst[lenStag]==' ' || pst[lenStag]=='>' )
			break;
		else {
		    pst +=lenStag; /* skip it */

//		printf("pst[lenStag]='%c'\n", pst[lenStag]);
		}
	}


	/* MATCH_STAG:  Tell differenc: '<a...' and '<alt...' OR '<a>' */
	if( pst!=NULL && (pst[lenStag]==' ' || pst[lenStag]=='>') ) {

  	   /* 1. As an image Tag(Selfclosed tag type) :  <img src="dog.jpg", alt="D.O.G"> */
  	   if( strcmp(tag, "img")==0 ) {
		pst+=strlen(stag);
		pet=strstr(pst,">");	    /* Get end position of image tag > */

		if(pet==NULL) {
			egi_dpstd("Error! <img tag closing token '>' NOT found!\n");
			return NULL;
		}

		/* Extract attributes betwee '<img' and '>' */
		if(attributes!=NULL) {
			if(pet-pst>attrisize-1) {
				egi_dpstd("attributes space NOT enough!\n");
				return NULL;
			}

			strncpy(attributes, pst, pet-pst); /* pst NOW pointer to the '>' of the start tag */
			attributes[pet-pst]='\0';
		}

  	   }
  	   /* 2. As a paragraph Tag(Non_Selfclosed tag type):  <p>0....9</p>
	    *	 OR it MAY be nested!: <p>____<p>....</p>____</p>
	    */
  	   else {
		/* TODO: Parse attributes and elements between <x> and </x> */

#if 1 /* ----- Parse and include nested tag pairs ---------- */
		/* Pass nested same type Tag pairs like: ...<p>____<p>....</p>____</p>..
		 * To locate the outmost pair <p>... </p>
		 */

		/* Parse attributes betwee '<x' and '>' */
		if(attributes!=NULL)
			pattr=pst+lenStag; /* TODO: SPACE aft '<x' is inluded here. */

		/* Get to the end position '>' of the start_tag */
		pst=strstr(pst,">");
		if(pst==NULL) {
			egi_dpstd("Error! tag closing token '>' NOT found!\n");
			return NULL; /* Fails */
		}

		/* Extract attributes now */
		if(attributes!=NULL) {
			if(pst-pattr>attrisize-1) {
				egi_dpstd("attributes space NOT enough!\n");
				return NULL;
			}

			strncpy(attributes, pattr, pst-pattr); /* pst NOW pointer to the '>' of the start tag */
			attributes[pst-pattr]='\0';
		}

		/* Move pst to pass the '>' of <x ...>  */
		//if(pst!=NULL)
		pst+=1;

		/* NOW: pst point to the end of start tag.  Example: '<x ... >0123...', pst point to 0 now. */

		/* Loop search through nested tags(IF any), and get to the closing tag! */
		pet=NULL;
		ptmp=pst;
		while((ptmp=strstr(ptmp,tag))!=NULL) {

			/* found a new start tag: '<tag'.   Rule out similar tags, as: '<a..' to '<alt...' */
			if( *(ptmp-1)=='<' && (ptmp[lenTag]==' ' || ptmp[lenTag]=='>') ) { /* 2021_12_08_ZHK_ */
				nestCnt+=1; /* Increase nest counter */
				egi_dpstd("nestCnt=%d\n", nestCnt);

				ptmp += lenTag; //strlen(tag); /* search next.. */
				continue;
			}
			/* found a new end tag: '</tag' */
			else if( *(ptmp-1)=='/' && *(ptmp-2)=='<' ) {
				/* Finally, found the closing end tag for the very first Stag! */
				if(nestCnt==0) {
					pet=ptmp-2; /* etag points to '<' of '</x>' */
					break;
				}
				else {
					nestCnt-=1;
					//egi_dpstd(" nestCnt=%d\n", nestCnt);

					ptmp += lenTag; //strlen(tag); /* go next.. */
				}
			}
			/* Not within tag tokens <...> */
			else
				ptmp += lenTag; //strlen(tag); /* go next.. */
		}

		/* NOW: Get the corresponding end Tag(pet) for the start Tag(pst)!  OR pet==NULL as ERROR */

#else /* ----- DO NOT parse nested Tag pair ---------- */
		/* TODO: Parse attributes betwee '<x' and '>' */
		/* Get to the end position '>' of the start_tag */
		pst=strstr(pst,">");
		if(pst != NULL) {
			pst+=1;  /* Skip '>' */

			pet=strstr(pst,etag);   /* Get start position of end_tag */
		}
		/* pst==NULL, ERROR */
#endif

	   }

  	}
	/* NO_MATCH_STAG: */
	else  /* Reset pst to NULL, as it may be '<alt..' instead of '<a...' */
		pst=NULL;

	/* ---NOW:
	 * 1. <img src="dog.jpg", alt="D.O.G">:  pst points to the char aft '<img', and pet points to '>'.
	 * 2. <p>0...<p>...xxx...</p>...9</p>:  pst points to '0'; and pet points to '<'aft 9.  <p>0....9</p>
	 */

	/* Only if tag content is valid/available! */
	if( pst!=NULL && pet!=NULL ) {
		/* TODO:  parse attributes between '<' and '>' */

		/* Get length of content */
		//pst +=1; // strlen(">");	/* skip '>', move to the beginning of the content */
		//TODO: For Non_selfcoled tag: pst pass attributes to  get end of ">",

		len_content=pet-pst;

		if(len_content<1) {
			egi_dpstd("CAUTION! len_content=%d <1! No content? \n", len_content);
		}

		/* 1. Calloc pctent and copy content */
		if( content != NULL) {
			pctent=calloc(1, len_content+len_indent+1);
			if(pctent==NULL)
				printf("%s: Fail to calloc pctent...\n",__func__);
			else {
				/* Now pst is pointer to the beginning of the content */
				#if 1 /* Put indentation */
				strncpy(pctent,str_indent, len_indent);
				strncpy(pctent+len_indent, pst, len_content);
				#else /* No indentation */
				strncpy(pctent, pst, len_content);
				#endif
			}
		}
	}

	/* Pass to the caller anyway */
	if( content != NULL )
		*content=pctent;
	if( length !=NULL )
		*length=len_content;

	/* Now pst is pointer to the beginning of the content in str_html */
	return ( (pst!=NULL && pet!=NULL) ? pst:NULL);
}

/*--------------------------------------------------------------------
Extract a HTML attribute value from a string/text, in which an expression
of AttrName="Attr value"(OR 'Attr value') should exist.

Exampel text: .... class="shining-star" ....
	attribute name:class  value:shining-star

Note:
1. If not found, the first byte of *value will be set as '\0' anyway.
2. The quotation mark to be taken as the first char after the '=',
   it may be " OR '.

@str:		A string containing attribue values. exampel src="....".
@name:		Name of the attribute ( without '=' ).
@value:		Pointer to pass attribute value.
			!!! CAUTION !!!
		The caller MUST ensure enough space.

Return:
	0	OK
	<0	Fails.
	>0 	Not found.
---------------------------------------------------------------------*/
int cstr_get_html_attribute(const char *str, const char *name, char *value)
{
	char *pst=NULL, *pet=NULL;
	int len;
	char sname[16];
	char quot[2];	/* The quotation mark */

	if(value==NULL)
		return -1;

	if(str==NULL || name==NULL) {
		*value=0;
		return -2;
	}

	/* Add '=' after name */
	len=strlen(name);
	if(len > sizeof(sname)-2) {
		egi_dpstd("Attribute name too long! limit to %d bytes.", sizeof(sname)-2);
		*value=0;
		return -3;
	}

	strncpy(sname, name, sizeof(sname)-2);
	sname[len]='=';
	sname[len+1]=0;

	/* To locate the name */
	pst=strstr(str, sname);
	if(pst==NULL) {
		*value=0;
		return 1;
	}
	pst +=strlen(sname) +1; /* Move to the end of the first quotation mark!  " OR ' */

	/* Get the quotation mark  2021_12_06_ZHK_ */
	quot[0]=*(pst-1);
	quot[1]=0;

	/* To locate the end token " */
	pet=strstr(pst,quot);
	if(pet==NULL) {
		*value=0;
		return 2;
	}

	/* Copy to value */
	strncpy(value, pst, pet-pst);
	value[pet-pst]='\0';

	return 0;
}


/*------------------------------------------------------------
Extract all text blocks from the HTML string, they are assumed
to be placed between "...>" and "<...".
Note:
1. A '\n' will be added at end of each text block. BUT the last
   one will be erased.
2. Any SPACEs block will be ignored!


@str_html:	Pointer to a html string with UFT-8 encoding.
@text:		Pointer to pass out text.
		text[0] to be reset as 0 at first, anyway.

		!!! --- CAUTION --- !!!
		The Caller MUST ensure enough space.

@txtsize:	Max size of text. including '/0'.
		AT LEAST ==2!


Return:
	<0	Fails.
	>=0	Bytes of content read out.
------------------------------------------------------------*/
int cstr_extract_html_text(const char *str_html, char *text, size_t txtsize)
{
	if(str_html==NULL || text==NULL || txtsize<2 )
		return -1;

	const char *pst=str_html;
	char *pw=text;
	char *ps=NULL, *pe=NULL;
	char *px=NULL;

	/* 1. Preset text to NULL */
	//text[0]='\0'; see 4. */

	/* 2. Scan blocks */
	while( (ps=strstr(pst,">")) ) {
	   ps +=1; /* ps point to text, OR maybe '<', as NO text at all. */
           if( (pe=strstr(ps, "<")) ){

		/* Check if all SPACEs+LFs(2021_12_15_HK_), OR ps==pe! */
		for(px=ps; px<pe; px++) {
			//if( (*px)!=' ' && (*px)!='\n' ) /* If other NOT SPACE_or_LF found, OK to break */
			if( !isspace(*px) )/* If other NOT SPACE_or_LF found, OK to break */
				break;
		}
		if(px==pe) {
			pst =pe+1;
			continue; /* <---- */
		}

		/* Copy block into text */
		/* XXX TODO, to deal with(squeeze out) any '\n's between [ps pe], as it may happen in HTLM, Example: tag <span>...</span> */
	#if 0   /* DO NOT skip any '\n' */
		strncpy(pw, ps, pe-ps);
		pw +=pe-ps;
	        if(pw-text > txtsize-2) // ==txtsize-1
			goto END_FUNC;

	#else   /* To skip any '\n'  2021-12_09_ZHK_ */
		/* Skip all leading '\n' and SPACEs (2021_12_15_HK_) */
		px=ps;
		while( isspace(*px) ) {
			px++;
		}
		for( ; px<pe; px++) {
			if( *px != '\n' ) {
			    *pw = *px;
			     pw +=1;
			     if(pw-text > txtsize-2) // ==txtsize-1
				goto END_FUNC;
			}
		}
	#endif


	#if 0	/* Add an '\n' for block text end. */
		*pw='\n';
		pw +=1;
	        if(pw-text > txtsize-2) // ==txtsize-1
			goto END_FUNC;
	#endif

		/* Reset pst */
		pst =pe+1;
		continue;
	   }
	   else {
		egi_dpstd("Error or incomplete of HTML string, the end pair token '<' NOT found!\n");
		break;
	   }
	}

END_FUNC:
	/* 3. Erase the last '\n' */
	if( pw!=text && *(pw-1)=='\n' ) {
		// *(pw-1)='\0';
		pw -=1;
	}

	/* 4. Set terminating NULL! */
	*pw='\0';

	return  pw-text;
}


/*--------------------- !!! In testing... !!! -------------------------
Parse a simple HTML text.

@str_html:	Pointer to a html string with UFT-8 encoding.
@text:		Pointer to pass out text.
		text[0] to be reset as 0 at first, anyway.

		!!! --- CAUTION --- !!!
		The Caller MUST ensure enough space.
@txtsize:	Max size of text. including '/0'.
		AT LEAST ==2;

NOTE:
XXX 1. Content ONLY between tag pairs (<tag>....</tag>) will be read
XXX   to text, others in places such as </tagA>...<tagC> will be ignored!

    1. Anything after token '>' is regarded as text content!

Return:
	<0	Fails.
	>=0	Bytes of content read out.
----------------------------------------------------------------------*/
int cstr_parse_simple_html(const char *str_html, char *text, size_t txtsize)
{
	if(str_html==NULL || text==NULL || txtsize<2 )
		return -1;

	const char *pst=str_html;
	char *pw=text;		  /* Current pointer to write char into text */
	char *pwcs=pw;	  	  /* Start position of a tag content in the text
				   * Check this pointer to skip any leading white_spaces in tag content
				   */
//	char *ps=NULL, *pe=NULL;
//	char *px=NULL;

	char tagSname[16]; 	/* Tag name/type at start. Example 'a' of  <a .. .>  */
	char tagEname[16]; 	/* Tag name/type at end. Example: 'b' of </b>  */
	char *ptn=NULL;		/* Pionter to element of tagSname[] OR tagEname[]
				 * ALWAY check(ptn!=NULL) before ref/derefering it!
				 */

	/* To indicate current pst postion as per HTML context */
  	enum pos_stat_type {
	   STAT_NONE=0,		/* Aft. end of a tag, and before a new tag. */
	   BRK_START, 		/* '<'*/
	   BRK_END,		/* '>' */
	   TAG_SNAME,	    	/* X:  <X..   tag name at start */
	   TAG_ENAME,		/* X:  </x..  tag name at end */
	   TAG_END,	    	/* X:  </X */
	   TAG_ATTR,		/* X:  <ab XX...XX> */
	   TAG_CONTENT,		/* X:  >XX...XX<, NOW any chars btween any '>' and '<'  */
  	};
  	enum pos_stat_type postat=STAT_NONE;	/* Type of content current pst points to... */


	/* Clear text */
	text[0]='\0';

   while(*pst) {
//	printf("C: %c\n", *pst);

	/* W1. Check tokens, stat one byte by one byte */
	switch(*pst) {
	   /* S1. Parse tokens and continue to while() */
	   case '<':
		postat=BRK_START;
		pst++; continue; break;  /* -----> while */

	   case '>':

		/* See token'>' as start of text content anyway,  see C>4. */

		/* C>1. Start of tag content and end of tagSname */
		if(postat==TAG_ATTR ) {
			if(ptn) *ptn='\0'; /* Close tag name, all maybe already overflow! */

//see C>4.		postat=TAG_CONTENT;
//			pwcs=pw; /* Set pwcs as start position of new content */

/* TEST---> */		printf("TAG_SNAME: %s\n", tagSname);
		}
		/* C>2. END of tagSname */
		else if(postat==TAG_SNAME) {
			if(ptn)	*ptn='\0'; /* Close tag name, all maybe already overflow ! */

//see C>4.		postat=TAG_CONTENT;
//			pwcs=pw; /* Set pwcs as start position of new content */

/* TEST---> */		printf("TAG_SNAME: %s\n", tagSname);
		}
		/* C>3. END of tagEname */
		else if(postat==TAG_ENAME) {
			//postat=STAT_NONE;  ... NOPE! see as start of content anyway.
			if(ptn)*ptn='\0'; /* Close tag name, all  maybe already overflow! */

/* TEST---> */		printf("TAG_ENAME: %s\n", tagEname);


/* TEST: ------ Add NL OR SPACE for table element */
			if(strcasecmp(tagEname,"tr")==0) {
				*pw='\n';  pw++;
			}
			else if (strcasecmp(tagEname,"td")==0 || strcasecmp(tagEname,"th")==0) {
				*pw=' ';  pw++;
			}

			/* --->>> Check space */
			if( pw-text > txtsize-2 ) /* Leave 1 byte for EOF */
				goto END_FUNC;
/* TEST: END */

		}

		/* C>4. See token '>' it as start of text content anyway.
		 *  Put C>4 at last, NOT at start of the case, as User may add addition NL OR SPACE, see above.
		 */
		postat=TAG_CONTENT;
		pwcs=pw; /* Set pwcs as start position of new content */

		/* C>5. continue to while() */
		pst++; continue; //break; /* -----> while */

	   case '/':
		if(postat==BRK_START) {
			postat=TAG_ENAME;
			ptn=tagEname;
		}

		pst++; continue; break; /* -----> while */

	   /* S2. Non_defined tokens/assics */
           default:
		if(postat==BRK_START) {
			postat=TAG_SNAME;
			ptn=tagSname;
		}

		/* DONT_continue */

		break;
	}

	/* W2. Execute current byte, depending on current postat */
	switch(postat) {
	   case TAG_SNAME:
		/* SPACE: End of reading tagSname */
		if(*pst==' ') {  /* '>' ALSO as end token,  see at  case '>': */
			if(ptn)*ptn='\0'; /* Close tag name, all maybe already overflow! */
			postat=TAG_ATTR;
			//printf("TAG_SNAME: %s\n", tagSname); To print at '>' */

		}
		/* Read tag name at start half <tag> */
		else {
		    if(ptn) {
			*ptn=*pst;
			ptn++;

			/* Check size */
			if(ptn-tagSname == sizeof(tagSname)-1) {
				tagSname[sizeof(tagSname)-1]='\0';
				ptn=NULL;
				egi_dpstd("tagSname overflow! truncated to be '%s'.\n", tagSname);
			}
		    }
		}
		break;
	   case TAG_ENAME:
		if(ptn) {
			*ptn=*pst;
			ptn++;

                        /* Check size */
                        if(ptn-tagEname == sizeof(tagEname)-1) {
                                tagEname[sizeof(tagEname)-1]='\0';
                                ptn=NULL;
				egi_dpstd("tagEname overflow! truncated to be '%s'.\n", tagEname);
                        }
		}
		break;

	   case TAG_ATTR:	/* Get content of tag atrributes between < and > */
		/* TODO: read and parse attribue */
		break;

	   case TAG_CONTENT:
		/* Ignore any white-space characters at start... */
		if(  !isspace(*pst) || pw>pwcs ) {
			*pw=*pst;
			pw++;  		/* Check size at W3 */
		}

		break;

	   case STAT_NONE:
		break;

           default:
		break;
	}

	/* W3. Check text mem. space */
	if( pw-text > txtsize-2 ) /* ==txtsize-1; leave 1 for EOF */
		goto END_FUNC;

	/* W4. Move on.. */
	pst++;

   } /* END while() */


END_FUNC:
   /* Set terminating NULL! */
   *pw='\0';

   return  pw-text;
}

/* Time related keywords in CHN UFT-8 encoding */
static const char CHNS_TIME_KEYWORDS[]="0123456789零半个一二三四五六七八九十百千秒分刻钟小时点号日月年";

/*-------------------------------------------------------------------------
Extract time related string from UFT-8 encoded Chinese string.
It only extracts the first available time_related string contained in src.

Note:
 1. Time string MUST be continous. ????  --- See Note in the codes.
    Example: "862小时58分钟，72秒"

 2. substitue "定时" with "定定", to get rid of ambiguous '时':
     Error without substitution:
     scr: 1分20秒定时  	---->  extract: 1分20秒时  ---> getSecFrom: Time point: 00:00:00
     scr: 定时3小时  	---->  extract: 时3小时    ---> getSecFrom: Time duration: 0H_0Min_0Sec

@src:	Pointer to Source string.
@buff:	Pointer to extacted time string.
	Note: The caller MUST ensure enough space for then buff.
@bufflen: Space of the buff.


Return:
	0	Ok
	<0	Fails, or No time related string.
-------------------------------------------------------------------------*/
int cstr_extract_ChnUft8TimeStr(const char *src, char *buff, int bufflen)
{
	const char* sp=NULL;
	char *dp=buff;
	char *pt=NULL;
	char *srcbuff=NULL;
	long  size;
	char  uchar[8];
	int   ret=0;

	if( src==NULL || buff==NULL )
		return -1;

	/* copy src to srcbuff */
	size=cstr_strlen_uft8((const unsigned char *)src) +1;
	srcbuff=calloc(1, size);
	if(srcbuff==NULL) {
		printf("%s: Fail to calloc srcbuff with size=%ld bytes!\n",__func__,size);
		return -1;
	}
	strncpy(srcbuff,src,size);

	/* To rule out some ambiguous keywords */
	/* 1. substitue "定时" with "定定", to get rid of '时' */
	if( (pt=strstr(srcbuff,"定时")) ) {
		strncpy(pt,"定定",cstr_strlen_uft8((const unsigned char *)"定定") );
	}
	/* 2. TODO: to rule out more ambiguous keywords... */
	printf("%s: srcbuff='%s'\n",__func__,srcbuff);

	/* assign source pointer, sp */
	sp=srcbuff;

	/* If no time units  */
	if( strstr(sp,"秒")==NULL && strstr(sp,"分")==NULL && strstr(sp,"刻")==NULL && strstr(sp,"时")==NULL )
	{
		ret=-2;
		goto END_FUNC;
	}

	/* search time related keywords */
	while( *sp != '\0' ) {
		size=cstr_charlen_uft8((const unsigned char *)sp); /* bytes of one CHN uft8 char */
		/* check buff space */
		if( dp+size-buff > bufflen-1 ) {
			printf("%s: --- WARNING ---  buff len is NOT enough!\n",__func__);
			break;
		}
		memset(uchar,0,sizeof(uchar));
		strncpy(uchar,sp,size);
		//printf("uchar:%s\n",uchar);
		/* copy time keywords to buff */
		if( strstr(CHNS_TIME_KEYWORDS, uchar)!=NULL ) {
			strncpy(dp,sp,size);
			dp+=size;
		}
#if 0 /* 1  Enable it if time_related string MUST be continous. */
		else {
			/* If not the first time_related string, break then */
			if(dp!=srcbuff)
				break;
		}
#endif
		sp+=size;
	}

	/* No result */
	if(dp==srcbuff)
		ret=-3;

END_FUNC:
	free(srcbuff);
	return ret;
}


/* ----------------------------------------------------------------------------
Convert time-related UFT-8 encoded string to seconds. if no time-related keyword
(strunit[]) contained in the string, then it will return 0;
输入字符串中必须包含时间单位，不然返回0.

@src:	Pointer to Source string containing time_related words.
@tp:	Pointer to time_t, as preset time point.
	If tp==NULL, then ignore.
	If parsed result ts==0, then ignore.


0. If arabic number found, then call atoi() to convert it to numbers.

1. Parse "小时" "分钟" "秒" keywords in the string and add up to total seconds.

2. Suppose that "小时" "分钟" "秒" will appear no more than once in the string.

3. If "点" or "时" is found in the string then it will be deemed as a time point,
   otherwise it's a time span (time duration).

4. Digit palces greater than "千" are ignored to avoid recursive numbering.
   Example: "一千六百三十七万五千八百六十三"

5. 中文描述格式必须为: x千x百x十x   其中x为中文数字"零一二三四五六七八九"中之一。

   5.1 如果x为连续多个中文数字，那么仅第一个被搜索到的数有效。
   如： 三六八八九  被解释为 3
	九八三七四  被解释为 3 （3第一个被搜索到！）

   5.2 重复累计：
	35分钟10分钟   		被解释为45分钟

   5.3 单位必须大到小排列，单位由大到小逐一解释：
	五秒八分钟6小时		被解释为5小时

   5.4 时间单位前数字要么全部用阿拉伯数字，要么全部用中文数字，混合情况会出错：
   	三十九分78秒  	--->  39分78秒  OK!
   	九千零3十七小时	--->  9017小时  WRONG!!!
	1刻3十八秒      --->  15分3秒	WRONG!!!



TODO:  1. To parse string containing  "半个小时"

Return:
	>0	Ok, total time span in seconds.
	<=0	Fails, or No time related string.
--------------------------------------------------------------------------- */
int cstr_getSecFrom_ChnUft8TimeStr(const char *src, time_t *tp)
{
	int i,j,k;
	int num=0;
	int ones=0;
	char *ps=(char *)src; 	/* start of possible number string */
	char *pe=ps;  	        /* end of number string */
	char *pps=NULL, *ppe=NULL; /* to locate Thousand/Hundred/Ten digit place */
	char *pn=NULL;  /* to locate digit number */
	char *pu=NULL;  /* to locate single number in units postion  */
	char buff[128];	/* to buffer number string */
	long ts=0;	/* time span/duration in seconds, from NOW to the preset time point */
	time_t tnow;
	struct tm *tmp;	/* of the preset local time point, NOTE: struct tm * ONLY one case in a process. */

	/* get NOW time in struct tm */
	tnow=time(NULL);
	tmp=localtime(&tnow);

	/* time description as per strunit[]，
	 * if "点" or "时" >=0: its TIME POINT;  otherwise its TIME SPACE(DURATION)
	 */
	int	    tmkeys=9;

	int	    tmdes[9]   ={  -1,      0,        0,     -1,    0,     0,     0,     0,    0 };  /* time_point: -1, time_span: 0 */
	/* To incorporate with CHNS_TIME_KEYWORDS[], sorted by time value AND size of each strunit[]!!!  */
	const char *strunit[9] ={ "点", "半个小时", "小时", "时", "刻钟", "刻", "分钟", "分", "秒" }; 	/* exameple: put "分钟" before "分", and "小时" befor "时" */


	/* digit place: Thousand/Hundred/Ten */
	int	    dplace[3]	={ 1000, 100, 10};
	const char *strplace[3] ={ "千", "百", "十" };

	/* number chars */
	//const char *strnum[20]={ "0","1","2","3","4","5","6","7","8","9",
	//		      "零","一","二","三","四","五","六","七","八","九" };
	/*  Note: '零' in the string will be ignored:
	 *  Example: 一千零六十， 三百零八,  parsed to be  一千六十， 三百八,
	 */
	const char *strnum[9]={ "一","二","三","四","五","六","七","八","九" };

	if(src==NULL)
		return -1;

# if 0 /* ---- For test ONLY  -----*/
	/* check time-related words */
	for(i=0; i<tmkeys; i++) {
		printf("check %s\n", strunit[i]);
		if( strstr(src, strunit[i]) != NULL )
			break;
		if( i == (tmkeys-1) ) {
			printf("%s: No time_related Chinese key words found in the string!\n",__func__);
			return -2;
		}
	}
#endif

	/* Tranverse time units descriptor  strunit[i] */
	for(i=0; i<tmkeys; i++) {
		if( ( pe=strstr(ps, strunit[i])) != NULL )  {  /* --- To locate time unit --- */
			memset(buff,0, sizeof(buff));
			if( pe-ps+1 > 128-1 ) {
				printf("%s: Buff size NOT enough!\n",__func__);
				return -2;
			}
			strncpy(buff,ps, pe-ps ); /* copy digit string ( OR string before strunit[]) into buff */
			printf("\n String for time keyword '%s' segment: '%s'\n",strunit[i], buff);

			num = atoi(buff);
			printf("atoi('%s')=%d\n", buff, num);

			/* CASE chinese char number: to locate digit place before time unit */
			if(num==0) {
				pu=buff;
				/* To locate digit place before time unit */
				pps=ppe=buff;
				for(j=0; j<3; j++) {
					if( (ppe=strstr(pps, strplace[j])) != NULL ) {  /* --- To locate digit place ...千百十， 从大单位到小单位 --- */
						printf("	%s位: ",strplace[j]);

						/* to locate digit number, before digit place */
						ones=1; /* Presume digit number 1 for all digit place.
							 * Example: 十五, 百二十，presumed as 一十五， 一百二十
							 * Renew it later if digit number exists. */

						pn=NULL;
						for(k=0; k<9; k++) {  /* k=20 */
							//if( (pn=strstr(buff+pos, strnum[k]))!=NULL && pp-pn > 0 ) {
							if( (pn=strstr(pps, strnum[k]))!=NULL && ppe-pn > 0 ) {
								printf("		%s\n",strnum[k]);
								ones=k+1; /* --- renew digit number ----*/
								break; /* Only one digit number for each digit place unit */
							}
						}

						/* ones==1, inexplict: 三百十五 not as 三百一十五  */
						if(pn==NULL || ppe-pn<0 ) printf("		一\n");

						/* add up tmdes[i] */
						if(tmdes[i]<0) tmdes[i]=0; /* init value may be -1 */
						tmdes[i] += ones*dplace[j];

						/* move pu, as for unit position */
						pu = ppe+cstr_strlen_uft8((const unsigned char *)strplace[j]);

						/* adjust ppe */
						pps=pu;

					}
					else {  /* Digit place strplace[j] not found */
						printf("	%s位:		零\n",strplace[j]);
					}
				}
				/* To locate Single digit in units position */
				printf("	个位:		%s\n",pu);
				for(k=0; k<9; k++) {
					if( strstr(pu, strnum[k])!=NULL ) {
						if(tmdes[i]<0) tmdes[i]=0; /* init value may be -1 */
						tmdes[i] += k+1;	/* i as for strunit[i] */
						break;
					}
				}
			}

			/* CASE arabic numbers: to extract arabic number */
			else {
				tmdes[i] = num;
			}

			/* move on ps */
			ps = pe+cstr_strlen_uft8((const unsigned char *)strunit[i]);
		}
	}

	/* Parsed as a time point: tmdes[0] "点"  tmdes[3] "时" */
	if(tmdes[0] >=0 ) {  	  /* containing "点" */

		/* Modify tmp as to preset H:M:S */
		tmp->tm_hour = tmdes[0];
  		tmp->tm_min  = tmdes[1]*30+(tmdes[4]+tmdes[5])*15+tmdes[6]+tmdes[7];
		tmp->tm_sec  = tmdes[8];
		printf("Preset time point: %02d:%02d:%02d \n", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		/* pass out tp */
		if( tp != NULL )
			*tp=mktime(tmp);

		/* return time span in seconds */
		ts=mktime(tmp)-tnow;
		printf("mktime(tmp)=%ld, tnow=%ld Time span ts=tmp-tnow=%ld\n", mktime(tmp),tnow,ts);
		return ts;
	}
	else if(tmdes[3] >=0 ) {  /* containing "时" */

		/* Modify tmp as to preset H:M:S */
		tmp->tm_hour = tmdes[3];
  		tmp->tm_min  = tmdes[1]*30+(tmdes[4]+tmdes[5])*15+tmdes[6]+tmdes[7];
		tmp->tm_sec  = tmdes[8];
		printf("Preset time point: %02d:%02d:%02d \n", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		/* pass out tp */
		if( tp != NULL )
			*tp=mktime(tmp);

		/* return time span in seconds */
		ts=mktime(tmp)-tnow;
		printf("mktime(tmp)=%ld, tnow=%ld Time span ts=tmp-tnow=%ld\n", mktime(tmp),tnow,ts);
		return ts;

	}

	/* Parsed as time span or duration */
	else  {
		printf("Time duration: %dH_%dMin_%dSec\n", tmdes[2], tmdes[1]*30+(tmdes[4]+tmdes[5])*15+tmdes[6]+tmdes[7], tmdes[8]);
		ts=tmdes[2]*3600 + ((tmdes[4]+tmdes[5])*15+tmdes[1]*30+tmdes[6]+tmdes[7])*60 + tmdes[8];

		/* pass out tp */
		if( tp!=NULL && ts>0 ) {
			*tp= tnow+ts;
		}
	}

	/* return time span/duration in seconds */
	return ts;
}


/*------------------------------------------------------------
A simple way to calculate hash value of a string.
Refrence: https://cp-algorithms.com/string/string-hashing.html

hash(s)=s[0]+s[1]*p+s[2]*p^2+s[3]*p^3+....+s[n-1]*p^(n-1)  mod M
Where: p=251;		( ASCII: 0 - 255, as unsigned char )
       M=10^9+9; 	( int range: -2147483648 ~ +2147483647)

@pch: Pointer to a string.
@mod: Modulus, if 0, take default as 1e9+9

Return: Hash value of the string.
------------------------------------------------------------*/
long long int cstr_hash_string(const unsigned char *pch, int mod)
{
	const int p=251;
	long long int hash=0;
	long long int pw=1;

	if(pch==NULL)
		return 0;

	if(mod==0)
		mod=1e9+9;

	while( *pch ) {
		hash=(hash+((*pch)+1)*pw);
		//printf("pch=%d, pw=%lld, hash=%lld, mod=%d\n", *pch, pw, hash, mod);
		hash=hash%mod;
		pw=(pw*p)%mod;
		pch++;
	}

	return hash;
}


