# Makefile for y4mdenoise

MAINTAINERCLEANFILES = Makefile.in

y4mdenoise_CFLAGS = -DNDEBUG -finline-functions -frename-registers
y4mdenoise_CXXFLAGS = -DNDEBUG -finline-functions -frename-registers

INCLUDES =  -I$(top_srcdir) -I$(includedir) -I$(top_srcdir)/utils

LIBMJPEGUTILS = $(top_builddir)/utils/libmjpegutils.la

bin_PROGRAMS = y4mdenoise

noinst_HEADERS = \
	Allocator.hh \
	BitmapRegion2D.hh \
	DoublyLinkedList.hh \
	Limits.hh \
	MotionSearcher.hh \
	ReferenceFrame.hh \
	Region2D.hh \
	SearchBorder.hh \
	SearchWindow.hh \
	Set.hh \
	SetRegion2D.hh \
	SkipList.hh \
	Status_t.h \
	TemplateLib.hh \
	newdenoise.hh

y4mdenoise_SOURCES = main.c newdenoise.cc
y4mdenoise_LDADD = $(LIBMJPEGUTILS)
