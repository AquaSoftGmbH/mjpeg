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
 * FILE: motionsearch_deint.c                              *
 *                                                         *
 ***********************************************************/

#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "motionsearch_deint.h"
#include "mjpeg_logging.h"
#include "motionsearch.h"
#include "transform_block.h"
#include "vector.h"

extern int width ;
extern int height ;
extern uint8_t *inframe[3];
extern uint8_t *outframe[3];
extern uint8_t *frame1[3];
extern uint8_t *frame2[3];
extern uint8_t *frame3[3];
extern uint8_t *reconstructed[3];

uint32_t mean_SAD = 0;

struct
{
  int x;
  int y;
}
pattern[255];
int pattern_length;

void
init_search_pattern (void)
{
  int x, y, r, cnt, i;
  int allready_in_path;

  cnt = 0;
  for (r = 0; r <= (6 * 6); r++)
    {
      for (y = -12; y <= 12; y +=2)
	for (x = -12; x <= 12; x++)
	  {
	    if (r >= (x * x + y * y))
	      {
		/* possible candidate */
		/* check if it's allready in the path */
		allready_in_path = 0;
		for (i = 0; i <= cnt; i++)
		  {
		    if (pattern[i].x == x && pattern[i].y == y)
		      {
			allready_in_path = 1;
		      }
		  }
		/* if it's not allready in the path, store it */
		if (allready_in_path == 0)
		  {
		    pattern[cnt].x = x;
		    pattern[cnt].y = y;
		    cnt++;
		  }
	      }
	  }
    }
  pattern_length = cnt + 1;
  mjpeg_log (LOG_INFO, "Search-Path-Lenght = %i", cnt + 1);
}

struct vector
search_forward_vector (int x, int y)
{
  uint32_t min, SAD;
  int dx, dy;
  int i;
  struct vector v;

  x -= 4;
  y -= 4;

  min = 100*psad_00
    (frame2[0] + (x) + (y) * width,
     frame1[0] + (x) + (y) * width, width, 16, 0x00ffffff);
  v.x = v.y = 0;
  min /= 350;
  
  if (min > mean_SAD)
    for (i = 1; i < pattern_length; i++)
      {
	dx = pattern[i].x;
	dy = pattern[i].y;

	SAD = psad_00
	  (frame2[0] + (x) + (y) * width,
	   frame1[0] + (x - dx) + (y - dy) * width, width, 16, 0x00ffffff);

	if (SAD < min)
	  {
	    min = SAD*100;
        min /= 100;
	    v.x = dx;
	    v.y = dy;
	  }
	if (min <= mean_SAD)
	  {
	    break;
	  }
      }

  v.min = min;
  return v;			/* return the found vector */
}

struct vector
search_backward_vector (int x, int y)
{
  uint32_t min, SAD;
  int dx, dy;
  struct vector v;
  int i;

  x -= 4;
  y -= 4;

  min = 100*psad_00
    (frame2[0] + (x) + (y) * width,
     frame3[0] + (x) + (y) * width, width, 16, 0x00ffffff);
  v.x = v.y = 0;
  min /= 350;
  
  if (min > mean_SAD)
    for (i = 1; i <= pattern_length; i++)
      {
	dx = pattern[i].x;
	dy = pattern[i].y;

	SAD = psad_00
	  (frame2[0] + (x) + (y) * width,
	   frame3[0] + (x + dx) + (y + dy) * width, width, 16, 0x00ffffff);

	if (SAD < min)
	  {
	    min = SAD*100;
        min /= 100;
	    v.x = dx;
	    v.y = dy;
	  }
	if (min <= mean_SAD)
	  {
	    break;
	  }
      }

  v.min = min;

  return v;			/* return the found vector */
}

