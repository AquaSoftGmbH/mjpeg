# Makefile.am for yuvdenoise (lavtools)

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir)/utils -I $(top_srcdir)

yuvdenoise_CFLAGS=@PROGRAM_NOPIC@

bin_PROGRAMS = yuvdenoise

yuvdenoise_SOURCES = \
	deinterlace.c \
	denoise.c \
	main.c \
	motion.c

noinst_HEADERS = \
	deinterlace.h \
	denoise.h \
	global.h \
	motion.h

yuvdenoise_LDADD = \
	$(top_builddir)/utils/libcpuaccel.la \
	$(top_builddir)/utils/libmjpegutils.la \
	@LIBM_LIBS@
