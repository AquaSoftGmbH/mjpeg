/***********************************************************
 * YUVDEINTERLACER for the mjpegtools                      *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 ***********************************************************/

/* dedicated to my grand-pa */

/* PLANS: I would like to be able to fill any kind of yuv4mpeg-format into 
 *        the deinterlacer. Mpeg2enc (all others, too??) doesn't seem to
 *        like interlaced material. Progressive frames compress much
 *        better... Despite that pogressive frames do look so much better
 *        even when viewed on a TV-Screen, that I really don't like to 
 *        deal with that weird 1920's interlacing sh*t...
 *
 *        I would like to support 4:2:2 PAL/NTSC recordings as well as 
 *        4:1:1 PAL/NTSC recordings to come out as correct 4:2:0 progressive-
 *        frames. I expect to get better colors and a better motion-search
 *        result, when using these two as the deinterlacer's input...
 *
 *        When I really get this working reliably (what I hope as I now own
 *        a digital PAL-Camcorder, but interlaced (progressive was to expen-
 *        sive *sic*)), I hope that I can get rid of that ugly interlacing
 *        crap in the denoiser's core. 
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "motionsearch.h"
#include "sinc_interpolation.h"
#include "interleave.h"
#include "copy_frame.h"
#include "blend_fields.h"
#include "transform_block.h"

struct vector search_backward_vector(int , int);
struct vector search_forward_vector(int , int );

int BLKthreshold=32*32*16;
int search_radius=8;
int verbose = 0;
int fast_mode = 0;
int field_order = 1;
int bttv_hack = 0;
int width = 0;
int height = 0;
int non_interleaved_fields = 0;

uint8_t f10y[1024 * 768];
uint8_t f10cr[512 * 384];
uint8_t f10cb[512 * 384];
uint8_t *frame10[3] = { f10y, f10cr, f10cb };

uint8_t f11y[1024 * 768];
uint8_t f11cr[512 * 384];
uint8_t f11cb[512 * 384];
uint8_t *frame11[3] = { f11y, f11cr, f11cb };

uint8_t f20y[1024 * 768];
uint8_t f20cr[512 * 384];
uint8_t f20cb[512 * 384];
uint8_t *frame20[3] = { f20y, f20cr, f20cb };

uint8_t f21y[1024 * 768];
uint8_t f21cr[512 * 384];
uint8_t f21cb[512 * 384];
uint8_t *frame21[3] = { f21y, f21cr, f21cb };

uint8_t f30y[1024 * 768];
uint8_t f30cr[512 * 384];
uint8_t f30cb[512 * 384];
uint8_t *frame30[3] = { f30y, f30cr, f30cb };

uint8_t f31y[1024 * 768];
uint8_t f31cr[512 * 384];
uint8_t f31cb[512 * 384];
uint8_t *frame31[3] = { f31y, f31cr, f31cb };

uint8_t f4y[1024 * 768];
uint8_t f4cr[512 * 384];
uint8_t f4cb[512 * 384];
uint8_t *frame4[3] = { f4y, f4cr, f4cb };

uint8_t f5y[1024 * 768];
uint8_t f5cr[512 * 384];
uint8_t f5cb[512 * 384];
uint8_t *frame5[3] = { f5y, f5cr, f5cb };

uint8_t f6y[1024 * 768];
uint8_t f6cr[512 * 384];
uint8_t f6cb[512 * 384];
uint8_t *frame6[3] = { f6y, f6cr, f6cb };

uint8_t f7y[1024 * 768];
uint8_t f7cr[512 * 384];
uint8_t f7cb[512 * 384];
uint8_t *frame_sub22[3] = { f7y, f7cr, f7cb };

uint8_t f8y[1024 * 768];
uint8_t f8cr[512 * 384];
uint8_t f8cb[512 * 384];
uint8_t *frame_sub44[3] = { f8y, f8cr, f8cb };

uint8_t f9y[1024 * 768];
uint8_t f9cr[512 * 384];
uint8_t f9cb[512 * 384];
uint8_t *frame_sub22_[3] = { f9y, f9cr, f9cb };

uint8_t fay[1024 * 768];
uint8_t facr[512 * 384];
uint8_t facb[512 * 384];
uint8_t *frame_sub44_[3] = { fay, facr, facb };

struct
{
	int x;
	int y;
	uint32_t SAD;
} mv_table[1024][1024];

struct
{
	int x;
	int y;
	uint32_t SAD;
} mv_table2[1024][1024];

struct vector
{
	int x;
	int y;
	uint32_t sad;
};

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void motion_compensate_field 	(void);
void (*blend_fields)			(void);

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
	int cpucap = cpu_accel();
	int framenr = 0;
	char c;
	int fd_in = 0;
	int fd_out = 1;
	int errno = 0;
	y4m_frame_info_t frameinfo;
	y4m_stream_info_t streaminfo;

	blend_fields = &blend_fields_non_accel;

	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");
	mjpeg_log (LOG_INFO,
		   "       Motion-Compensating-Deinterlacer          ");
	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");

	while ((c = getopt (argc, argv, "hbfivr:")) != -1)
	{
		switch (c)
		{
		case 'h':
		{
			mjpeg_log (LOG_INFO,
				   " -b activates the bttv-chroma-hack ");
			mjpeg_log (LOG_INFO,
				   " -f fast-mode (field-interpolation)");
			mjpeg_log (LOG_INFO,
				   " -v verbose/debug");
			mjpeg_log (LOG_INFO,
				   " -i non-interleaved-fields ");
			mjpeg_log (LOG_INFO,
				   "    this leads to a better quality when using mencoder to record");
			mjpeg_log (LOG_INFO,
				   " -r sets the serach-radius (default is 8)");
			exit (0);
			break;
		}
		case 'r':
		{
			search_radius = atoi(optarg);
			mjpeg_log (LOG_INFO,"serach-radius set to %i",search_radius);
			break;
		}
		case 'b':
		{
			bttv_hack = 1;
			break;
		}
		case 'f':
		{
			fast_mode = 1;
			break;
		}
		case 'i':
		{
			non_interleaved_fields = 1;
			break;
		}
		case 'v':
		{
			verbose = 1;
			break;
		}
		}
	}

	/* initialize motion_library */
	init_motion_search ();

	/* initialize MMX transforms (fixme) */
	if( (cpucap & ACCEL_X86_MMXEXT)!=0 ||
		(cpucap & ACCEL_X86_SSE   )!=0 )
	{
		mjpeg_log (LOG_INFO,"could use MMX/SSE Block/Frame-Copy/Blend if I had one ;-)");
	}

	/* initialize stream-information */
	y4m_init_stream_info (&streaminfo);
	y4m_init_frame_info (&frameinfo);

	/* open input stream */
	if ((errno = y4m_read_stream_header (fd_in, &streaminfo)) != Y4M_OK)
	{
		mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
			   y4m_strerr (errno));
		exit (1);
	}
	width = y4m_si_get_width (&streaminfo);
	height = y4m_si_get_height (&streaminfo);

	if (Y4M_ILACE_TOP_FIRST == y4m_si_get_interlace (&streaminfo))
	{
		mjpeg_log (LOG_INFO, "top-field-first");
		field_order = 1;
	}
	else if (Y4M_ILACE_BOTTOM_FIRST == y4m_si_get_interlace (&streaminfo))
	{
		mjpeg_log (LOG_INFO, "bottom-field-first");
		field_order = 0;
	}
	else
	{
		mjpeg_log (LOG_WARN, "stream either is marked progressive");
		mjpeg_log (LOG_WARN, "or stream has unknown field order");
		mjpeg_log (LOG_WARN, "using top-field-first");
		field_order = 1;
	}

	/* the output is not interlaced 4:2:0 */
	y4m_si_set_interlace (&streaminfo, Y4M_ILACE_NONE);

	/* write the outstream header */
	y4m_write_stream_header (fd_out, &streaminfo);

	/* read every single frame until the end of the input stream */
	while (Y4M_OK == (errno = y4m_read_frame (fd_in,
						  &streaminfo,
						  &frameinfo, frame11)))
	{

		if(non_interleaved_fields)
		{
			// interleave fields using frame10 as a buffer 
			interleave_fields (width,height,frame11,frame10);
		}
		/* copy the frame */

		memcpy (frame10[0], frame11[0], width * height);
		memcpy (frame10[1], frame11[1], width * height / 4);
		memcpy (frame10[2], frame11[2], width * height / 4);

		/* interpolate the other field */
		if (fast_mode == 0)
		{
			sinc_interpolation (frame10, field_order);
			sinc_interpolation (frame11, 1 - field_order);

			/* create working copy */

			copy_frame ( frame4, frame20 );

			/* deinterlace the frame in the middle of the ringbuffer */

			motion_compensate_field ();
			blend_fields ();

			/* write output-frame */
			if (framenr != 0)
			{
				y4m_write_frame (fd_out, &streaminfo,
						 &frameinfo, frame6);
				if(verbose == 1)
				mjpeg_log (LOG_INFO,"frame %i       ",framenr);
			}
			framenr++;
		}
		else
		{
			sinc_interpolation (frame10, field_order);

			y4m_write_frame (fd_out, &streaminfo, &frameinfo,
					 frame10);
		}

		/* rotate buffers */

		copy_frame ( frame31, frame21 );
		copy_frame ( frame30, frame20 );
		copy_frame ( frame21, frame11 );
		copy_frame ( frame20, frame10 );

	}

	/* did stream end unexpectedly ? */
	if (errno != Y4M_ERR_EOF)
		mjpeg_error_exit1 ("%s", y4m_strerr (errno));

	/* Exit gently */
	return (0);
}

