# Process this file with Automake to produce Makefile.in

CC = g++

EXTRA_DIST = \
	README  	INSTRUCT	TODO \
	.cvsignore


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
    Makefile.am \
	main.cpp	\
	interact.cpp		interact.hpp \
	bits.cpp			bits.hpp	\
	buffer.cpp		buffer.hpp \
	videostrm.hpp	audiostrm.hpp	padstrm.hpp \
	videostrm_in.cpp	 padstrm.cpp \
	lpcmstrm_in.cpp mpastrm_in.cpp	ac3strm_in.cpp \
	dtsstrm_in.cpp \
	videostrm_out.cpp	audiostrm_out.cpp	\
	multiplexor.cpp	multiplexor.hpp	\
	systems.cpp		systems.hpp \
	vector.cpp	vector.hpp	mplexconsts.hpp \
	inputstrm.cpp inputstrm.hpp aunit.hpp \
	stillsstream.cpp stillsstream.hpp \
	stream_params.cpp stream_params.hpp \
        $(ZALPHA_FILES)

mplex_LDADD = $(top_builddir)/utils/libmjpegutils.a \
		@LIBGETOPT_LIB@ $(LIBM_LIBS) 

mplex_CXXFLAGS = $(ZALPHA_FLAGS)
