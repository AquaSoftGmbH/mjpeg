#!/bin/sh
echo "Running libtoolize..."
libtoolize --force --copy
if test ! -r aclocal.m4; then
  echo "Running aclocal ..."
  aclocal 
else
  echo "Skiping aclocal, because aclocal.m4 exists"
fi
echo "Running autoheader..."
autoheader
echo "Running autoconf..."
autoconf
echo "Running automake..."
automake --add-missing --gnu
conf_flags="--enable-maintainer-mode --enable-compile-warnings"
./configure $conf_flags "$@"
