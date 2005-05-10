# Makefile.am for yuvdeinterlace

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

LIBMJPEGUTILS = $(top_builddir)/utils/libmjpegutils.la

bin_PROGRAMS = yuvdeinterlace

noinst_HEADERS = \
	blend_fields.h \
	sinc_interpolation.h \
	transform_block.h \
    motionsearch_deint.h \
    vector.h
    
yuvdeinterlace_SOURCES = blend_fields.c \
	main.c \
	sinc_interpolation.c \
	transform_block.c \
    motionsearch_deint.c

yuvdeinterlace_LDADD = $(LIBMJPEGUTILS)
