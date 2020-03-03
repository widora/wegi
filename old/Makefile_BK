#-------------------------------------------------------------------------------------------------------------
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License version 2 as
#	published by the Free Software Foundation.
#
#	Midas Zhou
# 一个Makefile文件通常所包含的内容和步骤：
#	1. 指定路径  VPATH
#	2. 源文件和目标文件列表，如：
#		SRC_FILES =
#		OBJ_FILES =
#		APP_FILES =
#		DEP_FILES = 头文件依赖
#       3.所需的编译标志，如：
#		CFLAGS =
#		LDFLAGS =
#	4.指定默认生成目标，如:
#		all: app1 app2
#	    4.1 手动设置特定目标生成规则
#	5.目标文件，库文件，应用程序的生成规则，如：
#		%.o:%.c
#			gcc -c .........
#		libapp.a:$(LIBOBJ)
#			ar ........
#	6.清理方法
#	7.所包含头文件的依赖和跟踪
#	8.安装方法
#
#
#                                ----- 自动依赖跟踪例子 -----
#
# 1. make会从头到尾一次次分析Makefile（从树根捋到每个树叉），每分析完一遍确定一个可以解决的目标或未知量，
#  接着就运行解出这个目标或未知量。
#  这样一次次直到最终目标生成。因此会看到$(warning...)内容被调用了好几次。
#
# 2. %.dep:%.c 这样的模板应该是按照最近原则进行匹配的。不然 %:%.c 这个模板可被所有情况引用了！！！
#
# 3. Makefile中应用shell命令需要注意：
#	3.1 每个命令需要用“;”分割，并且每一此shell命令从头到尾必须用”\“连接符号。
#	3.2 shell中的$var在Makefile中必须用$$var
#
#---------------------------------------------------------------------------------------------------------------

export STAGING_DIR=/home/midas-zhou/openwrt_widora/staging_dir
COMMON_USRDIR=/home/midas-zhou/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/

CC= $(STAGING_DIR)/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc

#### ----- 产生文件列表 ------
SRC_FILES = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRC_FILES))

##--- exclude some objs !!!BEWARE, :NO SPACE ----
OBJS := $(OBJS:test_color.o=)
OBJS := $(OBJS:test_sym.o=)
OBJS := $(OBJS:test_geom.o=)
OBJS := $(OBJS:test_stock.o=)
OBJS := $(OBJS:test_bigiot.o=)
OBJS := $(OBJS:tmp_app.o=)
OBJS := $(OBJS:test_bmpjpg.o=)
OBJS := $(OBJS:test_weather.o=)

DEP_FILES = $(patsubst %.c,%.dep,$(SRC_FILES))

#APPS  = test_stock
#APPS = test_bmpjpg
#APPS = test_geom
#APPS = test_sym
#APPS = tmp_app
#APPS = test_bigiot
#APPS = test_weather

#buttons.o
EXTRA_OBJS = iot/egi_iotclient.o utils/egi_utils.o utils/egi_iwinfo.o utils/egi_fifo.o \
	     utils/egi_filo.o utils/egi_cstring.o \
	     ffmpeg/ff_pcm.o

#### -----  编译标志 -----
## -Wall for __attribut__ // -I$(COMMON_USRDIR)/include (for jpeg)
CFLAGS = -I./ -I./iot -I./ffmpeg -I./utils -I$(COMMON_USRDIR)/include -I$(COMMON_USRDIR)/include/json-c
CFLAGS += -Wall -O2
#CFLAGS += -I./ -I./include
#### --- for debug, put before LDFLAGS!!!! ----
#CFLAGS += -O -g -DEGI_DEBUG

LDFLAGS += -L./lib -L$(COMMON_USRDIR)/lib
LIBS += -ljpeg -lm -lpthread -ljson-c -lasound

#----- 打印依赖文件  include $(DEP_FILES)
#-----如果前面有include命令，那么make命令只会执行下面all中的第一个依赖文件，除非用make all完整命令
#-----把 include &(DEP_FILES) 防到后面就 OK了。
#----在Makefile中$$var才相当于shell中的$var
$(warning "----- all:$(APPS) -----")
all:  $(APPS)
	@for var in $(APPS);\
	do \
		cat $$var.dep; \
	done

$(warning "----- %:%.c -----")


#### ------ 手动设置特定目标生成规则  ------
####----- !!!! manually edit here !!!! -----

egi_lib:
	$(CC) -shared -fPIC -Wl,-soname,libegi.so.1 $(OBJS)  $(EXTRA_OBJS)  -o libegi.so.1.0.0


$(APPS): $(OBJS) $(EXTRA_OBJS)  #buttons.o egi_iotclient.o ###touch.o spi.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS) $(EXTRA_OBJS) -o $(APPS)


#### ----- 目标文件自动生成规则 -----
%:%.c $(DEP_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $@.c


#### ----- 清除目标 -----
clean:
	rm -rf test_*.o $(OBJS) $(EXTRA_OBJS) $(APPS) $(DEP_FILES)


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
