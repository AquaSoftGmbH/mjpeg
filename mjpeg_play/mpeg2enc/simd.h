/*
 *
 * qblockdist_sse.simd.h
 * Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
 *
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
#ifndef __SIMD_H__
#define __SIMD_H__
#include <config.h>
#include <inttypes.h>


#ifdef HAVE_X86CPU



int quant_non_intra_3dnow(	struct pict_data *picture,int16_t *src, int16_t *dst,
							int mquant, int *nonsat_mquant);
int quant_non_intra_sse(	struct pict_data *picture,int16_t *src, int16_t *dst,
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


void predcomp_00_mmxe(char *src,char *dst,int lx, int w, int h, int addflag);
void predcomp_10_mmxe(char *src,char *dst,int lx, int w, int h, int addflag);
void predcomp_11_mmxe(char *src,char *dst,int lx, int w, int h, int addflag);
void predcomp_01_mmxe(char *src,char *dst,int lx, int w, int h, int addflag);

void predcomp_00_mmx(char *src,char *dst,int lx, int w, int h, int addflag);
void predcomp_10_mmx(char *src,char *dst,int lx, int w, int h, int addflag);
void predcomp_11_mmx(char *src,char *dst,int lx, int w, int h, int addflag);
void predcomp_01_mmx(char *src,char *dst,int lx, int w, int h, int addflag);

#endif

#endif /* __SIMD_H__ */
