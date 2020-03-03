#VPATH ../:../iot:../utils

export STAGING_DIR = /home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR = /home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

APP=egi_fifo

OBJS= egi_utils.o egi_fifo.o egi_filo.o egi_iwinfo.o egi_cstring.o ../egi_log.o ../egi_timer.o

## Shall also include all sys libs head file dir
CFLAGS += -Wall -I../ -I../utils -I$(COMMON_USRDIR)/include
CFLAGS += -D_GNU_SOURCE 	## for O_CLOEXEC flag ##
LDFLAGS += -L$(COMMON_USRDIR)/lib
LIBS += -lpthread
LIBS += -lcurl -lssl -lcrypto

read_conf: read_conf.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) read_conf.c -o read_conf

test_fifo: test_fifo.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) test_fifo.c -o test_fifo

test_filo: test_filo.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) test_filo.c -o test_filo

test_cstring: test_cstring.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) test_cstring.c -o test_cstring

egi_cstring.o:  egi_cstring.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_cstring.c

egi_fifo.o: egi_fifo.h egi_fifo.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_fifo.c

egi_filo.o: egi_filo.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_filo.c

egi_utils.o: egi_utils.h egi_utils.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_utils.c

egi_iwinfo.o: egi_iwinfo.h egi_iwinfo.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_iwinfo.c

egi_https.o: egi_https.h egi_https.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_https.c

egi_fftdemo.o: egi_fftdemo.h egi_fftdemo.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c egi_fftdemo.c




.PHONY:	clean
clean:
	rm -rf $(APP) $(APP).o *.o
	rm -rf test_fifo test_filo test_cstring read_conf

#CFLAGS += $(shell pkg-config --cflags json-c)
#LDFLAGS += $(shell pkg-config --libs json-c)

