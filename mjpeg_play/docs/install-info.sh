#!/bin/sh
install -m 644 -c  *.info* $1/

install-info --help 2>&1 | grep menuentry >/dev/null

if [ $? -eq 0 ]
then
	echo DEBIAN
  # Here we install the doku for debian style install-info
  #install-info $1/mjpeg-howto.info --infodir==$1/dir --menuentry=\
	#"* mjpeg-howto: (mjpeg-howto).        How to use the mjpeg tools"
else
  # Here we install the doku for gnu style install-info
  install-info $1/mjpeg-howto.info $1/dir --entry=\
"* mjpeg-howto: (mjpeg-howto).        How to use the mjpeg tools"
fi

