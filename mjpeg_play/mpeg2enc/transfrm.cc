/* transfrm.c,  forward / inverse transformation                            */

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
#include <math.h>
#include "global.h"
#include "attributes.h"
#include "mmx.h"
#include "cpu_accel.h"

#ifdef HAVE_ALTIVEC
#include "../utils/altivec/altivec_transform.h"
#endif

#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM) 
int field_dct_best_mmx( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);

extern void fdct_mmx( int16_t * blk ) __asm__ ("fdct_mmx");
extern void idct_mmx( int16_t * blk ) __asm__ ("idct_mmx");

extern void add_pred_mmx (uint8_t *pred, uint8_t *cur,
				   int lx, int16_t *blk) __asm__ ("add_pred_mmx");
extern void sub_pred_mmx (uint8_t *pred, uint8_t *cur,
				   int lx, int16_t *blk) __asm__ ("sub_pred_mmx");
#endif

int field_dct_best( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);

extern void fdct( int16_t *blk );
extern void idct( int16_t *blk );


/* private prototypes*/
/* static */ void add_pred (uint8_t *pred, uint8_t *cur,
					  int lx, int16_t *blk);
/* static */ void sub_pred (uint8_t *pred, uint8_t *cur,
					  int lx, int16_t *blk);

/*
  Pointers to version of transform and prediction manipulation
  routines to be used..
 */

static void (*pfdct)( int16_t * blk );
static void (*pidct)( int16_t * blk );
static void (*padd_pred) (uint8_t *pred, uint8_t *cur,
						  int lx, int16_t *blk);
static void (*psub_pred) (uint8_t *pred, uint8_t *cur,
						  int lx, int16_t *blk);
static int (*pfield_dct_best)( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);
/*
  Initialise DCT transformation routines
  Currently just activates MMX routines if available
 */


void init_transform(void)
{
	int flags;
	flags = cpu_accel();

#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM) 
	if( (flags & ACCEL_X86_MMX) ) /* MMX CPU */
	{
		mjpeg_info( "SETTING MMX for TRANSFORM!");
		pfdct = fdct_mmx;
		pidct = idct_mmx;
		padd_pred = add_pred_mmx;
		psub_pred = sub_pred_mmx;
		pfield_dct_best = field_dct_best_mmx;
	}
	else
#endif
	{
		pfdct = fdct;
		pidct = idct;
		padd_pred = add_pred;
		psub_pred = sub_pred;
		pfield_dct_best = field_dct_best;
	}

#ifdef HAVE_ALTIVEC
	if (flags > 0)
	{
#if ALTIVEC_TEST_TRANSFORM
#  if defined(ALTIVEC_BENCHMARK)
	    mjpeg_info("SETTING AltiVec BENCHMARK for TRANSFORM!");
#  elif defined(ALTIVEC_VERIFY)
	    mjpeg_info("SETTING AltiVec VERIFY for TRANSFORM!");
#  endif
#else
	    mjpeg_info("SETTING AltiVec for TRANSFORM!");
#endif

#if ALTIVEC_TEST_FUNCTION(fdct)
	    pfdct = ALTIVEC_TEST_SUFFIX(fdct);
#else
	    pfdct = ALTIVEC_SUFFIX(fdct);
#endif

#if ALTIVEC_TEST_FUNCTION(idct)
	    pidct = ALTIVEC_TEST_SUFFIX(idct);
#else
	    pidct = ALTIVEC_SUFFIX(idct);
#endif

#if ALTIVEC_TEST_FUNCTION(add_pred)
	    padd_pred = ALTIVEC_TEST_SUFFIX(add_pred);
#else
	    padd_pred = ALTIVEC_SUFFIX(add_pred);
#endif

#if ALTIVEC_TEST_FUNCTION(sub_pred)
	    psub_pred = ALTIVEC_TEST_SUFFIX(sub_pred);
#else
	    psub_pred = ALTIVEC_SUFFIX(sub_pred);
#endif
	}
#endif /* HAVE_ALTIVEC */
}


