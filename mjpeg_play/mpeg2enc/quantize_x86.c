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

int dudctr = 0;
int quant_non_intra(
	pict_data_s *picture,
	int16_t *src, int16_t *dst,
	int mquant,
	int *nonsat_mquant)
{
	int satshift = mpeg1 ? 8 : 4;
	float *i_quant_matf = i_inter_q_tblf[mquant]; 
	int   coeff_count = 64*block_count;
	uint32_t nzflag, flags;
	int16_t *psrc, *pdst;
	float *piqf;
	int i;
/*	float ir[4];
	short is[4];
	int32_t id[4];*/
	uint32_t tmp;

	/* Initialise zero block flags */
	nzflag = 0;
	psrc = src;
	pdst = dst;
	flags = 0;
	for (i=0; i < coeff_count ; i+=4)
	{

		if( (i & 63) == 0 )
		{
			nzflag = (nzflag<<1) | !!flags;
			flags = 0;
			piqf = i_quant_matf;
		}

		/* Load 1 into mm6 */
		__asm__ ( "movl %0, %%eax\n" 
				  "movd %%eax, %%mm6\n"
				  : :"g" (1) : "eax" );
		/* Load 4 words, unpack into mm2 and mm3 (with sign extension!)
		 */

		movq_m2r( *(mmx_t *)&psrc[0], mm2 );
		movq_r2r( mm2, mm7 );
		psraw_i2r( 16, mm7 );	/* Replicate sign bits mm2 in mm7 */
		movq_r2r( mm2, mm3 );
		punpcklwd_r2r( mm7, mm2 ); /* Unpack with sign extensions */
		punpckhwd_r2r( mm7, mm3);



		/* Multiply by sixteen... */
		pslld_i2r( 4, mm2 );
		pslld_i2r( 4, mm3 );
		
		/*
		   Load the inverse quantisation factors from the 
		   table in to mm4 and mm5
		   Interleaved with converting mm2 and mm3 to float's
		   to (hopefully) maximise parallelism.
		 */
		movq_m2r( *(mmx_t*)&piqf[0], mm4);
		pi2fd_r2r( mm2, mm2);
		movq_m2r( *(mmx_t*)&piqf[2], mm5);
		pi2fd_r2r( mm3, mm3);

		/* "Divide" by multiplying by inverse quantisation */
		pfmul_r2r( mm4, mm2 );
		pfmul_r2r( mm5, mm3);


		/* Convert back to integers */
		pf2id_r2r( mm2, mm2);
		pf2id_r2r( mm3, mm3);

		/* Convert the two pairs of double words into four words */
		packssdw_r2r(  mm3, mm2);


		/* Saturate by replicating the top-bits */
		movq_r2r( mm2, mm4 );
		psraw_i2r( 16, mm4 );
		psllw_i2r( 15, mm4 );
		movd_m2r( satshift, mm3 );
		psllw_r2r( mm3, mm2 );
		psrlw_i2r( 1, mm2 );
		psubb_r2r( mm6, mm3 );	/* mm6 has 1 in it! */
		por_r2r( mm4, mm2 );
		psraw_r2r( mm3, mm2 );

		/* Store and accumulate zero-ness */
		movq_r2r( mm2, mm3 );
		movq_r2m( mm2, *(mmx_t*)pdst );
		psrlq_i2r( 32, mm3 );
		por_r2r( mm3, mm2 );
		movd_r2m( mm2, tmp );
		flags |= tmp;
/*
		{
			int i,j;
			int x, d, y;
			for( j = 0; j < 4; ++j )
			{
				i = (&psrc[j]-src);
				x = (psrc[j] >= 0 ? psrc[j] : -psrc[j]);
				d = (int)inter_q[(i&63)]; 
				y = (32*x + (d>>1))/(d*2*mquant);
				if( y > 255 )
					y = 255;
				y = psrc[j] >= 0 ? y : -y;
				if( y != pdst[j] )
				{

					if( y == 2)
					{
						printf( "\n%08d  %08d  %08d  %08d\n",
								psrc[0], psrc[1], psrc[2], psrc[3]  );
						printf( "%3.4f  %3.4f  %3.4f  %3.4f\n",
								ir[0], ir[1], ir[2], ir[3] );
						printf( "%08x %08x   %08x %08x\n", 
								id[0], id[1], id[2], id[3] );
						printf( "%08x %08x   %08x %08x\n", 
								is[0], is[1], is[2], is[3] );

					}

					printf( "%03d:(%d) src=%04d dst=%04d ref=%04d i=%02.02f mq=%03d qm=%03d iiqf=%.0f\n", 
							i, j,
							(int)psrc[j], (int)pdst[j], 
							 y, ir[j], mquant, inter_q[(i&63)] ,
							1.0/piqf[j]);
				}
			}
		}
*/
		piqf += 4;
		pdst += 4;
		psrc += 4;
			
	}
	femms();

	nzflag = (nzflag<<1) | (!!flags);
	return nzflag;
}
