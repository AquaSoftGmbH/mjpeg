EXTRA_DIST = README.2-3pulldown .cvsignore
MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = yuvycsnoise yuvkineco
noinst_LIBRARIES = libyuvfilters.a

AM_CPPFLAGS = -I$(top_srcdir)/utils
MJPEGLIB = $(top_builddir)/utils/libmjpegutils.a

libyuvfilters_a_SOURCES = \
	yuvfilters.h \
	addtask.c alloctask.c initframe.c putframe.c yuvstdin.c yuvstdout.c \
	yuvycsnoise.c yuvkineco.c

yuvkineco_SOURCES = main.c
yuvkineco_CFLAGS = -DFILTER=yuvkineco
yuvkineco_LDADD = libyuvfilters.a $(MJPEGLIB) @LIBDL_LIB@

yuvycsnoise_SOURCES = main.c
yuvycsnoise_CFLAGS = -DFILTER=yuvycsnoise
yuvycsnoise_LDADD = libyuvfilters.a $(MJPEGLIB) @LIBDL_LIB@
