#!/bin/sh

srcdir=`dirname "$1"`
src=`basename "$1"`
dstdir=`dirname "$2"`
dst=`basename "$2"`

rm -f -- "$dstdir/$dst"
( cd "$srcdir"; tar cf - "$src" ) | ( cd "$dstdir"; tar xf - )
case "$dst" in
"$src")	: ;;
*)	cd "$dstdir"; mv -- "$src" "$dst" ;;
esac
touch -- "$dstdir/$dst"

exit 0
