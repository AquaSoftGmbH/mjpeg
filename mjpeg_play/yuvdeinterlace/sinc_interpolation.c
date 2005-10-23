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
#include <stdlib.h>
#include <math.h>

void
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int w, int h,
		    int field)
{
  int x, y, v;
  int dx, d,e, m, a, b, c;

int w1 = w*1;
int w2 = w*2;
int w3 = w*3;
int w4 = w*4;
int w5 = w*5;

	int dx1;
	int dx3;
	int dx5;
	
uint8_t * ptr = frame+field*w;
	
  // fill destination image with source image...
  memcpy (frame, inframe, w * h);

  // fill top out-off-range lines to avoid ringing
  memcpy (frame - w1, inframe, w);
  memcpy (frame - w2, inframe, w);
  memcpy (frame - w3, inframe, w);
  memcpy (frame - w4, inframe, w);
  memcpy (frame - w5, inframe, w);

  // fill bottom out-off-range lines to avoid ringing
  memcpy (frame + w * (h + 0), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 1), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 2), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 3), inframe + w * (h - 1), w);
  memcpy (frame + w * (h + 4), inframe + w * (h - 1), w);

  /* interpolate missing lines */
  for (y = 0; y < h; y += 2)
    {
      for (x = 0; x < w; x++)
	{
	  a  = *(ptr - w1);
		
	  b  = *(ptr - w5);
	  b -= *(ptr - w3) << 2;
	  b += *(ptr - w1) << 4;
	  b += *(ptr + w1) << 4;
	  b -= *(ptr + w3) << 2;
	  b += *(ptr + w5);
	  b /= 26;
		
	  c  = *(ptr + w1);

      b = b > 255 ? 255 : b;
	  b = b < 0 ? 0 : b;

	  m = 0x00ffffff;
	  v = b;
	  for (dx = 0; dx < 2; dx++)
	    {
		  dx1 = dx;
		  dx3 = dx*3;
		  dx5 = dx*5;
			
	      e  = *(ptr - dx - w1);
		  e -= *(ptr + dx + w1);
		  d  = e*e;	
	      e  = *(ptr - dx - w1 -1);
		  e -= *(ptr + dx + w1 -1);
		  d += e*e;	
	      e  = *(ptr - dx - w1 +1);
		  e -= *(ptr + dx + w1 +1);
		  d += e*e;	
					
		  b  = *(ptr - dx5 - w5);
		  b -= *(ptr - dx3 - w3) << 2;
		  b += *(ptr - dx1 - w1) << 4;
		  b += *(ptr + dx1 + w1) << 4;
		  b -= *(ptr + dx3 + w3) << 2;
		  b += *(ptr + dx5 + w5);
	  	  b /= 26;
			
	      if (m > d && ((a < b && b < c) || (a > b && b > c)))
		{
		  m = d;
		  v = b;
		}

		  dx1 = dx;
		  dx3 = dx*3;
		  dx5 = dx*5;
			
	      e  = *(ptr + dx - w1);
		  e -= *(ptr - dx + w1);
		  d  = e*e;	
	      e  = *(ptr + dx - w1 - 1);
		  e -= *(ptr - dx + w1 - 1);
		  d += e*e;	
	      e  = *(ptr + dx - w1 + 1);
		  e -= *(ptr - dx + w1 + 1);
		  d += e*e;	
					
		  b  = *(ptr + dx5 - w5);
		  b -= *(ptr + dx3 - w3) << 2;
		  b += *(ptr + dx1 - w1) << 4;
		  b += *(ptr - dx1 + w1) << 4;
		  b -= *(ptr - dx3 + w3) << 2;
		  b += *(ptr - dx5 + w5);
	  	  b /= 26;
			
	      if (m > d && ((a < b && b < c) || (a > b && b > c)))
		{
		  m = d;
		  v = b;
		}
	    }
		
	  *(ptr) = v;
	  ptr++;
	}
	ptr += w;
    }
}
