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
 * FILE: blend_fields.h                                    *
 *                                                         *
 ***********************************************************/

extern void (*blend_fields) (uint8_t * dst[3], uint8_t * src[3]);
void blend_fields_non_accel (uint8_t * dst[3], uint8_t * src[3]);
