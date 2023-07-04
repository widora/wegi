export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

SRC_PATH=/home/midas-zhou/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
CPP= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

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

EGILIB=static


### test_train ###
test_memleak:   test_memleak.c nnc.o actfs.o
ifeq ($(EGILIB),static)
	$(CC) test_memleak.c nnc.o actfs.o -o test_memleak  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o test_memleak test_memleak.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


### test_train ###
test_train:   digits_imgdata.h test_train.c nnc.o actfs.o
ifeq ($(EGILIB),static)
	$(CC) test_train.c nnc.o actfs.o -o test_train  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o test_train test_train.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif

### nnc_touchdraw ###
nnc_touchdraw:   digits_imgdata.h nnc_touchdraw.c  nnc.o actfs.o nnc_digits.o
ifeq ($(EGILIB),static)
	$(CC) nnc_touchdraw.c nnc.o actfs.o nnc_digits.o -o nnc_touchdraw  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o nnc_touchdraw nnc_touchdraw.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


### test_digit ###
test_digits:   test_digits.c  nnc.o actfs.o nnc_digits.o
ifeq ($(EGILIB),static)
	$(CC) test_digits.c nnc.o actfs.o nnc_digits.o -o test_digits  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o test_digits nnc_digits.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif

### nnc_digit.o ###
nnc_digits.o:   nnc_digits.c  nnc.o actfs.o
ifeq ($(EGILIB),static)
	$(CC) -c nnc_digits.c nnc.o actfs.o $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -c nnc_digits.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


###  actf.o  ###
actfs.o:
ifeq ($(EGILIB),static)
	$(CC) -c -o actfs.o actfs.c  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -c actfs.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif

###  nnc.o  ###
nnc.o:
ifeq ($(EGILIB),static)
	$(CC) -c -o nnc.o nnc.c  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -c -o nnc.o nnc.c $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif


clean:
	rm -rf *.o $(APPS) $(OBJ_IOT)
	rm -rf test_train test_digits nnc_touchdraw
