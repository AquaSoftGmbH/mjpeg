
bin_PROGRAMS = aenc
aenc_SOURCES = musicin.c common.c encode.c subs.c psy.c tonal.c tables.c
aenc_LDFLAGS = -ljpeg
aenc_LDADD = ../editlist.o ../lav_io.o ../avilib.o @MOVTAR_LIBS@

INCLUDES =  -I ..
CFLAGS =  -O6 -ffast-math @MTYPEFLAGS@ @MOVTAR_CFLAGS@

