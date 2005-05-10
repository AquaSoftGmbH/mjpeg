# Makefile.am for yuvdeinterlace

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

LIBMJPEGUTILS = $(top_builddir)/utils/libmjpegutils.la

AM_CFLAGS = -O3 -funroll-all-loops -ffast-math

bin_PROGRAMS = yuvdenoise

yuvdenoise_SOURCES = main.c 

yuvdenoise_LDADD = $(LIBMJPEGUTILS)
