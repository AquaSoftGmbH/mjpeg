# Process this file with Automake to produce Makefile.in

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
	inptstrm.cc		\
	multplex.cc	outputstream.hh	\
	systems.cc		systems.hh \
	vector.cc	vector.hh	mplexconsts.hh \
	inputstrm.cc inputstrm.hh aunit.hh \
	stillsstream.cc stillsstream.hh

mplex_LDADD = -L$(top_srcdir)/utils -lmjpegutils $(LIBM_LIBS) 


