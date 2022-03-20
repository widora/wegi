/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

A lookup table for converting GB2312 codes to Unicode.

Journal:
2021-11-24: Create the EGI_GB2UNICODE_LIST[] lookup table.


Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
-------------------------------------------------------------------*/
#ifndef __EGI_GB2UNICODE_H__
#define __EGI_GB2UNICODE_H__

typedef struct GB2312toUnicode {
                uint16_t  gbcode;   /* GB2312 code */
                uint16_t  unicode;  /* Unicode */
} EGI_GB2UNICODE;
extern const EGI_GB2UNICODE EGI_GB2UNICODE_LIST[];

int   UniHan_gb2unicode_listSize(void);
int   cstr_gb2312_to_unicode(const char *src,  wchar_t *dest); /* OBSOLETE */
int   cstr_gbk_to_unicode(const char *src,  wchar_t *dest);

#endif
