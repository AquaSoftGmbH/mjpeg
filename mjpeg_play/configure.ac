dnl Process this file with autoconf to produce a configure script.
AC_INIT([mjpegtools],[1.7.0],[mjpeg-developer@lists.sourceforge.net])
AC_PREREQ(2.57)
AC_CONFIG_SRCDIR([lavtools/lav_io.c])

MJPEG_MAJOR_VERSION=1
MJPEG_MINOR_VERSION=7
MJPEG_MICRO_VERSION=0
MJPEG_VERSION=$MJPEG_MAJOR_VERSION.$MJPEG_MINOR_VERSION.$MJPEG_MICRO_VERSION

AC_CONFIG_HEADERS([config.h])
dnl Create an installable config header
AX_CONFIG_PREFIXED_HEADER([mjpegtools-config.h], [MJPEGTOOLS], [config.h])

AC_CANONICAL_HOST
dnl CANONICAL-TARGET is only there because SDL's sdl.m4 is broken.  bleah.
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([1.6])
AM_MAINTAINER_MODE


# libtool versioning
LT_RELEASE=$MJPEG_MAJOR_VERSION.$MJPEG_MINOR_VERSION
LT_CURRENT=$MJPEG_MICRO_VERSION
LT_REVISION=$MJPEG_MICRO_VERSION
LT_AGE=$MJPEG_MICRO_VERSION
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl **********************************************************************
dnl Options

AC_ARG_ENABLE(compile-warnings,     
              AC_HELP_STRING([--enable-compile-warnings],
                             [Turn on compiler warnings.]))
AC_ARG_ENABLE(warnings_as_errors,   
              AC_HELP_STRING([--enable-warnings_as_errors],
                             [Compiler warnings are errors.]))
AC_ARG_ENABLE(simd-accel,
              AC_HELP_STRING([--enable-simd-accel],
                             [use SIMD multimedia instructions
                              (MMX, AltiVec, etc.) if possible]))
