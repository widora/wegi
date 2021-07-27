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

LIBS	=  -lepages
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


test_etouch:	test_etouch.c
ifeq ($(EGILIB),static)
	$(CC) test_etouch.c -o test_etouch  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o test_etouch test_etouch.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


#test_txt:	test_txt.c
#ifeq ($(EGILIB),static)
#	$(CC) test_txt.c -o test_txt  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
#-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
#else
#	$(CC) -o test_txt test_txt.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
#endif

#test_img:	test_img.c
#ifeq ($(EGILIB),static)
#	$(CC) test_img.c -o test_img  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
#-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
#else
#	$(CC) -o test_img test_img.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
#endif

show_pic:	show_pic.c
ifeq ($(EGILIB),static)
	$(CC) show_pic.c -o show_pic  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o test_img show_pic.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


test_bigiot:	test_bigiot.c  $(OBJ_IOT)
	$(CC) test_bigiot.c -o test_bigiot $(OBJ_IOT) $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -Wl,-Bdynamic


test_math:	test_math.c  ../egi_math.h
#	$(CC) -o test_math test_math.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi  #--use shared egilib
	$(CC) test_math.c -o test_math $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -Wl,-Bdynamic
#---use static egilib

test_fft:	test_fft.c  ../egi_math.h
#	$(CC) -o test_math test_math.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi  #--use shared egilib
	$(CC) test_fft.c -o test_fft $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -Wl,-Bdynamic
#---use static egilib

test_sndfft:	test_sndfft.c  ../egi_math.h
#	$(CC) -o test_math test_math.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi  #--use shared egilib
	$(CC) test_sndfft.c -o test_sndfft $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
#---use static egilib

test_tonefft:	test_tonefft.c  ../egi_math.h
#	$(CC) -o test_math test_math.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi  #--use shared egilib
	$(CC) test_tonefft.c -o test_tonefft $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
#---use static egilib


### !!! NOTE: put '-o $@  $@.c' ahead of FLAGS and LIBS !!!!
%:%.c
	$(CC) -o $@  $@.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi
#	$(CC)  $(CFLAGS) $(LDFLAGS) $(LIBS)  $@.c -o $@

clean:
	rm -rf *.o $(APPS) $(OBJ_IOT)