int field_dct_best( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb)
{
	/*
	 * calculate prediction error (cur-pred) for top (blk0)
	 * and bottom field (blk1)
	 */
	double r,d;
	int rowoffs = 0;
	int sumtop, sumbot, sumsqtop, sumsqbot, sumbottop;
	int j,i;
	int topvar, botvar;
	sumtop = sumsqtop = sumbot = sumsqbot = sumbottop = 0;
	for (j=0; j<8; j++)
	{
		for (i=0; i<16; i++)
		{
			register int toppix = 
				cur_lum_mb[rowoffs+i] - pred_lum_mb[rowoffs+i];
			register int botpix = 
				cur_lum_mb[rowoffs+opt_phy_width+i] 
				- pred_lum_mb[rowoffs+opt_phy_width+i];
			sumtop += toppix;
			sumsqtop += toppix*toppix;
			sumbot += botpix;
			sumsqbot += botpix*botpix;
			sumbottop += toppix*botpix;
		}
		rowoffs += (opt_phy_width<<1);
	}

	/* Calculate Variances top and bottom.  If they're of similar
	 sign estimate correlation if its good use frame DCT otherwise
	 use field.
	*/
	r = 0.0;
	topvar = sumsqtop-sumtop*sumtop/128;
	botvar = sumsqbot-sumbot*sumbot/128;
	if (!((topvar>0) ^ (botvar>0)))
	{
		d = ((double) topvar) * ((double)botvar);
		r = (sumbottop-(sumtop*sumbot)/128);
		if (r>0.5*sqrt(d))
			return 0; /* frame DCT */
		else
			return 1; /* field DCT */
	}
        return 1; /* field DCT */
}
#if defined(HAVE_ASM_MMX)
static __inline__ void
mmx_sum_4_word_accs( mmx_t *accs, int32_t *res )
{
	movq_m2r( *accs, mm1 );
	movq_r2r( mm1, mm3 );
	movq_r2r( mm1, mm2 );
	/* Generate sign extensions for mm1 words! */
	psraw_i2r( 15, mm3 );
	punpcklwd_r2r( mm3, mm1 );
	punpckhwd_r2r( mm3, mm2 );
	paddd_r2r( mm1, mm2 );
	movq_r2r( mm2, mm3);
	psrlq_i2r( 32, mm2);
	paddd_r2r( mm2, mm3);
	movd_r2m( mm3, *res );
}


