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
FILE *statistics;
#endif

int BLKmedian = 256;

uint32_t predicted_count = 0;
uint32_t search_count = 0;

int search_radius = 32;
int verbose = 0;
int fast_mode = 0;
int field_order = -1;
int width = 0;
int height = 0;
int input_chroma_subsampling = 0;
int output_chroma_subsampling = 0;
int non_interleaved_fields = 0;

struct vector forward_vector[768 / 16][576 / 16];
struct vector last_forward_vector[768 / 16][576 / 16];
struct vector last_forward_vector2[768 / 16][576 / 16];

uint8_t *inframe[3];
uint8_t *outframe[3];
uint8_t *frame1[3];
uint8_t *frame2[3];
uint8_t *frame3[3];
uint8_t *frame4[3];
uint8_t *frame1_[3];
uint8_t *frame2_[3];
uint8_t *reconstructed[3];

int buff_offset;
int buff_size;

struct vector mv_table[1024][1024];
//{
// int x;
// int y;
// uint32_t SAD;
//} mv_table[1024][1024];

struct vector
{
  int x;
  int y;
  uint32_t min;
  uint32_t min23;
  uint32_t min21;
};

struct
{
  int x;
  int y;
  uint32_t SAD;
} mv_table2[1024][1024];


struct vector search_forward_vector (int, int);

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void motion_compensate_field (void);
void (*blend_fields) (uint8_t * dst[3], uint8_t * src[3]);
void film_fx (void);

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
  int time;
  int cpucap = cpu_accel ();
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
  statistics = fopen ("SAD-statistics.data", "w");
#endif

  blend_fields = &blend_fields_non_accel;

  mjpeg_log (LOG_INFO, "-------------------------------------------------");
  mjpeg_log (LOG_INFO, "       Motion-Compensating-Deinterlacer          ");
  mjpeg_log (LOG_INFO, "-------------------------------------------------");

  while ((c = getopt (argc, argv, "hvr:s:")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, "\n\n"
		       " Usage of the deinterlacer                                  \n"
		       " -------------------------                                  \n"
		       "                                                            \n"
		       " You should use this program when you (as I do) prefer      \n"
		       " motion-compensation-artefacts over interlacing artefacts   \n"
		       " in your miniDV recordings. BTW, you can make them even     \n"
		       " more to apear like 35mm film recordings, when you lower    \n"
		       " the saturation a little (75%%-80%%) and raise the contrast \n"
		       " of the luma-channel. You may (if you don't care of the     \n"
		       " bitrate) add some noise and blurr the result with          \n"
		       " y4mspatialfilter (but not too much ...).                   \n"
		       "                                                            \n"
		       " y4mdeinterlace understands the following options:          \n"
		       "                                                            \n"
		       " -v verbose/debug                                           \n"
		       " -r [n=0...72] sets the serach-radius (default is 8)        \n"
		       " -s [n=0/1] forces field-order in case of misflagged streams\n"
		       "    -s0 is bottom-field-first                               \n"
		       "    -s1 is bottom-field-first                               \n");

	    exit (0);
	    break;
	  }
	case 'r':
	  {
	    search_radius = atoi (optarg);
	    mjpeg_log (LOG_INFO, "serach-radius set to %i", search_radius);
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
	    field_order = atoi (optarg);
	    if (field_order != 0)
	      {
		mjpeg_log (LOG_INFO, "forced top-field-first!");
		field_order = 1;
	      }
	    else
	      {
		mjpeg_log (LOG_INFO, "forced bottom-field-first!");
		field_order = 0;
	      }
	    break;
	  }
	}
    }

  /* initialize motion_library */
  init_motion_search ();

  /* initialize MMX transforms (fixme) */
  if ((cpucap & ACCEL_X86_MMXEXT) != 0 || (cpucap & ACCEL_X86_SSE) != 0)
    {
      mjpeg_log (LOG_INFO,
		 "FIXME: could use MMX/SSE Block/Frame-Copy/Blend if I had one ;-)");
    }

  /* initialize stream-information */
  y4m_accept_extensions (1);
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
	     input_chroma_subsampling == Y4M_CHROMA_420JPEG ? "4:2:0 MPEG1" :
	     input_chroma_subsampling == Y4M_CHROMA_420MPEG2 ? "4:2:0 MPEG2" :
	     input_chroma_subsampling ==
	     Y4M_CHROMA_420PALDV ? "4:2:0 PAL-DV" : input_chroma_subsampling
	     == Y4M_CHROMA_444 ? "4:4:4" : input_chroma_subsampling ==
	     Y4M_CHROMA_422 ? "4:2:2" : input_chroma_subsampling ==
	     Y4M_CHROMA_411 ? "4:1:1 NTSC-DV" : input_chroma_subsampling ==
	     Y4M_CHROMA_MONO ? "MONOCHROME" : input_chroma_subsampling ==
	     Y4M_CHROMA_444ALPHA ? "4:4:4:4 ALPHA" : "unknown");

  /* if chroma-subsampling isn't supported bail out ... */
  if (input_chroma_subsampling != Y4M_CHROMA_420JPEG)
    {
      mjpeg_log (LOG_ERROR,
		 "Y4M-Stream is not 4:2:0. Other chroma-modes currently not allowed. Sorry.");
      exit (-1);
    }

  /* the output is progressive 4:2:0 MPEG 1 */
  y4m_si_set_interlace (&ostreaminfo, Y4M_ILACE_NONE);
  y4m_si_set_chroma (&ostreaminfo, Y4M_CHROMA_420JPEG);
  y4m_si_set_width (&ostreaminfo, width);
  y4m_si_set_height (&ostreaminfo, height);
  y4m_si_set_framerate (&ostreaminfo, y4m_si_get_framerate (&istreaminfo));
  y4m_si_set_sampleaspect (&ostreaminfo,
			   y4m_si_get_sampleaspect (&istreaminfo));

