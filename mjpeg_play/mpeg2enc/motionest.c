/* motion.c, motion estimation                                              */

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
 * */

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
#include <limits.h>
#include "global.h"
#include "math.h"
#include "cpu_accel.h"
#include "simd.h"
#include "fastintfns.h"
#include "motionsearch.h"

/* Macro-block Motion estimation results record */

typedef struct _blockcrd {
	int16_t x;
	int16_t y;
} blockxy;

struct mb_motion
{
	blockxy pos;        // Half-pel co-ordinates of source block
	int sad;			// Sum of absolute difference
	int var;
	uint8_t *blk ;		// Source block data (in luminace data array)
	int hx, hy;			// Half-pel offsets
	int fieldsel;		// 0 = top 1 = bottom
	int fieldoff;       // Offset from start of frame data to first line
						// of field.	(top = 0, bottom = width );
};

typedef struct mb_motion mb_motion_s;

struct subsampled_mb
{
	uint8_t *mb;		// One pel
	uint8_t *fmb;		// Two-pel subsampled
	uint8_t *qmb;		// Four-pel subsampled
	uint8_t *umb; 		// U compoenent one-pel
	uint8_t *vmb;       // V component  one-pel
};

typedef struct subsampled_mb subsampled_mb_s;





/*
  Main field and frame based motion estimation entry points.
*/

static void frame_ME (pict_data_s *picture,
								  int mboffset,
								  int i, int j, struct mbinfo *mbi);

static void field_ME (pict_data_s *picture,
								  int mboffset,
								  int i, int j, 
								  struct mbinfo *mbi, 
								  int secondfield, 
								  int ipflag);

static void frame_estimate (
	uint8_t *org,
	 uint8_t *ref, 
	 subsampled_mb_s *topssmb,
	 subsampled_mb_s *botssmb,
	 int i, int j,
	 int sx, int sy, 
	  mb_motion_s *bestfr,
	  mb_motion_s *besttop,
	  mb_motion_s *bestbot,
	 int imins[2][2], int jmins[2][2]);

static void field_estimate (pict_data_s *picture,
							uint8_t *toporg,
							uint8_t *topref, 
							uint8_t *botorg, 
							uint8_t *botref,
							subsampled_mb_s *ssmb,
							int i, int j, int sx, int sy, int ipflag,
							mb_motion_s *bestfr,
							mb_motion_s *best8u,
							mb_motion_s *best8l,
							mb_motion_s *bestsp);

static void dpframe_estimate (
	pict_data_s *picture,
	uint8_t *ref,
	subsampled_mb_s *ssmb,
	int i, int j, int iminf[2][2], int jminf[2][2],
	mb_motion_s *dpbest,
	int *imindmvp, int *jmindmvp, 
	int *vmcp);

static void dpfield_estimate (
	pict_data_s *picture,
	uint8_t *topref,
	uint8_t *botref, 
	uint8_t *mb,
	int i, int j, 
	mb_motion_s *bestsp_mc,
	mb_motion_s *dpbest,
	int *vmcp);

static void mb_me_search (
	uint8_t *org, uint8_t *ref,
	subsampled_mb_s *ssblk,
	int lx, int i0, int j0, 
	int sx, int sy, int h, 
	int xmax, int ymax,
	mb_motion_s *motion );




/*
 *  Initialise motion estimation - currently only selection of which
 * versions of the various low level computation routines to use
 * 
 */

void init_motion()
{
	init_motion_search();
}


/* 
 *  Compute subsampled images for fast motion compensation search
 *  N.b. Sub-sampling works correctly if we treat interlaced images
 *  as two half-height images side-by-side.
 */

void motion_subsampled_lum( pict_data_s *picture )
{
	int linestride;

	/* In an interlaced field the "next" line is 2 width's down rather
	   than 1 width down  */

	if (!opt_fieldpic)
	{
		linestride = width;
	}
	else
	{
		linestride = 2*width;
	}

	subsample_image( picture->curorg[0], 
					 linestride,
					 picture->curorg[0]+fsubsample_offset, 
					 picture->curorg[0]+qsubsample_offset );
}

/*
 * motion estimation top level entry point
 *
 * Fills in the macro-block info of the specified picture with 
 * best-guess motion estimates.  Seperate frame and field based MC
 * routines are used depending on picture (and hence stream) structure.
 */


void motion_estimation(
	pict_data_s *picture
	)
{
	mbinfo_s *mbi = picture->mbinfo;
	int i, j;
	int mb_row_incr;			/* Offset increment to go down 1 row of mb's */
	int mb_row_start;
	mb_row_start = 0;

	if (picture->pict_struct==FRAME_PICTURE)
	{			
		mb_row_incr = 16*width;
		/* loop through all macroblocks of a frame picture */
		for (j=0; j<height2; j+=16)
		{
			for (i=0; i<width; i+=16)
			{
				frame_ME(picture,  mb_row_start, i,j, mbi);
				mbi++;
			}
			mb_row_start += mb_row_incr;
		}
	}
	else
	{		
		mb_row_incr = (16 * 2) * width;
		/* loop through all macroblocks of a field picture */
		for (j=0; j<height2; j+=16)
		{
			for (i=0; i<width; i+=16)
			{
				field_ME(picture, mb_row_start, i,j,
						 mbi,picture->secondfield,picture->ipflag);
				mbi++;
			}
			mb_row_start += mb_row_incr;
		}
	}
}

/*
 * Compute the variance of the residual of uni-directionally motion
 * compensated block.
 *
 */

static __inline__ int unidir_pred_var( const mb_motion_s *motion,
							uint8_t *mb,  
							int lx, 
							int h)
{
	return (*pdist2)(motion->blk, mb, lx, motion->hx, motion->hy, h);
}


/*
 * Compute the variance of the residual of bi-directionally motion
 * compensated block.
 */

static __inline__ int bidir_pred_var( const mb_motion_s *motion_f, 
									  const mb_motion_s *motion_b,
									  uint8_t *mb,  
									  int lx, int h)
{
	return (*pbdist2)( motion_f->blk, motion_b->blk,
					   mb, lx, 
					   motion_f->hx, motion_f->hy,
					   motion_b->hx, motion_b->hy,
					   h);
}


/* unidir_var_sum
 *
 * Compute the combined variance of luminance and
 * chrominance information for a particular non-intra macro block
 * after unidirectional motion estimation...
 *
 *  Note: results are scaled to give chrominance equal weight to
 *  chrominance.  The variance of the luminance portion is computed
 *  at the time the motion estimation is computed.
 *
 *  TODO: Perhaps we should compute the whole thing in mb_mc_search
 *  not seperate it.  However, that would involve a lot of fiddling
 *  with field_* and until its thoroughly debugged and tested I think
 *  I'll leave that alone. Furthermore, it is unclear if its really
 *  worth * doing these computations for B *and* P frames.
 *
 *  TODO: BUG: ONLY works for 420 video...
 *  
 */

static int unidir_var_sum( mb_motion_s *lum_mc, 
						   uint8_t **ref, 
						   subsampled_mb_s *ssblk,
						   int lx, int h )
{
	int uvlx = (lx>>1);
	int uvh = (h>>1);
	/* N.b. MC co-ordinates are computed in half-pel units! */
	int cblkoffset = (lum_mc->fieldoff>>1) +
		(lum_mc->pos.x>>2) + (lum_mc->pos.y>>2)*uvlx;
	
	return 	lum_mc->var +
		((*pdist2_22)( ref[1] + cblkoffset, ssblk->umb, uvlx, uvh) +
		 (*pdist2_22)( ref[2] + cblkoffset, ssblk->vmb, uvlx, uvh));
}

/*
 *  bidir_var_sum
 *  Compute the combined variance of luminance and chrominance information
 *  for a particular non-intra macro block after bidirectional
 *  motion estimation...  
 *
 *  Note: results are scaled to give chrominance equal weight to
 *  chrominance.  The variance of the luminance portion is computed
 *  at the time the motion estimation is computed.
 *
 *  Note: results scaled to give chrominance equal weight to chrominance.
 * 
 *  TODO: BUG: ONLY works for 420 video...
 *
 *  NOTE: Currently unused but may be required if it turns out that taking
 *  chrominance into account in B frames is needed.
 *
 */

