# Makefile for lavtools

INCLUDES =  -I$(top_srcdir) -I$(top_srcdir)/mjpeg -I$(includedir) \
	-DG_LOG_DOMAIN=\"lavtools\" -DLAVPLAY_VERSION=\"$(VERSION)\" \
	$(MOVTAR_CFLAGS) $(QUICKTIME_CFLAGS) -I /usr/X11R6/include/X11 \
        $(DV_CFLAGS) -I /usr/X11R6/include -I$(top_srcdir)/utils \
	$(AVIPLAY_CFLAGS) ${GTK_CFLAGS}

# *********************************************************************
# Support lib

lib_LTLIBRARIES = liblavfile.la liblavjpeg.la liblavrec.la liblavplay.la

liblavincludedir = $(includedir)/mjpegtools
liblavinclude_HEADERS = editlist.h lav_io.h jpegutils.h frequencies.h \
	liblavrec.h liblavplay.h

LAV_ALL_LIB_OPTS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic 

liblavfile_la_SOURCES = editlist.c lav_io.c avilib.c
liblavfile_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) \
	$(MOVTAR_LIBS) $(QUICKTIME_LIBS) $(JPEG_LIBS)

liblavjpeg_la_SOURCES = jpegutils.c
liblavjpeg_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) $(JPEG_LIBS)

liblavrec_la_SOURCES = liblavrec.c audiolib.c frequencies.c
liblavrec_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) 

liblavplay_la_SOURCES = liblavplay.c audiolib.c
liblavplay_la_LDFLAGS = $(LAV_ALL_LIB_OPTS) 

# *********************************************************************
# The tools themselves

if HAVE_AVIFILE
AVIFILE_BIN = lav2divx yuv2divx
else
AVIFILE_BIN = 
endif

if HAVE_GTK
GLAV_BIN = glav
else
GLAV_BIN = 
endif

bin_PROGRAMS = \
	lavplay   lavrec   lav2wav  lav2yuv yuvmedianfilter  lavaddwav \
	lavvideo  lavtrans $(GLAV_BIN)  ypipe yuv2lav testrec \
	transist.flt matteblend.flt lavpipe yuvscaler yuvplay \
	$(AVIFILE_BIN)

lav_headers = lav_io.h editlist.h glav.h avilib.h yuv4mpeg.h jpegutils.h \
	yuvscaler.h frequencies.h lav_common.h

ypipe_SOURCES = ypipe.c yuv4mpeg.c $(lav_headers)
ypipe_LDADD = -L$(top_srcdir)/utils -lutils

yuv2lav_SOURCES = yuv2lav.c yuv4mpeg.c $(lav_headers)
yuv2lav_LDADD = -L$(top_srcdir)/utils -lutils \
		 liblavfile.la liblavjpeg.la

transist_flt_SOURCES = transist.flt.c yuv4mpeg.c ${lav_headers}
transist_flt_LDADD = -L$(top_srcdir)/utils -lutils

matteblend_flt_SOURCES = matteblend.flt.c yuv4mpeg.c ${lav_headers}
matteblend_flt_LDADD = -L$(top_srcdir)/utils -lutils

lavplay_SOURCES = lavplay.c
lavplay_LDADD = -L$(top_srcdir)/utils -lutils liblavplay.la $(PTHREAD_LIBS) $(SDL_LIBS) liblavjpeg.la liblavfile.la


lavrec_SOURCES = lavrec.c $(lav_headers)
lavrec_LDADD = -L$(top_srcdir)/utils -lutils liblavrec.la liblavfile.la $(PTHREAD_LIBS)

testrec_SOURCES = testrec.c  audiolib.c  $(lav_headers)
testrec_LDADD = $(PTHREAD_LIBS) -L$(top_srcdir)/utils -lutils

lav2yuv_SOURCES = lav2yuv.c yuv4mpeg.c lav_common.c $(lav_headers)
lav2yuv_LDADD = -L$(top_srcdir)/utils -lutils \
		liblavfile.la liblavjpeg.la $(DV_LIBS)

lav2divx_SOURCES = lav2divx.cpp lav_common.c jpegutils.c yuv4mpeg.c $(lav_headers)
lav2divx_LDADD = -L$(top_srcdir)/utils -lutils \
	liblavfile.la liblavjpeg.la $(DV_LIBS) \
	$(AVIPLAY_LIBS)

lavpipe_SOURCES = lavpipe.c yuv4mpeg.c pipelist.c $(lav_headers)
lavpipe_LDADD = -L$(top_srcdir)/utils -lutils 

yuvmedianfilter_SOURCES = yuvmedianfilter.c $(lav_headers)
yuvmedianfilter_LDADD = -L$(top_srcdir)/utils -lutils

lav2wav_SOURCES = lav2wav.c  $(lav_headers)
lav2wav_LDADD = -L$(top_srcdir)/utils -lutils \
	 liblavfile.la

lavtrans_SOURCES = lavtrans.c $(lav_headers)
lavtrans_LDADD = -L$(top_srcdir)/utils -lutils \
	 liblavfile.la

lavaddwav_SOURCES = lavaddwav.c $(lav_headers)
lavaddwav_LDADD = -L$(top_srcdir)/utils -lutils \
	 liblavfile.la

glav_SOURCES = glav.c glav_main.c  $(lav_headers)
glav_LDADD = -L$(top_srcdir)/utils -lutils ${GTK_LIBS}

lavvideo_SOURCES = lavvideo.c

yuvscaler_SOURCES = yuvscaler.c $(lav_headers)
yuvscaler_LDADD = -L$(top_builddir)/utils -lutils \
	 liblavfile.la liblavjpeg.la

yuvplay_SOURCES = yuvplay.c yuv4mpeg.h yuv4mpeg.c
yuvplay_LDADD = $(SDL_LIBS) $(PTHREAD_LIBS) -L$(top_srcdir)/utils -lutils

yuv2divx_SOURCES = yuv2divx.cpp yuv4mpeg.h yuv4mpeg.c
yuv2divx_LDADD = $(PTHREAD_LIBS) -L$(top_srcdir)/utils -lutils \
	$(AVIPLAY_LIBS)
