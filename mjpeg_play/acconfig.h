#define _GNU_SOURCE 1			/* We make some use of C9X and POSIX and GNU
								 facilities... */

@TOP@

#undef X86_CPU 				/* Compiling for x86 CPU */

#undef P6_CPU				/* Compiling for PPro upwards x86
                                   instruction set? */

#undef BUILD_QUICKTIME		/* Quicktime libs present? */

#undef BUILD_MJPEG			/* Mpeg present? */

#undef BUILD_MOVTAR			/* movtar libs present  */

#undef BUILD_PNG				/* PNG libs present */

#define VERSION   "x.x.x"		/* Mjpeg tools release version */
