# Process this file with Automake to produce Makefile.in

CC = g++

EXTRA_DIST = \
	README  	INSTRUCT	TODO \
	.cvsignore
#	mpeg_systems_paper_0.99.ps.gz

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

bin_PROGRAMS = mplex

mplex_SOURCES = \
	main.cc	\
	interact.cc		interact.hh \
	bits.cc			bits.hh	\
	buffer.cc		buffer.hh \
	videostrm.hh	audiostrm.hh	padstrm.hh \
	videostrm_in.cc	 padstrm.cc \
	lpcmstrm_in.cc mpastrm_in.cc	ac3strm_in.cc \
	videostrm_out.cc	audiostrm_out.cc	\
	multiplexor.cc	multiplexor.hh	\
	systems.cc		systems.hh \
	vector.cc	vector.hh	mplexconsts.hh \
	inputstrm.cc inputstrm.hh aunit.hh \
	stillsstream.cc stillsstream.hh \
	zalphastrm.hh zalphastrm_in.cc zalphastrm_out.cc \
	stream_params.cc stream_params.hh

mplex_LDADD = -L$(top_builddir)/utils -lmjpegutils -lstdc++ @LIBGETOPT_LIB@ $(LIBM_LIBS) 


