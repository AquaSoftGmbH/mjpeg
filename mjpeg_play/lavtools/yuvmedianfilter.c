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
 *    This filter look around the current point for a radius and averages
 *    this values that fall inside a threshold.
 */
#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"


int verbose = 1;
uint8_t	*input_frame[3];
uint8_t	*output_frame[3];

void	filter(int width, int height, uint8_t *const input[], uint8_t *output[]);
void	filter_buffer(int width, int height, int stride, int radius, int threshold, uint8_t * const input, uint8_t * const output);

int	threshold_luma = 2;
int	threshold_chroma = 2;

int	radius_luma = 2;
int	radius_chroma = 2;
int	interlace;
#define	NUMAVG	1024
int	avg_replace[NUMAVG];

static void Usage(char *name )
{
	fprintf(stderr,
		"Usage: %s: [-h] [-r num] [-R num] [-t num] [-T num] [-v num]\n"
                "-h   - Print out this help\n"
		"-r   - Radius for luma median (default: 2 pixels)\n"
		"-R   - Radius for chroma median (default: 2 pixels)\n"
		"-t   - Trigger luma threshold (default: 2 [0=disable])\n"
		"-T   - Trigger chroma threshold (default: 2 [0=disable])\n"
		"-v   - Verbosity [0..2]\n", name);
}
			
int
main(int argc, char *argv[])
{
	int	i;
	long long avg;
	int	input_fd = 0;
	int	output_fd = 1;
	int	horz;
	int	vert;
	int	c;
	int	frame_count;

	y4m_stream_info_t istream, ostream;
	y4m_frame_info_t iframe;

	while((c = getopt(argc, argv, "r:R:t:T:v:h")) != EOF) {
		switch(c) {
		case 'r':
			radius_luma = atoi(optarg);
			break;
		case 'R':
			radius_chroma = atoi(optarg);
			break;
		case 't':
			threshold_luma = atoi(optarg);
			break;
		case 'T':
			threshold_chroma = atoi(optarg);
			break;
		case 'v':
			verbose = atoi (optarg);
			if (verbose < 0 || verbose >2)
			{
				Usage (argv[0]);
				exit (1);
			}
			break;		  
			
		case 'h':
                        Usage (argv[0]);
		default:
			exit(0);
		}
	}

	if (radius_luma <= 0 || radius_chroma <= 0)
	   mjpeg_error_exit1("radius values must be > 0!");

	if (threshold_luma < 0 || threshold_chroma < 0)
	   mjpeg_error_exit1("threshold values must be >= 0!");

   (void)mjpeg_default_handler_verbosity(verbose);

	y4m_init_stream_info(&istream);
	y4m_init_stream_info(&ostream);
	y4m_init_frame_info(&iframe);

	i = y4m_read_stream_header(input_fd, &istream);
	if (i != Y4M_OK)
		mjpeg_error_exit1("Input stream error: %s", y4m_strerr(i));

	i = y4m_si_get_interlace(&istream);
	switch (i)
	{
	case Y4M_ILACE_NONE:
	     interlace = 0;
	     break;
	case Y4M_ILACE_BOTTOM_FIRST:
	case Y4M_ILACE_TOP_FIRST:
	     interlace = 1;
	     break;
	default:
	     mjpeg_warn("Unknown interlacing '%d', assuming non-interlaced", i);
	     interlace = 0;
	     break;
	}

	if( interlace && istream.height % 2 != 0 )
		mjpeg_error_exit1("Input images have odd number of lines - can't treats as interlaced!" );

	horz = istream.width;
	vert = istream.height;
	mjpeg_debug("width=%d height=%d luma_r=%d chroma_r=%d luma_t=%d chroma_t=%d", horz, vert, radius_luma, radius_chroma, threshold_luma, threshold_chroma);

	y4m_copy_stream_info(&ostream, &istream);

	input_frame[0] = malloc(horz * vert);
	input_frame[1] = malloc((horz / 2) * (vert / 2));
	input_frame[2] = malloc((horz / 2) * (vert / 2));

	output_frame[0] = malloc(horz * vert);
	output_frame[1] = malloc((horz / 2) * (vert / 2));
	output_frame[2] = malloc((horz / 2) * (vert / 2));


	y4m_write_stream_header(output_fd, &ostream);

	frame_count = 0;
	while (y4m_read_frame(input_fd, &istream, &iframe, input_frame) == Y4M_OK)
	{ 
		frame_count++;
		filter(horz, vert,  input_frame, output_frame);
		y4m_write_frame(output_fd, &ostream, &iframe, output_frame);
	}

	for (avg=0, i=0; i < NUMAVG; i++)
		avg += avg_replace[i];
	mjpeg_info("frames=%d avg=%lld", frame_count, avg);

	for (i=0; i < NUMAVG; i++) {
		mjpeg_debug( "%02d: %6.2f", i,
			(((double)avg_replace[i]) * 100.0)/(double)(avg));
	}

	y4m_fini_stream_info(&istream);
	y4m_fini_stream_info(&ostream);
	y4m_fini_frame_info(&iframe);
	exit(0);
}

