# Makefile.am for yuvdenoise (lavtools)

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in
INCLUDES = -I $(top_srcdir)/utils -I $(top_srcdir)
AM_CFLAGS = -funroll-all-loops

bin_PROGRAMS       = yuvdenoise
yuvdenoise_SOURCES = yuvdenoise.c 
yuvdenoise_LDADD = $(top_srcdir)/utils/mjpeg_logging.o $(top_srcdir)/utils/yuv4mpeg.o $(top_srcdir)/utils/cpu_accel.o 
