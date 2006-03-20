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
 * FILE: sinc_interpolation.h                              *
 *                                                         *
 ***********************************************************/

void interpolate_field (uint8_t * frame, uint8_t * inframe, int w, int h,
			 int field);
void bandpass_field (uint8_t * frame, uint8_t * inframe, int w, int h);
void downsample_to_framesize (uint8_t * out, uint8_t * in, int w, int h);
