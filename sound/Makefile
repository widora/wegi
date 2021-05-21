export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
AR= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-ar

SRC_PATH = /home/midas-zhou/wegi

APP = test_pcmbuf recmp3 test_snd test_tone test_recplay
#shine_test2
#autorecord4

#OBJS = egi_pcm.o

CFLAGS  = -I./
CFLAGS += -I$(SRC_PATH) -I$(SRC_PATH)/utils -I$(SRC_PATH)/iot -I$(SRC_PATH)/sound
CFLAGS += -I$(COMMON_USRDIR)/include
CFLAGS += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS += -I$(COMMON_USRDIR)/include/json-c

#CFLAGS += -I/home/midas-zhou/openwrt_widora/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/alsa-lib-1.0.2/include
CFLAGS += -Wall -O2
CFLAGS += -D_FILE_OFFSET_BITS=64

LDFLAGS  += -L.
LDFLAGS += -L../lib -L$(COMMON_USRDIR)/lib
#LDFLAGS += -I/home/midas-zhou/openwrt_widora/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/alsa-lib-1.0.2/ipkg-install/usr/lib

LIBS	+= -lasound  -lshine -lsndfile
LIBS    += -lpng -ljpeg -lz
LIBS 	+= -lcurl -lssl -lcrypto
LIBS 	+= -lm -lz -pthread -lbz2
LIBS 	+= -lfreetype
LIBS    += -legi
LIBS    += -lgif


all:	$(APP) libesound.a


autorec: autorec.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o autorec autorec.c

recmp3: recmp3.c
#	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o recmp3 recmp3.c	#--use shared egilib
	$(CC) recmp3.c -o recmp3 $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -lesound -legi -Wl,-Bdynamic
#---use static egilib  !!!! -lesound needs -legi, with needs LIBS !!!!

test_minimad: test_minimad.c
	$(CC) test_minimad.c -o test_minimad $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -lesound -legi -Wl,-Bdynamic -lmad

test_pcmbuf: test_pcmbuf.c
	$(CC) -o test_pcmbuf test_pcmbuf.c $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) -Wl,-Bstatic -lesound -legi \
-Wl,-Bdynamic

test_snd: test_snd.c
	$(CC) -o test_snd test_snd.c $(CFLAGS) $(LDFLAGS) $(LIBS) -lesound

sndfile-to-text: sndfile-to-text.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o sndfile-to-text sndfile-to-text.c

test_tone: test_tone.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o test_tone test_tone.c

test_recplay: test_recplay.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o test_recplay test_recplay.c

libesound.a: egi_pcm.o
	$(AR) crv $@ egi_pcm.o

egi_pcm.o: egi_pcm.c egi_pcm.h
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_pcm.c

install:
	cp -rf libesound.a $(SRC_PATH)/lib
	rm libesound.a

clean:
	rm -rf $(APP) *.o libesound.a