void
filter(int width, int height, uint8_t * const input[], uint8_t *output[])
{
	if( interlace )
	{
		filter_buffer(width, height/2, width*2, 
					  radius_luma, threshold_luma, 
					  input[0], output[0]);
		filter_buffer(width, height/2, width*2, 
					  radius_luma, threshold_luma, 
					  input[0]+width, output[0]+width);

		filter_buffer(width/2, height/4, width, 
					  radius_chroma, threshold_chroma, 
					  input[1], output[1]);
		filter_buffer(width/2, height/4, width, 
					  radius_chroma, threshold_chroma, 
					  input[1]+width/2, output[1]+width/2);

		filter_buffer(width/2, height/4, width, 
					  radius_chroma, threshold_chroma, 
					  input[2], output[2]);
		filter_buffer(width/2, height/4, width,
					  radius_chroma, threshold_chroma, 
					  input[2]+width/2, output[2]+width/2);
	}
	else
	{
		filter_buffer(width, height, width, 
						  radius_luma, threshold_luma, 
						  input[0], output[0]);
		filter_buffer(width/2, height/2, width/2, 
					  radius_chroma, threshold_chroma, 
					  input[1], output[1]);
		filter_buffer(width/2, height/2, width/2, 
					  radius_chroma, threshold_chroma, 
					  input[2], output[2]);
	}
}

void
filter_buffer(int width, int height, int row_stride,
			  int radius, int threshold, uint8_t * const input, uint8_t * const output)
{
	int	reference;
	int	diff;
	int	a;
	int	b;
	uint8_t *pixel;
	int	total;
	int	count;
	int	radius_count;
	int	x;
	int	y;
	int	offset;
	int	min_count;
	uint8_t *inpix, *refpix;
	uint8_t *outpix;
	radius_count = radius + radius + 1;
	min_count = (radius_count * radius_count + 2)/3;
	

	if (threshold == 0)
	   {
           for (y = 0; y < height; y++)
	       memcpy(&output[y * row_stride], &input[y * row_stride], width);
	   return;
           }

	for(y=0; y < radius; y++)
		memcpy(&output[y * row_stride], &input[y * row_stride], width);

	for(y=height - radius; y < height; y++)
		memcpy(&output[y* row_stride], &input[y*row_stride], width);
	
	inpix = &input[0];
	outpix = &output[0];
	for(y=0; y < height; ++y)
	{
		a = 0;
		while(a<radius)
		{
			outpix[a] = inpix[a];
			++a;
			outpix[width-a] = inpix[width-a];
		}
		inpix += row_stride;
		outpix += row_stride;
	}

	count = 0;

	offset = radius*row_stride+radius;	/* Offset top-left of processing */
	                                /* Window to its centre */
	refpix = &input[offset];
	outpix = &output[offset];
	for(y=radius; y < height-radius; y++)
	{
		for(x=radius; x < width - radius; x++)
		{
			reference = *refpix;
			total = 0;
			count = 0;
			pixel = refpix-offset;
			b = radius_count;
			while( b > 0 )
			{
				a = radius_count;
				--b;
				while( a > 0 )
				{
					diff = reference - *pixel;
					--a;
					if (diff < threshold && diff > -threshold)
					{
						total += *pixel;
						count++;
					}
					++pixel;
				}
				pixel += (row_stride - radius_count);
			}
			++avg_replace[count];

			/*
			 * If we don't have enough samples to make a decent
			 * pseudo-median use a simple mean
			 */
			if (count <= min_count)
			{
				*outpix =  
					( ( (refpix[-row_stride-1] + refpix[-row_stride]) + 
						(refpix[-row_stride+1] +  refpix[-1]) 
						) 
					  + 
					  ( ((refpix[0]<<3) + 8 + refpix[1]) +
						(refpix[row_stride-1] + refpix[row_stride]) + 
						refpix[row_stride+1]
						  )
					 ) >> 4;
			} else {
				*outpix = total / count;
 			}
			++refpix;
			++outpix;
		}
		refpix += (row_stride-width+(radius*2));
		outpix += (row_stride-width+(radius*2));
	}
}
