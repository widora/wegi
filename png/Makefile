
export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr
SRC_PATH=/home/midas-zhou/Ctest/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

#OBJS += ../utils/egi_utils.o ../utils/egi_fifo.o ../utils/egi_filo.o

CFLAGS  = -I$(COMMON_USRDIR)/include	-I../. -I../utils/
CFLAGS  += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS += -Wall -O2

LDFLAGS += -L$(SRC_PATH)/lib -L$(COMMON_USRDIR)/lib


LIBS	= -legi
LIBS	+= -lpng -lz -lm -pthread -ljpeg
#LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c -pthread
LIBS	+= -lcurl -lssl -lcrypto -pthread
LIBS 	+= -lfreetype  -lm -lz -lbz2


APPS = test_png test_blend loop_show


all:	$(APPS)


%:%.c
	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) $@.c -o $@


#loop_show:	loop_show.c
#	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) loop_show.c -o loop_show

#test_png:	test_png.c
#	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) test_png.c -o test_png

#test_alpha:	test_alpha.c
#	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) test_alpha.c -o test_alpha


clean:
	rm -rf *.o $(APPS)