/* fixme, test for field dominance */
	
//	mjpeg_log (LOG_INFO, "");

  /* write the outstream header */
  y4m_write_stream_header (fd_out, &ostreaminfo);

  /* now allocate the needed buffers */
  {
	/* calculate the memory offset needed to allow the processing
	 * functions to overshot. The biggest overshot is needed for the
	 * MC-functions, so we'll use 8*width...
	 */
	buff_offset = width*8;
	buff_size = buff_offset*2 + width*height;
	
    inframe[0] = buff_offset + malloc (buff_size);
    inframe[1] = buff_offset + malloc (buff_size);
    inframe[2] = buff_offset + malloc (buff_size);

    reconstructed[0] = buff_offset + malloc (buff_size);
    reconstructed[1] = buff_offset + malloc (buff_size);
    reconstructed[2] = buff_offset + malloc (buff_size);

    frame1[0] = buff_offset + malloc (buff_size);
    frame1[1] = buff_offset + malloc (buff_size);
    frame1[2] = buff_offset + malloc (buff_size);

    frame2[0] = buff_offset + malloc (buff_size);
    frame2[1] = buff_offset + malloc (buff_size);
    frame2[2] = buff_offset + malloc (buff_size);

    frame3[0] = buff_offset + malloc (buff_size);
    frame3[1] = buff_offset + malloc (buff_size);
    frame3[2] = buff_offset + malloc (buff_size);

    mjpeg_log (LOG_INFO, "Buffers allocated.");
  }

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (fd_in,
					    &istreaminfo,
					    &iframeinfo, inframe)))
    {

		/* store one field behind in a ringbuffer */
		
	  memcpy (frame3[0], frame1[0], width*height );
	  memcpy (frame3[1], frame1[1], width*height/4 );
	  memcpy (frame3[2], frame1[2], width*height/4 );

	/* interpolate the missing field in the frame by 
	 * bandlimited sinx/x interpolation 
	 *
	 * field-order-flag has the following meaning:
	 * 0 == interpolate top field    
	 * 1 == interpolate bottom field 
	 */
      {
      int x,y,v;
      for(y=0;y<height;y++)
          {
              for(x=0;x<width;x++)
                  {
                  v  = *(inframe[0]+(x-1)+(y  )*width);
                  v += *(inframe[0]+(x  )+(y-2)*width);
                  v += *(inframe[0]+(x+1)+(y  )*width);
                  v += *(inframe[0]+(x  )+(y+2)*width);
                  v += *(inframe[0]+(x  )+(y  )*width)*2;
                  v /= 6;
                  *(inframe[0]+x+y*width)=v;
                  }
                  y++;
              for(x=0;x<width;x++)
                  {
                  v  = *(inframe[0]+(x-1)+(y  )*width);
                  v += *(inframe[0]+(x  )+(y-2)*width);
                  v += *(inframe[0]+(x+1)+(y  )*width);
                  v += *(inframe[0]+(x  )+(y+2)*width);
                  v += *(inframe[0]+(x  )+(y  )*width)*2;
                  v /= 6;
                  *(inframe[0]+x+y*width)=v;
                  }
          }
      }
      sinc_interpolation (frame1[0], inframe[0], 1-field_order);
      sinc_interpolation (frame2[0], inframe[0], field_order);

	  /* for the chroma-planes the function remains the same
	   * just the dimension of the processed frame differs
	   * so we temporarily adjust width and height
	   */
      width >>= 1;
	  height >>= 1;
      sinc_interpolation (frame1[1], inframe[1], 1-field_order);
      sinc_interpolation (frame1[2], inframe[2], 1-field_order);
      sinc_interpolation (frame2[1], inframe[1], field_order);
      sinc_interpolation (frame2[2], inframe[2], field_order);
	  width <<=1;
	  height <<=1;
	/* at this stage we have three buffers containing the following
	 * sequence of fields:
	 *
	 * frame3
	 * frame2 current field (to be reconstructed frame)
	 * frame1
	 *
	 * So we motion-compensate frame3 and frame1 against frame2 and
	 * try to interpolate frame2 by blending the artificially generated
	 * frame into frame2 ...
	 */
      motion_compensate_field();
      blend_fields ( reconstructed, frame2 );
	 /* all left is to write out the reconstructed frame
      */
      y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, reconstructed);
    }

  /* free allocated buffers */
  {
    free (inframe[0]-buff_offset);
    free (inframe[1]-buff_offset);
    free (inframe[2]-buff_offset);

    free (reconstructed[0]-buff_offset);
    free (reconstructed[1]-buff_offset);
    free (reconstructed[2]-buff_offset);

    free (frame1[0]-buff_offset);
    free (frame1[1]-buff_offset);
    free (frame1[2]-buff_offset);

    free (frame2[0]-buff_offset);
    free (frame2[1]-buff_offset);
    free (frame2[2]-buff_offset);

    free (frame3[0]-buff_offset);
    free (frame3[1]-buff_offset);
    free (frame3[2]-buff_offset);

    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}

