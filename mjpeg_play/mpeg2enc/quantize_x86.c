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




#include <config.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_FENV_H
#include <fenv.h>
#endif
#include "syntaxconsts.h"
#include "mjpeg_logging.h"
#include "fastintfns.h"
#include "cpu_accel.h"
#include "simd.h"
#include "mmx.h"
#include "tables.h"
#include "quantize_precomp.h"
#include "quantize_ref.h"
					
int quant_weight_coeff_sum_mmx (int16_t *blk, uint16_t *i_quant_mat );

void iquantize_non_intra_m1_mmx(int16_t *src, int16_t *dst, uint16_t *qmat);
void iquantize_non_intra_m2_mmx(int16_t *src, int16_t *dst, uint16_t *qmat);

/* 
 * Quantisation for non-intra blocks 
 *
 * Various versions for various SIMD instruction sets.  Not all of them
 * bother to implement the test model 5 quantisation of the reference source
 * (this has a bias of 1/8 stepsize towards zero - except for the DC coefficient).
 *
 * Actually, as far as I can tell even the reference source doesn't quite do it
 * for non-intra (though it *does* for intra).
 * 
 * Careful analysis of the code also suggests what it actually does is truncate
 * with a modest bias towards 1 (the d>>2 factor)
 *
 *	PRECONDITION: src dst point to *disinct* memory buffers...
 *	              of block_count *adjacent* int16_t[64] arrays...
 *
 *RETURN: A bit-mask of block_count bits indicating non-zero blocks (a 1).
 */
	

/*
 * 3D-Now version: simply truncates to zero...
 */
 
static int quant_non_intra_3dnow(	struct QuantizerWorkSpace *wsp,
									int16_t *src, int16_t *dst,
									int q_scale_type,
									int satlim,
									int *nonsat_mquant)
{
	int saturated;
	float *i_quant_matf; 
	int   coeff_count = 64*BLOCK_COUNT;
	int mquant = *nonsat_mquant;
	uint32_t nzflag, flags;
	int16_t *psrc, *pdst;
	float *piqf;
	int i;
	uint32_t tmp;

	/* Initialise zero block flags */
	/* Load 1 into mm6 */
	__asm__ ( "movl %0, %%eax\n" 
			  "movd %%eax, %%mm6\n"
			  : :"g" (1) : "eax" );
	/* Load satlim into mm1 */
	movd_g2r( satlim, mm1 );
	punpcklwd_r2r( mm1, mm1 );
	punpckldq_r2r( mm1, mm1 );
restart:
	i_quant_matf = wsp->i_inter_q_tblf[mquant];
	flags = 0;
	piqf = i_quant_matf;
	saturated = 0;
	nzflag = 0;
	psrc = src;
	pdst = dst;
	for (i=0; i < coeff_count ; i+=4)
	{

		/* TODO: For maximum efficiency this should be unrolled to allow
		   f.p. and int MMX to be interleaved... 
		*/

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

		/* "Divide" by multiplying by inverse quantisation
		 and convert back to integers*/
		pfmul_r2r( mm4, mm2 );
		pf2id_r2r( mm2, mm2);
		pfmul_r2r( mm5, mm3);
		pf2id_r2r( mm3, mm3);


		/* Convert the two pairs of double words into four words */
		packssdw_r2r(  mm3, mm2);


		/* Accumulate saturation... */
		movq_r2r( mm2, mm4 );

		pxor_r2r( mm5, mm5 );	// mm5 = -mm2
		pcmpgtw_r2r( mm1, mm4 ); // mm4 = (mm2 > satlim) 
		psubw_r2r( mm2, mm5 );
		pcmpgtw_r2r( mm1, mm5 ); // mm5 = -mm2 > satlim
		por_r2r( mm5, mm4 );  // mm4 = abs(mm2) > satlim
		movq_r2r( mm4, mm5 );
		psrlq_i2r( 32, mm5);
		por_r2r( mm5, mm4 );

		movd_g2r( saturated, mm5 ); // saturated |= mm4
		por_r2r( mm4, mm5 );
		movd_r2g( mm5, saturated );

		/* Store and accumulate zero-ness */
		movq_r2r( mm2, mm3 );
		movq_r2m( mm2, *(mmx_t*)pdst );
		psrlq_i2r( 32, mm3 );
		por_r2r( mm3, mm2 );
		movd_r2g( mm2, tmp );
		flags |= tmp;

		piqf += 4;
		pdst += 4;
		psrc += 4;

		if( (i & 63) == (63/4)*4 )
		{

			if( saturated )
			{
				int new_mquant = next_larger_quant( q_scale_type, mquant );
				if( new_mquant != mquant )
				{
					mquant = new_mquant;
					goto restart;
				}
				else
				{
					return quant_non_intra( wsp,
											src, dst, 
											q_scale_type,
										    satlim,
											nonsat_mquant);
				}
			}

			nzflag = (nzflag<<1) | !!flags;
			flags = 0;
			piqf = i_quant_matf;
		}
			
	}
	femms();

	//nzflag = (nzflag<<1) | (!!flags);
	return nzflag;
}