static __inline__ void
sum_sumsq_8bytes( uint8_t *cur_lum_mb, 
				  uint8_t *pred_lum_mb,
				  mmx_t *sumtop_accs,
				  mmx_t *sumbot_accs,
				  mmx_t *sumsqtop_accs,
				  mmx_t *sumsqbot_accs,
				  mmx_t *sumxprod_accs
	)
{
	pxor_r2r(mm0,mm0);

	/* Load pixels from top field into mm1.w,mm2.w
	 */
	movq_m2r( *((mmx_t*)cur_lum_mb), mm1 );
	movq_m2r( *((mmx_t*)pred_lum_mb), mm2 );
	
	/* mm3 := mm1 mm4 := mm2
	   mm1.w[0..3] := mm1.b[0..3]-mm2.b[0..3]
	*/ 
	
	movq_r2r( mm1, mm3 );
	punpcklbw_r2r( mm0, mm1 );
	movq_r2r( mm2, mm4 );
	punpcklbw_r2r( mm0, mm2 );
	psubw_r2r( mm2, mm1 );
	
	/* mm3.w[0..3] := mm3.b[4..7]-mm4.b[4..7]
	 */
	punpckhbw_r2r( mm0, mm3 );
	punpckhbw_r2r( mm0, mm4 );
	psubw_r2r( mm4, mm3 );

	/* sumtop_accs->w[0..3] += mm1.w[0..3];
	   sumtop_accs->w[0..3] += mm3.w[0..3];
	   mm6 = mm1; mm7 = mm3;
	*/
	movq_m2r( *sumtop_accs, mm5 );
	paddw_r2r( mm1, mm5 );
	paddw_r2r( mm3, mm5 );
	movq_r2r( mm1, mm6 );
	movq_r2r( mm3, mm7 );
	movq_r2m( mm5, *sumtop_accs );

	/* 
	   *sumsq_top_acc += mm1.w[0..3] * mm1.w[0..3];
	   *sumsq_top_acc += mm3.w[0..3] * mm3.w[0..3];
	*/
	pmaddwd_r2r( mm1, mm1 );
	movq_m2r( *sumsqtop_accs, mm5 );
	pmaddwd_r2r( mm3, mm3 );
	paddd_r2r( mm1, mm5 );
	paddd_r2r( mm3, mm5 );
	movq_r2m( mm5, *sumsqtop_accs );
	

	/* Load pixels from bot field into mm1.w,mm2.w
	 */
	movq_m2r( *((mmx_t*)(cur_lum_mb+opt_phy_width)), mm1 );
	movq_m2r( *((mmx_t*)(pred_lum_mb+opt_phy_width)), mm2 );
	
	/* mm2 := mm1 mm4 := mm2
	   mm1.w[0..3] := mm1.b[0..3]-mm2.b[0..3]
	*/ 
	
	movq_r2r( mm1, mm3 );
	punpcklbw_r2r( mm0, mm1 );
	movq_r2r( mm2, mm4 );
	punpcklbw_r2r( mm0, mm2 );
	psubw_r2r( mm2, mm1 );
	
	/* mm3.w[0..3] := mm3.b[4..7]-mm4.b[4..7]
	 */
	punpckhbw_r2r( mm0, mm3 );
	punpckhbw_r2r( mm0, mm4 );
	psubw_r2r( mm4, mm3 );

	/* 
	   sumbot_accs->w[0..3] += mm1.w[0..3];
	   sumbot_accs->w[0..3] += mm3.w[0..3];
	   mm2 := mm1; mm4 := mm3;
	*/
	movq_m2r( *sumbot_accs, mm5 );
	paddw_r2r( mm1, mm5 );
	movq_r2r( mm1, mm2 );
	paddw_r2r( mm3, mm5 );
	movq_r2r( mm3, mm4 );
	movq_r2m( mm5, *sumbot_accs );

	/* 
	   *sumsqbot_acc += mm1.w[0..3] * mm1.w[0..3];
	   *sumsqbot_acc += mm3.w[0..3] * mm3.w[0..3];
	*/
	pmaddwd_r2r( mm1, mm1 );
	movq_m2r( *sumsqbot_accs, mm5 );
	pmaddwd_r2r( mm3, mm3 );
	paddd_r2r( mm1, mm5 );
	paddd_r2r( mm3, mm5 );
	movq_r2m( mm5, *sumsqbot_accs );
	
	
	/* Accumulate cross-product 
	 *sum_xprod_acc += mm1.w[0..3] * mm6[0..3];
	 *sum_xprod_acc += mm3.w[0..3] * mm7[0..3];
	 */

	movq_m2r( *sumxprod_accs, mm5 );
	pmaddwd_r2r( mm6, mm2);
	pmaddwd_r2r( mm7, mm4);
	paddd_r2r( mm2, mm5 );
	paddd_r2r( mm4, mm5 );
	movq_r2m( mm5, *sumxprod_accs );
	emms();
}

