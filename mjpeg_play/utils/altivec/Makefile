# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.5 from Makefile.am.

# Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
# Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.



# Process this file with Automake to produce Makefile.in

SHELL = /bin/sh

srcdir = .
top_srcdir = ../..
prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include
pkgdatadir = $(datadir)/mjpegtools
pkglibdir = $(libdir)/mjpegtools
pkgincludedir = $(includedir)/mjpegtools
top_builddir = ../..

ACLOCAL = ${SHELL} /usr/local/src/mjpeg_play/missing --run aclocal
AUTOCONF = ${SHELL} /usr/local/src/mjpeg_play/missing --run autoconf
AUTOMAKE = ${SHELL} /usr/local/src/mjpeg_play/missing --run automake
AUTOHEADER = ${SHELL} /usr/local/src/mjpeg_play/missing --run autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
INSTALL_HEADER = $(INSTALL_DATA)
transform = s,x,x,
NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = i686-pc-linux-gnu
host_triplet = i686-pc-linux-gnu
ALTIVEC_CFLAGS = 
AMTAR = ${SHELL} /usr/local/src/mjpeg_play/missing --run tar
AS = /usr/bin/nasm
ASFLAGS = -f elf
AVIFILE_CONFIG = 
AVIPLAY_CFLAGS = 
AVIPLAY_LIBS = 
AWK = mawk
CC = gcc
CXX = c++
DEPDIR = .deps
DLLTOOL = @DLLTOOL@
DV_CFLAGS = -I/usr/local/include
DV_LIBS = -L/usr/local/lib -ldv  -lm  -L/usr/lib -lglib  -lpthread -ldl 
ECHO = echo
EXEEXT = 
GLIB_CFLAGS = -I/usr/include/glib-1.2 -I/usr/lib/glib/include
GLIB_CONFIG = /usr/bin/glib-config
GLIB_LIBS = -L/usr/lib -lglib
GTK_CFLAGS = -I/usr/include/gtk-1.2 -I/usr/include/glib-1.2 -I/usr/lib/glib/include -I/usr/X11R6/include
GTK_CONFIG = /usr/bin/gtk-config
GTK_LIBS = -L/usr/lib -L/usr/X11R6/lib -lgtk -lgdk -rdynamic -lgmodule -lglib -ldl -lXi -lXext -lX11 -lm
HAVE_OPENQUICKTIME = 0
HAVE_QUICKTIME = 1
INSTALL_STRIP_PROGRAM = ${SHELL} $(install_sh) -c -s
JPEG_CFLAGS = -I/usr/local/src/mjpeg_play/../jpeg-mmx
JPEG_LIBS =  -L/usr/local/src/mjpeg_play/../jpeg-mmx -ljpeg-mmx
LIBGETOPT_LIB = 
LIBM_LIBS = -lm 
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LN_S = ln -s
LT_AGE = 0
LT_CURRENT = 0
LT_RELEASE = 1.6
LT_REVISION = 0
MAINT = #
MOVTAR_CFLAGS = -I/usr/include -I/usr/local/include -I/usr/include/glib-1.2 -I/usr/lib/glib/include
MOVTAR_CONFIG = /usr/local/bin/movtar-config
MOVTAR_LIBS = -L/usr/local/lib -L/usr/lib -lglib -lmovtar
MOVTAR_VERSION = 0.1.2
OBJDUMP = @OBJDUMP@
OBJEXT = o
PACKAGE = mjpegtools
PRID64_STRING_FORMAT = lld
PTHREAD_LIBS = -lpthread
QUICKTIME_CFLAGS = -I /usr/local/src/quicktime4linux-1.3
QUICKTIME_DIR = /usr/local/src/quicktime4linux-1.3
QUICKTIME_LIBS = -L/usr/local/src/quicktime4linux-1.3 -L/usr/local/src/quicktime4linux-1.3/i686 -lquicktime -L/usr/lib -lglib -lpthread -lpng -lz -lm  -L/lib -ldl
RANLIB = ranlib
SDL_CFLAGS = -I/usr/include/SDL -D_REENTRANT
SDL_CONFIG = /usr/bin/sdl-config
SDL_LIBS = -L/usr/lib -lSDL -lpthread -L/usr/X11R6/lib -lXxf86dga -lXxf86vm -lXv
SDL_LIBS_FOR_LIBS = -L/usr/lib -lSDL -lpthread
SDL_LIBS_FOR_PLUGINS = -L/usr/lib -lSDL -lpthread -L/usr/X11R6/lib -lXxf86dga_pic -lXxf86vm_pic -lXv_pic
STRIP = strip
V4LCONF_LIBS = 
VERSION = 1.6.0
X_CFLAGS =  -I/usr/X11R6/include
X_EXTRA_LIBS = 
X_LIBS =  -L/usr/X11R6/lib
X_PRE_LIBS =  -lSM -lICE
am__include = include
am__quote = 
install_sh = /usr/local/src/mjpeg_play/install-sh
x_libraries = /usr/X11R6/lib

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(ALTIVEC_CFLAGS)


