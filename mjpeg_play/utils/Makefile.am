# Process this file with Automake to produce Makefile.in

if HAVE_ALTIVEC
SUBDIRS = altivec
endif
if HAVE_ASM_NASM
if HAVE_ASM_MMX
SUBDIRS = mmxsse
endif
endif

DIST_SUBDIRS = altivec mmxsse

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

noinst_LIBRARIES = libmotion.a libcpuaccel.a
lib_LIBRARIES = libmjpegutils.a

libmjpegutils_a_SOURCES = \
	mjpeg_logging.c \
	mpegconsts.c \
	mpegtimecode.c \
	yuv4mpeg.c \
	yuv4mpeg_ratio.c

noinst_HEADERS = \
	attributes.h \
	cpu_accel.h \
	fastintfns.h \
	format_codes.h \
	mmx.h \
	motionsearch.h \
	videodev_mjpeg.h \
	yuv4mpeg_intern.h

#if HAVE_ALTIVEC
#altivec_lib = $(top_builddir)/utils/altivec/libaltivec.a
#endif
#if HAVE_ASM_NASM
#if HAVE_ASM_MMX
#mmxsse_lib = $(top_builddir)/utils/mmxsse/libmmxsse.a
#endif
#endif

pkginclude_HEADERS = \
	mjpeg_logging.h \
	mjpeg_types.h \
	mpegconsts.h \
	mpegtimecode.h \
	yuv4mpeg.h

libmotion_a_SOURCES = \
	motionsearch.c

libcpuaccel_a_SOURCES = \
	cpu_accel.c

#libcpuaccel_a_LIBADD = $(mmxsse_lib) $(altivec_lib)

MAINTAINERCLEANFILES = Makefile.in

%.o: %.s ; 	$(AS) $(ASFLAGS) -o $@ $<