AC_ARG_ENABLE(zalpha,
              AC_HELP_STRING([--enable-zalpha],
                             [include MPEG Z/Alpha support in mplex (see
                              http://www.geofront.se/products_software_mza.php)]))

AC_ARG_WITH(extra-cflags,
            AC_HELP_STRING([--with-extra-cflags=flags],
                           [Options to be added to CFLAGS (optional)]))
AC_ARG_WITH(libquicktime,
            AC_HELP_STRING([--without-libquicktime],
                           [Do not use libquicktime.]),
            [], [with_libquicktime=yes])
AC_ARG_WITH(libdv,
            AC_HELP_STRING([--without-libdv], [Do not use libdv.]),
            [], [with_libdv=yes])
AC_ARG_WITH(libpng,
            AC_HELP_STRING([--without-libpng], [Do not use libpng.]),
            [], [with_libpng=yes])
AC_ARG_WITH(jpeg-mmx,
            AC_HELP_STRING([--with-jpeg-mmx=PFX],
                           [Prefix where jpeg-mmx is installed (optional).]))

AC_GNU_SOURCE

dnl Initialize libtool
AC_PROG_LIBTOOL

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_SYS_LARGEFILE

dnl configure AS and ASFLAGS... a pity nasm doesn't take  flags similar to cc
dnl CCAS=nasm
dnl CCASFLAGS=
AM_PROG_AS

if test x"$with_extra_cflags" != "x"; then
   CFLAGS="$CFLAGS $with_extra_cflags"
   CXXFLAGS="$CXXFLAGS $with_extra_cflags"
fi

dnl maddog:  What about CHECK-ALLOCA????
AC_CHECK_HEADERS([fenv.h stdint.h inttypes.h sys/types.h getopt.h alloca.h sys/soundcard.h malloc.h])

AX_PREFIXED_DEFINE([MJPEGTOOLS], [HAVE_STDINT_H])
AX_PREFIXED_DEFINE([MJPEGTOOLS], [HAVE_INTTYPES_H])

#maddog:  check for math lib, and add it to LIBS (needed for fmax, lround...)
#maddog:  (How does this interact with cpml stuff below???????)
AC_CHECK_LIB([m],[sin])
AC_CHECK_FUNCS([posix_memalign memalign fmax lround])

#maddog:  FIX UP?????????????
AC_CHECK_FUNC(getopt_long,
              [AC_DEFINE(HAVE_GETOPT_LONG, 1, [long getopt support])],
              [ # FreeBSD and BSD/OS have a gnugetopt library for this:
               AC_CHECK_LIB([gnugetopt], [getopt_long],
                            [AC_DEFINE(HAVE_GETOPT_LONG, 1,
                                       [long getopt support])
                             LIBGETOPT_LIB="-lgnugetopt"])
              ])
AC_SUBST(LIBGETOPT_LIB)

AC_TYPE_SIGNAL

AC_MSG_NOTICE([The host is $host  $host_cpu  $host_vendor  $host_os .])

dnl test for alpha architecture, and Compaq Math Library (cpml) -- much faster
dnl than GNU libm on this arch.
LIBM_LIBS="-lm "
AC_SUBST(LIBM_LIBS)
AC_MSG_CHECKING([for alpha architecture])
case $host in
	alpha*-*-linux-*)
	    AC_MSG_RESULT([yes])
	    AC_CHECK_LIB(cpml, asin,
                         [ LIBM_LIBS="-lcpml"
                           have_cpml=true ],,)
            ;;
        *)
            AC_MSG_RESULT([no])
            ;;
esac

dnl *********************************************************************
dnl Some host-specific tweaks:
dnl  o OS-X/Darwin needs "-no-cpp-precomp" to avoid compile errors.
dnl *********************************************************************
case $host in
  *-apple-darwin*)
   CFLAGS="$CFLAGS -no-cpp-precomp"
   ;;
esac

dnl *********************************************************************
dnl Test whether we're linux or not. If so, define HAVE_V4L and build the
dnl video4linux/hardware dependent tools. Else, undefine HAVE_V4L

have_video4linux=false
case $host in
  *-*-linux*)
  AC_CHECK_HEADER(linux/videodev.h,
                  [have_video4linux=true
                   AC_DEFINE(HAVE_V4L, 1,
		             [Building for Linux - using the video4linux API])],
                  [AC_MSG_ERROR([videodev.h not found - please install the linux kernel headers])])
   ;;
  *)
  AC_MSG_WARN([Did not find linux platform - video4linux functionality will be disabled])
   ;;
esac
AM_CONDITIONAL(HAVE_V4L, test x$have_video4linux = xtrue)


dnl *********************************************************************
dnl Figure out how to build with POSIX threads.
dnl (Creates PTHREAD_LIBS, PTHREAD_CFLAGS, PTHREAD_CC,
dnl  and defines HAVE_PTHREAD.)
dnl *********************************************************************
ACX_PTHREAD
dnl maddog: The following matches the original pthread-checking behavior
dnl         in this script, i.e. "If a CFLAG is necessary, use it for 
dnl         *everything*."  Hopefully, any CFLAGs will eventually go away.
dnl         The other option is to stick PTHREAD_CFLAGS/CC in Makefile.am....
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

oldLIBS="$LIBS"
LIBS="$LIBS $PTHREAD_LIBS"
AC_CHECK_FUNC(pthread_attr_getstacksize,
              [AC_DEFINE(HAVE_PTHREADSTACKSIZE, 1,
                         [pthread stack sizes accesible])])
LIBS="$oldLIBS"

dnl *********************************************************************
dnl Check for the special mmx accelerated jpeg library
dnl At the end of this mess, JPEG_LIBS will contain the libraries and 
dnl flags needed to link with the jpeg library and JPEG_CFLAGS will 
dnl have the needed flags to compile against said library
AC_SUBST(JPEG_LIBS)
AC_SUBST(JPEG_CFLAGS)
have_jpeg=false
if test x$with_jpeg_mmx = xyes ; then
  AC_MSG_ERROR([
*** A directory must be specified for --with-jpeg-mmx option.])
fi
if test x$with_jpeg_mmx = x ; then
  dnl Special case for building .deb's
  if test -d ../jpeg-mmx ; then
    with_jpeg_mmx=`pwd`/../jpeg-mmx
  else
    with_jpeg_mmx=/usr/local/src/jpeg-mmx
  fi
