/* quantize.c, quantization / inverse quantization                          */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */
/* Modifications and enhancements (C) 2000/2001 Andrew Stevens */

/* These modifications are free software; you can redistribute it
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


#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_FENV_H
#include <fenv.h>
#endif
#include "global.h"
#include "cpu_accel.h"
#include "simd.h"
#include "fastintfns.h"



int (*pquant_non_intra)( int16_t *src, int16_t *dst,
						 int q_scale_type, 
						 int mquant, int *nonsat_mquant);
int (*pquant_weight_coeff_sum)(int16_t *blk, uint16_t*i_quant_mat );

void (*piquant_non_intra)(int16_t *src, int16_t *dst, int mquant );


#ifdef HAVE_ALTIVEC
extern void enable_altivec_quantization();
#endif

/*
  Initialise quantization routines.
  Currently just setting up MMX routines if available...
*/

void init_quantizer(void)
{
#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM)
	int flags = cpu_accel();
	const char *opt_type1, *opt_type2;
	if( (flags & ACCEL_X86_MMX) != 0 ) /* MMX CPU */
	{
		if( (flags & ACCEL_X86_3DNOW) != 0 )
		{
			opt_type1 = "3DNOW and";
			pquant_non_intra = quant_non_intra_3dnow;
		}
		else if ( (flags & ACCEL_X86_SSE) != 0 )
		{
			opt_type1 = "SSE and";
			pquant_non_intra = quant_non_intra_sse;
		}
		else 
		{
			opt_type1 = "MMX and";
			pquant_non_intra = quant_non_intra_mmx;
		}

		if ( (flags & ACCEL_X86_MMXEXT) != 0 )
		{
			opt_type2 = "EXTENDED MMX";
			pquant_weight_coeff_sum = quant_weight_coeff_sum_mmx;
			piquant_non_intra = iquant_non_intra_mmx;
		}
		else
		{
			opt_type2 = "MMX";
			pquant_weight_coeff_sum = quant_weight_coeff_sum_mmx;
			piquant_non_intra = iquant_non_intra_mmx;
		}
		mjpeg_info( "SETTING %s %s for QUANTIZER!", opt_type1, opt_type2);
	}
	else
#endif
	{
		pquant_non_intra = quant_non_intra;	  
		pquant_weight_coeff_sum = quant_weight_coeff_sum;
		piquant_non_intra = iquant_non_intra;
	}
#ifdef HAVE_ALTIVEC
	if (cpu_accel())
	    enable_altivec_quantization();
#endif
}

void iquantize( Picture *picture )
{
	int j,k;
	int16_t (*qblocks)[64] = picture->qblocks;
	for (k=0; k<mb_per_pict; k++)
	{
		if (picture->mbinfo[k].mb_type & MB_INTRA)
			for (j=0; j<block_count; j++)
				iquant_intra(qblocks[k*block_count+j],
							 qblocks[k*block_count+j],
							 picture->dc_prec,
							 picture->mbinfo[k].mquant);
		else
			for (j=0;j<block_count;j++)
				iquant_non_intra(qblocks[k*block_count+j],
								 qblocks[k*block_count+j],
								 picture->mbinfo[k].mquant);
	}
}

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
