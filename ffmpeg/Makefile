export STAGING_DIR = /home/midas-zhou/openwrt_widora/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin
CC = $(STAGING_DIR)/mipsel-openwrt-linux-gcc

COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

SRC_PATH = /home/midas-zhou/Ctest/wegi

#APP = ffplay6
#APP = filtering_video

SOURCE = $(APP).c
DEPS = ff_utils.h ff_utils.c #ff_pcm.h ff_pcm.c
OBJ = $(APP).o ff_utils.o  #ff_pcm.o

APP2 = alsa_play
SOURCE2 = $(APP2).c

CFLAGS   += -I/home/midas-zhou/ffmpeg-2.8.15/finish/include
CFLAGS   += -I/home/midas-zhou/openwrt_widora/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/alsa-lib-1.0.28/include
#CFLAGS	 += -I/home/midas-zhou/Ctest/wegi/utils
CFLAGS   += -I$(COMMON_USRDIR)/include
CFLAGS   += -I$(SRC_PATH) -I$(SRC_PATH)/sound -I$(SRC_PATH)/utils
### for -Wall for __attribute__
CFLAGS   += -Wall

LDFLAGS  += -L/home/midas-zhou/ffmpeg-2.8.15/finish/lib
LDFLAGS  += -L$(SRC_PATH)/lib
LDFLAGS  += -L/home/midas-zhou/openwrt_widora/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/alsa-lib-1.0.28/ipkg-install/usr/lib
LDFLAGS  += -L$(COMMON_USRDIR)/lib

#------ -lz seems no use  ------------
LIBS     += -lavutil -lswscale -lavcodec -lavformat -lswresample -lavfilter -lpostproc
#LIBS     += -lftdi1 -lusb-1.0
LIBS	 += -lm -lasound -ljpeg
LIBS	 += -lpthread
LIBS	 += -lesound -legi -lpng -lz
LIBS 	 += -lcurl -lssl -lcrypto


#----------------  APP  --------------------
all:	$(APP)

$(APP): $(OBJ) $(DEPS)
	$(CC) -o $(APP) $(OBJ) $(CFLAGS) $(LDFLAGS) $(LIBS)

$(APP).o: $(SOURCE) $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c $(SOURCE)

egi_ffplay.o:	egi_ffplay.c egi_ffplay.h ff_utils.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_ffplay.c

ff_utils.o: ff_utils.c ff_utils.h
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c ff_utils.c

test_ffutils:  test_ffutils.c ff_utils.o egi_ffplay.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS)  ff_utils.o egi_ffplay.o ../sound/egi_pcm.o -o test_ffutils  test_ffutils.c


#---------------  APP2 -----------------------
$(APP2): $(SOURCE2) $(DEPS)
	$(CC) -o $(APPS) $(CFLAGS) $(LDFLAGS) $(LIBS) $(SOURCE2)


#%:%.c   $(SOURCE) $(DEPS)
#	$(CC)   $(CFLAGS) $(LDFLAGS) $(LIBS) -c $@.c

clean:
	rm -rf $(APP) $(APP2) *.o

