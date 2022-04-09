/*-----------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Original source: https://blog.csdn.net/luxiaoxun/article/details/7622988


Midas Zhou
-----------------------------------------------------------------------*/
#ifndef __EGI_BJP_H__
#define __EGI_BJP_H__

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <jpeglib.h>
#include <jerror.h>
//#include "egi.h"
#include "egi_image.h"
#include "egi_fbgeom.h"

#ifdef __cplusplus
 extern "C" {
#endif

/* 14byte文件头 */
typedef struct
{
	char cfType[2];//文件类型，"BM"(0x4D42)
	long cfSize;//文件大小（字节）
	long cfReserved;//保留，值为0
	long cfoffBits;//数据区相对于文件头的偏移量（字节）
}__attribute__((packed)) BITMAPFILEHEADER;
//__attribute__((packed))的作用是告诉编译器取消结构在编译过程中的优化对齐

/* 40byte信息头 */
typedef struct
{
	char ciSize[4];//BITMAPFILEHEADER所占的字节数
	long ciWidth;//宽度
	long ciHeight;//高度
	char ciPlanes[2];//目标设备的位平面数，值为1
	uint16_t ciBitCount;//每个像素的位数
	char ciCompress[4];//压缩说明
	char ciSizeImage[4];//用字节表示的图像大小，该数据必须是4的倍数
	char ciXPelsPerMeter[4];//目标设备的水平像素数/米
	char ciYPelsPerMeter[4];//目标设备的垂直像素数/米
	char ciClrUsed[4]; //位图使用调色板的颜色数
	char ciClrImportant[4]; //指定重要的颜色数，当该域的值等于颜色数时（或者等于0时），表示所有颜色都一样重要
}__attribute__((packed)) BITMAPINFOHEADER;

/* in order of BMP file BGR ??? */
typedef struct
{
	unsigned char blue;
	unsigned char green;
	unsigned char red;
//	unsigned char reserved; // for 32bit pix only
}__attribute__((packed)) PIXEL;//颜色模式RGB


/*** struct EGI_PICINFO
 */
typedef struct egi_picture_info
{
	/* Extract from JPEG segement SOF0, Marker 0xC0 */
	int width; /* Size in pixels */
	int height;

	int cpp;   /* color components per pixel */
	int bpc;   /* Bits per color component */
	unsigned char colorType;   /* NOW for PNG only, TODO JPEG */
	/*  Same definition as PNG standard
	   Greyscale               0
	   Truecolour(RGB)         2
           Indexed-colour          3
           Greyscale + alpha       4
           Truecolour + alpha      6
	 */


	bool progressive;  /* JPEG: Progressive,  PNG: Interlaced */

	/* Extract from JPEG segemnt, Marker */
	char *comment;	   /* Marker 0xFE, segment COM */
	char *app14com;	   /* Marker 0xEC, APP14 0xEC Picture info(older digicams), Photoshop save for web:Ducky  */

	/* Extract from Exif IFD0 by TAG */
	char *CameraMaker; /* TAG 0x010f */
	char *CameraModel; /* TAG 0x0110 */
	char *software;    /* TAG 0x0131 software */
	char *ImageDescription; /* TAG 0x010e */
	char *DateTaken;   /* TAG 0x9003 Date when the picture is taken. */
	char *DateLastModify;    /* 0x0132 time for last modification */
        char *title;      /* TAG 0x9c9b, ignored if ImageDescription exists */
        char *xcomment;   /* TAG 0x9c9c, Windows comment */
        char *author;	  /* TAG 0x9c9d */
	char *keywords;   /* TAG 0x9c9e */
	char *subject;	  /* TAG 0x9c9f */
	char *copyright;  /* TAG 0x8298  Copyright information */

	/* Thumbnail */
	EGI_IMGBUF *thumbnail;

} EGI_PICINFO;

EGI_PICINFO *egi_picinfo_calloc(void);
void egi_picinfo_free(EGI_PICINFO **info);
int egi_picinfo_print(const EGI_PICINFO *PicInfo);
//EGI_PICINFO* egi_parse_jpegExif(const char *fpath);
EGI_PICINFO* egi_parse_jpegFile(const char *fpath);
EGI_PICINFO* egi_parse_pngFile(const char *fpath);


#define SHOW_BLACK_TRANSP	1
#define SHOW_BLACK_NOTRANSP	0

/* Check image file integraty */
#define JPEGFILE_INVALID   -1
#define JPEGFILE_COMPLETE   0
#define JPEGFILE_INCOMPLETE 1
int egi_check_jpgfile(const char* path);
int egi_simpleCheck_jpgfile(const char* fpath);

#define PNGFILE_INVALID   -1
#define PNGFILE_COMPLETE   0
#define PNGFILE_INCOMPLETE 1
int egi_check_pngfile(const char *fpath);

/*  functions */
int compress_to_jpgFile(const char * filename, int quality, int width, int height,
			unsigned char *rgb24, J_COLOR_SPACE inspace);
int compress_to_jpgBuffer(unsigned char ** outbuffer, unsigned long * outsize,int quality,
					int width, int height, unsigned char *indata, J_COLOR_SPACE inspace);
unsigned char* 	open_jpgImg(const char *filename, int *w, int *h, int *components);
unsigned char* 	open_buffer_jpgImg( const unsigned char *inbuff, unsigned long insize, int *w, int *h, int *components );
void 		close_jpgImg(unsigned char *imgbuf);

int show_bmp(const char* fpath,FBDEV *fb_dev, int blackoff, int x0, int y0);
int show_jpg( const char* fpath, const unsigned char *inbuff, unsigned long insize,
              FBDEV *fb_dev, int blackoff, int x0, int y0);

int egi_imgbuf_loadjpg(const char* fpath, EGI_IMGBUF *egi_imgbuf);
int egi_imgbuf_loadpng(const char* fpath, EGI_IMGBUF *egi_imgbuf);
int egi_imgbuf_savejpg(const char* fpath,  EGI_IMGBUF *eimg, int quality);
int egi_imgbuf_savepng(const char* fpath, EGI_IMGBUF *egi_imgbuf);

/* roaming picture in a window */
int egi_roampic_inwin(const char *path, FBDEV *fb_dev, int step, int ntrip,
                              			int xw, int yw, int winw, int winh);

/* find all jpg files in a path --- OBSOLEGTE --- */
int egi_find_jpgfiles(const char* path, int *count, char **fpaths, int maxfnum, int maxflen);

/* save FB data to a 24bit color BMP file */
int egi_save_FBbmp(FBDEV *fb_dev, const char *fpath);

/* save FB data to a PNG file */
int egi_save_FBpng(FBDEV *fb_dev, const char *fpath);

/* save FB data to a JPG file */
int egi_save_FBjpg(FBDEV *fb_dev, const char *fpath, int quality);

#ifdef __cplusplus
 }
#endif

#endif
