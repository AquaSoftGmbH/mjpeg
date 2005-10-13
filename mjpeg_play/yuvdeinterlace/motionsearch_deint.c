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
#include <stdlib.h>
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
lowpass_plane ( uint8_t * d, uint8_t * s, int w, int h )
{
int x,y,v;

	for(y=0;y<h;y++)
	for(x=0;x<w;x++)
	{
	v =
		(
		*(s+x+(y-9)*w)*  0 +
		*(s+x+(y-7)*w)*-13 +
		*(s+x+(y-6)*w)*-21 +
		*(s+x+(y-5)*w)*-18 +
		*(s+x+(y-4)*w)*  0 +
		*(s+x+(y-3)*w)* 30 +
		*(s+x+(y-2)*w)* 64 +
		*(s+x+(y-1)*w)* 90 +
		*(s+x+(y  )*w)*100 +
		*(s+x+(y+1)*w)* 90 +
		*(s+x+(y+2)*w)* 64 +
		*(s+x+(y+3)*w)* 30 +
		*(s+x+(y+4)*w)*  0 +
		*(s+x+(y+5)*w)*-18 +
		*(s+x+(y+6)*w)*-21 +
		*(s+x+(y+7)*w)*-13 +
		*(s+x+(y+9)*w)*  0 
		)/384;
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

lowpass_plane ( lp0, f0, w, h );
lowpass_plane ( lp1, f1, w, h );
lowpass_plane ( lp2, f2, w, h );

  for (y = 0; y < h; y += 8)
    {
      for (x = 0; x < w; x += 8)
	{
	    fmin = psad_00
			(lp1 + (x) + (y) * w,
     			 lp0 + (x) + (y) * w, w, 16, 0x00ffffff );
	    bmin = psad_00
			(lp1 + (x) + (y) * w,
     			 lp2 + (x) + (y) * w, w, 16, 0x00ffffff );
	    fx = bx = 0 ;
	    fy = by = 0 ;

	    for(dy = -8; dy <= +8; dy+=2)
		for(dx = -8; dx <= +8; dx++)
		{
		sad  = psad_00
			(lp1 + (x   ) + (y   ) * w,
     			 lp0 + (x+dx) + (y+dy) * w, w, 16, 0x00ffffff );

		if(sad<fmin)
		{
		  fmin=sad;
		  fx=dx;
		  fy=dy;
	        }

		sad = psad_00
			(lp1 + (x   ) + (y   ) * w,
     			 lp2 + (x+dx) + (y+dy) * w, w, 16, 0x00ffffff );

		if(sad<bmin)
		{
		  bmin=sad;
		  bx=dx;
		  by=dy;
	        }

		}

           for(dy=0;dy<8;dy++)
              for(dx=0;dx<8;dx++)
		{
#if 0
	 	    // lowpass-filter current field;
		    b  = *(f1+(x+dx)+(y+dy-9)*w)*  1;
		    b += *(f1+(x+dx)+(y+dy-7)*w)* -4;
		    b += *(f1+(x+dx)+(y+dy-5)*w)* 16;
		    b += *(f1+(x+dx)+(y+dy-3)*w)*-64;
		    b += *(f1+(x+dx)+(y+dy-1)*w)*256;
		    b += *(f1+(x+dx)+(y+dy+1)*w)*256;
		    b += *(f1+(x+dx)+(y+dy+3)*w)*-64;
		    b += *(f1+(x+dx)+(y+dy+5)*w)* 16;
		    b += *(f1+(x+dx)+(y+dy+7)*w)* -4;
		    b += *(f1+(x+dx)+(y+dy+9)*w)*  1;
        	    b /= 410;

	 	    // highpass-filter previous field;
		    a  = *(f0+(x+dx+fx)+(y+dy+fy-10)*w)*  1;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 8)*w)* -4;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 6)*w)* 16;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 4)*w)*-64;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 2)*w)*256;
		    a -= *(f0+(x+dx+fx)+(y+dy+fy   )*w)*410;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 2)*w)*256;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 4)*w)*-64;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 6)*w)* 16;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 8)*w)* -4;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+10)*w)*  1;
        	    a /= 410;

	 	    // highpass-filter next field;
		    c  = *(f2+(x+dx+bx)+(y+dy+by-10)*w)*  1;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 8)*w)* -4;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 6)*w)* 16;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 4)*w)*-64;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 2)*w)*256;
		    c -= *(f2+(x+dx+bx)+(y+dy+by   )*w)*410;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 2)*w)*256;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 4)*w)*-64;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 6)*w)* 16;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 8)*w)* -4;
		    c += *(f2+(x+dx+bx)+(y+dy+by+10)*w)*  1;
        	    c /= 410;

		    d = b-(a+c)/8;
#endif

		    c = ( *(f0+(x+dx+fx)+(y+dy+fy   )*w)+
			  *(f2+(x+dx+bx)+(y+dy+by   )*w) )/2;


	    if(((dy+y)&1)== field)
               *(r+(x+dx)+(y+dy)*w) = c;
	    else
               *(r+(x+dx)+(y+dy)*w) = *(f1+(x+dx)+(y+dy)*w);

	        }
	}
    }

lowpass_plane (lp0,r,w,h);

// depending on the lowpass-filtered error, 
// blend between spatial-only and temporal-motion-compensated interpolation...
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	d  = abs(*(lp1+x+y*w)-*(lp0+x+y*w));
	d  = d>8 ? 0:8-d;

	a  = *(r+x+y*w) * d;
	a += *(f1+x+y*w) * (8-d);

	*(r+x+y*w)=a/8;*(f1+x+y*w);
	}
    }
}