static int bidir_var_sum( mb_motion_s *lum_mc_f, 
						  mb_motion_s *lum_mc_b, 
						  uint8_t **ref_f, 
						  uint8_t **ref_b,
						  subsampled_mb_s *ssblk,
						  int lx, int h )
{
	int uvlx = (lx>>1);
	int uvh = (h>>1);
	/* N.b. MC co-ordinates are computed in half-pel units! */
	int cblkoffset_f = (lum_mc_f->fieldoff>>1) + 
		(lum_mc_f->pos.x>>2) + (lum_mc_f->pos.y>>2)*uvlx;
	int cblkoffset_b = (lum_mc_b->fieldoff>>1) + 
		(lum_mc_b->pos.x>>2) + (lum_mc_b->pos.y>>2)*uvlx;
	
	return 	(
		(*pbdist2)( lum_mc_f->blk, lum_mc_b->blk,
					ssblk->mb, lx, 
					lum_mc_f->hx, lum_mc_f->hy,
					lum_mc_b->hx, lum_mc_b->hy,
					h) +
		(*pbdist2_22)( ref_f[1] + cblkoffset_f, ref_b[1] + cblkoffset_b,
					   ssblk->umb, uvlx, uvh ) +
		(*pbdist2_22)( ref_f[2] + cblkoffset_f, ref_b[2] + cblkoffset_b,
					   ssblk->vmb, uvlx, uvh ));

}

/*
 * Sum of chrominance variance of a block.
 */

static __inline__ int chrom_var_sum( subsampled_mb_s *ssblk, int h, int lx )
{
	return ((*pvariance)(ssblk->umb,(h>>1),(lx>>1)) + 
			(*pvariance)(ssblk->vmb,(h>>1),(lx>>1))) * 2;
}



/*
 * Compute SAD for bi-directionally motion compensated blocks...
 */

static __inline__ int bidir_pred_sad( const mb_motion_s *motion_f, 
									  const mb_motion_s *motion_b,
									  uint8_t *mb,  
									  int lx, int h)
{
	return (*pbdist1)(motion_f->blk, motion_b->blk, 
					 mb, lx, 
					 motion_f->hx, motion_f->hy,
					 motion_b->hx, motion_b->hy,
					 h);
}

#ifdef DEBUG_MC
static void display_mb(mb_motion_s *lum_mc, 
				  uint8_t **ref, 
				  subsampled_mb_s *ssblk,
				  int lx, int h )

{
	int x,y;
	int sum = 0;
	int uvlx = (lx>>1);
	int uvh = (h>>1);
	/* N.b. MC co-ordinates are computed in half-pel units! */
	int lblkoffset = lum_mc->fieldoff +
		(lum_mc->pos.x>>1) + (lum_mc->pos.y>>1)*lx;
	/*int cblkoffset = (lum_mc->fieldoff>>1) +
	  (lum_mc->pos.x>>2) + (lum_mc->pos.y>>2)*uvlx;*/
	fprintf(stderr, "ref[0] = %08x (%d,%d) width=%d reconbase = %08x blk =%08x\n", 
			(int)ref[0], (lum_mc->pos.x>>1), (lum_mc->pos.y>>1),
			lx,
			(int)(ref[0]+lblkoffset), (int)(lum_mc->blk) );
		for( y = 0; y < 16; ++y )
		{
			for( x = 0; x< 16; ++x )
			{
				int diff = *(ref[0]+lblkoffset+x+y*lx)-*(ssblk->mb+x+y*lx);
				sum += diff*diff;
				fprintf( stderr,"%04d ", diff );
			}
			fprintf(stderr,"\n");
		}
		fprintf(stderr, "sumsq = %d [%d] (%d)\n", 
				sum, 
				lum_mc->var,
				unidir_pred_var(lum_mc, ssblk->mb, lx,  h)
			);
		
}
#endif

/*
 *
 * motion estimation for frame pictures
 * picture: picture object for which MC is to be computed.
 * mbi:    pointer to macroblock info of picture object
 * mb_row_start: offset in chrominance block of start of this MB's row
 *
 * results:
 * mbi->
 *  mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *  MV[][][]: motion vectors (frame format)
 *  motion_type: MC_FRAME, MC_DMV, MC_FIELD
 *
 * TODO: MC_DMV currently never used.  It should in any case trigger on
 * the current (dynamically selected) bigroup length and not the fixed
 * Maximum bi-group length M.
 */
