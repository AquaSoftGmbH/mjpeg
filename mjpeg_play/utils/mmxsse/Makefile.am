# Process this file with Automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(ALTIVEC_CFLAGS)

lib_LIBRARIES = libmmxsse.a
noinst_LIBRARIES = libmmxsse.a

libmmxsse_a_SOURCES = \
	motion.c \
	mblock_sumsq_mmx.s \
	mblock_bsumsq_mmx.s \
	mblock_bsad_mmx.s \
	mblock_sad_mmxe.s \
	mblock_sad_mmx.s \
	build_sub44_mests.c \
	build_sub22_mests.c \
	find_best_one_pel.c \
    mblock_sub44_sads_x86.c \
	mblock_sub44_sads_x86.h

EXTRA_DIST = .cvsignore  mblock_sub44_sads_x86_h.c 

MAINTAINERCLEANFILES = Makefile.in


%.o: %.s ; 	$(AS) $(ASFLAGS) -o $@ $<

