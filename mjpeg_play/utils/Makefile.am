# Process this file with Automake to produce Makefile.in


if HAVE_ALTIVEC
SUBDIRS = altivec
endif
if HAVE_ASM_NASM
if HAVE_ASM_MMX
SUBDIRS = mmxsse
endif
endif

INCLUDES = -I$(top_srcdir)


noinst_LIBRARIES = libmotion.a
lib_LIBRARIES = libmjpegutils.a libmotion.a

libmjpegutils_a_SOURCES = mjpeg_logging.c cpu_accel.c \
                          mjpeg_logging.h mjpeg_types.h cpu_accel.h \
                          yuv4mpeg.c yuv4mpeg_ratio.c \
                          yuv4mpeg.h yuv4mpeg_intern.h \
                          mpegconsts.c mpegconsts.h \
                          mpegtimecode.c mpegtimecode.h

if HAVE_ALTIVEC
altivec_lib = altivec/*.o
endif
if HAVE_ASM_NASM
if HAVE_ASM_MMX
mmxsse_lib = mmxsse/*.o
endif
endif

libmjpegutils_a_LIBADD = $(mmxsse_lib) $(altivec_lib)

pkginclude_HEADERS = yuv4mpeg.h mpegconsts.h mjpeg_logging.h mjpeg_types.h mpegtimecode.h


libmotion_a_SOURCES = motionsearch.c motionsearch.h fastintfns.h 

EXTRA_DIST = videodev_mjpeg.h format_codes.h mmx.h attributes.h .cvsignore

MAINTAINERCLEANFILES = Makefile.in

%.o: %.s ; 	$(AS) $(ASFLAGS) -o $@ $<

