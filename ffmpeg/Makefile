export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

SRC_PATH=/home/midas-zhou/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
CPP= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

# !!!--- to be created and cleaned ---!!!
OBJ_IOT = ../iot/egi_iotclient.o

CFLAGS  = -I$(SRC_PATH) -I$(SRC_PATH)/utils  -I$(SRC_PATH)/iot -I$(SRC_PATH)/sound
CFLAGS += -I$(COMMON_USRDIR)/include
CFLAGS += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS += -I$(COMMON_USRDIR)/include/json-c
CFLAGS += -I/home/midas-zhou/helixaac/aac_pub
##CFLAGS += -I/home/midas-zhou/giflib-5.2.1/finish/include

#-I$(SRC_PATH)/page
CFLAGS  += -Wall -O2
CFLAGS  += -D_GNU_SOURCE
CFLAGS  += -D_FILE_OFFSET_BITS=64
CFLAGS  += -DENABLE_BACK_BUFFER
CFLAGS  += -Wno-maybe-uninitialized

## -std=c99 will fail MACRO typeof()
#CFLAGS  += '-std=c99'
CFLAGS += '-std=gnu99'

LDFLAGS += -L$(SRC_PATH)/lib -L$(COMMON_USRDIR)/lib
##LDFLAGS += -L/home/midas-zhou/giflib-5.2.1/finish/lib
LIBS    = -lavutil -lswscale -lavcodec -lavformat -lswresample -lavfilter -lpostproc
LIBS    += -lx264  -lmp3lame -lbz2
LIBS	+=  -lepages
LIBS    += -lesound
LIBS	+= -lpng -lz -lm -pthread -ljpeg -lasound -lsndfile
LIBS	+= -lcurl -lssl -lcrypto -pthread
LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c
LIBS    += -lfreetype -lz -lm -lbz2
LIBS	+= -lsqlite3
LIBS 	+= -lgif
LIBS	+= -lrt
LIBS	+= -lutil
LIBS	+= -lmad
#APPS =  test_fb test_sym tmp_app show_pic  test_bigiot test_math test_fft test_sndfft test_tonefft
#APPS += test_txt test_img test_img2 test_img3 test_resizeimg test_zoomimg test_etouch  test_geom
#APPS += test_bjp test_fbbuff

#--- use static or dynamic libs -----
EGILIB=dynamic

#### --- use dynamic and static libs in compilation!!! ---
# 1. Exclude dynamic -legi first.
# 2. Use -Wl,Bdynamic and -Wl,Bstatic to switch to dynamic/static libs
# 3. At last, we must switch back to dynamic libs, as default for Linux system runtime setup.
# 4. Source/obj files must be put before all -Wl.


### 		----- A template for making test app -----
###	Usage example: make test TEST_NAME=test_resizeimg EGILIB=static
###
test:	$(TEST_NAME).c
ifeq ($(EGILIB),static)
	$(CC) $(TEST_NAME).c -o $(TEST_NAME) $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lhelixaac -Wl,-Bdynamic
else
	$(CC) -o $(TEST_NAME) $(TEST_NAME).c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif

### 			------ template end -----



all:	$(APPS)


test_ffutils:   egi_ffplay.o ff_utils.o
ifeq ($(EGILIB),static)
	$(CC)  ff_utils.o egi_ffplay.o test_ffutils.c -o test_ffutils $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic

egi_ffplay.o:   egi_ffplay.c egi_ffplay.h ff_utils.o
	$(CC) -c egi_ffplay.c $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic

ff_utils.o: ff_utils.c ff_utils.h
	$(CC) -c ff_utils.c $(CFLAGS) $(LDFLAGS)-Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic

else
	$(CC) egi_ffplay.o ff_utils.o -o egi_ffplay $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound


endif


ffplay6:   ff_utils.o
ifeq ($(EGILIB),static)
	$(CC)  ff_utils.o ffplay6.c -o ffplay6 $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic

ff_utils.o: ff_utils.c ff_utils.h
	$(CC)  -c ff_utils.c $(CFLAGS) $(LDFLAGS)-Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic

else

endif




show_pic:	show_pic.c
ifeq ($(EGILIB),static)
	$(CC) show_pic.c -o show_pic  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o test_img show_pic.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


