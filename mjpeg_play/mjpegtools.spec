%define name    mjpegtools
%define version 1.6.0
%define release rc2
%define prefix  /usr

Name:           %name
Version:        %version
Release:        %release
Summary:	Tools for recording, editing, playing back and mpeg-encoding video under linux
License:	GPL
Url:		http://mjpeg.sourceforge.net/
Group:		Video
Source0:	http://prdownloads.sourceforge.net/mjpeg/mjpegtools-%{version}-%{release}.tar.gz
Source1:	http://prdownloads.sourceforge.net/mjpeg/quicktime4linux-1.4-patched-2.tar.gz
Source2:	http://prdownloads.sourceforge.net/mjpeg/libmovtar-0.1.3-rc1.tar.gz
Source3:	http://prdownloads.sourceforge.net/mjpeg/jpeg-mmx-0.1.4-rc1.tar.gz
BuildRoot:      %{_tmppath}/%{name}-buildroot-%{version}-%{release}
BuildRequires:  XFree86-devel automake >= 1.5
Prefix:		%{prefix}

%description
The MJPEG-tools are a basic set of utilities for recording, editing, 
playing back and encoding (to mpeg) video under linux. Recording can
be done with zoran-based MJPEG-boards (LML33, Iomega Buz, Pinnacle
DC10(+), Marvel G200/G400), these can also playback video using the
hardware. With the rest of the tools, this video can be edited and
encoded into mpeg1/2 or divx video.

%prep
%setup -q -n %{name}-%{version}-%{release} -a 1 -a 2 -a 3

mv jpeg-mmx-* jpeg-mmx
mv quicktime4linux-* quicktime4linux
mv libmovtar-* libmovtar

mkdir usr

%build

tmp_prefix="`pwd`/usr"
mkdir -p $tmp_prefix/{include,lib,bin,share}

CFLAGS="${CFLAGS:-%optflags}" ; export CFLAGS
CXXFLAGS="${CXXFLAGS:-%optflags}" ; export CXXFLAGS

(cd jpeg-mmx && ./configure)
make -C jpeg-mmx libjpeg-mmx.a

(cd quicktime4linux && ./configure)
make -C quicktime4linux

CONF_ARGS="--with-jpeg-mmx=`pwd`/jpeg-mmx"

(cd libmovtar && 
   ./configure --prefix="$tmp_prefix" \
	--with-m4data-prefix="$tmp_prefix/share/aclocal" \
	$CONF_ARGS )

make -C libmovtar
make -C libmovtar install

CONF_ARGS="$CONF_ARGS --prefix=%{prefix}"

(cd libmovtar&& ./configure $CONF_ARGS)
make -C libmovtar

CONF_ARGS="$CONF_ARGS --with-quicktime=`pwd`/quicktime4linux"
CONF_ARGS="$CONF_ARGS --with-movtar-prefix=$tmp_prefix"

MOVTAR_CONFIG=$tmp_prefix/bin/movtar-config; export MOVTAR_CONFIG
./configure $CONF_ARGS --enable-large-file --enable-cmov-extension
make

%install
[ -n "${RPM_BUILD_ROOT}" -a "${RPM_BUILD_ROOT}" != / ] \
 && rm -rf ${RPM_BUILD_ROOT}/

make -C libmovtar DESTDIR=${RPM_BUILD_ROOT} install

make prefix=${RPM_BUILD_ROOT}%{prefix} install

%post
/sbin/ldconfig

%clean
[ -n "${RPM_BUILD_ROOT}" -a "${RPM_BUILD_ROOT}" != / ] \
 && rm -rf ${RPM_BUILD_ROOT}/

%files
%defattr(-,root,root)
%doc AUTHORS BUGS CHANGES COPYING HINTS PLANS README TODO
%{_bindir}/lav*
%{_bindir}/yuv*
%{_bindir}/jpeg2yuv
%{_bindir}/divxdec
%{_bindir}/testrec
%{_bindir}/y4m*
%{_bindir}/ppm*
%{_bindir}/glav
%{_bindir}/ypipe
%{_bindir}/mp*
%{_bindir}/*.flt
%{_bindir}/movtar_*
%{_bindir}/pnm2rtj
%{_bindir}/rtjshow
%{_libdir}/*.so.*
%{prefix}/man/man1/*

%package devel
Summary: Development headers and libraries for the mjpegtools
Group: Development/Libraries

%description devel
This package contains static libraries and C system header files
needed to compile applications that use part of the libraries
of the mjpegtools package.

%files devel
%{_bindir}/*-config
%{_includedir}/mjpegtools/*.h
%{_includedir}/movtar.h
%{prefix}/share/aclocal/*.m4
%{_libdir}/pkgconfig/*.pc
%{_libdir}/*.a
%{_libdir}/*.la
%{_libdir}/*.so

%changelog
* Tue Feb 12 2002 Geoffrey T. Dairiki <dairiki@dairiki.org>
- Fix spec file to build in one directory, etc...

* Thu Dec 06 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- separated mjpegtools and mjpegtools-devel
- added changes by Marcel Pol <mpol@gmx.net> for cleaner RPM build

* Wed Jun 06 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- 1.4.0-final release, including precompiled binaries (deb/rpm)
