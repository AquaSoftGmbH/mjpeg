include ../global_config

OBJS = \
	jcapimin.o \
	jcapistd.o \
	jctrans.o \
	jcparam.o \
	jdatadst.o \
	jcinit.o \
	jcmaster.o \
	jcmarker.o \
	jcmainct.o \
	jcprepct.o \
	jccoefct.o \
	jccolor.o \
	jcsample.o \
	jchuff.o \
	jcphuff.o \
	jcdctmgr.o \
	jfdctfst.o \
	jfdctflt.o \
	jfdctint.o \
	jdapimin.o \
	jdapistd.o \
	jdtrans.o \
	jdatasrc.o \
	jdmaster.o \
	jdinput.o \
	jdmarker.o \
	jdhuff.o \
	jdphuff.o \
	jdmainct.o \
	jdcoefct.o \
	jdpostct.o \
	jddctmgr.o \
	jidctfst.o \
	jidctflt.o \
	jidctint.o \
	jidctred.o \
	jdsample.o \
	jdcolor.o \
	jquant1.o \
	jquant2.o \
	jdmerge.o \
	jcomapi.o \
	jutils.o \
	jerror.o \
	jmemmgr.o \
	jmemnobs.o


all: $(OBJS)

.c.o:
	$(CC) -c `./c_flags` $*.c

clean:
	rm -f *.o core *.a

install:
	install jpeglib.h /usr/local/include
	install jinclude.h /usr/local/include
	install jconfig.h /usr/local/include
	install jmorecfg.h /usr/local/include
	install jerror.h /usr/local/include
	install jpegint.h /usr/local/include
	install libjpeg.a /usr/local/lib
