#define _GNU_SOURCE 1			/* We make some use of C9X and POSIX and GNU
								 facilities... */

@TOP@
#define VERSION   "x.x.x"		/* Mjpeg tools release version */

/* Large file support ? */
#undef _FILE_OFFSET_BITS
#undef _LARGEFILE_SOURCE
#undef _LARGEFILE64_SOURCE

/* Define pthread lib stack size */
#undef HAVE_PTHREADSTACKSIZE

/* Define for an Intel based CPU */
#undef HAVE_X86CPU 	

/* For HAVEX86CPU: Define for availability of CMOV instruction (P6, P7
 * and Athlon cores).*/
#undef HAVE_CMOV

/* For HAVEX86CPU: Define if the installed GCC tool chain can generate
 * MMX instructions */
#undef HAVE_ASM_MMX

/* For HAVEX86CPU: Define if the installed GCC tool-chain can generate
 * 3DNow instructions */
#undef HAVE_ASM_3DNOW

/* For HAVEX86CPU: Define if the nasm assembler is available */
#undef HAVE_ASM_NASM

/* Define for a PowerPPC CPU */
#undef HAVE_PPCCPU

/* whether we're in linux or not (video4linux?) */
#undef HAVE_V4L

/* Define if the libpthread library is present */
#undef HAVE_LIBPTHREAD

/* Define if the quicktime for linux library is present */
#undef HAVE_LIBQUICKTIME
#undef HAVE_OPENQUICKTIME

/* Define if the libXxf86dga library is available */
#undef HAVE_LIBXXF86DGA

/* Define if the libmovtar library is available */
#undef HAVE_LIBMOVTAR

/* do we have some cool thing for 64bits integers? */
#undef PRID64_STRING_FORMAT

/* Define for software MJPEG playback */
#undef BUILD_MJPEG

/* Define for libDV and possibly YV12 support */
#undef LIBDV_PAL_YV12
#undef SUPPORT_READ_DV2
#undef SUPPORT_READ_YUV420
#undef LIBDV_PRE_0_9_5

/* whether we have avifile/gtk+/sdl */
#undef HAVE_AVIFILE
#undef AVIFILE_USE_DESTFMT
#undef HAVE_GTK
#undef HAVE_SDL

/* Name of package */
#define PACKAGE "mjpegtools"
#define MJPEGTOOLS 1

/* Define if disable assert() */
#undef NDEBUG
