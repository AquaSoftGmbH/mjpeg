# Makefile for lavtools

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in

INCLUDES =  -I$(top_srcdir) -I$(includedir) \
	-DG_LOG_DOMAIN=\"lavtools\" -DLAVPLAY_VERSION=\"$(VERSION)\" \
	$(MOVTAR_CFLAGS) $(DV_CFLAGS) $(QUICKTIME_CFLAGS) ${X_CFLAGS} \
        -I /usr/X11R6/include -I$(top_srcdir)/utils \
	${GTK_CFLAGS} $(JPEG_CFLAGS)

# *********************************************************************
# Support lib

if HAVE_V4L
  LAVREC_LIB_FILE = liblavrec.la
else
  LAVREC_LIB_FILE =
endif

if HAVE_V4L_OR_SDL
  LAVPLAY_LIB_FILE = liblavplay.la
else
  LAVPLAY_LIB_FILE =
endif

lib_LTLIBRARIES = liblavfile.la liblavjpeg.la $(LAVREC_LIB_FILE) \
	$(LAVPLAY_LIB_FILE)

liblavincludedir = $(includedir)/mjpegtools
liblavinclude_HEADERS = editlist.h lav_io.h jpegutils.h frequencies.h \
	liblavrec.h liblavplay.h audiolib.h

LAV_ALL_LIB_OPTS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic 

liblavfile_la_SOURCES = editlist.c lav_io.c avilib.c
liblavfile_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) \
	$(MOVTAR_LIBS) $(QUICKTIME_LIBS) $(JPEG_LIBS) $(DV_LIBS)

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

if HAVE_GTK
  GLAV_BIN = glav
else
  GLAV_BIN = 
endif

if HAVE_SDL
  YUVPLAY_BIN = yuvplay
else
  YUVPLAY_BIN =
endif

if HAVE_V4L
  LAVREC_BIN = lavrec testrec lavvideo
else
  LAVREC_BIN =
endif

if HAVE_V4L_OR_SDL
  LAVPLAY_BIN = lavplay
else
  LAVPLAY_BIN =
endif

if HAVE_PNG
  PNG_BIN = png2yuv
else
  PNG_BIN =
endif

bin_PROGRAMS = \
	$(LAVPLAY_BIN) $(LAVREC_BIN) lav2wav  lav2yuv \
	yuvmedianfilter lavaddwav lavtrans $(GLAV_BIN) \
	ypipe yuv2lav transist.flt matteblend.flt \
	multiblend.flt lavpipe \
	yuvscaler yuvcorrect yuvcorrect_tune jpeg2yuv $(YUVPLAY_BIN) \
	ppmtoy4m y4mtoppm pgmtoy4m y4mcolorbars lavinfo y4mshift yuvfps \
	$(PNG_BIN)

lav_headers = lav_io.h editlist.h glav.h avilib.h jpegutils.h \
	yuvscaler.h yuvcorrect.h frequencies.h lav_common.h lav2wav.h

ypipe_SOURCES = ypipe.c $(lav_headers)
ypipe_LDADD = $(top_builddir)/utils/libmjpegutils.a

yuv2lav_SOURCES = yuv2lav.c $(lav_headers)
yuv2lav_LDADD = $(top_builddir)/utils/libmjpegutils.a \
		 liblavfile.la liblavjpeg.la ${LIBM_LIBS}

transist_flt_SOURCES = transist.flt.c ${lav_headers}
transist_flt_LDADD = $(top_builddir)/utils/libmjpegutils.a

matteblend_flt_SOURCES = matteblend.flt.c ${lav_headers}
matteblend_flt_LDADD = $(top_builddir)/utils/libmjpegutils.a

multiblend_flt_SOURCES = multiblend.flt.c ${lav_headers}
multiblend_flt_LDADD = $(top_builddir)/utils/libmjpegutils.a

if HAVE_X
  LAVPLAY_X_LIBS = $(X_LIBS) -lX11
endif

lavplay_SOURCES = lavplay.c ${lav_headers}
lavplay_LDADD = $(top_builddir)/utils/libmjpegutils.a \
	liblavplay.la liblavjpeg.la liblavfile.la \
	@LIBGETOPT_LIB@ ${PTHREAD_LIBS} ${SDL_LIBS} $(LAVPLAY_X_LIBS)

lavrec_SOURCES = lavrec.c $(lav_headers)
lavrec_LDADD = $(top_builddir)/utils/libmjpegutils.a \
	liblavrec.la liblavfile.la liblavjpeg.la \
	@LIBGETOPT_LIB@ $(PTHREAD_LIBS)

