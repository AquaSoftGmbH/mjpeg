# Process this file with Automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

noinst_LIBRARIES = libmmxsse.a

libmmxsse_a_SOURCES = \
	build_sub44_mests.c \
	build_sub22_mests.c \
	find_best_one_pel.c \
	mblock_bsad_mmx.s \
	mblock_bsumsq_mmx.s \
	mblock_sad_mmx.s \
	mblock_sad_mmxe.s \
	mblock_sub44_sads_x86.c \
	mblock_sumsq_mmx.s \
	motion.c

noinst_HEADERS = \
	mblock_sub44_sads_x86.h \
	mmxsse_motion.h

EXTRA_DIST = \
	mblock_sub44_sads_x86_h.c 

MAINTAINERCLEANFILES = Makefile.in

%.o: %.s ; 	$(AS) $(ASFLAGS) -o $@ $<
