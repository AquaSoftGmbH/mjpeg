# Process with automake to produce Makefile.in

SUBDIRS    = utils mjpeg lavtools aenc mplex mpeg2enc scripts docs

#
# Add any non autoconf'd files here, extra readmes and other misc 
# info to be copied into the dist
#
EXTRA_DIST = HINTS README.avilib TODO BUGS CHANGES mjpeg_howto.txt mjpeg_man.1

## make rpms
rpm: Makefile
	$(MAKE) dist
	rpm -ta --clean $(PACKAGE)-$(VERSION).tar.gz

## make debs
deb: 
	mkdir debian/tmp
	dpkg-buildpackage -rfakeroot
	rm -r debian/tmp



