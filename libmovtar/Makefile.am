
SUBDIRS=rtjpeg

EXTRA_PROGRAMS = movtar_play
bin_PROGRAMS = @MOVTAR_PLAY@ movtar_split movtar_unify movtar_index movtar_setinfo movtar_yuv422
bin_SCRIPTS = movtar-config
lib_LIBRARIES = libmovtar.a
libmovtar_a_SOURCES = movtar.c tar.c 
INCLUDES = @GLIB_CFLAGS@ @JPEG_INCLUDE@ @RTJPEG_INCLUDE@ @SDL_CFLAGS@
include_HEADERS = movtar.h

movtar_play_LDADD = @SDL_LIBS@ -lpthread -ljpeg ./libmovtar.a @RTJPEG_OBJS@ @GLIB_LIBS@

movtar_unify_LDADD = ./libmovtar.a -ljpeg @RTJPEG_OBJS@ @GLIB_LIBS@
movtar_unify_COMPILE = -DRTJPEG

movtar_yuv422_LDADD = -ljpeg

LDADD = ./libmovtar.a @GLIB_LIBS@

EXTRA_DIST = README.movtar.implement movtar.m4

m4datadir = @M4DATADIR@
m4data_DATA = movtar.m4



