# Makefile for lavtools

M_OBJS       = lavrec lavplay movtar_play

LIBS= mjpeg/libmjpeg.a quicktime/quicktime/libquicktime.a jpeg-6b-mmx/libjpeg.a 
LIBS_AND_OPT= $(LIBS) -lpthread -lpng -lz

# uncomment if you want to use ALSA, 
# the Advanced Linux Sound Architecture (http://alsa.jcu.cz) 
#USE_ALSA = -DUSE_ALSA -lasound

#this is for software MJPEG playback, and GLib is used by movtarlib
L_FLAGS  = -lpthread -lSDL 
L_FLAGS_MOV = -lpthread -lSDL `glib-config glib --libs` 

CFLAGS= -O2 -I . -I buz -I jpeg-6b-mmx -I quicktime/quicktime -I mjpeg
CFLAGS_MOV =  -O2 -I . -I buz -I jpeg-6b-mmx -I quicktime/quicktime -I mjpeg -I movtar 

all:		lavrec lavplay

#the dependence files are those which have been adapted to MMX and therefore
#are suspect to change 
./jpeg-6b-mmx/libjpeg.a: ./jpeg-6b-mmx/jdapimin.c ./jpeg-6b-mmx/jdmerge.c \
	./jpeg-6b-mmx/jidctfst.c ./jpeg-6b-mmx/jidctint.c \
	./jpeg-6b-mmx/jdcolor.c ./jpeg-6b-mmx/jdsample.c
	cd jpeg-6b-mmx; make

./quicktime/quicktime/libquicktime.a: 
	cd quicktime/quicktime; make

./mjpeg/libmjpeg.a: mjpeg/mjpeg.c mjpeg/mjpeg.h mjpeg/jpeg_dec.c
	cd mjpeg; make

./movtar/libmovtar.a: movtar/movtar.c movtar/movtar.h 
	cd movtar; make

lavrec:		lavrec.o avilib.o audiolib.o lav_io.o movtar/movtar.c $(LIBS)
		cc -o lavrec lavrec.o avilib.o audiolib.o lav_io.o \
		$(LIBS_AND_OPT) $(L_FLAGS) $(CFLAGS)

lavplay:	lavplay.c avilib.o audiolib.o lav_io.o editlist.o $(LIBS)
		cc -g -o lavplay lavplay.c avilib.o audiolib.o lav_io.o \
		editlist.o $(LIBS_AND_OPT) $(L_FLAGS) $(CFLAGS)

movtar_play: movtar/libmovtar.a movtar_play.c ./jpeg-6b-mmx/libjpeg.a
	gcc movtar_play.c ./jpeg-6b-mmx/libjpeg.a movtar/libmovtar.a \
	$(CFLAGS_MOV) $(L_FLAGS_MOV) -o movtar_play

install:
	su -c "cp -v $(M_OBJS) /usr/local/bin/"

clean:
		rm -f *.o lavplay lavrec movtar_play

realclean:
		rm -f *.o lavplay lavrec
		cd quicktime/quicktime; make clean
		cd jpeg-6b-mmx; make clean
		cd mjpeg; make clean
#		cd utils; make clean
#		cd xlav; make clean
#		cd mpeg2enc; make clean
#		cd aenc; make clean











