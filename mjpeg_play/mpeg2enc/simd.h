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
#include "mjpeg_types.h"


#ifdef HAVE_X86CPU

extern "C" 
{

int quant_non_intra_3dnow( Picture *picture,int16_t *src, int16_t *dst,
						   int mquant, int *nonsat_mquant);
int quant_non_intra_sse( Picture *picture,int16_t *src, int16_t *dst,
						 int mquant, int *nonsat_mquant);
int quant_non_intra_mmx( Picture *picture,int16_t *src, int16_t *dst,
						 int mquant, int *nonsat_mquant);
							
int quantize_ni_mmx(int16_t *dst, int16_t *src, 
					uint16_t *quant_mat, 
					uint16_t *i_quant_mat, 
					int imquant, int mquant, 
					int sat_limit) __asm__ ("quantize_ni_mmx");
int quant_weight_coeff_sum_mmx (int16_t *blk, uint16_t *i_quant_mat ) __asm__ ("quant_weight_coeff_sum_mmx");
int cpuid_flags();

void iquant_non_intra_m1_sse(int16_t *src, int16_t *dst, uint16_t *qmat) __asm__ ("iquant_non_intra_m1_sse");
void iquant_non_intra_m1_mmx(int16_t *src, int16_t *dst, uint16_t *qmat) __asm__ ("iquant_non_intra_m1_mmx");


void predcomp_00_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_00_mmxe");
void predcomp_10_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_10_mmxe");
void predcomp_11_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_11_mmxe");
void predcomp_01_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_01_mmxe");

void predcomp_00_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_00_mmx");
void predcomp_10_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_10_mmx");
void predcomp_11_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_11_mmx");
void predcomp_01_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_01_mmx");

}

#endif

#endif /* __SIMD_H__ */
