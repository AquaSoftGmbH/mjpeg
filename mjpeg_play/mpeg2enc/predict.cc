/* predict.c, motion compensated prediction                                 */

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
#include "global.h"
#include "cpu_accel.h"
#include "simd.h"

#ifdef HAVE_ALTIVEC
#include "../utils/altivec/altivec_predict.h"
#endif


/* private prototypes */

static void pred (
	uint8_t *src[], int sfield,
	uint8_t *dst[], int dfield,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);

extern "C" /* static */ void pred_comp (
	uint8_t *src, uint8_t *dst,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);
#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM) 
static void pred_comp_mmxe(
	uint8_t *src, uint8_t *dst,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);
static void pred_comp_mmx(
	uint8_t *src, uint8_t *dst,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);
#endif

static void calc_DMV(	const Picture &picture,int DMV[][2], 
						int *dmvector, int mvx, int mvy);

static void clearblock (const Picture &picture,
						uint8_t *cur[], int i0, int j0);

void init_predict(void);


/*
  Initialise prediction - currently purely selection of which
  versions of the various low level computation routines to use
  
  */


static void (*ppred_comp)(
	uint8_t *src, uint8_t *dst,
	int lx, int w, int h, int x, int y, int dx, int dy, int addflag);

void init_predict(void)
{
	int cpucap = cpu_accel();

	if( cpucap  == 0 )	/* No MMX/SSE etc support available */
	{
		ppred_comp = pred_comp;
	}

#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM) 
	else if(cpucap & ACCEL_X86_MMXEXT ) /* AMD MMX or SSE... */
	{
		mjpeg_info( "SETTING EXTENDED MMX for PREDICTION!");
		ppred_comp = pred_comp_mmxe;
	}
    else if(cpucap & ACCEL_X86_MMX ) /* Original MMX... */
	{
		mjpeg_info( "SETTING MMX for PREDICTION!");
		ppred_comp = pred_comp_mmx;
	}
#endif
#ifdef HAVE_ALTIVEC
    else
	{
#  if ALTIVEC_TEST_PREDICT
#    if defined(ALTIVEC_BENCHMARK)
	    mjpeg_info("SETTING AltiVec BENCHMARK for PREDICTION!");
#    elif defined(ALTIVEC_VERIFY)
	    mjpeg_info("SETTING AltiVec VERIFY for PREDICTION!");
#    endif
#  else
	    mjpeg_info("SETTING AltiVec for PREDICTION!");
#  endif

#  if ALTIVEC_TEST_FUNCTION(pred_comp)
	    ppred_comp = ALTIVEC_TEST_SUFFIX(pred_comp);
#  else
	    ppred_comp = ALTIVEC_SUFFIX(pred_comp);
#  endif
	}
#endif /* HAVE_ALTIVEC */
}


/* form prediction for a complete picture (frontend for predict_mb)
 *
 * mbi:  macroblock info
 *
 */

void predict(Picture *picture)
{
	vector<MacroBlock>::iterator mbi;

	/* loop through all macroblocks of the picture */
	for( mbi = picture->mbinfo.begin(); mbi < picture->mbinfo.end(); ++mbi)
		mbi->Predict();
}

/* form prediction for one macroblock
 *
 * lx:     frame width (identical to global var `width')
 *
 * Notes:
 * - when predicting a P type picture which is the second field of
 *   a frame, the same parity reference field is in oldref, while the
 *   opposite parity reference field is assumed to be in newref!
 * - intra macroblocks are modelled to have a constant prediction of 128
 *   for all pels; this results in a DC DCT coefficient symmetric to 0
 * - vectors for field prediction in frame pictures are in half pel frame
 *   coordinates (vertical component is twice the field value and always
 *   even)
 *
 * already covers dual prime (not yet used)
 */

