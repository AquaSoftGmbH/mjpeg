#!/bin/sh
touch config.sub
chmod a+x config.sub
touch config.guess
chmod a+x config.guess
aclocal
autoheader
autoconf
automake
