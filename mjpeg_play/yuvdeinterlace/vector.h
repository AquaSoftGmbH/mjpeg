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
 * FILE: vector.h                                          *
 *                                                         *
 ***********************************************************/

struct vector
{
  int x;
  int y;
  uint32_t min;
  uint32_t min23;
  uint32_t min21;
};