/* MMX version, that is 100% accurate
   It works by multiplying by the inverse of the quant factor.  However
   in certain circumstances this results in a value too low.  To compensate,
   it then multiplies by the quant factor, and compares this to the original
   value.  If this new value is less than or equal to the original value
   minus the quant factor, then the intermediate result is incremented.
   This adjust-by-1 is all that is needed to correct the result.

   8 words are handled per iteration; first four words uses mm0-mm3,
   last four words use mm4-mm7.
*/
static int quant_non_intra_mmx( struct QuantizerWorkSpace *wsp,
                                int16_t *src, int16_t *dst,
                                int q_scale_type,
                                int satlim,
                                int *nonsat_mquant)
{
    int saturated;
    int mquant = *nonsat_mquant;
    int   coeff_count = 64*BLOCK_COUNT;
    uint32_t nzflag, flags;
    int16_t *psrc, *pdst, *pdiv, *pmul, *pdivbase, *pmulbase;
    int i;
    uint32_t tmp;
    uint64_t negone_q,satlim_q;

    /* Load -1 into negone_q */
    pxor_r2r( mm6, mm6 );
    pcmpeqw_r2r( mm6, mm6 );
    movq_r2m( mm6, negone_q );

    /* Load satlim into satlim_q */
    movd_g2r( satlim, mm7 );
    punpcklwd_r2r( mm7, mm7 );
    punpckldq_r2r( mm7, mm7 );
    movq_r2m( mm7, satlim_q );

    pdivbase = wsp->i_inter_q_tbl[mquant];
    pmulbase = wsp->inter_q_tbl[mquant];

 restart:
    flags = 0;
    pdiv = pdivbase;
    pmul = pmulbase;
    saturated = 0;
    nzflag = 0;
    psrc = src;
    pdst = dst;
    for (i=0; i < coeff_count ; i+=8)
    {
        movq_m2r( psrc[0], mm3 ); // load values
        movq_m2r( psrc[4], mm7 ); // load values
        psllw_i2r( 4, mm3 );   // multiply by 16
        psllw_i2r( 4, mm7 );   // multiply by 16
        movq_r2r( mm3, mm0 );  // keep sign in mm3, make mm0=abs(mm3)
        movq_r2r( mm7, mm4 );  // keep sign in mm3, make mm0=abs(mm3)
        psraw_i2r( 15, mm3 );
        psraw_i2r( 15, mm7 );
        pxor_r2r( mm3, mm0 );
        pxor_r2r( mm7, mm4 );
        psubw_r2r( mm3, mm0 );
        psubw_r2r( mm7, mm4 );
        movq_m2r( pdiv[0], mm1 ); // "divide" by quant
        movq_m2r( pdiv[4], mm5 ); // "divide" by quant
        pmulhw_r2r( mm0, mm1 );
        pmulhw_r2r( mm4, mm5 );
        movq_m2r( pmul[0], mm2 ); // check that x*q > X-q; i.e. make sure x is not off by too much
        movq_m2r( pmul[4], mm6 ); // check that x*q > X-q; i.e. make sure x is not off by too much
        psubw_r2r( mm2, mm0 );
        psubw_r2r( mm6, mm4 );
        pmullw_r2r( mm1, mm2 );
        pmullw_r2r( mm5, mm6 );
        pcmpgtw_r2r( mm0, mm2 );
        pcmpgtw_r2r( mm4, mm6 );
        pxor_m2r( negone_q, mm2 );
        pxor_m2r( negone_q, mm6 );
        psubw_r2r( mm2, mm1 ); // if x*q <= X-q, increment x by 1
        psubw_r2r( mm6, mm5 ); // if x*q <= X-q, increment x by 1
        movq_r2r( mm1, mm0 ); // stash abs of result away
        movq_r2r( mm5, mm4 ); // stash abs of result away
        pxor_r2r( mm3, mm1 ); // make result have same sign as orig value
        pxor_r2r( mm7, mm5 ); // make result have same sign as orig value
        psubw_r2r( mm3, mm1 );        
        psubw_r2r( mm7, mm5 );        
        movq_r2m( mm1, pdst[0] ); // store result
        movq_r2m( mm5, pdst[4] ); // store result
        
        por_r2r( mm5, mm1 );
        movq_r2r( mm1, mm2 ); // set flags for non null responses
        psrlq_i2r( 32, mm2 );
        por_r2r( mm1, mm2 );
        movd_r2g( mm2, tmp );
        flags |= tmp;

        pcmpgtw_m2r( satlim_q, mm0 ); // did result exceed satlim?
        pcmpgtw_m2r( satlim_q, mm4 );
        por_r2r( mm4, mm0 );
        movq_r2r( mm0, mm1 );
        psrlq_i2r( 32, mm1 );
        por_r2r( mm1, mm0 );
        movd_r2g( mm1, tmp );
        saturated |= tmp;

        pdiv += 8;
        pmul += 8;
        pdst += 8;
        psrc += 8;

        if( (i & 63) == (63/8)*8 )
        {
            if( saturated )
            {
                int new_mquant = next_larger_quant( q_scale_type, mquant );
                if( new_mquant != mquant )
                {
                    mquant = new_mquant;
                    goto restart;
                }
                else
                {
                    return quant_non_intra(wsp, src, dst, 
                                           q_scale_type,
                                           satlim,
                                           nonsat_mquant);
                }
            }

            nzflag = (nzflag<<1) | !!flags;
            flags = 0;
            pdiv = pdivbase;
            pmul = pmulbase;
        }
			
    }
    emms();

    //nzflag = (nzflag<<1) | (!!flags);
    return nzflag;
}


