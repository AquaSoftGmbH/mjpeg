############### BCBASE

CFLAGS = -O6 -I../jpeg-6b-mmx

OBJS = \
	atom.o \
	codecs.o \
	ctab.o \
	dinf.o \
	dref.o \
	dv.o \
	edts.o \
	elst.o \
	hdlr.o \
	ima4.o \
	jdatasrc.o \
	jdatadst.o \
	jpeg.o \
	matrix.o \
	mdat.o \
	mdhd.o \
	mdia.o \
	minf.o \
	moov.o \
	mvhd.o \
	qtpng.o \
	quicktime.o \
	raw.o \
	rawaudio.o \
	smhd.o \
	stbl.o \
	stco.o \
	stsc.o \
	stsd.o \
	stsdtable.o \
	stss.o \
	stsz.o \
	stts.o \
	tkhd.o \
	trak.o \
	twos.o \
	udta.o \
	ulaw.o \
	util.o \
	vmhd.o \
	wmx1.o \
	wmx2.o \
	yuv.o \
	yuv2.o \
	yuv4.o

OUTPUT = libquicktime.a

CC = gcc



$(OUTPUT): $(OBJS)
	ar rc $(OUTPUT) $(OBJS)
	ranlib $(OUTPUT)

dump: dump.o $(OUTPUT)
	$(CC) -o dump dump.o $(OUTPUT) -ljpeg -lpng -lz -lpthread

make_streamable: make_streamable.o $(OUTPUT)
	$(CC) -o make_streamable make_streamable.o $(OUTPUT) -ljpeg -lpng -lz -lpthread

dechunk: dechunk.o $(OUTPUT)
	$(CC) -o dechunk dechunk.o $(OUTPUT) -ljpeg -lpng -lz -lpthread

qtinfo: qtinfo.o
	$(CC) -o qtinfo qtinfo.o $(OUTPUT) -ljpeg -lpng -lz -lpthread

util:  dump make_streamable qtinfo dechunk
	

quicktime.o: quicktime.h  quicktime.c

clean:
	rm -f core
	rm -f *.o
	rm -f $(OUTPUT)
	rm -f dump make_streamable a.out qtinfo dechunk
	rm -rf .deps

wc:
	cat *.c *.h | wc

install:
