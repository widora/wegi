#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "egi_https.h"
#include "egi_utils.h"
#include "egi_cstring.h"

static char buff[32*1024];      /* for curl returned data */
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp);

const char *strhtml="<p> dfsdf sdfig df </p>";
//const char *strRequest="http://mini.eastday.com/mobile/191123112758979.html";
const char *strRequest="http://mini.eastday.com/mobile/191123190527243.html";

int main(void)
{
	char *content=NULL;
	int len;
	char *pstr=NULL;

	printf("%s\n", strhtml);
	pstr=cstr_parse_html_tag(strhtml, "p", &content, &len);
	printf("pstr: %s\n",pstr);
	printf("Get %d bytes content: %s\n", len, content);
	egi_free_char(&content);

	/* clear buff */
	memset(buff,0,sizeof(buff));
        /* Https GET request */
        if(https_curl_request(strRequest, buff, NULL, curlget_callback)!=0) {
                 printf("Fail to call https_curl_request()!\n");
                 exit(-1);
         }
	printf("%s\n", buff);

	/* Parse HTML */
	pstr=buff;
	do {
		/* parse returned data as html */
		pstr=cstr_parse_html_tag(pstr, "p", &content, &len);
		if(content!=NULL)
			printf("%s\n",content);
		egi_free_char(&content);
		//printf("Get %d bytes content: %s\n", len, content);
	} while( pstr!=NULL );

	egi_free_char(&content);
	return 0;
}

/*-----------------------------------------------
 A callback function to deal with replied data.
------------------------------------------------*/
static size_t curlget_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
        strcat(userp, ptr);
        return size*nmemb;
}


