# Makefile for lavtools

INCLUDES =   -I$(top_srcdir)/utils \
	-I$(top_srcdir)/mjpeg -I$(includedir) \
	-DG_LOG_DOMAIN=\"lavtools\" -DLAVPLAY_VERSION=\"$(VERSION)\" \
	$(MOVTAR_CFLAGS) $(QUICKTIME_CFLAGS)  -I /usr/X11R6/include/X11 -I /usr/X11R6/include

# *********************************************************************
# Support lib

lib_LIBRARIES = libavio.a
libavio_a_SOURCES = editlist.c lav_io.c avilib.c

# *********************************************************************
# The tools themselves

bin_PROGRAMS = \
	lavplay   lavrec   lav2wav  lav2yuv yuvmedianfilter lavaddwav \
	lavvideo  lavtrans xlav  ypipe yuv2lav testrec  \
	transist.flt matteblend.flt lavpipe yuvscaler yuvplay

lav_headers = lav_io.h editlist.h xlav.h avilib.h yuv4mpeg.h jpegutils.h yuvscaler.h

ypipe_SOURCES = ypipe.c yuv4mpeg.c $(lav_headers)
ypipe_LDADD =  -L$(top_srcdir)/utils -lutils

yuv2lav_SOURCES = yuv2lav.c jpegutils.c yuv4mpeg.c $(lav_headers)
yuv2lav_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
       $(QUICKTIME_LIBS) $(JPEG_LIBS)  -L$(top_srcdir)/utils -lutils

transist_flt_SOURCES = transist.flt.c yuv4mpeg.c ${lav_headers}
transist_flt_LDADD =  -L$(top_srcdir)/utils -lutils

matteblend_flt_SOURCES = matteblend.flt.c yuv4mpeg.c ${lav_headers}
matteblend_flt_LDADD =  -L$(top_srcdir)/utils -lutils

lavplay_SOURCES = lavplay.c avilib.c audiolib.c lav_io.c editlist.c  $(lav_headers)
lavplay_LDADD = -L$(top_builddir)/mjpeg -lmjpeg $(PTHREAD_LIBS) \
	$(QUICKTIME_LIBS) $(MOVTAR_LIBS) $(SDL_LIBS) $(JPEG_LIBS) \
	-L$(top_srcdir)/utils -lutils

lavrec_SOURCES = lavrec.c avilib.c audiolib.c lav_io.c frequencies.c \
        frequencies.h $(lav_headers)
lavrec_LDADD = -L$(top_builddir)/mjpeg -lmjpeg $(PTHREAD_LIBS) \
	$(QUICKTIME_LIBS) $(MOVTAR_LIBS) $(JPEG_LIBS) \
	-L$(top_srcdir)/utils -lutils

testrec_SOURCES = testrec.c  audiolib.c  $(lav_headers)
testrec_LDADD = $(PTHREAD_LIBS) -L$(top_srcdir)/utils -lutils

lav2yuv_SOURCES = lav2yuv.c  jpegutils.c yuv4mpeg.c $(lav_headers)
lav2yuv_LDADD = -L$(top_builddir)/lavtools -lavio $(QUICKTIME_LIBS) \
	$(MOVTAR_LIBS) 	$(JPEG_LIBS) -L$(top_srcdir)/utils -lutils

lavpipe_SOURCES = lavpipe.c yuv4mpeg.c pipelist.c $(lav_headers)
lavpipe_LDADD = -L$(top_srcdir)/utils -lutils

yuvmedianfilter_SOURCES = yuvmedianfilter.c $(lav_headers)
yuvmedianfilter_LDFLAGS = -L$(top_builddir)/lavtools 
yuvmedianfilter_LDADD =  -L$(top_srcdir)/utils -lutils

lav2wav_SOURCES = lav2wav.c  $(lav_headers)
lav2wav_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
	$(QUICKTIME_LIBS) $(JPEG_LIBS) -L$(top_srcdir)/utils -lutils

lavtrans_SOURCES = lavtrans.c $(lav_headers)
lavtrans_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
	$(QUICKTIME_LIBS) $(JPEG_LIBS) -L$(top_srcdir)/utils -lutils

lavaddwav_SOURCES = lavaddwav.c $(lav_headers)
lavaddwav_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
		$(QUICKTIME_LIBS) $(JPEG_LIBS) -L$(top_srcdir)/utils -lutils

xlav_SOURCES = xlav.c xlav_main.c  $(lav_headers)
xlav_LDADD = $(X_LIBS) -lX11 $(FORMS_LIBS) $(X_EXTRA_LIBS) -L$(top_srcdir)/utils -lutils

lavvideo_SOURCES = lavvideo.c

yuvscaler_SOURCES = yuvscaler.c editlist.c lav_io.c $(lav_headers)
yuvscaler_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
	$(QUICKTIME_LIBS) $(JPEG_LIBS) -L$(top_srcdir)/utils -lutils

yuvplay_SOURCES = yuvplay.c yuv4mpeg.h yuv4mpeg.c
yuvplay_LDADD = $(SDL_LIBS) $(PTHREAD_LIBS) -L$(top_srcdir)/utils -lutils
