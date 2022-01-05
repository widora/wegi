/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/
#ifndef __EGI_CSTRING_H__
#define __EGI_CSTRING_H__
#include <wchar.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
 extern "C" {
#endif

#ifdef LETS_NOTE
 #define EGI_CONFIG_PATH "/home/midas-zhou/egi/egi.conf"
#else
 #define EGI_CONFIG_PATH "/home/egi.conf"
#endif

/* ----- M3U8 Functions ------ */

/* M3u8 segment encryption method/type */
enum {
	M3U8ENCRYPT_NONE 		=0,
	M3U8ENCRYPT_AES_128 		=1,
	M3U8ENCRYPT_SAMPLE_AES 		=2,
};

/* List of m3u8 */
typedef struct egi_m3u8_list {
	int   ns;		/* Total number of list items */

	bool  isMasterList;	/* TRUE: It's a Master Playlist, it defines/lists Media Playlists.
				 * FALSE: It's a Media Playlist, it defines/lists Media Segments.
				 */

	/* For Media Playlist */
	float maxDuration;	/* EXT-X-TARGETDURATION: Max duration for any segement in the list, in seconds. */
	int   seqnum;		/* Sequence number for the first segment in the playlist, and the following
				 * segment has sequence number of seqnum+1, and next is seqnum+2, and so on...
				 * If NO #EXT-X-MEDIA-SEQUENCE tag found in the playlist, then it MUST be 0,
			 	 * which implys restart of ALL sequence numbering.
				 */

  	/* For Media Segment attributes */
	float *tsec;		/* Duration for each segment, in seconds. */
	int   *encrypt;		/* Method/type of encryption for each segement, default as NONE */
	char  **msURI;		/* [ns] List of URI for media segments
				 * 1. If it's the master playlist, then it's the list of Variant Streams: xxx.m3u8
				 * 2. It's maybe a name, a relative URL address, OR a full URL address(absolute URL)!
				 */
	char **keyURI;		/* [ns] List of URI for keys, if encrypted. MAYBE null! */
	char **strIV;		/* [ns] List of init vector strings, if so encrypted.  MAYBE null! */
} EGI_M3U8_LIST;
void m3u_free_list(EGI_M3U8_LIST **list);
EGI_M3U8_LIST* m3u_parse_simple_HLS(char *strHLS);


/* --- EGI_TXTGROUP --- */
typedef struct egi_txt_group {
        uint32_t        size;      	/* also as off_size, Total number of txt_groups in txt[]. */
        size_t          offs_capacity; 	/* Capacity of off[], in bytes */
	uint32_t      	*offs;		/* Offset array, for each txt group */
 	#define         TXTGROUP_OFF_GROW_SIZE   64

        uint32_t        buff_size;       /* Total bytes of buff[] used. */
        size_t          buff_capacity;   /* Capacity of buff[], in bytes */
	unsigned char 	*buff;		 /* txt buffer
					  *  String/txt groups are divided by '\0's between each other in buff!
					  */
 	#define         TXTGROUP_BUFF_GROW_SIZE   1024
} EGI_TXTGROUP;

unsigned char* egi_str2hex(char *str, size_t size);
char* egi_hex2str(unsigned char *hex, size_t len);

EGI_TXTGROUP *cstr_txtgroup_create(size_t offs_capacity, size_t buff_capacity);
void  	cstr_txtgroup_free(EGI_TXTGROUP **txtgroup);
int 	cstr_txtgroup_push(EGI_TXTGROUP *txtgroup, const char *txt);


/* --- util functions --- */
char*	cstr_replace_string(char **src, const char *obj, const char *sub);

bool    cstr_is_absoluteURL();
int 	cstr_parse_URL(const char *URL, char **protocol, char **hostname, unsigned int *port,
		 	 char **filename, char **path, char **dir, char **dirURL);
//int 	m3u_parse_simple_HLS(char *strHLS, char ***URI, int *ns);
char* 	cstr_decode_htmlSpecailChars(char *strHtml);
int 	cstr_squeeze_string(char *buff, int size, char spot);
int	cstr_clear_unprintChars(char *buff, int size);
int	cstr_clear_controlChars(char *buff, int size);
char* 	cstr_dup_repextname(char *fpath, char *new_extname);
char* 	cstr_split_nstr(const char *str, const char *split, unsigned n);
char* 	cstr_trim_space(char *buf);
int 	cstr_copy_line(const char *src, char *dest, size_t size);
int 	cstr_charlen_uft8(const unsigned char *cp);
int 	cstr_prevcharlen_uft8(const unsigned char *cp);
int 	cstr_strlen_uft8(const unsigned char *cp);
int 	cstr_strcount_uft8(const unsigned char *pstr);
char* 	cstr_get_peword(const unsigned char *cp);
int 	cstr_strlen_eword(const unsigned char *cp);

int 	char_uft8_to_unicode(const unsigned char *src, wchar_t *dest);
int 	char_unicode_to_uft8(const wchar_t *src, char *dest);
wchar_t char_unicode_DBC2SBC(wchar_t dbc);
int 	char_DBC2SBC_to_uft8(char dbc, char *dest);
int 	cstr_unicode_to_uft8(const wchar_t *src, char *dest);
int 	cstr_uft8_to_unicode(const unsigned char *src, wchar_t *dest);


int 	egi_count_file_lines(const char *fpath);

#define EGI_CONFIG_LMAX         256     /* Max. length of one line in a config file */
#define EGI_CONFIG_VMAX         64      /* Max. length of a SECTION/KEY/VALUE string  */
int 	egi_get_config_value(const char *sect, const char *key, char* value);
int 	egi_update_config_value(const char *sect, const char *key, const char* pvalue);

char* 	cstr_parse_html_tag(const char* str_html, const char *tag, char *attributes, int attrsize, char **content, int *length);
int 	cstr_get_html_attribute(const char *str, const char *name, char *value);
int 	cstr_extract_html_text(const char *str_html, char *text, size_t txtsize);
int 	cstr_parse_simple_html(const char *str_html, char *text, size_t txtsize);

int 	cstr_extract_ChnUft8TimeStr(const char *src, char *buff, int bufflen);
int 	cstr_getSecFrom_ChnUft8TimeStr(const char *src,time_t *tp);

long long int cstr_hash_string(const unsigned char *pch, int mod);

#ifdef __cplusplus
 }
#endif

#endif
