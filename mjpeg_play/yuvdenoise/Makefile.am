# Makefile.am for yuvdenoise (lavtools)

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir)/utils -I $(top_srcdir)

# (Ronald) do we have to do this?
AM_CFLAGS = -funroll-all-loops -finline-functions \
	-ffast-math -frerun-cse-after-loop -frerun-loop-opt \
	-fexpensive-optimizations -fmove-all-movables

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
	$(top_builddir)/utils/libmjpegutils.a \
	@LIBM_LIBS@