testrec_SOURCES = testrec.c  audiolib.c  $(lav_headers)
testrec_LDADD = $(PTHREAD_LIBS) $(top_builddir)/utils/libmjpegutils.a

lav2yuv_SOURCES = lav2yuv.c lav_common.c $(lav_headers)
lav2yuv_LDADD = $(top_builddir)/utils/libmjpegutils.a \
		liblavfile.la liblavjpeg.la $(DV_LIBS)

jpeg2yuv_SOURCES = jpeg2yuv.c $(lav_headers)
jpeg2yuv_LDADD = $(top_builddir)/utils/libmjpegutils.a \
		liblavjpeg.la ${GTK_LIBS}

png2yuv_SOURCES = png2yuv.c colorspace.h colorspace.c subsample.h subsample.c
png2yuv_LDADD = $(top_builddir)/utils/libmjpegutils.a -lpng -lz -lm

lavpipe_SOURCES = lavpipe.c pipelist.c pipelist.h $(lav_headers)
lavpipe_LDADD = $(top_builddir)/utils/libmjpegutils.a

yuvmedianfilter_SOURCES = yuvmedianfilter.c $(lav_headers)
yuvmedianfilter_LDADD = $(top_builddir)/utils/libmjpegutils.a

lav2wav_SOURCES = lav2wav.c  $(lav_headers)
lav2wav_LDADD = $(top_builddir)/utils/libmjpegutils.a \
	 liblavfile.la

lavtrans_SOURCES = lavtrans.c $(lav_headers)
lavtrans_LDADD = $(top_builddir)/utils/libmjpegutils.a \
	 liblavfile.la

lavaddwav_SOURCES = lavaddwav.c $(lav_headers)
lavaddwav_LDADD = $(top_builddir)/utils/libmjpegutils.a \
	 liblavfile.la

glav_SOURCES = glav.c glav_main.c  $(lav_headers)
glav_LDADD = $(top_builddir)/utils/libmjpegutils.a ${GTK_LIBS}

lavvideo_SOURCES = lavvideo.c frequencies.c $(lav_headers)

yuvscaler_SOURCES = yuvscaler.c yuvscaler_resample.c yuvscaler_bicubic.c $(lav_headers)
yuvscaler_LDADD = $(top_builddir)/utils/libmjpegutils.a $(LIBM_LIBS) \
	 liblavfile.la liblavjpeg.la

yuvcorrect_SOURCES = yuvcorrect.c yuvcorrect_functions.c $(lav_headers)
yuvcorrect_LDADD = $(top_builddir)/utils/libmjpegutils.a $(LIBM_LIBS) \
        liblavfile.la liblavjpeg.la

yuvcorrect_tune_SOURCES = yuvcorrect_tune.c yuvcorrect_functions.c $(lav_headers)
yuvcorrect_tune_LDADD = $(top_builddir)/utils/libmjpegutils.a $(LIBM_LIBS) \
        liblavfile.la liblavjpeg.la

yuvplay_SOURCES = yuvplay.c $(lav_headers)
yuvplay_LDADD = $(SDL_LIBS) $(PTHREAD_LIBS) $(top_builddir)/utils/libmjpegutils.a

ppmtoy4m_SOURCES = ppmtoy4m.c colorspace.h colorspace.c subsample.h subsample.c
ppmtoy4m_LDADD = $(top_builddir)/utils/libmjpegutils.a

pgmtoy4m_SOURCES = pgmtoy4m.c
pgmtoy4m_LDADD = $(top_builddir)/utils/libmjpegutils.a

y4mtoppm_SOURCES = y4mtoppm.c colorspace.h colorspace.c subsample.h subsample.c
y4mtoppm_LDADD = $(top_builddir)/utils/libmjpegutils.a

y4mcolorbars_SOURCES = y4mcolorbars.c colorspace.h colorspace.c subsample.h subsample.c
y4mcolorbars_LDADD = $(top_builddir)/utils/libmjpegutils.a

lavinfo_SOURCES = lavinfo.c $(lav_headers)
lavinfo_LDADD = $(top_builddir)/utils/libmjpegutils.a \
	liblavfile.la

y4mshift_SOURCES = y4mshift.c $(lav_headers)
y4mshift_LDADD = $(top_builddir)/utils/libmjpegutils.a

yuvfps_SOURCES = yuvfps.c ${lav_headers}
yuvfps_LDADD = ${top_builddir}/utils/libmjpegutils.a