void MacroBlock::Predict()
{
	const Picture &picture = ParentPicture();
	int bx = TopleftX();
	int by = TopleftY();
	uint8_t **oldref = picture.oldref;	// Forward prediction
	uint8_t **newref = picture.newref;	// Backward prediction
	uint8_t **cur = picture.pred;      // Frame to predict
	int lx = opt_phy_width;

	int addflag, currentfield;
	uint8_t **predframe;
	int DMV[2][2];

	if (mb_type&MB_INTRA)
	{
		clearblock(picture,cur,bx,by);
		return;
	}

	addflag = 0; /* first prediction is stored, second is added and averaged */

	if ((mb_type & MB_FORWARD) || (picture.pict_type==P_TYPE))
	{
		/* forward prediction, including zero MV in P pictures */

		if (picture.pict_struct==FRAME_PICTURE)
		{
			/* frame picture */

			if ((motion_type==MC_FRAME) || !(mb_type & MB_FORWARD))
			{
				/* frame-based prediction in frame picture */
				pred(
					 oldref,0,cur,0,
					 lx,16,16,bx,by,MV[0][0][0],MV[0][0][1],0);
			}
			else if (motion_type==MC_FIELD)
			{
				/* field-based prediction in frame picture
				 *
				 * note scaling of the vertical coordinates (by, MV[][0][1])
				 * from frame to field!
				 */

				/* top field prediction */
				pred(oldref,mv_field_sel[0][0],cur,0,
					 lx<<1,16,8,bx,by>>1,MV[0][0][0],MV[0][0][1]>>1,0);

				/* bottom field prediction */
				pred(oldref,mv_field_sel[1][0],cur,1,
					 lx<<1,16,8,bx,by>>1,MV[1][0][0],MV[1][0][1]>>1,0);
			}
			else if (motion_type==MC_DMV)
			{
				/* dual prime prediction */

				/* calculate derived motion vectors */
				calc_DMV(picture,DMV,dmvector,MV[0][0][0],MV[0][0][1]>>1);

				/* predict top field from top field */
				pred(oldref,0,cur,0,
					 lx<<1,16,8,bx,by>>1,MV[0][0][0],MV[0][0][1]>>1,0);

				/* predict bottom field from bottom field */
				pred(oldref,1,cur,1,
					 lx<<1,16,8,bx,by>>1,MV[0][0][0],MV[0][0][1]>>1,0);

				/* predict and add to top field from bottom field */
				pred(oldref,1,cur,0,
					 lx<<1,16,8,bx,by>>1,DMV[0][0],DMV[0][1],1);

				/* predict and add to bottom field from top field */
				pred(oldref,0,cur,1,
					 lx<<1,16,8,bx,by>>1,DMV[1][0],DMV[1][1],1);
			}
			else
			{
				/* invalid motion_type in frame picture */
				mjpeg_error_exit1("Internal: invalid motion_type");
			}
		}
		else /* TOP_FIELD or BOTTOM_FIELD */
		{
			/* field picture */

			currentfield = (picture.pict_struct==BOTTOM_FIELD);

			/* determine which frame to use for prediction */
			if ((picture.pict_type==P_TYPE) && picture.secondfield
				&& (currentfield!=mv_field_sel[0][0]))
				predframe = newref; /* same frame */
			else
				predframe = oldref; /* previous frame */

			if ((motion_type==MC_FIELD) || !(mb_type & MB_FORWARD))
			{
				/* field-based prediction in field picture */
				pred(predframe,mv_field_sel[0][0],cur,currentfield,
					 lx<<1,16,16,bx,by,MV[0][0][0],MV[0][0][1],0);
			}
			else if (motion_type==MC_16X8)
			{
				/* 16 x 8 motion compensation in field picture */

				/* upper half */
				pred(predframe,mv_field_sel[0][0],cur,currentfield,
					 lx<<1,16,8,bx,by,MV[0][0][0],MV[0][0][1],0);

				/* determine which frame to use for lower half prediction */
				if ((picture.pict_type==P_TYPE) && picture.secondfield
					&& (currentfield!=mv_field_sel[1][0]))
					predframe = newref; /* same frame */
				else
					predframe = oldref; /* previous frame */

				/* lower half */
				pred(predframe,mv_field_sel[1][0],cur,currentfield,
					 lx<<1,16,8,bx,by+8,MV[1][0][0],MV[1][0][1],0);
			}
			else if (motion_type==MC_DMV)
			{
				/* dual prime prediction */

				/* determine which frame to use for prediction */
				if (picture.secondfield)
					predframe = newref; /* same frame */
				else
					predframe = oldref; /* previous frame */

				/* calculate derived motion vectors */
				calc_DMV(picture,DMV,dmvector,MV[0][0][0],MV[0][0][1]);

				/* predict from field of same parity */
				pred(oldref,currentfield,cur,currentfield,
					 lx<<1,16,16,bx,by,MV[0][0][0],MV[0][0][1],0);

				/* predict from field of opposite parity */
				pred(predframe,!currentfield,cur,currentfield,
					 lx<<1,16,16,bx,by,DMV[0][0],DMV[0][1],1);
			}
			else
			{
				/* invalid motion_type in field picture */
				mjpeg_error_exit1("Internal: invalid motion_type");
			}
		}
		addflag = 1; /* next prediction (if any) will be averaged with this one */
	}

	if (mb_type & MB_BACKWARD)
	{
		/* backward prediction */

		if (picture.pict_struct==FRAME_PICTURE)
		{
			/* frame picture */

			if (motion_type==MC_FRAME)
			{
				/* frame-based prediction in frame picture */
				pred(newref,0,cur,0,
					 lx,16,16,bx,by,MV[0][1][0],MV[0][1][1],addflag);
			}
			else
			{
				/* field-based prediction in frame picture
				 *
				 * note scaling of the vertical coordinates (by, MV[][1][1])
				 * from frame to field!
				 */

				/* top field prediction */
				pred(newref,mv_field_sel[0][1],cur,0,
					 lx<<1,16,8,bx,by>>1,MV[0][1][0],MV[0][1][1]>>1,addflag);

				/* bottom field prediction */
				pred(newref,mv_field_sel[1][1],cur,1,
					 lx<<1,16,8,bx,by>>1,MV[1][1][0],MV[1][1][1]>>1,addflag);
			}
		}
		else /* TOP_FIELD or BOTTOM_FIELD */
		{
			/* field picture */

			currentfield = (picture.pict_struct==BOTTOM_FIELD);

			if (motion_type==MC_FIELD)
			{
				/* field-based prediction in field picture */
				pred(newref,mv_field_sel[0][1],cur,currentfield,
					 lx<<1,16,16,bx,by,MV[0][1][0],MV[0][1][1],addflag);
			}
			else if (motion_type==MC_16X8)
			{
				/* 16 x 8 motion compensation in field picture */

				/* upper half */
				pred(newref,mv_field_sel[0][1],cur,currentfield,
					 lx<<1,16,8,bx,by,MV[0][1][0],MV[0][1][1],addflag);

				/* lower half */
				pred(newref,mv_field_sel[1][1],cur,currentfield,
					 lx<<1,16,8,bx,by+8,MV[1][1][0],MV[1][1][1],addflag);
			}
			else
			{
				/* invalid motion_type in field picture */
				mjpeg_error_exit1("Internal: invalid motion_type");
			}
		}
	}
}

