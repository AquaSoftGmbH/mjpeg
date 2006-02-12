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

int median3 ( int a, int b, int c )
{
	return ((a<=b && b<=c)? b:
	        (a<=c && c<=b)? c:a);
}

uint32_t median3_l ( uint32_t a, uint32_t b, uint32_t c )
{
	return ((a<=b && b<=c)? b:
	        (a<=c && c<=b)? c:a);
}

void
motion_compensate (uint8_t * r, uint8_t * f0, uint8_t * f1, uint8_t * f2,
		   int w, int h, int field)
{
  static int mvx[512][512];
  static int mvy[512][512];
  static uint32_t sum[512][512];

  int ix,iy;

  int x, y;
	int ox,oy;
  int dx, dy;
  int fx, fy;
  uint32_t sad;
  uint32_t fmin;
  int a, b, c, d, e, v;
  uint32_t thres1;

fx=fy=0;

for(y=0;y<h;y+=8)
	for(x=0;x<w;x+=8)
	{
	ix = x/8;
	iy = y/8;

	x -= 4;
	y -= 4;

	thres1 = median3_l ( sum[ix-1][iy-1],sum[ix][iy-1],sum[ix-1][iy] );
	thres1 += (2*16*16)*4; // offset threshold with very good match
	
	// check zero-vector first
	dx = 0;
	dy = 0;
	fx=0;
	fy=0;
	fmin = psad_00
	  (f1 + (x) + (y) * w,
	   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
	fmin += psad_00
	  (f1 + (x) + (y) * w,
	   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
	if(fmin>thres1 && ix>0 && iy>0)
	{
	// check median-vector
	dx = median3( mvx[ix-1][iy-1],mvx[ix-1][iy],mvx[ix][iy-1] );
	dy = median3( mvy[ix-1][iy-1],mvy[ix-1][iy],mvy[ix][iy-1] );
	sad = psad_00
	  (f1 + (x) + (y) * w,
	   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
	sad += psad_00
	  (f1 + (x) + (y) * w,
	   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
	if (sad < fmin)
	  {
	    fmin = sad;
	    fx = dx;
	    fy = dy;
	  }

	if(fmin>thres1 && ix>0 && iy>0)
		{
		// check left-vector
		dx = mvx[ix-1][iy];
		dy = mvy[ix-1][iy];
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
		
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
		
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }

		if(fmin>thres1 && ix>0 && iy>0)
			{
			// check top-vector
			dx = mvx[ix][iy-1];
			dy = mvy[ix][iy-1];
			sad = psad_00
			  (f1 + (x) + (y) * w,
			   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
			
			sad += psad_00
			  (f1 + (x) + (y) * w,
			   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
			
			if (sad < fmin)
			  {
			    fmin = sad;
			    fx = dx;
			    fy = dy;
			  }

			if(fmin>thres1 && ix>0 && iy>0)
				{
				// check top-left--vector
				dx = mvx[ix-1][iy-1];
				dy = mvy[ix-1][iy-1];
				sad = psad_00
				  (f1 + (x) + (y) * w,
				   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
				
				sad += psad_00
				  (f1 + (x) + (y) * w,
				   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
				
				if (sad < fmin)
				  {
				    fmin = sad;
				    fx = dx;
				    fy = dy;
				  }
				}
			}
		}
	}


	if(fmin>thres1)
	{
	oy=fy;
	ox=fx;
	// as the center allready was checked, check only -16 and +16 offsets...
	  for (dy = (oy-16); dy <= (oy+16); dy+=32)
	    for (dx = (ox-16); dx <= (ox+16); dx+=32)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
	oy=fy;
	ox=fx;
	  for (dy = (oy-8); dy <= (oy+8); dy+=16)
	    for (dx = (ox-8); dx <= (ox+8); dx+=16)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
	oy=fy;
	ox=fx;
	  for (dy = (oy-4); dy <= (oy+4); dy+=8)
	    for (dx = (ox-4); dx <= (ox+4); dx+=8)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
	oy=fy;
	ox=fx;
	  for (dy = (oy-2); dy <= (oy+2); dy+=4)
	    for (dx = (ox-2); dx <= (ox+2); dx+=4)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
	oy=fy;
	ox=fx;
	  for (dy = (oy-1); dy <= (oy+1); dy+=2)
	    for (dx = (ox-1); dx <= (ox+1); dx+=2)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
//		fprintf(stderr,"f");
	}
	else
	{
	oy=fy;
	ox=fx;
	  for (dy = (oy-2); dy <= (oy+2); dy+=2)
	    for (dx = (ox-2); dx <= (ox+2); dx+=2)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
	oy=fy;
	ox=fx;
	  for (dy = (oy-1); dy <= (oy+1); dy+=2)
	    for (dx = (ox-1); dx <= (ox+1); dx+=2)
	      {
		sad = psad_00
		  (f1 + (x) + (y) * w,
		   f0 + (x + dx) + (y + dy) * w, w, 16, 0x00ffffff);
	
		sad += psad_00
		  (f1 + (x) + (y) * w,
		   f2 + (x - dx) + (y - dy) * w, w, 16, 0x00ffffff);
	
		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }
	      }
//		fprintf(stderr,"e");
	}

	x += 4;
	y += 4;

	//quantize y-displacement to field-locations
	fy /= 2;
	fy *= 2;

	sum[ix][iy]=fmin;
	mvx[ix][iy]=fx;
	mvy[ix][iy]=fy;
	
	// process the matched block
	  for (dy = 0; dy < 8; dy++)
	    for (dx = 0; dx < 8; dx++)
	      {
		a = *(f0+(x+dx+fx)+(y+dy+fy)*w);
		b = *(f1+(x+dx   )+(y+dy   )*w);
		c = *(f2+(x+dx-fx)+(y+dy-fy)*w);

		d = (a+b+c)/3;
		e = (d-b)<0? b-d:d-b;
		e = (48-e)<0 ? 0:48-e; // this makes an error of approx. +/- 24 visable...

		v = e*d + (48-e)*b;
		v /= 48;

		*(r+(x+dx)+(y+dy)*w) = v;
	      }
    }
}
