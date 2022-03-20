/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

To extract ID3V2 tag content from an MP3 file and display on screen
as a poster.

Usage: ./test_mp3tag  /media/music/*.mp3


Refrecne:
  1. https://www.cnblogs.com/ranson7zop/p/7655474.html
  2. http://www.multiweb.cz/twoinches/MP3inside.htm
  3. https://id3.org

Note:
1. MP3 file structure:
  [TAG ID3V2] Frame1 Frame2 Frame3 ... [TAG ID3V1]
  FrameX: Audio Frames, HOWEVER, for a VBR mp3 file, Frame1 is NOT an audio frame, see 1.4
  <<< Audio_Frame = Frame_Header + Compressed_Audio_Data  >>>

  1.1 Each Audio Frame contains compressed audio data:
      每帧持续时间(毫秒) = 每帧采样数 / 采样频率 * 1000
      (For MPEG1 Layer III with sampling rate 44.1KHz, it's appr. 26ms.)

  1.2 Size of each Audio Frame MAY vary acoording to its bitrate.
      LyaerI 使用公式:
	帧长度(字节) = (( 每帧采样数 / 8 * 比特率 ) / 采样频率 ) + 填充 * 4
      LyerII 和 LyaerIII 使用公式:
	帧长度(字节)= (( 每帧采样数 / 8 * 比特率 ) / 采样频率 ) + 填充

  1.3 The first 4 Bytes of each Audio Frame is the Frame Header.
      Structure of a Audio Frame Header(4Bytes, 32bits) for a CBR mp3 file:
	AAAAAAAA  AAABBCCD  EEEEFFGH IIJJKLMM
	A  --- Frame synchronizer(all bits set to 1<---------)
	B  --- MPEG version ID (00 MPEG V2.5, 01 reserved, 10 MPEG v2, 11 MPEG v1)
	C  --- Layer (00 reserved, 01 LayerIII, 10 LayerII, 11 LayerI)
	D  --- CRC   ( 0 protected by CRC, 1 Not protected)

	E  --- Bitrate index (data in kbps)
		0000:free   0001:32   0010:40   0011:48
		0100:56     0101:64   0110:80   0111:96
		1000:112    1001:128  1010:160  1011:192
		1100:224    1101:256  1110:320  1111:bad
  	  ( !!! Each Frame MAY have differenct bitrate. !!! )

	F  --- Sampling rate index (data in Hz)
		00:44100   01:48000   10:32000   11:reserved
	G  --- Padding
		0: Frame is NOT padded   1: Frame is padded
		!!! To make exact length of data according to the bitrate !!!
	H  --- Private bit
	I  --- Channle
		00:Stereo
		01:Joint Stereo
		10:Dual(two separated channels)
		11:Mono
	J  --- Mode extension(For Joint Stereo)
	K  --- Copyright
		0: NOT copyrighted
		1: Copyrighted
	L  --- Original
		0: Copy of original media
		1: Original media
	M  --- Emphasis
		00:None       01:50/15
		10:reserved   11:CCIT J.17

  1.4 Structure of the First Frame for a VBR mp3 file:
	Bytes  |   Content
	0-3:       Frame Header, same as: AAAAAAAA  AAABBCCD  EEEEFFGH IIJJKLMM  <---See 1.3

	4-x:       Not used till string "Xing" (58 69 6E 67).
	36-39:     "Xing" for MPEG1 and CHANNEL != mono (mostly used)
	21-24:     "Xing" for MPEG1 and CHANNEL == mono
	21-24:     "Xing" for MPEG2 and CHANNEL != mono
	13-16:     "Xing" for MPEG2 and CHANNEL == mono

	--- Data following to the end of "Xing" ---
	( Following schema is for case MPEG1 and CHANNEL != mono: )
	40-43:     Flags.
	44-47:     Number of frames in file. (Big_Endian)
	48-51:     File length in Bytes. (Big_Endian)
	52-151:    Indexes for lookup in file.
	152-155:   VBR scale. (Big_Endian)


  			           ===== Tags for an MP3 File =====
   1.5 Tag ID3V1 structure:
	Bytes  | Length  | Content
	0-2	   3       Tag identifier "TAG"
	3-32       30      Song name
        33-62      30      Artist
        63-92      30      Album
        93-96      4       Year
        97-126     30      Comment
        127        1       Genre

   1.6 Tag ID3V2 structrue:
       Tag_Header +Tag_Frame(TagFrameHeader+TagFrameBody) +Tag_Frame(TFH+TFB) +Tag_Frame(TFH+TFB) ...

   1.6.1 Struct of [Tag_Header]:
	Bytes  |   Content
	0-2	   TAG identifier "ID3"
	4-7        TAG version, eg 03 00
	5          Flags
        6-9	   Size of TAG (It doesnt contain Tag_Header itself)
		   The most significant bit in each Byte is set to 0 and ignored,
		   Only remaining 7 bits are used, as to avoid mismatch with audio frame header
                   which has the first synchro Byte 'FF'.

   1.6.2 Struct of [Tag_Frame_Header]:
	Bytes  |   Content
	0-3        Frame identifier
		   TRCK:  Track number
		   TIME:  time
		   WXXX:  URL
	           TCON:  Genre
		   TYER:  Year
	     	   TALB:  Album
		   TPE1:  Lead performer(s)/Soloist(s)
		   TIT2:  Title/songname/content description
		   APIC:  Attached picture

		   GEOB:  General encapsulated object
		   COMM:  Commnets
		   ... (more)
	4-7        Size (It doesnt include Tag_Frame_Header itself)
	8-9	   Flags

   1.6.3 Struct of [Tag_Frame_Body]:
	Depends on Frame identifer, see ID3V2 standard:
	Example for 'APCI':
		Text encoding(2Bytes)   $xx  (see Note)
		MIME type               <text string> $00

		Picture type(2Bytes)    $xx
					03 Cover(front)
					04 Cover(back)
					08 Artist/performer
					0C Lyricist/text writer
					...
		Description     	<text string according to encoding> $00 (00)
		Picture data    	<binary data>
					JPEG: starts with '0xFF 0xD8'
					PNG:  starts with '0x89 0x50'

     Note: (See "ID3v2 frame overview"  --- Document id3v2.3 )
     "All numeric strings and URLs are always encoded as ISO-8859-1. Terminated strings are terminated with $00 if encoded with ISO-8859-1,
     and $00 00 if encoded as unicode If nothing else is said newline character is forbidden. In ISO-8859-1 a new line is represented,
     when allowed, with $0A only. Frames that allow different types of text encoding have a text encoding description byte directly after
     the frame size. If ISO-8859-1 is used this byte should be $00, if Unicode is used it should be $01.
     (Note: others are not mentioned in this standard:  $02 -- UTF-16BE(without BOM), $03 -- UTF8 )
     Strings dependent on encoding is represented as <text string according to encoding>, or <full text string according to encoding> if
     newlines are allowed. Any empty Unicode strings which are NULL-terminated may have the Unicode BOM followed by a Unicode NULL
     ($FF FE 00 00 or $FE FF 00 00)."


Journal:
2022-02-12: Create this file.
2022-02-13: Process text information tag fram content.
2022-02-15: Extract tag 'COMM' as comment.
2022-02-16: Extract tag 'APIC' as attached picture.
2022-02-17:
	    1. Create EGI_ID3V2TAG and functions: egi_id3v2tag_calloc(), egi_id3v2tag_free().
	    2. Fill in members of EGI_ID3V2TAG.
	    3. Display members of EGI_ID3V2TAG.
2022-02-18:
	    1. Create egi_id3v2tag_readfile().
	    2. Move ID3V2TAG codes to module egi_utils.


Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
https://github.com/widora/wegi
------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "egi_debug.h"
#include "egi_cstring.h"
#include "egi_utils.h"
#include "egi_image.h"
#include "egi_fbdev.h"
#include "egi_color.h"
#include "egi_FTsymbol.h"


/*----------------------------
           MAIN
----------------------------*/
int main(int argc, char **argv)
{
	int ret=0;
	int fcnt;

	/* For poster text, multiline text. */
	char strMText[1024*4]; /* 4 textinfo tags in EGI_ID3V2TAG */
	EGI_ID3V2TAG *mp3tag=NULL;

        /* Initilize sys FBDEV */
        printf("init_fbdev()...\n");
        if(init_fbdev(&gv_fb_dev))
                return -1;

        /* Set sys FB mode */
        fb_set_directFB(&gv_fb_dev,false);
        fb_position_rotate(&gv_fb_dev,0);
        //gv_fb_dev.pixcolor_on=true;             /* Pixcolor ON */
        //gv_fb_dev.zbuff_on = true;              /* Zbuff ON */
        //fb_init_zbuff(&gv_fb_dev, INT_MIN);

	/* Load FT fonts */
        if(FTsymbol_load_appfonts() !=0 ) {
              printf("Fail to load FT appfonts, quit.\n");
              return -2;
        }
        //FTsymbol_disable_SplitWord();


while(1) { /////////////////////////////   LOOP TEST  //////////////////////////

  for(fcnt=1; fcnt < argc; fcnt ++) {
	printf(" <<< MP3 file: %s >>>\n", argv[fcnt]);

	/* Read file to get ID3V2 Tag */
	mp3tag=egi_id3v2tag_readfile(argv[fcnt]);
	if(mp3tag==NULL)
		continue;

	/* Make a poster and show it. */
	if(mp3tag->imgbuf) {
		/* Display TAG content */
		int sw=320, sh=240;

		/* Prepare poster image */
		egi_imgbuf_get_fitsize(mp3tag->imgbuf, &sw, &sh);
		egi_imgbuf_resize_update(&mp3tag->imgbuf,true, sw, sh); /* **pimg, keep_ratio, w, h */

		fb_clear_workBuff(&gv_fb_dev, WEGI_COLOR_GRAY3);
		egi_subimg_writeFB(mp3tag->imgbuf, &gv_fb_dev, 0, -1, (320-sw)/2,(240-sh)/2);   /* imgbuf, fbdev, subnum, subcolor, x0, y0 */

		/* Render */
		fb_render(&gv_fb_dev);
		//sleep(1);
		usleep(10000);

		/* Prepare poster text */
		strMText[0]=0;
		strcat(strMText, "Title: ");
		strcat(strMText, mp3tag->title?mp3tag->title:" "); strcat(strMText, "\n");
		strcat(strMText, "Album: ");
		strcat(strMText, mp3tag->album?mp3tag->album:" "); strcat(strMText, "\n");
		strcat(strMText, "Artist: ");
		strcat(strMText, mp3tag->performer?mp3tag->performer:" "); strcat(strMText, "\n");
		strcat(strMText, "Comment: ");
		strcat(strMText, mp3tag->comment?mp3tag->comment:" "); strcat(strMText, "\n");

		printf("%s\n", strMText);
	        draw_blend_filled_rect( &gv_fb_dev, 0,0, 320-1, 240-1, WEGI_COLOR_GRAY3, 200); /* fbdev, x1,y1,x2,y2, color,alpha */
        	FTsymbol_uft8strings_writeFB( &gv_fb_dev, egi_appfonts.bold,  /* fbdev, face */
                	                      15, 15, (UFT8_PCHAR)strMText,   /* fw,fh pstr */
                        	              320-10, 240/15+1, 4, 5, 5,       /* pixpl, lines, gap, x0,y0 */
                                	      WEGI_COLOR_WHITE, -1, 240,      /* fontcolor, transpcolor, opaque */
                                      	NULL, NULL, NULL, NULL );       /* *cnt, *lnleft, *penx, *peny */

		/* Render */
		fb_render(&gv_fb_dev);
		//sleep(1);
   	}

	/* Free mp3tag */
	egi_id3v2tag_free(&mp3tag);

  } /* End for() */

} /////////////////////////////   END LOOP TEST  //////////////////////////

	return ret;
}

