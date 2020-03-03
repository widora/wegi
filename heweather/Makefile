
export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr
SRC_PATH = /home/midas-zhou/Ctest/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

#OBJS = ../egi_log.o ../utils/egi_utils.o ../egi_timer.o

CFLAGS  += -I$(COMMON_USRDIR)/include	-I$(SRC_PATH)/. -I$(SRC_PATH)/utils
CFLAGS  += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS  += -D_GNU_SOURCE	# for strdupa

LDFLAGS += -L$(COMMON_USRDIR)/lib  -L$(SRC_PATH)/lib/

#LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c -pthread
LIBS   =  -ljpeg -lpng -lz	#-legi
LIBS  +=  -lcurl -lssl -lcrypto -pthread -ljson-c
LIBS  +=  -lfreetype  -lm -lz -lbz2

all:	test_now test_forecast he_weather.o

#https:	https.c
#	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) https.c -o https

test_forecast:	test_forecast.c  he_weather.o
#	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS)  he_weather.o test_forecast.c -o test_forecast
	$(CC) he_weather.o test_forecast.c -o test_forecast $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -Wl,-Bdynamic
#---use static egilib


test_now:	test_now.c  he_weather.o
	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) -legi  he_weather.o test_now.c -o test_now


he_weather.o:  he_weather.c he_weather.h
	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -c he_weather.c


clean:
	rm -rf *.o  test_now test_forecast

