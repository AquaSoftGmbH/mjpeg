#!/bin/sh
touch config.sub
chmod a+x config.sub
touch config.guess
chmod a+x config.guess
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
