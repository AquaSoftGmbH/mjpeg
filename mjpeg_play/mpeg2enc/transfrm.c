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

#ifdef HAVE_X86CPU 
extern void fdct_mmx( int16_t * blk );
extern void idct_mmx( int16_t * blk );

void add_pred_mmx (uint8_t *pred, uint8_t *cur,
				   int lx, int16_t *blk);
void sub_pred_mmx (uint8_t *pred, uint8_t *cur,
				   int lx, int16_t *blk);
#endif

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

/*
  Initialise DCT transformation routines
  Currently just activates MMX routines if available
 */


void init_transform(void)
{
	int flags;
	flags = cpu_accel();

#ifdef HAVE_X86CPU 
	if( (flags & ACCEL_X86_MMX) ) /* MMX CPU */
	{
		mjpeg_info( "SETTING MMX for TRANSFORM!\n");
		pfdct = fdct_mmx;
		pidct = idct_mmx;
		padd_pred = add_pred_mmx;
		psub_pred = sub_pred_mmx;
	}
	else
#endif
	{
		pfdct = fdct;
		pidct = idct;
		padd_pred = add_pred;
		psub_pred = sub_pred;

	}
}


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

	k = 0;

	for (j=0; j<height2; j+=16)
		for (i=0; i<width; i+=16)
		{
			/* TODO: BUG this needs to be outside the n loop! */
			mbi[k].dctblocks = &blocks[k*block_count];

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
					i1 = (chroma_format==CHROMA444) ? i : i>>1;
					j1 = (chroma_format!=CHROMA420) ? j : j>>1;

					if ((picture->pict_struct==FRAME_PICTURE) && mbi[k].dct_type
						&& (chroma_format!=CHROMA420))
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
					i1 = (chroma_format==CHROMA444) ? i : i>>1;
					j1 = (chroma_format!=CHROMA420) ? j : j>>1;

					if ((picture->pict_struct==FRAME_PICTURE) && mbi[k].dct_type
						&& (chroma_format!=CHROMA420))
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
			cur[i] = clp[blk[i] + pred[i]];
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

/*
 * select between frame and field DCT
 *
 * preliminary version: based on inter-field correlation
 */

void dct_type_estimation(
	pict_data_s *picture
	)
{
	uint8_t *cur = picture->curorg[0];
	uint8_t *pred = picture->pred[0];
	struct mbinfo *mbi = picture->mbinfo;

	int16_t blk0[128], blk1[128];
	int i, j, i0, j0, k, offs, s0, s1, sq0, sq1, s01;
	double d, r;

	k = 0;

	for (j0=0; j0<height2; j0+=16)
		for (i0=0; i0<width; i0+=16)
		{
			if (picture->frame_pred_dct || picture->pict_struct!=FRAME_PICTURE)
				mbi[k].dct_type = 0;
			else
			{
				/* interlaced frame picture */
				/*
				 * calculate prediction error (cur-pred) for top (blk0)
				 * and bottom field (blk1)
				 */
				for (j=0; j<8; j++)
				{
					offs = width*((j<<1)+j0) + i0;
					for (i=0; i<16; i++)
					{
						blk0[16*j+i] = cur[offs] - pred[offs];
						blk1[16*j+i] = cur[offs+width] - pred[offs+width];
						offs++;
					}
				}
				/* correlate fields */
				s0=s1=sq0=sq1=s01=0;

				for (i=0; i<128; i++)
				{
					s0+= blk0[i];
					sq0+= blk0[i]*blk0[i];
					s1+= blk1[i];
					sq1+= blk1[i]*blk1[i];
					s01+= blk0[i]*blk1[i];
				}

				d = (sq0-(s0*s0)/128.0)*(sq1-(s1*s1)/128.0);

				if (d>0.0)
				{
					r = (s01-(s0*s1)/128.0)/sqrt(d);
					if (r>0.5)
						mbi[k].dct_type = 0; /* frame DCT */
					else
						mbi[k].dct_type = 1; /* field DCT */
				}
				else
					mbi[k].dct_type = 1; /* field DCT */
			}
			k++;
		}
}
