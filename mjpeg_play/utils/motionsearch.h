/* (C) 2000/2001 Andrew Stevens */

/*  This software is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#ifndef __MOTIONSEARCH_H__
#define __MOTIONSEARCH_H__

#include <config.h>
#include "mjpeg_types.h"

struct mc_result
{
	uint16_t weight;
	int8_t x;
	int8_t y;
};

typedef struct mc_result mc_result_s;

#define MAX_MATCHES (256*256/4)

typedef struct _mc_result_vec {
	int len;
	mc_result_s mcomps[MAX_MATCHES];
} mc_result_set;

/*
 * Function pointers for selecting CPU specific implementations
 *
 */

extern void (*pfind_best_one_pel)( mc_result_set *sub22set,
							uint8_t *org, uint8_t *blk,
							int i0, int j0,
							int ihigh, int jhigh,
							int rowstride, int h, 
							mc_result_s *best
	);
extern int (*pbuild_sub22_mcomps)( mc_result_set *sub44set,
							mc_result_set *sub22set,
							int i0,  int j0, int ihigh, int jhigh, 
							int null_mc_sad,
							uint8_t *s22org,  uint8_t *s22blk, 
							int frowstride, int fh,
							int reduction);

extern int (*pbuild_sub44_mcomps)( mc_result_set *sub44set,
							int ilow, int jlow, int ihigh, int jhigh, 
							int i0, int j0,
							int null_mc_sad,
							uint8_t *s44org, uint8_t *s44blk, 
							int qrowstride, int qh,
							int reduction );
extern int (*pdist2_22)( uint8_t *blk1, uint8_t *blk2,
				  int rowstride, int h);
extern int (*pbdist2_22)( uint8_t *blk1f, uint8_t *blk1b, 
				   uint8_t *blk2,
				   int rowstride, int h);

extern int (*pvariance)(uint8_t *mb, int size, int rowstride);


extern int (*pdist22) ( uint8_t *blk1, uint8_t *blk2,  int frowstride, int fh);
extern int (*pdist44) ( uint8_t *blk1, uint8_t *blk2,  int qrowstride, int qh);
extern int (*pdist1_00) ( uint8_t *blk1, uint8_t *blk2,  int rowstride, int h, int distlim);
extern int (*pdist1_01) (uint8_t *blk1, uint8_t *blk2, int rowstride, int h);
extern int (*pdist1_10) (uint8_t *blk1, uint8_t *blk2, int rowstride, int h);
extern int (*pdist1_11) (uint8_t *blk1, uint8_t *blk2, int rowstride, int h);

extern int (*pdist2) (uint8_t *blk1, uint8_t *blk2,
			   int rowstride, int hx, int hy, int h);
  
  
extern int (*pbdist2) (uint8_t *pf, uint8_t *pb,
					   uint8_t *p2, int rowstride, int hxf, int hyf, int hxb, int hyb, int h);

extern int (*pbdist1) (uint8_t *pf, uint8_t *pb,
				uint8_t *p2, int rowstride, 
				int hxf, int hyf, int hxb, int hyb, int h);


void init_motion_search();
int round_search_radius( int radius );
void subsample_image( uint8_t *image, int rowstride, 
					  uint8_t *sub22_image, 
					  uint8_t *sub44_image);

#endif  /* __MOTIONSEARCH_H__ */
