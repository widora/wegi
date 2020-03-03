
 /*--------------------------------------------------------------------------
 Quote from :  http://blog.csdn.net/leixiaohua1020/article/details/50534316

 -------   Convert PCM16LE raw data to WAVE format   ----------

 ----------------------------------------------------------------------------*/

#ifndef __PCM2WAV_H__
#define __PCM2WAV_H__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct WAVE_HEADER{
        char         fccID[4];
        unsigned   long    dwSize;
        char         fccType[4];
}WAVE_HEADER;

typedef struct WAVE_FMT{
        char         fccID[4];
        unsigned   long       dwSize;
        unsigned   short     wFormatTag;
        unsigned   short     wChannels;
        unsigned   long       dwSamplesPerSec;
        unsigned   long       dwAvgBytesPerSec;
        unsigned   short     wBlockAlign;
        unsigned   short     uiBitsPerSample;
}WAVE_FMT;

typedef struct WAVE_DATA{
        char       fccID[4];
        unsigned long dwSize;
}WAVE_DATA;

 /**
 * Convert PCM16LE raw data to WAVE format
 * @pcm			pcm raw data
 * @wavepath      	input PCM file
 * @channels      	channel number
 * @sample_rate   	sample rate
 * @pcm_size       	pcm data size
 */
int simplest_pcm16le_to_wave(const unsigned char *pcm,const char *wavepath,int channels,int sample_rate, unsigned long pcm_size)
{

    /* set default */
    if(channels==0||sample_rate==0){
    	channels = 2;
    	sample_rate = 44100;
    }

    int bits = 16;    /* sample depth */
    WAVE_HEADER   pcmHEADER;
    WAVE_FMT   pcmFMT;
    WAVE_DATA   pcmDATA;

    unsigned   short   m_pcmData;
    FILE  *fpout;

    fpout=fopen(wavepath,   "wb+");
    if(fpout == NULL) {
        printf("create wav file error\n");
        return -1;
    }

    /* WAVE_HEADER */
    memcpy(pcmHEADER.fccID,"RIFF",strlen("RIFF"));
    memcpy(pcmHEADER.fccType,"WAVE",strlen("WAVE"));
    pcmHEADER.dwSize=44+pcm_size;
    fwrite(&pcmHEADER,sizeof(WAVE_HEADER),1,fpout);

    /*WAVE_FMT*/
    pcmFMT.dwSamplesPerSec=sample_rate;
    pcmFMT.dwAvgBytesPerSec=pcmFMT.dwSamplesPerSec*sizeof(m_pcmData);
    pcmFMT.uiBitsPerSample=bits;
    memcpy(pcmFMT.fccID,"fmt ",strlen("fmt "));
    pcmFMT.dwSize=16;
    pcmFMT.wBlockAlign=2;
    pcmFMT.wChannels=channels;
    pcmFMT.wFormatTag=1;
    fwrite(&pcmFMT,sizeof(WAVE_FMT),1,fpout);

    /* WAVE_DATA */
    memcpy(pcmDATA.fccID,"data",strlen("data"));
    pcmDATA.dwSize=pcm_size;
    fwrite(&pcmDATA,sizeof(WAVE_DATA),1,fpout);

    /* write PCM DATA */
    fwrite(pcm, pcm_size, 1, fpout);

    fclose(fpout);

    return 0;
}

#endif
