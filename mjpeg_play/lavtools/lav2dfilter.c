#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define	CHROMA420 1
#define	CHROMA422 2
#define	CHROMA444 3

unsigned char	*input_frame[3];
unsigned char	*output_frame[3];

void	read_yuv(int fd, int *horizontal, int *vertical, int *frame_rate_code);
int	read_frame(int fd, int chroma, int width, int height, unsigned char *frame[]);
int	write_frame(int fd, int chroma, int width, int height, unsigned char *frame[]);
int	piperead(int fd, char *buf, int len);
void	filter(int width, int height, int chroma, unsigned char *input[], unsigned char *output[]);
void	filter_buffer(int width, int height, int radus, int threshold, unsigned char *input, unsigned char *output);

int	threshold_luma = 16;
int	threshold_chroma = 16;

int	radus_luma = 1;
int	radus_chroma = 1;

int
main(int argc, char *argv[])
{
	int	input_fd;
	int	output_fd;
	int	horz;
	int	vert;
	int	frame_rate;
	int	eof;
	int	frame_count;
	char	line[256];

	input_fd = 0;
	output_fd = 1;

	read_yuv(input_fd, &horz, &vert, &frame_rate);

	fprintf(stderr, "width=%d height=%d rate=%d\n", horz, vert, frame_rate);

	input_frame[0] = malloc(horz * vert);
	input_frame[1] = malloc(horz * vert);
	input_frame[2] = malloc(horz * vert);

	output_frame[0] = malloc(horz * vert);
	output_frame[1] = malloc(horz * vert);
	output_frame[2] = malloc(horz * vert);


	frame_count = 0;

	sprintf(line, "YUV4MPEG %d %d %d\n", horz, vert, frame_rate);
	write(output_fd, line, strlen(line));

	for(;;) {
		eof = read_frame(input_fd, CHROMA420, horz, vert, input_frame);
		if (eof) {
			break;
		}
		frame_count++;
		filter(horz, vert, CHROMA420, input_frame, output_frame);

		write_frame(output_fd, CHROMA420, horz, vert, output_frame);
	}
	fprintf(stderr, "%d frames read\n", frame_count);

	exit(0);
}

void
read_yuv(int fd, int *horizontal, int *vertical, int *frame_rate_code)
{
	int	n;
	char	buffer[256];

	for(n=0; n<255; n++) {
		if(!read(fd, &buffer[n], 1)) {
			fprintf(stderr,"Error reading header\n");
			exit(1);
		}

		if(buffer[n]=='\n') 
			break;
	}
	if(buffer[n] != '\n') {
		fprintf(stderr,"Didn't get linefeed in first %d characters of data\n",
				255);
		exit(1);
	}

	buffer[n] = 0; /* Replace linefeed by end of string */

	if(strncmp(buffer,"YUV4MPEG",8))
	{
		fprintf(stderr,"Input starts not with \"YUV4MPEG\"\n");
		fprintf(stderr,"This is not a valid input for me\n");
		exit(1);
	}


	sscanf(&buffer[8], "%d %d %d", horizontal, vertical, frame_rate_code);
}


int
read_frame(int fd, int chroma, int width, int height, unsigned char *frame[])
{
	char	buffer[256];
	int	v;
	int	h;
	int	i;

	if (piperead(fd, buffer, 6) != 6) {
		return 1;
	}


	if (memcmp(buffer, "FRAME\n", 6)) {
		fprintf(stderr, "\n\nStart of new frame missing (%.6s\n", buffer);
		exit(1);
	}

	v = height;
	h = width;
	for(i=0; i < v; i++) {
		if (piperead(fd, frame[0] + i * width, h) != h) {
			fprintf(stderr, "EOF while reading luma data\n");
			exit(1);
		}
	}
	

	v = chroma == CHROMA420 ? height/2 : height;
	h = chroma == CHROMA420 ? width/2 : width;
	for(i=0; i < v; i++) {
		if (piperead(fd, frame[1] + i * h, h) != h) {
			fprintf(stderr, "EOF while reading chroma u data\n");
			exit(1);
		}
	}
	for(i=0; i < v; i++) {
		if (piperead(fd, frame[2] + i * h, h) != h) {
			fprintf(stderr, "EOF while reading chroma v data\n");
			exit(1);
		}
	}

	return 0;
}

int
write_frame(int fd, int chroma, int width, int height, unsigned char *frame[])
{
	int	v;
	int	h;
	int	i;

	if (write(fd, "FRAME\n", 6) != 6) {
		fprintf(stderr, "\n\nCan not write Frame to outptu\n");
		exit(1);
	}

	v = height;
	h = width;
	for(i=0; i < v; i++) {
		if (write(fd, frame[0] + i * width, h) != h) {
			fprintf(stderr, "Error writting reading luma data\n");
			exit(1);
		}
	}
	

	v = chroma == CHROMA420 ? height/2 : height;
	h = chroma == CHROMA420 ? width/2 : width;
	for(i=0; i < v; i++) {
		if (write(fd, frame[1] + i * h, h) != h) {
			fprintf(stderr, "EOF while writting chroma u data\n");
			exit(1);
		}
	}
	for(i=0; i < v; i++) {
		if (write(fd, frame[2] + i * h, h) != h) {
			fprintf(stderr, "EOF while writting chroma v data\n");
			exit(1);
		}
	}

	return 0;
}

int
piperead(int fd, char *buf, int len)
{
	int	n;
	int	r;

	r = 0;
	n = 0;

	do {
		n = read(fd, buf + r, len-r);
		if (n == 0)
			return r;
		r += n;
	} while(r < len);

	return r;
}

void
filter(int width, int height, int chroma, unsigned char *input[], unsigned char *output[])
{
	filter_buffer(width, height, radus_luma, threshold_luma, input[0], output[0]);
	filter_buffer(width/2, height/2, radus_chroma, threshold_chroma, input[1], output[1]);
	filter_buffer(width/2, height/2, radus_chroma, threshold_chroma, input[2], output[2]);
}

void
filter_buffer(int width, int height, int radus, int threshold, unsigned char *input, unsigned char *output)
{
	int	x;
	int	y;
	int	diff;
	int	reference;
	int	total;
	int	count;
	int	lowx;
	int	lowy;
	int	highx;
	int	highy;
	int	a;
	int	b;
	unsigned char *pixel;

	for(y=0; y < height; y++) {
		for(x=0; x < width; x++) {
			lowx = -radus;
			highx = radus;

			lowy = - radus;
			highy = radus;

			if (x + lowx < 0)
				lowx = 0;
			if (x + highx > width-1)
				highx = (width - 1) - x;

			if (y + lowy < 0)
				lowy = 0;
			if (y + highy > height-1)
				highy = (height-1) - y;

			total = input[width * y + x];
			reference = total;
			count = 1;

			for(b=lowy; b < highy; b++) {
//				pixel = &input[y * width + lowx];
				for(a = lowx; a < highx; a++) {
					pixel = &input[(y + b) * width + x + lowx];
					diff = reference - *pixel;
					if (diff > threshold || diff < -threshold)
						continue;

					total += *pixel;
					count++;
				}
			}
			output[width * y + x] = total / count;
		}
	}
}