/* predict a rectangular block (all three components)
 *
 * src:     source frame (Y,U,V)
 * sfield:  source field select (0: frame or top field, 1: bottom field)
 * dst:     destination frame (Y,U,V)
 * dfield:  destination field select (0: frame or top field, 1: bottom field)
 *
 * the following values are in luminance picture (frame or field) dimensions
 * lx:      distance of vertically adjacent pels (selects frame or field pred.)
 * w,h:     width and height of block (only 16x16 or 16x8 are used)
 * x,y:     coordinates of destination block
 * dx,dy:   half pel motion vector
 * addflag: store or add (= average) prediction
 */

static void pred (	uint8_t *src[], int sfield,
					uint8_t *dst[], int dfield,
					int lx, int w, int h, int x, int y, 
					int dx, int dy, int addflag
	)
{
	int cc;

	for (cc=0; cc<3; cc++)
	{
		if (cc==1)
		{
			/* scale for color components */
			if (opt_chroma_format==CHROMA420)
			{
				/* vertical */
				h >>= 1; y >>= 1; dy /= 2;
			}
			if (opt_chroma_format!=CHROMA444)
			{
				/* horizontal */
				w >>= 1; x >>= 1; dx /= 2;
				lx >>= 1;
			}
		}
		(*ppred_comp)(	src[cc]+(sfield?lx>>1:0),dst[cc]+(dfield?lx>>1:0),
						lx,w,h,x,y,dx,dy,addflag);
	}
}