int field_dct_best_mmx( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb)
{
	/*
	 * calculate prediction error (cur-pred) for top (blk0)
	 * and bottom field (blk1)
	 */
	double r,d;
	int rowoffs = 0;
	int sumtop, sumbot, sumsqtop, sumsqbot, sumbottop;
	int j;
	int dct_type;
	int topvar, botvar;
	mmx_t sumtop_accs, sumbot_accs;
	mmx_t sumsqtop_accs, sumsqbot_accs, sumxprod_accs;
	int32_t sumtop_acc, sumbot_acc;
	int32_t sumsqtop_acc, sumsqbot_acc, sumxprod_acc;

	pxor_r2r(mm0,mm0);
	movq_r2m( mm0, *(&sumtop_accs) );
	movq_r2m( mm0, *(&sumbot_accs) );
	movq_r2m( mm0, *(&sumsqtop_accs) );
	movq_r2m( mm0, *(&sumsqbot_accs) );
	movq_r2m( mm0, *(&sumxprod_accs) );
	
	sumtop = sumsqtop = sumbot = sumsqbot = sumbottop = 0;
	sumtop_acc = sumbot_acc = sumsqtop_acc = sumsqbot_acc = sumxprod_acc = 0; 
	for (j=0; j<8; j++)
	{
#ifdef ORIGINAL_CODE
		for (i=0; i<16; i++)
		{
			register int toppix = 
				cur_lum_mb[rowoffs+i] - pred_lum_mb[rowoffs+i];
			register int botpix = 
				cur_lum_mb[rowoffs+width+i] - pred_lum_mb[rowoffs+width+i];
			sumtop += toppix;
			sumsqtop += toppix*toppix;
			sumbot += botpix;
			sumsqbot += botpix*botpix;
			sumbottop += toppix*botpix;
		}
#endif
		sum_sumsq_8bytes( &cur_lum_mb[rowoffs], &pred_lum_mb[rowoffs],
						  &sumtop_accs, &sumbot_accs,
						  &sumsqtop_accs, &sumsqbot_accs, &sumxprod_accs
						  );
		sum_sumsq_8bytes( &cur_lum_mb[rowoffs+8], &pred_lum_mb[rowoffs+8],
						  &sumtop_accs, &sumbot_accs,
						  &sumsqtop_accs, &sumsqbot_accs, &sumxprod_accs );
		rowoffs += (opt_phy_width<<1);
	}

	mmx_sum_4_word_accs( &sumtop_accs, &sumtop );
	mmx_sum_4_word_accs( &sumbot_accs, &sumbot );
	emms();
	sumsqtop = sumsqtop_accs.d[0] + sumsqtop_accs.d[1];
	sumsqbot = sumsqbot_accs.d[0] + sumsqbot_accs.d[1];
	sumbottop = sumxprod_accs.d[0] + sumxprod_accs.d[1];

	/* Calculate Variances top and bottom.  If they're of similar
	 sign estimate correlation if its good use frame DCT otherwise
	 use field.
	*/
	r = 0.0;
	topvar = sumsqtop-sumtop*sumtop/128;
	botvar = sumsqbot-sumbot*sumbot/128;
	if ( !((topvar <= 0) ^ (botvar <= 0)) )
	{
		d = ((double) topvar) * ((double)botvar);
		r = (sumbottop-(sumtop*sumbot)/128);
		if (r>0.5*sqrt(d))
			return 0; /* frame DCT */
		else
			return 1; /* field DCT */
	}
	else
		return 1; /* field DCT */

	return dct_type;
}
#endif

void MacroBlock::Transform()
{
	uint8_t **cur = picture->curorg;
	uint8_t **pred = picture->pred;
	// assert( dctblocks == &blocks[k*block_count]);
	int i = TopleftX();
	int j = TopleftY();
	int blocktopleft = j*opt_phy_width+i;
	field_dct =
		! picture->frame_pred_dct 
		&& picture->pict_struct == FRAME_PICTURE
		&& (*pfield_dct_best)( &cur[0][blocktopleft], 
							   &pred[0][blocktopleft]);
	int i1, j1, n, cc, offs, lx;

	for (n=0; n<block_count; n++)
	{
		cc = (n<4) ? 0 : (n&1)+1; /* color component index */
		if (cc==0)
		{
			/* A.Stevens Jul 2000 Record dct blocks associated
			 * with macroblock We'll use this for quantisation
			 * calculations  */
			/* luminance */
			if ((picture->pict_struct==FRAME_PICTURE) && field_dct)
			{
				/* field DCT */
				offs = i + ((n&1)<<3) + opt_phy_width*(j+((n&2)>>1));
				lx =  opt_phy_width<<1;
			}
			else
			{
				/* frame DCT */
				offs = i + ((n&1)<<3) +  opt_phy_width2*(j+((n&2)<<2));
				lx =  opt_phy_width2;
			}

			if (picture->pict_struct==BOTTOM_FIELD)
				offs +=  opt_phy_width;
		}
		else
		{
			/* chrominance */

			/* scale coordinates */
			i1 = (opt_chroma_format==CHROMA444) ? i : i>>1;
			j1 = (opt_chroma_format!=CHROMA420) ? j : j>>1;

			if ((picture->pict_struct==FRAME_PICTURE) && field_dct
				&& (opt_chroma_format!=CHROMA420))
			{
				/* field DCT */
				offs = i1 + (n&8) +  opt_phy_chrom_width*(j1+((n&2)>>1));
				lx =  opt_phy_chrom_width<<1;
			}
			else
			{
				/* frame DCT */
				offs = i1 + (n&8) +  opt_phy_chrom_width2*(j1+((n&2)<<2));
				lx =  opt_phy_chrom_width2;
			}

			if (picture->pict_struct==BOTTOM_FIELD)
				offs +=  opt_phy_chrom_width;
		}

		(*psub_pred)(pred[cc]+offs,cur[cc]+offs,lx, dctblocks[n]);
		(*pfdct)(dctblocks[n]);
	}
		
}
/* subtract prediction and transform prediction error */
void transform(	Picture *picture )
{
	vector<MacroBlock>::iterator mbi;

	for( mbi = picture->mbinfo.begin(); mbi < picture->mbinfo.end(); ++mbi)
	{
		mbi->Transform();
	}
}

