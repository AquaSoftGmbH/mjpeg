
CFLAGS =  -O3 -m486 -ffast-math -I..
LDFLAGS=

QT_LIBS= ../quicktime/libquicktime.a ../jpeg-6b/libjpeg.a -lpthread -lpng -lz


OBJS = musicin.o common.o encode.o subs.o psy.o tonal.o tables.o

aenc:	$(OBJS)
	cc $(LDFLAGS) -o aenc $(OBJS) ../editlist.o ../lav_io.o ../avilib.o $(QT_LIBS) -lm

clean:
	rm -f *.o aenc
