# Process this file with Automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = mjpeg_logging.c cpu_accel.c \
	                 mjpeg_logging.h mjpeg_types.h cpu_accel.h

EXTRA_DIST = format_codes.h .cvsignore
MAINTAINERCLEANFILES = Makefile.in
