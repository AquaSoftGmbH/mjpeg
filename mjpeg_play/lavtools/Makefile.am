# Makefile for lavtools

MAINTAINERCLEANFILES = Makefile.in

INCLUDES =  -I$(top_srcdir) -I$(includedir) \
	-DG_LOG_DOMAIN=\"lavtools\" -DLAVPLAY_VERSION=\"$(VERSION)\" \
	$(LIBDV_CFLAGS) $(LIBQUICKTIME_CFLAGS) ${X_CFLAGS} \
        -I /usr/X11R6/include -I$(top_srcdir)/utils \
	${GTK_CFLAGS} $(JPEG_CFLAGS)

LIBMJPEGUTILS = $(top_builddir)/utils/libmjpegutils.la

# *********************************************************************
# Support lib

lib_LTLIBRARIES = liblavfile.la liblavjpeg.la

if HAVE_V4L
  lib_LTLIBRARIES += liblavrec.la
endif
if HAVE_V4L_OR_SDL
  lib_LTLIBRARIES += liblavplay.la
endif


liblavincludedir = $(pkgincludedir)
liblavinclude_HEADERS = \
	avilib.h \
	editlist.h \
	lav_io.h \
	jpegutils.h \
	frequencies.h \
	liblavrec.h \
	liblavplay.h \
	audiolib.h

LAV_ALL_LIB_OPTS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE)

liblavfile_la_SOURCES = editlist.c lav_io.c avilib.c
liblavfile_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) \
	$(LIBQUICKTIME_LIBS) $(JPEG_LIBS) $(LIBDV_LIBS)

liblavjpeg_la_SOURCES = jpegutils.c
liblavjpeg_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) $(JPEG_LIBS)

liblavrec_la_SOURCES = liblavrec.c audiolib.c frequencies.c
liblavrec_la_LDFLAGS = $(LAV_ALL_LIB_OPTS)
liblavrec_la_CFLAGS = $(AM_CFLAGS)

liblavplay_la_SOURCES = liblavplay.c audiolib.c
liblavplay_la_LDFLAGS = $(LAV_ALL_LIB_OPTS)
liblavplay_la_CFLAGS = $(AM_CFLAGS)

# *********************************************************************
# The tools themselves

bin_PROGRAMS = \
	lav2wav \
	lav2yuv \
	yuvmedianfilter \
	lavaddwav \
	lavtrans \
	ypipe yuv2lav \
	transist.flt \
	matteblend.flt \
	multiblend.flt \
	lavpipe \
	yuvscaler \
	yuvcorrect \
	yuvcorrect_tune \
	jpeg2yuv \
	ppmtoy4m \
	y4mtoppm \
	pgmtoy4m \
	y4mcolorbars \
	lavinfo \
	y4mshift \
	y4mspatialfilter \
	y4mstabilizer \
	yuvfps \
	y4mblack \
	y4mtoyuv \
	y4minterlace \
	yuv4mpeg \
	yuvinactive

if HAVE_GTK
  bin_PROGRAMS += glav
endif
if HAVE_SDL
  bin_PROGRAMS += yuvplay
endif
if HAVE_V4L
  bin_PROGRAMS += lavrec testrec lavvideo
endif
if HAVE_V4L_OR_SDL
  bin_PROGRAMS += lavplay
endif
if HAVE_LIBPNG
  bin_PROGRAMS += png2yuv
endif

noinst_HEADERS = \
	colorspace.h \
	glav.h \
	pipelist.h \
	subsample.h \
	lav_common.h \
	lav2wav.h \
	yuvscaler.h \
	yuvcorrect.h

ypipe_SOURCES = ypipe.c
ypipe_LDADD = $(LIBMJPEGUTILS)

yuv2lav_SOURCES = yuv2lav.c
yuv2lav_LDADD = $(LIBMJPEGUTILS) liblavfile.la liblavjpeg.la $(LIBM_LIBS)

transist_flt_SOURCES = transist.flt.c
transist_flt_LDADD = $(LIBMJPEGUTILS)

matteblend_flt_SOURCES = matteblend.flt.c
matteblend_flt_LDADD = $(LIBMJPEGUTILS)

multiblend_flt_SOURCES = multiblend.flt.c
multiblend_flt_LDADD = $(LIBMJPEGUTILS)

if HAVE_X
  LAVPLAY_X_LIBS = $(X_LIBS) -lX11
endif

lavplay_SOURCES = lavplay.c
lavplay_LDADD = $(LIBMJPEGUTILS) \
	liblavplay.la liblavjpeg.la liblavfile.la \
	@LIBGETOPT_LIB@ $(PTHREAD_LIBS) $(SDL_LIBS) $(LAVPLAY_X_LIBS)