static void iquant_non_intra_m1_mmx(struct QuantizerWorkSpace *wsp,
							 int16_t *src, int16_t *dst, int mquant )
{
	iquantize_non_intra_m1_mmx(src,dst,wsp->inter_q_tbl[mquant]);
}

static void iquant_non_intra_m2_mmx(struct QuantizerWorkSpace *wsp,
							 int16_t *src, int16_t *dst, int mquant )
{
	iquantize_non_intra_m2_mmx(src,dst,wsp->inter_q_tbl[mquant]);
}

static int quant_weight_coeff_x86_intra( struct QuantizerWorkSpace *wsp,
								  int16_t *blk )
{
	return quant_weight_coeff_sum_mmx( blk, wsp->i_intra_q_mat );
}
static int quant_weight_coeff_x86_inter( struct QuantizerWorkSpace *wsp,
								  int16_t *blk )
{
	return quant_weight_coeff_sum_mmx( blk, wsp->i_inter_q_mat );
}

#if 0
static int quant_non_intra_test(struct QuantizerWorkSpace *wsp,
                                int16_t *src, int16_t *dst,
                                int q_scale_type,
                                int satlim,
                                int *nonsat_mquant)
{
    int16_t d2[64*BLOCK_COUNT];
    int rv1,rv2,mq1,mq2;

    mq1=*nonsat_mquant;
    rv1=quant_non_intra(wsp,src,d2,q_scale_type,satlim,&mq1);
    rv2=quant_non_intra_mmx(wsp,src,dst,q_scale_type,satlim,nonsat_mquant);
    mq2=*nonsat_mquant;

    if( rv1!=rv2 )
        mjpeg_warn("quant_non_intra disparity: ret=%d vs %d",rv1,rv2);
    if( mq1!=mq2 )
        mjpeg_warn("quant_non_intra disparity: mq=%d vs %d",mq1,mq2);
    if( memcmp(d2,dst,64*BLOCK_COUNT*sizeof(int16_t)) ) {
        int i;

        mjpeg_warn("quant_non_intra disparity: dst differs");
        for( i=0; i<64*BLOCK_COUNT; i++ ) {
            if( d2[i]!=dst[i] )
                mjpeg_warn("\t[%d]: %4d vs %4d",i,dst[i],d2[i]);
        }
    }

    return rv2;
}
#endif

