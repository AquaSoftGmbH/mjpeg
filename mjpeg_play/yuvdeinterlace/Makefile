all:	main.o
	gcc -pg -o yuvdeinterlace main.o -L ../utils/ -lmotion -lmjpegutils -lm

main.o:	main.c
	gcc -pg -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c main.c

clean:
	rm -f *.o *~
	rm -f yuvdeinterlace
