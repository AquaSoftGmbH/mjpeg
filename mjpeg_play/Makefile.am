# Process with automake to produce Makefile.in

SUBDIRS    = utils lavtools aenc mplex mpeg2enc	scripts docs yuvdenoise mjpegtools yuvfilters

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
 mjpeg_howto.txt\
 mjpegtools.spec\
 autogen.sh\
 .cvsignore

MAINTAINERCLEANFILES = \
 Makefile.in \
 aclocal.m4 \
 confdefs.h \
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



