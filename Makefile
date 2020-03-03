export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
AR= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-ar
LD= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-ld

SRC_PATH = /home/midas-zhou/Ctest/wegi

#### ----- 产生文件列表 ------
SRC_FILES = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRC_FILES))
SRC_UTILS_FILES = $(wildcard utils/*.c)
OBJS += $(patsubst %.c, %.o, $(SRC_UTILS_FILES))
#OBJS += utils/egi_utils.o utils/egi_cstring.o utils/egi_fifo.o utils/egi_filo.o utils/egi_iwinfo.o
#OBJS += utils/egi_https.o
#OBJS += heweather/he_weather.o

$(warning "----- OBJS:$(OBJS)")

##--- exclude some objs !!!BEWARE, :NO SPACE ----
#OBJS := $(OBJS:test_color.o=)


DEP_FILES = $(patsubst %.c,%.dep,$(SRC_FILES))

#EXTRA_OBJS = iot/egi_iotclient.o utils/egi_utils.o utils/egi_iwinfo.o utils/egi_fifo.o \
#	     utils/egi_filo.o utils/egi_cstring.o \
#	     ffmpeg/ff_pcm.o


#### -----  编译标志 -----
## -Wall for __attribut__
CFLAGS  = -I./ -I./page -I./iot -I./ffmpeg -I./utils -I$(COMMON_USRDIR)/include
CFLAGS += -I$(COMMON_USRDIR)/include/json-c
CFLAGS += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS += -Wall -fPIC -O2 	#-Wno-unused-but-set-variable  # -Wno-unused-variable
CFLAGS += -D_GNU_SOURCE 	## for O_CLOEXEC flag ##
CFLAGS += -DENABLE_BACK_BUFFER

#### --- for debug, put before LDFLAGS!!!! ----
#CFLAGS += -g -DEGI_DEBUG

LDFLAGS += -L./lib -L$(COMMON_USRDIR)/lib

LIBS = -ljpeg -lm -lpthread #-ljson-c -lasound
LIBS += -lfreetype  -lm -lz -lbz2

#### ------ 手动设置特定目标生成规则  ------

all: shared_egilib static_egilib

shared_egilib: $(OBJS)
	$(LD) -shared -soname libegi.so.1 $(OBJS) -o libegi.so.1.0.0

# 1. Bewear of the order of the lib and objs.
# 2. Note!!! All libs here are compiled with -fPIC option.
static_egilib: $(OBJS)
	$(AR) rcv libegi.a $(OBJS)

install: libegi.so.1.0.0 libegi.a
	cd $(SRC_PATH)
	cp -rf libegi.a ./lib/libegi.a
	cp -rf libegi.so.1.0.0 ./lib/libegi.so.1.0.0
	ln -s -f $(SRC_PATH)/lib/libegi.so.1.0.0 $(SRC_PATH)/lib/libegi.so.1
	ln -s -f $(SRC_PATH)/lib/libegi.so.1.0.0 $(SRC_PATH)/lib/libegi.so
	rm libegi.so.1.0.0 libegi.a

#### ----- 目标文件自动生成规则 -----
%:%.c $(DEP_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c -o $@ $@.c


#### ----- 清除目标 -----
clean:
	rm -rf test_*.o libegi.so.1.0.0 libegi.a $(OBJS) $(APPS) $(DEP_FILES)


include $(DEP_FILES)
$(warning "----- %.dep: %.c -----")
#### ---- 依赖文件自动生成规则 -----
%.dep: %.c
	@set -e; rm -f $@
	@$(CC) -MM $(CFLAGS) $< > $@.123
	@sed 's,\($*\)\.o[: ]*,\1 : ,g' < $@.123 > $@
##	cat $@.123


#------- \1 表示第一个$(*)所代表的内容
	@rm -f $@.123


#----- 下面这个会被执行两遍 ---------
$(warning "----- end -----")

#-------------------------------------------------------------------------------------------
#
#  自动依赖的例子：
#  midas-zhou@midas-hp:~/apptest$ gcc -MM fbin_r.c | sed 's,\($*\)\.o[: ]*,\1 : ,g'
#  fbin_r : fbin_r.c fbin.h
#
#   gcc -M fbin_r.c 会将标准库的头文件也包括进来，用 gcc -MM 就不会了。
#
#-------------------------------------------------------------------------------------------
