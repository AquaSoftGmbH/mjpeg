#!/bin/sh

rm -f $1/mjpeg-howto.info*

install-info --help 2>&1 | grep menuentry >/dev/null

if [ $? -eq 0 ]
then
  # Here we install the doku for debian style install-info
  install-info  --delete --infodir=$1 --menuentry="* mjpeg-howto: (mjpeg-howto).        How to use the mjpeg tools" --description="mjpeg tools documentation" $1/mjpeg-howto.info
else
  # Here we install the doku for gnu style install-info
  install-info --delete $1/mjpeg-howto.info $1/dir --entry="* mjpeg-howto: (mjpeg-howto).        How to use the mjpeg tools"
fi

