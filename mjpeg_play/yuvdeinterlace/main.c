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

int BLKmedian=256;
int BLKquantile25=256;
int BLKquantile75=256;

int search_radius=48;
int verbose = 0;
int fast_mode = 0;
int field_order = -1;
int bttv_hack = 0;
int width = 0;
int height = 0;
int input_chroma_subsampling = 0;
int output_chroma_subsampling = 0;
int non_interleaved_fields = 0;

uint8_t fiy	[1024 * 768];
uint8_t ficr[1024 * 768];
uint8_t ficb[1024 * 768];
uint8_t *inframe[3] = { fiy, ficr, ficb };

uint8_t foy	[1024 * 768];
uint8_t focr[1024 * 768];
uint8_t focb[1024 * 768];
uint8_t *outframe[3] = { foy, focr, focb };

uint8_t f1y	[1024 * 768];
uint8_t f1cr[1024 * 768];
uint8_t f1cb[1024 * 768];
uint8_t *frame1[3] = { f1y, f1cr, f1cb };

uint8_t f2y	[1024 * 768];
uint8_t f2cr[1024 * 768];
uint8_t f2cb[1024 * 768];
uint8_t *frame2[3] = { f2y, f2cr, f2cb };

uint8_t f3y	[1024 * 768];
uint8_t f3cr[1024 * 768];
uint8_t f3cb[1024 * 768];
uint8_t *frame3[3] = { f3y, f3cr, f3cb };

uint8_t f4y	[1024 * 768];
uint8_t f4cr[1024 * 768];
uint8_t f4cb[1024 * 768];
uint8_t *frame4[3] = { f4y, f4cr, f4cb };


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

