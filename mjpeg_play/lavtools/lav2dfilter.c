/*
 *    Copyright (C) 2001 Mike Bernson <mike@mlb.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    Filter Based on code from Jim Cassburi filter: 2dclean
 *
 *    This filter look around the current point for a radus and averages
 *    this values that fall inside a threshold.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define	CHROMA420 1
#define	CHROMA422 2
#define	CHROMA444 3

#define	PIXEL_AVG \
	diff = reference - *++pixel; \
	if (diff < threshold && diff > -threshold) { \
			total += *pixel; \
			count++; \
	}


unsigned char	*input_frame[3];
unsigned char	*output_frame[3];

void	read_yuv(int fd, int *horizontal, int *vertical, int *frame_rate_code);
int	read_frame(int fd, int chroma, int width, int height, unsigned char *frame[]);
int	write_frame(int fd, int chroma, int width, int height, unsigned char *frame[]);
int	piperead(int fd, char *buf, int len);
void	filter(int width, int height, int chroma, unsigned char *input[], unsigned char *output[]);
void	filter_buffer(int width, int height, int radus, int threshold, unsigned char *input, unsigned char *output);

int	threshold_luma = 2;
int	threshold_chroma = 2;

int	radus_luma = 2;
int	radus_chroma = 2;

int	avg_replace[1024];
int	ovr_replace = 0;
int	chg_replace = 0;

int
main(int argc, char *argv[])
{
	int	i;
	int	avg;
	int	input_fd;
	int	output_fd;
	int	horz;
	int	vert;
	int	frame_rate;
	int	eof;
	int	c;
	int	frame_count;
	char	line[256];

	input_fd = 0;
	output_fd = 1;

	while((c = getopt(argc, argv, "r:t:")) != EOF) {

		switch(c) {
			case 'r':
				radus_luma = radus_chroma = atoi(optarg);
				break;
			case 't':
				threshold_luma = threshold_chroma = atoi(optarg);
				break;
			default:
				exit(0);
		}
	}
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

	for(avg=0, i=0; i < 64; i++)
		avg += avg_replace[i];

	fprintf(stderr, "\nframes=%d avg=%d replaced=%d\n", avg, chg_replace, ovr_replace);
	for(i=0; i < 64; i++) {
		if ((i % 8) == 0) fprintf(stderr, "\n");
		fprintf(stderr, "%2d-%6.2f\n", i,
			(((double)avg_replace[i]) * 100.0)/(double)(avg));
	}
	fprintf(stderr, "\n");

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
	int	reference;
	int	diff;
	int	a;
	int	b;
	unsigned char *pixel;
	int	total;
	int	count;
	int	radus_count;
	int	x;
	int	y;
	int	offset;
	int	min_count;

	radus_count = radus + radus + 1;
	min_count = (radus_count * radus_count + 2)/3;
	

	for(y=0; y < radus; y++)
		memcpy(&output[y * width], &input[width * radus], width);

	for(y=height - radus; y < height; y++)
		memcpy(&output[y* width], &input[width * (height - radus-1)], width);
	count = 0;

	for(y=radus; y < height-radus; y++) {
		for(x=radus; x < width - radus; x++) {
			reference = input[offset = width * y + x];
			total = 0;
			count = 0;
			pixel = &input[(y - radus) * width + x - radus - 1];
			for(b=radus_count; b--;) {
				for(a = radus_count; a--;) {
					diff = reference - *++pixel;
					if (diff < threshold && diff > -threshold) {
						total += *pixel;
						count++;
					}
				}
				pixel += (width - radus_count);

			}
			avg_replace[count]++;
			if (count <= min_count) {
				 output[offset] = 
					(input[offset - 1] +
					input[offset + 1] +
					input[offset - width] +
					input[offset + width]) >> 2;
			} else {
				output[offset] = total / count;
 			}
		}
	}
}
