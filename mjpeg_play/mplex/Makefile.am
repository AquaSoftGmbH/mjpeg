# Process this file with Automake to produce Makefile.in

EXTRA_DIST = \
	README  	INSTRUCT	TODO

INCLUDES = -I $(top_srcdir) -I $(top_srcdir)/utils

bin_PROGRAMS = mplex

mplex_SOURCES = \
	main.cc 		main.hh	\
	bits.cc 		bits.hh	\
	buffer.cc 		\
	inits.cc 		\
	inptstrm.cc 		\
	interact.cc		\
	multplex.cc 		\
	systems.cc 		\
	vector.cc 	vector.hh \
	inputstrm.cc inputstrm.hh aunit.hh mpegconsts.hh

mplex_LDADD = -L$(top_srcdir)/utils -lutils -lm 


