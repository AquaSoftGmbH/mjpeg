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
#include <string.h>

extern int width;
extern int height;

void
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int field)
{
  int x, y, v;
  int t1, t2, t3, t4, t5, t6, t7, t8;
  uint8_t *ip;
  uint8_t *op;

  t1 = -7 * width;
  t2 = -5 * width;
  t3 = -3 * width;
  t4 = -1 * width;
  t5 = +1 * width;
  t6 = +3 * width;
  t7 = +5 * width;
  t8 = +7 * width;

  memcpy (frame, inframe, width * height);

  /* fill up top-/bottom-out-of-range lines to avoid ringing */
  for (y = -7; y < 0; y ++)
    memcpy (frame+width*y,frame,width);
  for (y = height; y < (height+7); y ++)
    memcpy (frame+width*y,frame+(height-1)*width,width);

  ip = frame + (field) * width;
  op = frame + (field) * width;

  /* interpolate missing lines */ 
  for (y = 0; y < height ; y += 2)
    {
      for (x = 0; x < width; x++)
	{
	  v = -9 * *(ip + t1);
	  v += 21 * *(ip + t2);
	  v += -47 * *(ip + t3);
	  v += 163 * *(ip + t4);
	  v += 163 * *(ip + t5);
	  v += -47 * *(ip + t6);
	  v += 21 * *(ip + t7);
	  v += -9 * *(ip + t8);
	  v >>= 8;

	  /* Limit output */
      /* No, Matto, this is _not_ a bug. It's intentional. As I now know,
       * that 'limits' on YUV are to avoid a signal wich cannot be retrans-
       * formed to RGB 'without loss'... But on YCr'Cb' they're perfectly
       * legal...
       */
	  v = v > 255 ? 255 : v;
	  v = v < 0 ? 0 : v;
	  *(op) = v;

	  /* step one pixel */
	  ip++;
	  op++;
	}
      /* step one line */
      ip += width;
      op += width;
    }
}
