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

/* write SADs out to "SAD-statistics.data"
 *
 * I used this, to verify the theory I had about the statistic distribution of 
 * good and perfect SADs. The distribution is not gaussian, so we can't use the
 * mean-value, to distinguish between good and bad hits -- but well, my first
 * approach to use the interquantile-distances (25% and 75% of the distribution)
 * was wrong, too... 
 * The only working solution I was able to find, was to compare to half and double
 * of the median-SAD. I don't think, that this is proper statistics, but ... it
 * seem's to work...
 *
 * #define STATFILE
 *
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

#ifdef STATFILE
FILE * statistics;
#endif

int BLKmedian    =256;

uint32_t predicted_count=0;
uint32_t search_count=0;

int search_radius=16;
int verbose = 0;
int fast_mode = 0;
int field_order = -1;
int bttv_hack = 0;
int width = 0;
int height = 0;
int input_chroma_subsampling = 0;
int output_chroma_subsampling = 0;
int non_interleaved_fields = 0;

struct vector forward_vector[768/16][576/16];
struct vector last_forward_vector[768/16][576/16];
struct vector last_forward_vector2[768/16][576/16];

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

uint8_t f3y_s1	[1024 * 768];
uint8_t f3cr_s1 [1024 * 768];
uint8_t f3cb_s1	[1024 * 768];
uint8_t *frame3_sub1[3] = { f3y_s1, f3cr_s1, f3cb_s1 };

uint8_t f4y_s1	[1024 * 768];
uint8_t f4cr_s1 [1024 * 768];
uint8_t f4cb_s1	[1024 * 768];
uint8_t *frame4_sub1[3] = { f4y_s1, f4cr_s1, f4cb_s1 };

uint8_t f3y_s2	[1024 * 768];
uint8_t f3cr_s2 [1024 * 768];
uint8_t f3cb_s2	[1024 * 768];
uint8_t *frame3_sub2[3] = { f3y_s2, f3cr_s2, f3cb_s2 };

uint8_t f4y_s2	[1024 * 768];
uint8_t f4cr_s2 [1024 * 768];
uint8_t f4cb_s2	[1024 * 768];
uint8_t *frame4_sub2[3] = { f4y_s2, f4cr_s2, f4cb_s2 };

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

struct vector search_forward_vector(int , int);
void subsample ( uint8_t *, uint8_t * );

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

#ifdef STATFILE
	statistics = fopen("SAD-statistics.data","w");
#endif

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
		input_chroma_subsampling==Y4M_CHROMA_411      ? "4:1:1 NTSC-DV" :
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
			sinc_interpolation_420JPEG_to_444 (frame1,inframe,field_order);
			sinc_interpolation_420JPEG_to_444 (frame2,inframe,1-field_order);
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

		/* subsample by 2 */
		subsample (frame4_sub1[0], frame4[0]);
		subsample (frame3_sub1[0], frame3[0]);

		/* subsample by 4 */
		subsample (frame4_sub2[0], frame4_sub1[0]);
		subsample (frame3_sub2[0], frame3_sub1[0]);

		motion_compensate_field();
		blend_fields();

		/* write out reconstructed fields */
		if(framenr++>0)
//	        y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame4_sub1);
//	        y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame3_sub1);
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

