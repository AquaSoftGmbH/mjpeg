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

/* Define if the installed GCC tool chain can generate MMX instructions */
#undef HAVE_ASM_MMX

/* Define if the installed GCC tool-chain can generate 3DNow instructions */
#undef HAVE_ASM_3DNOW

/* Define if the nasm assembler is available */
#undef HAVE_ASM_NASM

/* Define if the libpthread library is present */
#undef HAVE_LIBPTHREAD

/* Define if the quicktime for linux library is present */
#undef HAVE_LIBQUICKTIME
#undef HAVE_OPENQUICKTIME

/* Define if the libXxf86dga library is available */
#undef HAVE_LIBXXF86DGA

/* Define if the libmovtar library is available */
#undef HAVE_LIBMOVTAR

/* Define for availability of CMOV instruction (P6, P7 and  Athlon cores).*/
#undef HAVE_CMOV

/* Define for an Intel based CPU */
#undef HAVE_X86CPU 	

/* Define for software MJPEG playback */
#undef BUILD_MJPEG

/* Define for libDV and possibly YV12 support */
#undef LIBDV_PAL_YV12
#undef SUPPORT_READ_DV2
#undef SUPPORT_READ_YUV420

/* whether we have avifile/gtk+ */
#undef HAVE_AVIFILE
#undef HAVE_GTK

/* Name of package */
#define PACKAGE "mjpegtools"
#define MJPEGTOOLS 1

/* Define if disable assert() */
#undef NDEBUG

/* Define stream header formats supported by libyst */
#undef USE_Y4M_PLAIN_FIELD
#undef USE_Y4M_TAGGED_FIELD
#undef USE_Y4M_NAMED_FIELD
#undef USE_Y4M_BINARY_HEADER
#undef USE_VIDEO_RAW

/* Define if libyst output stream header with "named" fields by default */
#undef WRITE_Y4M_NAMED_FIELD

/* Define streaming media supported by libyst */
#undef USE_MEDIA_FILE
#undef USE_MEDIA_STACK
#undef USE_MEDIA_GST

/* Define if build libyst media drivers and yuvfilters as plugin modules */
#undef USE_DLFCN_H
