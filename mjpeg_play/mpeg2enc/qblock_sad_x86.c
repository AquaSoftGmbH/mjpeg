/*
 *
 * qblockdist_sse.
 * Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
 *
 * Fast block sum-absolute difference computation for a rectangular area 4*x
 * by y where y > h against a 4 by h block.
 *
 * Used for 4*4 sub-sampled motion compensation calculations.
 * 
 *
 * This file is part of mpeg2enc, a free MPEG-2 video stream encoder
 * based on the original MSSG reference design
 *
 * mpeg2enc is free software; you can redistribute new parts
 * and/or modify under the terms of the GNU General Public License 
 * as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * See the files for those sections (c) MSSG
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "simd.h"
#include "attributes.h"
#include "mmx.h"
#include "stdio.h"

/*
  Register usage:
  mm0-mm3  Hold the current row 
  mm4      Used for accumulating partial SAD
  mm7      Holds zero
 */

static inline void mmx_zero_reg (void)
{
	/*  load 0 into mm7	 */
	pxor_r2r (mm7, mm7);
}

static inline void load_blk(uint8_t *blk,uint32_t rowstride)
{
	movq_m2r( *blk, mm0);
	blk += rowstride;
	movq_m2r( *blk, mm1);
	blk += rowstride;
	movq_m2r( *blk, mm2);
	blk += rowstride;
	movq_m2r( *blk, mm3);
}

static __inline__ void shift_blk(const uint32_t shift)
{
	psrlq_i2r( shift,mm0);
	psrlq_i2r( shift,mm1);
	psrlq_i2r( shift,mm2);
	psrlq_i2r( shift,mm3);
}

static __inline__ int qblock_sad_sse(uint8_t *refblk, 
								  uint32_t h,
								  uint32_t rowstride)
{
	int res;
	pxor_r2r 	(mm4,mm4);
			
	movq_r2r	(mm0,mm5);		/* First row */
	movd_m2r	(*refblk, mm6);
	pxor_r2r    ( mm7, mm7);
	refblk += rowstride;
	punpcklbw_r2r	( mm7, mm5);
	punpcklbw_r2r	( mm7, mm6);
	psadbw_r2r      ( mm5, mm6);
	paddw_r2r     ( mm6, mm4 );
	


	movq_r2r	(mm1,mm5);		/* Second row */
	movd_m2r	(*refblk, mm6);
	refblk += rowstride;
	punpcklbw_r2r	( mm7, mm5);
	punpcklbw_r2r	( mm7, mm6);
	psadbw_r2r      ( mm5, mm6);
	paddw_r2r     ( mm6, mm4 );

	if( h == 4 )
	{

		movq_r2r	(mm2,mm5);		/* Third row */
		movd_m2r	(*refblk, mm6);
		refblk += rowstride;
		punpcklbw_r2r	( mm7, mm5);
		punpcklbw_r2r	( mm7, mm6);
		psadbw_r2r      ( mm5, mm6);
		paddw_r2r     ( mm6, mm4 );

		
		movq_r2r	(mm3,mm5);		/* Fourth row */
		movd_m2r	(*refblk, mm6);
		punpcklbw_r2r	( mm7, mm5);
		punpcklbw_r2r	( mm7, mm6);
		psadbw_r2r      ( mm5, mm6);
		paddw_r2r     ( mm6, mm4 );
		
	}
	movd_r2m      ( mm4, res );

	return res;
}



