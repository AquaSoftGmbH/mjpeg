#Makefile for mjpeg
#the dependence files are those which have been adapted to MMX and therefore
#are suspect to change 

## FIXME ! Hardcoded libraries are strongly disliked !
##v4l-conf.o: v4l-conf.c 
##v4l-conf.o: v4l-conf.c
##	gcc $(INCLUDES) -c v4l-conf.c

## EXTRA-DIST = README PLANS
INCLUDES = -I./mjpeg @MOVTAR_CFLAGS@ -I/usr/X11R6/lib @GLIB_CFLAGS@ @QUICKTIME_INCLUDE@
SUBDIRS = mjpeg lavtools aenc mplex mpeg2enc
CFLAGS=-Wall -g


## make rpms
rpm: Makefile
	$(MAKE) dist
	rpm -ta --clean $(PACKAGE)-$(VERSION).tar.gz

## make debs
deb: 
	mkdir debian/tmp
	dpkg-buildpackage -rfakeroot
	rm -r debian/tmp