/* low level prediction routine
 *
 * src:     prediction source
 * dst:     prediction destination
 * lx:      line width (for both src and dst)
 * x,y:     destination coordinates
 * dx,dy:   half pel motion vector
 * w,h:     size of prediction block
 * addflag: store or add prediction
 *
 * There are also SIMD versions of this routine...
 */

extern "C" /* static */ void pred_comp(
	uint8_t *src,
	uint8_t *dst,
	int lx,
	int w, int h,
	int x, int y,
	int dx, int dy,
	int addflag)
{
	int xint, xh, yint, yh;
	int i, j;
	uint8_t *s, *d;

	/* half pel scaling */
	xint = dx>>1; /* integer part */
	xh = dx & 1;  /* half pel flag */
	yint = dy>>1;
	yh = dy & 1;

	/* origins */
	s = src + lx*(y+yint) + (x+xint); /* motion vector */
	d = dst + lx*y + x;

	if (!xh && !yh)
		if (addflag)
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (unsigned int)(d[i]+s[i]+1)>>1;
				s+= lx;
				d+= lx;
			}
		else
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = s[i];
				s+= lx;
				d+= lx;
			}
	else if (!xh && yh)
		if (addflag)
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (d[i] + ((unsigned int)(s[i]+s[i+lx]+1)>>1)+1)>>1;
				s+= lx;
				d+= lx;
			}
		else
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (unsigned int)(s[i]+s[i+lx]+1)>>1;
				s+= lx;
				d+= lx;
			}
	else if (xh && !yh)
		if (addflag)
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (d[i] + ((unsigned int)(s[i]+s[i+1]+1)>>1)+1)>>1;
				s+= lx;
				d+= lx;
			}
		else
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (unsigned int)(s[i]+s[i+1]+1)>>1;
				s+= lx;
				d+= lx;
			}
	else /* if (xh && yh) */
		if (addflag)
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (d[i] + ((unsigned int)(s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2)+1)>>1;
				s+= lx;
				d+= lx;
			}
		else
			for (j=0; j<h; j++)
			{
				for (i=0; i<w; i++)
					d[i] = (unsigned int)(s[i]+s[i+1]+s[i+lx]+s[i+lx+1]+2)>>2;
				s+= lx;
				d+= lx;
			}
}

#if defined(HAVE_ASM_MMX) && defined(HAVE_ASM_NASM) 
static void pred_comp_mmxe(
	uint8_t *src,
	uint8_t *dst,
	int lx,
	int w, int h,
	int x, int y,
	int dx, int dy,
	int addflag)
{
	int xint, xh, yint, yh;
	uint8_t *s, *d;
	
	/* half pel scaling */
	xint = dx>>1; /* integer part */
	xh = dx & 1;  /* half pel flag */
	yint = dy>>1;
	yh = dy & 1;

	/* origins */
	s = src + lx*(y+yint) + (x+xint); /* motion vector */
	d = dst + lx*y + x;

	if( xh )
	{
		if( yh ) 
			predcomp_11_mmxe(s,d,lx,w,h,addflag);
		else /* !yh */
			predcomp_10_mmxe(s,d,lx,w,h,addflag);
	}
	else /* !xh */
	{
		if( yh ) 
			predcomp_01_mmxe(s,d,lx,w,h,addflag);
		else /* !yh */
			predcomp_00_mmxe(s,d,lx,w,h,addflag);
	}
		
}

