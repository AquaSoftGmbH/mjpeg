# CFLAGS and library paths for MOVTAR
# HEAVILY based on the one from XMPS

dnl Usage:
dnl AM_PATH_MOVTAR([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl
dnl Example:
dnl AM_PATH_MOVTAR(0.1.1, , AC_MSG_ERROR([*** MOVTAR >= 0.1.1 not installed - please install first ***]))
dnl
dnl Defines MOVTAR_CFLAGS, MOVTAR_LIBS, MOVTAR_DATA_DIR, 
dnl and MOVTAR_VERSION for your plugin pleasure.
dnl

AC_DEFUN(MOVTAR_TEST_VERSION, [

# Determine which version number is greater. Prints 2 to stdout if	
# the second number is greater, 1 if the first number is greater,	
# 0 if the numbers are equal.						
									
# Written 15 December 1999 by Ben Gertzfield <che@debian.org>		
# Revised 15 December 1999 by Jim Monty <monty@primenet.com>		
									
    AC_PROG_AWK
    movtar_got_version=[` $AWK '						\
BEGIN {									\
    print vercmp(ARGV[1], ARGV[2]);					\
}									\
									\
function vercmp(ver1, ver2,    ver1arr, ver2arr,			\
                               ver1len, ver2len,			\
                               ver1int, ver2int, len, i, p) {		\
									\
    ver1len = split(ver1, ver1arr, /\./);				\
    ver2len = split(ver2, ver2arr, /\./);				\
									\
    len = ver1len > ver2len ? ver1len : ver2len;			\
									\
    for (i = 1; i <= len; i++) {					\
        p = 1000 ^ (len - i);						\
        ver1int += ver1arr[i] * p;					\
        ver2int += ver2arr[i] * p;					\
    }									\
									\
    if (ver1int < ver2int)						\
        return 2;							\
    else if (ver1int > ver2int)						\
        return 1;							\
    else								\
        return 0;							\
}' $1 $2`]								

    if test $movtar_got_version -eq 2; then 	# failure
	ifelse([$4], , :, $4)			
    else  					# success!
	ifelse([$3], , :, $3)
    fi
])

AC_DEFUN(AM_PATH_MOVTAR,
[
AC_ARG_WITH(movtar-prefix,[  --with-movtar-prefix=PFX  Prefix where movtar is installed (optional)],
	movtar_config_prefix="$withval", movtar_config_prefix="")
AC_ARG_WITH(movtar-exec-prefix,[  --with-movtar-exec-prefix=PFX Exec prefix where movtar is installed (optional)],
	movtar_config_exec_prefix="$withval", movtar_config_exec_prefix="")

if test x$movtar_config_exec_prefix != x; then
    movtar_config_args="$movtar_config_args --exec-prefix=$movtar_config_exec_prefix"
    if test x${MOVTAR_CONFIG+set} != xset; then
	MOVTAR_CONFIG=$movtar_config_exec_prefix/bin/movtar-config
    fi
fi

if test x$movtar_config_prefix != x; then
    movtar_config_args="$movtar_config_args --prefix=$movtar_config_prefix"
    if test x${MOVTAR_CONFIG+set} != xset; then
  	MOVTAR_CONFIG=$movtar_config_prefix/bin/movtar-config
    fi
fi

AC_PATH_PROG(MOVTAR_CONFIG, movtar-config, no)
min_movtar_version=ifelse([$1], ,0.1.1, $1)

if test "$MOVTAR_CONFIG" = "no"; then
    no_movtar=yes
else
    MOVTAR_CFLAGS=`$MOVTAR_CONFIG $movtar_config_args --cflags`
    MOVTAR_LIBS=`$MOVTAR_CONFIG $movtar_config_args --libs`
    MOVTAR_VERSION=`$MOVTAR_CONFIG $movtar_config_args --version`

    MOVTAR_TEST_VERSION($MOVTAR_VERSION, $min_movtar_version, ,no_movtar=version)
fi

AC_MSG_CHECKING(for movtar - version >= $min_movtar_version)

if test "x$no_movtar" = x; then
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])
else
    AC_MSG_RESULT(no)

    if test "$MOVTAR_CONFIG" = "no" ; then
	echo "*** The movtar-config script installed by movtar could not be found."
      	echo "*** If movtar was installed in PREFIX, make sure PREFIX/bin is in"
	echo "*** your path, or set the MOVTAR_CONFIG environment variable to the"
	echo "*** full path to movtar-config."
    else
	if test "$no_movtar" = "version"; then
	    echo "*** An old version of movtar, $MOVTAR_VERSION, was found."
	    echo "*** You need a version of movtar newer than $min_movtar_version."
	    echo "*** The latest version of movtar is always available from"
	    echo "*** http://mjpeg.sourceforge.net/"
	    echo "***"

            echo "*** If you have already installed a sufficiently new version, this error"
            echo "*** probably means that the wrong copy of the movtar-config shell script is"
            echo "*** being found. The easiest way to fix this is to remove the old version"
            echo "*** of movtar, but you can also set the MOVTAR_CONFIG environment to point to the"
            echo "*** correct copy of movtar-config. (In this case, you will have to"
            echo "*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf"
            echo "*** so that the correct libraries are found at run-time)"
	fi
    fi
    MOVTAR_CFLAGS=""
    MOVTAR_LIBS=""
    ifelse([$3], , :, [$3])
fi
AC_SUBST(MOVTAR_CFLAGS)
AC_SUBST(MOVTAR_LIBS)
AC_SUBST(MOVTAR_VERSION)
])
