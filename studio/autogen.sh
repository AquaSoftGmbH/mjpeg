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
autoheader
autoconf
automake
