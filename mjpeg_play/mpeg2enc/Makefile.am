#
# Automake Makefile template for mpeg2enc...

bin_PROGRAMS = mpeg2enc 

mpeg2enc_MMXSSE = \
    fdct_mmx.s fdctdata.c idct_mmx.s idctdata.c \
    quant_mmx.s dist2_mmx.s bdist2_mmx.s\
	bdist1_mmx.s predict_mmx.s \
	mblockq_sad_mmxe.s mblock_sad_mmxe.s mblock_sad_mmx.s  \
    mblock_sub44_sads.c \
	quantize_x86.c  \
    predcomp_mmxe.s predcomp_mmx.s

EXTRA_DIST = \
    fdct_mmx.o idct_mmx.o quant_mmx.o dist2_mmx.o bdist2_mmx.o\
	bdist1_mmx.o predict_mmx.o \
	mblockq_sad_mmxe.o mblock_sad_mmxe.o mblock_sad_mmx.o \
    mblock_sub44_sads_x86_h.c \
	predcomp_mmxe.o predcomp_mmx.o

if HAVE_X86CPU
mpeg2enc_SIMD = $(mpeg2enc_MMXSSE)
else
mpeg2enc_SIMD = 
endif

mpeg2enc_SOURCES = mpeg2enc.c conform.c putseq.c putpic.c puthdr.c putmpg.c \
	 putvlc.c putbits.c motion.c predict.c readpic.c writepic.c transfrm.c \
	fdctref.c idct.c quantize.c ratectl.c stats.c cpu_accel.c  \
	fastintfns.h  global.h mpeg2enc.h vlc.h cpu_accel.h mmx.h attributes.h \
	simd.h $(mpeg2enc_SIMD)


mpeg2enc_LDADD=-lm

ASM = nasm


.s.o:
	$(ASM) -o $@ -f elf $<


