#
# Automake Makefile template for mpeg2enc...

bin_PROGRAMS = mpeg2enc 

mpeg2enc_MMXSSE = \
    fdist1_mmx.s dist1_00_mmx.s dist1_01_mmx.s dist1_10_mmx.s dist1_11_mmx.s \
    fdct_mmx.s fdctdata.c idct_mmx.s idctdata.c \
    qdist1_mmx.s quant_mmx.s dist2_mmx.s bdist2_mmx.s\
	bdist1_mmx.s predict_mmx.s \
	fdist1_00.s dist1_00.s dist1_01.s dist1_10.s dist1_11.s \
    qdist1_sse.s quantize_x86.c  qblock_sad_x86.c  \
    predcomp_mmxe.s predcomp_mmx.s

EXTRA_DIST = \
    fdist1_mmx.o dist1_00_mmx.o dist1_01_mmx.o dist1_10_mmx.o dist1_11_mmx.o \
    fdct_mmx.o idct_mmx.o  qdist1_mmx.o quant_mmx.o dist2_mmx.o bdist2_mmx.o\
	bdist1_mmx.o predict_mmx.o \
	fdist1_00.o dist1_00.o dist1_01.o dist1_10.o dist1_11.o \
    qdist1_sse.o predcomp_mmxe.o qblock_sad_x86_h.c

if X86CPU
mpeg2enc_SIMD = $(mpeg2enc_MMXSSE)
else
mpeg2enc_SIMD = 
endif

mpeg2enc_SOURCES = mpeg2enc.c conform.c putseq.c putpic.c puthdr.c putmpg.c \
	 putvlc.c putbits.c motion.c predict.c readpic.c writepic.c transfrm.c \
	fdctref.c idct.c quantize.c ratectl.c stats.c cpu_accel.c  \
	fastintfns.h  global.h mpeg2enc.h vlc.h cpu_accel.h mmx.h attributes.h \
	simd.h $(mpeg2enc_SIMD)



INCLUDES = 
CPPFLAGS = @MTYPEFLAGS@
CFLAGS =  -O6 -Werror -Wall -Wno-uninitialized
ASM = nasm


.s.o:
	$(ASM) -o $@ -f elf $<


