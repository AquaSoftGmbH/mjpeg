# Process this file with Automake to produce Makefile.in

EXTRA_DIST = \
	README \
	INSTRUCT \
	TODO

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

bin_PROGRAMS = mplex

lib_LTLIBRARIES = libmplex2.la

if HAVE_ZALPHA
  ZALPHA_FILES = zalphastrm.hpp zalphastrm_in.cpp zalphastrm_out.cpp
  ZALPHA_FLAGS = -DZALPHA
else
  ZALPHA_FILES =
  ZALPHA_FLAGS =
endif


libmplex2_la_SOURCES = \
	ac3strm_in.cpp \
	audiostrm_out.cpp \
	bits.cpp \
	buffer.cpp \
	dtsstrm_in.cpp \
	inputstrm.cpp \
	interact.cpp \
	lpcmstrm_in.cpp \
	mpastrm_in.cpp \
	multiplexor.cpp \
	padstrm.cpp \
	stillsstream.cpp \
	stream_params.cpp \
	systems.cpp \
	vector.cpp \
	videostrm_in.cpp \
	videostrm_out.cpp \
	$(ZALPHA_FILES)

libmplex_includedir = $(pkgincludedir)/mplex

libmplex_include_HEADERS = \
	audiostrm.hpp \
	aunit.hpp \
	bits.hpp \
	buffer.hpp \
	inputstrm.hpp \
	interact.hpp \
	mplexconsts.hpp \
	multiplexor.hpp \
	outputstrm.hpp \
	padstrm.hpp \
	stillsstream.hpp \
	stream_params.hpp \
	systems.hpp \
	vector.hpp \
	videostrm.hpp

libmplex2_la_LDFLAGS =  \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic 

libmplex2_la_LIBADD = \
	$(top_builddir)/utils/libmjpegutils.a

libmplex2_la_CXXFLAGS = $(ZALPHA_FLAGS)

mplex_SOURCES = main.cpp 

mplex_LDADD = \
	$(top_builddir)/mplex/libmplex2.la \
	@LIBGETOPT_LIB@ $(LIBM_LIBS) 
