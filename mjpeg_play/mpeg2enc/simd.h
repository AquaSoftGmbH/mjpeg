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

#ifdef __cplusplus
extern "C" 
{
#endif

/*
 * Reference / Architecture neutral implementations of functions which
 * may have architecture-specific implementations.
 *
 * N.b. linkage and implemenations of such functions is C not C++!
 * N.b. some of these refewrence implementations may be invoked by
 * architecture specific routines as fall-backs. Hence their inclusion here...
 *
 */


#include "quantize_ref.h"
#include "transfrm_ref.h"
#include "predict_ref.h"

#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM)

	
int quant_non_intra_3dnow( int16_t *src, int16_t *dst,
						   int q_scale_type,
						   int *mquant);
int quant_non_intra_sse( int16_t *src, int16_t *dst,
						 int q_scale_type,
						 int *mquant);
int quant_non_intra_mmx( int16_t *src, int16_t *dst,
						 int q_scale_type,
						 int *mquant);
	
int quantize_ni_mmx(int16_t *dst, int16_t *src, 
					uint16_t *quant_mat, 
					uint16_t *i_quant_mat, 
					int imquant, int mquant, 
					int sat_limit) __asm__ ("quantize_ni_mmx");

int quant_weight_coeff_sum_mmx (int16_t *blk, uint16_t *i_quant_mat ) __asm__ ("quant_weight_coeff_sum_mmx");
int cpuid_flags();

void iquant_non_intra_m1_extmmx(int16_t *src, int16_t *dst, uint16_t *qmat) __asm__ ("iquant_non_intra_m1_extmmx");
void iquant_non_intra_m1_mmx(int16_t *src, int16_t *dst, uint16_t *qmat) __asm__ ("iquant_non_intra_m1_mmx");
void iquant_non_intra_m2_extmmx(int16_t *src, int16_t *dst, uint16_t *qmat) __asm__ ("iquant_non_intra_extmmx");
void iquant_non_intra_m2_mmx(int16_t *src, int16_t *dst, uint16_t *qmat) __asm__ ("iquant_non_intra_mmx");

void iquant_non_intra_extmmx(int16_t *src, int16_t *dst, int mquant );
void iquant_non_intra_mmx(int16_t *src, int16_t *dst, int mquant );

extern int field_dct_best_mmx( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);

extern void fdct_mmx( int16_t * blk ) __asm__ ("fdct_mmx");
extern void idct_mmx( int16_t * blk ) __asm__ ("idct_mmx");

extern void add_pred_mmx (uint8_t *pred, uint8_t *cur,
						  int lx, int16_t *blk) __asm__ ("add_pred_mmx");
extern  void sub_pred_mmx (uint8_t *pred, uint8_t *cur,
						  int lx, int16_t *blk) __asm__ ("sub_pred_mmx");

void predcomp_00_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_00_mmxe");
void predcomp_10_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_10_mmxe");
void predcomp_11_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_11_mmxe");
void predcomp_01_mmxe(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_01_mmxe");

void predcomp_00_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_00_mmx");
void predcomp_10_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_10_mmx");
void predcomp_11_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_11_mmx");
void predcomp_01_mmx(uint8_t *src,uint8_t *dst,int lx, int w, int h, int addflag) __asm__ ("predcomp_01_mmx");


void pred_comp_mmxe(
	uint8_t *src, uint8_t *dst,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);
void pred_comp_mmx(
	uint8_t *src, uint8_t *dst,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);

#endif


/*
 * Global function pointers to functions with architecture-specific
 * implementations 
 */

extern int (*pquant_non_intra)( int16_t *src, int16_t *dst,
						 int q_scale_type, 
						 int *nonsat_mquant);
extern int (*pquant_weight_coeff_sum)(int16_t *blk, uint16_t*i_quant_mat );

extern void (*piquant_non_intra)(int16_t *src, int16_t *dst, int mquant );

#ifdef __cplusplus
}
#endif

#endif /* __SIMD_H__ */
