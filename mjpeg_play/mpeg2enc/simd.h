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
 * mpeg2enc is distributed in the hope that it will be useful,
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

#include <stdint.h>

struct pict_data;

#ifdef X86_CPU

struct mc_result
{
	uint16_t weight;
	uint8_t x;
	uint8_t y;
	uint8_t *blk;
};

typedef struct mc_result mc_result_s;

int qblock_8grid_dists_sse( uint8_t *blk,  uint8_t *ref,
						int ilow, int jlow,
						uint32_t width, uint32_t depth, 
						int h, int rowstride, mc_result_s *resvec);
int qblock_near_dist_sse( uint8_t *blk,  uint8_t *ref,
					  int basex, int basey,
					  int across, int down,
					  int threshold,
					  int h, int rowstride, mc_result_s *resvec);
int qblock_8grid_dists_mmx( uint8_t *blk,  uint8_t *ref,
						int ilow, int jlow,
						uint32_t width, uint32_t depth, 
						int h, int rowstride, mc_result_s *resvec);
int qblock_near_dist_mmx( uint8_t *blk,  uint8_t *ref,
					  int basex, int basey,
					  int across, int down,
					  int threshold,
					  int h, int rowstride, mc_result_s *resvec);

int quant_non_intra_3dnow(	struct pict_data *picture,int16_t *src, int16_t *dst,
							int mquant, int *nonsat_mquant);
int quant_non_intra_mmx(	struct pict_data *picture,int16_t *src, int16_t *dst,
							int mquant, int *nonsat_mquant);
							
int quantize_ni_mmx(short *dst, short *src, short *quant_mat, 
						   short *i_quant_mat, 
						   int imquant, int mquant, int sat_limit);
int quant_weight_coeff_sum_mmx (short *blk, unsigned short*i_quant_mat );
int cpuid_flags();

void iquant_non_intra_m1_sse(int16_t *src, int16_t *dst, uint16_t *qmat);
void iquant_non_intra_m1_mmx(int16_t *src, int16_t *dst, uint16_t *qmat);

int dist1_00_SSE(uint8_t *blk1, uint8_t *blk2, int lx, int h, int distlim);
int dist1_01_SSE(uint8_t *blk1, uint8_t *blk2, int lx, int h);
int dist1_10_SSE(uint8_t *blk1, uint8_t *blk2, int lx, int h);
int dist1_11_SSE(uint8_t *blk1, uint8_t *blk2, int lx, int h);
int fdist1_SSE ( uint8_t *blk1, uint8_t *blk2,  int flx, int fh);
int qdist1_SSE ( uint8_t *blk1, uint8_t *blk2,  int flx, int fh);
int dist2_mmx( uint8_t *blk1, uint8_t *blk2,
			   int lx, int hx, int hy, int h);
int bdist2_mmx (uint8_t *pf, uint8_t *pb,
				uint8_t *p2, int lx,
				int hxf, int hyf, int hxb, int hyb, int h);
int bdist1_mmx (uint8_t *pf, uint8_t *pb,
				uint8_t *p2, int lx,
				int hxf, int hyf, int hxb, int hyb, int h);


int dist1_00_MMX ( uint8_t *blk1, uint8_t *blk2,  int lx, int h, int distlim);
int dist1_01_MMX(uint8_t *blk1, uint8_t *blk2, int lx, int h);
int dist1_10_MMX(uint8_t *blk1, uint8_t *blk2, int lx, int h);
int dist1_11_MMX(uint8_t *blk1, uint8_t *blk2, int lx, int h);
int fdist1_MMX ( uint8_t *blk1, uint8_t *blk2,  int flx, int fh);
int qdist1_MMX (uint8_t *blk1, uint8_t *blk2,  int qlx, int qh);
int dist2_mmx  ( uint8_t *blk1, uint8_t *blk2,
			   int lx, int hx, int hy, int h);
int bdist2_mmx (uint8_t *pf, uint8_t *pb,
				uint8_t *p2, int lx, 
				int hxf, int hyf, int hxb, int hyb, int h);
int bdist1_mmx (uint8_t *pf, uint8_t *pb,
				uint8_t *p2, int lx, 
				int hxf, int hyf, int hxb, int hyb, int h);

#endif
