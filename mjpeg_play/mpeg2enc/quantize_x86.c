/* quantize_x86.c Quantization / inverse quantization    
   In compiler (gcc) embdeed assmbley language...
*/

/* Copyright (C) 2000 Andrew Stevens */

/* This program is free software; you can redistribute it
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



/* 
 * 3DNow version of
 * Quantisation for non-intra blocks using Test Model 5 quantization
 *
 * this quantizer has a bias of 1/8 stepsize towards zero
 * (except for the DC coefficient)
 *
 *	PRECONDITION: src dst point to *disinct* memory buffers...
 *	              of block_count *adjacent* int16_t[64] arrays...
 *
 * RETURN: 1 If non-zero coefficients left after quantisaion 0 otherwise
 */


#include <stdio.h>
#include <math.h>
#include <fenv.h>
#include "config.h"
#include "global.h"
#include "cpu_accel.h"
#include "simd.h"
#include "attributes.h"
#include "mmx.h"

int quant_non_intra_3dnow(
	pict_data_s *picture,
	int16_t *src, int16_t *dst,
	int mquant,
	int *nonsat_mquant)
{
	int i;
	int x,y,d;
	int flags = 0;
	int nzflag;
	int clipvalue  = mpeg1 ? 255 : 2047;
	int saturated = 0;
	float qtr = 1.0f/4.0f;
	int   coeff_count = 64*block_count;
	mmx_t yi;
	int j;

	float *i_quant_matf = i_inter_q_tblf[mquant]; 
	uint16_t *quant_mat = inter_q;

	nzflag = 0;
	for (i=0; i<coeff_count; ++i)
	{
		if( (i%64) == 0 )
		{
			nzflag = (nzflag<<1) | !!flags;
			flags = 0;
		  
		}
		/* RJ: save one divide operation */
	  
		x = (src[i] >= 0 ? src[i] : -src[i]);
		d = (int)quant_mat[(i&63)]; 
		y = (32*x + (d>>1))/(d*2*mquant);
		if ( y > clipvalue )
		{
			printf( "XXXXX");
			if( saturated )
			{
				y = clipvalue;
			}
			else
			{
				int new_mquant = next_larger_quant( picture, mquant );
				if( new_mquant != mquant )
					mquant = new_mquant;
				else
				{
					saturated = 1;
				}
				i=0;
				nzflag =0;
				continue;
			}
		}
		dst[i] = (src[i] >= 0 ? y : -y);
		flags |= dst[i];
		  
		yi.d[0] = 16*src[i];
		movd_m2r( yi, mm0 );
		pi2fd_r2r( mm0, mm0);
		movd_m2r( i_quant_matf[j], mm1 );
		movd_m2r( qtr, mm2 );
		pfmul_r2r( mm1, mm0 );
		pf2id_r2r( mm0, mm0 );
		movd_r2m(mm0, yi);
		femms();
		if( x < yi.d[0]-1 || x > yi.d[1] + 1 )
		{
			printf("\nOO src=%d q=%d mq=%d mq'=%f NEW=%d ORG=%d\n",
				   src[j], mquant, quant_mat[j], 1.0/i_quant_matf[j], y, x);
				 
		}
	}
	nzflag = (nzflag<<1) | !!flags;
	return nzflag;
}