void MacroBlock::ITransform()
{
	uint8_t **cur = picture->curref;
	uint8_t **pred = picture->pred;

	int i1, j1, n, cc, offs, lx;
	int i = TopleftX();
	int j = TopleftY();
			
	for (n=0; n<block_count; n++)
	{
		cc = (n<4) ? 0 : (n&1)+1; /* color component index */
			
		if (cc==0)
		{
			/* luminance */
			if ((picture->pict_struct==FRAME_PICTURE) && field_dct)
			{
				/* field DCT */
				offs = i + ((n&1)<<3) + opt_phy_width*(j+((n&2)>>1));
				lx = opt_phy_width<<1;
			}
			else
			{
				/* frame DCT */
				offs = i + ((n&1)<<3) + opt_phy_width2*(j+((n&2)<<2));
				lx = opt_phy_width2;
			}

			if (picture->pict_struct==BOTTOM_FIELD)
				offs +=  opt_phy_width;
		}
		else
		{
			/* chrominance */

			/* scale coordinates */
			i1 = (opt_chroma_format==CHROMA444) ? i : i>>1;
			j1 = (opt_chroma_format!=CHROMA420) ? j : j>>1;

			if ((picture->pict_struct==FRAME_PICTURE) && field_dct
				&& (opt_chroma_format!=CHROMA420))
			{
				/* field DCT */
				offs = i1 + (n&8) + opt_phy_chrom_width*(j1+((n&2)>>1));
				lx = opt_phy_chrom_width<<1;
			}
			else
			{
				/* frame DCT */
				offs = i1 + (n&8) + opt_phy_chrom_width2*(j1+((n&2)<<2));
				lx = opt_phy_chrom_width2;
			}

			if (picture->pict_struct==BOTTOM_FIELD)
				offs +=  opt_phy_chrom_width;
		}
		(*pidct)(qdctblocks[n]);
		(*padd_pred)(pred[cc]+offs,cur[cc]+offs,lx,qdctblocks[n]);
	}
}

/* inverse transform prediction error and add prediction */
void itransform(Picture *picture)
{
    vector<MacroBlock>::iterator mbi = picture->mbinfo.begin();
	for( mbi = picture->mbinfo.begin(); mbi < picture->mbinfo.end(); ++mbi)
	{
		mbi->ITransform();
	}
}


/* add prediction and prediction error, saturate to 0...255 */

void add_pred(uint8_t *pred, uint8_t *cur,
			  int lx,
			  int16_t *blk)
{
	int i, j;

	for (j=0; j<8; j++)
	{
		for (i=0; i<8; i++)
			cur[i] = clp_0_255[blk[i] + pred[i]];
		blk+= 8;
		cur+= lx;
		pred+= lx;
	}
}


/* subtract prediction from block data */
/* static */
void sub_pred(uint8_t *pred, uint8_t *cur, int lx, 	int16_t *blk)
{
	int i, j;

	for (j=0; j<8; j++)
	{
		for (i=0; i<8; i++)
			blk[i] = cur[i] - pred[i];
		blk+= 8;
		cur+= lx;
		pred+= lx;
	}
}


