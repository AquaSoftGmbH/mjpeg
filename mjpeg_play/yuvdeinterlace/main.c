/***********************************************************
 * YUVDEINTERLACER for the mjpegtools                      *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 *                                                         *
 * FILE: main.c                                            *
 *                                                         *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "motionsearch.h"
#include "sinc_interpolation.h"
#include "blend_fields.h"
#include "motionsearch_deint.h"

int search_radius = 32;
int verbose = 0;
int fast_mode = 0;
int field_order = -1;
int width = 0;
int height = 0;
int input_chroma_subsampling = 0;
int output_chroma_subsampling = 0;
int non_interleaved_fields = 0;
int use_film_fx = 0;

uint8_t *inframe[3];
uint8_t *outframe[3];
uint8_t *frame1[3];
uint8_t *frame2[3];
uint8_t *frame3[3];
uint8_t *reconstructed[3];

int buff_offset;
int buff_size;

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void (*blend_fields) (uint8_t * dst[3], uint8_t * src[3]);
void film_fx (void);

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
  int cpucap = cpu_accel ();
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

  while ((c = getopt (argc, argv, "fhvs:")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, " Usage of the deinterlacer");
	    mjpeg_log (LOG_INFO, " -------------------------");
	    mjpeg_log (LOG_INFO,
		      " You should use this program when you (as I do) prefer");
	    mjpeg_log (LOG_INFO,
		       " motion-compensation-artifacts over interlacing artifacts");
	    mjpeg_log (LOG_INFO,
		       " in your miniDV recordings. BTW, you can make them even");
	    mjpeg_log (LOG_INFO,
		       " more to apear like 35mm film recordings, when you lower");
	    mjpeg_log (LOG_INFO,
		       " the saturation a little (75%%-80%%) and raise the contrast");
	    mjpeg_log (LOG_INFO,
		       " of the luma-channel. You may (if you don't care of the");
	    mjpeg_log (LOG_INFO, " bitrate) add some noise and blur the result with");
	    mjpeg_log (LOG_INFO, " y4mspatialfilter (but not too much ...).");
	    mjpeg_log (LOG_INFO, "");
	    mjpeg_log (LOG_INFO, " y4mdeinterlace understands the following options:");
	    mjpeg_log (LOG_INFO, "");
	    mjpeg_log (LOG_INFO, " -v verbose/debug");
	    mjpeg_log (LOG_INFO,
		       " -f film-FX processing to give DV a more film-like-look...");
	    mjpeg_log (LOG_INFO,
		       " -s [n=0/1] forces field-order in case of misflagged streams");
	    mjpeg_log (LOG_INFO, "    -s0 is top-field-first");
	    mjpeg_log (LOG_INFO, "    -s1 is bottom-field-first");
	    exit (0);
	    break;
	  }
	case 'v':
	  {
	    verbose = 1;
	    break;
	  }
	case 'f':
	  {
	    use_film_fx = 1;
	    mjpeg_log (LOG_INFO,
		       "Film-FX turned on ... love it or leave it ...");
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
#if 0
      mjpeg_log (LOG_INFO,
		 "FIXME: could use MMX/SSE Block/Frame-Copy/Blend if I had one ;-)");
#endif
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

/* check for field dominance */

  if (field_order == -1)
    {
      /* field-order was not specified on commandline. So we try to
       * get it from the stream itself...
       */

      if (y4m_si_get_interlace (&istreaminfo) == Y4M_ILACE_TOP_FIRST)
	{
	  /* got it: Top-field-first... */
	  mjpeg_log (LOG_INFO, " Stream is interlaced, top-field-first.");
	  field_order = 1;
	}
      else if (y4m_si_get_interlace (&istreaminfo) == Y4M_ILACE_BOTTOM_FIRST)
	{
	  /* got it: Bottom-field-first... */
	  mjpeg_log (LOG_INFO, " Stream is interlaced, bottom-field-first.");
	  field_order = 0;
	}
      else
	{
	  mjpeg_log (LOG_ERROR,
		     "Unable to determine field-order from input-stream.  ");
	  mjpeg_log (LOG_ERROR,
		     "This is most likely the case when using mplayer to  ");
	  mjpeg_log (LOG_ERROR,
		     "produce my input-stream.                            ");
	  mjpeg_log (LOG_ERROR,
		     "                                                    ");
	  mjpeg_log (LOG_ERROR,
		     "Either the stream is misflagged or progressive...   ");
	  mjpeg_log (LOG_ERROR,
		     "I will stop here, sorry. Please choose a field-order");
	  mjpeg_log (LOG_ERROR,
		     "with -s0 or -s1. Otherwise I can't do anything for  ");
	  mjpeg_log (LOG_ERROR, "you. TERMINATED. Thanks...");
	  exit (-1);
	}
    }

  /* write the outstream header */
  y4m_write_stream_header (fd_out, &ostreaminfo);

  /* now allocate the needed buffers */
  {
    /* calculate the memory offset needed to allow the processing
     * functions to overshot. The biggest overshot is needed for the
     * MC-functions, so we'll use 8*width...
     */
    buff_offset = width * 8;
    buff_size = buff_offset * 2 + width * height;

    inframe[0] = buff_offset + (uint8_t *) malloc (buff_size);
    inframe[1] = buff_offset + (uint8_t *) malloc (buff_size);
    inframe[2] = buff_offset + (uint8_t *) malloc (buff_size);

    reconstructed[0] = buff_offset + (uint8_t *) malloc (buff_size);
    reconstructed[1] = buff_offset + (uint8_t *) malloc (buff_size);
    reconstructed[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mjpeg_log (LOG_INFO, "Buffers allocated.");
  }

  /* initialize motion-search-pattern */
  init_search_pattern ();

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (fd_in,
					    &istreaminfo,
					    &iframeinfo, inframe)))
    {

      /* store one field behind in a ringbuffer */

      memcpy (frame3[0], frame1[0], width * height);
      memcpy (frame3[1], frame1[1], width * height / 4);
      memcpy (frame3[2], frame1[2], width * height / 4);

      /* interpolate the missing field in the frame by 
       * bandlimited sinx/x interpolation 
       *
       * field-order-flag has the following meaning:
       * 0 == interpolate top field    
       * 1 == interpolate bottom field 
       */
      sinc_interpolation (frame1[0], inframe[0], 1 - field_order);
      sinc_interpolation (frame2[0], inframe[0], field_order);

      /* for the chroma-planes the function remains the same
       * just the dimension of the processed frame differs
       * so we temporarily adjust width and height
       */
      width >>= 1;
      height >>= 1;
      sinc_interpolation (frame1[1], inframe[1], 1 - field_order);
      sinc_interpolation (frame1[2], inframe[2], 1 - field_order);
      sinc_interpolation (frame2[1], inframe[1], field_order);
      sinc_interpolation (frame2[2], inframe[2], field_order);
      width <<= 1;
      height <<= 1;
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
      motion_compensate_field ();
      blend_fields (reconstructed, frame2);
      if (use_film_fx)
	{
	  film_fx ();
	}
      /* all left is to write out the reconstructed frame
       */
      y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, reconstructed);
