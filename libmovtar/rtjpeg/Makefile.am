
bin_PROGRAMS=pnm2rtj rtjshow

pnm2rtj_SOURCES=pnm2rtj.c RTjpeg.c RTjpeg.h format.c format.h
pnm2rtj_LDADD = @SDL_LIBS@ @JPEG_LIBS@

rtjshow_SOURCES=rtjshow.c RTjpeg.c RTjpeg.h format.c format.h
rtjshow_LDADD = @SDL_LIBS@ @JPEG_LIBS@

INCLUDES = @GLIB_CFLAGS@ @JPEG_INCLUDE@ @RTJPEG_INCLUDE@ @SDL_INCLUDE@
CFLAGS=@CFLAGS@ -g

EXTRA_DIST = .cvsignore
MAINTAINERCLEANFILES = Makefile.in
