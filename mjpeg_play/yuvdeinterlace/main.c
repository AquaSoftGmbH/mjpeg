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

#define BOTTOM_FIRST 0
#define TOP_FIRST 1

int both_fields = 0;
int search_radius = 32;
int verbose = 0;
int fast_mode = 0;
int field_order = -1;
int width = 0;
int height = 0;
int lwidth = 0;
int lheight = 0;
int cwidth = 0;
int cheight = 0;
int input_chroma_subsampling = 0;
int output_chroma_subsampling = 0;
int non_interleaved_fields = 0;
int use_film_fx = 0;

uint8_t *inframe[3];
uint8_t *frame1[3];
uint8_t *frame2[3];
uint8_t *frame3[3];
uint8_t *frame4[3];
uint8_t *r0[3];
uint8_t *r1[3];

int buff_offset;
int buff_size;

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void (*blend_fields) (uint8_t * dst[3], uint8_t * src[3]);
void upscale (uint8_t * dst, uint8_t * src, int w, int h);
void antialias (uint8_t * src[3]);
void
rotate_buffers (void)
{
  int i;
  uint8_t *p1;
  uint8_t *p2;

  for (i = 0; i < 3; i++)
    {
      p1 = frame3[i];
      p2 = frame4[i];

      frame4[i] = frame2[i];
      frame3[i] = frame1[i];

      frame1[i] = p1;
      frame2[i] = p2;
    }
}