noinst_LIBRARIES = libaltivec.a

libaltivec_a_SOURCES = \
  detect.c \
  motion.c \
  quantize.c \
  benchmark.c \
  amber.c \
  sad_00.c \
  sad_01.c \
  sad_10.c \
  sad_11.c \
  bsad.c \
  sub_mean_reduction.c \
  sumsq.c \
  sumsq_sub22.c \
  bsumsq.c \
  bsumsq_sub22.c \
  build_sub44_mests.c \
  build_sub22_mests.c \
  find_best_one_pel.c \
  fdct_idct.c \
  quant_non_intra.c \
  quant_weight_coeff_sum.c \
  variance.c \
  sub_pred.c \
  add_pred.c \
  pred_comp.c \
  subsample_image.c \
  altivec_conf.h \
  altivec_motion.h \
  altivec_predict.h \
  altivec_quantize.h \
  altivec_transform.h \
  benchmark.h \
  amber.h \
  vectorize.h \
  verify.h


EXTRA_DIST = .cvsignore

MAINTAINERCLEANFILES = Makefile.in
subdir = utils/altivec
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = $(top_builddir)/config.h
CONFIG_CLEAN_FILES =
LIBRARIES = $(noinst_LIBRARIES)

libaltivec_a_AR = $(AR) cru
libaltivec_a_LIBADD =
am_libaltivec_a_OBJECTS = detect.$(OBJEXT) motion.$(OBJEXT) \
	quantize.$(OBJEXT) benchmark.$(OBJEXT) amber.$(OBJEXT) \
	sad_00.$(OBJEXT) sad_01.$(OBJEXT) sad_10.$(OBJEXT) \
	sad_11.$(OBJEXT) bsad.$(OBJEXT) sub_mean_reduction.$(OBJEXT) \
	sumsq.$(OBJEXT) sumsq_sub22.$(OBJEXT) bsumsq.$(OBJEXT) \
	bsumsq_sub22.$(OBJEXT) build_sub44_mests.$(OBJEXT) \
	build_sub22_mests.$(OBJEXT) find_best_one_pel.$(OBJEXT) \
	fdct_idct.$(OBJEXT) quant_non_intra.$(OBJEXT) \
	quant_weight_coeff_sum.$(OBJEXT) variance.$(OBJEXT) \
	sub_pred.$(OBJEXT) add_pred.$(OBJEXT) pred_comp.$(OBJEXT) \
	subsample_image.$(OBJEXT)
libaltivec_a_OBJECTS = $(am_libaltivec_a_OBJECTS)

