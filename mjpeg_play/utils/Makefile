# Makefile for lavtools/utils

QT_LIBS= ../quicktime/quicktime/libquicktime.a ../jpeg-6b-mmx/libjpeg.a -lpthread -lpng -lz


CFLAGS= -O2 -I .. -I ../buz -I ../jpeg-6b -I quicktime

all:		lavtrans lavaddwav

lavtrans:	lavtrans.o
		cc -o lavtrans lavtrans.o ../editlist.o ../avilib.o ../lav_io.o $(QT_LIBS)

lavaddwav:	lavaddwav.o
		cc -o lavaddwav lavaddwav.o ../avilib.o ../lav_io.o $(QT_LIBS)

clean:
		rm -f *.o lavtrans lavaddwav