struct vector
search_vector (int x, int y)
{
  uint32_t min, SAD;
  int dx=0, dy=0;
  struct vector v;
  int i;

  x -= 4;
  y -= 4;

  min = psad_00
    (frame2[0] + (x) + (y) * width,
     frame3[0] + (x) + (y) * width, width*2, 8, 0x00ffffff);
  min += psad_00
    (frame2[0] + (x) + (y) * width,
     frame1[0] + (x) + (y) * width, width*2, 8, 0x00ffffff);
  v.x = v.y = 0;
  
  if (min > (mean_SAD*6))
    for (i = 1; i <= pattern_length; i++)
      {
	dx = pattern[i].x;
	dy = pattern[i].y;

	SAD = psad_00
	  (frame2[0] + (x) + (y) * width,
	   frame3[0] + (x + dx) + (y + dy) * width, width*2, 8, 0x00ffffff);
	SAD += psad_00
	  (frame2[0] + (x) + (y) * width,
	   frame1[0] + (x - dx) + (y - dy) * width, width*2, 8, 0x00ffffff);

	if (SAD < min)
	  {
	    min = SAD;
	    v.x = dx;
	    v.y = dy;
	  }
	if (min <= mean_SAD)
	  {
	    break;
	  }
      }

    v.min = min;
	v.min23 = psad_00
	  (frame2[0] + (x) + (y) * width,
	   frame3[0] + (x + dx) + (y + dy) * width, width*2, 8, 0x00ffffff);
	v.min21 = psad_00
	  (frame2[0] + (x) + (y) * width,
	   frame1[0] + (x - dx) + (y - dy) * width, width*2, 8, 0x00ffffff);

  return v;			/* return the found vector */
}

void
motion_compensate_field (void)
{
  int x, y;
  struct vector fv;
  struct vector bv;
  float match_coeff21;
  float match_coeff23;

  match_coeff21 = 0;
  match_coeff23 = 0;
  for (y = 0; y < height; y += 8)
    {
      for (x = 0; x < width; x += 8)
	{
	  /* search the forward and backward motion-vector using
	   * an early terminating circular full-search. Please do
	   * not fiddle with the motion-search itself, as that one
	   * used was the only I could find which met the following
	   * conditions:
	   *
	   * a) consistent vectors
	   * b) shortest possible vectors
	   * c) near alias-free vectors
	   *
	   * So if you want to speed the deinterlacer at that level
	   * be warned. I have tested nearly everything (EPZS,HBM,TTS
	   * NTTS,LOGZ...) They all work well for an encoder but perform
	   * badly in a deinterlacer ! Even a plain FS does perform bad...
	   */

	  bv = fv = search_vector (x, y);

	  /* finally transform the macroblock using a blended variant of
	   * forward and backward block
	   */
	  transform_block
	    (reconstructed[0] + x + y * width,
	     frame1[0] + (x - fv.x) + (y - fv.y) * width,
	     frame3[0] + (x + bv.x) + (y + bv.y) * width, width);

	  transform_block_chroma
	    (reconstructed[1] + x / 2 + y / 2 * width / 2,
	     frame1[1] + (x - fv.x) / 2 + (y - fv.y) / 2 * width / 2,
	     frame3[1] + (x + bv.x) / 2 + (y + bv.y) / 2 * width / 2,
	     width / 2);

	  transform_block_chroma
	    (reconstructed[2] + x / 2 + y / 2 * width / 2,
	     frame1[2] + (x - fv.x) / 2 + (y - fv.y) / 2 * width / 2,
	     frame3[2] + (x + bv.x) / 2 + (y + bv.y) / 2 * width / 2,
	     width / 2);

	  /* Do some statistics on the SAD values. This might enable
	   * the deinterlacer to fish out scene-changes and mismatching
	   * macro-blocks
	   */
	  match_coeff21 += fv.min23;
	  match_coeff23 += fv.min21;
	}
    }
  match_coeff21 = match_coeff21 / match_coeff23;

  if (match_coeff21 < 0.25 || match_coeff21 > 4)
    {
      mjpeg_log (LOG_INFO, "Scene-Change or field-mismatch. Copying field.");
      memcpy (reconstructed[0], frame2[0], width * height);
      memcpy (reconstructed[1], frame2[1], width * height / 4);
      memcpy (reconstructed[2], frame2[2], width * height / 4);
    }
}
