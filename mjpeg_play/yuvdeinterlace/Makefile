all:	main.o
	gcc -o yuvdeinterlace main.o -L ../utils -L ../utils/.libs -lmjpegutils -lmotion -lcpuaccel -lm

main.o:	main.c
	gcc -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c main.c

clean:
	rm -f *.o *~
	rm -f yuvdeinterlace
