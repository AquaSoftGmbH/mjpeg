#######################################################
#          Makefile for Linux Video Studio            #
#######################################################

INCLUDES = -I../yuv4mpeg \
	-Wunused -Wmissing-declarations -Wmissing-prototypes -Wall

bin_PROGRAMS = blend.flt wipe.flt overlay.flt foreveryuv

blend_flt_SOURCES = blend.flt.c
blend_flt_LDADD = -L$(top_srcdir)/yuv4mpeg -lyuv4mpeg
wipe_flt_SOURCES = wipe.flt.c
wipe_flt_LDADD = -L$(top_srcdir)/yuv4mpeg -lyuv4mpeg
overlay_flt_SOURCES = overlay.flt.c
overlay_flt_LDADD = -L$(top_srcdir)/yuv4mpeg -lyuv4mpeg
foreveryuv_SOURCES = foreveryuv
