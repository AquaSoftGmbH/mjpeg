# This script should allow a one-command-build for all subdirectories
# Written by Ronald Bultje

prefix=/usr
current_dir=`pwd`
tmp_movtar_dir=${current_dir}/debian/libmovtar_tmp_usr
jpeg_dir=${current_dir}/jpeg-mmx
quicktime_dir=${current_dir}/quicktime4linux

#x first, libjpeg-mmx
cd jpeg-mmx && \
./configure && \
make libjpeg-mmx.a && \
cd ..

# quicktime4linux
cd quicktime4linux && \
./configure && \
make && \
cd ..

# libmovtar
cd libmovtar && \
./configure --prefix=${tmp_movtar_dir} \
  --with-jpeg-mmx=${jpeg_dir} && \
make && \
make m4datadir=${tmp_movtar_dir}/share/aclocal install && \
make distclean && \
./configure --prefix=${prefix} \
  --with-jpeg-mmx=${jpeg_dir} && \
cd ..

# mjpegtools (no build needed, debian package system will do that)
cd mjpegtools && \
./configure --prefix=${prefix} \
  --with-jpeg-mmx=${jpeg_dir} \
  --with-quicktime=${quicktime_dir} \
  --with-movtar-prefix=${tmp_movtar_dir} && \
cd ..

# Now that we have everything setup, we can build the debian packages
if [ `whoami` = "root" ]; then
	dpkg-buildpackage
else
	fakeroot dpkg-buildpackage
fi

# Now, we don't need libmovtar's temp dir anymore
rm -fr ${tmp_movtar_dir}
