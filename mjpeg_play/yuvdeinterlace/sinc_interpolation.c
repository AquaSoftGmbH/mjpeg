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

extern int width;
extern int height;

extern uint8_t *frame3[3];

void
sinc_interpolation (uint8_t * frame, uint8_t * inframe, int field)
{
  int x, y, v;
  int a,b,c,d,e,f,g;

  memcpy (frame,inframe,width*height);

  // fill top out-off-range lines to avoid ringing
  memcpy (frame-width*1,inframe,width);
  memcpy (frame-width*2,inframe,width);
  memcpy (frame-width*3,inframe,width);
  memcpy (frame-width*4,inframe,width);
  memcpy (frame-width*5,inframe,width);

  // fill bottom out-off-range lines to avoid ringing
  memcpy (frame+width*(height+0),inframe+width*(height-2),width);
  memcpy (frame+width*(height+1),inframe+width*(height-2),width);
  memcpy (frame+width*(height+2),inframe+width*(height-2),width);
  memcpy (frame+width*(height+3),inframe+width*(height-2),width);
  memcpy (frame+width*(height+4),inframe+width*(height-2),width);

  /* interpolate missing lines */ 
  for (y = field; y < height ; y += 2)
    {
      for (x = 0; x < width; x++)
	{
		v = ( 
			*(frame+x+(y-5)*width)*+1 +
			*(frame+x+(y-3)*width)*-4 +
			*(frame+x+(y-1)*width)*16 + 
			*(frame+x+(y+1)*width)*16 +
			*(frame+x+(y+3)*width)*-4 +
			*(frame+x+(y+5)*width)*+1   )/26;

		v= v>255? 255:v;
		v= v<0? 0:v;

		*(frame+x+y*width)=v;
	}
    }
}

uint32_t SQD8 ( uint8_t * p0, uint8_t * p1 )
{
uint32_t sumsq=0;
uint32_t delta=0;
int r;

int length=1;

p0 -= length;
p1 -= length;

for(r=-length;r<=+length;r++)
  {
	delta = *(p0)-*(p1);
	sumsq += delta*delta;
	p0++;
	p1++;
  }

return (sumsq/(2*length+1));
}

void
edge_interpolation ( uint8_t * frame, uint8_t * inframe, int field)
{
  int x, y;
  int vx,vx1,vx0, dx,v,d;
  int a,b,c,e,m;
  uint32_t f;
  uint32_t min,min0,max;
  int mean;
  int diff;

  memcpy (frame,inframe,width*height);

  for (y = field; y < height ; y += 2)
    {
      for (x = 0; x < width; x++)
	{
        min=65536;
	vx=0;

	m  = *(inframe+(x-1)+(y-1)*width);
	m += *(inframe+(x  )+(y-1)*width)*4;
	m += *(inframe+(x+1)+(y-1)*width);

	m += *(inframe+(x-1)+(y+1)*width);
	m += *(inframe+(x  )+(y+1)*width)*4;
	m += *(inframe+(x+1)+(y+1)*width);

	m /= 12;

	m = ( 
		*(inframe+x+(y-5)*width) * +1 +
		*(inframe+x+(y-3)*width) * -6 +
		*(inframe+x+(y-1)*width) * 36 + 
		*(inframe+x+(y+1)*width) * 36 +
		*(inframe+x+(y+3)*width) * -6 +
		*(inframe+x+(y+5)*width) * +1   )/62;

	m= m<0? 0:m;
	m= m>255? 255:m;

	a = *(inframe+(x)+(y-1)*width);
	c = *(inframe+(x)+(y+1)*width);

	//if(abs(a-c)>16)
	for(dx=0;dx<=12;dx++)
        { 
	d  = *(inframe+(x-dx-1)+(y-1)*width)-*(inframe+(x+dx-1)+(y+1)*width);
	e = d*d;
//	d  = *(inframe+(x-dx  )+(y-1)*width)-*(inframe+(x+dx  )+(y+1)*width);
//	e += d*d;
//	d  = *(inframe+(x-dx+1)+(y-1)*width)-*(inframe+(x+dx+1)+(y+1)*width);
//	e += d*d;
//	e /= 3;

	d = m-(*(inframe+(x-dx)+(y-1)*width)+*(inframe+(x+dx)+(y+1)*width))/2;
	e += d*d;

	if(e<min)
		{
		vx=dx;
		min=e;
		}

	d  = *(inframe+(x+dx-1)+(y-1)*width)-*(inframe+(x-dx-1)+(y+1)*width);
	e = d*d;
//	d  = *(inframe+(x+dx  )+(y-1)*width)-*(inframe+(x-dx  )+(y+1)*width);
//	e += d*d;
//	d  = *(inframe+(x+dx+1)+(y-1)*width)-*(inframe+(x-dx+1)+(y+1)*width);
//	e += d*d;
//	e /= 3;

	d = m-(*(inframe+(x+dx)+(y-1)*width)+*(inframe+(x-dx)+(y+1)*width))/2;
	e += d*d;

	if(e<min)
		{
		vx=-dx;
		min=e;
		}

	}

	b = ( 
		*(inframe+x+(y-5)*width-vx*5) * +1 +
		*(inframe+x+(y-3)*width-vx*3) * -6 +
		*(inframe+x+(y-1)*width-vx  ) * 36 + 
		*(inframe+x+(y+1)*width+vx  ) * 36 +
		*(inframe+x+(y+3)*width+vx*3) * -6 +
		*(inframe+x+(y+5)*width+vx*5) * +1   )/62;

	b= b<0? 0:b;
	b= b>255? 255:b;

	*(frame+x+y*width) = (2*b+m)/3;
	}
    }
}
