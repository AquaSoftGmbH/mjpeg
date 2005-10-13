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
#include <stdio.h>
#include "config.h"
#include "mjpeg_types.h"
#include "motionsearch_deint.h"
#include "mjpeg_logging.h"
#include "motionsearch.h"
#include "transform_block.h"
#include "vector.h"

extern uint8_t * lp0;
extern uint8_t * lp1;
extern uint8_t * lp2;

void
lowpass_plane_2D ( uint8_t * d, uint8_t * s, int w, int h )
{
int x,y,v;

	for(y=0;y<h;y++)
	for(x=0;x<w;x++)
	{
	v =
		(
		*(s+x+(y-11)*w)*  21+
		*(s+x+(y-10)*w)*  33+
		*(s+x+(y- 9)*w)*  26+
		*(s+x+(y- 7)*w)* -33+
		*(s+x+(y- 6)*w)* -54+
		*(s+x+(y- 5)*w)* -46+
		*(s+x+(y- 3)*w)*  77+
		*(s+x+(y- 2)*w)* 163+
		*(s+x+(y- 1)*w)* 230+
		*(s+x+(y   )*w)* 256+
		*(s+x+(y+ 1)*w)* 230+
		*(s+x+(y+ 2)*w)* 163+
		*(s+x+(y+ 3)*w)*  77+
		*(s+x+(y+ 5)*w)* -46+
		*(s+x+(y+ 6)*w)* -54+
		*(s+x+(y+ 7)*w)* -33+
		*(s+x+(y+ 9)*w)*  26+
		*(s+x+(y+10)*w)*  33+
		*(s+x+(y+11)*w)*  21
		)/1090;
	v = v<0? 0:v;
	v = v>255? 255:v;

	*(d+x+y*w) = v;

	}

	for(y=0;y<h;y++)
	for(x=0;x<w;x++)
	{
	v =
		(
		*(d+(x-11)+y*w)*  21+
		*(d+(x-10)+y*w)*  33+
		*(d+(x- 9)+y*w)*  26+
		*(d+(x- 7)+y*w)* -33+
		*(d+(x- 6)+y*w)* -54+
		*(d+(x- 5)+y*w)* -46+
		*(d+(x- 3)+y*w)*  77+
		*(d+(x- 2)+y*w)* 163+
		*(d+(x- 1)+y*w)* 230+
		*(d+(x   )+y*w)* 256+
		*(d+(x+ 1)+y*w)* 230+
		*(d+(x+ 2)+y*w)* 163+
		*(d+(x+ 3)+y*w)*  77+
		*(d+(x+ 5)+y*w)* -46+
		*(d+(x+ 6)+y*w)* -54+
		*(d+(x+ 7)+y*w)* -33+
		*(d+(x+ 9)+y*w)*  26+
		*(d+(x+10)+y*w)*  33+
		*(d+(x+11)+y*w)*  21
		)/1090;
	v = v<0? 0:v;
	v = v>255? 255:v;

	*(d+x+y*w) = v;
	}
}

void
lowpass_plane_1D ( uint8_t * d, uint8_t * s, int w, int h )
{
int x,y,v;

	for(y=0;y<h;y++)
	for(x=0;x<w;x++)
	{
	v =
		(
		*(s+x+(y-11)*w)*  21+
		*(s+x+(y-10)*w)*  33+
		*(s+x+(y- 9)*w)*  26+
		*(s+x+(y- 7)*w)* -33+
		*(s+x+(y- 6)*w)* -54+
		*(s+x+(y- 5)*w)* -46+
		*(s+x+(y- 3)*w)*  77+
		*(s+x+(y- 2)*w)* 163+
		*(s+x+(y- 1)*w)* 230+
		*(s+x+(y   )*w)* 256+
		*(s+x+(y+ 1)*w)* 230+
		*(s+x+(y+ 2)*w)* 163+
		*(s+x+(y+ 3)*w)*  77+
		*(s+x+(y+ 5)*w)* -46+
		*(s+x+(y+ 6)*w)* -54+
		*(s+x+(y+ 7)*w)* -33+
		*(s+x+(y+ 9)*w)*  26+
		*(s+x+(y+10)*w)*  33+
		*(s+x+(y+11)*w)*  21
		)/1090;
	v = v<0? 0:v;
	v = v>255? 255:v;

	*(d+x+y*w) = v;
	}
}

