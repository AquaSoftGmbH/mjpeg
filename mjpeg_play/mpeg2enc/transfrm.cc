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
#include "cpu_accel.h"
#include "simd.h"

#ifdef HAVE_ALTIVEC
#include "../utils/altivec/altivec_transform.h"
#endif


extern "C" void init_fdct (void);
extern "C" void init_idct (void);


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
	init_fdct();
	init_idct();
}



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


