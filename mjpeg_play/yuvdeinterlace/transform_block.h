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
 * FILE: transform_block.h                                 *
 *                                                         *
 ***********************************************************/

void transform_block (uint8_t * a1, uint8_t * a2, uint8_t * a3,
		      int rowstride);
void transform_block_chroma (uint8_t * a1, uint8_t * a2, uint8_t * a3,
			     int rowstride);
