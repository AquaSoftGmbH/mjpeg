# Process this file with Automake to produce Makefile.in

CC = g++

EXTRA_DIST = \
	README  	INSTRUCT	TODO \
	.cvsignore


MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

bin_PROGRAMS = mplex

mplex_SOURCES = \
    Makefile.am \
	main.cpp	\
	interact.cpp		interact.hpp \
	bits.cpp			bits.hpp	\
	buffer.cpp		buffer.hpp \
	videostrm.hpp	audiostrm.hpp	padstrm.hpp \
	videostrm_in.cpp	 padstrm.cpp \
	lpcmstrm_in.cpp mpastrm_in.cpp	ac3strm_in.cpp \
	videostrm_out.cpp	audiostrm_out.cpp	\
	multiplexor.cpp	multiplexor.hpp	\
	systems.cpp		systems.hpp \
	vector.cpp	vector.hpp	mplexconsts.hpp \
	inputstrm.cpp inputstrm.hpp aunit.hpp \
	stillsstream.cpp stillsstream.hpp \
	stream_params.cpp stream_params.hpp

mplex_LDADD = $(top_builddir)/utils/libmjpegutils.a \
		-lstdc++ @LIBGETOPT_LIB@ $(LIBM_LIBS) 
