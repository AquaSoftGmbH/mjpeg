#######################################################
#          Makefile for Linux Video Studio            #
#######################################################

INCLUDES = ${GTK_CFLAGS} ${GDK_PIXBUF_CFLAGS} ${GLIB_CFLAGS} \
	${X_CFLAGS} -I../pixmaps -I../yuv4mpeg \
	-Wunused -Wmissing-declarations -Wmissing-prototypes -Wall
OBJS= studio.c config.c parseconfig.c gtktvplug.c lavrec_pipe.c \
	lavplay_pipe.c gtkimageplug.c lavedit.c mixer.c lavencode.c \
	pipes.c gtkenhancedscale.c lavedit_trimming.c gtkfunctions.c \
	config_encode.c lavedit_effects.c lavencode_mpeg.c gtkscenelist.c

bin_PROGRAMS = studio stv

studio_headers = gtktvplug.h gtkimageplug.h studio.h parseconfig.h \
	mixer.h gtkfunctions.h yuv4mpeg.h gtkscenelist.h \
	gdkpixbuf_write.h

studio_SOURCES = ${studio_headers} ${OBJS}
studio_LDADD = ${X_LIBS} -lXv -L$(top_srcdir)/yuv4mpeg -lyuv4mpeg
studio_LDFLAGS = ${GTK_LIBS} ${GLIB_LIBS} ${GDK_PIXBUF_LIBS}

if HAVE_PNG
PNG_LIBS = -lpng
else
PNG_LIBS =
endif

if HAVE_JPEG
JPEG_LIBS = -ljpeg
else
JPEG_LIBS =
endif

if OSS
MIXER_OBJECTS = mixer.c mixer.h
else
MIXER_OBJECTS =
endif

stv_SOURCES = studio-tv.c gtktvplug.c gtktvplug.h gtkfunctions.c \
	parseconfig.c gdkpixbuf_write.c ${MIXER_OBJECTS} lirc.c \
	lirc.h channels.c channels.h
stv_LDADD = ${X_LIBS} -lXv ${PNG_LIBS} ${JPEG_LIBS}
stv_LDFLAGS = ${GTK_LIBS} ${GLIB_LIBS} ${GDK_PIXBUF_LIBS}
