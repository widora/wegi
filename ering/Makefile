
export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
#CC = gcc

OBJS = ../egi_log.o  ../utils/egi_utils.o ../egi_timer.o

CFLAGS  += -I$(COMMON_USRDIR)/include	-I../.
LDFLAGS += -L$(COMMON_USRDIR)/lib
LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c -pthread


all:	ering_server ering_client

ering_server:	ering_server.c egi_ring.h egi_ring.o
	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) egi_ring.o ering_server.c -o ering_server

ering_client:	ering_client.c egi_ring.h egi_ring.o
	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) egi_ring.o ering_client.c -o ering_client

egi_ring.o:	egi_ring.c egi_ring.h ../egi_debug.h ../egi_log.h
	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_ring.c

clean:
	rm -rf *.o ering_server ering_client