fi

dnl
dnl Look for the installed/specified copy
dnl

OLD_CFLAGS="$CFLAGS"
OLD_LIBS="$LIBS"
LIBS="$LIBS -L$with_jpeg_mmx"
CFLAGS="$CFLAGS -I$with_jpeg_mmx"
AC_CHECK_LIB(jpeg-mmx, jpeg_start_compress,
   [ JPEG_LIBS="$LIBS -ljpeg-mmx"
     JPEG_CFLAGS="-I$with_jpeg_mmx"
       have_jpeg=true ],,)
  LIBS="$OLD_LIBS"
  CFLAGS="$OLD_CFLAGS"

dnl 
dnl Look for _a_ jpeg lib that will work.
dnl
if test x$have_jpeg = xfalse ; then
  AC_CHECK_HEADER(jpeglib.h,,
    AC_MSG_ERROR([jpeglib.h not found - please install the libjpeg headers]))
  AC_CHECK_LIB(jpeg, jpeg_start_compress,
    [ JPEG_LIBS="-ljpeg"
      JPEG_CFLAGS=""
      have_jpeg=true ],,)
fi

have_libquicktime=false
if test x$with_libquicktime != xno ; then
  AC_PATH_LIB_LIBCONFIG
  AC_PATH_LIB(libquicktime, [], [], [lqt-config], [],
              [have_libquicktime=true
               AC_DEFINE(HAVE_LIBQUICKTIME, 1, [libquicktime is present])])
fi

dnl *********************************************************************
dnl Check for PNG library
dnl   (creates LIBPNG_CFLAGS, LIBPNG_LIBS)
dnl *********************************************************************
have_libpng=false
if test x$with_libpng != xno ; then
  PKG_CHECK_MODULES(LIBPNG, [libpng], [have_libpng=true], [have_libpng=false])
fi
AM_CONDITIONAL(HAVE_LIBPNG, test x$have_libpng = xtrue)

dnl *********************************************************************
dnl Check for libdv
dnl   (creates LIBDV_CFLAGS, LIBDV_LIBS; defines HAVE_LIBDV)
dnl *********************************************************************
have_libdv=false
if test x$with_libdv != xno ; then
  PKG_CHECK_MODULES(LIBDV, [libdv >= 0.9],
                    [have_libdv=true
                     AC_DEFINE(HAVE_LIBDV, 1, [libdv is present])],
                    [have_libdv=false])
fi

dnl *********************************************************************
dnl  Look for X Window System.
dnl   (creates X_CFLAGS, X_PRE_LIBS, X_EXTRA_LIBS, X_LIBS;
dnl    defines X_DISPLAY_MISSING if no X)
dnl *********************************************************************
AC_PATH_XTRA

dnl maddog:  even better -- all that other stuff was unnecessary.
AM_CONDITIONAL(HAVE_X, test x$have_x = xyes)

dnl ********************************************************************
dnl Check to see if __progname is provided by the system
dnl ********************************************************************
AC_CACHE_CHECK([for __progname],
               [mjt_cv_extern___progname],
               [AC_LINK_IFELSE([AC_LANG_PROGRAM([[]],
                                                [[extern char *__progname;
                                                  puts(__progname);]])],
                               [mjt_cv_extern___progname=yes],
			       [mjt_cv_extern___progname=no])])
if test x$mjt_cv_extern___progname = xyes ; then
  AC_DEFINE(HAVE___PROGNAME, 1, [Is __progname defined by system?])
fi

dnl ********************************************************************
dnl Check for DGA (for v4l-conf)
dnl ********************************************************************
AC_SUBST(V4LCONF_LIBS)
AC_CHECK_LIB(Xxf86dga, XF86DGAQueryExtension,
	V4LCONF_LIBS="$X_LIBS -lXxf86dga" 
	AC_DEFINE(HAVE_LIBXXF86DGA,1, [libxf86dga is present]),,
	$X_LIBS $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS)

dnl ********************************************************************
dnl Check for Gtk+/glib (for glav)
dnl   (defines GTK_CFLAGS and GTK_LIBS)
dnl ********************************************************************
AM_PATH_GTK(1.2.0,
            [have_gtk=true
             AC_DEFINE(HAVE_GTK, 1, [Gtk library present]) ],
            [have_gtk=false])
