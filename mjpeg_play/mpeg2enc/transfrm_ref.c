/* transfrm_ref.c,   Low-level Architecture neutral quantization /
 * inverse quantization routines */

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
#include <math.h>
#include "mjpeg_types.h"
#include "syntaxparams.h"


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



/* add prediction and prediction error, saturate to 0...255 */

void add_pred(uint8_t *pred, uint8_t *cur,
			  int lx,
			  int16_t *blk)
{
	int i, j;

	for (j=0; j<8; j++)
	{
		for (i=0; i<8; i++)
		{
			int16_t rawsum = blk[i] + pred[i];
			cur[i] = (rawsum<0) ? 0 : ((rawsum>255) ? 255 : rawsum);
		}
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