static void frame_ME(pict_data_s *picture,
					 int mb_row_start,
					 int i, int j, 
					 mbinfo_s *mbi)
{
	uint8_t **oldrefimg, **newrefimg;

	mb_motion_s framef_mc;
	mb_motion_s frameb_mc;
	mb_motion_s dualpf_mc;
	mb_motion_s topfldf_mc;
	mb_motion_s botfldf_mc;
	mb_motion_s topfldb_mc;
	mb_motion_s botfldb_mc;
	mb_motion_s zeromot_mc;

	int var,v0;
	int vmc,vmcf,vmcr,vmci;
	int vmcfieldf,vmcfieldr,vmcfieldi;
	subsampled_mb_s ssmb;
	subsampled_mb_s  botssmb;

	int imins[2][2],jmins[2][2];
	int imindmv,jmindmv,vmc_dp;
	
	/* Select between the original or the reconstructed image for
	   final refinement of motion estimation */
	if( ctl_refine_from_rec )
	{
		oldrefimg = picture->oldref;
		newrefimg = picture->newref;
	}
	else
	{
		oldrefimg = picture->oldorg;
		newrefimg = picture->neworg;
	}


	/* A.Stevens fast motion estimation data is appended to actual
	   luminance information. 
	   TODO: The append thing made sense before we had
	   a nice tidy compression record for each picture but now 
	   it should really be replaced by additional pointers to
	   seperate buffers.
	*/
	ssmb.mb = picture->curorg[0] + mb_row_start + i;
	ssmb.umb = (uint8_t*)(picture->curorg[1] + (i>>1) + (mb_row_start>>2));
	ssmb.vmb = (uint8_t*)(picture->curorg[2] + (i>>1) + (mb_row_start>>2));
	ssmb.fmb = (uint8_t*)(picture->curorg[0] + fsubsample_offset + 
						  ((i>>1) + (mb_row_start>>2)));
	ssmb.qmb = (uint8_t*)(picture->curorg[0] + qsubsample_offset + 
						  (i>>2) + (mb_row_start>>4));


	/* Zero motion vector - useful for some optimisations
	 */
	zeromot_mc.pos.x = i;
	zeromot_mc.pos.y = j;
	zeromot_mc.fieldsel = 0;
	zeromot_mc.fieldoff = 0;
	zeromot_mc.blk = oldrefimg[0]+mb_row_start+i;

	/* Compute variance MB as a measure of Intra-coding complexity
	   We include chrominance information here, scaled to compensate
	   for sub-sampling.  Silly MPEG forcing chrom/lum to have same
	   quantisations... ;-)
	 */
	var = (*pvariance)(ssmb.mb,16,width);

	if (picture->pict_type==I_TYPE)
	{
		mbi->mb_type = MB_INTRA;
	}
	else if (picture->pict_type==P_TYPE)
	{
		/* For P pictures we take into account chrominance. This
		   provides much better performance at scene changes */
		var += chrom_var_sum(&ssmb,16,width);

		if (picture->frame_pred_dct)
		{
			mb_me_search(picture->oldorg[0],oldrefimg[0],&ssmb,
					   width,i,j,picture->sxf,picture->syf,16,width,height,
					    &framef_mc);
			framef_mc.fieldoff = 0;
			vmc = vmcf =
				unidir_var_sum( &framef_mc, oldrefimg, &ssmb, width, 16 );
			mbi->motion_type = MC_FRAME;
		}
		else
		{
			botssmb.mb = ssmb.mb+width;
			botssmb.fmb = ssmb.mb+(width>>1);
			botssmb.qmb = ssmb.qmb+(width>>2);
			botssmb.umb = ssmb.umb+(width>>1);
			botssmb.vmb = ssmb.vmb+(width>>1);

			frame_estimate(picture->oldorg[0],oldrefimg[0],
						   &ssmb, &botssmb,
						   i,j,picture->sxf,picture->syf,
						   &framef_mc,
						   &topfldf_mc,
						   &botfldf_mc,
						   imins,jmins);
			vmcf = 
				unidir_var_sum( &framef_mc, oldrefimg, &ssmb, width, 16 );
			vmcfieldf = 
				unidir_var_sum( &topfldf_mc, oldrefimg, &ssmb, (width<<1), 8 ) +
				
				unidir_var_sum( &botfldf_mc, oldrefimg, &botssmb, (width<<1), 8 );
			if ( ctl_M==1)
			{
				dpframe_estimate(picture,oldrefimg[0],&ssmb,
								 i,j>>1,imins,jmins,
								 &dualpf_mc,
								 &imindmv,&jmindmv, &vmc_dp);
			}

			/* NOTE: Typically M =3 so DP actually disabled... */
			/* select between dual prime, frame and field prediction */
			if ( ctl_M==1 && vmc_dp<vmcf && vmc_dp<vmcfieldf)
			{
				mbi->motion_type = MC_DMV;
				/* No chrominance squared difference measure yet.
				   Assume identical to luminance */
				vmc = vmc_dp + vmc_dp;
			}
			else if ( vmcf < vmcfieldf)
			{
				mbi->motion_type = MC_FRAME;
				vmc = vmcf;
					
			}
			else
			{
				mbi->motion_type = MC_FIELD;
				vmc = vmcfieldf;
			}
		}


		/* select between intra or non-intra coding:
		 *
		 * selection is based on intra block variance (var) vs.
		 * prediction error variance (vmc)
		 *
		 * Used to be: blocks with small prediction error are always 
		 * coded non-intra even if variance is smaller 
		 * This seemed unreasonable and potentially brittle and was changed so
		 * that intra is chosen if its variance is smaller.
		 * 
		 */


		if (vmc>var*3/2 )
		{
			mbi->mb_type = MB_INTRA;
			mbi->var = var;
		}
		
		else
		{
			/* select between MC / No-MC
			 *
			 * use No-MC if var(No-MC) <= 1.25*var(MC)
			 * (i.e slightly biased towards No-MC)
			 *
			 * blocks with small prediction error are always coded as No-MC
			 * (requires no motion vectors, allows skipping)
			 */

			zeromot_mc.var = unidir_pred_var(&zeromot_mc, ssmb.mb, width, 16 );
			v0 = unidir_var_sum(&zeromot_mc, oldrefimg, &ssmb, width, 16 );
			if (4*v0>5*vmc )
			{
				/* use MC */
				var = vmc;
				mbi->mb_type = MB_FORWARD;
				if (mbi->motion_type==MC_FRAME)
				{
					mbi->MV[0][0][0] = framef_mc.pos.x - (i<<1);
					mbi->MV[0][0][1] = framef_mc.pos.y - (j<<1);
				}
				else if (mbi->motion_type==MC_DMV)
				{
					/* these are FRAME vectors */
					/* same parity vector */
					mbi->MV[0][0][0] = dualpf_mc.pos.x - (i<<1);
					mbi->MV[0][0][1] = (dualpf_mc.pos.y<<1) - (j<<1);

					/* opposite parity vector */
					mbi->dmvector[0] = imindmv;
					mbi->dmvector[1] = jmindmv;
				}
				else
				{
					/* these are FRAME vectors */
					mbi->MV[0][0][0] = topfldf_mc.pos.x - (i<<1);
					mbi->MV[0][0][1] = (topfldf_mc.pos.y<<1) - (j<<1);
					mbi->MV[1][0][0] = botfldf_mc.pos.x - (i<<1);
					mbi->MV[1][0][1] = (botfldf_mc.pos.y<<1) - (j<<1);
					mbi->mv_field_sel[0][0] = topfldf_mc.fieldsel;
					mbi->mv_field_sel[1][0] = botfldf_mc.fieldsel;
				}
			}
			else
			{

				/* No-MC */
				var = v0;
				mbi->mb_type = 0;
				mbi->motion_type = MC_FRAME;
				mbi->MV[0][0][0] = 0;
				mbi->MV[0][0][1] = 0;
			}
		}
	}
	else /* if (pict_type==B_TYPE) */
	{
		if (picture->frame_pred_dct)
		{


			var = (*pvariance)(ssmb.mb,16,width) +
				  chrom_var_sum(&ssmb,16,width);
			/* forward */
			mb_me_search(picture->oldorg[0],oldrefimg[0],&ssmb,
					   width,i,j,picture->sxf,picture->syf,
					   16,width,height,
					   &framef_mc
					   );
			framef_mc.fieldoff = 0;
			vmcf = unidir_var_sum( &framef_mc, oldrefimg, &ssmb, width, 16 );

			/* backward */
			mb_me_search(picture->neworg[0],newrefimg[0],&ssmb,
						 width,i,j,picture->sxb,picture->syb,
						 16,width,height,
						 &frameb_mc);
			frameb_mc.fieldoff = 0;
			vmcr = unidir_var_sum( &frameb_mc, newrefimg, &ssmb, width, 16 );

			/* interpolated (bidirectional) */

			/*vmci = bidir_pred_var( &framef_mc, &frameb_mc, ssmb.mb, width, 16 );*/
			vmci = bidir_var_sum( &framef_mc, &frameb_mc,  oldrefimg, newrefimg,  &ssmb, width, 16 );



			/* decisions */

			/* select between forward/backward/interpolated prediction:
			 * use the one with smallest mean sqaured prediction error
			 */
			if (vmcf<=vmcr && vmcf<=vmci)
			{
				vmc = vmcf;
				mbi->mb_type = MB_FORWARD;
			}
			else if (vmcr<=vmci)
			{
				vmc = vmcr;
				mbi->mb_type = MB_BACKWARD;
			}
			else
			{
				vmc = vmci;
				mbi->mb_type = MB_FORWARD|MB_BACKWARD;
			}

			mbi->motion_type = MC_FRAME;
		}
		else
		{
			botssmb.mb = ssmb.mb+width;
			botssmb.fmb = ssmb.mb+(width>>1);
			botssmb.qmb = ssmb.qmb+(width>>2);
			botssmb.umb = ssmb.umb+(width>>1);
			botssmb.vmb = ssmb.vmb+(width>>1);

			/* forward prediction */
			frame_estimate(picture->oldorg[0],oldrefimg[0],
						   &ssmb, &botssmb,
						   i,j,picture->sxf,picture->syf,
						   &framef_mc,
						   &topfldf_mc,
						   &botfldf_mc,
						   imins,jmins);


			/* backward prediction */
			frame_estimate(picture->neworg[0],newrefimg[0],
						   &ssmb, &botssmb,
						   i,j,picture->sxb,picture->syb,
						   &frameb_mc,
						   &topfldb_mc,
						   &botfldb_mc,
						   imins,jmins);

			vmcf = unidir_var_sum( &framef_mc, oldrefimg, &ssmb, width, 16 );
			vmcr = unidir_var_sum( &frameb_mc, newrefimg, &ssmb, width, 16 );
			vmci = bidir_var_sum( &framef_mc, &frameb_mc, 
								  oldrefimg, newrefimg,
								  &ssmb, width, 16 );

			vmcfieldf =	
				unidir_var_sum( &topfldf_mc, oldrefimg, &ssmb, (width<<1), 8 ) +
				unidir_var_sum( &botfldf_mc, oldrefimg, &botssmb, (width<<1), 8 );
			vmcfieldr =	
				unidir_var_sum( &topfldb_mc, newrefimg, &ssmb, (width<<1), 8 ) +
				unidir_var_sum( &botfldb_mc, newrefimg, &botssmb, (width<<1), 8 );
			vmcfieldi = bidir_var_sum( &topfldf_mc, &topfldb_mc, 
									   oldrefimg,newrefimg,
									   &ssmb, width<<1, 8) +
				        bidir_var_sum( &botfldf_mc, &botfldb_mc, 
									   oldrefimg,newrefimg,
									   &botssmb, width<<1, 8);

			/* select prediction type of minimum distance from the
			 * six candidates (field/frame * forward/backward/interpolated)
			 */
			if (vmci<vmcfieldi && vmci<vmcf && vmci<vmcfieldf
				  && vmci<vmcr && vmci<vmcfieldr)
			{
				/* frame, interpolated */
				mbi->mb_type = MB_FORWARD|MB_BACKWARD;
				mbi->motion_type = MC_FRAME;
				vmc = vmci;
			}
			else if ( vmcfieldi<vmcf && vmcfieldi<vmcfieldf
					   && vmcfieldi<vmcr && vmcfieldi<vmcfieldr)
			{
				/* field, interpolated */
				mbi->mb_type = MB_FORWARD|MB_BACKWARD;
				mbi->motion_type = MC_FIELD;
				vmc = vmcfieldi;
			}
			else if (vmcf<vmcfieldf && vmcf<vmcr && vmcf<vmcfieldr)
			{
				/* frame, forward */
				mbi->mb_type = MB_FORWARD;
				mbi->motion_type = MC_FRAME;
				vmc = vmcf;

			}
			else if ( vmcfieldf<vmcr && vmcfieldf<vmcfieldr)
			{
				/* field, forward */
				mbi->mb_type = MB_FORWARD;
				mbi->motion_type = MC_FIELD;
				vmc = vmcfieldf;
			}
			else if (vmcr<vmcfieldr)
			{
				/* frame, backward */
				mbi->mb_type = MB_BACKWARD;
				mbi->motion_type = MC_FRAME;
				vmc = vmcr;
			}
			else
			{
				/* field, backward */
				mbi->mb_type = MB_BACKWARD;
				mbi->motion_type = MC_FIELD;
				vmc = vmcfieldr;

			}
		}

		/* select between intra or non-intra coding:
		 *
		 * selection is based on intra block variance (var) vs.
		 * prediction error variance (vmc)
		 *
		 * Used to be: blocks with small prediction error are always 
		 * coded non-intra even if variance is smaller 
		 * This seemed unreasonable and potentially brittle and was changed so
		 * that intra is chosen if its variance is smaller.
		 * 
		 */

		if (vmc>var*3/2)
			mbi->mb_type = MB_INTRA;
		else
		{
			mbi->var = vmc;
			if (mbi->motion_type==MC_FRAME)
			{
				/* forward */
				mbi->MV[0][0][0] = framef_mc.pos.x - (i<<1);
				mbi->MV[0][0][1] = framef_mc.pos.y - (j<<1);
				/* backward */
				mbi->MV[0][1][0] = frameb_mc.pos.x - (i<<1);
				mbi->MV[0][1][1] = frameb_mc.pos.y - (j<<1);
			}
			else
			{
				/* these are FRAME vectors */
				/* forward */
				mbi->MV[0][0][0] = topfldf_mc.pos.x - (i<<1);
				mbi->MV[0][0][1] = (topfldf_mc.pos.y<<1) - (j<<1);
				mbi->MV[1][0][0] = botfldf_mc.pos.x - (i<<1);
				mbi->MV[1][0][1] = (botfldf_mc.pos.y<<1) - (j<<1);
				mbi->mv_field_sel[0][0] = topfldf_mc.fieldsel;
				mbi->mv_field_sel[1][0] = botfldf_mc.fieldsel;
				/* backward */
				mbi->MV[0][1][0] = topfldb_mc.pos.x - (i<<1);
				mbi->MV[0][1][1] = (topfldb_mc.pos.y<<1) - (j<<1);
				mbi->MV[1][1][0] = botfldb_mc.pos.x - (i<<1);
				mbi->MV[1][1][1] = (botfldb_mc.pos.y<<1) - (j<<1);
				mbi->mv_field_sel[0][1] = topfldb_mc.fieldsel;
				mbi->mv_field_sel[1][1] = botfldb_mc.fieldsel;
			}
		}
	}
#ifdef DEBUG_MC
	if( picture->present >= 275 && picture->present <= 275 &&
		i == 16*18 && j == 9*16 )
	{
		fprintf(stderr," PIC INFO: %d %d %d\n",
				picture->present, picture->pict_type, picture->temp_ref );
		fprintf( stderr,"MC mvf(%d,%d)%d/%d mvb(%d,%d)%d/%d i=%d var=%d type=%d\n",
				 mbi->MV[0][0][0],mbi->MV[0][0][1], vmcf, framef_mc.sad,
				 mbi->MV[0][1][0],mbi->MV[0][1][1], vmcr, frameb_mc.sad,
				 vmci, var, mbi->mb_type
				 );

		display_mb( &frameb_mc, newrefimg, &ssmb, width, 16 );
	}
#endif
}


