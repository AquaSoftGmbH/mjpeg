#include "config.h"
#include "mjpeg_types.h"
#include "sinc_interpolation.h"
#include "motionsearch.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern int width;
extern int height;

void
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int field)
{
  int x, y, v;
  int t1,t2,t3,t4,t5,t6,t7,t8;
  uint8_t * ip;
  uint8_t * op;

  t1 = -7 * width;
  t2 = -5 * width;
  t3 = -3 * width;
  t4 = -1 * width;
  t5 = +1 * width;
  t6 = +3 * width;
  t7 = +5 * width;
  t8 = +7 * width;

  memcpy (frame, inframe, width * height);

  ip = inframe+(field)*width;
  op = frame+(field)*width;

  for (y = 0; y < (height+4); y += 2)
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
	  v = v > 255 ? 255 : v;
	  v = v < 0 ? 0 : v;
	  *(op) = v;
      
      /* step one pixel */
      ip++;
      op++;
	}
      /* step one line */
      ip+=width;
      op+=width;
    }
}
