#!/bin/sh
touch config.sub
chmod a+x config.sub
touch config.guess
chmod a+x config.guess
if test ! -r aclocal.m4; then
  echo "Running aclocal ..."
  aclocal 
else
  echo "Skiping aclocal, because aclocal.m4 exists"
fi
echo "Running autoheader..."
autoheader
echo "Running libtoolize..."
libtoolize --force --copy
echo "Running autoconf..."
autoconf
echo "Running automake..."
automake --add-missing --gnu
echo "Running ./configure..."
conf_flags="--enable-maintainer-mode --enable-compile-warnings"
./configure $conf_flags "$@"
