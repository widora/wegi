/*----------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
-----------------------------------------------------------------*/
#ifndef __EGI_CSTRING_H__
#define __EGI_CSTRING_H__
#include <wchar.h>

#ifdef LETS_NOTE
 #define EGI_CONFIG_PATH "/home/midas-zhou/egi/egi.conf"
#else
 #define EGI_CONFIG_PATH "/home/egi.conf"
#endif

int 	cstr_squeeze_string(char *buff, int size, char spot);
int	cstr_clear_unprintChars(char *buff, int size);
int	cstr_clear_controlChars(char *buff, int size);
char* 	cstr_dup_repextname(char *fpath, char *new_extname);
char* 	cstr_split_nstr(char *str, char *split, unsigned n);
char* 	cstr_trim_space(char *buf);
int 	cstr_copy_line(const char *src, char *dest, size_t size);
int 	cstr_charlen_uft8(const unsigned char *cp);
int 	cstr_prevcharlen_uft8(const unsigned char *cp);
int 	cstr_strlen_uft8(const unsigned char *cp);
int 	cstr_strcount_uft8(const unsigned char *pstr);
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

char* 	cstr_parse_html_tag(const char* str_html, const char *tag, char **content, int *length);
int 	cstr_extract_ChnUft8TimeStr(const char *src, char *buff, int bufflen);
int 	cstr_getSecFrom_ChnUft8TimeStr(const char *src,time_t *tp);


#endif