inline uint32_t
median3 (uint32_t a, uint32_t b, uint32_t c)
{
  return (a < b && b < c) ? b : (a < b && c < b) ? c : a;
}

inline uint32_t
min3 (uint32_t a, uint32_t b, uint32_t c)
{
  return (a <= b && a <= c) ? a : (b <= a && b <= c) ? b : c;
}

inline uint32_t
max3 (uint32_t a, uint32_t b, uint32_t c)
{
  return (a >= b && a >= c) ? a : (b >= a && b >= c) ? b : c;
}

struct vector
search_forward_vector (int x, int y)
{
	uint32_t min,SAD;
	int dx,dy;
	struct vector v;
		
    min = psad_sub22
      (frame2[0] + (x) + (y) * width,
       frame1[0] + (x) + (y) * width, width, 8);
	
    v.x = v.y = 0;
	
    for (dy = -6; dy <= +6; dy+=2)
      for (dx = -6; dx <= +6; dx++)
	{
	  SAD = psad_sub22
	    (frame2[0] + (x     ) + (y      ) * width,
	     frame1[0] + (x - dx) + (y - dy ) * width, width, 8);

	if (SAD <= min)
	    {
	      min = SAD;
	      v.x = dx;
	      v.y = dy;
	    }
	}
  v.min = min;

  return v;			/* return the found vector */
}

