/* -----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Char and String Functions

Midas Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <stdint.h>
#include <ctype.h>
#include "egi_cstring.h"
#include "egi_log.h"


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
 	!!! DO NOT use strlen(buff) to get the size, if the
	spot is '\0'.

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



/*--------------------------------------------------------
Get pointer to the first char after Nth split char.

str:	source string.
split:  split char (or string).
n:	the n_th split
	if n=0, then return split=str;

Example str:"data,12.23,94,343.43"
	split=",", n=2 (from 0...)
	then get pointer to '9',

Return:
	pointer	to a char	OK, spaces trimed.
        NULL			Fail, or not found.
---------------------------------------------------------*/
char * cstr_split_nstr(char *str, char *split, unsigned n)
{
	int i;
	char *pt;

	if(str==NULL || split==NULL)
		return NULL;

	if(n==0)return str;

	pt=str;
	for(i=1;i<=n;i++) {
		pt=strstr(pt,split);
		if(pt==NULL)return NULL;
		pt=pt+1;
	}

	return pt;
}


/*--------------------------------------------------
Trim all spaces at end of a string, return a pointer
to the first non_space char.

Return:
	pointer	to a char	OK, spaces trimed.
        NULL			Input buf invalid
--------------------------------------------------*/
char * cstr_trim_space(char *buf)
{
	char *ps, *pe;

	if(buf==NULL)return NULL;

	/* skip front spaces */
	ps=buf;
	for(ps=buf; *ps==' '; ps++)
	{ };

	/* eat up back spaces/returns, replace with 0 */
	for(  pe=buf+strlen(buf)-1;
	      *pe==' '|| *pe=='\n' || *pe=='\r' ;
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



/*-----------------------------------------------------------------------
Get length of a character with UTF-8 encoding.

@cp:	A pointer to a character with UTF-8 encoding.

Note:
1. UTF-8 maps to UNICODE according to following relations:
				(No endianess problem for UTF-8 conding)

	   --- UNICODE ---	      --- UTF-8 CODING ---
	U+  0000 - U+  007F:	0XXXXXXX
	U+  0080 - U+  07FF:	110XXXXX 10XXXXXX
	U+  0800 - U+  FFFF:	1110XXXX 10XXXXXX 10XXXXXX
	U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX

2. If illegal coding is found...

Return:
	>0	OK, string length in bytes.
	<0	Fails
------------------------------------------------------------------------*/
inline int cstr_charlen_uft8(const unsigned char *cp)
{
	if(cp==NULL)
		return -1;

	if(*cp < 0b10000000)
		return 1;	/* 1 byte */
	else if( *cp >= 0b11110000 )
		return 4;	/* 4 bytes */
	else if( *cp >= 0b11100000 )
		return 3;	/* 3 bytes */
	else if( *cp >= 0b11000000 )
		return 2;	/* 2 bytes */
	else
		return -2;	/* unrecognizable starting byte */
}


/*--------------------------------------------------
Get length of a character with UTF-8 encoding.

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
1. Count all ASCII characters, both printables and unprintables.

2. UTF-8 maps to UNICODE according to following relations:
				(No endianess problem for UTF-8 conding)

	   --- UNICODE ---	      --- UTF-8 CODING ---
	U+  0000 - U+  007F:	0XXXXXXX
	U+  0080 - U+  07FF:	110XXXXX 10XXXXXX
	U+  0800 - U+  FFFF:	1110XXXX 10XXXXXX 10XXXXXX
	U+ 10000 - U+ 1FFFF:	11110XXX 10XXXXXX 10XXXXXX 10XXXXXX


3. If illegal coding is found...

Return:
	>=0	OK, total numbers of character in the string.
	<0	Fails
------------------------------------------------------------------------*/
int cstr_strcount_uft8(const unsigned char *pstr)
{
	unsigned char *cp;
	int size;	/* in bytes, size of the character with UFT-8 encoding*/
	int count;

	if(pstr==NULL)
		return -1;

	cp=(unsigned char *)pstr;
	count=0;
	while(*cp) {
		size=cstr_charlen_uft8(cp);
		if(size>0) {
			//printf("%d\n",*cp);
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


2. If illegal coding is found...

Return:
	>0 	OK, bytes of src consumed and converted into unicode.
	=0	Fails, or unrecognizable uft-8 .
-----------------------------------------------------------------------*/
inline int char_uft8_to_unicode(const unsigned char *src, wchar_t *dest)
{
//	unsigned char *cp; /* tmp to dest */
	unsigned char *sp; /* tmp to src  */

	int size=0;	/* in bytes, size of the character with UFT-8 encoding*/

	if(src==NULL || dest==NULL )
		return 0;

//	cp=(unsigned char *)dest;
	sp=(unsigned char *)src;

	size=cstr_charlen_uft8(src);

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

		default: /* if size<=0 or size>5 */
			printf("%s: Unrecognizable uft-8 character! \n",__func__);
			break;
	}

	return size;
}


/*-------------------------------------------------------------------------
Convert a character from UFT-8 to UNICODE.

@src:	A pointer to character in UNICODE.
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

2. If illegal coding is found...

Return:
	>0 	OK, bytes of dest in UFT-8 encoding.
	<=0	Fail, or unrecognizable unicode
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



/*---------------------------------------------------------------------
Convert a string in UNICODE to UFT-8 by calling char_unicode_to_uft8()

@src:	Input string in UNICODE
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
		return 0;

	while( *ps !=L'\0' ) {  /* wchar t end token */
		ret = char_unicode_to_uft8(ps, dest+size);
		if(ret<=0)
			return size;

		size += ret;
		ps++;
	}

	return size;
}


/*----------------------------------------------------------------------------------
Search given SECTION and KEY string in the config file, copy VALUE
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

		[[ ------  LIMITS -----  ]]
6. Max. length for one line in a config file is 256-1. ( see. line_buff[256] )
7. Max. length of a SECTION/KEY/VALUE string is 64-1. ( see. str_test[64] )

TODO:	If key value includes BLANKS, use "".--- OK, sustained.


Return:
	3	VALE string is NULL
	2	Fail to find KEY string
	1	Fail to find SECTION string
	0	OK
	<0	Fails
------------------------------------------------------------------------------------*/
int egi_get_config_value(char *sect, char *key, char* pvalue)
{
	int lnum=0;
	int ret=0;

	FILE *fil;
	char line_buff[256];
	char *ps=NULL, *pe=NULL; /* start/end char pointer for [ and  ] */
	char *pt=NULL;
	char str_test[64];
	int  found_sect=0; /* 0 - section not found, !0 - found */

	/* check input param */
	if(sect==NULL || key==NULL || pvalue==NULL) {
		printf("%s: One or more input param is NULL.\n",__func__);
		return -1;
	}

	/* open config file */
	fil=fopen( EGI_CONFIG_PATH, "re");
	if(fil==NULL) {
		printf("Fail to open config file '%s', %s\n",EGI_CONFIG_PATH, strerror(errno));
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
					printf("Found SECTION:[%s] \n",str_test);
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
				printf("Get bottom of the section, fail to find key '%s'.\n",key);
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
				   printf("Key name is NULL in line_buff: '%s' \n",line_buff);
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



/*---------------------------------------------------------------------------------------
Get pointer to the beginning of HTML element content, which is defined between start tag
and end tag. It returns only the first matched case!


Note:
0. For UFT-8 encoding only!
1. Input tag MUST be closed type, it appeares as <X> ..... </X> in HTML string.
   Void elements, such as <hr />, <br /> are not applicable for the function!
   If input html string contains only single <X> OR </X> tag, a NULL pointer will
   be returned.
2. The parameter *content has independent memory space if it is passed to the caller,
   so it's the caller's responsibility to free it after use!
3. The returned char pointer is a reference pointer to a position in original HTML string,
   so it needs NOT to be freed.
4. Default indentation of two SPACEs is inserted.

5. Limits:
   4.1 Length of tag.
   4.2 Nested conditions such as  <x>...<y>...</y>...</x> are NOT considered!

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
@length:	Pointer to pass length of the element content, in bytes.
		if input is NULL, ignore.
		if fails, pass NULL to the caller then.
@content:	Pointer to pass element content.
		if input is NULL, ignore.
		if fails, pass NULL to the caller then.
		!!! --- Note: the content has independent memory space, and
		so do NOT forget to free it. --- !!!

Return:
	Pointer to taged content in str_html:		Ok
	NULL:						Fails
--------------------------------------------------------------------------------------*/
char* cstr_parse_html_tag(const char* str_html, const char *tag, char **content, int *length)
{
	char stag[16]; 	/* start tag */
	char etag[16]; 	/* end tag   */
	char *pst=NULL;	/* Pointer to the beginning of start tags in str_html
			 * then adjusted to the beginning of content later.
			 */
	char *pet=NULL;  	/* Pointer to the beginning of end tag in str_html */
	char str_indent[32];			/* UFT-8, indentation string */
	/* For LOCALE SPACE INDENTATION */
	const wchar_t wstr_indent[]={12288,12288, L'\0'}; /* sizelimit 32!! UNICODE, indentation string */
	int  len_indent; 	//strlen(str_indent);
	int  len_content=0;	/* length of content, in bytes. NOT include len_indent */
	char *pctent=NULL; /* allocated mem to hold copied content */

	/* Set indentation string in UFT-8 */
	memset(str_indent,0, sizeof(str_indent));
	cstr_unicode_to_uft8(wstr_indent, str_indent);
	len_indent=strlen(str_indent);

	/* Reset content and length to NULL First!!! */
	if(content != NULL)
		*content=NULL;
	if(length != NULL)
		*length=0;

	/* check input data */
	if( strlen(tag) > 16-4 ) {
		return NULL;
	}
	if(str_html==NULL || tag==NULL ) {
		return NULL;
	}

	/* init. start/end tag */
	memset(stag,0, sizeof(stag));
	strcat(stag,"<");
	strcat(stag,tag);
	/* Note: attributes may exists between '<x' and '>',
	   Example: <p class="section txt">
	 */
	//printf("stag: %s\n", stag);

	memset(etag,0, sizeof(etag));
	strcat(etag,"</");
	strcat(etag,tag);
	strcat(etag,">");
	//printf("etag: %s\n", etag);

	/* locate start and end tag in html string, nested elements NOT considered! */
	pst=strstr(str_html, stag);	/* Get start positon of start_tag */
	if(pst != NULL)	 {
		/* TODO: Parse attributes/elements between <x> and </x> */
		pst=strstr(pst,">");		/* Get end position of start_tag */
		if(pst != NULL)
			pet=strstr(pst,etag);   /* Get start position of end_tag */
	}

	/* Only if tag content is valid/available */
	if( pst!=NULL && pet!=NULL ) {
		/* TODO:  parse attributes between '<' and '>' */

		/* get length of content */
		pst += strlen(">");	/* skip '>', move to the beginning of the content */
		len_content=pet-pst;

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

	/* pass to the caller anyway */
	if( content != NULL )
		*content=pctent;
	if( length !=NULL )
		*length=len_content;

	/* Now pst is pointer to the beginning of the content */
	return ( (pst!=NULL && pet!=NULL) ? pst:NULL);
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

