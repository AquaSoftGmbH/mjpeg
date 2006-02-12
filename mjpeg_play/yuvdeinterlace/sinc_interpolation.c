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

  for(y=field;y<h;y+=2)
	{
	memcpy ( frame+y*w, inframe+y*w, w);

	for(x=0;x<w;x++)
		{
			*(frame+(x)+(y+1)*w) = (*(inframe+(x)+(y)*w)+*(inframe+(x)+(y+2)*w))/2;
		}
	}

}
