EXTRA_DIST = README.2-3pulldown

MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = yuvycsnoise yuvkineco

noinst_LIBRARIES = libyuvfilters.a

AM_CPPFLAGS = -I$(top_srcdir)/utils
MJPEGLIB = $(top_builddir)/utils/libmjpegutils.a

libyuvfilters_a_SOURCES = \
	addtask.c \
	alloctask.c \
	initframe.c \
	putframe.c \
	yuvkineco.c \
	yuvstdin.c \
	yuvstdout.c \
	yuvycsnoise.c

noinst_HEADERS = \
	yuvfilters.h

yuvkineco_SOURCES = main.c
yuvkineco_CFLAGS = -DFILTER=yuvkineco
yuvkineco_LDADD = libyuvfilters.a $(MJPEGLIB) @LIBDL_LIB@

yuvycsnoise_SOURCES = main.c
yuvycsnoise_CFLAGS = -DFILTER=yuvycsnoise
yuvycsnoise_LDADD = libyuvfilters.a $(MJPEGLIB) @LIBDL_LIB@