lavrec_SOURCES = lavrec.c
lavrec_LDADD = $(LIBMJPEGUTILS) \
	liblavrec.la liblavfile.la liblavjpeg.la \
	@LIBGETOPT_LIB@ $(PTHREAD_LIBS)

testrec_SOURCES = testrec.c audiolib.c
testrec_LDADD = $(PTHREAD_LIBS) $(LIBMJPEGUTILS)

lav2yuv_SOURCES = lav2yuv.c lav_common.c
lav2yuv_LDADD = $(LIBMJPEGUTILS) \
		liblavfile.la liblavjpeg.la $(LIBDV_LIBS)

jpeg2yuv_SOURCES = jpeg2yuv.c
jpeg2yuv_LDADD = $(LIBMJPEGUTILS) liblavjpeg.la $(JPEG_LIBS)

png2yuv_SOURCES = png2yuv.c colorspace.c subsample.c
png2yuv_LDADD = $(LIBMJPEGUTILS) -lpng -lz -lm

lavpipe_SOURCES = lavpipe.c pipelist.c
lavpipe_LDADD = $(LIBMJPEGUTILS)

yuvmedianfilter_SOURCES = yuvmedianfilter.c
yuvmedianfilter_LDADD = $(LIBMJPEGUTILS)

lav2wav_SOURCES = lav2wav.c
lav2wav_LDADD = $(LIBMJPEGUTILS) liblavfile.la

lavtrans_SOURCES = lavtrans.c
lavtrans_LDADD = $(LIBMJPEGUTILS) liblavfile.la

lavaddwav_SOURCES = lavaddwav.c
lavaddwav_LDADD = $(LIBMJPEGUTILS) liblavfile.la

glav_SOURCES = glav.c glav_main.c
glav_LDADD = $(LIBMJPEGUTILS) ${GTK_LIBS}

lavvideo_SOURCES = lavvideo.c frequencies.c

yuvscaler_SOURCES = yuvscaler.c yuvscaler_resample.c yuvscaler_bicubic.c
yuvscaler_LDADD = $(LIBMJPEGUTILS) $(LIBM_LIBS)

yuvcorrect_SOURCES = yuvcorrect.c yuvcorrect_functions.c
yuvcorrect_LDADD = $(LIBMJPEGUTILS) $(LIBM_LIBS)

yuvcorrect_tune_SOURCES = yuvcorrect_tune.c yuvcorrect_functions.c
yuvcorrect_tune_LDADD = $(LIBMJPEGUTILS) $(LIBM_LIBS)

yuvplay_SOURCES = yuvplay.c
yuvplay_LDADD = $(SDL_LIBS) $(LIBMJPEGUTILS)

ppmtoy4m_SOURCES = ppmtoy4m.c colorspace.c subsample.c
ppmtoy4m_LDADD = $(LIBMJPEGUTILS)

pgmtoy4m_SOURCES = pgmtoy4m.c
pgmtoy4m_LDADD = $(LIBMJPEGUTILS)

y4mtoppm_SOURCES = y4mtoppm.c colorspace.c subsample.c
y4mtoppm_LDADD = $(LIBMJPEGUTILS)

y4mcolorbars_SOURCES = y4mcolorbars.c colorspace.c subsample.c
y4mcolorbars_LDADD = $(LIBMJPEGUTILS)

lavinfo_SOURCES = lavinfo.c
lavinfo_LDADD = $(LIBMJPEGUTILS) liblavfile.la

y4mshift_SOURCES = y4mshift.c
y4mshift_LDADD = $(LIBMJPEGUTILS)

y4mspatialfilter_SOURCES = y4mspatialfilter.c
y4mspatialfilter_LDADD = $(LIBMJPEGUTILS) $(LIBM_LIBS)

y4mstabilizer_SOURCES = y4mstabilizer.c subsample.c
y4mstabilizer_LDADD = $(LIBMJPEGUTILS)

y4mblack_SOURCES = y4mblack.c
y4mblack_LDADD = $(LIBMJPEGUTILS)

y4minterlace_SOURCES = y4minterlace.c
y4minterlace_LDADD = $(LIBMJPEGUTILS)

y4mtoyuv_SOURCES = y4mtoyuv.c
y4mtoyuv_LDADD = $(LIBMJPEGUTILS)

yuv4mpeg_SOURCES = yuv4mpeg.c
yuv4mpeg_LDADD = $(LIBMJPEGUTILS)

yuvfps_SOURCES = yuvfps.c
yuvfps_LDADD = $(LIBMJPEGUTILS)

yuvinactive_SOURCES = yuvinactive.c
yuvinactive_LDADD = $(LIBMJPEGUTILS)
