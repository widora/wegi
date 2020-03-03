/*----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

An example for www.juhe.com https news interface.


Midas Zhou
-----------------------------------------------------------------------*/
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <libgen.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <fcntl.h>
#include <errno.h>
#include "egi_common.h"
#include "egi_https.h"
#include "egi_cstring.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"
#include "page_minipanel.h"
#include "juhe_news.h"


/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	size_t session_size=size*nmemb;

	//printf("%s: session_size=%zd\n",__func__, session_size);

        strcat(userp, ptr);

        return session_size;
}


/*-----------------------------------------------
 A callback function for write data to file.
------------------------------------------------*/
size_t download_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
       size_t written;

	written = fwrite(ptr, size, nmemb, (FILE *)stream);
	//printf("%s: written size=%zd\n",__func__, written);

        return written;
}


/*--------------------------------------------------------------------------
Parse JUHE.cn returned data and get error_code.

@strinput       input juhe.cn free news return  string

Return:
	<0	Fails
	>=0	JUHE error_code in returned JUHE json string
--------------------------------------------------------------------------*/
int juhe_get_errorCode(const char *strinput)
{
	int error=0;

        json_object *json_input=NULL;
	json_object *json_error=NULL;

        /* Parse returned string */
        json_input=json_tokener_parse(strinput);
        if(json_input==NULL)
		return -1;

	/* Get error_code json obj */
	json_object_object_get_ex(json_input,"error_code",&json_error);
	if(json_error==NULL) {
		error=-2;
		goto GET_FAIL;
	}

	/* Get error_code */
	error=json_object_get_int(json_error);
	if( errno==EINVAL ) {
		EGI_PLOG(LOGLV_WARN,"%s: errno is set to be EINVAL!",__func__);
		//error=-3;
		//got GET_FAIL;
	}

GET_FAIL:
	/* free json */
	json_object_put(json_input);

	return error;
}

/*--------------------------------------------------------------------------
Parse JUHE.cn returned data and get length of 'data' array, as total number
of news items in the strinput.

@strinput       input juhe.cn free news return  string

Return:
        Total number of news items in the strinput.
--------------------------------------------------------------------------*/
int juhe_get_totalItemNum(const char *strinput)
{
	int total=0;
        json_object *json_input=NULL;
        json_object *json_result=NULL;
        json_object *json_array=NULL;

        /* parse returned string */
        json_input=json_tokener_parse(strinput);
        if(json_input==NULL) {
		printf("%s: json_input is NULL!\n",__func__);
		goto GET_FAIL;
	}

        json_object_object_get_ex(json_input,"result",&json_result); /* Ref count NOT change */
        if(json_result==NULL) {
		printf("%s: json_result is NULL!\n",__func__);
		goto GET_FAIL;
	}

	json_object_object_get_ex(json_result,"data",&json_array);
        if(json_array==NULL) {
		printf("%s: json_array is NULL!\n",__func__);
		goto GET_FAIL;
	}

	total=json_object_array_length(json_array);

GET_FAIL:
	/* Free json obj */
        json_object_put(json_input);

	return total;
}