inline struct vector medianvector (struct vector v1, struct vector v2, struct vector v3)
{
	struct vector v;
	v.x = 	(v1.x <= v2.x && v1.x >= v3.x ) ? v1.x :
			(v2.x <= v1.x && v2.x >= v3.x ) ? v2.x :
			v3.x ;
	v.y = 	(v1.y <= v2.y && v1.y >= v3.y ) ? v1.y :
			(v2.y <= v1.y && v2.y >= v3.y ) ? v2.y :
			v3.y ;
	v.sad =	(v1.sad <= v2.sad && v1.sad >= v3.sad ) ? v1.sad :
			(v2.sad <= v1.sad && v2.sad >= v3.sad ) ? v2.sad :
			v3.sad ;
	return v;
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

uint32_t medianSAD (uint32_t * SADlist, int n)
{
	qsort ( SADlist, n, sizeof(SADlist[0]), qsort_cmp_medianSAD);

	return (SADlist[n/2]+SADlist[n/2+1])>>1;
}

inline uint32_t SADfunction ( uint8_t * f1, uint8_t * f2 )
{
	f1 -= 8+8*width;
	f2 -= 8+8*width;
	return psad_00( f1, f2, width, 32, 0 )+psad_00( f1+16, f2+16, width, 32, 0 );
}

struct vector
search_forward_vector( int x, int y )
{
	int dx,dy;
	int w2=width/2;
	int h2=height/2;
	int w4=width/4;
	int h4=height/4;
	int x2=x/2;
	int y2=y/2;
	int x4=x/4;
	int y4=y/4;
	struct vector v;
	struct vector me_candidates1[22];
	int cc1=0; // candidate count
	struct vector me_candidates2[12];
	int cc2=0; // candidate count
	int i;

	uint32_t SAD;
	uint32_t min;

	/* subsampled full-search with radius 16*4=64 */
	min = 0x00ffffff;
	cc1 = 0;
	for(dy=(-search_radius/4);dy<=(search_radius/4);dy++)
		for(dx=-search_radius/4;dx<=(search_radius/4);dx++)
		{
			SAD  = psad_sub44 ( frame4_sub2[0]+x4+y4*width, frame3_sub2[0]+(x4+dx)+(y4+dy)*width, width, 4 );

			if(SAD <= min)
			{
				min = SAD;
				me_candidates1[cc1].x = dx;
				me_candidates1[cc1].y = dy;
				cc1++;

				if(cc1==20) // roll over candidates...
				{
					cc1--;
					for(i=0;i<20;i++)
						me_candidates1[i] = me_candidates1[i+1];
				}
			} 
		}


	/* subsampled reduced full-search arround sub44 candidates */
	min = 0x00ffffff;
	cc2 = 0;
	while(cc1>-1)
	{
		for(dy=(me_candidates1[cc1].y*2-1);dy<=(me_candidates1[cc1].y*2+1);dy++)
			for(dx=(me_candidates1[cc1].x*2-1);dx<=(me_candidates1[cc1].x*2+1);dx++)
			{
				SAD  = psad_00 ( frame4_sub1[0]+x2+y2*width, frame3_sub1[0]+(x2+dx)+(y2+dy)*width, width, 16, 0 );

				if(SAD <= min)
				{
					min = SAD;
					me_candidates2[cc2].x = dx;
					me_candidates2[cc2].y = dy;
					cc2++;

					if(cc2==10) // roll over...
					{
						cc2--;
						for(i=0;i<10;i++)
							me_candidates2[i] = me_candidates2[i+1];
					}
				} 
			}
		cc1--;
	}

	/* reduced full-search arround sub22 candidates */
	min = 0x00ffffff;
	while(cc2>-1)
	{
		for(dy=(me_candidates2[cc2].y*2-1);dy<=(me_candidates2[cc2].y*2+1);dy++)
			for(dx=(me_candidates2[cc2].x*2-1);dx<=(me_candidates2[cc2].x*2+1);dx++)
			{
				SAD  = psad_00 ( frame4[0]+x+y*width, frame3[0]+(x+dx)+(y+dy)*width, width, 16, 0 );
				SAD += psad_00 ( frame4[1]+x+y*width, frame3[1]+(x+dx)+(y+dy)*width, width, 16, 0 );
				SAD += psad_00 ( frame4[2]+x+y*width, frame3[2]+(x+dx)+(y+dy)*width, width, 16, 0 );

				if(SAD<min)
				{
					min = SAD;
					v.x = dx;
					v.y = dy;
					v.sad = min;
				} 
			}
		cc2--;
	}

	return v;
}

void
motion_compensate_field (void)
{
	int x,y;
	int xi,yi;
	uint32_t SADlist[768*576/16/16];
	int cnt=0;
	
	/* search the vectors */
	for(y=0;y<height;y+=16)
	{
		for(x=0;x<width;x+=16)
		{
			xi = x>>4;
			yi = y>>4;

			forward_vector[xi][yi] = search_forward_vector ( x, y );
			SADlist[cnt]=forward_vector[xi][yi].sad;
			cnt++;
		}
	}
	/* copy the found vectors in the previous-vectors list 
	 * this allows reusage in the vector-prediction
     */
	memcpy ( last_forward_vector2, last_forward_vector, sizeof(forward_vector) );
	memcpy ( last_forward_vector, forward_vector, sizeof(forward_vector) );

	/* calculate some Block-Statistics to have a measure how good a block is */
	BLKmedian     = medianSAD(SADlist,cnt);
	
	mjpeg_log(LOG_INFO," median SAD = %i -- (P)%i:(S)%i",BLKmedian,predicted_count,search_count);

#ifdef STATFILE
	for(x=0;x<(width*height/16/16);x++)
		fprintf(statistics,"%i\n",SADlist[x]/20);
#endif

	predicted_count=search_count=0;
	for(y=0;y<height;y+=16)
		for(x=0;x<width;x+=16)
		{
			xi = x>>4;
			yi = y>>4;

			if (forward_vector[xi][yi].sad<=(BLKmedian*3) )
			{
				int dx = forward_vector[xi][yi].x/2;
				int dy = forward_vector[xi][yi].y/2;
			
				transform_block_Y  (outframe[0]+x+y*width,
									frame3[0]+(x+dx)+(y+dy)*width, width );
				transform_block_Y  (outframe[1]+x+y*width,
									frame3[1]+(x+dx)+(y+dy)*width, width );
				transform_block_Y  (outframe[2]+x+y*width,
									frame3[2]+(x+dx)+(y+dy)*width, width );
			}
			else
			{
				transform_block_Y  (outframe[0]+x+y*width,
									frame4[0]+x+y*width, width );
				transform_block_Y  (outframe[1]+x+y*width,
									frame4[1]+x+y*width, width );
				transform_block_Y  (outframe[2]+x+y*width,
									frame4[2]+x+y*width, width );
			}

			/* draw vectors */
//#define DEBUG
#ifdef DEBUG
			{
				int lx;
				float ly;
	
				if(forward_vector[xi][yi].x>0)
				{
					ly=0;
					for(lx=0;lx<=forward_vector[xi][yi].x;lx++)
					{
						*(outframe[0]+(x+8)+lx+((int)ly+y+8)*width)=255;
						*(outframe[2]+(x+8)+lx+((int)ly+y+8)*width)=255;
						ly += (float)forward_vector[xi][yi].y / (float)forward_vector[xi][yi].x;
					}
				}

				if(forward_vector[xi][yi].x<0)
				{
					ly=forward_vector[xi][yi].y;
					for(lx=forward_vector[xi][yi].x;lx<=0;lx++)
					{
						*(outframe[0]+(x+8)+lx+((int)ly+y+8)*width)=255;
						*(outframe[2]+(x+8)+lx+((int)ly+y+8)*width)=255;
						ly -= (float)forward_vector[xi][yi].y / (float)forward_vector[xi][yi].x;
					}
				}
				*(outframe[0]+(x+8)+(y+8)*width)=255;
				*(outframe[0]+(x+7)+(y+7)*width)=255;
				*(outframe[0]+(x+9)+(y+9)*width)=255;
				*(outframe[0]+(x+9)+(y+7)*width)=255;
				*(outframe[0]+(x+7)+(y+9)*width)=255;

			}
#endif
		}
}

void subsample ( uint8_t * dst, uint8_t * src )
{
  int x, y;
 
  uint8_t *s  = src;
  uint8_t *s2 = src+width;
  uint8_t *d  = dst;

  /* subsample component */

  for (y = 0; y < (height>>1); y += 1)
  {
      for (x = 0; x < width; x += 2)
      {
	  *(d + (x>>1)) =
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) + 2 )>>2;
      }
      s+=2*width;
      s2+=2*width;
      d+=width;
  }
}
