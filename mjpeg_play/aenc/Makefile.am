
bin_PROGRAMS = aenc
aenc_SOURCES = musicin.c common.c encode.c subs.c psy.c tonal.c tables.c
aenc_LDFLAGS = -L ../movtar 
aenc_LDADD = ../editlist.o ../lav_io.o ../avilib.o -lmovtar

INCLUDES =  -I ..
CFLAGS =  -O6 -ffast-math @MTYPEFLAGS@