AM_CONDITIONAL(HAVE_GTK, test x$have_gtk = xtrue)

dnl ********************************************************************
dnl Check for the SDL library (for software playback)
dnl   (defines SDL_CFLAGS and SDL_LIBS)
dnl ********************************************************************
AM_PATH_SDL(1.1.3,
            [have_sdl=true
             AC_DEFINE(HAVE_SDL, 1, [SDL library present]) ],
            [have_sdl=false])
AM_CONDITIONAL(HAVE_SDL, test x$have_sdl = xtrue)

dnl ********************************************************************
dnl Test for MMX support if an IA32 platform.  If on a PPC then look for
dnl Altivec support.  For the IA32 platform see if gcc understands inline 
dnl MMX instructions and also make sure nasm is present for the files that 
dnl need that. Both are checked for for the maximum flexiblity.
dnl
dnl TODO: host = non intel, target = intel <blah> do the checks also?
dnl       Remember we are checking that the toolchain can generate the
dnl       code, not whether the host can execute the code, thats done
dnl       at run time with the exception of the SSE code.

have_asm_mmx=false
have_asm_nasm=false
have_x86cpu=false
have_altivec=false
AC_MSG_CHECKING([Architecture])
case $host_cpu in
  i[[3-7]]86)
    AC_MSG_RESULT(IA32)
    AC_DEFINE(HAVE_X86CPU,1, [Compiling for x86 architecture CPU])
    have_x86cpu=true
    PROGRAM_NOPIC="-fno-PIC"
    ;;
  powerpc)
    AC_MSG_RESULT(PowerPC)
    AC_DEFINE(HAVE_PPCCPU,1, [Compiling for PowerPC CPU])
    have_ppccpu=true
    PROGRAM_NOPIC="-mdynamic-no-pic"
    ;;
  *)
    AC_MSG_RESULT(unknown)
    PROGRAM_NOPIC=""
    ;;
esac

dnl This flag is used for PROGRAMS not SHARED LIBRARIES.  PIC code is required
dnl for shared libraries but is slower than non-pic code.  Compute bound 
dnl programs such as yuvdenoise and y4mspatialfilter can use this flag by
dnl simply adding programname_CFLAGS=@PROGRAM_NOPIC@ to the Makefile.am

AC_SUBST(PROGRAM_NOPIC)

if test "$enable_simd_accel" != "false" -a "$enable_simd_accel" != "no"
then
  if test x$have_x86cpu = xtrue
  then
      AC_MSG_CHECKING([if C compiler accepts inline MMX assembly])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
                                         [[__asm__ __volatile__("emms":::"memory");]])],
                        [have_asm_mmx=true],
			[have_asm_mmx=false])
      if test $have_asm_mmx = true; then
	 AC_MSG_RESULT(yes)
      else
	 AC_MSG_RESULT(no)
      fi

      AC_PATH_PROGS(AS, nasm nasmw)
      if test x$AS = x; then
	 AC_MSG_WARN(*** nasm not found PERFORMANCE WILL BE CRAP ***)
      else
	 have_asm_nasm=true
      fi
      if test $have_asm_mmx = true; then
	 AC_DEFINE(HAVE_ASM_MMX,1,[Inline MMX assembly accepted by C compiler])
      fi
      if test $have_asm_nasm = true; then
	 ASFLAGS="-f elf"
	 AC_DEFINE(HAVE_ASM_NASM,1,[nasm assembler found])
	 AC_SUBST(ASFLAGS)
      fi
  fi

  if test x$have_ppccpu = xtrue
  then
    AC_MSG_CHECKING([compiler support for AltiVec])
    cat > conftest.c <<EOF
