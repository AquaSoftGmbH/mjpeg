%define prefix  /usr/local
%define version 1.5.4
%define release 1

Summary: Tools for recording, editing, playing back and mpeg/divx-encoding video under linux
Name: mjpegtools
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Applications/Multimedia
Prefix: %{prefix}
Source0: http://prdownloads.sourceforge.net/mjpeg/mjpegtools-%{version}.tar.gz
Source1: http://prdownloads.sourceforge.net/mjpeg/quicktime4linux-1.4-patched.tar.gz
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

%setup -b 1 -n quicktime4linux-1.4-patch
./configure
make

%setup -b 2 -n libmovtar
./configure --prefix=%{prefix}
make
make install

%setup -b 0 -n mjpegtools-%{version}
./configure --with-quicktime=`pwd`/../quicktime4linux-1.4-patch \
	--with-jpeg-mmx=`pwd`/../jpeg-mmx \
	--with-movtar-prefix=`pwd`/../libmovtar-0.1.2 \
	--with-movtar-exec-prefix=%{prefix} \
	--enable-large-file --prefix=%{prefix}

%build
make

%install
make install

%clean

%files
%defattr(-,root,root)
%doc README README.DV README.avilib README.glav README.lavpipe README.transist AUTHORS COPYING

%{prefix}/lib/liblavplay*so*
%{prefix}/lib/liblavrec*so*
%{prefix}/lib/liblavfile*so*
%{prefix}/lib/liblavjpeg*so*
%{prefix}/lib/libmjpegutils.a
%{prefix}/lib/liblavplay.a
%{prefix}/lib/liblavrec.a
%{prefix}/lib/liblavfile.a
%{prefix}/lib/liblavjpeg.a
%{prefix}/bin/lavplay
%{prefix}/bin/lavrec
%{prefix}/bin/lav2wav
%{prefix}/bin/lav2yuv
%{prefix}/bin/yuvmedianfilter
%{prefix}/bin/lavaddwav
%{prefix}/bin/lavvideo
%{prefix}/bin/lavtrans
%{prefix}/bin/glav
%{prefix}/bin/ypipe
%{prefix}/bin/yuv2lav
%{prefix}/bin/testrec
%{prefix}/bin/transist.flt
%{prefix}/bin/matteblend.flt
%{prefix}/bin/lavpipe
%{prefix}/bin/yuvscaler
%{prefix}/bin/yuvplay
%{prefix}/bin/jpeg2yuv
%{prefix}/bin/lav2divx
%{prefix}/bin/yuv2divx
%{prefix}/bin/mp2enc
%{prefix}/bin/mplex
%{prefix}/bin/mpeg2enc
%{prefix}/bin/yuvdenoise
%{prefix}/bin/yuvycsnoise
%{prefix}/bin/yuvkineco
${prefix}/include/mjpegtools/*.h
%{prefix}/bin/movtar_play
%{prefix}/bin/movtar_split
%{prefix}/bin/movtar_unify
%{prefix}/bin/movtar_index
%{prefix}/bin/movtar_setinfo
%{prefix}/bin/movtar_yuv422
%{prefix}/bin/movtar-config
%{prefix}/lib/libmovtar.a
%{prefix}/include/movtar.h
%{prefix}/man/man1/lav2wav.1
%{prefix}/man/man1/lav2yuv.1
%{prefix}/man/man1/lavpipe.1
%{prefix}/man/man1/lavplay.1
%{prefix}/man/man1/lavrec.1
%{prefix}/man/man1/lavtrans.1
%{prefix}/man/man1/mjpegtools.1
%{prefix}/man/man1/mp2enc.1
%{prefix}/man/man1/mpeg2enc.1
%{prefix}/man/man1/mplex.1
%{prefix}/man/man1/yuv2lav.1
%{prefix}/man/man1/yuvplay.1
%{prefix}/man/man1/yuvscaler.1

%changelog
* Wed Jun 06 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- 1.4.0-final release, including precompiled binaries (deb/rpm)
