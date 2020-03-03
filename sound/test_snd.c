#include <stdio.h>
#include <inttypes.h>
#include <sndfile.h>
#include <stdbool.h>
#include "egi_pcm.h"
//#include <ao/ao.h>

/*---------------------------------------------------------------------------
** String types that can be set and read from files. Not all file types
** support this and even the file types which support one, may not support
** all string types.
enum
{       SF_STR_TITLE                                    = 0x01,
        SF_STR_COPYRIGHT                                = 0x02,
        SF_STR_SOFTWARE                                 = 0x03,
        SF_STR_ARTIST                                   = 0x04,
        SF_STR_COMMENT                                  = 0x05,
        SF_STR_DATE                                             = 0x06,
        SF_STR_ALBUM                                    = 0x07,
        SF_STR_LICENSE                                  = 0x08,
        SF_STR_TRACKNUMBER                              = 0x09,
        SF_STR_GENRE                                    = 0x10
} ;

Functions:
	int sf_set_string (SNDFILE *sndfile, int str_type, const char* str) ;
	const char* sf_get_string (SNDFILE *sndfile, int str_type) ;
----------------------------------------------------------------------------*/


#define SF_CONTAINER(x)		((x) & SF_FORMAT_TYPEMASK)
#define SF_CODEC(x)			((x) & SF_FORMAT_SUBMASK)
#define CASE_NAME(x)            case x : return #x ; break ;

const char* str_of_major_format(int format)
{
	switch (SF_CONTAINER (format))
        {       CASE_NAME (SF_FORMAT_WAV) ;
                CASE_NAME (SF_FORMAT_AIFF) ;
                CASE_NAME (SF_FORMAT_AU) ;
                CASE_NAME (SF_FORMAT_RAW) ;
                CASE_NAME (SF_FORMAT_PAF) ;
                CASE_NAME (SF_FORMAT_SVX) ;
                CASE_NAME (SF_FORMAT_NIST) ;
                CASE_NAME (SF_FORMAT_VOC) ;
                CASE_NAME (SF_FORMAT_IRCAM) ;
                CASE_NAME (SF_FORMAT_W64) ;
                CASE_NAME (SF_FORMAT_MAT4) ;
                CASE_NAME (SF_FORMAT_MAT5) ;
                CASE_NAME (SF_FORMAT_PVF) ;
                CASE_NAME (SF_FORMAT_XI) ;
                CASE_NAME (SF_FORMAT_HTK) ;
                CASE_NAME (SF_FORMAT_SDS) ;
                CASE_NAME (SF_FORMAT_AVR) ;
                CASE_NAME (SF_FORMAT_WAVEX) ;
                CASE_NAME (SF_FORMAT_SD2) ;
                CASE_NAME (SF_FORMAT_FLAC) ;
                CASE_NAME (SF_FORMAT_CAF) ;
                CASE_NAME (SF_FORMAT_WVE) ;
                CASE_NAME (SF_FORMAT_OGG) ;
                default :
                        break ;
                } ;

        return "BAD_MAJOR_FORMAT" ;
} /* str_of_major_format */

const char* str_of_minor_format (int format)
{
	switch (SF_CODEC (format))
        {       CASE_NAME (SF_FORMAT_PCM_S8) ;
                CASE_NAME (SF_FORMAT_PCM_16) ;
                CASE_NAME (SF_FORMAT_PCM_24) ;
                CASE_NAME (SF_FORMAT_PCM_32) ;
                CASE_NAME (SF_FORMAT_PCM_U8) ;
                CASE_NAME (SF_FORMAT_FLOAT) ;
                CASE_NAME (SF_FORMAT_DOUBLE) ;
                CASE_NAME (SF_FORMAT_ULAW) ;
                CASE_NAME (SF_FORMAT_ALAW) ;
                CASE_NAME (SF_FORMAT_IMA_ADPCM) ;
                CASE_NAME (SF_FORMAT_MS_ADPCM) ;
                CASE_NAME (SF_FORMAT_GSM610) ;
                CASE_NAME (SF_FORMAT_VOX_ADPCM) ;
                CASE_NAME (SF_FORMAT_G721_32) ;
                CASE_NAME (SF_FORMAT_G723_24) ;
                CASE_NAME (SF_FORMAT_G723_40) ;
                CASE_NAME (SF_FORMAT_DWVW_12) ;
                CASE_NAME (SF_FORMAT_DWVW_16) ;
                CASE_NAME (SF_FORMAT_DWVW_24) ;
                CASE_NAME (SF_FORMAT_DWVW_N) ;
                CASE_NAME (SF_FORMAT_DPCM_8) ;
                CASE_NAME (SF_FORMAT_DPCM_16) ;
                CASE_NAME (SF_FORMAT_VORBIS) ;
                default :
                        break ;
                } ;

        return "BAD_MINOR_FORMAT" ;
} /* str_of_minor_format */