/*--------------------------------------------------------------------------------------------
Parse juhe.cn news json string and return string pointer to the vale of specified strkey of
data[index], or to data[index] if strkey is NULL.

 		-----  JUHE.cn returned data format -----
{
        "reason":"成功的返回",
        "result":{
                "stat":"1",
                "data":[
                        {
                                "uniquekey":"af9debdd0055d05cdccd18a59e4067a8",
                                "title":"花钱买空气！印度空气质量告急 民众可花50元吸氧15分钟",
                                "date":"2019-11-19 15:07",
                                "category":"国际",
                                "author_name":"海外网",
                                "url":"http:\/\/mini.eastday.com\/mobile\/191119150735829.html",
                                "thumbnail_pic_s":"http:\/\/06imgmini.eastday.com\/mobile\/20191119\/2019111915$
                        },
			... ...  data array ...
		...
	...
	jpg"}]},
	"error_code":0
}

Note:
        !!! Don't forget to free the returned string pointer !!!

@strinput       input juhe.cn free news return string
@index		index of news array data[]
@strkey         key name of the news items in data[index]. Example: "uniquekey","title","url"...
		See above juhe data format.
                Note: if strkey==NULL, then return string pointer to data[index].

Return:
        0       ok
        <0      fails
----------------------------------------------------------------------------------------------*/
char* juhe_dupGet_elemValue(const char *strinput, int index, const char *strkey)
{
        char *pt=NULL;

        json_object *json_input=NULL;
        json_object *json_result=NULL;
        json_object *json_array=NULL; /* Array of news items */
        json_object *json_data=NULL;
        json_object *json_get=NULL;

        /* parse returned string */
        json_input=json_tokener_parse(strinput);
        if(json_input==NULL) goto GET_FAIL;

        /* strip to get array data[]  */
        json_object_object_get_ex(json_input,"result",&json_result); /* Ref count NOT change */
        if(json_result==NULL)goto GET_FAIL;
	json_object_object_get_ex(json_result,"data",&json_array);
        if(json_array==NULL)goto GET_FAIL;

	/* Get an item by index from the array , TODO: limit index */
	json_data=json_object_array_get_idx(json_array,index);  /* Title array itmes */
	if(json_data==NULL)goto GET_FAIL;
	//print_json_object(json_data);

        /* if strkey, get key obj */
        if(strkey!=NULL) {
                json_object_object_get_ex(json_data, strkey, &json_get);
                if(json_get==NULL)goto GET_FAIL;
        } else {
                json_get=json_data;
        }

        /* Get pointer to the item string */
        pt=strdup((char *)json_object_get_string(json_get));

GET_FAIL:
        json_object_put(json_input);
	json_object_put(json_data);

        return pt;
}


/*--------------------------------------
	Print a json object
--------------------------------------*/
void  print_json_object(const json_object *json)
{

//	char *pstr=NULL;
//	pstr=strdup((char *)json_object_get_string((json_object *)json));
//	if(pstr==NULL)
//		return;
	printf("%s\n", (char *)json_object_get_string((json_object *)json));
//	free(pstr);
}


/*-----------------------------------------------------------------
Get html text from URL or a local html file, then extract paragraph
content item by item, each item is saved to an allocated memory,
Addresses of those memories are pushed to an EGI_FILO and passed to
the caller finally.

Limit:
1. Try only one HTTP request session and max. buffer is
   CURL_RETDATA_BUFF_SIZE.

@url:	  URL address of html
	  If it contains "//", it deems as a web address and
	  will call https_curl_request() to get content.
	  Else, it is a local html file.

Return:
	A FILO pointer		Ok
	NULL			Fails
------------------------------------------------------------------*/
EGI_FILO* juhe_get_newsFilo(const char* url)
{
	int ret=0;
//	int i;
	int fd=-1;			   /* file descreptor for the input file */
	int fsize=0;			   /* size of the input file */
	struct stat  sb;
	bool URL_Is_FilePath=false;	   /* If the URL is a file path */
	char buff[CURL_RETDATA_BUFF_SIZE]; /* For CURL returned html text */
        char *content=NULL;		   /* Pointer to content of a piece of news */
        int len;
        char *pstr=NULL;		   /* Pointer to file OR buff */
	char *pmap=NULL;		   /* mmap of a file */
	EGI_FILO* filo=NULL; 		   /* to buffer content pointers */


	if(url==NULL)
		return NULL;

	/* init filo */
	printf("%s: init FILO...\n",__func__);
	filo=egi_malloc_filo(8, sizeof(char *), FILO_AUTO_DOUBLE);
	if(filo==NULL)
		return NULL;

	/* Check if it's a web address or a file path */
   	if( strstr(url,"//") == NULL )
		URL_Is_FilePath=true;
	else
		URL_Is_FilePath=false;

	/* IF :: Input URL is a Web address */
	if( URL_Is_FilePath==false )
	{
        	/* clear buff */
	        memset(buff,0,sizeof(buff));
        	/* Https GET request */
		printf("%s: https_curl_request...\n",__func__);
	        if(https_curl_request(url, buff, NULL, curlget_callback)!=0) {
        	         printf("Fail to call https_curl_request()!\n");
			 ret=-1;
			 goto FUNC_END;
		}

		/* assign buff to pstr */
        	pstr=buff;
	}
	/* ELSE :: Input URL is a file path */
	else
	{
	       /* open local file and mmap */
        	fd=open(url,O_RDONLY);
	        if(fd<0) {
			printf("%s: Fail to open file '%s'\n", __func__, url);
			ret=-2;
                	goto FUNC_END;
	        }
        	/* obtain file stat */
	        if( fstat(fd,&sb)<0 ) {
			printf("%s: Fail to get fstat of file '%s'\n", __func__, url);
			ret=-3;
                	goto FUNC_END;
	        }
        	fsize=sb.st_size;
	        pmap=mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
        	if(pmap==MAP_FAILED) {
			printf("%s: Fail to mmap file '%s'\n", __func__, url);
			ret=-4;
	                goto FUNC_END;
        	}
		/* assign pmap to pstr */
		pstr=pmap;
	}

        /* Parse HTML and push to FILO */
        do {
                /* parse returned data as html text, extract paragraph content and push its pointer to FILO */
//		printf("%s: parse html tag <paragrah>...\n",__func__);
                pstr=cstr_parse_html_tag(pstr, "p", &content, &len); /* Contest is allocated if succeeds! */
                if(content!=NULL) {
                        //printf("%s: %s\n",__func__, content);
//			printf("%s: push content pointer to FILO ...\n",__func__);
			if( egi_filo_push(filo, &content) !=0 ) /* Push a pointer to FILO */
				EGI_PLOG(LOGLV_ERROR,"%s: fail to push content to FILO: %s.",__func__, content);
			content=NULL; /* Ownership transformed */
		}
                //printf("Get %d bytes content: %s\n", len, content);
        } while( pstr!=NULL );
	printf("%s: finish parsing HTML...\n",__func__);


FUNC_END:
	/* Close file and mumap */
	printf("%s: release file mmap ...\n",__func__);
	if(URL_Is_FilePath) {
		if(fd>0)
		    	close(fd);
		if( pmap!=MAP_FAILED && pmap!=NULL )
			munmap(pmap,fsize);
	}

	/* If fail, free filo */
	if(ret!=0) {
		/* Free all content pointers in FILO  */
		printf("%s:filo pop pointers to content...\n",__func__);
		content=NULL; /* set content as NULL first! */
		while( egi_filo_pop(filo, &content)==0 )
			free(content);

		/* Free FILO */
		egi_free_filo(filo);

		return NULL;
	}

	/* Retrun FILO */
	return filo;
}



