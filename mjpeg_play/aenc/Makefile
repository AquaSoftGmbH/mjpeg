
CFLAGS =  -O3 -m486 -ffast-math -I..
LDFLAGS=

QT_LIBS= ../quicktime/quicktime/libquicktime.a ../jpeg-6b-mmx/libjpeg.a ../movtar/libmovtar.a \
	-lpthread -lpng -lz `glib-config --libs` `glib-config --cflags`


OBJS = musicin.o common.o encode.o subs.o psy.o tonal.o tables.o

all: aenc

aenc:	$(OBJS)
	cc $(LDFLAGS) -o aenc $(OBJS) ../editlist.o ../lav_io.o ../avilib.o $(QT_LIBS) -lm

clean:
	rm -f *.o aenc
