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

EXTRA_DIST = cpuinfo.c

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

noinst_LTLIBRARIES = libmotion.la libcpuaccel.la

lib_LTLIBRARIES = libmjpegutils.la

libmjpegutils_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic

libmjpegutils_la_SOURCES = \
	mjpeg_logging.c \
	mpegconsts.c \
	mpegtimecode.c \
	yuv4mpeg.c \
	yuv4mpeg_ratio.c

noinst_HEADERS = \
	attributes.h \
	cpu_accel.h \
	fastintfns.h \
	mmx.h \
	motionsearch.h \
	videodev_mjpeg.h \
	yuv4mpeg_intern.h

if HAVE_ALTIVEC
altivec_lib = $(top_builddir)/utils/altivec/libaltivec.la
endif
if HAVE_ASM_NASM
if HAVE_ASM_MMX
mmxsse_lib = $(top_builddir)/utils/mmxsse/libmmxsse.la
endif
endif

pkginclude_HEADERS = \
	format_codes.h \
	mjpeg_logging.h \
	mjpeg_types.h \
	mpegconsts.h \
	mpegtimecode.h \
	yuv4mpeg.h

libmotion_la_SOURCES = \
	motionsearch.c

libcpuaccel_la_SOURCES = \
	cpu_accel.c

libcpuaccel_la_LIBADD = $(mmxsse_lib) $(altivec_lib)

MAINTAINERCLEANFILES = Makefile.in
