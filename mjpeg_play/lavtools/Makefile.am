# Makefile for lavtools

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in

INCLUDES =  -I$(top_srcdir) -I$(includedir) \
	-DG_LOG_DOMAIN=\"lavtools\" -DLAVPLAY_VERSION=\"$(VERSION)\" \
	$(MOVTAR_CFLAGS) $(DV_CFLAGS) $(QUICKTIME_CFLAGS) ${X_CFLAGS} \
        -I /usr/X11R6/include -I$(top_srcdir)/utils \
	$(AVIPLAY_CFLAGS) ${GTK_CFLAGS}

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

liblavplay_la_SOURCES = liblavplay.c audiolib.c
liblavplay_la_LDFLAGS = $(LAV_ALL_LIB_OPTS)

# *********************************************************************
# The tools themselves

if HAVE_AVIFILE
  AVIFILE_BIN = lav2divx yuv2divx divxdec
else
  AVIFILE_BIN = 
endif

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

bin_PROGRAMS = \
	$(LAVPLAY_BIN) $(LAVREC_BIN) lav2wav  lav2yuv \
	yuvmedianfilter lavaddwav lavtrans $(GLAV_BIN) \
	ypipe yuv2lav transist.flt matteblend.flt lavpipe \
	yuvscaler jpeg2yuv $(AVIFILE_BIN) $(YUVPLAY_BIN) \
	ppmtoy4m y4mtoppm y4mcolorbars lavinfo

lav_headers = lav_io.h editlist.h glav.h avilib.h jpegutils.h \
	yuvscaler.h frequencies.h lav_common.h lav2wav.h

ypipe_SOURCES = ypipe.c $(lav_headers)
ypipe_LDADD = -L$(top_builddir)/utils -lmjpegutils

yuv2lav_SOURCES = yuv2lav.c $(lav_headers)
yuv2lav_LDADD = -L$(top_builddir)/utils -lmjpegutils \
		 liblavfile.la liblavjpeg.la

transist_flt_SOURCES = transist.flt.c ${lav_headers}
transist_flt_LDADD = -L$(top_builddir)/utils -lmjpegutils

matteblend_flt_SOURCES = matteblend.flt.c ${lav_headers}
matteblend_flt_LDADD = -L$(top_builddir)/utils -lmjpegutils

lavplay_SOURCES = lavplay.c ${lav_headers}
lavplay_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	liblavplay.la liblavjpeg.la liblavfile.la \
	@LIBGETOPT_LIB@ ${PTHREAD_LIBS} ${SDL_LIBS} ${X_LIBS} -lX11

lavrec_SOURCES = lavrec.c $(lav_headers)
lavrec_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	liblavrec.la liblavfile.la liblavjpeg.la \
	@LIBGETOPT_LIB@ $(PTHREAD_LIBS)

testrec_SOURCES = testrec.c  audiolib.c  $(lav_headers)
testrec_LDADD = $(PTHREAD_LIBS) -L$(top_builddir)/utils -lmjpegutils

lav2yuv_SOURCES = lav2yuv.c lav_common.c $(lav_headers)
lav2yuv_LDADD = -L$(top_builddir)/utils -lmjpegutils \
		liblavfile.la liblavjpeg.la $(DV_LIBS)

jpeg2yuv_SOURCES = jpeg2yuv.c $(lav_headers)
jpeg2yuv_LDADD = -L$(top_builddir)/utils -lmjpegutils liblavjpeg.la ${GTK_LIBS}

lav2divx_SOURCES = lav2divx.cpp libavifile.h lav_common.c $(lav_headers)
lav2divx_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	liblavfile.la liblavjpeg.la $(DV_LIBS) \
	$(AVIPLAY_LIBS) $(LIBM_LIBS)

lavpipe_SOURCES = lavpipe.c pipelist.c pipelist.h $(lav_headers)
lavpipe_LDADD = -L$(top_builddir)/utils -lmjpegutils 

yuvmedianfilter_SOURCES = yuvmedianfilter.c $(lav_headers)
yuvmedianfilter_LDADD = -L$(top_builddir)/utils -lmjpegutils

lav2wav_SOURCES = lav2wav.c  $(lav_headers)
lav2wav_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	 liblavfile.la

lavtrans_SOURCES = lavtrans.c $(lav_headers)
lavtrans_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	 liblavfile.la

lavaddwav_SOURCES = lavaddwav.c $(lav_headers)
lavaddwav_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	 liblavfile.la

glav_SOURCES = glav.c glav_main.c  $(lav_headers)
glav_LDADD = -L$(top_builddir)/utils -lmjpegutils ${GTK_LIBS}

lavvideo_SOURCES = lavvideo.c $(lav_headers)

yuvscaler_SOURCES = yuvscaler.c yuvscaler_resample.c yuvscaler_bicubic.c $(lav_headers)
yuvscaler_LDADD = -L$(top_builddir)/utils -lmjpegutils $(LIBM_LIBS) \
	 liblavfile.la liblavjpeg.la

yuvplay_SOURCES = yuvplay.c $(lav_headers)
yuvplay_LDADD = $(SDL_LIBS) $(PTHREAD_LIBS) -L$(top_builddir)/utils -lmjpegutils

yuv2divx_SOURCES = yuv2divx.cpp libavifile.h $(lav_headers)
yuv2divx_LDADD = $(PTHREAD_LIBS) -L$(top_builddir)/utils -lmjpegutils \
	$(AVIPLAY_LIBS) $(LIBM_LIBS) liblavfile.la

ppmtoy4m_SOURCES = ppmtoy4m.c colorspace.h colorspace.c subsample.h subsample.c
ppmtoy4m_LDADD = -L$(top_builddir)/utils -lmjpegutils

y4mtoppm_SOURCES = y4mtoppm.c colorspace.h colorspace.c subsample.h subsample.c
y4mtoppm_LDADD = -L$(top_builddir)/utils -lmjpegutils

y4mcolorbars_SOURCES = y4mcolorbars.c colorspace.h colorspace.c subsample.h subsample.c
y4mcolorbars_LDADD = -L$(top_builddir)/utils -lmjpegutils

divxdec_SOURCES = divxdec.cpp riffinfo.h libavifile.h lav_common.c
divxdec_LDADD = $(PTHREAD_LIBS) -L$(top_builddir)/utils -lmjpegutils \
	$(AVIPLAY_LIBS) $(LIBM_LIBS) \
	liblavfile.la liblavplay.la liblavjpeg.la

lavinfo_SOURCES = lavinfo.c $(lav_headers)
lavinfo_LDADD = -L$(top_builddir)/utils -lmjpegutils \
	liblavfile.la