//      y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame2);
//      y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame1);
    }

  /* free allocated buffers */
  {
    free (inframe[0] - buff_offset);
    free (inframe[1] - buff_offset);
    free (inframe[2] - buff_offset);

    free (reconstructed[0] - buff_offset);
    free (reconstructed[1] - buff_offset);
    free (reconstructed[2] - buff_offset);

    free (frame1[0] - buff_offset);
    free (frame1[1] - buff_offset);
    free (frame1[2] - buff_offset);

    free (frame2[0] - buff_offset);
    free (frame2[1] - buff_offset);
    free (frame2[2] - buff_offset);

    free (frame3[0] - buff_offset);
    free (frame3[1] - buff_offset);
    free (frame3[2] - buff_offset);

    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}

/*****************************************************************************
 * miniDV CCD-Cameras suffer mainly from two things, when compared to "real" *
 * film-cameras: 1. they're normaly too cool (meaning a slight shift towards *
 * blue and 2. the chroma is far to saturated. Real film instead very often  *
 * is tinted a very little inbetween yellow and red. Real film does not have *
 * such an oversaturated color. Despite these two differences the graduation *
 * if film is somewhat completely different in the middle-shadows and middle-*
 * highlights. So this is what the tables below try to mimik...              *
 * BTW: if you have good other tables, feel free to send them. I'll build 'em*
 * in. What about having some film-material presets?                         *
 *****************************************************************************/

