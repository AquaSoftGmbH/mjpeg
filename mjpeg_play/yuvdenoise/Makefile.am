# Makefile.am for yuvdenoise (lavtools)

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I $(top_srcdir)/utils -I $(top_srcdir)

# (Ronald) do we have to do this?
AM_CFLAGS = -funroll-all-loops -finline-functions -ffast-math -frerun-cse-after-loop -frerun-loop-opt -fexpensive-optimizations -fmove-all-movables

bin_PROGRAMS = yuvdenoise

if HAVE_ALTIVEC
CPULIB = $(top_builddir)/utils/altivec/libaltivec.a
else # HAVE_ALTIVEC
if HAVE_ASM_NASM
if HAVE_ASM_MMX
CPULIB = $(top_builddir)/utils/mmxsse/libmmxsse.a
else # HAVE_ASM_NASM
CPULIB =
endif # HAVE_ASM_NASM
else # HAVE_ASM_MMX
CPULIB = 
endif # HAVE_ASM_MMX
endif # HAVE_ALTIVEC

yuvdenoise_SOURCES = \
	deinterlace.c \
	denoise.c \
	main.c \
	motion.c

noinst_HEADERS = \
	deinterlace.h \
	denoise.h \
	global.h \
	motion.h

yuvdenoise_LDADD = \
	$(top_builddir)/utils/libcpuaccel.a \
	$(top_builddir)/utils/libmjpegutils.a \
	$(CPULIB) @LIBM_LIBS@