/*--------------------------------------------------------
Save (html)text buffer to a file.

@fpath:   File path for saving.
@buff:	  A buffer holding text/string.

Return:
	>0	Ok, bytes written to the file.
	<=0	Fails
---------------------------------------------------------*/
int juhe_save_charBuff(const char *fpath, const char *buff)
{
	FILE *fil;
	int ret=0;

	if(buff==NULL || fpath==NULL)
		return -1;

        /* open file for write */
        fil=fopen(fpath,"wbe");
        if(fil==NULL) {
                printf("%s: Fail to open %s for write.\n",__func__, fpath);
                return -1;
        }

	/* write to file */
	ret=fwrite(buff, strlen(buff), 1, fil);

	fclose(fil);
	return ret;
}


/*-------------------------------------------------------------
Read and put a (html)text file content to a buffer.

@fpath:	   	A file holding text/string.
@buff:   	A buffer to hold text.
@size:		Size of the buffer, in bytes.
		Or size of data expected to be read in.

Return:
	>0	Bytes read from file and put to buff.
	=<0  	Fails
---------------------------------------------------------------*/
int juhe_fill_charBuff(const char *fpath, char *buff, int size)
{
	FILE *fil;
	int ret=0;

	if(buff==NULL || fpath==NULL )
		return -1;

        /* open file for write */
        fil=fopen(fpath,"rbe");
        if(fil==NULL) {
                printf("%s: Fail to open %s for read.\n",__func__, fpath);
                return -2;
        }

	/* read in to buff */
 	ret=fread(buff, size, 1, fil);

	/* Note: fread() does not distinguish between end-of-file and error, and callers must
	   use feof(3) and ferror(3) to determine which occurred.
	*/
	if( feof(fil) ) {
		//"If an error occurs, or the end of the file is reached, the return value is a short item count (or zero)."
		ret=ftell(fil); /* return whole length of file */
		printf("%s: End of file reached! read %ld bytes.\n",__func__, ftell(fil));
	}
	else {
		if( ferror(fil) )
			printf("%s: Fail to read %s \n", __func__, fpath);
		else
			printf("%s: Read in file incompletely!, try to increase buffer size.\n", __func__);

		ret =-3;
	}

	fclose(fil);
	return ret;
}




