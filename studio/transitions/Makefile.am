#######################################################
#          Makefile for Linux Video Studio            #
#######################################################

INCLUDES = -I../yuv4mpeg ${MJPEGTOOLS_CFLAGS} \
	-Wunused -Wmissing-declarations -Wmissing-prototypes -Wall

bin_PROGRAMS = blend.flt wipe.flt overlay.flt

bin_SCRIPTS = forevery4m2

blend_flt_SOURCES = blend.flt.c
blend_flt_LDADD = $(top_srcdir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
wipe_flt_SOURCES = wipe.flt.c
wipe_flt_LDADD = $(top_srcdir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
overlay_flt_SOURCES = overlay.flt.c
overlay_flt_LDADD = $(top_srcdir)/yuv4mpeg/libyuv4mpeg.a \
	${MJPEGTOOLS_LIBS}
