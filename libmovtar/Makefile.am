EXTRA_PROGRAMS = movtar_play
bin_PROGRAMS = @MOVTAR_PLAY@ movtar_split movtar_unify movtar_index movtar_setinfo movtar_yuv422
bin_SCRIPTS = movtar-config
lib_LIBRARIES = libmovtar.a
libmovtar_a_SOURCES = movtar.c tar.c
INCLUDES = @GLIB_CFLAGS@ -I ../jpeg-mmx
include_HEADERS = movtar.h

movtar_play_LDFLAGS = -lSDL -lpthread -ljpeg
movtar_unify_LDFLAGS = -ljpeg
movtar_yuv422_LDFLAGS = -ljpeg
LDFLAGS = @GLIB_LIBS@

LDADD = ./libmovtar.a

EXTRA_DIST = README.movtar.implement movtar.m4

m4datadir = $(datadir)/aclocal
m4data_DATA = movtar.m4