/* ---------------------------	 ERROR CALL 	-------------------------

1. --- Fail to extract title for "新的LSAM MT机型为增材制造提供新的选择"

...,{"uniquekey":"9999816945df6cbff47dcd9d4404dfbd","title":"金牛座有缘无分的恋人是谁？","date":"2019-12-06 10:20","category":"头条","author_name":"星座互侃","url":"http:\/\/mini.eastday.com\/mobile\/191206102033097.html","thumbnail_pic_s":"http:\/\/02imgmini.eastday.com\/mobile\/20191206\/20191206102033_a1680a0a1441d1dda466c8df92512b1b_1_mwpm_03200403.jpg"},{"uniquekey":"6c570b0293cef7db2c5bd139e8f401fa","title":"新的LSAM MT机型为增材制造提供新的选择","date":"2019-12-06 10:17","category":"头条","author_name":"ZAKER网","url":"http:\/\/mini.eastday.com\/mobile\/191206101708250.html","thumbnai
l_pic_s":"http:\/\/09imgmini.eastday.com\/mobile\/20191206\/20191206101708_9aea787b5f4b6980ef48056072a1f61e_3_mwpm_03200403.jpg","thumbnail_pic_s02":"http:\/\/09imgmini.eastday.com\/mobile\/20191206\/20191206101708_9aea787b5f4b6980ef48056072a1f61e_2_mwpm_03200403.jpg","thumbnail_pic_s03":"http:\/\/09imgmini.eastday.com\/mobile\/20191206\/20191206101708_9aea787b5f4b6980ef48056072a1f61e_4_mwpm_03200403.jpg"},{"uniquekey":"ea5de56c39c2a0acfaca84e5fed12c1c","title":"赣县中学西校区组织开展劳动实践活动","date":"2019-12-06 10:10","category":"头条","author_name":"客家新闻网","url":"http:\/\/mini.eastday.com\/mobile\/191206101008804.html","thumbnail_pic_s":"http:\/\/00imgmini.eastday.com\/mobile\/20191206\/20191206101008_2664a915115a22fc4acc88e917e373ac_1_mwpm_03200403.jpg"},....



2. ---

New item[13] URL: http://mini.eastday.com/mobile/191204113356706.html
[2019-12-04 15:57:35] [LOGLV_INFO] main: Start juhe_get_newsContent()...
juhe_get_newsFilo: init FILO...
juhe_get_newsFilo: https_curl_request...
[2019-12-04 15:57:35] [LOGLV_INFO] https_curl_request: start curl_global_init and easy init...
[2019-12-04 15:57:36] [LOGLV_INFO] https_curl_request: start curl_easy_perform()...
https_curl_request: curl_easy_perform() failed: No error  <-------- Ok HERE ----------
https_curl_request: CURLINFO_CONTENT_LENGTH_DOWNLOAD = -1  !!! --- BUT --- !!!!
Fail to call https_curl_request()!


<!DOCTYPE html>
<html lang="zh-cmn-Hans-CN">
<head>
    <meta charset="utf-8" />
    <title>新闻头条_东方资讯客户端</title>
    <!--target-densitydpi=medium-dpi,-->
    <meta id="viewport" name="viewport" content=" width=640, height=device-height, initial-scale=0.5, user-scalable=yes" />
    <link href="/assets/images/favicon.ico" type="image/x-icon" rel="icon" />

    <link type="text/css" rel="stylesheet" href="/assets/error_404/css/error_404.css?20190225" />
    <script type="text/javascript" src="/assets/js/jquery.merge.min.js"></script>
    <script type="text/javascript" src="/assets/js/resources/minicookie.js"></script>
    <script type="text/javascript">
        var newstype = '404';
    </script>
    <script type="text/javascript" src="/assets/error_404/js/global_404.js?2019005071"></script>
</head>

<body>
    <div id="J_404" style="display: none">
        <div class='header'>
            <div class="logo-wrapper">
                <div class="logo">
                    <a class="logo-img" href="/" target="_blank"></a>
                </div>
            </div>
            <div class="error-tip">
                <p class="error_info1">404&nbsp;&nbsp;很抱歉！您访问页面被外星人劫持了</p>
                <p class="error_info2">可能原因：你要查看的网址可能被删，名称已被更改，或者暂时不可用</p>
            </div>
        </div>
        <div class="news-list-wrapper">
            <div class="word-wrapper">


---------------------------------------------------------------------------- */

