# Makefile for mpeg2enc

# Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

#
# Disclaimer of Warranty
#
# These software programs are available to the user without any license fee or
# royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
# any and all warranties, whether express, implied, or statuary, including any
# implied warranties or merchantability or of fitness for a particular
# purpose.  In no event shall the copyright-holder be liable for any
# incidental, punitive, or consequential damages of any kind whatsoever
# arising from the use of these programs.
#
# This disclaimer of warranty extends to the user of these programs and user's
# customers, employees, agents, transferees, successors, and assigns.
#
# The MPEG Software Simulation Group does not represent or warrant that the
# programs furnished hereunder are free of infringement of any third-party
# patents.
#
# Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
# are subject to royalty fees to patent holders.  Many of these patents are
# general enough such that they are unavoidable regardless of implementation
# design.
#
#

# See INSTALL for explanation of the SSE /MMX  options
# You should set the -m flag in CFLAGS appropriately for your
# CPU.  The -m486 setting is *not* optimal for more modern x86 CPU's/

#
# GNU gcc
#
CC = gcc
SSEFLAGS=-DSSE
#MMXFLAGS=-DMMX

CFLAGS = -pg -O6 $(SSEFLAGS) $(MMXFLAGS) -mpentiumpro -Wall -I .. -I ../jpeg-6b 
QT_LIBS= ../quicktime/quicktime/libquicktime.a ../jpeg-6b-mmx/libjpeg.a -lpthread -lpng -lz
#QT_LIBS= ../jpeg-6b/libjpeg.a -lpng -lz

#MMXOBJ = fdist1_mmx.o dist1_00_mmx.o dist1_01_mmx.o dist1_10_mmx.o dist1_11_mmx.o fdct_mmx.o fdctdata.o qdist1_mmx.o
SSEOBJ = fdist1_00.o dist1_00.o dist1_01.o dist1_10.o dist1_11.o fdct_mmx.o \
         fdctdata.o idct_mmx.o idctdata.o qdist1_sse.o qdist1_mmx.o quant_non_i_sse.o

COBJ = mpeg2enc.o conform.o putseq.o putpic.o puthdr.o putmpg.o putvlc.o putbits.o motion.o predict.o readpic.o writepic.o transfrm.o fdctref.o idct.o quantize.o ratectl.o stats.o jpegutils.o

OBJ = $(COBJ) $(MMXOBJ) $(SSEOBJ)

all: mpeg2enc

pc: mpeg2enc.exe

clean:
	rm -f $(COBJ)  *% core mpeg2enc

.s.o:
	nasm -o $@ -f elf $<

mpeg2enc.exe: mpeg2enc
	coff2exe mpeg2enc

mpeg2enc: $(OBJ)
	$(CC) $(CFLAGS) -o mpeg2enc $(OBJ) ../editlist.o ../lav_io.o ../avilib.o $(QT_LIBS) -lm

conform.o : conform.c config.h global.h mpeg2enc.h 
fdctref.o : fdctref.c config.h 
idct.o : idct.c config.h 
motion.o : motion.c config.h global.h mpeg2enc.h 
mpeg2enc.o : mpeg2enc.c config.h global.h mpeg2enc.h 
predict.o : predict.c config.h global.h mpeg2enc.h 
putbits.o : putbits.c config.h 
puthdr.o : puthdr.c config.h global.h mpeg2enc.h 
putmpg.o : putmpg.c config.h global.h mpeg2enc.h 
putpic.o : putpic.c config.h global.h mpeg2enc.h 
putseq.o : putseq.c config.h global.h mpeg2enc.h 
putvlc.o : putvlc.c config.h global.h mpeg2enc.h vlc.h 
quantize.o : quantize.c config.h global.h mpeg2enc.h 
ratectl.o : ratectl.c config.h global.h mpeg2enc.h 
readpic.o : readpic.c config.h global.h mpeg2enc.h 
stats.o : stats.c config.h global.h mpeg2enc.h 
transfrm.o : transfrm.c config.h global.h mpeg2enc.h 
writepic.o : writepic.c config.h global.h mpeg2enc.h 

dist1_00.o: dist1_00.s
	nasm -o dist1_00.o -f elf dist1_00.s
