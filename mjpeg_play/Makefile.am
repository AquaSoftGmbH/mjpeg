# Process with automake to produce Makefile.in

SUBDIRS = \
	utils \
	lavtools \
	aenc \
	mplex \
	mpeg2enc \
	scripts \
	docs \
	yuvdenoise \
	yuvfilters \
	yuvdeinterlace \
	y4mdenoise \
	debian

bin_SCRIPTS = mjpegtools-config

#
# Add any non autoconf'd files here, extra readmes and other misc 
# info to be copied into the dist
#
EXTRA_DIST = \
	BUGS \
	CHANGES \
	HINTS \
	PLANS \
	README.AltiVec \
	README.DV \
	README.avilib \
	README.glav \
	README.lavpipe \
	README.transist \
	TODO \
	mjpeg_howto.txt \
	mjpegtools.spec \
	mjpegtools.spec.in \
	mjpegtools-config.in \
	mjpegtools.pc.in \
	autogen.sh \
	strip_fPIC.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mjpegtools.pc

DISTCLEANFILES = \
	mjpegtools.pc \
	mjpegtools-config \
	confdefs.h \
	config.cache \
	config.status \
	config.log

MAINTAINERCLEANFILES = \
	compile \
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
	rpmbuild -ta --clean $(PACKAGE)-$(VERSION).tar.gz

## make debs
deb: Makefile dist
	-chmod -R +w $(PACKAGE)-$(VERSION)
	rm -rf $(PACKAGE)-$(VERSION)
	tar xzf $(PACKAGE)-$(VERSION).tar.gz
	cd $(PACKAGE)-$(VERSION); dpkg-buildpackage -rfakeroot
