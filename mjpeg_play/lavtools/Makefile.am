# Makefile for lavtools
#the dependence files are those which have been adapted to MMX and therefore
#are suspect to change 

## FIXME ! Hardcoded libraries are strongly disliked !
##v4l-conf.o: v4l-conf.c 
##v4l-conf.o: v4l-conf.c
##	gcc $(INCLUDES) -c v4l-conf.c

## EXTRA-DIST = README PLANS
INCLUDES = -I../mjpeg @MOVTAR_CFLAGS@ -I/usr/X11R6/lib @GLIB_CFLAGS@ @QUICKTIME_INCLUDE@
CFLAGS=-Wall -Werror -g

##
## TODO: Need to include the mpeg programs in bin_PROGRAMS for installation
##
bin_PROGRAMS = lavplay lavrec lavvideo v4l-conf lav2wav lav2yuv lavtrans lavaddwav
lavplay_SOURCES = lavplay.c avilib.c audiolib.c lav_io.c editlist.c
lavplay_LDFLAGS = -L../mjpeg/ @GLIB_LIBS@
lavplay_LDADD = @MOVTAR_LIBS@ -lmjpeg

lavrec_SOURCES = lavrec.c avilib.c audiolib.c lav_io.c
lavrec_LDFLAGS = -L../mjpeg/ @GLIB_LIBS@
lavrec_LDADD = @MOVTAR_LIBS@ -lmjpeg

lav2yuv_LDFLAGS  = -L movtar 
lav2yuv_LDADD = editlist.o lav_io.o avilib.o -lmovtar
lav2yuv_SOURCES = lav2yuv.c  jpegutils.c

lav2wav_SOURCES = lav2wav.c
lav2wav_LDADD = editlist.o lav_io.o avilib.o -lmovtar

lavtrans_LDFLAGS  = -L movtar 
lavtrans_SOURCES = lavtrans.c
lavtrans_LDADD = editlist.o lav_io.o avilib.o -lmovtar

lavaddwav_LDFLAGS  = -L movtar 
lavaddwav_SOURCES = lavaddwav.c
lavaddwav_LDADD = editlist.o lav_io.o avilib.o -lmovtar

lavvideo_SOURCES = lavvideo.c

lib_LIBRARIES = liblavio.a
liblavio_a_SOURCES = lav_io.c avilib.c
## O.k. to do this right we need to *link* against movtar libs.
## Seems more sensible simply to ask that movtar is linked as well if you
## use this lib.
##liblavio_a_LIBADD = @MOVTAR_LIBS@
include_HEADERS = lav_io.h editlist.h

v4l_conf_SOURCES = v4l-conf.c 
v4l_conf_INCLUDES = -I./.. -I.. -I/usr/X11R6/lib 

## A.Stevens Nov 2000, No idea what all this is about.  Links fine with me...
## What an AWFUL HACK ! v4l-conf.o needs to stand where it does now, but automake always wants
## to add v4l-conf.o in the end (which causes multiple function linkage errors), so I declare
## the last v4l-conf.o as an _INCLUDE DIRECTORY_ :-/ :-/ 
##v4l_conf_LINK = gcc -Wall -Wstrict-prototypes -g -O2 -D_REENTRANT -o v4l-conf v4l-conf.o -L/usr/X11R6/lib @X_LIBS@ -lXext -lX11 @X_EXTRA_LIBS@ -I
## DON'T add any new programs below here, only above ! (because of the hack)












