YUVFILTER =\
 libyuvthru.la\
 #

if USE_DLFCN_H
YSTLIBRARIES = $(top_srcdir)/mjpegtools/libyst.la
else
YSTLIBRARIES = $(YUVFILTER) $(top_srcdir)/mjpegtools/libyst.la
endif

LIBLDFLAGS = -release $(VERSION)

bin_PROGRAMS = yuvfilter
yuvfilter_SOURCES =\
 yuvfilter.c yuvfilter_static.c yuvfilter_static.h
yuvfilter_LDADD = $(YSTLIBRARIES) -L$(top_srcdir)/utils -lmjpegutils -ldl
yuvfilter_LDFLAGS = -export-dynamic

pkglib_LTLIBRARIES = $(YUVFILTER)
libyuvthru_la_SOURCES = yuvthru.c
libyuvthru_la_LDFLAGS = $(LIBLDFLAGS)

O		= .$(if $(OBJEXT),$(OBJEXT),o)
X		= $(EXEEXT)

READERS		=
WRITERS		=
FILTERS		= yuvycsnoise$X yuvkineco$X
COREOBJS	= addtask$O
STDOBJS		= $(COREOBJS) alloctask$O initframe$O putframe$O yuvstdin$O yuvstdout$O
CORECFLAGS	= $(DEBUG) -O2 -Wall -Wpointer-arith -Wcast-align -Wwrite-strings
CPPFLAGS	= $(INCLUDES)
CFLAGS		= $(CORECFLAGS) -Wcast-qual
L		= libyuvfilters.a
BINARIES	= $(READERS) $(WRITERS) $(FILTERS)
READERMAINS	= $(READERS:$X=_main$O)
WRITERMAINS	= $(WRITERS:$X=_main$O)
FILTERMAINS	= $(FILTERS:$X=_main$O)
MAINS		= $(BINARIES:$X=_main$O)

bin_SCRIPTS	= $(BINARIES)
INCLUDES	= -I$(top_srcdir) -I$(top_srcdir)/mjpegtools -I$(top_srcdir)/utils
DEPENDENCIES	= $(top_srcdir)/utils/libmjpegutils.a
LDADD		= -L$(top_srcdir)/utils -lmjpegutils
CLEANFILES	= $(BINARIES) $(MAINS) $L
YFSOURCES	= main.c $(BINARIES:$X=.c) $(STDOBJS:$O=.c) yuvfilters.h
EXTRA_DIST	= README.yuvkineco README.2-3pulldown .cvsignore $(YFSOURCES)
MAINTAINERCLEANFILES = Makefile.in

#%$X: %_main$O $L(%$O $(STDOBJS)) $(DEPENDENCIES)
#	$(CC) $(LDFLAGS) -o $@ $< $L $(LDADD)
yuvycsnoise$X: yuvycsnoise_main$O $L(yuvycsnoise$O $(STDOBJS)) $(DEPENDENCIES)
	$(CC) $(LDFLAGS) -o $@ $< $L $(LDADD)
yuvkineco$X: yuvkineco_main$O $L(yuvkineco$O $(STDOBJS)) $(DEPENDENCIES)
	$(CC) $(LDFLAGS) -o $@ $< $L $(LDADD)
yuvkineco$X: $L(yuvkineco$O)
yuvycsnoise$X: $L(yuvycsnoise$O)

$(MAINS): main.c yuvfilters.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<
$(READERMAINS): override CPPFLAGS += -DREADER=$(@:_main$O=)
$(WRITERMAINS): override CPPFLAGS += -DWRITER=$(@:_main$O=)
$(FILTERMAINS): override CPPFLAGS += -DFILTER=$(@:_main$O=)

$L(%$O): %.c yuvfilters.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $*$O -c $<
	$(AR) r $L $*$O
	$(RM) $*$O
$L($(COREOBJS)): override CFLAGS = $(CORECFLAGS)
