CFLAGS = -O6 -funroll-all-loops -finline-functions -Wall -fstrength-reduce -fexpensive-optimizations

all:	main.o sinc_interpolation.o interleave.o copy_frame.o blend_fields.o transform_block.o
	gcc -o yuvdeinterlace \
	       main.o \
	       sinc_interpolation.o \
	       interleave.o \
	       copy_frame.o \
	       blend_fields.o \
	       transform_block.o \
	       -L ../utils -L ../utils/.libs -lmjpegutils -lmotion -lcpuaccel -lm

main.o:	main.c
	gcc $(CFLAGS) -I.. -I ../utils -c main.c

sinc_interpolation.o: sinc_interpolation.c
	gcc $(CFLAGS) -I.. -I ../utils -c sinc_interpolation.c

interleave.o: interleave.c
	gcc $(CFLAGS) -I.. -I ../utils -c interleave.c

copy_frame.o: copy_frame.c
	gcc $(CFLAGS) -I.. -I ../utils -c copy_frame.c

blend_fields.o: blend_fields.c
	gcc $(CFLAGS) -I.. -I ../utils -c blend_fields.c

transform_block.o: transform_block.c
	gcc $(CFLAGS) -I.. -I ../utils -c transform_block.c

clean:
	rm -f *.o *~
	rm -f yuvdeinterlace
