%define name    mjpegtools
%define version 1.6.0
%define release beta1
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
Source2:	http://prdownloads.sourceforge.net/mjpeg/libmovtar-0.1.3.tar.gz
Source3:	http://prdownloads.sourceforge.net/mjpeg/jpeg-mmx-0.1.4.tar.gz
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
 [ -n "${RPM_BUILD_ROOT}" -a "${RPM_BUILD_ROOT}" != / ] \
 && rm -rf ${RPM_BUILD_ROOT}/
%setup -b 3 -n jpeg-mmx-0.1.4
%configure
make libjpeg-mmx.a

%setup -b 1 -n quicktime4linux-1.4-patch
%configure
make

%setup -b 2 -n libmovtar-0.1.3
%configure --prefix=%{prefix}
make
make DESTDIR=${RPM_BUILD_ROOT} install  

%setup -b 0 -n mjpegtools-%{version}-%{release}
./configure --prefix=%{prefix} \
	--with-quicktime=`pwd`/../quicktime4linux-1.4-patch \
	--with-jpeg-mmx=`pwd`/../jpeg-mmx-0.1.4 \
	--with-movtar-prefix=`pwd`/../libmovtar-0.1.3 \
	--with-movtar-exec-prefix=${RPM_BUILD_ROOT}%{prefix} \
	--enable-large-file --enable-cmov-extension
%build
make

%install
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
%{_libdir}/*.so*
%{_libdir}/*.la*
%{prefix}/man/man1/*

%package devel
Summary: Development headers and libraries for the mjpegtools
Group: Development/Libraries
Requires: %{name} = %{version}

%description devel
This package contains static libraries and C system header files
needed to compile applications that use part of the libraries
of the mjpegtools package.

%files devel
%{_bindir}/*-config
%{_includedir}/mjpegtools/*.h
%{_includedir}/movtar.h
%{prefix}/share/aclocal/*.m4
%{_libdir}/*.a

%changelog
* Thu Dec 06 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- separated mjpegtools and mjpegtools-devel
- added changes by Marcel Pol <mpol@gmx.net> for cleaner RPM build

* Wed Jun 06 2001 Ronald Bultje <rbultje@ronald.bitfreak.net>
- 1.4.0-final release, including precompiled binaries (deb/rpm)
