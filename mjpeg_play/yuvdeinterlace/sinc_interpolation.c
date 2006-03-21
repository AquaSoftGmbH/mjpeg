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
interpolate_field (uint8_t * frame, uint8_t * inframe, int w, int h,
		   int field)
{
  int x, y;
  int min;
  int e;
  int i;
  int a,b,c;

  for (y = field; y < h; y += 2)
    {
	memcpy (frame + (y) * w, inframe + y * w, w);

	for(x=0;x<w;x++)
	{
		a = *(inframe + (x  ) + (y  ) * w);
		c = *(inframe + (x  ) + (y+2) * w);
		a = (a+c)/2;

		// init edge-interpolation with plain vertical interpolation
		min  = abs(a-*(inframe + (x  ) + (y  ) * w));
		min += abs(a-*(inframe + (x  ) + (y+2) * w));
		b = a;

		// calculate diagonal interpolation nr.1
		i  = *(inframe + (x-1) + (y  ) * w);
		i += *(inframe + (x+1) + (y+2) * w);
		i /= 2;

		// calculate it's error
		e  = abs(i-a);
		e += abs(i-*(inframe + (x-1) + (y  ) * w));
		e += abs(i-*(inframe + (x+1) + (y+2) * w));

		// if the error is less than before, use it...
		if(min>e)
		{
			b = i;
			min = e;
		}

		// calculate diagonal interpolation nr.2
		i  = *(inframe + (x+1) + (y  ) * w);
		i += *(inframe + (x-1) + (y+2) * w);
		i /= 2;

		// calculate it's error
		e  = abs(i-a);
		e += abs(i-*(inframe + (x+1) + (y  ) * w));
		e += abs(i-*(inframe + (x-1) + (y+2) * w));

		// if the error is less than before, use it...
		if(min>e)
		{
			b = i;
			min = e;
		}

		// calculate diagonal interpolation nr.3
		i  = *(inframe + (x-2) + (y  ) * w);
		i += *(inframe + (x+2) + (y+2) * w);
		i /= 2;

		// calculate it's error
		e  = abs(i-a);
		e += abs(i-*(inframe + (x-2) + (y  ) * w));
		e += abs(i-*(inframe + (x+2) + (y+2) * w));

		// if the error is less than before, use it...
		if(min>e)
		{
			b = i;
			min = e;
		}

		// calculate diagonal interpolation nr.4
		i  = *(inframe + (x+2) + (y  ) * w);
		i += *(inframe + (x-2) + (y+2) * w);
		i /= 2;

		// calculate it's error
		e  = abs(i-a);
		e += abs(i-*(inframe + (x+2) + (y  ) * w));
		e += abs(i-*(inframe + (x-2) + (y+2) * w));

		// if the error is less than before, use it...
		if(min>e)
		{
			b = i;
			min = e;
		}

		*(frame +x+(y + 1) *w) = b;
	}
    }

	// no green lines...

	if(field==1)
		memcpy (frame,inframe+w,w);
	else
		memcpy (frame+(w*h-w),inframe+(w*h-w),w);
}

void
bandpass_field (uint8_t * frame, uint8_t * inframe, int w, int h)
{
  int x, y;
  int b;

  for (y = 0; y < h; y++)
    {
      for(x=0;x<w;x++)
	{
		b  = *(inframe+(x)+(y -7)*w)* -9;
		b += *(inframe+(x)+(y -6)*w)*-14;
		b += *(inframe+(x)+(y -5)*w)*-12;
		b += *(inframe+(x)+(y -4)*w)*  0;
		b += *(inframe+(x)+(y -3)*w)* 20;
		b += *(inframe+(x)+(y -2)*w)* 43;
		b += *(inframe+(x)+(y -1)*w)* 60;
		b += *(inframe+(x)+(y -0)*w)* 68;
		b += *(inframe+(x)+(y +1)*w)* 60;
		b += *(inframe+(x)+(y +2)*w)* 43;
		b += *(inframe+(x)+(y +3)*w)* 20;
		b += *(inframe+(x)+(y +4)*w)*  0;
		b += *(inframe+(x)+(y +5)*w)*-12;
		b += *(inframe+(x)+(y +6)*w)*-14;
		b += *(inframe+(x)+(y +7)*w)* -9;

		b /= 243;
		b = b>255? 255:b;
		b = b<0? 0:b;

	  	*(frame+x+(y)*w)=b;
	}
    }

  for (y = 0; y < h; y++)
    {
      for(x=0;x<w;x++)
	{
		b  = *(frame+(x-7)+(y)*w)* -9;
		b += *(frame+(x-6)+(y)*w)*-14;
		b += *(frame+(x-5)+(y)*w)*-12;
		b += *(frame+(x-4)+(y)*w)*  0;
		b += *(frame+(x-3)+(y)*w)* 20;
		b += *(frame+(x-2)+(y)*w)* 43;
		b += *(frame+(x-1)+(y)*w)* 60;
		b += *(frame+(x+0)+(y)*w)* 68;
		b += *(frame+(x+1)+(y)*w)* 60;
		b += *(frame+(x+2)+(y)*w)* 43;
		b += *(frame+(x+3)+(y)*w)* 20;
		b += *(frame+(x+4)+(y)*w)*  0;
		b += *(frame+(x+5)+(y)*w)*-12;
		b += *(frame+(x+6)+(y)*w)*-14;
		b += *(frame+(x+7)+(y)*w)* -9;

		b /= 243;
		b = b>255? 255:b;
		b = b<0? 0:b;

	  	*(frame+x+(y)*w)=b;
	}
    }
}

