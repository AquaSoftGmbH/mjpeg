Summary: Tools for recording, editing, playing back and mpeg-encoding video under linux
Name: mjpegtools
Version: 1.3.0
Release: 1
Copyright: GPL
Group: Applications/Multimedia
Source0: http://prdownloads.sourceforge.net/mjpeg/mjpegtools-1.3.0.tar.gz
Source1: http://prdownloads.sourceforge.net/mjpeg/quicktime4linux-1.3-patched.tar.gz
Source2: http://prdownloads.sourceforge.net/mjpeg/libmovtar-0.1.2.tar.gz
Source3: http://prdownloads.sourceforge.net/mjpeg/jpeg-mmx-0.1.3.tar.gz

%description
The MJPEG-tools are a basic set of utilities for recording, editing, 
playing back and encoding (to mpeg) video under linux. Recording can
be done with zoran-based MJPEG-boards (LML33, Iomega Buz, Pinnacle
DC10(+), Marvel G200/G400), these can also playback video using the
hardware. With the rest of the tools, this video can be edited and
encoded into mpeg1/2 video.
 �
%prep
%setup -b 3 -n jpeg-mmx
./configure
make libjpeg-mmx.a

%setup -D -b 1 -n quicktime4linux-1.3-patch
./configure
make

%setup -D -b 2 -n libmovtar-0.1.2
./configure
make
make install

%setup -b 0 -D -n mjpeg_play
./configure --with-quicktime=`pwd`/../quicktime4linux-1.3-patch --with-jpeg-mmx=`pwd`/../jpeg-mmx --with-movtar-prefix=`pwd`/../libmovtar-0.1.2
make
make install

%build

%install

%clean

%files
%defattr(-,root,root)
%doc README README.lavpipe

/usr/local/bin/lavplay
/usr/local/bin/lavrec
/usr/local/bin/lav2wav
/usr/local/bin/lav2yuv
/usr/local/bin/lav2dfilter
/usr/local/bin/lavaddwav
/usr/local/bin/lavvideo
/usr/local/bin/lavtrans
/usr/local/bin/xlav
/usr/local/bin/ypipe
/usr/local/bin/yuv2lav
/usr/local/bin/testrec
/usr/local/bin/v4l-conf
/usr/local/bin/transist.flt
/usr/local/bin/matteblend.flt
/usr/local/bin/lavpipe
/usr/local/bin/yuvscaler
/usr/local/bin/mp2enc
/usr/local/bin/mplex
/usr/local/bin/mpeg2enc
/usr/local/bin/movtar_play
/usr/local/bin/movtar_split
/usr/local/bin/movtar_unify
/usr/local/bin/movtar_index
/usr/local/bin/movtar_setinfo
/usr/local/bin/movtar_yuv422
/usr/local/bin/movtar-config

%changelog
* Tue Apr 24 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- Initial RPM release
