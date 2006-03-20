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

extern uint8_t * scratch;

int median ( int a, int b, int c)
{
	return 	((a<=b && b<=c)||(a>=b && b>=c))? b:
		((a<=c && c<=b)||(a>=c && c>=b))? c:a;
}

void motion_compensate (uint8_t * r, uint8_t * f , int w, int h, int field)
{
int x,y;
int dx,dy;
int vx,vy;
int oy,ox;
uint32_t sad;
uint32_t min;
int a,b,c,d,e;

for(y=0;y<h;y+=8)
	for(x=0;x<w;x+=8)
	{
	x -= 4;
	y -= 4;

	min=psad_00 ( f+x+y*w, r+x+y*w, w, 16, 0x00ffffff);
	vx=0;
	vy=0;

	for(dy=-8;dy<=8;dy+=8) 
	for(dx=-8;dx<=8;dx+=8)
		{
		sad = psad_00 ( f+x+y*w, r+(x+dx)+(y+dy)*w, w, 16, 0x00ffffff);

		if(sad<min)
			{
			vx=dx;
			vy=dy;
			min=sad;
			}
		}

	ox=vx;
	oy=vy;
	for(dy=(oy-4);dy<=(oy+4);dy+=4) 
	for(dx=(ox-4);dx<=(ox+4);dx+=4)
		{
		sad = psad_00 ( f+x+y*w, r+(x+dx)+(y+dy)*w, w, 16, 0x00ffffff);

		if(sad<min)
			{
			vx=dx;
			vy=dy;
			min=sad;
			}
		}

	ox=vx;
	oy=vy;
	for(dy=(oy-2);dy<=(oy+2);dy+=1) 
	for(dx=(ox-2);dx<=(ox+2);dx+=1)
		{
		sad = psad_00 ( f+x+y*w, r+(x+dx)+(y+dy)*w, w, 16, 0x00ffffff);

		if(sad<min)
			{
			vx=dx;
			vy=dy;
			min=sad;
			}
		}

	x += 4;
	y += 4;

	// set illegal MVs to zero...
	vx = ((vx+x+8)>w)? 0:vx;
	vx = ((vx+x  )<0)? 0:vx;
	vy = ((vy+y+8)>h)? 0:vy;
	vy = ((vy+y  )<0)? 0:vy;

	for(dy=(field+1);dy<=8;dy+=2)
	for(dx=0;dx<8;dx++)
		{
		*(scratch+(x+dx)+(y+dy)*w)=*(r+(x+dx+vx)+(y+dy+vy)*w);
		}
	}	

// copy field lines
for(y=field;y<h;y+=2)
	memcpy ( scratch+y*w, f+y*w, w);

// update outframe
memcpy (r,scratch,w*h);

// protect outframe against motion-mismatches and leak in some spatial interpolation to make
// motion-compensation a little more robust...
for(y=field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
		a = *(r+x+y*w); 		// original field-line;
		b = *(r+x+y*w+w); 		// motion-based interpolation;
		c = *(r+x+y*w+2*w); 		// original field-line;
		d = *(f+x+y*w+w); 		// edge-interpolated;

		b = (d+3*b)/4;

		*(r+x+y*w+w) = median (a,b,c);
	}

// blend fields to fullfill Kell-Factor and supress ALIAS...
// this is important! do *NOT* deactivate unless you want to see bad 
// flicker on TV and alias/line-stripes on PC. The algorithm heavyly
// relies on this filter.
//
// If you think this is wrong, please read how (good) interlaced cameras
// generate one field from the CCD-chip. There is no resolution above.
// And I don't like it either, but I repeat: there is no resolution above
// to be reconstructed. Only badly computer-generated interlaced frames
// do have frequencies above and this looks really, really bad on an
// interlaced screen, too...

#if 1
for(y=1;y<(h-1);y++)
	for(x=0;x<w;x++)
	{
		a  = *(r+x+y*w-w); 	
		a += *(r+x+y*w)*2; 	
		a += *(r+x+y*w+w); 	

		*(r+x+y*w) = a/4;
	}
#endif
}
