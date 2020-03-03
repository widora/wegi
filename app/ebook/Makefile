export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr
SRC_PATH=/home/midas-zhou/Ctest/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

CFLAGS  = -I$(COMMON_USRDIR)/include  -I$(SRC_PATH) -I$(SRC_PATH)/utils -I$(SRC_PATH)/page
CFLAGS  += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS  += -Wall -O2

LDFLAGS += -L$(SRC_PATH)/lib -L$(COMMON_USRDIR)/lib

LIBS	=  -lepages -lesound
LIBS	+= -lpng -lz -lm -pthread -ljpeg -lasound
LIBS	+= -lcurl -lssl -lcrypto -pthread
LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c
LIBS 	+= -lfreetype  -lm -lz -lbz2

APPS = app_ebook

all:	$(APPS)
### !!! NOTE: put '-o $@  $@.c' ahead of FLAGS and LIBS in LD !!!!

app_ebook: app_ebook.c page_ebook.o
	$(CC) app_ebook.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi page_ebook.o -o app_ebook
#	$(CC) app_ebook.c $(CFLAGS) $(LDFLAGS) -Wl,-Bstatic -legi -Wl,-Bdynamic $(LIBS) page_ebook.o -o app_ebook


page_ebook.o:  page_ebook.c  page_ebook.h
	$(CC) -c page_ebook.c $(CFLAGS) $(LDFLAGS) -legi $(LIBS)
#	$(CC) -c page_ebook.c $(CFLAGS) $(LDFLAGS) -Wl,-Bstatic -legi -Wl,-Bdynamic  $(LIBS)


clean:
	rm -rf *.o $(APPS) *.dep