static __inline__ int qblock_sad_mmx(uint8_t *refblk, 
								  uint32_t h,
								  uint32_t rowstride)
{
	int res;
	pxor_r2r 	(mm4,mm4);
			
	movq_r2r	(mm0,mm5);		/* First row */
	movd_m2r	(*refblk, mm6);
	pxor_r2r    ( mm7, mm7);
	refblk += rowstride;
	punpcklbw_r2r	( mm7, mm5);

	punpcklbw_r2r	( mm7, mm6);

	movq_r2r		( mm5, mm7);
	psubusw_r2r	( mm6, mm5);

	psubusw_r2r   ( mm7, mm6);

	paddw_r2r     ( mm5, mm4);
	paddw_r2r     ( mm6, mm4 );
	


	movq_r2r	(mm1,mm5);		/* Second row */
	movd_m2r	(*refblk, mm6);
	pxor_r2r    ( mm7, mm7);
	refblk += rowstride;
	punpcklbw_r2r	( mm7, mm5);
	punpcklbw_r2r	( mm7, mm6);
	movq_r2r		( mm5, mm7);
	psubusw_r2r	( mm6, mm5);
	psubusw_r2r   ( mm7, mm6);
	paddw_r2r     ( mm5, mm4);
	paddw_r2r     ( mm6, mm4 );

	if( h == 4 )
	{

		movq_r2r	(mm2,mm5);		/* Third row */
		movd_m2r	(*refblk, mm6);
		pxor_r2r    ( mm7, mm7);
		refblk += rowstride;
		punpcklbw_r2r	( mm7, mm5);
		punpcklbw_r2r	( mm7, mm6);
		movq_r2r		( mm5, mm7);
		psubusw_r2r	( mm6, mm5);
		psubusw_r2r   ( mm7, mm6);
		paddw_r2r     ( mm5, mm4);
		paddw_r2r     ( mm6, mm4 );
		
		movq_r2r	(mm3,mm5);		/* Fourth row */
		movd_m2r	(*refblk, mm6);
		pxor_r2r    ( mm7, mm7);
		punpcklbw_r2r	( mm7, mm5);
		punpcklbw_r2r	( mm7, mm6);
		movq_r2r		( mm5, mm7);
		psubusw_r2r	( mm6, mm5);
		psubusw_r2r   ( mm7, mm6);
		paddw_r2r     ( mm5, mm4);
		paddw_r2r     ( mm6, mm4 );
	}


	movq_r2r      ( mm4, mm5 );
    psrlq_i2r     ( 32, mm5 );
    paddw_r2r     ( mm5, mm4 );
	movq_r2r      ( mm4, mm6 );
    psrlq_i2r     ( 16, mm6 );
    paddw_r2r     ( mm6, mm4 );
	movd_r2m      ( mm4, res );

	return res & 0xffff;
}


__inline__ int fastmax( register int x, register int y )
{
	asm( "cmpl %1, %0\n"
	     "cmovl %1, %0\n"
         : "+r" (x) :  "r" (y)
       );
	return x;
}

__inline__ int fastmin( register int x, register int y )
{
	asm( "cmpl %1, %0\n"
	     "cmovg %1, %0\n"
         : "+r" (x) :  "rm" (y)
       );
	return x;
}

/*
 *
 *
 *
 *
 * Density of sampling grid (in pel)
 *
 */

int qblock_8grid_dists( uint8_t *blk,  uint8_t *ref,
						int ilow,int jlow,
						uint32_t width, uint32_t depth, 
						int h, int rowstride, mc_result_s *resvec)
{
	uint32_t x,y;
	uint8_t *currowblk = blk;
	uint8_t *curblk;
	mc_result_s *cres = resvec;
	int      gridrowstride = rowstride<<1;
	int      threshold = 100000;

	for( y=0; y <= depth ; y+=8)
	{
		curblk = currowblk;
		for( x = 0; x <= width; x += 8)
		{
			int weight;
			if( (x & 15) == 0 )
			{
				load_blk( curblk, rowstride );
			}
			weight = qblock_sad_sse(ref, h, rowstride);
			if( weight <= threshold )
			{
				threshold =  fastmin(weight<<2,threshold);
				cres->weight = (uint16_t)weight;
				cres->x = (uint8_t)x;
				cres->y = (uint8_t)y;
				cres->blk = curblk;
				++cres;
			}
			curblk += 2;
			shift_blk(16);
		}
		currowblk += gridrowstride;
	}
	emms();
	return cres - resvec;
}

/*
 * across - Look at neighbours to the right (1 or 0)
 * down   - look at neighbours to below (1 or 0)
 */

int qblock_near_dist( uint8_t *blk,  uint8_t *ref,
					  int basex, int basey,
					  int across, int down,
					  int threshold,
					  int h, int rowstride, mc_result_s *resvec)
{
	int x,y;
	uint8_t *curblk = blk;
	mc_result_s *cres = resvec;
	int start = 1-across;
	int fin   = across + down + (across&down);
	int i;
	int weight;
	y = basey;
	for( i = start; i < fin; ++i )
	{
		if (i==1)
		{
			curblk = blk+rowstride;
			x=basex;
			y+=4;
		}
		if( i < 2)
			load_blk( curblk, rowstride );
		if( i != 1)
		{
			x = basex+4;
			shift_blk(8);
		}
		weight = qblock_sad_sse(ref, h, rowstride);
		if( weight <= threshold )
		{
			cres->weight = weight;
			cres->x = (uint8_t)x;
			cres->y = (uint8_t)y;
			cres->blk = curblk;
			++cres;
		}
	}		
	emms();
	return cres - resvec;
}
