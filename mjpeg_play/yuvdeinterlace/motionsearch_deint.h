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
 * FILE: motionsearch_deint.h                              *
 *                                                         *
 ***********************************************************/

void motion_compensate (uint8_t * r, uint8_t * f0, uint8_t * f1, uint8_t * f2,
			int w, int h, int field);
