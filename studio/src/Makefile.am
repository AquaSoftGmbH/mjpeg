#######################################################
#          Makefile for Linux Video Studio            #
#######################################################

INCLUDES = ${GTK_CFLAGS} ${GDK_PIXBUF_CFLAGS} ${GLIB_CFLAGS} \
	${X_CFLAGS} -I../pixmaps -I../yuv4mpeg \
	-Wunused -Wmissing-declarations -Wmissing-prototypes -Wall
OBJS= studio.c config.c parseconfig.c gtktvplug.c lavrec_pipe.c \
	lavplay_pipe.c gtkimageplug.c lavedit.c mixer.c lavencode.c \
	pipes.c gtkenhancedscale.c lavedit_trimming.c gtkfunctions.c \
	config_encode.c lavedit_effects.c

bin_PROGRAMS = studio

studio_headers = gtktvplug.h gtkimageplug.h studio.h parseconfig.h \
	mixer.h gtkfunctions.h yuv4mpeg.h

studio_SOURCES = ${studio_headers} ${OBJS}
studio_LDADD = ${X_LIBS} -lXv -L$(top_srcdir)/yuv4mpeg -lyuv4mpeg
studio_LDFLAGS = ${GTK_LIBS} ${GLIB_LIBS} ${GDK_PIXBUF_LIBS}