struct vector
search_backward_vector (int x, int y)
{
	uint32_t min,SAD;
	int dx,dy;
	struct vector v;
		
    min = psad_sub22
      (frame2[0] + (x) + (y) * width,
       frame3[0] + (x) + (y) * width, width, 8);
	
    v.x = v.y = 0;
	
    for (dy = -6; dy <= +6; dy+=2)
      for (dx = -6; dx <= +6; dx++)
	{
	  SAD = psad_sub22
	    (frame2[0] + (x     ) + (y      ) * width,
	     frame3[0] + (x + dx) + (y + dy ) * width, width, 8);

	if (SAD <= min)
	    {
	      min = SAD;
	      v.x = dx;
	      v.y = dy;
	    }
	}
  v.min = min;

  return v;			/* return the found vector */
}

struct vector
search_direct_vector (int x, int y)
{
	uint32_t min,SAD;
	int dx,dy;
	struct vector v;
		
    min = psad_sub22
      (frame1[0] + (x) + (y) * width,
       frame3[0] + (x) + (y) * width, width, 8);
	min += psad_sub22
	    (frame2[0] + (x) + (y) * width,
	     frame3[0] + (x) + (y) * width, width, 8);
	min += psad_sub22
	    (frame2[0] + (x) + (y) * width,
	     frame1[0] + (x) + (y) * width, width, 8);

	
    v.x = v.y = 0;
//	x -= 4;
//	y -= 4;
    /* search-radius may not exceed half of the macro-block-size ! */
    for (dy = -6; dy <= +6; dy+=2)
      for (dx = -6; dx <= +6; dx+=2)
	{
	  SAD = psad_sub22
	    (frame1[0] + (x - dx) + (y + dy ) * width,
	     frame3[0] + (x + dx) + (y + dy ) * width, width, 8);
	  SAD += psad_sub22
	    (frame2[0] + (x     ) + (y      ) * width,
	     frame3[0] + (x + dx) + (y + dy ) * width, width, 8);
	  SAD += psad_sub22
	    (frame2[0] + (x     ) + (y      ) * width,
	     frame1[0] + (x - dx) + (y - dy ) * width, width, 8);

	if (SAD <= min)
	    {
	      min = SAD;
	      v.x = dx;
	      v.y = dy;

	  v.min = min;
	  v.min23 = psad_sub22
	    (frame2[0] + (x     ) + (y      ) * width,
	     frame3[0] + (x + dx) + (y + dy ) * width, width, 8);
	  v.min21 = psad_sub22
	    (frame2[0] + (x     ) + (y      ) * width,
	     frame1[0] + (x - dx) + (y - dy ) * width, width, 8);
		}
	}

  return v;			/* return the found vector */
}

void
motion_compensate_field (void)
{
  int x, y;
  int dx, dy;
  int min;
  struct vector fv;
  struct vector bv;
  struct vector dv;
  int cnt1=0;
  int cnt2=0;
  float match_coeff21;
  float match_coeff23;
  
  /* search the vectors */
match_coeff21 = 0;
match_coeff23 = 0;
  
  for (y = 0; y < height; y += 8)
    {
      for (x = 0; x < width; x += 8)
	{
	  dv = search_direct_vector (x, y);
    
	  match_coeff21 += dv.min21;
	  match_coeff23 += dv.min23;
      
	  transform_block
	    (reconstructed[0] + x + y * width,
	     frame1[0] + (x - dv.x) + (y - dv.y) * width,
	     frame3[0] + (x + dv.x) + (y + dv.y) * width, width);
	transform_block_chroma
	    (reconstructed[1] + x/2 + y/2 * width/2,
	     frame1[1] + (x - dv.x)/2 + (y - dv.y)/2 * width/2,
	     frame3[1] + (x + dv.x)/2 + (y + dv.y)/2 * width/2, width/2);
		transform_block_chroma
	    (reconstructed[2] + x/2 + y/2 * width/2,
		frame1[2] + (x - dv.x)/2 + (y - dv.y)/2 * width/2,
	     frame3[2] + (x + dv.x)/2 + (y + dv.y)/2 * width/2, width/2);
	}
    }
    match_coeff21 = match_coeff21/match_coeff23;

    if(match_coeff21<0.5 || match_coeff21>2)
	{
        mjpeg_log (LOG_INFO, "Scene-Change or field-mismatch. Copying field.");
		memcpy( reconstructed[0], frame2[0], width*height );
		memcpy( reconstructed[1], frame2[1], width*height/4 );
		memcpy( reconstructed[2], frame2[2], width*height/4 );
	}
}