struct vector search_forward_vector(int , int , struct vector topleft , struct vector top, struct vector left, struct vector co);

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
	y4m_frame_info_t iframeinfo;
	y4m_stream_info_t istreaminfo;
	y4m_frame_info_t oframeinfo;
	y4m_stream_info_t ostreaminfo;

	blend_fields = &blend_fields_non_accel;

	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");
	mjpeg_log (LOG_INFO,
		   "       Motion-Compensating-Deinterlacer          ");
	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");

	while ((c = getopt (argc, argv, "hbfivr:s:")) != -1)
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
				   " -r [n=0...72] sets the serach-radius (default is 8)");
			mjpeg_log (LOG_INFO,
				   " -s [n=0/1] forces field-order");
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
		case 's':
		{
			field_order = atoi(optarg);
			if(field_order!=0) 
			{
				mjpeg_log (LOG_INFO,"forced top-field-first!");
				field_order=1;
			}
			else
			{
				mjpeg_log (LOG_INFO,"forced bottom-field-first!");
			}
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
		mjpeg_log (LOG_INFO,"FIXME: could use MMX/SSE Block/Frame-Copy/Blend if I had one ;-)");
	}

	/* initialize stream-information */
	y4m_accept_extensions(1);
	y4m_init_stream_info (&istreaminfo);
	y4m_init_frame_info (&iframeinfo);
	y4m_init_stream_info (&ostreaminfo);
	y4m_init_frame_info (&oframeinfo);

	/* open input stream */
	if ((errno = y4m_read_stream_header (fd_in, &istreaminfo)) != Y4M_OK)
	{
		mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
			   y4m_strerr (errno));
		exit (1);
	}

	/* get format information */
	width = y4m_si_get_width (&istreaminfo);
	height = y4m_si_get_height (&istreaminfo);
	input_chroma_subsampling = y4m_si_get_chroma (&istreaminfo);
	mjpeg_log (LOG_INFO, "Y4M-Stream is %ix%i(%s)",
		width,
		height,
		input_chroma_subsampling==Y4M_CHROMA_420JPEG  ? "4:2:0 MPEG1"   :
		input_chroma_subsampling==Y4M_CHROMA_420MPEG2 ? "4:2:0 MPEG2"   :
		input_chroma_subsampling==Y4M_CHROMA_420PALDV ? "4:2:0 PAL-DV"  :
		input_chroma_subsampling==Y4M_CHROMA_444      ? "4:4:4"         :
		input_chroma_subsampling==Y4M_CHROMA_422      ? "4:2:2"         :
		input_chroma_subsampling==Y4M_CHROMA_411      ? "4:2:0 NTSC-DV" :
		input_chroma_subsampling==Y4M_CHROMA_MONO     ? "MONOCHROME"    :
		input_chroma_subsampling==Y4M_CHROMA_444ALPHA ? "4:4:4:4 ALPHA" : 
		"unknown" );

	/* if chroma-subsampling isn't supported bail out ... */
	if( input_chroma_subsampling==Y4M_CHROMA_MONO     ||
		input_chroma_subsampling==Y4M_CHROMA_444ALPHA )
	{
		exit (-1);
	}

	/* check for the field-order or if the fieldorder was forced */
	if(field_order==-1)
	{
		if (Y4M_ILACE_TOP_FIRST == y4m_si_get_interlace (&istreaminfo))
		{
			mjpeg_log (LOG_INFO, "top-field-first");
			field_order = 1;
		}
		else if (Y4M_ILACE_BOTTOM_FIRST == y4m_si_get_interlace (&istreaminfo))
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
	}

	/* the output is not interlaced 4:2:0 MPEG 1 */
	y4m_si_set_interlace (&ostreaminfo, Y4M_ILACE_NONE);
	y4m_si_set_chroma    (&ostreaminfo, Y4M_CHROMA_444);
	y4m_si_set_width     (&ostreaminfo, width);
	y4m_si_set_height    (&ostreaminfo, height);
	y4m_si_set_framerate (&ostreaminfo, y4m_si_get_framerate(&istreaminfo));
	/* write the outstream header */
	y4m_write_stream_header (fd_out, &ostreaminfo);

	/* read every frame until the end of the input stream and process it */
	while (Y4M_OK == (errno = y4m_read_frame (fd_in,
						  &istreaminfo,
						  &iframeinfo, inframe)))
	{
		/* reconstruct 4:4:4 full resolution frame by upsampling luma and chroma of one field */
		if( input_chroma_subsampling == Y4M_CHROMA_420JPEG )
		{
			sinc_interpolation_420JPEG_to_444 (frame1,inframe,1-field_order);
			sinc_interpolation_420JPEG_to_444 (frame2,inframe,field_order);
		}
		else
			if( input_chroma_subsampling == Y4M_CHROMA_420MPEG2 )
			{
			}
			else
				if( input_chroma_subsampling == Y4M_CHROMA_420PALDV )
				{
				}
				else
					if( input_chroma_subsampling == Y4M_CHROMA_444 )
					{
					}
					else
						if( input_chroma_subsampling == Y4M_CHROMA_422 )
						{
						}
						else
							if( input_chroma_subsampling == Y4M_CHROMA_411 )
							{
							}			

		motion_compensate_field();
		blend_fields();

		/* write out reconstructed fields */
        //y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame1);
        y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, outframe);

		/* move fields */
		memcpy ( frame3[0], frame1[0], width*height );
		memcpy ( frame3[1], frame1[1], width*height );
		memcpy ( frame3[2], frame1[2], width*height );
		memcpy ( frame4[0], frame2[0], width*height );
		memcpy ( frame4[1], frame2[1], width*height );
		memcpy ( frame4[2], frame2[2], width*height );
	}

	/* did stream end unexpectedly ? */
	if (errno != Y4M_ERR_EOF)
		mjpeg_error_exit1 ("%s", y4m_strerr (errno));

	/* Exit gently */
	return (0);
}

int qsort_cmp_medianvector ( const void * x, const void * y )
{
	int a = *(const int*)x;
	int b = *(const int*)y;
	if (a<b)  return -1;
	else
		if (a>b)  return  1;
		else
			return 0;
}

int qsort_cmp_medianSAD ( const void * x, const void * y )
{
	int a = *(const uint32_t*)x;
	int b = *(const uint32_t*)y;
	if (a<b)  return -1;
	else
		if (a>b)  return  1;
		else
			return 0;
}