DEFS = -DHAVE_CONFIG_H
DEFAULT_INCLUDES =  -I. -I$(srcdir) -I$(top_builddir)
CPPFLAGS = 
LDFLAGS = 
LIBS = 
depcomp = $(SHELL) $(top_srcdir)/depcomp
DEP_FILES = $(DEPDIR)/add_pred.Po $(DEPDIR)/amber.Po \
	$(DEPDIR)/benchmark.Po $(DEPDIR)/bsad.Po \
	$(DEPDIR)/bsumsq.Po $(DEPDIR)/bsumsq_sub22.Po \
	$(DEPDIR)/build_sub22_mests.Po \
	$(DEPDIR)/build_sub44_mests.Po $(DEPDIR)/detect.Po \
	$(DEPDIR)/fdct_idct.Po \
	$(DEPDIR)/find_best_one_pel.Po $(DEPDIR)/motion.Po \
	$(DEPDIR)/pred_comp.Po $(DEPDIR)/quant_non_intra.Po \
	$(DEPDIR)/quant_weight_coeff_sum.Po \
	$(DEPDIR)/quantize.Po $(DEPDIR)/sad_00.Po \
	$(DEPDIR)/sad_01.Po $(DEPDIR)/sad_10.Po \
	$(DEPDIR)/sad_11.Po $(DEPDIR)/sub_mean_reduction.Po \
	$(DEPDIR)/sub_pred.Po $(DEPDIR)/subsample_image.Po \
	$(DEPDIR)/sumsq.Po $(DEPDIR)/sumsq_sub22.Po \
	$(DEPDIR)/variance.Po
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) \
	$(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
	$(AM_LDFLAGS) $(LDFLAGS) -o $@
CFLAGS = -mcpu=i686 -march=i686 -g -O2 -Wall -Wunused
DIST_SOURCES = $(libaltivec_a_SOURCES)
DIST_COMMON = README Makefile.am Makefile.in
SOURCES = $(libaltivec_a_SOURCES)

all: all-am

.SUFFIXES:
.SUFFIXES: .c .lo .o .obj

mostlyclean-libtool:
	-rm -f *.lo

clean-libtool:
	-rm -rf .libs _libs

distclean-libtool:
	-rm -f libtool
$(srcdir)/Makefile.in: # Makefile.am  $(top_srcdir)/configure.in $(ACLOCAL_M4)
	cd $(top_srcdir) && \
	  $(AUTOMAKE) --gnu  utils/altivec/Makefile
Makefile: # $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) && \
	  CONFIG_HEADERS= CONFIG_LINKS= \
	  CONFIG_FILES=$(subdir)/$@ $(SHELL) ./config.status

AR = ar

clean-noinstLIBRARIES:
	-test -z "$(noinst_LIBRARIES)" || rm -f $(noinst_LIBRARIES)
libaltivec.a: $(libaltivec_a_OBJECTS) $(libaltivec_a_DEPENDENCIES) 
	-rm -f libaltivec.a
	$(libaltivec_a_AR) libaltivec.a $(libaltivec_a_OBJECTS) $(libaltivec_a_LIBADD)
	$(RANLIB) libaltivec.a

mostlyclean-compile:
	-rm -f *.$(OBJEXT) core *.core

distclean-compile:
	-rm -f *.tab.c

include $(DEPDIR)/add_pred.Po
include $(DEPDIR)/amber.Po
include $(DEPDIR)/benchmark.Po
include $(DEPDIR)/bsad.Po
include $(DEPDIR)/bsumsq.Po
include $(DEPDIR)/bsumsq_sub22.Po
include $(DEPDIR)/build_sub22_mests.Po
include $(DEPDIR)/build_sub44_mests.Po
include $(DEPDIR)/detect.Po
include $(DEPDIR)/fdct_idct.Po
include $(DEPDIR)/find_best_one_pel.Po
include $(DEPDIR)/motion.Po
include $(DEPDIR)/pred_comp.Po
include $(DEPDIR)/quant_non_intra.Po
include $(DEPDIR)/quant_weight_coeff_sum.Po
include $(DEPDIR)/quantize.Po
include $(DEPDIR)/sad_00.Po
include $(DEPDIR)/sad_01.Po
include $(DEPDIR)/sad_10.Po
include $(DEPDIR)/sad_11.Po
include $(DEPDIR)/sub_mean_reduction.Po
include $(DEPDIR)/sub_pred.Po
include $(DEPDIR)/subsample_image.Po
include $(DEPDIR)/sumsq.Po
include $(DEPDIR)/sumsq_sub22.Po
include $(DEPDIR)/variance.Po

distclean-depend:
	-rm -rf $(DEPDIR)

.c.o:
	source='$<' object='$@' libtool=no \
	depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(COMPILE) -c `test -f $< || echo '$(srcdir)/'`$<

.c.obj:
	source='$<' object='$@' libtool=no \
	depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(COMPILE) -c `cygpath -w $<`

.c.lo:
	source='$<' object='$@' libtool=yes \
	depfile='$(DEPDIR)/$*.Plo' tmpdepfile='$(DEPDIR)/$*.TPlo' \
	$(CCDEPMODE) $(depcomp) \
	$(LTCOMPILE) -c -o $@ `test -f $< || echo '$(srcdir)/'`$<
CCDEPMODE = depmode=gcc
uninstall-info-am:

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP) $(TAGS_FILES)
	list='$(SOURCES) $(HEADERS) $(TAGS_FILES)'; \
	unique=`for i in $$list; do \
	    if test -f "$$i"; then echo $$i; else echo $(srcdir)/$$i; fi; \
	  done | \
	  $(AWK) '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	mkid -fID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) \
		$(TAGS_FILES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS) $(TAGS_FILES)'; \
	unique=`for i in $$list; do \
	    if test -f "$$i"; then echo $$i; else echo $(srcdir)/$$i; fi; \
	  done | \
	  $(AWK) '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || etags $(ETAGS_ARGS) $$tags  $$unique $(LISP)

