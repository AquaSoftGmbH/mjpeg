CC = /usr/local/bin/gcc
CFLAGS = -pg -lm -march=athlon -O3 -mintel-syntax -fmove-all-movables -funroll-loops

yuvdenoise:	Makefile yuvdenoise.c
	$(CC) $(CFLAGS) yuvdenoise.c -o yuvdenoise

clean:
	rm *~ yuvdenoise

edit:
	emacs Makefile *.c *.h &
