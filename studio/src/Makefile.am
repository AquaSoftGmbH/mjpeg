#######################################################
#          Makefile for Linux Video Studio            #
#######################################################

INCLUDES = ${GTK_CFLAGS} ${GDK_PIXBUF_CFLAGS} ${GLIB_CFLAGS} \
	${X_CFLAGS} ${MJPEGTOOLS_CFLAGS} \
	-I$(top_srcdir)/pixmaps -I$(top_srcdir)/yuv4mpeg \
	-Wunused -Wmissing-declarations -Wmissing-prototypes -Wall
OBJS = studio.c config.c parseconfig.c gtktvplug.c lavrec_pipe.c \
	lavplay_pipe.c gtkimageplug.c lavedit.c mixer.c lavencode.c \
	pipes.c lavedit_trimming.c gtkfunctions.c \
	config_encode.c lavedit_effects.c lavencode_mpeg.c gtkscenelist.c \
        lavencode_distributed.c lavencode_script.c lavencode_util.c \
	gtkenhancedscale.c

bin_PROGRAMS = studio stv

noinst_HEADERS = gtktvplug.h gtkimageplug.h studio.h parseconfig.h \
	mixer.h gtkfunctions.h gtkscenelist.h gdkpixbuf_write.h \
	pipes.h channels.h lavedit.h lirc.h
#	gtkenhancedscale.h

studio_SOURCES = ${OBJS}
studio_LDADD = ${X_LIBS} -lXv \
	$(top_builddir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
studio_LDFLAGS = ${GTK_LIBS} ${GLIB_LIBS} ${GDK_PIXBUF_LIBS} ${PTHREAD_LIBS}

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
MIXER_OBJECTS = mixer.c
else
MIXER_OBJECTS =
endif

stv_SOURCES = studio-tv.c gtktvplug.c gtkfunctions.c \
	parseconfig.c gdkpixbuf_write.c ${MIXER_OBJECTS} lirc.c \
	lirc.h channels.c
stv_LDADD = ${X_LIBS} -lXv ${PNG_LIBS} ${JPEG_LIBS}
stv_LDFLAGS = ${GTK_LIBS} ${GLIB_LIBS} ${GDK_PIXBUF_LIBS}
