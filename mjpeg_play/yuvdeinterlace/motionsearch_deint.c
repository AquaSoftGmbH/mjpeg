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
#include "config.h"
#include "mjpeg_types.h"
#include "motionsearch_deint.h"
#include "mjpeg_logging.h"
#include "motionsearch.h"
#include "transform_block.h"
#include "vector.h"

extern int width ;
extern int height ;
extern uint8_t *inframe[3];
extern uint8_t *outframe[3];
extern uint8_t *frame1[3];
extern uint8_t *frame2[3];
extern uint8_t *frame3[3];
extern uint8_t *frame4[3];
extern uint8_t *frame5[3];
extern uint8_t *frame6[3];
extern uint8_t *reconstructed[3];

uint32_t mean_SAD = 0;

struct
{
  int x;
  int y;
}
pattern[4096];
int pattern_length;

void
init_search_pattern (void)
{
  int x, y, r, cnt, i;
  int allready_in_path;

  cnt = 0;
  for (r = 0; r <= (4*4); r++)
    {
      for (y = -32; y <= 32; y +=2)
	for (x = -32; x <= 32; x++)
	  {
	    if (r >= (x * x + y * y))
	      {
		/* possible candidate */
		/* check if it's allready in the path */
		allready_in_path = 0;
		for (i = 0; i <= cnt; i++)
		  {
		    if (pattern[i].x == x && pattern[i].y == y)
		      {
			allready_in_path = 1;
		      }
		  }
		/* if it's not allready in the path, store it */
		if (allready_in_path == 0)
		  {
		    pattern[cnt].x = x;
		    pattern[cnt].y = y;
		    cnt++;
		  }
	      }
	  }
    }
  pattern_length = cnt + 1;
  mjpeg_log (LOG_INFO, "Search-Path-Lenght = %i", cnt + 1);
}

struct vector
search_forward_vector (int x, int y)
{
  uint32_t min, SAD;
  int dx, dy;
  int i;
  struct vector v;

  x -= 3;
  y -= 3;

  min  = psad_sub22
    (frame3[0] + (x) + (y) * width,
     frame4[0] + (x) + (y) * width, width, 8);
//  min += psad_00
//    (frame2[0] + (x) + (y) * width,
//     frame1[0] + (x) + (y) * width, width, 16, 0x00ffffff);
  v.x = v.y = 0;

if(min>512)
for(dy=-16;dy<=16;dy+=2)
for(dx=-16;dx<=16;dx+=1)
    //for (i = 1; i < pattern_length; i++)
      {
	//dx = pattern[i].x;
	//dy = pattern[i].y;

	SAD  = psad_sub22
	  (frame3[0] + (x) + (y) * width,
	   frame4[0] + (x - dx) + (y - dy) * width, width, 8);
//	SAD += psad_00
//	  (frame2[0] + (x) + (y) * width,
//	   frame1[0] + (x + dx) + (y + dy) * width, width, 16, 0x00ffffff);

	if (SAD < min)
	  {
	    min = SAD;
	    v.x = dx;
	    v.y = dy;
	  }
      }

  v.min = min;
  return v;			/* return the found vector */
}

struct vector
search_backward_vector (int x, int y)
{
  uint32_t min, SAD;
  int dx, dy;
  int i;
  struct vector v;

  x -= 3;
  y -= 3;

  min  = psad_sub22
    (frame4[0] + (x) + (y) * width,
     frame3[0] + (x) + (y) * width, width, 8 );
//  min += psad_00
//    (frame2[0] + (x) + (y) * width,
//     frame1[0] + (x) + (y) * width, width, 16, 0x00ffffff);
  v.x = v.y = 0;
  
if(min>512)
for(dy=-16;dy<=16;dy+=2)
for(dx=-16;dx<=16;dx+=1)
    //for (i = 1; i < pattern_length; i++)
      {
	//dx = pattern[i].x;
	//dy = pattern[i].y;

	SAD  = psad_sub22
	  (frame4[0] + (x) + (y) * width,
	   frame3[0] + (x - dx) + (y - dy) * width, width, 8 );
//	SAD += psad_00
//	  (frame2[0] + (x) + (y) * width,
//	   frame1[0] + (x + dx) + (y + dy) * width, width, 16, 0x00ffffff);

	if (SAD < min)
	  {
	    min = SAD;
	    v.x = dx;
	    v.y = dy;
	  }
      }

  v.min = min;
  return v;			/* return the found vector */
}

