# Makefile.am for yuvdeinterlace

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

LIBMJPEGUTILS = $(top_builddir)/utils/libmjpegutils.la

bin_PROGRAMS = yuvdeinterlace

yuvdeinterlace_SOURCES = blend_fields.c \
	copy_frame.c \
	interleave.c \
	main.c \
	sinc_interpolation.c \
	transform_block.c

yuvdeinterlace_LDADD = \
	$(top_builddir)/utils/libmotion.la \
	$(top_builddir)/utils/libcpuaccel.la \
	$(LIBMJPEGUTILS)
