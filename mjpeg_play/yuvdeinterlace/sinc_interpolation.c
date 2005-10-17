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
 * FILE: sinc_interpolation.c                              *
 *                                                         *
 ***********************************************************/

#include "config.h"
#include "mjpeg_types.h"
#include "sinc_interpolation.h"
#include "motionsearch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int w, int h,
		    int field)
{
  int x, y, v;
  int dx, d, m, a, b, c;

  memcpy (frame, inframe, w * h);

  // fill top out-off-range lines to avoid ringing
  memcpy (frame - w * 1, inframe, w);
  memcpy (frame - w * 2, inframe, w);
  memcpy (frame - w * 3, inframe, w);
  memcpy (frame - w * 4, inframe, w);
  memcpy (frame - w * 5, inframe, w);

  // fill bottom out-off-range lines to avoid ringing
  memcpy (frame + w * (h + 0), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 1), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 2), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 3), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 4), inframe + w * (h - 1), w);

  /* interpolate missing lines */
  for (y = field; y < h; y += 2)
    {
      for (x = 0; x < w; x++)
	{
	  a = *(frame + (x) + (y - 1) * w);
	  b = (*(frame + (x) + (y - 5) * w) * +1 +
	       *(frame + (x) + (y - 3) * w) * -4 +
	       *(frame + (x) + (y - 1) * w) * 16 +
	       *(frame + (x) + (y + 1) * w) * 16 +
	       *(frame + (x) + (y + 3) * w) * -4 +
	       *(frame + (x) + (y + 5) * w) * +1) / 26;
	  b = b > 255 ? 255 : b;
	  b = b < 0 ? 0 : b;
	  c = *(frame + (x) + (y + 1) * w);

	  m = 255;
	  v = b;
	  for (dx = 0; dx < 2; dx++)
	    {
	      d =
		abs (*(frame + (x - dx) + (y - 1) * w) -
		     *(frame + (x + dx) + (y + 1) * w));
	      b =
		(*(frame + (x - dx) + (y - 5) * w) * +1 +
		 *(frame + (x - dx) + (y - 3) * w) * -4 + *(frame + (x - dx) +
							    (y -
							     1) * w) * 16 +
		 *(frame + (x + dx) + (y + 1) * w) * 16 + *(frame + (x + dx) +
							    (y +
							     3) * w) * -4 +
		 *(frame + (x + dx) + (y + 5) * w) * +1) / 26;
	      b = b > 255 ? 255 : b;
	      b = b < 0 ? 0 : b;

	      if (m > d && ((a < b && b < c) || (a > b && b > c)))
		{
		  m = d;
		  v = b;
		}

	      d =
		abs (*(frame + (x + dx) + (y - 1) * w) -
		     *(frame + (x - dx) + (y + 1) * w));
	      b =
		(*(frame + (x + dx) + (y - 5) * w) * +1 +
		 *(frame + (x + dx) + (y - 3) * w) * -4 + *(frame + (x + dx) +
							    (y -
							     1) * w) * 16 +
		 *(frame + (x - dx) + (y + 1) * w) * 16 + *(frame + (x - dx) +
							    (y +
							     3) * w) * -4 +
		 *(frame + (x - dx) + (y + 5) * w) * +1) / 26;
	      b = b > 255 ? 255 : b;
	      b = b < 0 ? 0 : b;

	      if (m > d && ((a < b && b < c) || (a > b && b > c)))
		{
		  m = d;
		  v = b;
		}
	    }

	  *(frame + x + y * w) = v;
	}
    }
}