struct vector
search_forward_vector( int x, int y )
{
	struct vector v;
	int vx,vy;
	int r=search_radius;
	uint32_t min;
	uint32_t SAD;

	min=0x00ffffff;
	for(vy = -r ; vy <= r ; vy += 2)
		for(vx = -r ; vx <= r ; vx += 2)
		{
			SAD  = psad_00( frame20[0]+x+y*width, frame10[0]+(x+vx)+(y+vy)*width, width, 32, 0 );
			SAD += psad_00( frame20[0]+x+y*width+16, frame10[0]+(x+vx)+(y+vy)*width+16, width, 32, 0 );
			if(SAD<min)
			{
				min   = SAD;
				v.x   = vx/2;
				v.y   = vy/2;
				v.sad = SAD;
			}
		}
	return v;
}

struct vector
search_backward_vector( int x, int y )
{
	struct vector v;
	int vx,vy;
	int r=search_radius;
	uint32_t min;
	uint32_t SAD;

	min=0x00ffffff;
	for(vy = -r ; vy <= r ; vy += 2)
		for(vx = -r ; vx <= r ; vx += 2)
		{
			SAD  = psad_00( frame20[0]+x+y*width, frame30[0]+(x+vx)+(y+vy)*width, width, 32, 0 );
			SAD += psad_00( frame20[0]+x+y*width+16, frame30[0]+(x+vx)+(y+vy)*width+16, width, 32, 0 );
			if(SAD<min)
			{
				min   = SAD;
				v.x   = vx/2;
				v.y   = vy/2;
				v.sad = SAD;
			}
		}
	return v;
}