/*
 * motion estimation for field pictures
 * picture: picture object for which MC is to be computed.
 * mbi:    pointer to macroblock info of picture object
 * mb_row_start: offset in chrominance block of start of this MB's row
 * secondfield: indicates second field of a frame (in P fields this means
 *              that reference field of opposite parity is in curref instead
 *              of oldref)
 * ipflag: indicates a P type field which is the second field of a frame
 *         in which the first field is I type (this restricts predictions
 *         to be based only on the opposite parity (=I) field)
 *
 * results:
 * mbi->
 *  mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *  MV[][][]: motion vectors (field format)
 *  mv_field_sel: top/bottom field
 *  motion_type: MC_FIELD, MC_16X8
 *
 */
static void field_ME(
	pict_data_s *picture,
	int mb_row_start,
	int i, int j, 
	mbinfo_s *mbi, 
	int secondfield, int ipflag)
{
	int w2;
	uint8_t **oldrefimg, **newrefimg;
	uint8_t *toporg, *topref, *botorg, *botref;
	subsampled_mb_s ssmb;
	mb_motion_s fieldsp_mc, dualp_mc;
	mb_motion_s fieldf_mc, fieldb_mc;
	mb_motion_s field8uf_mc, field8lf_mc;
	mb_motion_s field8ub_mc, field8lb_mc;
	int var,vmc,v0,dmc,dmcfieldi,dmcfield,dmcfieldf,dmcfieldr,dmc8i;
	int dmc8f,dmc8r;
	int vmc_dp,dctl_dp;


	/* Select between the original or the reconstructed image for
	   final refinement of motion estimation */
	if( ctl_refine_from_rec )
	{
		oldrefimg = picture->oldref;
		newrefimg = picture->newref;
	}
	else
	{
		oldrefimg = picture->oldorg;
		newrefimg = picture->neworg;
	}

	w2 = width<<1;

	/* Fast motion data sits at the end of the luminance buffer */
	ssmb.mb = picture->curorg[0] + i + w2*j;
	ssmb.umb = picture->curorg[1] + ((i>>1)+(w2>>1)*(j>>1));
	ssmb.vmb = picture->curorg[2] + ((i>>1)+(w2>>1)*(j>>1));
	ssmb.fmb = picture->curorg[0] + fsubsample_offset+((i>>1)+(w2>>1)*(j>>1));
	ssmb.qmb = picture->curorg[0] + qsubsample_offset+ (i>>2)+(w2>>2)*(j>>2);

	if (picture->pict_struct==BOTTOM_FIELD)
	{
		ssmb.mb += width;
		ssmb.umb += (width >> 1);
		ssmb.vmb += (width >> 1);
		ssmb.fmb += (width >> 1);
		ssmb.qmb += (width >> 2);
	}

	var = (*pvariance)(ssmb.mb,16,w2) + 
		( (*pvariance)(ssmb.umb,8,(width>>1)) + (*pvariance)(ssmb.vmb,8,(width>>1)))*2;

	if (picture->pict_type==I_TYPE)
		mbi->mb_type = MB_INTRA;
	else if (picture->pict_type==P_TYPE)
	{
		toporg = picture->oldorg[0];
		topref = oldrefimg[0];
		botorg = picture->oldorg[0] + width;
		botref = oldrefimg[0] + width;
                                                        
		if (secondfield)
		{
			/* opposite parity field is in same frame */
			if (picture->pict_struct==TOP_FIELD)
			{
				/* current is top field */
				botorg = picture->curorg[0] + width;
				botref = picture->curref[0] + width;
			}
			else
			{
				/* current is bottom field */
				toporg = picture->curorg[0];
				topref = picture->curref[0];
			}
			if( frame_num > 8 )
				frame_num = (frame_num + 1) - 1 ;
		}
		field_estimate(picture,
					   toporg,topref,botorg,botref,&ssmb,
					   i,j,picture->sxf,picture->syf,ipflag,
					   &fieldf_mc,
					   &field8uf_mc,
					   &field8lf_mc,
					   &fieldsp_mc);
		dmcfield = fieldf_mc.sad;
		dmc8f = field8uf_mc.sad + field8lf_mc.sad;

		dctl_dp = 100000000;		/* Suppress compiler warning */
		if (ctl_M==1 && !ipflag)  /* generic condition which permits Dual Prime */
		{
			dpfield_estimate(picture,
							 topref,botref,ssmb.mb,i,j,
							 &fieldsp_mc,
							 &dualp_mc,
							 &vmc_dp);
			dctl_dp = dualp_mc.sad;
		}
		/* select between dual prime, field and 16x8 prediction */
		if (ctl_M==1 && !ipflag && dctl_dp<dmc8f && dctl_dp<dmcfield)
		{
			/* Dual Prime prediction */
			mbi->motion_type = MC_DMV;
			dmc = dualp_mc.sad;
			vmc = vmc_dp;

		}
		else if (dmc8f<dmcfield)
		{
			/* 16x8 prediction */
			mbi->motion_type = MC_16X8;
			/* upper and lower half blocks */
			vmc =  field8uf_mc.var + unidir_pred_var( &field8uf_mc, ssmb.mb, w2, 8);
			vmc += field8lf_mc.var + unidir_pred_var( &field8lf_mc, ssmb.mb, w2, 8);
		}
		else
		{
			/* field prediction */
			mbi->motion_type = MC_FIELD;
			vmc = fieldf_mc.var + unidir_pred_var( &fieldf_mc, ssmb.mb, w2, 16 );
		}

		/* select between intra and non-intra coding */

		if ( vmc>var && vmc>=9*256)
			mbi->mb_type = MB_INTRA;
		else
		{
			/* zero MV field prediction from same parity ref. field
			 * (not allowed if ipflag is set)
			 */
			if (!ipflag)
				v0 = (*pdist2)(((picture->pict_struct==BOTTOM_FIELD)?botref:topref) + i + w2*j,
							   ssmb.mb,w2,0,0,16);
			else
				v0 = 1234;			/* Keep Compiler happy... */

			if (ipflag  || (4*v0>5*vmc && v0>=9*256))
			{
				mbi->mb_type = MB_FORWARD;
				if (mbi->motion_type==MC_FIELD)
				{
					mbi->MV[0][0][0] = fieldf_mc.pos.x - (i<<1);
					mbi->MV[0][0][1] = fieldf_mc.pos.y - (j<<1);
					mbi->mv_field_sel[0][0] = fieldf_mc.fieldsel;
				}
				else if (mbi->motion_type==MC_DMV)
				{
					/* same parity vector */
					mbi->MV[0][0][0] = fieldsp_mc.pos.x - (i<<1);
					mbi->MV[0][0][1] = fieldsp_mc.pos.y - (j<<1);

					/* opposite parity vector */
					mbi->dmvector[0] = dualp_mc.pos.x;
					mbi->dmvector[1] = dualp_mc.pos.y;
				}
				else
				{
					mbi->MV[0][0][0] = field8uf_mc.pos.x - (i<<1);
					mbi->MV[0][0][1] = field8uf_mc.pos.y - (j<<1);
					mbi->MV[1][0][0] = field8lf_mc.pos.x - (i<<1);
					mbi->MV[1][0][1] = field8lf_mc.pos.y - ((j+8)<<1);
					mbi->mv_field_sel[0][0] = field8uf_mc.fieldsel;
					mbi->mv_field_sel[1][0] = field8lf_mc.fieldsel;
				}
			}
			else
			{
				/* No MC */
				mbi->mb_type = 0;
				mbi->motion_type = MC_FIELD;
				mbi->MV[0][0][0] = 0;
				mbi->MV[0][0][1] = 0;
				mbi->mv_field_sel[0][0] = (picture->pict_struct==BOTTOM_FIELD);
			}
		}
	}
	else /* if (pict_type==B_TYPE) */
	{
		/* forward prediction */
		field_estimate(picture,
					   picture->oldorg[0],oldrefimg[0],
					   picture->oldorg[0]+width,oldrefimg[0]+width,&ssmb,
					   i,j,picture->sxf,picture->syf,0,
					   &fieldf_mc,
					   &field8uf_mc,
					   &field8lf_mc,
					   &fieldsp_mc);
		dmcfieldf = fieldf_mc.sad;
		dmc8f = field8uf_mc.sad + field8lf_mc.sad;

		/* backward prediction */
		field_estimate(picture,
					   picture->neworg[0],newrefimg[0],picture->neworg[0]+width,newrefimg[0]+width,
					   &ssmb,
					   i,j,picture->sxb,picture->syb,0,
					   &fieldb_mc,
					   &field8ub_mc,
					   &field8lb_mc,
					   &fieldsp_mc);
		dmcfieldr = fieldb_mc.sad;
		dmc8r = field8ub_mc.sad + field8lb_mc.sad;

		/* calculate distances for bidirectional prediction */
		/* field */
		dmcfieldi = bidir_pred_sad( &fieldf_mc, &fieldb_mc, ssmb.mb, w2, 16);

		/* 16x8 upper and lower half blocks */
		dmc8i =  bidir_pred_sad( &field8uf_mc, &field8ub_mc, ssmb.mb, w2, 16 );
		dmc8i += bidir_pred_sad( &field8lf_mc, &field8lb_mc, ssmb.mb, w2, 16 );

		/* select prediction type of minimum distance */
		if (dmcfieldi<dmc8i && dmcfieldi<dmcfieldf && dmcfieldi<dmc8f
			&& dmcfieldi<dmcfieldr && dmcfieldi<dmc8r)
		{
			/* field, interpolated */
			mbi->mb_type = MB_FORWARD|MB_BACKWARD;
			mbi->motion_type = MC_FIELD;
			vmc = fieldf_mc.var + bidir_pred_var( &fieldf_mc, &fieldb_mc, ssmb.mb, w2, 16);
		}
		else if (dmc8i<dmcfieldf && dmc8i<dmc8f
				 && dmc8i<dmcfieldr && dmc8i<dmc8r)
		{
			/* 16x8, interpolated */
			mbi->mb_type = MB_FORWARD|MB_BACKWARD;
			mbi->motion_type = MC_16X8;

			/* upper and lower half blocks */
			vmc =  field8uf_mc.var + bidir_pred_var( &field8uf_mc, &field8ub_mc, ssmb.mb, w2, 8);
			vmc += field8lf_mc.var + bidir_pred_var( &field8lf_mc, &field8lb_mc, ssmb.mb, w2, 8);
		}
		else if (dmcfieldf<dmc8f && dmcfieldf<dmcfieldr && dmcfieldf<dmc8r)
		{
			/* field, forward */
			mbi->mb_type = MB_FORWARD;
			mbi->motion_type = MC_FIELD;
			vmc = fieldf_mc.var + unidir_pred_var( &fieldf_mc, ssmb.mb, w2, 16);
		}
		else if (dmc8f<dmcfieldr && dmc8f<dmc8r)
		{
			/* 16x8, forward */
			mbi->mb_type = MB_FORWARD;
			mbi->motion_type = MC_16X8;

			/* upper and lower half blocks */
			vmc = field8uf_mc.var +  unidir_pred_var( &field8uf_mc, ssmb.mb, w2, 8);
			vmc += field8lf_mc.var + unidir_pred_var( &field8lf_mc, ssmb.mb, w2, 8);
		}
		else if (dmcfieldr<dmc8r)
		{
			/* field, backward */
			mbi->mb_type = MB_BACKWARD;
			mbi->motion_type = MC_FIELD;
			vmc = fieldb_mc.var + unidir_pred_var( &fieldb_mc, ssmb.mb, w2, 16 );
		}
		else
		{
			/* 16x8, backward */
			mbi->mb_type = MB_BACKWARD;
			mbi->motion_type = MC_16X8;

			/* upper and lower half blocks */
			vmc =  field8ub_mc.var + unidir_pred_var( &field8ub_mc, ssmb.mb, w2, 8);
			vmc += field8lb_mc.var + unidir_pred_var( &field8lb_mc, ssmb.mb, w2, 8);

		}

		/* select between intra and non-intra coding */
		if (vmc>var && vmc>=9*256)
			mbi->mb_type = MB_INTRA;
		else
		{
			if (mbi->motion_type==MC_FIELD)
			{
				/* forward */
				mbi->MV[0][0][0] = fieldf_mc.pos.x - (i<<1);
				mbi->MV[0][0][1] = fieldf_mc.pos.y - (j<<1);
				mbi->mv_field_sel[0][0] = fieldf_mc.fieldsel;
				/* backward */
				mbi->MV[0][1][0] = fieldb_mc.pos.x - (i<<1);
				mbi->MV[0][1][1] = fieldb_mc.pos.y - (j<<1);
				mbi->mv_field_sel[0][1] = fieldb_mc.fieldsel;
			}
			else /* MC_16X8 */
			{
				/* forward */
				mbi->MV[0][0][0] = field8uf_mc.pos.x - (i<<1);
				mbi->MV[0][0][1] = field8uf_mc.pos.y - (j<<1);
				mbi->mv_field_sel[0][0] = field8uf_mc.fieldsel;
				mbi->MV[1][0][0] = field8lf_mc.pos.x - (i<<1);
				mbi->MV[1][0][1] = field8lf_mc.pos.y - ((j+8)<<1);
				mbi->mv_field_sel[1][0] = field8lf_mc.fieldsel;
				/* backward */
				mbi->MV[0][1][0] = field8ub_mc.pos.x - (i<<1);
				mbi->MV[0][1][1] = field8ub_mc.pos.y - (j<<1);
				mbi->mv_field_sel[0][1] = field8ub_mc.fieldsel;
				mbi->MV[1][1][0] = field8lb_mc.pos.x - (i<<1);
				mbi->MV[1][1][1] = field8lb_mc.pos.y - ((j+8)<<1);
				mbi->mv_field_sel[1][1] = field8lb_mc.fieldsel;
			}
		}
	}

}

