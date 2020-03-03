export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr
SRC_PATH=/home/midas-zhou/Ctest/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

#### ----- 产生文件列表 ------
SRC_FILES = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRC_FILES))
DEP_FILES = $(patsubst %.c,%.dep,$(SRC_FILES))

##--- exclude some objs !!!BEWARE, :NO SPACE ----
OBJS := $(OBJS:test_ffmuz.o=)
OBJS := $(OBJS:app_ffmusic.o=)

CFLAGS  = -I$(COMMON_USRDIR)/include  -I$(SRC_PATH) -I$(SRC_PATH)/utils/ -I$(SRC_PATH)/page/  -I$(SRC_PATH)/ffmpeg/
CFLAGS  += -I/home/midas-zhou/ffmpeg-2.8.15/finish/include
CFLAGS  += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS  += -Wall -O2

LDFLAGS += -L$(SRC_PATH)/lib -L$(COMMON_USRDIR)/lib
LDFLAGS += -L/home/midas-zhou/ffmpeg-2.8.15/finish/lib

LIBS	= -lepages -lesound -lsndfile #-legi
LIBS	+= -lpng -lz -lm -pthread -ljpeg -lasound
LIBS	+= -lcurl -lssl -lcrypto -pthread
LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c
LIBS    += -lavutil -lswscale -lavcodec -lavformat -lswresample -lavfilter -lpostproc
LIBS 	+= -lfreetype  -lm -lz -lbz2

#--- use static or dynamic libs -----
EGILIB=dynamic


#---!!! OBJS will be made and cleaned !!!---
#OBJS = page_ffmusic.o $(SRC_PATH)/ffmpeg/egi_ffplay.o $(SRC_PATH)/ffmpeg/ff_utils.o $(SRC_PATH)/sound/egi_pcm.o
APPS = app_ffmusic


all:	$(APPS)
### !!! NOTE: put '-o $@  $@.c' ahead of FLAGS and LIBS in LD !!!!

app_ffmusic:  app_ffmusic.c $(OBJS)
###  Use dynamic or static egi_libs
ifeq ($(EGILIB),static)
	$(CC) $(OBJS) -o app_ffmusic $(CFLAGS) $(LDFLAGS) $(LIBS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -Wl,-Bdynamic
else
	$(CC) $(OBJS) -o app_ffmusic  $(CFLAGS) $(LDFLAGS) $(LIBS) -legi   #Sequence!
endif



test_ffmuz: test_ffmuz.c $(OBJS) test_ffmuz.o
ifeq ($(EGILIB),static)
	$(CC) $(OBJS) test_ffmuz.o -o test_ffmuz $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -Wl,-Bdynamic
else
	$(CC) $(OBJS) test_ffmuz.o -o test_ffmuz $(CFLAGS) $(LDFLAGS) $(LIBS) -legi
endif



#page_ffmusic.o:	page_ffmusic.c  page_ffmusic.h
#	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -c page_ffmusic.c


#### ----- 目标文件自动生成规则 -----
%:%.c $(DEP_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c -o $@ $@.c


#### ---- 依赖文件自动生成规则 -----
include $(DEP_FILES)
$(warning "----- %.dep: %.c -----")
%.dep: %.c
	@set -e; rm -f $@
	@$(CC) -MM $(CFLAGS) $< > $@.123
	@sed 's,\($*\)\.o[: ]*,\1 : ,g' < $@.123 > $@
	cat $@.123
#------- \1 表示第一个$(*)所代表的内容
	@rm -f $@.123

clean:
	rm -rf *.o $(APPS) *.dep $(OBJS)

