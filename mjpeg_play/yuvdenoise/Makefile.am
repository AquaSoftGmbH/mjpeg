# Makefile.am for yuvdenoise (lavtools)

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir)/utils

bin_PROGRAMS       = yuvdenoise
yuvdenoise_SOURCES = yuvdenoise.c 
yuvdenoise_LDADD = $(top_srcdir)/utils/mjpeg_logging.o $(top_srcdir)/utils/yuv4mpeg.o 