/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
  char c;
  int fd_in = 0;
  int fd_out = 1;
  int errno = 0;
  y4m_frame_info_t iframeinfo;
  y4m_stream_info_t istreaminfo;
  y4m_frame_info_t oframeinfo;
  y4m_stream_info_t ostreaminfo;
  static uint32_t framenr;

  blend_fields = &blend_fields_non_accel;

  mjpeg_log (LOG_INFO, "-------------------------------------------------");
  mjpeg_log (LOG_INFO, "       Motion-Compensating-Deinterlacer          ");
  mjpeg_log (LOG_INFO, "-------------------------------------------------");

  while ((c = getopt (argc, argv, "hvds:")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, " Usage of the deinterlacer");
	    mjpeg_log (LOG_INFO, " -------------------------");
	    mjpeg_log (LOG_INFO, " -v be verbose");
	    mjpeg_log (LOG_INFO, " -d output both fields");
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
	case 'd':
	  {
	    both_fields = 1;
	    mjpeg_log (LOG_INFO,
		       "Regenerating both fields. Please fix the Framerate.");
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
  mjpeg_log (LOG_INFO, "Y4M-Stream is %ix%i(%s)", width, height,
	     y4m_chroma_keyword (input_chroma_subsampling));

  /* if chroma-subsampling isn't supported bail out ... */
  if (input_chroma_subsampling == Y4M_CHROMA_420JPEG ||
      input_chroma_subsampling == Y4M_CHROMA_420MPEG2 ||
      input_chroma_subsampling == Y4M_CHROMA_420PALDV)
    {
      lwidth = width;
      lheight = height;
      cwidth = width / 2;
      cheight = height / 2;
    }
  else if (input_chroma_subsampling == Y4M_CHROMA_444)
    {
      lwidth = width;
      lheight = height;
      cwidth = width;
      cheight = height;
    }
  else if (input_chroma_subsampling == Y4M_CHROMA_422)
    {
      lwidth = width;
      lheight = height;
      cwidth = width / 2;
      cheight = height;
    }
  else if (input_chroma_subsampling == Y4M_CHROMA_411)
    {
      lwidth = width;
      lheight = height;
      cwidth = width / 4;
      cheight = height;
    }
  else
    {
      mjpeg_log (LOG_ERROR,
		 "Y4M-Stream is not in a supported chroma-format. Sorry.");
      exit (-1);
    }

  /* the output is progressive 4:2:0 MPEG 1 */
  y4m_si_set_interlace (&ostreaminfo, Y4M_ILACE_NONE);
  y4m_si_set_chroma (&ostreaminfo, input_chroma_subsampling);
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

    frame1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame4[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame4[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame4[2] = buff_offset + (uint8_t *) malloc (buff_size);

    r0[0] = buff_offset + (uint8_t *) malloc (buff_size);
    r0[1] = buff_offset + (uint8_t *) malloc (buff_size);
    r0[2] = buff_offset + (uint8_t *) malloc (buff_size);

    r1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    r1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    r1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mjpeg_log (LOG_INFO, "Buffers allocated.");
  }

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (fd_in,
					    &istreaminfo,
					    &iframeinfo, inframe)))
    {
      /* rotate frames...
       * the deinterlacer heavily relies on knowing past fields, so
       * we need to save them. But instead of copying it is better to
       * just rotate the buffers, as this saves a lot cycles on the 
       * bus. memory transfer would cause a rather high load on the
       * machine...
       */
      rotate_buffers ();

      if (field_order == BOTTOM_FIRST)
	{
	  interpolate_field (frame1[0], inframe[0], lwidth, lheight, 0);
	  interpolate_field (frame1[1], inframe[1], cwidth, cheight, 0);
	  interpolate_field (frame1[2], inframe[2], cwidth, cheight, 0);

	  interpolate_field (frame2[0], inframe[0], lwidth, lheight, 1);
	  interpolate_field (frame2[1], inframe[1], cwidth, cheight, 1);
	  interpolate_field (frame2[2], inframe[2], cwidth, cheight, 1);

	  motion_compensate (r0[0], frame1[0], frame2[0], frame3[0], lwidth,
			     lheight, 0);
	  motion_compensate (r0[1], frame1[1], frame2[1], frame3[1], cwidth,
			     cheight, 0);
	  motion_compensate (r0[2], frame1[2], frame2[2], frame3[2], cwidth,
			     cheight, 0);

	  if (both_fields)
	    {
	      motion_compensate (r1[0], frame2[0], frame3[0], frame4[0],
				 lwidth, lheight, 1);
	      motion_compensate (r1[1], frame2[1], frame3[1], frame4[1],
				 cwidth, cheight, 1);
	      motion_compensate (r1[2], frame2[2], frame3[2], frame4[2],
				 cwidth, cheight, 1);
	    }
	}
      else
	{			// top field first
	  interpolate_field (frame1[0], inframe[0], lwidth, lheight, 1);
	  interpolate_field (frame1[1], inframe[1], cwidth, cheight, 1);
	  interpolate_field (frame1[2], inframe[2], cwidth, cheight, 1);

	  interpolate_field (frame2[0], inframe[0], lwidth, lheight, 0);
	  interpolate_field (frame2[1], inframe[1], cwidth, cheight, 0);
	  interpolate_field (frame2[2], inframe[2], cwidth, cheight, 0);

	  motion_compensate (r0[0], frame1[0], frame2[0], frame3[0], lwidth,
			     lheight, 1);
	  motion_compensate (r0[1], frame1[1], frame2[1], frame3[1], cwidth,
			     cheight, 1);
	  motion_compensate (r0[2], frame1[2], frame2[2], frame3[2], cwidth,
			     cheight, 1);

	  if (both_fields)
	    {
	      motion_compensate (r1[0], frame2[0], frame3[0], frame4[0],
				 lwidth, lheight, 0);
	      motion_compensate (r1[1], frame2[1], frame3[1], frame4[1],
				 cwidth, cheight, 0);
	      motion_compensate (r1[2], frame2[2], frame3[2], frame4[2],
				 cwidth, cheight, 0);
	    }
	}

      // blank 4 lines on top and bottom, as this algorithm can not recon-
      // struct them...
      memset (r0[0], 0, lwidth * 4);
      memset (r0[0] + (lwidth * lheight - 4 * lwidth), 0, lwidth * 4);
      memset (r0[1], 128, cwidth * 2);
      memset (r0[1] + (cwidth * cheight - 2 * cwidth), 128, cwidth * 2);
      memset (r0[2], 128, cwidth * 2);
      memset (r0[2] + (cwidth * cheight - 2 * cwidth), 128, cwidth * 2);

      if (both_fields)
	{
	  memset (r1[0], 0, lwidth * 4);
	  memset (r1[0] + (lwidth * lheight - 4 * lwidth), 0, lwidth * 4);
	  memset (r1[1], 128, cwidth * 2);
	  memset (r1[1] + (cwidth * cheight - 2 * cwidth), 128, cwidth * 2);
	  memset (r1[2], 128, cwidth * 2);
	  memset (r1[2] + (cwidth * cheight - 2 * cwidth), 128, cwidth * 2);
	}

      if (framenr > 0)
	{
	  if (both_fields)
	    y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, r1);
	  y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, r0);
	}

      framenr++;

    }

  /* free allocated buffers */
  {
    free (inframe[0] - buff_offset);
    free (inframe[1] - buff_offset);
    free (inframe[2] - buff_offset);

    free (frame1[0] - buff_offset);
    free (frame1[1] - buff_offset);
    free (frame1[2] - buff_offset);

    free (frame2[0] - buff_offset);
    free (frame2[1] - buff_offset);
    free (frame2[2] - buff_offset);

    free (frame3[0] - buff_offset);
    free (frame3[1] - buff_offset);
    free (frame3[2] - buff_offset);

    free (frame4[0] - buff_offset);
    free (frame4[1] - buff_offset);
    free (frame4[2] - buff_offset);

    free (r0[0] - buff_offset);
    free (r0[1] - buff_offset);
    free (r0[2] - buff_offset);

    free (r1[0] - buff_offset);
    free (r1[1] - buff_offset);
    free (r1[2] - buff_offset);

    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}

