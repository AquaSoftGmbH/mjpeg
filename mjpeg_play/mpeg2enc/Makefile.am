#
# Automake Makefile template for mpeg2enc...

bin_PROGRAMS = mpeg2enc 

mpeg2enc_MMXSSE = \
    fdct_mmx.s fdctdata.c idct_mmx.s idctdata.c \
    quant_mmx.s quantize_x86.c predict_mmx.s \
    predcomp_mmxe.s predcomp_mmx.s


EXTRA_DIST = mblock_sub44_sads_x86_h.c NOTES .cvsignore


MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

if HAVE_X86CPU
mpeg2enc_SIMD = $(mpeg2enc_MMXSSE)
libmotion_SIMD = $(libmotion_MMXSSE)
else
mpeg2enc_SIMD = 
libmotion_SIMD = 
endif

mpeg2enc_SOURCES = mpeg2enc.c conform.c putseq.c putpic.c puthdr.c putmpg.c \
	 putvlc.c putbits.c motionest.c predict.c readpic.c writepic.c transfrm.c \
	fdctref.c idct.c quantize.c ratectl.c stats.c synchrolib.c \
	fastintfns.h  global.h mpeg2enc.h  vlc.h mmx.h attributes.h \
	simd.h synchrolib.h $(mpeg2enc_SIMD)

mpeg2enc_DEPENDENCIES = $(top_srcdir)/utils/libutils.a $(top_srcdir)/utils/libmotion.a

mpeg2enc_LDADD=-L$(top_srcdir)/utils -lmotion -lutils @PTHREAD_LIBS@ -lm


ASM = nasm


.s.o:
	$(ASM) -o $@ -f elf $<
