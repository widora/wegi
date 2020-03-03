
export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
#CC = gcc

#OBJ	= ../egi_symbol.o

CFLAGS  += -I../ -I../utils
CFLAGS  += -I$(COMMON_USRDIR)/include
CFLAGS  += -I$(COMMON_USRDIR)/include/freetype2

LDFLAGS += -L$(COMMON_USRDIR)/lib
LDFLAGS += -L../lib
#libblobmsg_json.so  libjson_script.so  libubox.so
#LIBS   += -lubus -lubox -lblobmsg_json -ljson_script -ljson-c
LIBS    = -lfreetype  -lm -lz -lbz2
LIBS    += -lepages -lesound
LIBS    += -lpng -lz -lm -pthread -ljpeg -lasound
LIBS    += -lcurl -lssl -lcrypto -pthread
LIBS    += -lubox -lubus -lblobmsg_json -ljson_script -ljson-c


all:	 test_freetype get_sympgHeight test_asciibook test_wchar test_wbook

test_freetype:	test_freetype.c
	$(CC) $(CFLAGS) $(LDFLAGS) -legi $(LIBS) $(OBJ) test_freetype.c -o test_freetype

test_asciibook:	test_asciibook.c
	$(CC) $(CFLAGS) $(LDFLAGS) -legi $(LIBS) $(OBJ) test_asciibook.c -o test_asciibook

get_sympgHeight: get_sympgHeight.c
	$(CC) $(CFLAGS) $(LDFLAGS) -legi $(LIBS) $(OBJ) get_sympgHeight.c -o get_sympgHeight

test_wchar:	test_wchar.c
	$(CC) $(CFLAGS) $(LDFLAGS) -legi $(LIBS) $(OBJ) test_wchar.c -o test_wchar

test_wbook:	test_wbook.c  ../lib/libegi.a  # link LIBEGI statically.
	$(CC) test_wbook.c $(CFLAGS) $(LDFLAGS) $(OBJS) -Wl,-Bstatic -legi -Wl,-Bdynamic $(LIBS) -o test_wbook

clean:
	rm -rf *.o test_freetype test_asciibook get_sympgHeight test_wchar test_wbook


