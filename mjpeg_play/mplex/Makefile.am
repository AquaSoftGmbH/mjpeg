# Process this file with Automake to produce Makefile.in

EXTRA_DIST = \
	README \
	INSTRUCT \
	TODO

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

bin_PROGRAMS = mplex

if HAVE_ZALPHA
  ZALPHA_FILES = zalphastrm.hpp zalphastrm_in.cpp zalphastrm_out.cpp
  ZALPHA_FLAGS = -DZALPHA
else
  ZALPHA_FILES =
  ZALPHA_FLAGS =
endif

mplex_SOURCES = \
	ac3strm_in.cpp \
	audiostrm_out.cpp \
	bits.cpp \
	buffer.cpp \
	dtsstrm_in.cpp \
	inputstrm.cpp \
	interact.cpp \
	lpcmstrm_in.cpp \
	main.cpp \
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

noinst_HEADERS = \
	audiostrm.hpp \
	aunit.hpp \
	bits.hpp \
	buffer.hpp \
	inputstrm.hpp \
	interact.hpp \
	mplexconsts.hpp \
	multiplexor.hpp \
	padstrm.hpp \
	stillsstream.hpp \
	stream_params.hpp \
	systems.hpp \
	vector.hpp \
	videostrm.hpp

mplex_LDADD = \
	$(top_builddir)/utils/libmjpegutils.a \
	@LIBGETOPT_LIB@ $(LIBM_LIBS) 

mplex_CXXFLAGS = $(ZALPHA_FLAGS)
