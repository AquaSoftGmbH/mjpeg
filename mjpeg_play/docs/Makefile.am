# If the installation of man pages by you (e.g. you're a package builder)
# You may want to disable it
man_MANS = lavplay.1  lavrec.1  mjpegtools.1  mpeg2enc.1  mplex.1 \
	yuvplay.1 lav2wav.1 lav2yuv.1 lavtrans.1 mp2enc.1 \
	yuvscaler.1 lavpipe.1 yuv2lav.1 yuvdenoise.1 jpeg2yuv.1 \
        ppmtoy4m.1 y4mtoppm.1 y4mcolorbars.1 \
        yuvkineco.1 yuvycsnoise.1 yuvmedianfilter.1 \
	lav2divx.1 yuv2divx.1 divxdec.1 lav2mpeg.1 \
	yuv4mpeg.5 yuvfps.1 img2mpg.1
install-exec-hook:
	./install-info.sh $(infodir)
EXTRA_DIST = $(man_MANS) authors.sgml lavtools.sgml .cvsignore

MAINTAINERCLEANFILES = Makefile.in
