export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr

SRC_PATH=/home/midas-zhou/Ctest/wegi

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc


#### ----- 产生文件列表 ------
SRC_FILES = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRC_FILES))

#OBJS += heweather/he_weather.o

$(warning "----- OBJS:$(OBJS) -----")

##--- exclude some objs !!!BEWARE, :NO SPACE ----
#OBJS := $(OBJS:test_color.o=)

DEP_FILES = $(patsubst %.c,%.dep,$(SRC_FILES))


CFLAGS  = -I$(SRC_PATH) -I$(SRC_PATH)/utils  -I$(SRC_PATH)/iot -I$(SRC_PATH)/sound
CFLAGS += -I$(COMMON_USRDIR)/include
CFLAGS += -I$(COMMON_USRDIR)/include/freetype2
CFLAGS += -I$(COMMON_USRDIR)/include/json-c

#-I$(SRC_PATH)/page
CFLAGS  += -Wall -O2
CFLAGS  += -D_GNU_SOURCE
CFLAGS  += -DENABLE_BACK_BUFFER

LDFLAGS += -L$(SRC_PATH)/lib -L$(COMMON_USRDIR)/lib

LIBS	=  -lepages
LIBS    += -lesound
LIBS	+= -lpng -lz -lm -pthread -ljpeg -lasound -lsndfile
LIBS	+= -lcurl -lssl -lcrypto -pthread
LIBS	+= -lubox -lubus -lblobmsg_json -ljson_script -ljson-c
LIBS    += -lfreetype  -lm -lz -lbz2
LIBS    += -lgif

APP_NAME = app_juhe

#--- use static or dynamic libs -----
EGILIB=dynamic

#### --- use dynamic and static libs in compilatoin!!! ---
# 1. Exclude dynamic -legi first.
# 2. Use -Wl,Bdynamic and -Wl,Bstatic to switch to dynamic/static libs
# 3. At last, we must switch back to dynamic libs, as default for Linux system runtime setup.
# 4. Source/obj files must be put before all -Wl.

#all:	$(APPS)

$(APP_NAME):  $(OBJS) $(DEP_FILES)
ifeq ($(EGILIB),static)
	$(CC) $(OBJS) -o $(APP_NAME)  $(CFLAGS) $(LDFLAGS) -Wl,-Bdynamic $(LIBS) \
-Wl,-Bstatic -legi -lesound -Wl,-Bdynamic
else
	$(CC) -o $(APP_NAME) $(OBJS) $(CFLAGS) $(LDFLAGS) $(LIBS) -legi -lesound
endif

#### ----- 目标文件自动生成规则 -----
%:%.c $(DEP_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c -o $@ $@.c


include $(DEP_FILES)

$(warning "----- %.dep: %.c -----")
#### ---- 依赖文件自动生成规则 -----
%.dep: %.c
	@set -e; rm -f $@
	@$(CC) -MM $(CFLAGS) $< > $@.123	### $< 表示第一个依赖文件 $@ 当前目标文件
	@sed 's,\($*\)\.o[: ]*,\1 : ,g' < $@.123 > $@
#	cat $@.123
#------- \1 表示第一个$(*)所代表的内容
	@rm -f $@.123


#### ----- 清除目标 -----
clean:
	rm -rf $(OBJS) $(APP_NAME) $(DEP_FILES)