/*
 * frame picture motion estimation
 *
 * org: top left pel of source reference frame
 * ref: top left pel of reconstructed reference frame
 * ssmb:  macroblock to be matched
 * i,j: location of mb relative to ref (=center of search window)
 * sx,sy: half widths of search window
 * besfr: location and SAD of best frame prediction
 * besttop: location of best field pred. for top field of mb
 * bestbo : location of best field pred. for bottom field of mb
 */

static void frame_estimate(
	uint8_t *org,
	uint8_t *ref,
	subsampled_mb_s *topssmb,
	subsampled_mb_s *botssmb,
	int i, int j, int sx, int sy,
	mb_motion_s *bestfr,
	mb_motion_s *besttop,
	mb_motion_s *bestbot,
	int imins[2][2],
	int jmins[2][2]
	)
{
	mb_motion_s topfld_mc;
	mb_motion_s botfld_mc;

	/* frame prediction */
	mb_me_search(org,ref,topssmb,width,i,j,sx,sy,16,width,height,
						  bestfr );
	bestfr->fieldsel = 0;
	bestfr->fieldoff = 0;

	/* predict top field from top field */
	mb_me_search(org,ref,topssmb,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
			   &topfld_mc);

	/* predict top field from bottom field */
	mb_me_search(org+width,ref+width,topssmb, width<<1,i,j>>1,sx,sy>>1,8,
			   width,height>>1, &botfld_mc);

	/* set correct field selectors... */
	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	topfld_mc.fieldoff = 0;
	botfld_mc.fieldoff = width;

	imins[0][0] = topfld_mc.pos.x;
	jmins[0][0] = topfld_mc.pos.y;
	imins[1][0] = botfld_mc.pos.x;
	jmins[1][0] = botfld_mc.pos.y;

	/* select prediction for top field */
	if (topfld_mc.sad<=botfld_mc.sad)
	{
		*besttop = topfld_mc;
	}
	else
	{
		*besttop = botfld_mc;
	}

	/* predict bottom field from top field */
	mb_me_search(org,ref,botssmb,
					width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
					&topfld_mc);

	/* predict bottom field from bottom field */
	mb_me_search(org+width,ref+width,botssmb,
					width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
					&botfld_mc);

	/* set correct field selectors... */
	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	topfld_mc.fieldoff = 0;
	botfld_mc.fieldoff = width;

	imins[0][1] = topfld_mc.pos.x;
	jmins[0][1] = topfld_mc.pos.y;
	imins[1][1] = botfld_mc.pos.x;
	jmins[1][1] = botfld_mc.pos.y;

	/* select prediction for bottom field */
	if (botfld_mc.sad<=topfld_mc.sad)
	{
		*bestbot = botfld_mc;
	}
	else
	{
		*bestbot = topfld_mc;
	}
}

