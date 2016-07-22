# *************************************************************
# *	@Hardware: TQ2416	
# *	@software: ubuntu-12.04
# *	@author	 : <mingllu@163.com>
# *	@date    : 2015-07-09
# *	@filename: led.S
# **************************************************************

BINANME = odf_china_Unicom

DIR_INC_USR = ./include

DIR_SRC_ENV = ./env
DIR_SRC_ORDERS = ./lib_orders
DIR_SRC_SOFTUPDATE = ./dfu_update

CFLAGS += -I$(DIR_INC_USR)

CROSS_COMPILE = arm-linux-
CC 		= $(CROSS_COMPILE)gcc
LD 		= $(CROSS_COMPILE)ld
AR 		= $(CROSS_COMPILE)ar
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
STRIP	= $(CROSS_COMPILE)strip
READELF = $(CROSS_COMPILE)readelf

SRC_C = $(wildcard *.c) $(wildcard $(DIR_SRC_ORDERS)/*.c) $(wildcard $(DIR_SRC_SOFTUPDATE)/*.c) $(wildcard $(DIR_SRC_ENV)/*.c)
OBJ_C = $(patsubst %.c,%.o,$(SRC_C))

OBJ_ALL = $(OBJ_C)

.PHONY:all 
all:$(OBJ_ALL)
	$(CC) -o $(BINANME) $(OBJ_ALL) -lpthread
	rm -rf *.o *.elf  $(DIR_SRC_ORDERS)/*.o $(DIR_SRC_SOFTUPDATE)/*.o $(DIR_SRC_ENV)/*.o
	cp -a $(BINANME) ~/prj-odf/nfs/rootfs/etc/rc.d/init.d/
	cp -a $(BINANME) /mnt/hgfs/share/$(BINANME).bin

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@  $<

clean:
	rm -rf *.elf *.o $(BINANME) $(DIR_SRC_ORDERS)/*.o $(DIR_SRC_SOFTUPDATE)/*.o $(DIR_SRC_ENV)/*.o
	rm -rf ~/prj-odf/nfs/rootfs/etc/rc.d/init.d/$(BINANME)
	rm -rf /mnt/hgfs/share/$(BINANME).bin
