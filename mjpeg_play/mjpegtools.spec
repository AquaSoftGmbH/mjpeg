Summary: Tools for recording, editing, playing back and mpeg-encoding video under linux
Name: mjpegtools
Version: 1.4.0
Release: 1
Copyright: GPL
Group: Applications/Multimedia
Source0: http://prdownloads.sourceforge.net/mjpeg/mjpegtools-1.4.0.tar.gz
Source1: http://prdownloads.sourceforge.net/mjpeg/quicktime4linux-1.3-patched.tar.gz
Source2: http://prdownloads.sourceforge.net/mjpeg/libmovtar-0.1.2a.tar.gz
Source3: http://prdownloads.sourceforge.net/mjpeg/jpeg-mmx-0.1.3.tar.gz

%description
The MJPEG-tools are a basic set of utilities for recording, editing, 
playing back and encoding (to mpeg) video under linux. Recording can
be done with zoran-based MJPEG-boards (LML33, Iomega Buz, Pinnacle
DC10(+), Marvel G200/G400), these can also playback video using the
hardware. With the rest of the tools, this video can be edited and
encoded into mpeg1/2 video.
  
%prep
%setup -b 3 -n jpeg-mmx
./configure
make libjpeg-mmx.a

%setup -b 1 -n quicktime4linux-1.3-patch
./configure
make

%setup -b 2 -n libmovtar
./configure
make
make install

%setup -b 0 -n mjpegtools-1.4.0
./configure --with-quicktime=`pwd`/../quicktime4linux-1.3-patch \
	--with-jpeg-mmx=`pwd`/../jpeg-mmx \
	--with-movtar-prefix=`pwd`/../libmovtar-0.1.2 \
	--with-movtar-exec-prefix=/usr/local \
	--enable-large-file --enable-cmov-extension

%build
make

%install
make install

%clean

%files
%defattr(-,root,root)
%doc README

/usr/local/bin/lavplay
/usr/local/bin/lavrec
/usr/local/bin/lav2wav
/usr/local/bin/lav2yuv
/usr/local/bin/yuvmedianfilter
/usr/local/bin/lavaddwav
/usr/local/bin/lavvideo
/usr/local/bin/lavtrans
/usr/local/bin/xlav
/usr/local/bin/ypipe
/usr/local/bin/yuv2lav
/usr/local/bin/testrec
/usr/local/bin/transist.flt
/usr/local/bin/matteblend.flt
/usr/local/bin/lavpipe
/usr/local/bin/yuvscaler
/usr/local/bin/yuvplay
/usr/local/bin/mp2enc
/usr/local/bin/mplex
/usr/local/bin/mpeg2enc
/usr/local/bin/movtar_play
/usr/local/bin/movtar_split
/usr/local/bin/movtar_unify
/usr/local/bin/movtar_index
/usr/local/bin/movtar_setinfo
/usr/local/bin/movtar_yuv422

/usr/local/man/man1/lav2wav.1
/usr/local/man/man1/lav2yuv.1
/usr/local/man/man1/lavpipe.1
/usr/local/man/man1/lavplay.1
/usr/local/man/man1/lavrec.1
/usr/local/man/man1/lavtrans.1
/usr/local/man/man1/mjpegtools.1
/usr/local/man/man1/mp2enc.1
/usr/local/man/man1/mpeg2enc.1
/usr/local/man/man1/mplex.1
/usr/local/man/man1/xlav.1
/usr/local/man/man1/yuv2lav.1
/usr/local/man/man1/yuvplay.1
/usr/local/man/man1/yuvscaler.1

%changelog
* Wed Jun 06 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- 1.4.0-final release, including precompiled binaries (deb/rpm)
