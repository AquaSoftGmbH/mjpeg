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
 * FILE: blend_fields.c                                    *
 *                                                         *
 ***********************************************************/

#include "config.h"
#include "mjpeg_types.h"
#include "blend_fields.h"
#include <stdio.h>
#include <math.h>

extern int width;
extern int height;

void
blend_fields_non_accel (uint8_t * dst[3], uint8_t * src[3] )
{
	int x,y;
	int w = width;

	for(y=0;y<height;y+=2)
		for(x=0;x<w;x++)
		{
//			*(dst[0]+x+y*w) = (*(dst[0]+x+y*w)+*(src[0]+x+y*w))/2;
			*(dst[0]+x+y*w) = *(src[0]+x+y*w);
//			*(dst[0]+x+y*w+w) = *(src[0]+x+y*w);
		}
}