struct vector medianvector (struct vector v1, struct vector v2, struct vector v3)
{
	struct vector v;
	int xlist[4];
	int ylist[4];

	xlist[0] = v1.x;
	xlist[1] = v2.x;
	xlist[2] = v3.x;

	ylist[0] = v1.y;
	ylist[1] = v2.y;
	ylist[2] = v3.y;

	/* need to sort the vectors for median calculation */
	qsort ( xlist, 3, sizeof(xlist[0]), qsort_cmp_medianvector );
	qsort ( ylist, 3, sizeof(ylist[0]), qsort_cmp_medianvector );

	v.x = xlist[2];
	v.y = ylist[2];

	return v;
}

uint32_t medianSAD (uint32_t * SADlist, int n)
{
	qsort ( SADlist, n, sizeof(SADlist[0]), qsort_cmp_medianSAD);

	return (SADlist[n/2]+SADlist[n/2+1])>>1;
}

struct vector
search_forward_vector( int x, int y , struct vector topleft , struct vector top, struct vector left, struct vector co)
{
	static int initialized=0;
	static struct
	{
		int x;
		int y;
	} search_path[15000];
	static int cnt=0;

	struct vector v,w;
	struct vector mean;
	struct vector median;
	int vx,vy;
	int r=search_radius;
	uint32_t min,mmin;
	uint32_t SAD,SAD2;

	if(!initialized)
	{

		if(search_radius>72) search_radius = 72;

		for(r=1;r<=search_radius;r++)
		{
			for(vy=-64;vy<=64;vy+=1) 
				for(vx=-64;vx<=64;vx++)
				{
					if( (vx*vx+vy*vy) <= (r*r) &&
						(vx*vx+vy*vy) >  ((r-1)*(r-1)) )
					{
						search_path[cnt].x = vx;
						search_path[cnt].y = vy;
						cnt++;
					}
				}
		}
		initialized=1;
		mjpeg_log (LOG_INFO, "Number of maximum search positions : %i",cnt);
	}

	mean.x = (top.x + left.x + topleft.x + co.x )/4;
	mean.y = (top.y + left.y + topleft.y + co.y )/4;
	median = medianvector( topleft , top, left );

	/* initialize best-match with (0,0)MV SAD */
	min  = 	psad_00( frame1[0]+x+y*width, frame3[0]+x+y*width, width, 16, 0 );
	v.x  = 0;
	v.y  = 0;

	/* If we are in a region were we have predicted MVs check them first */
	if ( (x>>4)>0 && (y>>4)>0 )
	{
		/* check median predictor */
		SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+median.x)+(y+median.y)*width, width, 16, 0 );
		if (SAD<min)
		{
			min = SAD;
			v.x = median.x;
			v.y = median.y;
		}

		/* don't check the other predictors if the median is reasonably good */
		if(min>BLKmedian)
		{
			/* check colocated predictor */
			SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+co.x)+(y+co.y)*width, width, 16, 0 );
			if (SAD<min)
			{
				min = SAD;
				v.x = co.x;
				v.y = co.y;
			}
			/* check top predictor */
			SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+top.x)+(y+top.y)*width, width, 16, 0 );
			if (SAD<min)
			{
				min = SAD;
				v.x = top.x;
				v.y = top.y;
			}
			/* check left predictor */
			SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+left.x)+(y+left.y)*width, width, 16, 0 );
			if (SAD<min)
			{
				min = SAD;
				v.x = left.x;
				v.y = left.y;
			}
			/* check top-left predictor */
			SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+topleft.x)+(y+topleft.y)*width, width, 16, 0 );
			if (SAD<min)
			{
				min = SAD;
				v.x = topleft.x;
				v.y = topleft.y;
			}
			/* check mean predictor */
			SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+mean.x)+(y+mean.y)*width, width, 16, 0 );
			if (SAD<min)
			{
				min = SAD;
				v.x = mean.x;
				v.y = mean.y;
			}
		}
	}

	/* if one of the predictors is good, we should be below BLKquantile75, now
     * if not, do a circular full search for this block ... shit happens... :)
     */
	if(min>BLKquantile75)
	{
		min  = psad_00( frame1[0]+x+y*width, frame3[0]+x+y*width, width, 16, 0 );

		v.x = 0;
		v.y = 0;

		for(r = 0 ; r < cnt ; r++)
		{
			vx = search_path[r].x;
			vy = search_path[r].y;

			SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+vx)+(y+vy)*width, width, 16, 0 );
			if( SAD<min )
			{
				min   = SAD;
				v.x   = vx;
				v.y   = vy;
			}
			/* abort the search as soon, as we get below the median */
			if(min<(BLKmedian)) break;
		}
	}

	/* refine the search by a minimal search arround the found match */
	if(min>BLKquantile25)
		{
		w.x = v.x;
		w.y = v.y;
		for(r = 0 ; r <= 12 ; r++) // search radius 2pixel
			{
				vx = search_path[r].x+v.x;
				vy = search_path[r].y+v.y;
	
				SAD  = psad_00( frame1[0]+x+y*width, frame3[0]+(x+vx)+(y+vy)*width, width, 16, 0 );
				if( SAD<min )
				{
					min   = SAD;
					w.x   = vx;
					w.y   = vy;
				}
			}
		v.x = w.x;
		v.y = w.y;
		}

	/* we need the SAD of the current field for statistics, not of the upcoming-next one */
	v.sad  = psad_00( frame1[0]+x+y*width, frame2[0]+(x+v.x/2)+(y+v.y/2)*width, width, 16, 0 );
	return v;
}

