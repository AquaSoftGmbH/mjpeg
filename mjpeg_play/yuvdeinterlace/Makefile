all:	main.o sinc_interpolation.o interleave.o copy_frame.o blend_fields.o
	gcc -o yuvdeinterlace \
	       main.o \
	       sinc_interpolation.o \
	       interleave.o \
	       copy_frame.o \
	       blend_fields.o \
	       -pg -L ../utils -L ../utils/.libs -lmjpegutils -lmotion -lcpuaccel -lm

main.o:	main.c
	gcc -pg -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c main.c

sinc_interpolation.o: sinc_interpolation.c
	gcc -pg -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c sinc_interpolation.c

interleave.o: interleave.c
	gcc -pg -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c interleave.c

copy_frame.o: copy_frame.c
	gcc -pg -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c copy_frame.c

blend_fields.o: blend_fields.c
	gcc -pg -O6 -funroll-all-loops -finline-functions -Wall -I.. -I ../utils -c blend_fields.c

clean:
	rm -f *.o *~
	rm -f yuvdeinterlace