void
upscale (uint8_t * dst, uint8_t * src, int w, int h)
{
  // triangulation scaler
  int x, y;
  int m;
  int a, b, c, d;
  int ae, be, ce, de, me;

  for (y = 0; y <= h; y++)
    for (x = 0; x <= w; x++)
      {
	*(dst + (x * 2) + (y * 2) * (w * 2)) = *(src + x + y * w);
      }

  w *= 2;
  h *= 2;

  for (y = 1; y <= h; y += 2)
    for (x = 1; x <= w; x += 2)
      {
	// fill in the four neighbor-pixels
	a = *(dst + (x - 1) + (y - 1) * w);
	b = *(dst + (x + 1) + (y - 1) * w);
	c = *(dst + (x - 1) + (y + 1) * w);
	d = *(dst + (x + 1) + (y + 1) * w);

	// calculate the mean of the neighbors
	m = (a + b + c + d) / 4;

	// calculate the error for every neighbor-pixel
	ae = (m - a) * (m - a);
	be = (m - b) * (m - b);
	ce = (m - c) * (m - c);
	de = (m - d) * (m - d);

	// find the maximum error-value
	me = ae;
	me = (me < be) ? be : me;
	me = (me < ce) ? ce : me;
	me = (me < de) ? de : me;


	// generate mixing coefficients
	ae = me - ae;
	be = me - be;
	ce = me - ce;
	de = me - de;
	me = ae + be + ce + de;

	if (me != 0)
	  m = (a * ae + b * be + c * ce + d * de) / me;

	*(dst + x + y * w) = m;
      }

  for (y = 0; y <= h; y += 2)
    for (x = 1; x <= w; x += 2)
      {
	// fill in the four neighbor-pixels
	a = *(dst + (x - 1) + y * w);
	b = *(dst + (x + 1) + y * w);
	c = *(dst + x + (y - 1) * w);
	d = *(dst + x + (y + 1) * w);

	// calculate the mean of the neighbors
	m = (a + b + c + d) / 4;

	// calculate the error for every neighbor-pixel
	ae = (m - a) * (m - a);
	be = (m - b) * (m - b);
	ce = (m - c) * (m - c);
	de = (m - d) * (m - d);

	// find the maximum error-value
	me = ae;
	me = (me < be) ? be : me;
	me = (me < ce) ? ce : me;
	me = (me < de) ? de : me;


	// generate mixing coefficients
	ae = me - ae;
	be = me - be;
	ce = me - ce;
	de = me - de;
	me = ae + be + ce + de;

	if (me != 0)
	  m = (a * ae + b * be + c * ce + d * de) / me;

	*(dst + x + y * w) = m;
      }

  for (y = 1; y <= h; y += 2)
    for (x = 0; x <= w; x += 2)
      {
	// fill in the four neighbor-pixels
	a = *(dst + (x - 1) + y * w);
	b = *(dst + (x + 1) + y * w);
	c = *(dst + x + (y - 1) * w);
	d = *(dst + x + (y + 1) * w);

	// calculate the mean of the neighbors
	m = (a + b + c + d) / 4;

	// calculate the error for every neighbor-pixel
	ae = (m - a) * (m - a);
	be = (m - b) * (m - b);
	ce = (m - c) * (m - c);
	de = (m - d) * (m - d);

	// find the maximum error-value
	me = ae;
	me = (me < be) ? be : me;
	me = (me < ce) ? ce : me;
	me = (me < de) ? de : me;


	// generate mixing coefficients
	ae = me - ae;
	be = me - be;
	ce = me - ce;
	de = me - de;
	me = ae + be + ce + de;

	if (me != 0)
	  m = (a * ae + b * be + c * ce + d * de) / me;

	*(dst + x + y * w) = m;
      }
}
