# Makefile for lavtools

INCLUDES =  -I$(top_srcdir) -I$(top_srcdir)/mjpeg -I$(includedir) \
	-DG_LOG_DOMAIN=\"lavtools\" -DLAVPLAY_VERSION=\"$(VERSION)\" \
	$(MOVTAR_CFLAGS) -I /usr/X11R6/include/X11
# *********************************************************************
# Support lib

lib_LIBRARIES = libavio.a
libavio_a_SOURCES = editlist.c lav_io.c avilib.c

# *********************************************************************
# The tools themselves

bin_PROGRAMS = \
	lavplay   lavrec   lav2wav  lav2yuv lav2dfilter  lavaddwav \
	lavvideo  lavtrans xlav     testrec v4l-conf

lav_headers = lav_io.h editlist.h xlav.h avilib.h


lavplay_SOURCES = lavplay.c avilib.c audiolib.c lav_io.c editlist.c  $(lav_headers)
lavplay_LDADD = -L$(top_builddir)/mjpeg -lmjpeg $(PTHREAD_LIBS) \
	$(MOVTAR_LIBS) $(SDL_LIBS) $(JPEG_LIBS)

lavrec_SOURCES = lavrec.c avilib.c audiolib.c lav_io.c $(lav_headers)
lavrec_LDADD = -L$(top_builddir)/mjpeg -lmjpeg $(PTHREAD_LIBS) \
	$(MOVTAR_LIBS)

testrec_SOURCES = testrec.c  audiolib.c  $(lav_headers)
testrec_LDADD = $(PTHREAD_LIBS)

lav2yuv_SOURCES = lav2yuv.c  jpegutils.c $(lav_headers)
lav2yuv_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
	$(JPEG_LIBS)

lav2dfilter_SOURCES = lav2dfilter.c $(lav_headers)
lav2dfilter_LDFLAGS = -L$(top_builddir)/lavtools 
lav2dfilter_LDADD = 

lav2wav_SOURCES = lav2wav.c  $(lav_headers)
lav2wav_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS)

lavtrans_SOURCES = lavtrans.c $(lav_headers)
lavtrans_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS) \
	$(JPEG_LIBS)

lavaddwav_SOURCES = lavaddwav.c $(lav_headers)
lavaddwav_LDADD = -L$(top_builddir)/lavtools -lavio $(MOVTAR_LIBS)

xlav_SOURCES = xlav.c xlav_main.c  $(lav_headers)
xlav_LDADD = $(X_LIBS) -lX11 $(FORMS_LIBS) $(X_EXTRA_LIBS)

lavvideo_SOURCES = lavvideo.c

v4l_conf_SOURCES = v4l-conf.c
v4l_conf_LDADD = $(X_LIBS) -lX11 $(V4LCONF_LIBS) $(X_EXTRA_LIBS)

