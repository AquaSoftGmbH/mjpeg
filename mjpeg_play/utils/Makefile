# Makefile for lavtools/utils

QT_LIBS=   -lpthread -lpng -lz -lglib -lmovtar \
	-lm -lquicktime -ljpeg                     

CFLAGS= -O2 -I .. 

all:		lavtrans lavaddwav

lavtrans:	lavtrans.o
		cc -o lavtrans lavtrans.o ../editlist.o ../avilib.o ../lav_io.o $(QT_LIBS)

lavaddwav:	lavaddwav.o
		cc -o lavaddwav lavaddwav.o ../avilib.o ../lav_io.o $(QT_LIBS)

clean:
		rm -f *.o lavtrans lavaddwav