#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif
int main() {
    union { vector signed int v;
            signed int i;
          } vi;
    vi.v = vec_splat_s32(1);
    return vi.i;
}
EOF
    ALTIVEC_CFLAGS=""
    if $CC -o conftest conftest.c >/dev/null 2>&1; then
      have_altivec=true
    elif $CC $CFLAGS -faltivec -o conftest conftest.c >/dev/null 2>&1; then
      # Mac OS X style
      have_altivec=true
      ALTIVEC_CFLAGS="-faltivec"
    elif $CC $CFLAGS -fvec -o conftest conftest.c >/dev/null 2>&1; then
      # Motorola style
      have_altivec=true
      ALTIVEC_CFLAGS="-fvec"
    elif $CC $CFLAGS -DHAVE_ALTIVEC_H=1 -maltivec -mabi=altivec -o conftest conftest.c >/dev/null 2>&1; then
      # gcc 3.1 style
      have_altivec=true
      ALTIVEC_CFLAGS="-DHAVE_ALTIVEC_H=1 -maltivec -mabi=altivec"
    fi
    rm -f conftest*;
  
    if test x$have_altivec = xtrue
    then
      # add -O3 flag
      ALTIVEC_CFLAGS="-O3 ${ALTIVEC_CFLAGS}"
      AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_ALTIVEC,1,
				[Inline PPC Altivec primitives available])
      AC_SUBST(ALTIVEC_CFLAGS)
    else
      AC_MSG_RESULT(no)
    fi
  fi
fi

dnl *********************************************************************
dnl For x86 and ppc machines check which code model we should use.  This is
dnl done with the helper script cpuinfo.sh because the logic to do the job
dnl correctly is complex and lengthy (about as long as the entire configure.ac
dnl file).

if test x$have_x86cpu = xtrue -o x$have_ppccpu = xtrue; then
  if test x$host_alias != x; then
    AC_MSG_CHECKING(sub-architecture settings)
    if test x$have_x86cpu = xtrue; then
       ARCHFLAGS="-march=$host_cpu -mcpu=$host_cpu"
    fi
  else
    if test ! -r $srcdir/cpuinfo.sh; then
       AC_MSG_ERROR([cpuinfo.sh script not found - do a cvs update])
    fi
    AC_MSG_CHECKING(sub-architecture settings)
    chmod +x $srcdir/cpuinfo.sh
    if test x$have_x86cpu = xtrue; then
       ARCHFLAGS=`$srcdir/cpuinfo.sh x86`
    elif test x$have_ppccpu = xtrue; then
       ARCHFLAGS=`$srcdir/cpuinfo.sh ppc`
    fi
  fi
  AC_MSG_RESULT($ARCHFLAGS)
fi

dnl *********************************************************************
dnl Check if Z/Alpha has been enabled. 

use_zalpha=false
if test "x$enable_zalpha" = "xyes" ; then
  use_zalpha=true
fi
AM_CONDITIONAL(HAVE_ZALPHA, test x$use_zalpha = xtrue)

dnl **********************************************************************
dnl All the conditional stuff for the Makefiles

AM_CONDITIONAL(HAVE_ASM_MMX, test x$have_asm_mmx = xtrue)
AM_CONDITIONAL(HAVE_ASM_NASM, test x$have_asm_nasm = xtrue)
AM_CONDITIONAL(HAVE_X86CPU, test x$have_x86cpu = xtrue)
AM_CONDITIONAL(HAVE_PPCCPU, test x$have_ppccpu = xtrue)
AM_CONDITIONAL(HAVE_ALTIVEC, test x$have_altivec = xtrue)
have_v4l_or_sdl=false
if test x$have_video4linux = xtrue; then
  have_v4l_or_sdl=true
else
  if test x$have_sdl = xtrue; then
    have_v4l_or_sdl=true
  fi
fi
AM_CONDITIONAL(HAVE_V4L_OR_SDL, test x$have_v4l_or_sdl = xtrue)

dnl *********************************************************************
dnl Check for what warnings we want gcc to use and adjust the CFLAGS
dnl as needed. This only works for GCC.
dnl We do this last as it can screw up library detection etc...

dnl maddog:  check out AC-COMPILE-WARNINGS ???????

CFLAGS="$ARCHFLAGS $CFLAGS"
CXXFLAGS="$ARCHFLAGS $CXXFLAGS"

if test "x${GCC}" != "xyes" ; then
  enable_compile_warnings=no
