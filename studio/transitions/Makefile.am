#######################################################
#          Makefile for Linux Video Studio            #
#######################################################

INCLUDES = -I$(top_srcdir)/yuv4mpeg ${MJPEGTOOLS_CFLAGS} \
	-Wunused -Wmissing-declarations -Wmissing-prototypes -Wall

noinst_HEADERS = wave_io.h

bin_PROGRAMS = blend.flt wipe.flt overlay.flt fade.flt fade_s.flt

bin_SCRIPTS = forevery4m2
EXTRA_DIST = ${bin_SCRIPTS}

blend_flt_SOURCES = blend.flt.c
blend_flt_LDADD = $(top_builddir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
wipe_flt_SOURCES = wipe.flt.c
wipe_flt_LDADD = $(top_builddir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
overlay_flt_SOURCES = overlay.flt.c
overlay_flt_LDADD = $(top_builddir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
fade_flt_SOURCES = fade.flt.c
fade_flt_LDADD = $(top_builddir)/yuv4mpeg/libyuv4mpeg.a \
       ${MJPEGTOOLS_LIBS} -lm
fade_s_flt_SOURCES = fade_s.flt.c wave_io.c
fade_s_flt_LDADD = $(top_builddir)/yuv4mpeg/libyuv4mpeg.a \
       ${MJPEGTOOLS_LIBS} -lm

