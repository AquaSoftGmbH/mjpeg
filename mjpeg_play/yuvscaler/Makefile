# makefile for yuvscaler by xbiquard@free.fr

CC = gcc

ARCH = pentiumpro

#CFLAGS =  -g -Wall -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
CFLAGS = -s -O6 -fomit-frame-pointer -Wall -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -fno-exceptions -m${ARCH} -mcpu=${ARCH} -march=${ARCH} -ffast-math -fexpensive-optimizations -malign-functions=4 -funroll-all-loops
#CFLAGS = -pg -s -O3 -Wall -fno-exceptions -mpentiumpro  -mcpu=pentiumpro -march=pentiumpro -ffast-math -fexpensive-optimizations -malign-functions=4 -funroll-loops

OBJS = yuvscaler.o

all:  yuvscaler

yuvscaler:     yuvscaler.h yuvscaler.c
	      ${CC} ${CFLAGS} -o ${@} ${@}.c

clean:
		rm -f *.o yuvscaler
		
install:
		cp yuvscaler /usr/local/bin
		
