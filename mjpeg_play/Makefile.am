# Process with automake to produce Makefile.in

SUBDIRS    = utils lavtools aenc mplex mpeg2enc	scripts docs yuvdenoise yuvfilters

bin_SCRIPTS = mjpegtools-config

#
# Add any non autoconf'd files here, extra readmes and other misc 
# info to be copied into the dist
#
EXTRA_DIST = \
 README.avilib\
 README.lavpipe\
 README.transist\
 README.DV\
 README.glav\
 HINTS\
 TODO\
 BUGS\
 CHANGES\
 PLANS\
 mjpeg_howto.txt \
 mjpegtools.spec \
 mjpegtools.pc.in \
 autogen.sh\
 depcomp\
 .cvsignore

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mjpegtools.pc

DISTCLEANFILES = \
 confdefs.h \
 config.cache \
 config.status \
 config.log

MAINTAINERCLEANFILES = \
 depcomp \
 install-sh \
 missing \
 mkinstalldirs \
 Makefile.in \
 aclocal.m4 \
 config.guess \
 config.h.in \
 config.sub \
 configure \
 ltmain.sh \
 stamp-h.in

## make rpms
rpm: Makefile
	$(MAKE) dist
	rpm -ta --clean $(PACKAGE)-$(VERSION).tar.gz

## make debs
deb: 
	mkdir debian/tmp
	dpkg-buildpackage -rfakeroot
	rm -r debian/tmp