GTAGS:
	here=`CDPATH=: && cd $(top_builddir) && pwd` \
	  && cd $(top_srcdir) \
	  && gtags -i $(GTAGS_ARGS) $$here

distclean-tags:
	-rm -f TAGS ID GTAGS GRTAGS GSYMS GPATH

DISTFILES = $(DIST_COMMON) $(DIST_SOURCES) $(TEXINFOS) $(EXTRA_DIST)

top_distdir = ../..
distdir = $(top_distdir)/$(PACKAGE)-$(VERSION)

distdir: $(DISTFILES)
	@for file in $(DISTFILES); do \
	  if test -f $$file; then d=.; else d=$(srcdir); fi; \
	  dir=`echo "$$file" | sed -e 's,/[^/]*$$,,'`; \
	  if test "$$dir" != "$$file" && test "$$dir" != "."; then \
	    $(mkinstalldirs) "$(distdir)/$$dir"; \
	  fi; \
	  if test -d $$d/$$file; then \
	    cp -pR $$d/$$file $(distdir) \
	    || exit 1; \
	  else \
	    test -f $(distdir)/$$file \
	    || cp -p $$d/$$file $(distdir)/$$file \
	    || exit 1; \
	  fi; \
	done
check-am: all-am
check: check-am
all-am: Makefile $(LIBRARIES)

installdirs:

install: install-am
install-exec: install-exec-am
install-data: install-data-am
uninstall: uninstall-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

installcheck: installcheck-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) INSTALL_PROGRAM="$(INSTALL_STRIP_PROGRAM)" \
	  `test -z '$(STRIP)' || \
	    echo "INSTALL_PROGRAM_ENV=STRIPPROG='$(STRIP)'"` install
mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES) stamp-h stamp-h[0-9]*

maintainer-clean-generic:
	@echo "This command is intended for maintainers to use"
	@echo "it deletes files that may require special tools to rebuild."
	-test -z "$(MAINTAINERCLEANFILES)" || rm -f $(MAINTAINERCLEANFILES)
clean: clean-am

clean-am: clean-generic clean-libtool clean-noinstLIBRARIES \
	mostlyclean-am

distclean: distclean-am

distclean-am: clean-am distclean-compile distclean-depend \
	distclean-generic distclean-libtool distclean-tags

dvi: dvi-am

dvi-am:

info: info-am

info-am:

install-data-am:

install-exec-am:

install-info: install-info-am

install-man:

installcheck-am:

maintainer-clean: maintainer-clean-am

maintainer-clean-am: distclean-am maintainer-clean-generic

mostlyclean: mostlyclean-am

mostlyclean-am: mostlyclean-compile mostlyclean-generic \
	mostlyclean-libtool

uninstall-am: uninstall-info-am

.PHONY: GTAGS all all-am check check-am clean clean-generic \
	clean-libtool clean-noinstLIBRARIES distclean distclean-compile \
	distclean-depend distclean-generic distclean-libtool \
	distclean-tags distdir dvi dvi-am info info-am install \
	install-am install-data install-data-am install-exec \
	install-exec-am install-info install-info-am install-man \
	install-strip installcheck installcheck-am installdirs \
	maintainer-clean maintainer-clean-generic mostlyclean \
	mostlyclean-compile mostlyclean-generic mostlyclean-libtool \
	tags uninstall uninstall-am uninstall-info-am

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