void
motion_compensate_fields (void)
{
  int x, y;
  struct vector fv;
  struct vector bv;
  float match_coeff21=0;
  float match_coeff23=0;
  uint32_t SAD_accu=0;
  uint32_t SAD_cnt=0;

  for (y = 0; y < height; y += 1)
    {
      for (x = 0; x < width; x += 1)
	{
	  fv = search_forward_vector (x, y);
	  bv = search_backward_vector (x, y);

	  if( (y+bv.y)<0 ) bv.y=0;
	  if( (y-fv.y)<0 ) fv.y=0;

	  if( (y-bv.y)>height ) bv.y=0;
	  if( (y+fv.y)>height ) fv.y=0;

	  transform_block ( frame5[0]+(x)+(y)*width, frame4[0]+(x-fv.x)+(y-fv.y)*width, width );
	  transform_block ( frame6[0]+(x)+(y)*width, frame3[0]+(x-bv.x)+(y-bv.y)*width, width );
	}
    }
}

void
motion_compensate_field1 (void)
{
  int x, y;
  struct vector fv;
  struct vector bv;
  float match_coeff21=0;
  float match_coeff23=0;
  uint32_t SAD_accu=0;
  uint32_t SAD_cnt=0;

  for (y = 0; y < height; y += 8)
    {
      for (x = 0; x < width; x += 8)
	{
	  fv = search_backward_vector (x, y);

	  transform_block ( frame4[0]+(x)+(y)*width, frame1[0]+(x-fv.x)+(y-fv.y)*width, width );
	}
    }
}