void
film_fx (void)
{
  static int LUT_Y[256] = { 0, 0, 2, 3, 5, 6, 8, 9,
    11, 12, 14, 15, 17, 18, 20, 20,
    22, 23, 25, 26, 28, 28, 30, 31,
    33, 34, 35, 36, 38, 38, 40, 41,
    43, 43, 45, 46, 47, 48, 50, 50,
    52, 53, 54, 55, 56, 57, 59, 59,
    61, 61, 63, 63, 65, 65, 67, 67,
    69, 69, 71, 71, 73, 73, 75, 75,
    77, 77, 79, 79, 80, 81, 82, 83,
    84, 84, 86, 86, 88, 88, 89, 90,
    91, 91, 93, 93, 94, 95, 96, 96,
    98, 98, 99, 100, 101, 101, 102, 103,
    104, 104, 106, 106, 107, 107, 109, 109,
    110, 110, 112, 112, 113, 113, 115, 115,
    116, 116, 117, 118, 119, 119, 120, 121,
    122, 122, 123, 123, 125, 125, 126, 126,
    128, 128, 129, 129, 130, 131, 132, 132,
    133, 133, 135, 135, 136, 136, 138, 138,
    139, 139, 140, 141, 142, 142, 143, 144,
    145, 145, 146, 147, 148, 148, 149, 150,
    151, 151, 153, 153, 154, 154, 156, 156,
    157, 158, 159, 159, 161, 161, 162, 163,
    164, 164, 166, 166, 167, 168, 169, 170,
    171, 171, 173, 173, 175, 175, 176, 177,
    178, 179, 180, 181, 182, 183, 184, 185,
    186, 187, 188, 189, 190, 191, 192, 193,
    194, 195, 196, 197, 199, 199, 201, 201,
    203, 204, 205, 206, 208, 208, 210, 211,
    212, 213, 215, 216, 217, 218, 220, 220,
    222, 223, 225, 226, 227, 228, 230, 231,
    233, 234, 235, 236, 238, 239, 241, 242,
    244, 245, 247, 248, 250, 251, 253, 254,
  };

  static int LUT_Cr[256] = { 0, 0, 0, 0, 0, 1, 3, 4,
    6, 8, 9, 11, 12, 14, 16, 17,
    19, 20, 22, 23, 25, 26, 28, 29,
    30, 32, 33, 35, 36, 37, 39, 40,
    41, 42, 44, 45, 46, 48, 49, 50,
    51, 52, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 64, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 75,
    76, 77, 78, 79, 80, 81, 82, 83,
    83, 84, 85, 86, 87, 87, 88, 89,
    90, 91, 91, 92, 93, 94, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 104, 105, 106,
    106, 107, 108, 108, 109, 109, 110, 111,
    111, 112, 112, 113, 114, 114, 115, 115,
    116, 116, 117, 118, 118, 119, 119, 120,
    121, 121, 122, 122, 123, 123, 124, 125,
    125, 126, 126, 127, 127, 128, 129, 129,
    130, 130, 131, 132, 132, 133, 133, 134,
    135, 135, 136, 137, 137, 138, 138, 139,
    140, 140, 141, 142, 142, 143, 144, 145,
    145, 146, 147, 147, 148, 149, 150, 150,
    151, 152, 153, 154, 154, 155, 156, 157,
    158, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 166, 167, 168, 169, 170, 171,
    172, 173, 174, 175, 176, 177, 179, 180,
    181, 182, 183, 184, 185, 186, 187, 189,
    190, 191, 192, 193, 195, 196, 197, 199,
    200, 201, 202, 204, 205, 206, 208, 209,
    211, 212, 213, 215, 216, 218, 219, 221,
    222, 224, 225, 227, 229, 230, 232, 233,
    235, 237, 238, 240, 242, 243, 245, 247,
  };
  static int LUT_Cb[256] = { 6, 8, 10, 12, 13, 15, 17, 18,
    20, 22, 23, 25, 26, 28, 30, 31,
    33, 34, 36, 37, 39, 40, 42, 43,
    44, 46, 47, 49, 50, 51, 53, 54,
    55, 56, 58, 59, 60, 62, 63, 64,
    65, 66, 68, 69, 70, 71, 72, 73,
    74, 75, 76, 78, 79, 80, 81, 82,
    83, 84, 85, 86, 87, 88, 89, 89,
    90, 91, 92, 93, 94, 95, 96, 97,
    97, 98, 99, 100, 101, 101, 102, 103,
    104, 105, 105, 106, 107, 108, 108, 109,
    110, 110, 111, 112, 113, 113, 114, 115,
    115, 116, 117, 117, 118, 118, 119, 120,
    120, 121, 122, 122, 123, 123, 124, 125,
    125, 126, 126, 127, 128, 128, 129, 129,
    130, 130, 131, 132, 132, 133, 133, 134,
    135, 135, 136, 136, 137, 137, 138, 139,
    139, 140, 140, 141, 141, 142, 143, 143,
    144, 144, 145, 146, 146, 147, 147, 148,
    149, 149, 150, 151, 151, 152, 152, 153,
    154, 154, 155, 156, 156, 157, 158, 159,
    159, 160, 161, 161, 162, 163, 164, 164,
    165, 166, 167, 168, 168, 169, 170, 171,
    172, 172, 173, 174, 175, 176, 177, 178,
    179, 180, 180, 181, 182, 183, 184, 185,
    186, 187, 188, 189, 190, 191, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 203,
    204, 205, 206, 207, 209, 210, 211, 213,
    214, 215, 216, 218, 219, 220, 222, 223,
    225, 226, 227, 229, 230, 232, 233, 235,
    236, 238, 239, 241, 243, 244, 246, 247,
    248, 249, 250, 251, 252, 253, 254, 265,
  };
  int x, y, z;

  z = 0;
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
	*(reconstructed[0] + z) = LUT_Y[*(reconstructed[0] + z)];
	z++;
      }
  z = 0;
  for (y = 0; y < height / 2; y++)
    for (x = 0; x < width / 2; x++)
      {				// -120:+90 
	*(reconstructed[1] + z) = LUT_Cr[*(reconstructed[1] + z)];
	*(reconstructed[2] + z) = LUT_Cb[*(reconstructed[2] + z)];
	z++;
      }
}