/*
 * field picture motion estimation subroutine
 *
 * toporg: address of original top reference field
 * topref: address of reconstructed top reference field
 * botorg: address of original bottom reference field
 * botref: address of reconstructed bottom reference field
 * ssmmb:  macroblock to be matched
 * i,j: location of mb (=center of search window)
 * sx,sy: half width/height of search window
 *
 * bestfld: location and distance of best field prediction
 * best8u: location of best 16x8 pred. for upper half of mb
 * best8lp: location of best 16x8 pred. for lower half of mb
 * bdestsp: location and distance of best same parity field
 *                    prediction (needed for dual prime, only valid if
 *                    ipflag==0)
 */

static void field_estimate (
	pict_data_s *picture,
	uint8_t *toporg,
	uint8_t *topref, 
	uint8_t *botorg, 
	uint8_t *botref,
	subsampled_mb_s *ssmb,
	int i, int j, int sx, int sy, int ipflag,
	mb_motion_s *bestfld,
	mb_motion_s *best8u,
	mb_motion_s *best8l,
	mb_motion_s *bestsp)

{
	mb_motion_s topfld_mc;
	mb_motion_s botfld_mc;
	int dt, db;
	int notop, nobot;
	subsampled_mb_s botssmb;

	botssmb.mb = ssmb->mb+width;
	botssmb.umb = ssmb->umb+(width>>1);
	botssmb.vmb = ssmb->vmb+(width>>1);
	botssmb.fmb = ssmb->fmb+(width>>1);
	botssmb.qmb = ssmb->qmb+(width>>2);

	/* if ipflag is set, predict from field of opposite parity only */
	notop = ipflag && (picture->pict_struct==TOP_FIELD);
	nobot = ipflag && (picture->pict_struct==BOTTOM_FIELD);

	/* field prediction */

	/* predict current field from top field */
	if (notop)
		topfld_mc.sad = dt = 65536; /* infinity */
	else
		mb_me_search(toporg,topref,ssmb,width<<1,
				   i,j,sx,sy>>1,16,width,height>>1,
				   &topfld_mc);
	dt = topfld_mc.sad;
	/* predict current field from bottom field */
	if (nobot)
		botfld_mc.sad = db = 65536; /* infinity */
	else
		mb_me_search(botorg,botref,ssmb,width<<1,
				   i,j,sx,sy>>1,16,width,height>>1,
				   &botfld_mc);
	db = botfld_mc.sad;
	/* Set correct field selectors */
	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	topfld_mc.fieldoff = 0;
	botfld_mc.fieldoff = width;

	/* same parity prediction (only valid if ipflag==0) */
	if (picture->pict_struct==TOP_FIELD)
	{
		*bestsp = topfld_mc;
	}
	else
	{
		*bestsp = botfld_mc;
	}

	/* select field prediction */
	if (dt<=db)
	{
		*bestfld = topfld_mc;
	}
	else
	{
		*bestfld = botfld_mc;
	}


	/* 16x8 motion estimation */

	/* predict upper half field from top field */

	if (notop)
		topfld_mc.sad = dt = 65536;
	else
		mb_me_search(toporg,topref,ssmb,width<<1,
				   i,j,sx,sy>>1,8,width,height>>1,
				    &topfld_mc);
	dt = topfld_mc.sad;
	/* predict upper half field from bottom field */
	if (nobot)
		botfld_mc.sad = db = 65536;
	else
		mb_me_search(botorg,botref,ssmb,width<<1,
				   i,j,sx,sy>>1,8,width,height>>1,
				    &botfld_mc);
	db = botfld_mc.sad;

	/* Set correct field selectors */
	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	topfld_mc.fieldoff = 0;
	botfld_mc.fieldoff = width;

	/* select prediction for upper half field */
	if (dt<=db)
	{
		*best8u = topfld_mc;
	}
	else
	{
		*best8u = botfld_mc;
	}

	/* predict lower half field from top field */
	/*
	  N.b. For interlaced data width<<4 (width*16) takes us 8 rows
	  down in the same field.  
	  Thus for the fast motion data (2*2
	  sub-sampled) we need to go 4 rows down in the same field.
	  This requires adding width*4 = (width<<2).
	  For the 4*4 sub-sampled motion data we need to go down 2 rows.
	  This requires adding width = width
	 
	*/
	if (notop)
		topfld_mc.sad = dt = 65536;
	else
		mb_me_search(toporg,topref,&botssmb,
				   width<<1,
				   i,j+8,sx,sy>>1,8,width,height>>1,
				    &topfld_mc);
	dt = topfld_mc.sad;
	/* predict lower half field from bottom field */
	if (nobot)
		botfld_mc.sad = db = 65536;
	else
		mb_me_search(botorg,botref,&botssmb,width<<1,
				   i,j+8,sx,sy>>1,8,width,height>>1,
				   &botfld_mc);
	db = botfld_mc.sad;
	/* Set correct field selectors */
	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	topfld_mc.fieldoff = 0;
	botfld_mc.fieldoff = width;

	/* select prediction for lower half field */
	if (dt<=db)
	{
		*best8l = topfld_mc;
	}
	else
	{
		*best8l = botfld_mc;
	}
}