int main(int argc, char **argv)
{
	SNDFILE *snf;
	SF_INFO  sinfo={.format=0 };

	int ret;

	unsigned int frame_total; 	  /* total number of frames */
	unsigned int nchan;		  /* number of channels */
	unsigned int srate;		  /* sample rate */
	bool	     is_interleaved=true; /* Assume TRUE in most case */

	int		nf=512*2;		  /* Number of frames for each playback */
	short buff[1024*4];	  	  /* LIMIT: U8-S16 MAX. S16/16*nchan(2)*nf(512) = 1024bytes */
					/* if nchan=5 S16/16*nchan(5)*nf(512)=2560; */
	if(argc<2) {
		printf("Usage:  %s file \n", argv[0] );
		return 1;
	}

	snf=sf_open(argv[1], SFM_READ, &sinfo); /* SFM_READ, SFM_WRITE, SFM_RDWR */
	if(snf==NULL) {
		printf("Fail to open wav file, %s \n",sf_strerror(NULL));
		return -1;
	}

	printf("--- wav file info --- \n");
	printf("Frames:		%"PRIu64"\n", sinfo.frames);
	printf("Sample Rate:	%d\n", sinfo.samplerate);
	printf("Channels:	%d\n", sinfo.channels);
	printf("Formate:  	%s %s\n", str_of_major_format(sinfo.format),
					  str_of_minor_format(sinfo.format) );
	printf("Sections:	%d\n", sinfo.sections);
	printf("seekable:	%d\n", sinfo.seekable);
	printf("COPYRIGHT:	%s\n", sf_get_string(snf,SF_STR_COPYRIGHT) );
	printf("TITLE:		%s\n", sf_get_string(snf,SF_STR_TITLE) );
	printf("ARTIST:		%s\n", sf_get_string(snf,SF_STR_ARTIST) );
	printf("COMMENT:	%s\n", sf_get_string(snf,SF_STR_COMMENT) );

	nchan=sinfo.channels;
	srate=sinfo.samplerate;

	/*
sf_count_t      sf_seek                 (SNDFILE *sndfile, sf_count_t frames, int whence)
sf_count_t      sf_read_raw             (SNDFILE *sndfile, void *ptr, sf_count_t bytes) ;
sf_count_t      sf_write_raw    (SNDFILE *sndfile, const void *ptr, sf_count_t bytes) ;
	sf_read(write)_short( SNDFILE *sndfile, short *ptr, sf_count_t items);
	sf_read(write)_int(...)
	sf_read(write)_float(...)
	sf_read(write)_double(...)
	sf_readf(write)_... 		[ sf_count_t frames ]

sf_count_t      sf_readf_short  (SNDFILE *sndfile, short *ptr, sf_count_t frames) ;
sf_count_t      sf_writef_short (SNDFILE *sndfile, const short *ptr, sf_count_t frames) ;

sf_count_t      sf_readf_int    (SNDFILE *sndfile, int *ptr, sf_count_t frames) ;
sf_count_t      sf_writef_int   (SNDFILE *sndfile, const int *ptr, sf_count_t frames) ;

sf_count_t      sf_readf_float  (SNDFILE *sndfile, float *ptr, sf_count_t frames) ;
sf_count_t      sf_writef_float (SNDFILE *sndfile, const float *ptr, sf_count_t frames) ;

sf_count_t      sf_readf_double         (SNDFILE *sndfile, double *ptr, sf_count_t frames) ;
sf_count_t      sf_writef_double        (SNDFILE *sndfile, const double *ptr, sf_count_t frames) ;

	*/


	/* --- TEST SET STRING ---
	  sf_set_string (snf, SF_STR_ARTIST, "AVENGER");
	  goto CLOSE_SNF;
	*/

	/* open pcm device  */
	if( egi_prepare_pcm_device(nchan, srate, true) !=0 ) {
		printf("Fail to prepare PCM device!\n");
		exit(-2);
	}
	printf("PCM HW period size is %d frames.\n", egi_pcm_period_size());

   while(1) {

	/* read interleaved data */
	ret=sf_readf_short(snf, buff, nf);
	if( ret<=0 )
		break;
	if(ret != nf){
		printf("WARNING: sf_readf_short() returns %d frames, less than required nf=%d \n",ret, nf);
	}

	/* playback */
	egi_play_pcm_buff((void *)&buff, ret);
   }

	printf("close pcm device...\n");
	egi_close_pcm_device();


CLOSE_SNF:
	printf("sf_close...\n");
	sf_close(snf);

return 0;
}

