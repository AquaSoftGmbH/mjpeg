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

#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM) 
int select_dct_type_mmx( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);

extern void fdct_mmx( int16_t * blk ) __asm__ ("fdct_mmx");
extern void idct_mmx( int16_t * blk ) __asm__ ("idct_mmx");

extern void add_pred_mmx (uint8_t *pred, uint8_t *cur,
				   int lx, int16_t *blk) __asm__ ("add_pred_mmx");
extern void sub_pred_mmx (uint8_t *pred, uint8_t *cur,
				   int lx, int16_t *blk) __asm__ ("sub_pred_mmx");
#endif

int select_dct_type( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);

extern void fdct( int16_t *blk );
extern void idct( int16_t *blk );



/* private prototypes*/
static void add_pred (uint8_t *pred, uint8_t *cur,
					  int lx, int16_t *blk);
static void sub_pred (uint8_t *pred, uint8_t *cur,
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
static int (*pselect_dct_type)( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);
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
		pselect_dct_type = select_dct_type_mmx;
	}
	else
#endif
	{
		pfdct = fdct;
		pidct = idct;
		padd_pred = add_pred;
		psub_pred = sub_pred;
		pselect_dct_type = select_dct_type;
	}
}


int select_dct_type( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb)
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
				cur_lum_mb[rowoffs+width+i] - pred_lum_mb[rowoffs+width+i];
			sumtop += toppix;
			sumsqtop += toppix*toppix;
			sumbot += botpix;
			sumsqbot += botpix*botpix;
			sumbottop += toppix*botpix;
		}
		rowoffs += (width<<1);
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
	movq_m2r( *((mmx_t*)(cur_lum_mb+width)), mm1 );
	movq_m2r( *((mmx_t*)(pred_lum_mb+width)), mm2 );
	
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

int select_dct_type_mmx( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb)
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
		rowoffs += (width<<1);
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

/* subtract prediction and transform prediction error */
void transform(
	pict_data_s *picture
	)
{
	int i, j, i1, j1, k, n, cc, offs, lx;
	uint8_t **cur = picture->curorg;
	uint8_t **pred = picture->pred;
	mbinfo_s *mbi = picture->mbinfo;
	int16_t (*blocks)[64] = picture->blocks;
	int introwstart = 0;
	k = 0;

	for (j=0; j<height2; j+=16)
	{
		for (i=0; i<width; i+=16)
		{
			mbi[k].dctblocks = &blocks[k*block_count];
			mbi[k].dct_type =
				(picture->frame_pred_dct || 
				 picture->pict_struct != FRAME_PICTURE 
				 ) 
				? 0	: (*pselect_dct_type)( &cur[0][introwstart+i], 
										   &pred[0][introwstart+i]);

			for (n=0; n<block_count; n++)
			{
				cc = (n<4) ? 0 : (n&1)+1; /* color component index */
				if (cc==0)
				{
					/* A.Stevens Jul 2000 Record dct blocks associated with macroblock */
					/* We'll use this for quantisation calculations                    */
					/* luminance */
					if ((picture->pict_struct==FRAME_PICTURE) && mbi[k].dct_type)
					{
						/* field DCT */
						offs = i + ((n&1)<<3) + width*(j+((n&2)>>1));
						lx = width<<1;
					}
					else
					{
						/* frame DCT */
						offs = i + ((n&1)<<3) + width2*(j+((n&2)<<2));
						lx = width2;
					}

					if (picture->pict_struct==BOTTOM_FIELD)
						offs += width;
				}
				else
				{
					/* chrominance */

					/* scale coordinates */
					i1 = (opt_chroma_format==CHROMA444) ? i : i>>1;
					j1 = (opt_chroma_format!=CHROMA420) ? j : j>>1;

					if ((picture->pict_struct==FRAME_PICTURE) && mbi[k].dct_type
						&& (opt_chroma_format!=CHROMA420))
					{
						/* field DCT */
						offs = i1 + (n&8) + chrom_width*(j1+((n&2)>>1));
						lx = chrom_width<<1;
					}
					else
					{
						/* frame DCT */
						offs = i1 + (n&8) + chrom_width2*(j1+((n&2)<<2));
						lx = chrom_width2;
					}

					if (picture->pict_struct==BOTTOM_FIELD)
						offs += chrom_width;
				}

				(*psub_pred)(pred[cc]+offs,cur[cc]+offs,lx,
							 blocks[k*block_count+n]);
				(*pfdct)(blocks[k*block_count+n]);
			}

			k++;
		}
		introwstart += 16*width;
	}
}


/* inverse transform prediction error and add prediction */
void itransform(pict_data_s *picture)
{
    mbinfo_s *mbi = picture->mbinfo;
	uint8_t **cur = picture->curref;
	uint8_t **pred = picture->pred;
	/* Its the quantised / inverse quantised blocks were interested in
	   for inverse transformation */
	int16_t (*blocks)[64] = picture->qblocks;
	int i, j, i1, j1, k, n, cc, offs, lx;

	k = 0;

	for (j=0; j<height2; j+=16)
		for (i=0; i<width; i+=16)
		{
			for (n=0; n<block_count; n++)
			{
				cc = (n<4) ? 0 : (n&1)+1; /* color component index */

				if (cc==0)
				{
					/* luminance */
					if ((picture->pict_struct==FRAME_PICTURE) && mbi[k].dct_type)
					{
						/* field DCT */
						offs = i + ((n&1)<<3) + width*(j+((n&2)>>1));
						lx = width<<1;
					}
					else
					{
						/* frame DCT */
						offs = i + ((n&1)<<3) + width2*(j+((n&2)<<2));
						lx = width2;
					}

					if (picture->pict_struct==BOTTOM_FIELD)
						offs += width;
				}
				else
				{
					/* chrominance */

					/* scale coordinates */
					i1 = (opt_chroma_format==CHROMA444) ? i : i>>1;
					j1 = (opt_chroma_format!=CHROMA420) ? j : j>>1;

					if ((picture->pict_struct==FRAME_PICTURE) && mbi[k].dct_type
						&& (opt_chroma_format!=CHROMA420))
					{
						/* field DCT */
						offs = i1 + (n&8) + chrom_width*(j1+((n&2)>>1));
						lx = chrom_width<<1;
					}
					else
					{
						/* frame DCT */
						offs = i1 + (n&8) + chrom_width2*(j1+((n&2)<<2));
						lx = chrom_width2;
					}

					if (picture->pict_struct==BOTTOM_FIELD)
						offs += chrom_width;
				}
				(*pidct)(blocks[k*block_count+n]);
				(*padd_pred)(pred[cc]+offs,cur[cc]+offs,lx,blocks[k*block_count+n]);
			}

			k++;
		}
}


/* add prediction and prediction error, saturate to 0...255 */
static void add_pred(pred,cur,lx,blk)
	uint8_t *pred, *cur;
	int lx;
	int16_t *blk;
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
static void sub_pred(pred,cur,lx,blk)
	uint8_t *pred, *cur;
	int lx;
	int16_t *blk;
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


