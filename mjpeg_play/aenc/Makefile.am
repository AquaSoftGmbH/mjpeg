# Process this file with automake to produce Makefile.in

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

bin_PROGRAMS = mp2enc
mp2enc_SOURCES = musicin.c common.c encode.c subs.c psy.c tonal.c \
	tables.c wav_io.c fft.c \
	common.h table_absthr.h table_cb.h  table_th.h \
	encoder.h  table_alloc.h   table_enwindow.h  wav_io.h

mp2enc_LDADD = -L$(top_builddir)/utils -lmjpegutils $(LIBM_LIBS)

