INCLUDE=-I ../utils -I ..
# Debug version
#OPT=-g
#LINK=-g
# Profile version
#OPT=-O3 -march=athlon-xp -pg -DNDEBUG
#LINK=-pg
# Release version
OPT=-O3 -march=athlon-xp -DNDEBUG
LINK=-s

all: newdenoise regiontest

clean:
	rm -f *.o blah newdenoise regiontest templatetest Makefile.bak \
		gmon.out bug.txt

depend:
	makedepend -fMakefile $(INCLUDE) -I /usr/include/c++/3.3.3 \
		-I /usr/include/c++/3.3.3/i386-redhat-linux *.cc *.c

newdenoise.o: newdenoise.cc
	g++ -Wall $(INCLUDE) $(OPT) -DHAVE_CONFIG_H -c newdenoise.cc

main.o: main.c
	gcc -Wall $(INCLUDE) $(OPT) -DHAVE_CONFIG_H -c main.c

newdenoise: newdenoise.o main.o
	g++ -Wall $(LINK) -o newdenoise newdenoise.o main.o \
		-lmjpegutils

regiontest.o: regiontest.cc
	g++ -Wall $(INCLUDE) $(OPT) -c regiontest.cc

regiontest: regiontest.o
	g++ -Wall -o regiontest regiontest.o \
		-lmjpegutils

templatetest.o: templatetest.cc
	g++ -Wall $(INCLUDE) $(OPT) -c templatetest.cc

templatetest: templatetest.o
	g++ -Wall -o templatetest templatetest.o

# DO NOT DELETE

