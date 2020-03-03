export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
AR= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-ar

SRC_PATH = /home/midas-zhou/Ctest/wegi


#### ----- 产生文件列表 ------
SRC_FILES = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRC_FILES))

$(warning "----- OBJS:$(OBJS) -----")

##--- exclude some objs !!!BEWARE, :NO SPACE ----
#OBJS := $(OBJS:test_color.o=)

DEP_FILES = $(patsubst %.c, %.dep, $(SRC_FILES))


#### -----  编译标志 -----
## -Wall for __attribut__
CFLAGS := -I./ -I../ -I../iot -I../ffmpeg -I../utils  -I../heweather
CFLAGS += -I$(COMMON_USRDIR)/include -I$(COMMON_USRDIR)/include/json-c
CFLAGS += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS += -Wall -O2
CFLAGS += -D_GNU_SOURCE         ## for O_CLOEXEC flag ##
#### --- for debug, put before LDFLAGS!!!! ----
#CFLAGS += -g -DEGI_DEBUG

LDFLAGS += -L./lib -L$(COMMON_USRDIR)/lib
LIBS += -legi -ljpeg -lm -lpthread


#### ------ 手动设置特定目标生成规则  ------
libepages.a: $(OBJS)
	$(AR) crv libepages.a $(OBJS)

#	$(CC) -shared -soname libegi.so.1 $(OBJS) -o libegi.so.1.0.0

install: libepages.a
	mv libepages.a $(SRC_PATH)/lib

#### ----- 目标文件自动生成规则 -----
%:%.c $(DEP_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c -o $@ $@.c

#### ----- 清除目标 -----
clean:
	rm -rf *.o $(OBJS) $(DEP_FILES) libepages.a


include $(DEP_FILES)
$(warning "----- %.dep: %.c -----")
#### ---- 依赖文件自动生成规则 -----
%.dep: %.c
	@set -e; rm -f $@
	@$(CC) -MM $(CFLAGS) $< > $@.123
	@sed 's,\($*\)\.o[: ]*,\1 : ,g' < $@.123 > $@
	cat $@.123


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
