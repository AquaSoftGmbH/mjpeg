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
 * FILE: transform_block.c                                 *
 *                                                         *
 ***********************************************************/

#include "config.h"
#include "mjpeg_types.h"
#include "transform_block.h"

void
transform_block (uint8_t * a1, uint8_t * a2, uint8_t * a3, int rowstride)
{
  int x, y;

  for (y = 0; y < 8; y++)
    {
      for (x = 0; x < 8; x++)
	{
	  *(a1) = (*(a2) + *(a3)) / 2;
	  a1++;
	  a2++;
	  a3++;
	}
      /* process every second line */
      a1 += rowstride - 8;
      a2 += rowstride - 8;
      a3 += rowstride - 8;
    }
}

void
transform_block_chroma (uint8_t * a1, uint8_t * a2, uint8_t * a3,
			int rowstride)
{
  int x, y;

  for (y = 0; y < 4; y++)
    {
      for (x = 0; x < 4; x++)
	{
	  *(a1) = (*(a2) + *(a3)) / 2;
	  a1++;
	  a2++;
	  a3++;
	}
      /* process every second line */
      a1 += rowstride - 4;
      a2 += rowstride - 4;
      a3 += rowstride - 4;
    }
}
