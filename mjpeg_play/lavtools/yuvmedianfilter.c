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
#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"

#define	PIXEL_AVG \
	diff = reference - *++pixel; \
	if (diff < threshold && diff > -threshold) { \
			total += *pixel; \
			count++; \
	}

int verbose = 1;
unsigned char	*input_frame[3];
unsigned char	*output_frame[3];

void	filter(int width, int height, unsigned char *input[], unsigned char *output[]);
void	filter_buffer(int width, int height, int radus, int threshold, unsigned char *input, unsigned char *output);

int	threshold_luma = 2;
int	threshold_chroma = 2;

int	radus_luma = 2;
int	radus_chroma = 2;

int	avg_replace[1024];
int	ovr_replace = 0;
int	chg_replace = 0;

static void Usage(char *name )
{
	fprintf(stderr,
			"Usage: %s: [-r num] [-t num] [-v num]\n"
			"-r   - Radius for median (default: 2 pixels)\n"
			"-t   - Trigger threshold (default: 2)\n"
			"-v   - Verbosity [0..2]\n", name);
}
			
int
main(int argc, char *argv[])
{
	int	i;
	int	avg;
	int	input_fd = 0;
	int	output_fd = 1;
	int	horz;
	int	vert;
	int	c;
	int	frame_count;
	y4m_stream_info_t istream, ostream;
	y4m_frame_info_t iframe;

	while((c = getopt(argc, argv, "r:t:v:")) != EOF) {
		switch(c) {
			case 'r':
				radus_luma = radus_chroma = atoi(optarg);
				break;
			case 't':
				threshold_luma = threshold_chroma = atoi(optarg);
				break;
		case 'v':
			verbose = atoi (optarg);
			if( verbose < 0 || verbose >2 )
			{
				Usage (argv[0]);
				exit (1);
			}
			break;		  
			
		default:
			exit(0);
		}
	}

   (void)mjpeg_default_handler_verbosity(verbose);

	y4m_init_stream_info(&istream);
	y4m_init_frame_info(&iframe);

	i = y4m_read_stream_header(input_fd, &istream);
	if (i != Y4M_OK)
		mjpeg_error_exit1("Input stream error: %s\n", y4m_strerr(i));

	horz = istream.width;
	vert = istream.height;
	mjpeg_debug( "width=%d height=%d\n", horz, vert);

	y4m_copy_stream_info(&ostream, &istream);

	input_frame[0] = alloca(horz * vert);
	input_frame[1] = alloca((horz / 2) * (vert / 2));
	input_frame[2] = alloca((horz / 2) * (vert / 2));

	output_frame[0] = alloca(horz * vert);
	output_frame[1] = alloca((horz / 2) * (vert / 2));
	output_frame[2] = alloca((horz / 2) * (vert / 2));


	y4m_write_stream_header(output_fd, &ostream);

	frame_count = 0;
	while (y4m_read_frame(input_fd, &istream, &iframe, input_frame) == Y4M_OK)
		{ 
		frame_count++;
		filter(horz, vert, input_frame, output_frame);
		y4m_write_frame(output_fd, &ostream, &iframe, output_frame);
		}

	for (avg=0, i=0; i < 64; i++)
		avg += avg_replace[i];

	mjpeg_info("\nframes=%d avg=%d replaced=%d\n", avg, chg_replace, ovr_replace);
	for (i=0; i < 64; i++) {
		mjpeg_debug( "%02d: %6.2f\n", i,
			(((double)avg_replace[i]) * 100.0)/(double)(avg));
	}
	exit(0);
}

void
filter(int width, int height, unsigned char *input[], unsigned char *output[])
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
