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
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int w, int h,
		    int field)
{
  int x, y, v;
  int dx, d, e, m, a, b, c, i;

  for(y=field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
	*(frame+x+(y/2)*w) = *(inframe+x+y*w);
	}

  if(field==0)
	{
  for(y=0;y<(h/2);y++)
	for(x=0;x<w;x++)
	{
	v  = *(frame+x+y*w-w*2) * 0.08;
	v += *(frame+x+y*w-w*1) * -.13;
	v += *(frame+x+y*w    ) * 0.30;
        v += *(frame+x+y*w+w*1) * 0.90;
        v += *(frame+x+y*w+w*2) * -.18;
        v += *(frame+x+y*w+w*3) * 0.10;
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
	v  = *(frame+x+y*w-w*2) * 0.10;
	v += *(frame+x+y*w-w*1) * -.18;
	v += *(frame+x+y*w    ) * 0.90;
        v += *(frame+x+y*w+w*1) * 0.30;
        v += *(frame+x+y*w+w*2) * -.13;
        v += *(frame+x+y*w+w*3) * 0.08;
	v = v<0? 0:v;
	v = v>255? 255:v;
	*(frame+x+(h/2+y)*w) = v;
	}
	}

#if 0
  for(y=field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
	*(lp0+x+y*w)=*(lp0+x+y*w+w)= (*(inframe+x+y*w)-*(inframe+x+y*w+w+w))/2+128;
	}
  for(y=field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
	*(lp1+x+y*w) =*(lp1+x+y*w+w) = (*(lp0+x+y*w-8)+
					*(lp0+x+y*w-7)+
					*(lp0+x+y*w-6)+
					*(lp0+x+y*w-5)+
					*(lp0+x+y*w-4)+
					*(lp0+x+y*w-3)+
					*(lp0+x+y*w-2)+
					*(lp0+x+y*w-1)+
					*(lp0+x+y*w  )+
					*(lp0+x+y*w+1)+
					*(lp0+x+y*w+2)+
					*(lp0+x+y*w+3)+
					*(lp0+x+y*w+4)+
					*(lp0+x+y*w+5)+
					*(lp0+x+y*w+6)+
					*(lp0+x+y*w+7)+
					*(lp0+x+y*w+8))/17;
	}
  for(y=field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
	*(lp2+x+y*w) =*(lp2+x+y*w+w) = (*(inframe+x+y*w-8)+
					*(inframe+x+y*w-7)+
					*(inframe+x+y*w-6)+
					*(inframe+x+y*w-5)+
					*(inframe+x+y*w-4)+
					*(inframe+x+y*w-3)+
					*(inframe+x+y*w-2)+
					*(inframe+x+y*w-1)+
					*(inframe+x+y*w  )+
					*(inframe+x+y*w+1)+
					*(inframe+x+y*w+2)+
					*(inframe+x+y*w+3)+
					*(inframe+x+y*w+4)+
					*(inframe+x+y*w+5)+
					*(inframe+x+y*w+6)+
					*(inframe+x+y*w+7)+
					*(inframe+x+y*w+8))/17;
	}

  for(y=field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
	i  = *(inframe+x+y*w-w) + *(inframe+x+y*w+w);
	i /= 2;
	m = 65535;
	for(dx=0;dx<8;dx++)
		{
		d = i-(*(inframe+x+dx+y*w-w) + *(inframe+x-dx+y*w+w))/2;
		e = d<0? -d:d;

		d = *(inframe+x+dx+y*w-w) - *(inframe+x-dx+y*w+w);
		e += d<0? -d:d;

		d = *(inframe+x+dx*0+y*w-w) - *(inframe+x-dx*2+y*w+w);
		e += d<0? -d:d;

		d = *(inframe+x+dx*2+y*w-w) - *(inframe+x-dx*0+y*w+w);
		e += d<0? -d:d;

		if(e<m)
			{
				m  = e/2;
				v =(*(inframe+x+dx+y*w-w) + *(inframe+x-dx+y*w+w))/2;
			}

		d = i-(*(inframe+x-dx+y*w-w) + *(inframe+x+dx+y*w+w))/2;
		e = d<0? -d:d;

		d = *(inframe+x-dx+y*w-w) - *(inframe+x+dx+y*w+w);
		e += d<0? -d:d;

		d = *(inframe+x-dx*0+y*w-w) - *(inframe+x+dx*2+y*w+w);
		e += d<0? -d:d;

		d = *(inframe+x-dx*2+y*w-w) - *(inframe+x+dx*0+y*w+w);
		e += d<0? -d:d;

		if(e<m)
			{
				m  = e/2;
				v =(*(inframe+x-dx+y*w-w) + *(inframe+x+dx+y*w+w))/2;
			}
		}

	*(frame+x+y*w-w) = *(inframe+x+y*w-w);
	*(frame+x+y*w) = v;
	}
#endif
}