static int quant_non_intra_can_use_mmx(struct QuantizerWorkSpace *wsp)
{
    int i;

    for( i=0; i<64; i++ ) {
        int q=wsp->inter_q_mat[i];

        // if q<0, then bad things happen (mismatched sign)
        // if q==0, then divide by 0 happens
        // if q==1 or q==2, then 65536/q >= 32768 (negative sign when using signed multiply)
        // if q>292, then q*112 (which is the max quant scale) >=32768 (again, negative sign when using signed multiply)
        if( q<3 || q>292 )
            return 0;
    }
    return 1;
}

void init_x86_quantization( struct QuantizerCalls *qcalls,
                            struct QuantizerWorkSpace *wsp,
                            int mpeg1)
{
    int flags = cpu_accel();
    int d_quant_nonintra, d_weight_intra, d_weight_nonintra, d_iquant_intra;
    int d_iquant_nonintra;
    const char *opt_type1 = "", *opt_type2 = "";

    if  ((flags & ACCEL_X86_MMX) != 0 ) /* MMX CPU */
    {
	d_quant_nonintra = disable_simd("quant_nonintra");
	d_weight_intra = disable_simd("quant_weight_intra");
	d_weight_nonintra = disable_simd("quant_weight_nonintra");
	d_iquant_intra = disable_simd("iquant_intra");
	d_iquant_nonintra = disable_simd("iquant_nonintra");

        if  (d_quant_nonintra == 0)
        {
            if ((flags & ACCEL_X86_3DNOW) != 0)
            {
                opt_type1 = "3DNOW and";
                qcalls->pquant_non_intra = quant_non_intra_3dnow;
            }
            else if( quant_non_intra_can_use_mmx(wsp) )
            {
	        opt_type1 = "MMX and";
	        qcalls->pquant_non_intra = quant_non_intra_mmx;
            } else {
                mjpeg_warn("Non-intra quantization table out of range; disabling MMX");
            }
        }

        opt_type2 = "MMX";
        if (d_weight_intra == 0)
            qcalls->pquant_weight_coeff_intra = quant_weight_coeff_x86_intra;
        if (d_weight_nonintra == 0)
            qcalls->pquant_weight_coeff_inter = quant_weight_coeff_x86_inter;
        
        if (mpeg1)
        {
            if (d_iquant_nonintra == 0)
                qcalls->piquant_non_intra = iquant_non_intra_m1_mmx;
        }
        else
        {
            if (d_iquant_nonintra == 0)
                qcalls->piquant_non_intra = iquant_non_intra_m2_mmx;
        }
        
        if  (d_quant_nonintra)
            mjpeg_info(" Disabling quant_non_intra");
        if  (d_iquant_intra)
            mjpeg_info(" Disabling iquant_intra");
        if  (d_iquant_nonintra)
            mjpeg_info(" Disabling iquant_nonintra");
        if  (d_weight_intra)
            mjpeg_info(" Disabling quant_weight_intra");
        if (d_weight_nonintra)
            mjpeg_info(" Disabling quant_weight_nonintra");

	mjpeg_info( "SETTING %s %s for QUANTIZER!", opt_type1, opt_type2);
    }
}
