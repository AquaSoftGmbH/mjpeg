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

/*
 *
 * Generates a vector sad's for 4*4 sub-sampled pel (qpel) data (with
 * co-ordinates and top-left qpel address) from specified rectangle
 * against a specified 16*16 pel (4*4 qpel) reference block.  The
 * generated vector contains results only for those sad's that fall
 * below twice the running best sad and are aligned on 8-pel
 * boundaries
 *
 * TODO: Experiment with doing 4-pel boundaries in the x-axis
 * and "seeding" the threshold with the sad for co-ordinates of the
 * reference block.
 *
 * sad = Sum Absolute Differences
 *
 * */

int SIMD_SUFFIX(qblock_8grid_dists)( uint8_t *blk,  uint8_t *ref,
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
				load_blk( curblk, rowstride, h );
			}
			weight = SIMD_SUFFIX(qblock_sad)(ref, h, rowstride);
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

int SIMD_SUFFIX(qblock_near_dist)( uint8_t *blk,  uint8_t *ref,
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
			load_blk( curblk, rowstride, h );
		if( i != 1)
		{
			x = basex+4;
			shift_blk(8);
		}
		weight = SIMD_SUFFIX(qblock_sad)(ref, h, rowstride);
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
#undef concat