void
motion_compensate_field (void)
{
	int x,y;
	static struct vector fv[45][36];
	struct vector bv[45][36];
	uint32_t SADlist[768*576/16/16];
	int cnt=0;
	
	for(y=0;y<height;y+=16)
	{
		for(x=0;x<width;x+=16)
		{
			fv[x>>4][y>>4] = 
					search_forward_vector ( x, y,
											fv[(x>>4)-1][(y>>4)-1],
											fv[x>>4][(y>>4)-1],
											fv[(x>>4)-1][y>>4],
											fv[x>>4][y>>4]);
			SADlist[cnt]=fv[x>>4][y>>4].sad;
			cnt++;
		}
	}

	BLKmedian     *= 15;
	BLKquantile25 *= 15;
	BLKquantile75 *= 15;
	BLKmedian     += medianSAD(SADlist,cnt);
	BLKquantile25 += SADlist[cnt/4];
	BLKquantile75 += SADlist[cnt/4*3];
	BLKmedian     >>= 4;
	BLKquantile25 >>= 4;
	BLKquantile75 >>= 4;
	
	/* limit values to lower-boundary to avoid unnessecary searches*/
	if(BLKquantile25 <  512) BLKquantile25 =  512;
	if(BLKmedian     < 1024) BLKmedian     = 1024;
	if(BLKquantile75 < 3840) BLKquantile75 = 3840;

	for(y=0;y<height;y+=16)
		for(x=0;x<width;x+=16)
		{
			if (fv[x>>4][y>>4].sad<=BLKquantile75)
			{
				transform_block_Y  (outframe[0]+x+y*width,
									frame2[0]+(x+fv[x>>4][y>>4].x/2)+(y+fv[x>>4][y>>4].y/2)*width, width );
				transform_block_Y  (outframe[1]+x+y*width,
									frame2[1]+(x+fv[x>>4][y>>4].x/2)+(y+fv[x>>4][y>>4].y/2)*width, width );
				transform_block_Y  (outframe[2]+x+y*width,
									frame2[2]+(x+fv[x>>4][y>>4].x/2)+(y+fv[x>>4][y>>4].y/2)*width, width );
			}
			else
			{
				transform_block_Y  (outframe[0]+x+y*width,
									frame1[0]+x+y*width, width );
				transform_block_Y  (outframe[1]+x+y*width,
									frame1[1]+x+y*width, width );
				transform_block_Y  (outframe[2]+x+y*width,
									frame1[2]+x+y*width, width );
			}
		}
}

