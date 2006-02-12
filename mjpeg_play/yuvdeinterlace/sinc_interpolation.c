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

extern uint8_t *lp0;
extern uint8_t *lp1;
extern uint8_t *lp2;

void
prepare_field (uint8_t * frame, uint8_t * inframe, int w, int h,
		    int field)
{
  int x, y, v;
  uint8_t * s;
  uint8_t * d;

  for(y=field;y<h;y+=2)
	{
	memcpy ( frame+(y/2)*w, inframe+y*w, w);
	}

	if(field==0)
		{
		  	for(y=0;y<(h/2);y++)
				for(x=0;x<w;x++)
				{
					v  = *(frame+x+y*w+w*4) * 0.07;
					v += *(frame+x+y*w+w*3) * 0.38;
					v += *(frame+x+y*w+w*2) * 0.71;
					v += *(frame+x+y*w+w*1) * 0.94;
					v += *(frame+x+y*w    ) * 0.99;
        				v += *(frame+x+y*w-w*1) * 0.85;
        				v += *(frame+x+y*w-w*2) * 0.56;
        				v += *(frame+x+y*w-w*3) * 0.22;
        				v += *(frame+x+y*w-w*4) * -.06;
					v /= 4.66;
					v = v<0? 0:v;
					v = v>255? 255:v;
					*(frame+x+(h/2+y)*w) = v;
				}
		}
	else
		{
  			for(y=0;y<(h/2);y++)
				for(x=0;x<w;x++)
				{
					v  = *(frame+x+y*w-w*4) * 0.07;
					v += *(frame+x+y*w-w*3) * 0.38;
					v += *(frame+x+y*w-w*2) * 0.71;
					v += *(frame+x+y*w-w*1) * 0.94;
					v += *(frame+x+y*w    ) * 0.99;
        				v += *(frame+x+y*w+w*1) * 0.85;
        				v += *(frame+x+y*w+w*2) * 0.56;
        				v += *(frame+x+y*w+w*3) * 0.22;
        				v += *(frame+x+y*w+w*4) * -.06;
					v /= 4.66;
					v = v<0? 0:v;
					v = v>255? 255:v;
					*(frame+x+(h/2+y)*w) = v;
				}
		}

}

void
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int w, int h,
		    int field)
{
  int x, y, v;

  for(y=field;y<h;y+=2)
	{
	memcpy ( frame+y*w, inframe+y*w, w);

	for(x=0;x<w;x++)
		{
			*(frame+(x)+(y+1)*w) = (*(inframe+(x)+(y)*w)+*(inframe+(x)+(y+2)*w))/2;
		}
	}

}