static void dpframe_estimate (
	pict_data_s *picture,
	uint8_t *ref,
	subsampled_mb_s *ssmb,
	
	int i, int j, int iminf[2][2], int jminf[2][2],
	mb_motion_s *best_mc,
	int *imindmvp, int *jmindmvp,
	int *vmcp)
{
	int pref,ppred,delta_x,delta_y;
	int is,js,it,jt,ib,jb,it0,jt0,ib0,jb0;
	int imins = 0;
	int jmins = 0;
	int imint = 0;
	int jmint = 0;
	int iminb = 0;
	int jminb = 0;
	int imindmv = 0;
	int jmindmv = 0;
	int vmc,local_dist;

	/* Calculate Dual Prime distortions for 9 delta candidates
	 * for each of the four minimum field vectors
	 * Note: only for P pictures!
	 */

	/* initialize minimum dual prime distortion to large value */
	vmc = INT_MAX;

	for (pref=0; pref<2; pref++)
	{
		for (ppred=0; ppred<2; ppred++)
		{
			/* convert Cartesian absolute to relative motion vector
			 * values (wrt current macroblock address (i,j)
			 */
			is = iminf[pref][ppred] - (i<<1);
			js = jminf[pref][ppred] - (j<<1);

			if (pref!=ppred)
			{
				/* vertical field shift adjustment */
				if (ppred==0)
					js++;
				else
					js--;

				/* mvxs and mvys scaling*/
				is<<=1;
				js<<=1;
				if (picture->topfirst == ppred)
				{
					/* second field: scale by 1/3 */
					is = (is>=0) ? (is+1)/3 : -((-is+1)/3);
					js = (js>=0) ? (js+1)/3 : -((-js+1)/3);
				}
				else
					continue;
			}

			/* vector for prediction from field of opposite 'parity' */
			if (picture->topfirst)
			{
				/* vector for prediction of top field from bottom field */
				it0 = ((is+(is>0))>>1);
				jt0 = ((js+(js>0))>>1) - 1;

				/* vector for prediction of bottom field from top field */
				ib0 = ((3*is+(is>0))>>1);
				jb0 = ((3*js+(js>0))>>1) + 1;
			}
			else
			{
				/* vector for prediction of top field from bottom field */
				it0 = ((3*is+(is>0))>>1);
				jt0 = ((3*js+(js>0))>>1) - 1;

				/* vector for prediction of bottom field from top field */
				ib0 = ((is+(is>0))>>1);
				jb0 = ((js+(js>0))>>1) + 1;
			}

			/* convert back to absolute half-pel field picture coordinates */
			is += i<<1;
			js += j<<1;
			it0 += i<<1;
			jt0 += j<<1;
			ib0 += i<<1;
			jb0 += j<<1;

			if (is >= 0 && is <= (width-16)<<1 &&
				js >= 0 && js <= (height-16))
			{
				for (delta_y=-1; delta_y<=1; delta_y++)
				{
					for (delta_x=-1; delta_x<=1; delta_x++)
					{
						/* opposite field coordinates */
						it = it0 + delta_x;
						jt = jt0 + delta_y;
						ib = ib0 + delta_x;
						jb = jb0 + delta_y;

						if (it >= 0 && it <= (width-16)<<1 &&
							jt >= 0 && jt <= (height-16) &&
							ib >= 0 && ib <= (width-16)<<1 &&
							jb >= 0 && jb <= (height-16))
						{
							/* compute prediction error */
							local_dist = (*pbdist2)(
								ref + (is>>1) + (width<<1)*(js>>1),
								ref + width + (it>>1) + (width<<1)*(jt>>1),
								ssmb->mb,             /* current mb location */
								width<<1,       /* adjacent line distance */
								is&1, js&1, it&1, jt&1, /* half-pel flags */
								8);             /* block height */
							local_dist += (*pbdist2)(
								ref + width + (is>>1) + (width<<1)*(js>>1),
								ref + (ib>>1) + (width<<1)*(jb>>1),
								ssmb->mb + width,     /* current mb location */
								width<<1,       /* adjacent line distance */
								is&1, js&1, ib&1, jb&1, /* half-pel flags */
								8);             /* block height */

							/* update delta with least distortion vector */
							if (local_dist < vmc)
							{
								imins = is;
								jmins = js;
								imint = it;
								jmint = jt;
								iminb = ib;
								jminb = jb;
								imindmv = delta_x;
								jmindmv = delta_y;
								vmc = local_dist;
							}
						}
					}  /* end delta x loop */
				} /* end delta y loop */
			}
		}
	}

	/* TODO: This is now likely to be obsolete... */
	/* Compute L1 error for decision purposes */
	local_dist = (*pbdist1)(
		ref + (imins>>1) + (width<<1)*(jmins>>1),
		ref + width + (imint>>1) + (width<<1)*(jmint>>1),
		ssmb->mb,
		width<<1,
		imins&1, jmins&1, imint&1, jmint&1,
		8);
	local_dist += (*pbdist1)(
		ref + width + (imins>>1) + (width<<1)*(jmins>>1),
		ref + (iminb>>1) + (width<<1)*(jminb>>1),
		ssmb->mb + width,
		width<<1,
		imins&1, jmins&1, iminb&1, jminb&1,
		8);

	best_mc->sad = local_dist;
	best_mc->pos.x = imins;
	best_mc->pos.y = jmins;
	*vmcp = vmc;
	*imindmvp = imindmv;
	*jmindmvp = jmindmv;

}

static void dpfield_estimate(
	pict_data_s *picture,
	uint8_t *topref,
	uint8_t *botref, 
	uint8_t *mb,
	int i, int j, 
	mb_motion_s *bestsp_mc,
	mb_motion_s *bestdp_mc,
	int *vmcp
	)

{
	uint8_t *sameref, *oppref;
	int io0,jo0,io,jo,delta_x,delta_y,mvxs,mvys,mvxo0,mvyo0;
	int imino = 0;
	int jmino = 0;
	int imindmv = 0;
	int jmindmv = 0;
	int vmc_dp,local_dist;
	int imins = 0;
	int jmins = 0;

	/* Calculate Dual Prime distortions for 9 delta candidates */
	/* Note: only for P pictures! */

	/* Assign opposite and same reference pointer */
	if (picture->pict_struct==TOP_FIELD)
	{
		sameref = topref;    
		oppref = botref;
	}
	else 
	{
		sameref = botref;
		oppref = topref;
	}

	/* convert Cartesian absolute to relative motion vector
	 * values (wrt current macroblock address (i,j)
	 */
	mvxs = imins - (i<<1);
	mvys = jmins - (j<<1);

	/* vector for prediction from field of opposite 'parity' */
	mvxo0 = (mvxs+(mvxs>0)) >> 1;  /* mvxs / / */
	mvyo0 = (mvys+(mvys>0)) >> 1;  /* mvys / / 2*/

			/* vertical field shift correction */
	if (picture->pict_struct==TOP_FIELD)
		mvyo0--;
	else
		mvyo0++;

			/* convert back to absolute coordinates */
	io0 = mvxo0 + (i<<1);
	jo0 = mvyo0 + (j<<1);

			/* initialize minimum dual prime distortion to large value */
	vmc_dp = INT_MAX;

	for (delta_y = -1; delta_y <= 1; delta_y++)
	{
		for (delta_x = -1; delta_x <=1; delta_x++)
		{
			/* opposite field coordinates */
			io = io0 + delta_x;
			jo = jo0 + delta_y;

			if (io >= 0 && io <= (width-16)<<1 &&
				jo >= 0 && jo <= (height2-16)<<1)
			{
				/* compute prediction error */
				local_dist = (*pbdist2)(
					sameref + (imins>>1) + width2*(jmins>>1),
					oppref  + (io>>1)    + width2*(jo>>1),
					mb,             /* current mb location */
					width2,         /* adjacent line distance */
					imins&1, jmins&1, io&1, jo&1, /* half-pel flags */
					16);            /* block height */

				/* update delta with least distortion vector */
				if (local_dist < vmc_dp)
				{
					imino = io;
					jmino = jo;
					imindmv = delta_x;
					jmindmv = delta_y;
					vmc_dp = local_dist;
				}
			}
		}  /* end delta x loop */
	} /* end delta y loop */

	/* Compute L1 error for decision purposes */
	bestdp_mc->sad =
		(*pbdist1)(
			sameref + (imins>>1) + width2*(jmins>>1),
			oppref  + (imino>>1) + width2*(jmino>>1),
			mb,             /* current mb location */
			width2,         /* adjacent line distance */
			imins&1, jmins&1, imino&1, jmino&1, /* half-pel flags */
			16);            /* block height */

	bestdp_mc->pos.x = imindmv;
	bestdp_mc->pos.x = jmindmv;
	*vmcp = vmc_dp;
}




 
/* Hierarchical block matching motion estimation search
 *
 * A.Stevens 2000: This is now a big misnomer.  The search is now a
 * hierarchical/sub-sampling search not a full search.  However,
 * experiments have shown it is always close to optimal and almost
 * always very close or optimal.
 *
 * org: top left pel of source reference picture
 * ref: top left pel of reconstructed reference picture
 * ssblk: top-left element of macro block to be motion compensated
 *        at 1*1,2*2 and 4*4 subsampling
 * lx: distance (in bytes) of vertically adjacent pels in ref,blk
 * i0,j0: center of search window
 * sx,sy: half widths of search window
 * h: height of macro block
 * xmax,ymax: right/bottom limits of search area for macro block
 * res: pointers to where the result is stored
 *      N.b. as in the original code result is given as
 *      half pel offset from ref(0,0) not the position relative to i0 j0
 *      as will actually be used.
 *
 * TODO: SHould use half-pel co-ordinates relative to i0,j0 for motion vectors
 * throughout the motion estimation code but this would be damn fiddly to
 * do without introducing lots of tricky-to-find bugs.
 *
 */
 

#ifdef ORIGINAL_CODE

