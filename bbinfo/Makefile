
all: bbinfo bbvinfo bbainfo

install: all
	install bbinfo /usr/local/bin
	install bbinfo /usr/local/bin
	install bbinfo /usr/local/bin

clean:
	rm -f bbinfo bbainfo bbvinfo *.o

CFLAGS = -g 

bbinfo: bits.o bbinfo.o
	gcc -g -o bbinfo bbinfo.o bits.o -lm

bbvinfo: bits.o bbvinfo.o
	gcc -g -o bbvinfo bbvinfo.o bits.o -lm

bbainfo: bits.o bbainfo.o
	gcc -g -o bbainfo bbainfo.o bits.o -lm