void
motion_compensate ( uint8_t * r, uint8_t * f0, uint8_t * f1, uint8_t * f2, int field )
{
  int x, y;
  int dx,dy;
  int fx,fy;
  int bx,by;
  uint32_t sad;
  uint32_t fmin,bmin;
int a,b,c,d,e,m;
int min;

  // fill top out-off-range lines to avoid ringing
for(y=1;y<10;y++)
{
  memcpy (f0-width*y,f0,width);
  memcpy (f1-width*y,f0,width);
  memcpy (f2-width*y,f0,width);
}
  // fill bottom out-off-range lines to avoid ringing
for(y=0;y<10;y++)
{
  memcpy (f0+width*(height+y),f0+width*(height-1),width);
  memcpy (f1+width*(height+y),f0+width*(height-1),width);
  memcpy (f2+width*(height+y),f0+width*(height-1),width);
}

  for (y = 0; y < height; y += 8)
    {
      for (x = 0; x < width; x += 8)
	{
	    fmin = 0.8 * psad_00
			(f1 + (x   -6) + (y   -6) * width,
     			 f0 + (x   -6) + (y   -6) * width, width, 16, 0x0000ffff );
	    fx = 0 ;
	    fy = 0 ;
	    bmin = 0.8 * psad_00
			(f1 + (x   -6) + (y   -6) * width,
     			 f2 + (x   -6) + (y   -6) * width, width, 16, 0x0000ffff );
	    bx = 0 ;
	    by = 0 ;

	    if(bmin>2048 || fmin>2048)
	    for(dy = -8; dy <= +8; dy+=2)
		for(dx = -8; dx <= +8; dx++)
		{
		sad  = psad_00
			(f1 + (x   -6) + (y   -6) * width,
     			 f0 + (x+dx-6) + (y+dy-6) * width, width, 16, 0x0000ffff );

		if(sad<fmin)
		{
		  fmin=sad;
		  fx=dx;
		  fy=dy;
	        }

		sad = psad_00
			(f1 + (x   -6) + (y   -6) * width,
     			 f2 + (x+dx-6) + (y+dy-6) * width, width, 16, 0x0000ffff );
		if(sad<bmin)
		{
		  bmin=sad;
		  bx=dx;
		  by=dy;
	        }

		}
		else
	    for(dy = -2; dy <= +2; dy+=2)
		for(dx = -2; dx <= +2; dx++)
		{
		sad  = psad_00
			(f1 + (x   -6) + (y   -6) * width,
     			 f0 + (x+dx-6) + (y+dy-6) * width, width, 16, 0x0000ffff );

		if(sad<fmin)
		{
		  fmin=sad;
		  fx=dx;
		  fy=dy;
	        }

		sad = psad_00
			(f1 + (x   -6) + (y   -6) * width,
     			 f2 + (x+dx-6) + (y+dy-6) * width, width, 16, 0x0000ffff );
		if(sad<bmin)
		{
		  bmin=sad;
		  bx=dx;
		  by=dy;
	        }

		}


	if( (x+fx)<0 ) fx=0;
	if( (y+fy)<0 ) fy=0;
	if( (x+bx)<0 ) bx=0;
	if( (y+by)<0 ) by=0;

	if( (x+fx)>width ) fx=0;
	if( (y+fy)>height ) fy=0;
	if( (x+bx)>width ) bx=0;
	if( (y+by)>height ) by=0;

           for(dy=0;dy<8;dy++)
              for(dx=0;dx<8;dx++)
		{
	 	    // lowpass-filter current field;
		    b  = *(f1+(x+dx)+(y+dy-9)*width)*  1;
		    b += *(f1+(x+dx)+(y+dy-7)*width)* -4;
		    b += *(f1+(x+dx)+(y+dy-5)*width)* 16;
		    b += *(f1+(x+dx)+(y+dy-3)*width)*-64;
		    b += *(f1+(x+dx)+(y+dy-1)*width)*256;
		    b += *(f1+(x+dx)+(y+dy+1)*width)*256;
		    b += *(f1+(x+dx)+(y+dy+3)*width)*-64;
		    b += *(f1+(x+dx)+(y+dy+5)*width)* 16;
		    b += *(f1+(x+dx)+(y+dy+7)*width)* -4;
		    b += *(f1+(x+dx)+(y+dy+9)*width)*  1;
        	    b /= 410;

	 	    // highpass-filter previous field;
		    a  = *(f0+(x+dx+fx)+(y+dy+fy-10)*width)*  1;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 8)*width)* -4;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 6)*width)* 16;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 4)*width)*-64;
		    a += *(f0+(x+dx+fx)+(y+dy+fy- 2)*width)*256;
		    a -= *(f0+(x+dx+fx)+(y+dy+fy   )*width)*410;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 2)*width)*256;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 4)*width)*-64;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 6)*width)* 16;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+ 8)*width)* -4;
		    a += *(f0+(x+dx+fx)+(y+dy+fy+10)*width)*  1;
        	    a /= 410;

	 	    // highpass-filter next field;
		    c  = *(f2+(x+dx+bx)+(y+dy+by-10)*width)*  1;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 8)*width)* -4;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 6)*width)* 16;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 4)*width)*-64;
		    c += *(f2+(x+dx+bx)+(y+dy+by- 2)*width)*256;
		    c -= *(f2+(x+dx+bx)+(y+dy+by   )*width)*410;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 2)*width)*256;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 4)*width)*-64;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 6)*width)* 16;
		    c += *(f2+(x+dx+bx)+(y+dy+by+ 8)*width)* -4;
		    c += *(f2+(x+dx+bx)+(y+dy+by+10)*width)*  1;
        	    c /= 410;

		    c = b-(a+c)/8;

	    c = c>255? 255:c;
	    c = c<0  ?   0:c;

	    if(((dy+y)&1)== field)
               *(r+(x+dx)+(y+dy)*width) = c;
	    else
               *(r+(x+dx)+(y+dy)*width) = *(f1+(x+dx)+(y+dy)*width);

	        }
	}
    }

}

void smooth_isophotes ( uint8_t * r, int field )
{
	int x,y;
	int min;
	int m,e,d,dx,fx;
	int a;

    // smooth isophotes out ... "antialias"
    for(y=field;y<height;y+=2)
	for(x=0;x<width;x++)
	{
		m = *(r+(x  )+(y  )*width);

		min  = 0x00ffff;
		fx = 0;

		for(dx=0;dx<16;dx++)
		{
		e = *(r+(x-dx  )+y*width-width) - *(r+(x+dx  )+y*width+width) ;
		d = e*e;
		e = m-(*(r+(x-dx)+y*width-width) + *(r+(x+dx)+y*width+width))/2;
		d += e*e;

		if(d<min)
		{
			min = d;
			fx = dx;
		}

		e = *(r+(x+dx  )+y*width-width) - *(r+(x-dx  )+y*width+width) ;
		d = e*e;
		e = m-(*(r+(x+dx)+y*width-width) + *(r+(x-dx)+y*width+width))/2;
		d += e*e;

		if(d<min)
		{
			min = d;
			fx = -dx;
		}

		}

		*(r+x+y*width) = ( *(r+x+y*width)+*(r+(x-fx)+y*width-width)+*(r+(x+fx)+y*width+width) )/3;
	}
}