static void mb_me_search(
	uint8_t *org,
	uint8_t *ref,
	subsampled_mb_s *ssblk,
	int lx, int i0, int j0, 
	int sx, int sy, int h,
	int xmax, int ymax,
	mb_motion_s *res
	)
{
	mb_motion_s best;
	/* int imin, jmin, dmin */
	int i,j,ilow,ihigh,jlow,jhigh;
	int d;

	/* NOTE: Surprisingly, the initial motion estimation search
	   works better when the original image not the reference (reconstructed)
	   image is used. 
	*/
	uint8_t *s22org = (uint8_t*)(org+fsubsample_offset);
	uint8_t *s44org = (uint8_t*)(org+qsubsample_offset);
	uint8_t *orgblk;


	int flx = lx >> 1;
	int qlx = lx >> 2;
	int fh = h >> 1;
	int qh = h >> 2;

	mc_result_set sub44set;
	mc_result_set sub22set;

	/* xmax and ymax into more useful form... */
	xmax -= 16;
	ymax -= h;
  
  
  	/* The search radii are *always* multiples of 4 to avoid messiness
	   in the initial 4*4 pel search.  This is handled by the
	   parameter checking/processing code in readparmfile() */
  
	/* Create a distance-order mcomps of possible motion estimations
	  based on the fast estimation data - 4*4 pel sums (4*4
	  sub-sampled) rather than actual pel's.  1/16 the size...  */
	jlow = j0-sy;
	jlow = jlow < 0 ? 0 : jlow;
	jhigh =  j0+(sy-1);
	jhigh = jhigh > ymax ? ymax : jhigh;
	ilow = i0-sx;
	ilow = ilow < 0 ? 0 : ilow;
	ihigh =  i0+(sx-1);
	ihigh = ihigh > xmax ? xmax : ihigh;

	/*
 	   Very rarely this may fail to find matchs due to all the good
	   looking ones being over threshold. hence we make sure we
	   fall back to a 0 motion estimation in this case.
	   
		 The sad for the 0 motion estimation is also very useful as
		 a basis for setting thresholds for rejecting really dud 4*4
		 and 2*2 sub-sampled matches.
	*/
	best.sad = (*pdist1_00)(ref+i0+j0*lx,ssblk->mb,lx,h,INT_MAX);
	best.pos.x = i0;
	best.pos.y = j0;

	/* Generate the best matches at 4*4 sub-sampling. 
	   The precise fraction of the matches included is
	   controlled by ctl_44_red
	   Note: we use the original picture here for the match...
	 */
	(*pbuild_sub44_mcomps)( &sub44set,
							ilow, jlow, ihigh, jhigh,
							i0, j0,
							best.sad,
							s44org, 
							ssblk->qmb, qlx, qh );

	
	/* Generate the best 2*2 sub-sampling matches from the
	   immediate 2*2 neighbourhoods of the 4*4 sub-sampling matches.
	   The precise fraction of the matches included is controlled
	   by ctl_22_red.
	   Note: we use the original picture here for the match...

	*/

	(*pbuild_sub22_mcomps)( &sub44set, &sub22set,
							i0, j0, ihigh,  jhigh, 
							best.sad,
							s22org, ssblk->fmb, flx, fh );

		
    /* Now choose best 1-pel match from the 2*2 neighbourhoods
	   of the best 2*2 sub-sampled matches.
	   Note that here we start using the reference picture not the
	   original.
	*/
	

	(*pfind_best_one_pel)( &sub22set,
						   ref, ssblk->mb, 
						   i0, j0,
						   ilow, jlow, xmax, ymax, 
						   lx, h, &best );

	/* Final polish: half-pel search of best 1*1 against
	   reconstructed image. 
	*/

	best.pos.x <<= 1; 
	best.pos.y <<= 1;
	best.hx = 0;
	best.hy = 0;

	ilow = best.pos.x - (best.pos.x>(ilow<<1));
	ihigh = best.pos.x + (best.pos.x<((ihigh)<<1));
	jlow = best.pos.y - (best.pos.y>(jlow<<1));
	jhigh =  best.pos.y+ (best.pos.y<((jhigh)<<1));

	for (j=jlow; j<=jhigh; j++)
	{
		for (i=ilow; i<=ihigh; i++)
		{
			orgblk = ref+(i>>1)+((j>>1)*lx);
			if( i&1 )
			{
				if( j & 1 )
					d = (*pdist1_11)(orgblk,ssblk->mb,lx,h);
				else
					d = (*pdist1_01)(orgblk,ssblk->mb,lx,h);
			}
			else
			{
				if( j & 1 )
					d = (*pdist1_10)(orgblk,ssblk->mb,lx,h);
				else
					d = (*pdist1_00)(orgblk,ssblk->mb,lx,h,best.sad);
			}
			if (d<best.sad)
			{
				best.sad = d;
				best.pos.x = i;
				best.pos.y = j;
				best.blk = orgblk;
				best.hx = i&1;
				best.hy = j&1;
			}
		}
	}
	best.var = (*pdist2)(best.blk, ssblk->mb, lx, best.hx, best.hy, h);
	*res = best;
}
#else


static void mb_me_search(
	uint8_t *org,
	uint8_t *ref,
	subsampled_mb_s *ssblk,
	int lx, int i0, int j0, 
	int sx, int sy, int h,
	int xmax, int ymax,
	mb_motion_s *res
	)
{
	mc_result_s best;
	int i,j,ilow,ihigh,jlow,jhigh;
	int x,y;
	int d;

	/* NOTE: Surprisingly, the initial motion estimation search
	   works better when the original image not the reference (reconstructed)
	   image is used. 
	*/
	uint8_t *s22org = (uint8_t*)(org+fsubsample_offset);
	uint8_t *s44org = (uint8_t*)(org+qsubsample_offset);
	uint8_t *orgblk;


	int flx = lx >> 1;
	int qlx = lx >> 2;
	int fh = h >> 1;
	int qh = h >> 2;

	mc_result_set sub44set;
	mc_result_set sub22set;

	/* xmax and ymax into more useful form... */
	xmax -= 16;
	ymax -= h;
  
  
  	/* The search radii are *always* multiples of 4 to avoid messiness
	   in the initial 4*4 pel search.  This is handled by the
	   parameter checking/processing code in readparmfile() */
  
	/* Create a distance-order mcomps of possible motion estimations
	  based on the fast estimation data - 4*4 pel sums (4*4
	  sub-sampled) rather than actual pel's.  1/16 the size...  */
	jlow = j0-sy;
	jlow = jlow < 0 ? 0 : jlow;
	jhigh =  j0+(sy-1);
	jhigh = jhigh > ymax ? ymax : jhigh;
	ilow = i0-sx;
	ilow = ilow < 0 ? 0 : ilow;
	ihigh =  i0+(sx-1);
	ihigh = ihigh > xmax ? xmax : ihigh;

	/*
 	   Very rarely this may fail to find matchs due to all the good
	   looking ones being over threshold. hence we make sure we
	   fall back to a 0 motion estimation in this case.
	   
		 The sad for the 0 motion estimation is also very useful as
		 a basis for setting thresholds for rejecting really dud 4*4
		 and 2*2 sub-sampled matches.
	*/
	best.weight = (*pdist1_00)(ref+i0+j0*lx,ssblk->mb,lx,h,INT_MAX);
	best.x = 0;
	best.y = 0;

	/* Generate the best matches at 4*4 sub-sampling. 
	   The precise fraction of the matches included is
	   controlled by ctl_44_red
	   Note: we use the original picture here for the match...
	 */
	(*pbuild_sub44_mcomps)( &sub44set,
							ilow, jlow, ihigh, jhigh,
							i0, j0,
							best.weight,
							s44org, 
							ssblk->qmb, qlx, qh,
							ctl_44_red);

	
	/* Generate the best 2*2 sub-sampling matches from the
	   immediate 2*2 neighbourhoods of the 4*4 sub-sampling matches.
	   The precise fraction of the matches included is controlled
	   by ctl_22_red.
	   Note: we use the original picture here for the match...

	*/

	(*pbuild_sub22_mcomps)( &sub44set, &sub22set,
							i0, j0, 
							ihigh,  jhigh, 
							best.weight,
							s22org, ssblk->fmb, flx, fh,
							ctl_22_red);

		
    /* Now choose best 1-pel match from the 2*2 neighbourhoods
	   of the best 2*2 sub-sampled matches.
	   Note that here we start using the reference picture not the
	   original.
	*/
	

	(*pfind_best_one_pel)( &sub22set,
						   ref, ssblk->mb, 
						   i0, j0,
						   ihigh, jhigh, 
						   lx, h, &best );

	/* Final polish: half-pel search of best 1*1 against
	   reconstructed image. 
	*/
	res->sad = INT_MAX;
	x = (i0+best.x)<<1;
	y = (j0+best.y)<<1;

	/* Narrow search box to half-pel's around best 1-pel match - half-pel
	   units....*/
	ilow = x - (x>(ilow<<1));
	ihigh = x + (x<((ihigh)<<1));
	jlow = y - (y>(jlow<<1));
	jhigh =  y+ (y<((jhigh)<<1));

	for (j=jlow; j<=jhigh; j++)
	{
		for (i=ilow; i<=ihigh; i++)
		{
			orgblk = ref+(i>>1)+((j>>1)*lx);
			if( i&1 )
			{
				if( j & 1 )
					d = (*pdist1_11)(orgblk,ssblk->mb,lx,h);
				else
					d = (*pdist1_01)(orgblk,ssblk->mb,lx,h);
			}
			else
			{
				if( j & 1 )
					d = (*pdist1_10)(orgblk,ssblk->mb,lx,h);
				else
					d = (*pdist1_00)(orgblk,ssblk->mb,lx,h,res->sad);
			}
			if (d<res->sad)
			{
				res->sad = d;
				res->pos.x = i;
				res->pos.y = j;
				res->blk = orgblk;
				res->hx = i&1;
				res->hy = j&1;
			}
		}
	}
	res->var = (*pdist2)(res->blk, ssblk->mb, lx, res->hx, res->hy, h);

}

#endif