void
motion_compensate_field (void)
{
	int x,y;
	struct vector fv;
	struct vector bv;
	
	for(y=0;y<height;y+=32)
		for(x=0;x<width;x+=32)
		{
			fv = search_forward_vector(x,y);
			bv = search_backward_vector(x,y);

			if(fv.sad < BLKthreshold || bv.sad < BLKthreshold)
			{
				if(fv.sad < bv.sad)
				{
					transform_block_Y  (frame5[0]+x+y*width , frame21[0]+(x+fv.x)+(y+fv.y)*width, width );
					transform_block_UV  (frame5[1]+x/2+y/2*width/2 , frame21[1]+(x+fv.x)/2+(y+fv.y)/2*width/2, width/2 );
					transform_block_UV  (frame5[2]+x/2+y/2*width/2 , frame21[2]+(x+fv.x)/2+(y+fv.y)/2*width/2, width/2 );
				}
				else
				{
					transform_block_Y  (frame5[0]+x+y*width , frame31[0]+(x+bv.x)+(y+bv.y)*width, width );
					transform_block_UV  (frame5[1]+x/2+y/2*width/2 , frame31[1]+(x+bv.x)/2+(y+bv.y)/2*width/2, width/2 );
					transform_block_UV  (frame5[2]+x/2+y/2*width/2 , frame31[2]+(x+bv.x)/2+(y+bv.y)/2*width/2, width/2 );
				}
			}
			else
			{
				transform_block_Y  (frame5[0]+x+y*width , frame20[0]+x+y*width, width );
				transform_block_UV  (frame5[1]+x/2+y/2*width/2 , frame20[1]+x/2+y/2*width/2, width/2 );
				transform_block_UV  (frame5[2]+x/2+y/2*width/2 , frame20[2]+x/2+y/2*width/2, width/2 );
			}
		}
}