static void pred_comp_mmx(
	uint8_t *src,
	uint8_t *dst,
	int lx,
	int w, int h,
	int x, int y,
	int dx, int dy,
	int addflag)
{
	int xint, xh, yint, yh;
	uint8_t *s, *d;
	
	/* half pel scaling */
	xint = dx>>1; /* integer part */
	xh = dx & 1;  /* half pel flag */
	yint = dy>>1;
	yh = dy & 1;

	/* origins */
	s = src + lx*(y+yint) + (x+xint); /* motion vector */
	d = dst + lx*y + x;

	if( xh )
	{
		if( yh ) 
			predcomp_11_mmx(s,d,lx,w,h,addflag);
		else /* !yh */
			predcomp_10_mmx(s,d,lx,w,h,addflag);
	}
	else /* !xh */
	{
		if( yh ) 
			predcomp_01_mmx(s,d,lx,w,h,addflag);
		else /* !yh */
			predcomp_00_mmx(s,d,lx,w,h,addflag);
	}
		
}
#endif

/* calculate derived motion vectors (DMV) for dual prime prediction
 * dmvector[2]: differential motion vectors (-1,0,+1)
 * mvx,mvy: motion vector (for same parity)
 *
 * DMV[2][2]: derived motion vectors (for opposite parity)
 *
 * uses global variables pict_struct and topfirst
 *
 * Notes:
 *  - all vectors are in field coordinates (even for frame pictures)
 *
 */
static void calc_DMV(const Picture &picture,int DMV[][2], 
					 int *dmvector, int mvx, int mvy
)
{
  if (picture.pict_struct==FRAME_PICTURE)
  {
    if (picture.topfirst)
    {
      /* vector for prediction of top field from bottom field */
      DMV[0][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] - 1;

      /* vector for prediction of bottom field from top field */
      DMV[1][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
      DMV[1][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] + 1;
    }
    else
    {
      /* vector for prediction of top field from bottom field */
      DMV[0][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
      DMV[0][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] - 1;

      /* vector for prediction of bottom field from top field */
      DMV[1][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
      DMV[1][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] + 1;
    }
  }
  else
  {
    /* vector for prediction from field of opposite 'parity' */
    DMV[0][0] = ((mvx+(mvx>0))>>1) + dmvector[0];
    DMV[0][1] = ((mvy+(mvy>0))>>1) + dmvector[1];

    /* correct for vertical field shift */
    if (picture.pict_struct==TOP_FIELD)
      DMV[0][1]--;
    else
      DMV[0][1]++;
  }
}

static void clearblock(	const Picture &picture,
						uint8_t *cur[], int i0, int j0
	)
{
  int i, j, w, h;
  uint8_t *p;

  p = cur[0] 
	  + ((picture.pict_struct==BOTTOM_FIELD) ? opt_phy_width : 0) 
	  + i0 + opt_phy_width2*j0;

  for (j=0; j<16; j++)
  {
    for (i=0; i<16; i++)
      p[i] = 128;
    p+= opt_phy_width2;
  }

  w = h = 16;

  if (opt_chroma_format!=CHROMA444)
  {
    i0>>=1; w>>=1;
  }

  if (opt_chroma_format==CHROMA420)
  {
    j0>>=1; h>>=1;
  }

  p = cur[1] 
	  + ((picture.pict_struct==BOTTOM_FIELD) ? opt_phy_chrom_width : 0) 
	  + i0 + opt_phy_chrom_width2*j0;

  for (j=0; j<h; j++)
  {
    for (i=0; i<w; i++)
      p[i] = 128;
    p+= opt_phy_chrom_width2;
  }

  p = cur[2] 
	  + ((picture.pict_struct==BOTTOM_FIELD) ? opt_phy_chrom_width : 0) 
	  + i0 + opt_phy_chrom_width2*j0;

  for (j=0; j<h; j++)
  {
    for (i=0; i<w; i++)
      p[i] = 128;
    p+= opt_phy_chrom_width2;
  }
}