fi
AC_MSG_CHECKING(what warning flags to pass to the C compiler)
warnCFLAGS=
warnCXXFLAGS=
if test "x$enable_compile_warnings" != "xno" ; then
 if test "x$GCC" = "xyes" ; then
    case "$CFLAGS" in
     *-Wall*) ;;
     *) warnCFLAGS="-Wall -Wunused" ;;
    esac
  if test "x$enable_compile_warnings" = "xyes" ; then
    warnCFLAGS="$warnCFLAGS -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith -Wcast-align -Wwrite-strings -Wcast-qual"
    warnCXXFLAGS="$warnCXXFLAGS -Wmissing-prototypes  -Wpointer-arith -Wcast-align -Wwrite-strings -Wcast-qual"
  fi
  if test "x$enable_warnings_as_errors" = "xyes" ; then
    warnCFLAGS="$warnCFLAGS -Werror"
    warnCXXFLAGS="$warnCXXFLAGS -Werror"
  fi
 fi
fi
AC_MSG_RESULT($warnCFLAGS)
AC_MSG_RESULT($warnCXXFLAGS)
if test "x$cflags_set" != "xyes" ; then
  CFLAGS="$CFLAGS $warnCFLAGS"
  CXXFLAGS="$CXXFLAGS $warnCXXFLAGS"
  cflags_set=yes
fi

AC_ARG_ENABLE(assert,
              AC_HELP_STRING([--disable-assert],
                             [Disable assertions (compile with NDEBUG).]),
 [ case "$enableval" in yes|no) : ;; *) AC_MSG_ERROR(bad value for --enable-assert)                ;; esac ],
 [ enable_assert=yes ])

if test x"$enable_assert"                 = x"no"; then NDEBUG=1;                fi

AC_DEFINE(MJPEGTOOLS,1, [Building mjpegtools])

case x"$NDEBUG"                in x"1") 
	AC_DEFINE(NDEBUG,1, [Optimised no DEBUG build, assertions disabled])       
	;; 
esac

dnl **********************************************************************
dnl Output a Makefile or two and the lib/header descriptor script
dnl **********************************************************************
AC_CONFIG_FILES([
     Makefile
     debian/Makefile
     debian/control
     docs/Makefile
     lavtools/Makefile
     yuvdenoise/Makefile
     yuvdeinterlace/Makefile
     yuvfilters/Makefile
     mpeg2enc/Makefile
     aenc/Makefile
     mplex/Makefile
     scripts/Makefile
     utils/Makefile
     utils/altivec/Makefile
     utils/mmxsse/Makefile
     y4mdenoise/Makefile
     mjpegtools.pc
     mjpegtools.spec
     ])
AC_OUTPUT

dnl ************************************************************************
dnl Summarize the config for the user.
dnl ************************************************************************
AC_MSG_NOTICE([])
AC_MSG_NOTICE([ MJPEG tools ${VERSION} build configuration :])
AC_MSG_NOTICE([])
if test "$have_x86cpu" = "true" ; then
  AC_MSG_NOTICE([  - X86 Optimizations:])
  AC_MSG_NOTICE([    - MMX/3DNow!/SSE enabled      : ${have_asm_mmx}])
fi
if test "$have_ppccpu" = "true" ; then
  AC_MSG_NOTICE([  - PowerPC Optimizations:])
  AC_MSG_NOTICE([    - AltiVec enabled             : ${have_altivec}])
fi
if test x"$ARCHFLAGS" != x; then
  AC_MSG_NOTICE([  - arch/cpu compiler flags       : ${ARCHFLAGS}])
fi
AC_MSG_NOTICE([  - video4linux recording/playback: ${have_video4linux}])
AC_MSG_NOTICE([  - software MJPEG playback       : ${have_sdl}])
AC_MSG_NOTICE([  - MPEG Z/Alpha                  : ${use_zalpha}])
AC_MSG_NOTICE([  - Quicktime playback/recording  : ${have_libquicktime}])
AC_MSG_NOTICE([  - PNG input support             : ${have_libpng}])
AC_MSG_NOTICE([  - AVI MJPEG playback/recording  : true (always)])
AC_MSG_NOTICE([  - libDV (digital video) support : ${have_libdv} ])
AC_MSG_NOTICE([  - Gtk+ support for glav         : ${have_gtk}])
AC_MSG_NOTICE([])