void
motion_compensate ( uint8_t * r, uint8_t * f0, uint8_t * f1, uint8_t * f2, int w, int h, int field )
{
  int x, y;
  int dx,dy;
  int fx,fy;
  int bx,by;
  uint32_t sad;
  uint32_t fmin,bmin;
int a,c,d;

  // fill top out-off-range lines to avoid ringing
for(y=1;y<10;y++)
{
  memcpy (f0-w*y,f0,w);
  memcpy (f1-w*y,f0,w);
  memcpy (f2-w*y,f0,w);
}
  // fill bottom out-off-range lines to avoid ringing
for(y=0;y<10;y++)
{
  memcpy (f0+w*(h+y),f0+w*(h-1),w);
  memcpy (f1+w*(h+y),f0+w*(h-1),w);
  memcpy (f2+w*(h+y),f0+w*(h-1),w);
}

lowpass_plane_2D ( lp0, f0, w, h );
lowpass_plane_2D ( lp1, f1, w, h );
lowpass_plane_2D ( lp2, f2, w, h );

  for (y = 0; y < h; y += 32)
    {
      for (x = 0; x < w; x += 32)
	{
	    fx = bx = 0 ;
	    fy = by = 0 ;
		fmin=bmin=0x00ffffff;
	    for(dy = -8; dy <= +8; dy+=2)
		for(dx = -8; dx <= +8; dx++)
		{
		sad  = psad_00
			(lp1 + (x   ) + (y   ) * w,
     			 lp0 + (x+dx)  + (y+dy) * w, w, 32, 0x00ffffff);
		sad += psad_00
			(lp1 + (x   +16) + (y   ) * w,
     			 lp0 + (x+dx+16)  + (y+dy) * w, w, 32, 0x00ffffff);

		if(sad<fmin)
		{
		  fmin=sad;
		  fx=dx;
		  fy=dy;
	        }

		sad = psad_00
			(lp1 + (x   ) + (y   ) * w,
     			 lp2 + (x+dx) + (y+dy) * w, w, 32, 0x00ffffff);
		sad += psad_00
			(lp1 + (x   +16) + (y   ) * w,
     			 lp2 + (x+dx+16) + (y+dy) * w, w, 32, 0x00ffffff);

		if(sad<bmin)
		{
		  bmin=sad;
		  bx=dx;
		  by=dy;
	        }

		}
           for(dy=0;dy<32;dy++)
              for(dx=0;dx<32;dx++)
		{

		    c = ( *(f0+(x+dx+fx)+(y+dy+fy   )*w)+
			  *(f2+(x+dx+bx)+(y+dy+by   )*w) )/2;


	    if(((dy+y)&1)== field)
               *(r+(x+dx)+(y+dy)*w) = c;
	    else
               *(r+(x+dx)+(y+dy)*w) = *(f1+(x+dx)+(y+dy)*w);

	        }
	}
    }


// depending on the lowpass-filtered error, 
// blend between spatial-only and temporal-motion-compensated interpolation...
lowpass_plane_1D (lp0,r,w,h);
lowpass_plane_1D (lp1,f1,w,h);
  for (y = field; y < h; y+=2) 
    {
      for (x = 0; x < w; x++)
	{
	d  = *(lp1+x+y*w)-*(lp0+x+y*w);
	d = d<0? -d:d;

	d  = d>32 ? 0:32-d;

	a  = *(r+x+y*w) * d;
	a += *(f1+x+y*w) * (32-d);

	*(r+x+y*w)=a/32;
	}
    }
#if 1
// finally do a linear blend to get rid of artefacts
  for (y = 1; y < (h-1); y++) 
    {
      for (x = 0; x < w; x++)
	{
	*(r+x+y*w)=
		(
		*(r+x+y*w-w)*1+
		*(r+x+y*w  )*2+
		*(r+x+y*w+w)*1
		)/4;
	}
    }
#endif
}

