# Process this file with Automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)

noinst_LIBRARIES = libutils.a libmotion.a

libmotion_MMXSSE =  mblock_sumsq_mmx.s mblock_bsumsq_mmx.s mblock_bsad_mmx.s \
                    mblock_sad_mmxe.s mblock_sad_mmx.s  \
	                mblock_sub44_sads_x86.c attributes.h

libutils_a_SOURCES = mjpeg_logging.c cpu_accel.c \
	                 mjpeg_logging.h mjpeg_types.h cpu_accel.h \
					 yuv4mpeg.c yuv4mpeg.h mpegconsts.c mpegconsts.h

if HAVE_X86CPU
libmotion_SIMD = $(libmotion_MMXSSE)
else
mpeg2enc_SIMD = 
endif

libmotion_a_SOURCES = motionsearch.c $(libmotion_SIMD) mmx.h motionsearch.h fastintfns.h

EXTRA_DIST = format_codes.h mblock_sub44_sads_x86_h.c .cvsignore

MAINTAINERCLEANFILES = Makefile.in


ASM = nasm


.s.o:
	$(ASM) -o $@ -f elf $<