newdenoise.o: /usr/include/assert.h /usr/include/features.h
newdenoise.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
newdenoise.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
newdenoise.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
newdenoise.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
newdenoise.o: /usr/include/string.h /usr/include/math.h
newdenoise.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
newdenoise.o: /usr/include/bits/mathcalls.h ../utils/mjpeg_types.h
newdenoise.o: /usr/include/sys/types.h /usr/include/time.h
newdenoise.o: /usr/include/stdio.h /usr/include/libio.h
newdenoise.o: /usr/include/_G_config.h /usr/include/wchar.h
newdenoise.o: /usr/include/bits/wchar.h /usr/include/gconv.h
newdenoise.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
newdenoise.o: newdenoise.hh MotionSearcher.hh ../config.h TemplateLib.hh
newdenoise.o: Limits.hh DoublyLinkedList.hh ReferenceFrame.hh Status_t.h
newdenoise.o: SetRegion2D.hh Region2D.hh /usr/include/c++/3.3.3/iostream
newdenoise.o: /usr/include/c++/3.3.3/ostream /usr/include/c++/3.3.3/ios
newdenoise.o: /usr/include/c++/3.3.3/iosfwd /usr/include/c++/3.3.3/cctype
newdenoise.o: /usr/include/ctype.h /usr/include/endian.h
newdenoise.o: /usr/include/bits/endian.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stringfwd.h
newdenoise.o: /usr/include/c++/3.3.3/bits/fpos.h
newdenoise.o: /usr/include/c++/3.3.3/cwchar /usr/include/c++/3.3.3/cstddef
newdenoise.o: /usr/include/c++/3.3.3/ctime
newdenoise.o: /usr/include/c++/3.3.3/bits/functexcept.h
newdenoise.o: /usr/include/c++/3.3.3/exception_defines.h
newdenoise.o: /usr/include/c++/3.3.3/exception
newdenoise.o: /usr/include/c++/3.3.3/bits/char_traits.h
newdenoise.o: /usr/include/c++/3.3.3/cstring /usr/include/c++/3.3.3/cstdio
newdenoise.o: /usr/include/c++/3.3.3/bits/localefwd.h
newdenoise.o: /usr/include/c++/3.3.3/bits/ios_base.h
newdenoise.o: /usr/include/c++/3.3.3/bits/locale_classes.h
newdenoise.o: /usr/include/c++/3.3.3/string /usr/include/c++/3.3.3/memory
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_algobase.h
newdenoise.o: /usr/include/c++/3.3.3/climits /usr/include/limits.h
newdenoise.o: /usr/include/c++/3.3.3/cstdlib /usr/include/stdlib.h
newdenoise.o: /usr/include/c++/3.3.3/new
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_pair.h
newdenoise.o: /usr/include/c++/3.3.3/bits/type_traits.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_iterator_base_types.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_iterator_base_funcs.h
newdenoise.o: /usr/include/c++/3.3.3/bits/concept_check.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_iterator.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_alloc.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_threads.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_construct.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_uninitialized.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_raw_storage_iter.h
newdenoise.o: /usr/include/c++/3.3.3/bits/stl_function.h
newdenoise.o: /usr/include/c++/3.3.3/bits/basic_string.h
newdenoise.o: /usr/include/c++/3.3.3/streambuf
newdenoise.o: /usr/include/c++/3.3.3/bits/basic_ios.h
newdenoise.o: /usr/include/c++/3.3.3/bits/streambuf_iterator.h
newdenoise.o: /usr/include/c++/3.3.3/bits/locale_facets.h
newdenoise.o: /usr/include/c++/3.3.3/cwctype
newdenoise.o: /usr/include/c++/3.3.3/bits/codecvt.h
newdenoise.o: /usr/include/c++/3.3.3/istream /usr/include/c++/3.3.3/limits
newdenoise.o: Set.hh SkipList.hh Allocator.hh BitmapRegion2D.hh
newdenoise.o: SearchBorder.hh SearchWindow.hh
regiontest.o: ../config.h /usr/include/stdio.h /usr/include/features.h
regiontest.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
regiontest.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
regiontest.o: /usr/include/bits/typesizes.h /usr/include/libio.h
regiontest.o: /usr/include/_G_config.h /usr/include/wchar.h
regiontest.o: /usr/include/bits/wchar.h /usr/include/gconv.h
regiontest.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
regiontest.o: SetRegion2D.hh Region2D.hh Status_t.h
regiontest.o: /usr/include/c++/3.3.3/iostream /usr/include/c++/3.3.3/ostream
regiontest.o: /usr/include/c++/3.3.3/ios /usr/include/c++/3.3.3/iosfwd
regiontest.o: /usr/include/c++/3.3.3/cctype /usr/include/ctype.h
regiontest.o: /usr/include/endian.h /usr/include/bits/endian.h
regiontest.o: /usr/include/c++/3.3.3/bits/stringfwd.h
regiontest.o: /usr/include/c++/3.3.3/bits/fpos.h
regiontest.o: /usr/include/c++/3.3.3/cwchar /usr/include/c++/3.3.3/cstddef
regiontest.o: /usr/include/c++/3.3.3/ctime /usr/include/time.h
regiontest.o: /usr/include/c++/3.3.3/bits/functexcept.h
regiontest.o: /usr/include/c++/3.3.3/exception_defines.h
regiontest.o: /usr/include/c++/3.3.3/exception
regiontest.o: /usr/include/c++/3.3.3/bits/char_traits.h
regiontest.o: /usr/include/c++/3.3.3/cstring /usr/include/string.h
regiontest.o: /usr/include/c++/3.3.3/cstdio
regiontest.o: /usr/include/c++/3.3.3/bits/localefwd.h
regiontest.o: /usr/include/c++/3.3.3/bits/ios_base.h
regiontest.o: /usr/include/c++/3.3.3/bits/locale_classes.h
regiontest.o: /usr/include/c++/3.3.3/string /usr/include/c++/3.3.3/memory
regiontest.o: /usr/include/c++/3.3.3/bits/stl_algobase.h
regiontest.o: /usr/include/c++/3.3.3/climits /usr/include/limits.h
regiontest.o: /usr/include/c++/3.3.3/cstdlib /usr/include/stdlib.h
regiontest.o: /usr/include/c++/3.3.3/new
regiontest.o: /usr/include/c++/3.3.3/bits/stl_pair.h
regiontest.o: /usr/include/c++/3.3.3/bits/type_traits.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_iterator_base_types.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_iterator_base_funcs.h
regiontest.o: /usr/include/c++/3.3.3/bits/concept_check.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_iterator.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_alloc.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_threads.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_construct.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_uninitialized.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_raw_storage_iter.h
regiontest.o: /usr/include/c++/3.3.3/bits/stl_function.h
regiontest.o: /usr/include/c++/3.3.3/bits/basic_string.h
regiontest.o: /usr/include/c++/3.3.3/streambuf
regiontest.o: /usr/include/c++/3.3.3/bits/basic_ios.h
regiontest.o: /usr/include/c++/3.3.3/bits/streambuf_iterator.h
regiontest.o: /usr/include/c++/3.3.3/bits/locale_facets.h
regiontest.o: /usr/include/c++/3.3.3/cwctype
regiontest.o: /usr/include/c++/3.3.3/bits/codecvt.h
regiontest.o: /usr/include/c++/3.3.3/istream /usr/include/c++/3.3.3/limits
regiontest.o: Set.hh SkipList.hh /usr/include/assert.h ../utils/mjpeg_types.h
regiontest.o: /usr/include/sys/types.h Allocator.hh Limits.hh TemplateLib.hh
main.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
main.o: /usr/include/gnu/stubs.h /usr/include/bits/types.h
main.o: /usr/include/bits/wordsize.h /usr/include/bits/typesizes.h
main.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
main.o: /usr/include/bits/wchar.h /usr/include/gconv.h
main.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
main.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/unistd.h
main.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
main.o: ../utils/mjpeg_types.h /usr/include/sys/types.h /usr/include/time.h
main.o: ../utils/yuv4mpeg.h ../utils/mjpeg_logging.h ../config.h
main.o: newdenoise.hh /usr/include/sys/stat.h /usr/include/bits/stat.h
main.o: /usr/include/fcntl.h /usr/include/bits/fcntl.h
