# Makefile for lavtools
#the dependence files are those which have been adapted to MMX and therefore
#are suspect to change 

## FIXME ! Hardcoded libraries are strongly disliked !
##v4l-conf.o: v4l-conf.c 
##v4l-conf.o: v4l-conf.c
##	gcc $(INCLUDES) -c v4l-conf.c

## EXTRA-DIST = README PLANS
INCLUDES = -I./mjpeg -I./movtar -I./quicktime/quicktime -I/usr/X11R6/lib @GLIB_CFLAGS@ 
SUBDIRS = mjpeg movtar quicktime . aenc mpegjoin mplex utils xlav
## mpeg2enc taken out temporarily

bin_PROGRAMS = lavplay lavrec lavvideo v4l-conf
lavplay_SOURCES = lavplay.c avilib.c audiolib.c lav_io.c editlist.c
lavplay_LDFLAGS = -L./movtar/ -L./mjpeg/ -L./quicktime/quicktime @GLIB_LIBS@
lavplay_LDADD = -lmovtar -lmjpeg -lquicktime -lpng -lpthread @SDL@ @JPEG@ 

lavrec_SOURCES = lavrec.c avilib.c audiolib.c lav_io.c
lavrec_LDFLAGS = -L./movtar/ -L./mjpeg/ -L./quicktime/quicktime @GLIB_LIBS@
lavrec_LDADD = -lmovtar -lmjpeg -lquicktime -lpng -lpthread @JPEG@ 

lavvideo_SOURCES = lavvideo.c

v4l_conf_SOURCES = v4l-conf.c 
v4l_conf_INCLUDES = -I./.. -I.. -I/usr/X11R6/lib 
## What an AWFUL HACK ! v4l-conf.o needs to stand where it does now, but automake always wants
## to add v4l-conf.o in the end (which causes multiple function linkage errors), so I declare
## the last v4l-conf.o as an _INCLUDE DIRECTORY_ :-/ :-/ 
v4l_conf_LINK = gcc -Wall -Wstrict-prototypes -g -O2 -D_REENTRANT -o v4l-conf v4l-conf.o -L/usr/X11R6/lib -lXdpms -lXxf86vm -lXxf86dga -lXext -lX11 -I
## DON'T add any new programs below here, only above ! (because of the hack)

## make rpms
rpm: Makefile
	$(MAKE) dist
	rpm -ta --clean $(PACKAGE)-$(VERSION).tar.gz

## make debs
deb: 
	mkdir debian/tmp
	dpkg-buildpackage -rfakeroot
	rm -r debian/tmp












