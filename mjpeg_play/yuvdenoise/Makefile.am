# Makefile.am for yuvdenoise (lavtools)

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in
INCLUDES = -I $(top_srcdir)/utils -I $(top_srcdir)
AM_CFLAGS = -funroll-all-loops -finline-functions -ffast-math -frerun-cse-after-loop -frerun-loop-opt -fexpensive-optimizations -fmove-all-movables -Werror

bin_PROGRAMS       = yuvdenoise
yuvdenoise_SOURCES = yuvdenoise.c 
yuvdenoise_LDADD = -L$(top_builddir)/utils -lmjpegutils @LIBM_LIBS@
