# Makefile.am for yuvdeinterlace

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

LIBMJPEGUTILS = $(top_builddir)/utils/libmjpegutils.la

AM_CFLAGS = -O3 -funroll-all-loops -ffast-math

bin_PROGRAMS = yuvdenoise

#noinst_HEADERS = \
#    main_filter.h \
#    post_filter.h \
#    vector.h
    
yuvdenoise_SOURCES = \
    main.c 

yuvdenoise_LDADD = \
	$(top_builddir)/utils/libmotion.la \
	$(top_builddir)/utils/libcpuaccel.la \
	$(LIBMJPEGUTILS)
