#################################################
# some magic for using linux kernel settings
# when compiling module(s)

EXTRA_CFLAGS = -DEXPORT_SYMTAB # -save-temps
CURRENT = $(shell uname -r)
KERNEL_LOCATION = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
DEST = /lib/modules/$(CURRENT)/kernel/drivers/media/video

MAJ=$(shell grep "\#define MAJOR_VERSION" zoran.h | cut -f3 -d " " | cut -f1)
MIN=$(shell grep "\#define MINOR_VERSION" zoran.h | cut -f3 -d " " | cut -f1)
REL=$(shell grep "\#define RELEASE_VERSION" zoran.h | cut -f3 -d " " | cut -f1)
DATE=$(shell date +%Y%m%d)
BASE_DIR=driver-zoran
REL_DIR=$(BASE_DIR)-$(MAJ).$(MIN).$(REL)
SNP_DIR=$(BASE_DIR)-$(DATE)

here:
	@echo "Making Unified Zoran (zr360x7) driver for $(CURRENT) kernel..."
	@(cd $(KERNEL_LOCATION) && make -C $(KERNEL_LOCATION) SUBDIRS=$(PWD) modules)

obj-m      := zoran.o \
		saa7111.o saa7110.o saa7185.o saa7114.o \
		adv7175.o adv7170.o \
		bt819.o bt856.o \
		vpx3220.o \
		zr36060.o zr36050.o zr36016.o \
		videocodec.o
zoran-objs := zoran_procfs.o zoran_device.o zoran_driver.o zoran_card.o

zoran.o: $(zoran-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(zoran-objs)

ifneq (,$(findstring 2.4.,$(CURRENT)))
install:
	su -c "cp -v $(obj-m) $(DEST) && /sbin/depmod -ae -F $(KERNEL_LOCATION)/System.map $(CURRENT)"
else
install:
	su -c "cp -v *.ko $(DEST) && /sbin/depmod -ae -F $(KERNEL_LOCATION)/System.map $(CURRENT)"
endif

clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c

release: clean
	mkdir $(REL_DIR)
	cp *.[ch] FAQ Makefile ChangeLog update $(REL_DIR)/
	tar -zcf $(REL_DIR).tar.gz $(REL_DIR)
	rm -fr $(REL_DIR)

snapshot: clean
	mkdir $(SNP_DIR)
	cp *.[ch] FAQ Makefile ChangeLog update $(SNP_DIR)/
	tar -zcf $(SNP_DIR).tar.gz $(SNP_DIR)
	rm -fr $(SNP_DIR)

-include $(KERNEL_LOCATION)/Rules.make
