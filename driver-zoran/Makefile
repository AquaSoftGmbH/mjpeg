#################################################
# config

# where the kernel sources are located
KERNEL_LOCATION=/usr/src/linux
TOPDIR=/usr/src/linux
#################################################
# some magic for using linux kernel settings
# when compiling module(s)

EXTRA_CFLAGS = -DEXPORT_SYMTAB # -save-temps
CURRENT = $(shell uname -r)

here:
	@echo Making Zoran 36060+36057/67 driver for $(CURRENT) kernel...
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

ifneq (,$(findstring 2.2.,$(CURRENT)))
# This part is for 2.2.x kernels

M_OBJS       = zoran.o saa7111.o saa7110.o saa7185.o adv7175.o bt819.o bt856.o

install:
	su -c "cp -v $(M_OBJS) /lib/modules/$(CURRENT)/misc;/sbin/depmod -a"

clean:
	-rm -f $(M_OBJS) .*.o.flags *~ *.bck tags *.avi

else
# This part is for 2.4.x kernels (but not for 2.4.0test series!!!)

obj-m       = zoran.o saa7111.o saa7110.o saa7185.o adv7175.o bt819.o bt856.o

install:
	su -c "cp -v $(obj-m) /lib/modules/$(CURRENT)/kernel/drivers/media/video/;/sbin/depmod -a"

clean:
	-rm -f $(obj-m) .*.o.flags *~ *.bck tags *.avi

endif


include $(KERNEL_LOCATION)/Rules.make

zoran.o:  zoran_procfs.c
