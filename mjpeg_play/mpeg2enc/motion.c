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

/* Modifications and enhancements (C) 2000 Andrew Stevens */

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


#include <stdio.h>
#include <limits.h>
#include "config.h"
#include "global.h"
#include "math.h"
#include "cpu_accel.h"
#include "simd.h"


/* Macro-block Motion compensation results record */

typedef struct _blockcrd {
	int16_t x;
	int16_t y;
} blockxy;

struct mb_motion
{
	blockxy pos;        // Half-pel co-ordinates of source block
	int sad;			// Sum of absolute difference
	int var;
	uint8_t *blk;		// Source block data (in luminace data array)
	int hx, hy;			// Half-pel offsets
	int fieldsel;		// 0 = top 1 = bottom
};

typedef struct mb_motion mb_motion_s;

struct subsampled_mb
{
	uint8_t *mb;		// One pel
	uint8_t *fmb;		// Two-pel subsampled
	uint8_t *qmb;		// Four-pel subsampled
};

typedef struct subsampled_mb subsampled_mb_s;


/* RJ: parameter if we should not search at all */
extern int do_not_search;

/* private prototypes */

static void frame_ME _ANSI_ARGS_((pict_data_s *picture,
								  motion_comp_s *mc,
								  int mboffset,
								  int i, int j, struct mbinfo *mbi));

static void field_ME _ANSI_ARGS_((pict_data_s *picture,
								  motion_comp_s *mc,
								  int mboffset,
								  int i, int j, 
								  struct mbinfo *mbi, 
								  int secondfield, 
								  int ipflag));

static void frame_estimate (
	unsigned char *org,
	 unsigned char *ref, 
	 subsampled_mb_s *ssmb,
	 int i, int j,
	 int sx, int sy, 
	  mb_motion_s *bestfr,
	  mb_motion_s *besttop,
	  mb_motion_s *bestbot,
	 int imins[2][2], int jmins[2][2]);

static void field_estimate 
_ANSI_ARGS_((pict_data_s *picture,
	         unsigned char *toporg,
			 unsigned char *topref, 
			 unsigned char *botorg, 
			 unsigned char *botref,
			 subsampled_mb_s *ssmb,
			 int i, int j, int sx, int sy, int ipflag,
			  mb_motion_s *bestfr,
			  mb_motion_s *best8u,
			  mb_motion_s *best8l,
			  mb_motion_s *bestsp));

static void dpframe_estimate (
	pict_data_s *picture,
	unsigned char *ref,
	subsampled_mb_s *ssmb,
	int i, int j, int iminf[2][2], int jminf[2][2],
	mb_motion_s *dpbest,
	int *imindmvp, int *jmindmvp, 
	int *vmcp);

static void dpfield_estimate (
	pict_data_s *picture,
	unsigned char *topref,
	unsigned char *botref, 
	unsigned char *mb,
	int i, int j, 
	int imins, int jmins, 
	mb_motion_s *dpbest,
	int *vmcp);

static void fullsearch (
	unsigned char *org, unsigned char *ref,
	subsampled_mb_s *ssblk,
	int lx, int i0, int j0, 
	int sx, int sy, int h, 
	int xmax, int ymax,
	mb_motion_s *motion );

static int unidir_pred_var( const mb_motion_s *motion, 
							uint8_t *mb,  int lx, int h);
static int bidir_pred_var( const mb_motion_s *motion_f,  
						   const mb_motion_s *motion_b, 
						   uint8_t *mb,  int lx, int h);
static int bidir_pred_sad( const mb_motion_s *motion_f,  
						   const mb_motion_s *motion_b, 
						   uint8_t *mb,  int lx, int h);

static int variance(  uint8_t *mb, int lx);


static int fdist1 ( uint8_t *blk1, uint8_t *blk2,  int flx, int fh);
static int qdist1 ( uint8_t *blk1, uint8_t *blk2,  int qlx, int qh);
static int dist1_00( uint8_t *blk1, uint8_t *blk2,  int lx, int h, int distlim);
static int dist1_01(unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int dist1_10(unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int dist1_11(unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int dist2 _ANSI_ARGS_((unsigned char *blk1, unsigned char *blk2,
							  int lx, int hx, int hy, int h));
static int bdist2 _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
							   unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));
static int bdist1 _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
							   unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));

static int (*pfdist1) ( uint8_t *blk1, uint8_t *blk2,  int flx, int fh);
static int (*pqdist1) ( uint8_t *blk1, uint8_t *blk2,  int qlx, int qh);
static int (*pdist1_00) ( uint8_t *blk1, uint8_t *blk2,  int lx, int h, int distlim);
static int (*pdist1_01) (unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int (*pdist1_10) (unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int (*pdist1_11) (unsigned char *blk1, unsigned char *blk2, int lx, int h);

static int (*pdist2) (unsigned char *blk1, unsigned char *blk2,
					  int lx, int hx, int hy, int h);
  
  
static int (*pbdist2) (unsigned char *pf, unsigned char *pb,
					   unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int (*pbdist1) (unsigned char *pf, unsigned char *pb,
					   unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int nodual_qdist;	/* Can the qdist function return two adjacent
							   quad-pel distances in one call? */
/*
  Initialise motion compensation - currently purely selection of which
  versions of the various low level computation routines to use
  
 */

void init_motion()
{
	int cpucap = cpu_accel();

	if( cpucap  == 0 )	/* No MMX/SSE etc support available */
	{
		pfdist1 = fdist1;
		pqdist1 = qdist1;
		pdist1_00 = dist1_00;
		pdist1_01 = dist1_01;
		pdist1_10 = dist1_10;
		pdist1_11 = dist1_11;
		pbdist1 = bdist1;
		pdist2 = dist2;
		pbdist2 = bdist2;
		nodual_qdist = 1;
	}
	else if(cpucap & ACCEL_X86_MMXEXT ) /* AMD MMX or SSE... */
	{
		fprintf( stderr, "SETTING EXTENDED MMX for MOTION!\n");
		pfdist1 = fdist1_SSE;
		pqdist1 = qdist1_SSE;
		pdist1_00 = dist1_00_SSE;
		pdist1_01 = dist1_01_SSE;
		pdist1_10 = dist1_10_SSE;
		pdist1_11 = dist1_11_SSE;
		pbdist1 = bdist1_mmx;
		pdist2 = dist2_mmx;
		pbdist2 = bdist2_mmx;
		nodual_qdist = 0;
	}
	else if(cpucap & ACCEL_X86_MMX) /* Ordinary MMX CPU */
	{
		fprintf( stderr, "SETTING MMX for MOTION!\n");

		pfdist1 = fdist1_MMX;
		pqdist1 = qdist1_MMX;
		pdist1_00 = dist1_00_MMX;
		pdist1_01 = dist1_01_MMX;
		pdist1_10 = dist1_10_MMX;
		pdist1_11 = dist1_11_MMX;
		pbdist1 = bdist1_mmx;
		pdist2 = dist2_mmx;
		pbdist2 = bdist2_mmx;
		nodual_qdist = 0;
	}

}




/* 
   Match distance Thresholds for full radius motion compensation search
   Based on moving averages of fast motion compensation differences.
   If after restricted search, the fast motion compensation block
   difference is larger than this average search is expanded out to
   the user-specified radius to see if we can do better.
 */

/*
  Weighting of moving average relative to new data.  Currently set to
  around  1/2 the number of macroblocks in a frame.
 */
 
static double fast_motion_weighting;
/* 
   Reset the match accuracy threshhold used to decide whether to
   restrict the size of the the fast motion compensation search window.
 */

typedef struct _thresholdrec
{
	double fast_average;
	double slow_average;
	int updates_for_valid_slow;
	int threshold;
} thresholdrec;
	
thresholdrec twopel_threshold;
thresholdrec onepel_threshold;
thresholdrec quadpel_threshold;

#if defined(HEAPS) || defined(HALF_HEAPS)
static void update_threshold( thresholdrec *rec, int match_dist )
{
  
	rec->fast_average = (fast_motion_weighting * rec->fast_average +
						 ((double) match_dist)) / (fast_motion_weighting+1);
	/* We initialise the slow with the fast avergage */
	if ( rec->updates_for_valid_slow )
	{
		--rec->updates_for_valid_slow;
		rec->slow_average = rec->fast_average;
	}

	rec->slow_average = (100.0 *  rec->slow_average +
						 rec->fast_average) / 101.0;
	if ( rec->slow_average < rec->fast_average )
		rec->threshold = (int) (0.90 *  rec->slow_average);
	else
		rec->threshold = (int) (0.90 * rec->fast_average);

}

#endif
/*
 * Round search radius to suit the search algorithm.
 * Currently radii must be multiples of 4.
 *
 */

int round_search_radius( int radius )
{
	return ((radius+3) /4)*4;
}

/* 
   Reset the match accuracy threshhold used to decide whether to
   restrict the size of the the fast motion compensation search window.
 */

void reset_thresholds(int macroblocks_per_frame)
{
	onepel_threshold.fast_average = 5.0;
	twopel_threshold.fast_average = 5.0;
	quadpel_threshold.fast_average = 5.0;
	onepel_threshold.threshold = 5;
	twopel_threshold.threshold = 5;
	quadpel_threshold.threshold = 5;
	onepel_threshold.updates_for_valid_slow = 4*macroblocks_per_frame;
	twopel_threshold.updates_for_valid_slow = macroblocks_per_frame;
	quadpel_threshold.updates_for_valid_slow = macroblocks_per_frame;
	fast_motion_weighting = (double)2*macroblocks_per_frame;
}



/*
 * motion estimation for progressive and interlaced frame pictures
 *
 * oldorg: source frame for forward prediction (used for P and B frames)
 * neworg: source frame for backward prediction (B frames only)
 * oldref: reconstructed frame for forward prediction (P and B frames)
 * newref: reconstructed frame for backward prediction (B frames only)
 * cur:    current frame (the one for which the prediction is formed)
 * sxf,syf: forward search window (frame coordinates)
 * sxb,syb: backward search window (frame coordinates)
 * mbi:    pointer to macroblock info structure
 *
 * results:
 * mbi->
 *  mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *  MV[][][]: motion vectors (frame format)
 *  mv_field_sel: top/bottom field (for field prediction)
 *  motion_type: MC_FRAME, MC_FIELD
 *
 * uses global vars: pict_type, frame_pred_dct
 */


void motion_estimation(
	pict_data_s *picture,
	motion_comp_s *mc_data,
	int secondfield,
	int ipflag
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
				frame_ME(picture, mc_data, mb_row_start, i,j, mbi);
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
				field_ME(picture, mc_data, mb_row_start, i,j,
						 mbi,secondfield,ipflag);
				mbi++;
			}
			mb_row_start += mb_row_incr;
		}
	}
}

static void frame_ME(pict_data_s *picture,
					 motion_comp_s *mc,
					 int mb_row_start,
					 int i, int j, 
					 mbinfo_s *mbi)
{
	mb_motion_s framef_mc;
	mb_motion_s frameb_mc;
	mb_motion_s dualpf_mc;
	mb_motion_s topfldf_mc;
	mb_motion_s botfldf_mc;
	mb_motion_s topfldb_mc;
	mb_motion_s botfldb_mc;


	int var,v0;
	int dmc,dmcf,dmcr,dmci,vmc,vmcf,vmcr,vmci;
	int dmcfield,dmcfieldf,dmcfieldr,dmcfieldi;
	subsampled_mb_s ssmb;
	int imins[2][2],jmins[2][2];
	int imindmv,jmindmv,dmc_dp,vmc_dp;
	

	/* A.Stevens fast motion estimation data is appended to actual
	   luminance information 
	*/
	ssmb.mb = mc->cur + mb_row_start + i;
	ssmb.fmb = (uint8_t*)(mc->cur + fsubsample_offset + (i>>1) + (mb_row_start>>2));
	ssmb.qmb = (uint8_t*)(mc->cur + qsubsample_offset + (i>>2) + (mb_row_start>>4));
	var = variance(ssmb.mb,width);

	if (picture->pict_type==I_TYPE)
	{
		mbi->mb_type = MB_INTRA;
	}
	else if (picture->pict_type==P_TYPE)
	{
		if (picture->frame_pred_dct)
		{
			fullsearch(mc->oldorg,mc->oldref,&ssmb,
					   width,i,j,mc->sxf,mc->syf,16,width,height,
					    &framef_mc);
			dmc = framef_mc.sad;
			vmc = unidir_pred_var( &framef_mc, ssmb.mb, width, 16);
			mbi->motion_type = MC_FRAME;
		}
		else
		{
			frame_estimate(mc->oldorg,mc->oldref,&ssmb,
						   i,j,mc->sxf,mc->syf,
						   &framef_mc,
						   &topfldf_mc,
						   &botfldf_mc,
						   imins,jmins);
			dmc = framef_mc.sad;
			dmcfield = topfldf_mc.sad + botfldf_mc.sad;
			
			if (M==1)
				dpframe_estimate(picture,mc->oldref,&ssmb,
								 i,j>>1,imins,jmins,
								 &dualpf_mc,
								 &imindmv,&jmindmv, &vmc_dp);
			dmc_dp = dualpf_mc.sad;
			/* select between dual prime, frame and field prediction */
			if (M==1 && dmc_dp<dmc && dmc_dp<dmcfield)
			{
				mbi->motion_type = MC_DMV;
				dmc = dmc_dp;
				vmc = vmc_dp;
			}
			else if (dmc<=dmcfield)
			{
				mbi->motion_type = MC_FRAME;
				vmc = unidir_pred_var( &framef_mc, ssmb.mb, width,16);
			}
			else
			{
				mbi->motion_type = MC_FIELD;
				dmc = dmcfield;
				vmc =  unidir_pred_var( &topfldf_mc, ssmb.mb, width<<1, 8);
				vmc += unidir_pred_var( &botfldf_mc, ssmb.mb, width<<1, 8);
			}
		}


		/* select between intra or non-intra coding:
		 *
		 * selection is based on intra block variance (var) vs.
		 * prediction error variance (vmc)
		 *
		 * Used to be: blocks with small prediction error are always 
		 * coded non-intra even if variance is smaller (is this reasonable?
		 *
		 * TODO: A.Stevens Jul 2000
		 * The bbmpeg guys have found this to be *unreasonable*.
		 * I'm not sure I buy their solution using vmc*2.  It is probabably
		 * the vmc>= 9*256 test that is suspect.
		 * 
		 */
		if (vmc>var /* && vmc>=9*256*/ )
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
			v0 = (*pdist2)(mc->oldref+i+width*j,ssmb.mb,width,0,0,16);

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
				mbi->MV[0][0][0] = framef_mc.pos.x - (i<<1);
				mbi->MV[0][0][1] = framef_mc.pos.y - (j<<1);
			}
		}
	}
	else /* if (pict_type==B_TYPE) */
	{
		if (picture->frame_pred_dct)
		{
			/* forward */
			fullsearch(mc->oldorg,mc->oldref,&ssmb,
					   width,i,j,mc->sxf,mc->syf,
					   16,width,height,
					   &framef_mc
					   );
			dmcf = framef_mc.sad;
			vmcf = unidir_pred_var( &framef_mc, ssmb.mb, width, 16);

			/* backward */
			fullsearch(mc->neworg,mc->newref,&ssmb,
					   width,i,j,mc->sxb,mc->syb,
					   16,width,height,
					   &frameb_mc);
			dmcr = frameb_mc.sad;
			vmcr = unidir_pred_var( &frameb_mc, ssmb.mb, width, 16 );

			/* interpolated (bidirectional) */

			vmci = bidir_pred_var( &framef_mc, &frameb_mc, ssmb.mb, width, 16 );

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
			/* forward prediction */
			frame_estimate(mc->oldorg,mc->oldref,&ssmb,
						   i,j,mc->sxf,mc->syf,
						   &framef_mc,
						   &topfldf_mc,
						   &botfldf_mc,
						   imins,jmins);
			dmcf = framef_mc.sad;
			dmcfieldf = topfldf_mc.sad + botfldf_mc.sad;
			/* backward prediction */
			frame_estimate(mc->neworg,mc->newref,&ssmb,
						   i,j,mc->sxb,mc->syb,
						   &frameb_mc,
						   &topfldb_mc,
						   &botfldb_mc,
						   imins,jmins);
			dmcr = frameb_mc.sad;
			dmcfieldr = topfldb_mc.sad + botfldb_mc.sad;

			/* calculate interpolated distance */
			/* frame */
			dmci = bidir_pred_sad( &framef_mc, &frameb_mc, ssmb.mb, width, 16);
			/* top and bottom fields */
			dmcfieldi = bidir_pred_sad( &topfldf_mc, &topfldb_mc, ssmb.mb, 
										width<<1, 8);
			dmcfieldi+= bidir_pred_sad( &botfldf_mc, &botfldb_mc, ssmb.mb, 
										width<<1, 8);

			/* select prediction type of minimum distance from the
			 * six candidates (field/frame * forward/backward/interpolated)
			 */
			if (dmci<dmcfieldi && dmci<dmcf && dmci<dmcfieldf
				&& dmci<dmcr && dmci<dmcfieldr)
			{
				/* frame, interpolated */
				mbi->mb_type = MB_FORWARD|MB_BACKWARD;
				mbi->motion_type = MC_FRAME;
				vmc = bidir_pred_var( &framef_mc, &frameb_mc, ssmb.mb, width, 16);
			}
			else if (dmcfieldi<dmcf && dmcfieldi<dmcfieldf
					 && dmcfieldi<dmcr && dmcfieldi<dmcfieldr)
			{
				/* field, interpolated */
				mbi->mb_type = MB_FORWARD|MB_BACKWARD;
				mbi->motion_type = MC_FIELD;
				vmc =  bidir_pred_var( &topfldf_mc, &topfldb_mc, ssmb.mb, width<<1, 8);
				vmc += bidir_pred_var( &botfldf_mc, &botfldb_mc, ssmb.mb, width<<1, 8);
			}
			else if (dmcf<dmcfieldf && dmcf<dmcr && dmcf<dmcfieldr)
			{
				/* frame, forward */
				mbi->mb_type = MB_FORWARD;
				mbi->motion_type = MC_FRAME;
				vmc = unidir_pred_var( &framef_mc, ssmb.mb, width, 16);
			}
			else if (dmcfieldf<dmcr && dmcfieldf<dmcfieldr)
			{
				/* field, forward */
				mbi->mb_type = MB_FORWARD;
				mbi->motion_type = MC_FIELD;
				vmc =  unidir_pred_var( &topfldf_mc, ssmb.mb, width<<1, 8);
				vmc += unidir_pred_var( &botfldf_mc, ssmb.mb, width<<1, 8);

			}
			else if (dmcr<dmcfieldr)
			{
				/* frame, backward */
				mbi->mb_type = MB_BACKWARD;
				mbi->motion_type = MC_FRAME;
				vmc = unidir_pred_var( &frameb_mc, ssmb.mb, width, 16);
			}
			else
			{
				/* field, backward */
				mbi->mb_type = MB_BACKWARD;
				mbi->motion_type = MC_FIELD;
				vmc =  unidir_pred_var( &topfldb_mc, ssmb.mb, width<<1, 8);
				vmc += unidir_pred_var( &botfldb_mc, ssmb.mb, width<<1, 8);

			}
		}

		/* select between intra or non-intra coding:
		 *
		 * selection is based on intra block variance (var) vs.
		 * prediction error variance (vmc)
		 *
		 * Used to be: blocks with small prediction error are always 
		 * coded non-intra even if variance is smaller (is this reasonable?
		 *
		 * TODO: A.Stevens Jul 2000
		 * The bbmpeg guys have found this to be *unreasonable*.
		 * I'm not sure I buy their solution using vmc*2 in the first comparison.
		 * It is probabably the vmc>= 9*256 test that is suspect.
		 *
		 */
		if (vmc>var && vmc>=9*256)
			mbi->mb_type = MB_INTRA;
		else
		{
			var = vmc;
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
				mbi->MV[0][1][1] = (topfldb_mc.pos.x<<1) - (j<<1);
				mbi->MV[1][1][0] = botfldb_mc.pos.x - (i<<1);
				mbi->MV[1][1][1] = (botfldb_mc.pos.y<<1) - (j<<1);
				mbi->mv_field_sel[0][1] = topfldb_mc.fieldsel;
				mbi->mv_field_sel[1][1] = botfldb_mc.fieldsel;
			}
		}
	}

	mbi->var = var;


}

/*
 * motion estimation for field pictures
 *
 * mbi:    pointer to macroblock info structure
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
 * uses global vars: pict_type, pict_struct
 */
static void field_ME(
	pict_data_s *picture,
	motion_comp_s *mc,
	int mb_row_start,
	int i, int j, 
	mbinfo_s *mbi, 
	int secondfield, int ipflag)
{
	int w2;
	unsigned char *toporg, *topref, *botorg, *botref;
	subsampled_mb_s ssmb;
	mb_motion_s fields_mc, dualp_mc;
	mb_motion_s fieldf_mc, fieldb_mc;
	mb_motion_s field8uf_mc, field8lf_mc;
	mb_motion_s field8ub_mc, field8lb_mc;
	int var,vmc,v0,dmc,dmcfieldi,dmcfield,dmcfieldf,dmcfieldr,dmc8i;
	/* int imin,jmin,imin8u,jmin8u,imin8l,jmin8l,sel,sel8u,sel8l;
	int iminf,jminf,imin8uf,jmin8uf,imin8lf,jmin8lf,dmc8f,self,sel8uf,sel8lf;
	int iminr,jminr,imin8ur,jmin8ur,imin8lr,jmin8lr,dmc8rdmc8r,selr,sel8ur,sel8lr; */
	int imins,jmins;
	int dmc8f,dmc8r,ds;
	/* int imindmv,jmindmv;*/
	int vmc_dp,dmc_dp;

	w2 = width<<1;

	/* Fast motion data sits at the end of the luminance buffer */
	ssmb.mb = mc->cur + i + w2*j;
	ssmb.fmb = ((uint8_t*)(mc->cur + fsubsample_offset))+(i>>1)+(w2>>1)*(j>>1);
	ssmb.qmb = ((uint8_t*)(mc->cur + qsubsample_offset))+ (i>>2)+(w2>>2)*(j>>2);
	if (picture->pict_struct==BOTTOM_FIELD)
	{
		ssmb.mb += width;
		ssmb.fmb += (width >> 1);
		ssmb.qmb += (width >> 2);
	}


	var = variance(ssmb.mb,w2);

	if (picture->pict_type==I_TYPE)
		mbi->mb_type = MB_INTRA;
	else if (picture->pict_type==P_TYPE)
	{
		toporg = mc->oldorg;
		topref = mc->oldref;
		botorg = mc->oldorg + width;
		botref = mc->oldref + width;

		if (secondfield)
		{
			/* opposite parity field is in same frame */
			if (picture->pict_struct==TOP_FIELD)
			{
				/* current is top field */
				botorg = mc->cur + width;
				botref = mc->curref + width;
			}
			else
			{
				/* current is bottom field */
				toporg = mc->cur;
				topref = mc->curref;
			}
		}

		field_estimate(picture,
					   toporg,topref,botorg,botref,&ssmb,
					   i,j,mc->sxf,mc->syf,ipflag,
					   &fieldf_mc,
					   &field8uf_mc,
					   &field8lf_mc,
					   &fields_mc);
		dmcfield = fieldf_mc.sad;
		dmc8f = field8uf_mc.sad + field8lf_mc.sad;
		ds = fields_mc.sad;

		if (M==1 && !ipflag)  /* generic condition which permits Dual Prime */
		{
			dpfield_estimate(picture,
							 topref,botref,ssmb.mb,i,j,imins,jmins,
							 &dualp_mc,
							 &vmc_dp);
			dmc_dp = dualp_mc.sad;
		}
		
		/* select between dual prime, field and 16x8 prediction */
		if (M==1 && !ipflag && dmc_dp<dmc8f && dmc_dp<dmcfield)
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
			vmc =  unidir_pred_var( &field8uf_mc, ssmb.mb, w2, 8);
			vmc += unidir_pred_var( &field8lf_mc, ssmb.mb, w2, 8);
		}
		else
		{
			/* field prediction */
			mbi->motion_type = MC_FIELD;
			vmc = unidir_pred_var( &fieldf_mc, ssmb.mb, w2, 16 );
		}

		/* select between intra and non-intra coding */
		if (vmc>var && vmc>=9*256)
			mbi->mb_type = MB_INTRA;
		else
		{
			/* zero MV field prediction from same parity ref. field
			 * (not allowed if ipflag is set)
			 */
			if (!ipflag)
				v0 = (*pdist2)(((picture->pict_struct==BOTTOM_FIELD)?botref:topref) + i + w2*j,
							   ssmb.mb,w2,0,0,16);
			if (ipflag || (4*v0>5*vmc && v0>=9*256))
			{
				var = vmc;
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
					mbi->MV[0][0][0] = imins - (i<<1);
					mbi->MV[0][0][1] = jmins - (j<<1);

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
				var = v0;
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
					   mc->oldorg,mc->oldref,
					   mc->oldorg+width,mc->oldref+width,&ssmb,
					   i,j,mc->sxf,mc->syf,0,
					   &fieldf_mc,
					   &field8uf_mc,
					   &field8lf_mc,
					   &fields_mc);
		dmcfieldf = fieldf_mc.sad;
		dmc8f = field8uf_mc.sad + field8lf_mc.sad;
		/* TODO: ds is probably a dummy in this case... */
		ds = fields_mc.sad;

		/* backward prediction */
		field_estimate(picture,
					   mc->neworg,mc->newref,mc->neworg+width,mc->newref+width,
					   &ssmb,
					   i,j,mc->sxb,mc->syb,0,
					   &fieldb_mc,
					   &field8ub_mc,
					   &field8lb_mc,
					   &fields_mc);
		dmcfieldr = fieldb_mc.sad;
		dmc8r = field8ub_mc.sad + field8lb_mc.sad;
		ds = fields_mc.sad;

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
			vmc = bidir_pred_var( &fieldf_mc, &fieldb_mc, ssmb.mb, w2, 16);
		}
		else if (dmc8i<dmcfieldf && dmc8i<dmc8f
				 && dmc8i<dmcfieldr && dmc8i<dmc8r)
		{
			/* 16x8, interpolated */
			mbi->mb_type = MB_FORWARD|MB_BACKWARD;
			mbi->motion_type = MC_16X8;

			/* upper and lower half blocks */
			vmc =  bidir_pred_var( &field8uf_mc, &field8ub_mc, ssmb.mb, w2, 8);
			vmc += bidir_pred_var( &field8lf_mc, &field8lb_mc, ssmb.mb, w2, 8);
		}
		else if (dmcfieldf<dmc8f && dmcfieldf<dmcfieldr && dmcfieldf<dmc8r)
		{
			/* field, forward */
			mbi->mb_type = MB_FORWARD;
			mbi->motion_type = MC_FIELD;
			vmc = unidir_pred_var( &fieldf_mc, ssmb.mb, w2, 16);
		}
		else if (dmc8f<dmcfieldr && dmc8f<dmc8r)
		{
			/* 16x8, forward */
			mbi->mb_type = MB_FORWARD;
			mbi->motion_type = MC_16X8;

			/* upper and lower half blocks */
			vmc =  unidir_pred_var( &field8uf_mc, ssmb.mb, w2, 8);
			vmc += unidir_pred_var( &field8lf_mc, ssmb.mb, w2, 8);
		}
		else if (dmcfieldr<dmc8r)
		{
			/* field, backward */
			mbi->mb_type = MB_BACKWARD;
			mbi->motion_type = MC_FIELD;
			vmc = unidir_pred_var( &fieldb_mc, ssmb.mb, w2, 16 );
		}
		else
		{
			/* 16x8, backward */
			mbi->mb_type = MB_BACKWARD;
			mbi->motion_type = MC_16X8;

			/* upper and lower half blocks */
			vmc =  unidir_pred_var( &field8ub_mc, ssmb.mb, w2, 8);
			vmc += unidir_pred_var( &field8lb_mc, ssmb.mb, w2, 8);

		}

		/* select between intra and non-intra coding */
		if (vmc>var && vmc>=9*256)
			mbi->mb_type = MB_INTRA;
		else
		{
			var = vmc;
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
	mbi->var = var;
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
	unsigned char *org,
	unsigned char *ref,
	subsampled_mb_s *ssmb,
	int i, int j, int sx, int sy,
	mb_motion_s *bestfr,
	mb_motion_s *besttop,
	mb_motion_s *bestbot,
	int imins[2][2],
	int jmins[2][2]
	)
{
	subsampled_mb_s  botssmb;
	mb_motion_s topfld_mc;
	mb_motion_s botfld_mc;

	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	botssmb.mb = ssmb->mb+width;
	botssmb.fmb = ssmb->mb+(width>>1);
	botssmb.qmb = ssmb->qmb+(width>>2);
	
	/* frame prediction */
	fullsearch(org,ref,ssmb,width,i,j,sx,sy,16,width,height,
						  bestfr );

	/* predict top field from top field */
	fullsearch(org,ref,ssmb,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
			   &topfld_mc);

	/* predict top field from bottom field */
	fullsearch(org+width,ref+width,ssmb, width<<1,i,j>>1,sx,sy>>1,8,
			   width,height>>1, &botfld_mc);

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
	fullsearch(org,ref,&botssmb,
					width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
					&topfld_mc);

	/* predict bottom field from bottom field */
	fullsearch(org+width,ref+width,&botssmb,
					width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
					&botfld_mc);

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
	unsigned char *toporg,
	unsigned char *topref, 
	unsigned char *botorg, 
	unsigned char *botref,
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
	botssmb.fmb = ssmb->fmb+(width>>1);
	botssmb.qmb = ssmb->qmb+(width>>2);
	
	topfld_mc.fieldsel = 0;
	botfld_mc.fieldsel = 1;
	/* if ipflag is set, predict from field of opposite parity only */
	notop = ipflag && (picture->pict_struct==TOP_FIELD);
	nobot = ipflag && (picture->pict_struct==BOTTOM_FIELD);

	/* field prediction */

	/* predict current field from top field */
	if (notop)
		topfld_mc.sad = dt = 65536; /* infinity */
	else
		fullsearch(toporg,topref,ssmb,width<<1,
				   i,j,sx,sy>>1,16,width,height>>1,
				   &topfld_mc);
	dt = topfld_mc.sad;
	/* predict current field from bottom field */
	if (nobot)
		botfld_mc.sad = db = 65536; /* infinity */
	else
		fullsearch(botorg,botref,ssmb,width<<1,
				   i,j,sx,sy>>1,16,width,height>>1,
				   &botfld_mc);
	db = botfld_mc.sad;

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


	/* 16x8 motion compensation */

	/* predict upper half field from top field */
	if (notop)
		topfld_mc.sad = dt = 65536;
	else
		fullsearch(toporg,topref,ssmb,width<<1,
				   i,j,sx,sy>>1,8,width,height>>1,
				    &topfld_mc);
	dt = topfld_mc.sad;
	/* predict upper half field from bottom field */
	if (nobot)
		botfld_mc.sad = db = 65536;
	else
		fullsearch(botorg,botref,ssmb,width<<1,
				   i,j,sx,sy>>1,8,width,height>>1,
				    &botfld_mc);
	db = botfld_mc.sad;
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
		fullsearch(toporg,topref,&botssmb,
						width<<1,
						i,j+8,sx,sy>>1,8,width,height>>1,
				   /* &imint,&jmint, &dt,*/ &topfld_mc);
	dt = topfld_mc.sad;
	/* predict lower half field from bottom field */
	if (nobot)
		botfld_mc.sad = db = 65536;
	else
		fullsearch(botorg,botref,&botssmb,width<<1,
						i,j+8,sx,sy>>1,8,width,height>>1,
				   /* &iminb,&jminb, &db,*/ &botfld_mc);
	db = botfld_mc.sad;
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
	unsigned char *ref,
	subsampled_mb_s *ssmb,
	
	int i, int j, int iminf[2][2], int jminf[2][2],
	mb_motion_s *best_mc,
	int *imindmvp, int *jmindmvp,
	int *vmcp)
{
	int pref,ppred,delta_x,delta_y;
	int is,js,it,jt,ib,jb,it0,jt0,ib0,jb0;
	int imins,jmins,imint,jmint,iminb,jminb,imindmv,jmindmv;
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
	unsigned char *topref,
	unsigned char *botref, 
	unsigned char *mb,
	int i, int j, int imins, int jmins, 
	mb_motion_s *bestdp_mc,
	int *vmcp
	)

{
	unsigned char *sameref, *oppref;
	int io0,jo0,io,jo,delta_x,delta_y,mvxs,mvys,mvxo0,mvyo0;
	int imino,jmino,imindmv,jmindmv,vmc_dp,local_dist;

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
	vmc_dp = 1 << 30;

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
	bestdp_mc->pos.x = imindmv;
	*vmcp = vmc_dp;
}

/*
 * Heaps of distance estimates for motion compensation search
 */

/* A.Stevens 2000: WARNING: distance measures must fit into int16_t 
   or bad things will happen! */
struct _sortelt 
{
	uint16_t weight;
	uint16_t index;
};

typedef struct _sortelt sortelt;




#define childl(x) (x+x+1)
#define childr(x) (x+x+2)
#define parent(x) ((x-1)>>1)

/*
 *   Insert a new element into an existing heap.  Included only for
 * completeness.  Not used.
 */

void heap_insert( sortelt heap[], int *heapsize, sortelt *newelt )
{
	int i;

	++(*heapsize);
	i = (*heapsize);

	while( i > 0 && heap[parent(i)].weight > newelt->weight)
	{
		heap[i] = heap[parent(i)];
		i = parent(i);
	}
	heap[i] = *newelt;
}


/*
 *  Standard extract-smallest-element from heap and maintain heap property
 *  procedure.
 */
void heap_extract( sortelt *heap, int *heapsize, sortelt *extelt )
{
	int i,j,s;
	s = (*heapsize);


	if( s == 0 )
		abort();
	--(*heapsize);
	*extelt = heap[0];
	s = s - 1;

	i = 0; 
	j = childl(i); 
	while( j < s )
	{
		register int w = s;
		/* ORIGINAL slow branchy code
		   N.b. childl(x) = x + (x+1) childr(x) = x + (x+2)
		   if( heap[w].weight > heap[j].weight )
		   w = j;
		   if( j+1 < s && heap[w].weight > heap[j+1].weight )
		   w = j+1;
		*/

		w += (j-w)&(-(heap[w].weight > heap[j].weight));
		++j;
		w += (j-w)&-(((j < s) & (heap[w].weight > heap[j].weight )));

		if( w == s )
			break;
		heap[i] = heap[w];
		i = w;
		j = childl(i);
	}  
  
	heap[i] = heap[s];
}

/*
 *   Why bother studying Comp. Sci.?  Well maybe building a heap in linear
 *   time might convince some...
 */

void heapify( sortelt *heap, int heapsize )
{
	int base = 0;
	sortelt tmp;
	int i;
	if( heapsize == 0)
		return;
	do
	{
		base = childl(base);
	} 
	while( childl(base) < heapsize );
  
	do {
		base = parent(base);
		for( i = base; i < childl(base); ++i )
		{
			int j = i;
			int k = childl(i);

			while( k < heapsize )
			{
				register int w = j;
				/* ORIGINAL slow branchy code 
				   N.b. childl(x) = x + (x+1) childr(x) = x + (x+2) 
				   if( heap[j].weight > heap[k].weight )
				   w = k;
				   if( k+1 < heapsize && heap[w].weight > heap[k+1].weight )
				   w = k+1;
				*/

				w += (k-w)&(-(heap[j].weight > heap[k].weight));
				++k;
				w += ((k)-w)&-(((k < heapsize) & (heap[w].weight > heap[k].weight )));

				if( w == j )
					break;
				tmp = heap[j];
				heap[j] = heap[w];
				heap[w] = tmp;

				j = w;
				k = childl(j);
			}
		}
	} while( base != 0 );

}

void test_heap( sortelt *heap, int heapsize, char *mesg, int n )
{
	int i = 0;
	while( childr(i) < heapsize )
	{
		if( heap[childr(i)].weight < heap[i].weight ||
			heap[childl(i)].weight < heap[i].weight )
		{
			printf( "Fails I heap property: %s at %d pw=%d cl=%d cr=%d!\n", mesg, i,
					heap[i].weight, heap[childl(i)].weight, heap[childr(i)].weight );
			exit(1);
		}
		++i;
	}
	if( childl(i) < heapsize && heap[childl(i)].weight < heap[i].weight )
	{
		printf( "Fails L heap property: %s!\n", mesg );
		exit(1);
	}
}


#undef childl
#undef childr
#undef parent

#ifdef HALF_HEAPS

static int thin_vector( sortelt vec[], int threshold, int len )
{
 	int i;
	int j = 0;
	for( i = 0; i < len; ++i )
	{
		if( vec[i].weight <= threshold )
		{
			vec[j] = vec[i];
			++j;
		}
	}
	return j;
}
#endif




#ifdef TEST_GRAD_DESCENT

/*
	A table for cacheing already searched locations to allow search to be aborted once
	existing descent paths have been found
	CAUTION: Depends on maximum search radius permitted...
	*/

/* TODO: BUG: CAUTION: Presupposes motion compensation radii of > 256 don't occur */


#define LG_MAX_SEARCH_RADIUS 8
#define MAX_SEARCH_RADIUS (1<<LG_MAX_SEARCH_RADIUS)
#define SEARCH_TABLE_SIZE (MAX_SEARCH_RADIUS*MAX_SEARCH_RADIUS)

typedef struct _searchtablerec 
{
	int16_t tag;
	int16_t weight;
} searchtablerec;
	
static int16_t searchtable_fresh_ctr = 0xac45;

static searchtablerec searchtable[SEARCH_TABLE_SIZE];
#define searchentry(x,y) ( searchtable[(x + (y<<LG_MAX_SEARCH_RADIUS)) & (SEARCH_TABLE_SIZE-1)])
#define isfreshentry(x,y) ( searchentry(x,y).tag == searchtable_fresh_ctr  )
#define freshensearchtable searchtable_fresh_ctr++
#define freshsearchtableentry(x,y,w) { searchentry(x,y).tag = searchtable_fresh_ctr;\
                                       searchentry(x,y).weight = w; }
                                        

/*
	Shiny new distance measure absolute difference sum of row/col sums

*/

static int rcdist( unsigned char *org, uint16_t* blksums, int lx, int h )
{
	int i,j;
	unsigned char *porg; 
	int dist = 0;
	int rowsum;
	uint16_t *blkrowsums = &blksums[0];
	uint16_t *blkcolsums = &blksums[h];
	int colsums[16];
	

	porg = org;
	for( i = 0; i <16; ++i )
		colsums[i] = 0;
	for( j = 0; j < h; ++j )
	{
		rowsum = 0;
		for( i = 0; i < 16; ++i )
		{
			rowsum += porg[i];
			colsums[i] += porg[i];
		}
		dist += abs(rowsum-blkrowsums[j]);
		porg += lx;
	}
	
	
	for( i = 0; i < 16; ++i )
		dist += abs(colsums[i]-blkcolsums[i]);

	return dist;
}

static int rcdist1( uint16_t *orgrsums, uint16_t *orgcsums, 
                    uint16_t *blkrsums, uint16_t *blkcsums, int h )
{
	/* BUG: We ignore h and assume it is 16 since the code doesn't work for
	   non 16 h anyway. THus, *THIS CODE WILL NOT WORK FOR FIELD BASED DATA
	*/
	int i;
	int dist = 0;
	for( i = 0; i < 16; ++i )
	{
		dist += abs(orgrsums[i]-blkrsums[i]) + abs(orgcsums[i]-blkcsums[i]);
	}
	return dist;
}

static void rcsums( unsigned char *org, uint16_t *sums, int lx, int h)
{
	int i,j;
	unsigned char *porg;
	int rowsum;
	uint16_t *rowsums = &sums[0];
	uint16_t *colsums = &sums[h];
	

	for( i = 0; i <16; ++i )
		colsums[i] = 0;
	porg = org;
	for( j = 0; j < h; ++j )
	{
		rowsum = 0;
		for( i = 0; i < 16; ++i )
		{
			rowsum += porg[i];
			colsums[i] += porg[i];
		}
		rowsums[j] = rowsum;
		porg += lx;
	}
}


/*
	local_optimum
	Do a 2D binary search for a locally optimal row/column sum difference 
	distance match in the 16-pel box right/below initx inity
	
	TODO: Lots of performance tweaks in the code's structure not all documented...

*/


static int rcdist_table( unsigned char *org, int16_t* blksums, 
						 int x, int y, int lx, int h, int (*pdist) )
{
	int i,j;
	unsigned char *porg, *pblk;
	int dist = 0;
	int rso, rsb;
	int cso[16], csb[16];
	
	if( isfreshentry(x,y) )
	{
		(*pdist) = searchentry(x,y).weight;
		return 1;
	}
	porg = org;
	pblk = blk;
	for( i = 0; i <16; ++i )
		cso[i] = csb[i] = 0;
	for( j = 0; j < h; ++j )
	{
		rso = rsb = 0;
		for( i = 0; i < 16; ++i )
		{
			rso += porg[i];
			cso[i] += porg[i];
			rsb += pblk[i];
			csb[i] += pblk[i];
				
		}
		dist += abs(rso-rsb);
		porg += lx;
		pblk += lx;
	}
		
	for( i = 0; i < 16; ++i )
		dist += abs(cso[i]-csb[i]);
	
	freshsearchtableentry(x,y, dist);
	(*pdist) = dist;
	return 0;

}

static void local_optimum( unsigned char *org, unsigned char *blk, 
						   int initx, int inity,
						   int xlow, int xhigh,
						   int ylow, int yhigh,
						   int threshold,
						   int lx, int h, int *pbestd, int *pbestx, int *pbesty )
{
	unsigned char *porg;
	int deltax = 16 / 2;
	int deltay = h /2;
	int bestdx;
	int bestdy;
	int x = initx;
	int y = inity;
	int bestdist = 1234567; 
	int dist;
	int allstale,stale;
		
	porg = org+(x)+lx*(y);
			
	allstale = rcdist( porg, blk, x, y, lx, h, &bestdist);
	while( (deltax+deltay > 2))  /* zero in to the 1*1 pel match level */
	{
		/*
		  We've already covered the "no movement" option in the previous iteration or during
		  initialisation.
		*/
		bestdx = 0;
		bestdy = 0;
			
		/* Try the four quadrants and choose the best 
		 */
		if( deltax > 1 )
		{
			stale = rcdist_table( porg + deltax, blk, x+deltax, y, lx, h, &dist );
			allstale &= stale;
			if( !stale && dist < bestdist && x+deltax <= xhigh)
			{
				bestdist = dist;
				bestdx = deltax;
				bestdy = 0;
			}
			stale = rcdist( porg - deltax, blk, x-deltax, y, lx, h, &dist );
			allstale &= stale;	
			if( !stale && dist < bestdist && x-deltax >= xlow)
			{
				bestdist = dist;
				bestdx = -deltax;
				bestdy = 0;
			}

		}
			
		if( deltay > 1 )	
		{
			stale = rcdist_table( porg + deltay*lx, blk, x, y+deltay, lx, h, &dist );
			allstale &= stale;	
			if( !stale && dist < bestdist && y+deltay <= yhigh)
			{
				bestdist = dist;
				bestdx = 0;
				bestdy = deltay;
			}
			stale = rcdist_table( porg - deltay*lx, blk, x, y-deltay, lx, h, &dist );
			allstale &= stale;
			if( !stale && dist < bestdist && x-deltay >= ylow )
			{
				bestdist = dist;
				bestdx = 0;
				bestdy = -deltay;
			}

			
		}
			

		/*printf( "SEARCH X=%d Y=%d DX=%d DY=%d BDX=%d BDY=%d D=%d\n",
		  x, y, deltax, deltay, bestdx, bestdy, bestdist );
		*/
		/* Heuristic: if neither quadrant works we simply assume they're overshoots.  We assume
		   diagonals need not be considered because it would make a quadrant better than the current....
		*/
			
		x += bestdx;
		porg += bestdx;
		y += bestdy;
		porg += lx*bestdy;
			
		/* Look closer if not different position is better */
		if( bestdx == 0 && bestdy == 0 )
		{
			/* int unchangeable_x, unchangeable_y; */
			deltax /= 2;
			deltay /= 2;
			/* As a a rule of thumb, once the search narrows we are stuck with some of the match.
			   If both delta's 1/2 of match block then we're stuck with 1/4.
			   If both delta's are 1/4 then we're stuck with 3/4's
			   However, since its a fudge we add a 25% safety margin to allow for multi-step shifts.
			   We don't actually bother doing any hard sums since we hit here when we're already at the
			   1/4 stage which is good enough for an approximation...
			*/
			/* unchangeable_x = 16 - deltax;
			   unchangeable_y = h - deltay; */
			if( bestdist / 4 > threshold+(threshold>>2) )
			{
				/*printf("Threshold abort!\n");*/
				break;
			}
		}

		/* If we have considered no fresh locations we also abort the search early  
		   One refinement might be to take into account search radius...
		*/
		if( allstale )
		{
			/*printf( "Staleness abort\n" );*/
			break;
		}
		allstale = 1;
	}

	*pbestx = x;
	*pbesty = y;
	*pbestd = bestdist;
			
}

/*
	best_graddesc_match
	
	Find a good motion compensation search based on local optima in row-col sums.
	
	TODO: This *not* the right way to do this.  A priority queue based approach
	"seeded" with the start 24-pitch grid would probably work much better
	as it would allow a much efficient means of increasing precision in the event
	of no good match being found first time around... 
*/


static void best_graddesc_match( int ilow, int ihigh, int jlow, int jhigh, 
								 unsigned char *org, uint8_t *forg, 
								 unsigned char *blk,  uint8_t *fblk, 
								 int lx, int h, 
								 int *pdmin, int *pimin, int *pjmin )
{
  	unsigned char *orgblk;
	int i,j;
	int bestrcdist = 256*255;		/* Can't be lazy and use INT_MAX as it gets used in a thresholding
									   calculation and we don't want to cause overflows */
	int bestdist = 7891011;
	int bestx, besty;
	int dist,x,y;
	int d;
	int onepel_radius;
		
	freshensearchtable;		/* Ensure we can recognise fresh entries in the search cache */

	/* Find the most promising starting point in the search box */

	for( j = jlow+8; j <= jhigh; j += 24 )
	{
		for( i = ilow+8; i <= ihigh; i += 24 )
		{
			/*printf( "Starting candidate %d %d \n", i, j );*/
			local_optimum( org, 	blk, i, j, ilow, ihigh, jlow, jhigh, bestrcdist, lx, h, &dist, &x, &y );
			if( dist < bestrcdist )
			{
				bestrcdist = dist;
			}
			/* Now compute 1*1 pel distance measure (we could even try a DCT here  for real accuracy... *) 
			   And use that as the basis for our final selection of the best match position.
			*/
			dist = (*pdist1_)00(org+x+y*lx, blk, lx, h, bestdist);
			if ( dist < bestdist  )
			{
				bestdist = dist;
				bestx = x;
				besty = y;
				if( bestdist < onepel_threshold.threshold )
				{
					i = ihigh;
					j = jhigh;
				}
			}
		}
	}
		
	/* We now try to figure out how hard to look for alternative matches based on the crumminess of
	   the results we have obtained...
	*/
	onepel_radius = bestdist / onepel_threshold.threshold ;
		
	/* We're already good we simply look for a bit of polish */
	if( onepel_radius < 1 )
		onepel_radius = 1;
		
	/* If its really bad we simply search the whole box using a coarse 2*2 pel search first and pick out 
	   the best local area and then zero in with 1*1 pel search.
	*/
		
	if( onepel_radius > 3 )
	{
		int flx = lx / 2;
		int fh = h / 2;
		uint8_t *forgblk;
		int bestfdist;
		int filow = bestx - 16  /* ilow */;
		int fihigh = bestx + 16  /*ihigh */;
		int fjlow =  besty - 10 /*jlow  */;
		int fjhigh = besty + 10 /*jhigh*/;
					
		if( filow < ilow )
			filow = ilow;
		if( fihigh > ihigh )
			fihigh = ihigh;
		if( fjlow < jlow )
			fjlow = jlow;
		if( fjhigh > jhigh )
			fjhigh = jhigh;
						
		/* Now find the best 2*2 pel match in the neighbourhood.  This is probably overkill.
		   TODO: There is almost invariably *some* locality so should be tuned later... */
	
		bestfdist = INT_MAX;
		forgblk = forg+ (fjlow>>1)*flx;
		for( j = fjlow; j <= fjhigh; j += 2 )
		{
			for( i = filow; i <= fihigh; i += 2 )
			{
				dist = *pfdist1( forgblk+(i>>1), fblk, flx, fh );	
				if( dist < bestfdist )
				{
					bestfdist = dist;
					bestx = i;
					besty = j;
				}
			}
			forgblk += flx;
		}

		/* The best 1*1 based on it... going upward only */
		onepel_radius = 1;   
		bestdist = INT_MAX;
				
		ilow =bestx;
		jlow =besty;
	}
	else
	{
		/* Now the best 1*1 pel match... down as well as up */
		if ( bestx-onepel_radius > 0 )
			ilow = bestx-onepel_radius;
		if( besty-onepel_radius > 0 )
			jlow = besty-onepel_radius;
				
	}
			
			
	if (bestx+onepel_radius <= width-16 )
		ihigh = bestx+onepel_radius;

	if( besty+onepel_radius <= height-16 )
		jhigh = besty+onepel_radius;

	orgblk = org +jlow*lx;
	for( j = jlow; j <= jhigh; ++j )
	{
		for( i = ilow; i <= ihigh; ++i)
		{
			dist = (*pdist1_)00( orgblk+i, blk, lx, h, bestdist );	
			if( dist < bestdist )
			{
				/*
				  printf("Correction to estimate %d %d dist %d (%d %d dist %d )\n", 
				  i,j, dist, bestx, besty, bestdist );
				*/
				bestdist = dist;
				bestx = i;
				besty = j;
			}
		}
		orgblk += lx;
	}

	if( bestdist > 65000 )
	{
		printf( "Screwed bestdist %d %d %d %d!\n", ilow, ihigh, jlow, jhigh );
		exit(1);
	}

	/*printf( "RC local minimum at %d %d rcdist %d dist %d\n", bestx, besty, bestrcdist, bestdist );
	 */
		
	update_threshold( &onepel_threshold, bestdist );
	*pimin = bestx;
	*pjmin = besty;
	*pdmin = bestdist;


}														

#endif


#ifdef TEST_RCSUM_SEARCH

/*
  Build a distance based heap of the 4*4 sub-sampled motion compensations.

  N.b. You'd think it would be worth exploiting the fact that using
  MMX/SSE you can do two adjacent 4*4 blocks of 4*4 sums in about the
  same as one.  Well it does save some time but its really very little...
  
  Similarly, you might think it worth collecting first on 8*8
  boundaries and then considering neighbours of the better ones.
  This makes stuff faster but apprecieably reduces the quality of the
  matches found so its a no-no in my book...

*/

extern int rcdist_mmx( unsigned char *org, uint16_t *sums, int lx, int h );
  
static int build_rcquad_heap( int ilow, int ihigh, int jlow, int jhigh, 
unsigned char *org, unsigned char *blk, 
uint16_t *blkcsums, *blkrsums,
							  int lx, int h )
{
	int i,j;
	unsigned char *orgblk, *old_orgblk;
	uint16_t *orgcsums, *orgrsums,;
	uint16_t blksums[32];
	
	
	rcsums( blk, blksums, lx, h );
	quad_heap_size = 0;
    /* Invariant:  orgblk = org+(i)+qlx*(j) */
    orgblk = org+(ilow)+lx*(jlow);
    
	for( j = jlow; j <= jhigh; j += 4 )
	  {
		old_orgblk = orgblk;
		for( i = ilow; i <= ihigh; i += 4 )
		  {
		    quad_match_heap[quad_heap_size].index = quad_heap_size;
		    quad_match_heap[quad_heap_size].weight = rcdist_mmx( orgblk,blksums,lx,h);
		    quad_matches[quad_heap_size].dx = i;
		    quad_matches[quad_heap_size].dy = j;
		    ++quad_heap_size;
			orgblk += 4;
		  }
		orgblk = old_orgblk + lx*4;
	  }
	  
	heapify( quad_match_heap, quad_heap_size );
	return quad_heap_size;

}


/* Build a distance based heap of absolute row-col sum differences
 on a 2*2 grid...
*/


static int build_rchalf_heap(int ihigh, int jhigh, 
					        unsigned char *org,  unsigned char *blk, 
							int lx, int h,  int searched_quad_size )
{
  int k,s;
  sortelt distrec;
  blockxy matchrec;
  uint8_t *orgblk;
  int dist_sum  = 0;
  int best_fast_dist = INT_MAX;
  uint16_t blksums[32];
	
  rcsums( blk, blksums, lx, h ); 
  
  half_heap_size = 0;
  for( k = 0; k < searched_quad_size; ++k )
	{
	  heap_extract( quad_match_heap, &quad_heap_size, &distrec );
	  matchrec = quad_matches[distrec.index];
	  orgblk =  org + (matchrec.dy>>1)*lx +(matchrec.dx);
		  
	  half_matches[half_heap_size].dx = matchrec.dx;
	  half_matches[half_heap_size].dy = matchrec.dy;
	  s = rcdist_mmx( orgblk,blksums,lx,h);
	  half_match_heap[half_heap_size].weight = s;
	  half_match_heap[half_heap_size].index = half_heap_size;

	  if( s < best_fast_dist )
		  best_fast_dist = s;
	  ++half_heap_size;
	  dist_sum += s;
	  
		  
	 if(  matchrec.dy+2 <= jhigh )
	   {
	   	  half_matches[half_heap_size].dx = matchrec.dx;
		  half_matches[half_heap_size].dy = matchrec.dy+2;
		  s = rcdist_mmx( orgblk+lx,blksums,lx,h);
		  half_match_heap[half_heap_size].index = half_heap_size;
		  half_match_heap[half_heap_size].weight = s;
		  if( s < best_fast_dist )
				best_fast_dist = s;
		  ++half_heap_size;
		  dist_sum += s;
		} 
		  
	  if( matchrec.dx+2 <= ihigh )
		  {
		  	  half_matches[half_heap_size].dx = matchrec.dx+2;
			  half_matches[half_heap_size].dy = matchrec.dy;
			  s = rcdist_mmx( orgblk+1,blksums,lx,h);
			  half_match_heap[half_heap_size].index = half_heap_size;
			  half_match_heap[half_heap_size].weight = s;
			  if( s < best_fast_dist )
					best_fast_dist = s;
			  ++half_heap_size;
			  dist_sum += s;
			  
			 if( matchrec.dy+2 <= jhigh )
				{
					half_matches[half_heap_size].dx = matchrec.dx+2;
					half_matches[half_heap_size].dy = matchrec.dy+2;
				    s = rcdist_mmx( orgblk+lx+1,blk,lx,h);
					half_match_heap[half_heap_size].index = half_heap_size;
					half_match_heap[half_heap_size].weight = s;
					if( s < best_fast_dist )
						  best_fast_dist = s;
					++half_heap_size;
					dist_sum += s;
				}
		 }

	  /* If we're thresholding check  got a match below the threshold. 
	*and* .. stop looking... we've already found a good candidate...
	*/
	  if( fast_mc_threshold && best_fast_dist < twopel_threshold.threshold )
	  { 
		  break;
	  }
	}

  /* Since heapification is relatively expensive we do a swift
	 pre-processing step throwing out all candidates that
	 are heavier than the heap average */

  half_heap_size = thin_vector(  half_match_heap, 
                                 dist_sum / half_heap_size, half_heap_size );

  heapify( half_match_heap, half_heap_size );
  /* Update the fast motion match average using the best 2*2 match */
  update_threshold( &twopel_threshold, smallest_weight );

  return half_heap_size;
}
#endif



/* 
 *   Vector of motion compensations plus a heap of the associated
 *   weights (macro-block distances).
 *  TODO: Should be put into nice tidy records...
 */

#define MAX_COARSE_HEAP_SIZE 100*100

#ifdef HALF_HEAPS
static sortelt half_match_heap[MAX_COARSE_HEAP_SIZE];
static blockxy half_matches[MAX_COARSE_HEAP_SIZE];
#endif
#ifdef HEAPS
static sortelt quad_match_heap[MAX_COARSE_HEAP_SIZE];
static blockxy quad_matches[MAX_COARSE_HEAP_SIZE];

static sortelt rough_match_heap[MAX_COARSE_HEAP_SIZE];
static blockxy rough_matches[MAX_COARSE_HEAP_SIZE];
#endif
static int rough_heap_size;
static int half_heap_size;
static int quad_heap_size;

/*
  Build a distance based heap of the 4*4 sub-sampled motion compensations.

  N.b. You'd think it would be worth exploiting the fact that using
  MMX/SSE you can do two adjacent 4*4 blocks of 4*4 sums in about the
  same as one.  Well it does save some time but its really very little...
  
  Similarly, you might think it worth collecting first on 8*8
  boundaries and then considering neighbours of the better ones.
  This makes stuff faster but appreciably reduces the quality of the
  matches found so it is a no-no in my book...

*/
#ifndef HEAPS
static void mean_partition( mc_result_s *matches, int len, int times,
								    int *newlen_res, int *minweight_res)
{
	int i,j;
	int weight_sum;
	int mean_weight;
	int min_weight = 100000;
	if( len == 0 )
	{
		*minweight_res = 100000;
		*newlen_res = 0;
		return;
	}

	while( times > 0 )
	{
		weight_sum = 0;
		for( i = 0; i < len ; ++i )
			weight_sum += matches[i].weight;
		mean_weight = weight_sum / len;

		j = 0;
		for( i =0; i < len; ++i )
		{
			if( matches[i].weight <= mean_weight )
			{
				if( times == 1)
				{
					min_weight = matches[i].weight ;
				}
				matches[j] = matches[i];
				++j;
			}
		}
		len = j;
		--times;
	}
	*newlen_res = len;
	*minweight_res = min_weight;
}

static mc_result_s finalres[100*100];
#endif
#ifndef HALF_HEAPS
static mc_result_s halfres[100*100];
#endif

static int build_quad_heap( int ilow, int ihigh, int jlow, int jhigh, 
							int i0, int j0,
							uint8_t *qorg, uint8_t *qblk, int qlx, int qh )
{
	uint8_t *qorgblk;
	int k;
	int dist_sum;
#ifdef HEAPS
	int i,j;
	uint8_t *old_qorgblk;
	int s1,s2;
	int best_quad_dist;
	sortelt distrec;
	blockxy matchrec;
	int searched_rough_size;
#else
	int ilim = ihigh-ilow;
	int jlim = jhigh-jlow;
	int mean_weight;
	int rangex, rangey;
	static mc_result_s roughres[50*50];
#endif

	/* N.b. we may ignore the right hand block of the pair going over the
	   right edge as we have carefully allocated the buffer oversize to ensure
	   no memory faults.  The later motion compensation calculations
	   performed on the results of this pass will filter out
	   out-of-range blocks...
	*/

	dist_sum = 0;
	quad_heap_size = 0;

#ifdef HEAPS
	if(  nodual_qdist )
	{
		/* Invariant:  qorgblk = qorg+(i>>2)+qlx*(j>>2) */
		qorgblk = qorg+(ilow>>2)+qlx*(jlow>>2);
		for( j = jlow; j <= jhigh; j += 4 )
		{
			old_qorgblk = qorgblk;
			for( i = ilow; i <= ihigh; i += 4 )
			{
				s1 = ((*pqdist1)( qorgblk,qblk,qlx,qh) & 0xffff)
					+ fastabs(i-i0) + fastabs(j-j0);
				quad_matches[quad_heap_size].x = i;
				quad_matches[quad_heap_size].y = j;
				quad_match_heap[quad_heap_size].index = quad_heap_size;
				quad_match_heap[quad_heap_size].weight = s1 ;
				dist_sum += s1;
				++quad_heap_size;
				qorgblk += 1;
			}
			qorgblk = old_qorgblk + qlx;
		}
		heapify( quad_match_heap, quad_heap_size );
	}
	else
	{
		rough_heap_size = 0;
		qorgblk = qorg+(ilow>>2)+qlx*(jlow>>2);
		for( j = jlow; j <= jhigh; j += 8 )
		{
			old_qorgblk = qorgblk;
			k = 0;
			for( i = ilow; i <= ihigh; i+= 8  )
			{
				s1 = (*pqdist1)( qorgblk,qblk,qlx,qh);
				s2 = ((s1 >> 16) & 0xffff);
				s1 = (s1 & 0xffff);
				s2 += fastabs(i+8-i0) + fastabs(j-j0);
				s1 += fastabs(i-i0) + fastabs(j-j0);
				dist_sum += s1+s2;
				rough_match_heap[rough_heap_size].weight = s1;
				rough_match_heap[rough_heap_size].index = rough_heap_size;	
				rough_match_heap[rough_heap_size+1].weight = s2;
				rough_match_heap[rough_heap_size+1].index = rough_heap_size+1;	
				rough_matches[rough_heap_size].x = i;
				rough_matches[rough_heap_size].y = j;
				rough_matches[rough_heap_size+1].x = i+16;
				rough_matches[rough_heap_size+1].y = j;
				/* A sneaky fast way way of discarding out-of-limits estimates ... */
				rough_heap_size += 1 + (i+16 <= ihigh);

				/* NOTE:  If you change the grid size you *must* adjust this code... too */
	
				qorgblk+=2;
				k = (k+2)&3;
				/* Original branchy code... so you can see what this stuff is meant to
				   do!!!!
				   if( k == 0 )
				   {
				   qorgblk += 4;
				   i += 16;
				   }
				*/
				qorgblk += (k==0)<<2;
				i += (k==0)<<4;
		  
			}
			qorgblk = old_qorgblk + (qlx<<1);
		}

		/* Statistically thin the rough matches to roughly the upper quartile */
		/*
		rough_heap_size = thin_vector(  rough_match_heap, 
										dist_sum / rough_heap_size, rough_heap_size );
		dist_sum = 0;
		for( i = 0; i < rough_heap_size; ++i )
			dist_sum += rough_match_heap[i].weight;
		rough_heap_size = thin_vector(  rough_match_heap, 
										dist_sum / rough_heap_size, rough_heap_size );
		dist_sum = 0;
		for( i = 0; i < rough_heap_size; ++i )
			dist_sum += rough_match_heap[i].weight;
		rough_heap_size = thin_vector(  rough_match_heap, 
										dist_sum / rough_heap_size, rough_heap_size );
		searched_rough_size = rough_heap_size;
		*/


		heapify( rough_match_heap, rough_heap_size );
		best_quad_dist = rough_match_heap[0].weight;	
		searched_rough_size = 1+rough_heap_size / 6;
		best_quad_dist = rough_match_heap[0].weight;	

		/* 
		   We now use the good matches on 8-pel boundaries 
		   as starting points for matches on 4-pel boundaries...
		*/

		dist_sum = 0;
		quad_heap_size = 0;
		for( k = 0; k < searched_rough_size; ++k )
		{
			heap_extract( rough_match_heap, &rough_heap_size, &distrec );
			matchrec = rough_matches[distrec.index];
			qorgblk =  qorg + (matchrec.y>>2)*qlx +(matchrec.x>>2);
			quad_matches[quad_heap_size].x = matchrec.x;
			quad_matches[quad_heap_size].y = matchrec.y;
			quad_match_heap[quad_heap_size].index = quad_heap_size;
			quad_match_heap[quad_heap_size].weight = distrec.weight;
			dist_sum += distrec.weight;	
			++quad_heap_size;	  

			for( i = 0; i < 4; ++i )
			{
				int x, y;
				x = matchrec.x + (i & 0x1)*4;
				y = matchrec.y + (i >>1)*4;
				s1 = (*pqdist1)( qorgblk+(i & 0x1),qblk,qlx,qh) & 0xffff;
				dist_sum += s1;
				if( s1 < best_quad_dist )
					best_quad_dist = s1;
				quad_match_heap[quad_heap_size].weight = s1;
				quad_match_heap[quad_heap_size].index = quad_heap_size;
				quad_matches[quad_heap_size].x = x;
				quad_matches[quad_heap_size].y = y;
				quad_heap_size += (x <= ihigh && y <= jhigh);

				if( i == 2 )
					qorgblk += qlx;
			}
		}
		heapify( quad_match_heap, quad_heap_size );
	}
#else
		qorgblk = qorg+(ilow>>2)+qlx*(jlow>>2);
		rough_heap_size
			= qblock_8grid_dists( qorgblk, qblk,
								  ilow, jlow,
								  ilim, jlim, 
								  qh, qlx, roughres);
		k = 0;

		mean_partition( roughres, rough_heap_size, 1, 
						&rough_heap_size, &mean_weight);
		/* 
		   We now use the good matches on 8-pel boundaries 
		   as starting points for matches on 4-pel boundaries...
		*/

		quad_heap_size = 0;
		for( k = 0; k < rough_heap_size; ++k )
		{
			finalres[quad_heap_size] = roughres[k];
			rangex = (roughres[k].x < ilim);
			rangey = (roughres[k].y < jlim);
			++quad_heap_size;
			quad_heap_size +=
				qblock_near_dist( roughres[k].blk, qblk, 
								  roughres[k].x, roughres[k].y,
								  rangex, rangey,
								  mean_weight,
								  qh, qlx, finalres+quad_heap_size);
		}

		mean_partition( finalres, quad_heap_size, 2, 
						&quad_heap_size, &mean_weight);


#endif
	return quad_heap_size;
}


/* Build a distance based heap of the 2*2 sub-sampled motion
  compensations using the best 4*4 matches as starting points.  As
  with with the 4*4 matches We don't collect them densely as they're
  just search starting points for 1-pel search and ones that are 1 out
  should still give better than average matches...

*/


static int build_half_heap( int ilow,  int ihigh, int jlow, int jhigh, 
						   uint8_t *forg,  uint8_t *fblk, 
						   int flx, int fh,  int searched_quad_size )
{
	int i,k,s;
#ifdef HEAPS
	sortelt distrec;
#endif
#ifndef HALF_HEAPS
	int min_weight;
	int ilim = ihigh-ilow;
	int jlim = jhigh-jlow;
#else
	int ilim = ihigh;
	int jlim = jhigh;
	int best_fast_dist = 100000;
#endif
	blockxy matchrec;
	uint8_t *forgblk;
	int dist_sum  = 0;
  
	half_heap_size = 0;
	for( k = 0; k < searched_quad_size; ++k )
	{
#ifdef HEAPS
		heap_extract( quad_match_heap, &quad_heap_size, &distrec );
		matchrec = quad_matches[distrec.index];
		forgblk =  forg + ((matchrec.y)>>1)*flx +((matchrec.x)>>1);
#else
#ifdef HALF_HEAPS
		matchrec.x = ilow+finalres[k].x;
		matchrec.y = jlow+finalres[k].y;
		forgblk =  forg + ((matchrec.y)>>1)*flx +((matchrec.x)>>1);
#else
		matchrec.x = finalres[k].x;
		matchrec.y = finalres[k].y;
		forgblk =  forg + ((matchrec.y+jlow)>>1)*flx +((matchrec.x+ilow)>>1);
#endif
#endif
		  
		for( i = 0; i < 4; ++i )
		{
			if( matchrec.x <= ilim && matchrec.y <= jlim )
			{
				s = (*pfdist1)( forgblk,fblk,flx,fh);
#ifdef HALF_HEAPS
				half_matches[half_heap_size] = matchrec;
				half_match_heap[half_heap_size].weight = s;
				half_match_heap[half_heap_size].index = half_heap_size;
				best_fast_dist = fastmin( s, best_fast_dist );
#else
				halfres[half_heap_size].x = (int8_t)matchrec.x;
				halfres[half_heap_size].y = (int8_t)matchrec.y;
				halfres[half_heap_size].weight = s;
#endif

				++half_heap_size;
				dist_sum += s;
			}

			if( i == 1 )
			{
				forgblk += flx-1;
				matchrec.x -= 2;
				matchrec.y += 2;
			}
			else
			{
				forgblk += 1;
				matchrec.x += 2;
				
			}
		}


		/* If we're thresholding check  got a match below the threshold. 
		 *and* .. stop looking... we've already found a good candidate...
		 */
#ifdef HALF_HEAPS
		if( fast_mc_threshold && best_fast_dist < twopel_threshold.threshold )
		{ 
			break;
		}
#endif
	}

	
	/* Since heapification is relatively expensive we do a swift
	   pre-processing step throwing out all candidates that
	   are heavier than the heap average */
#ifdef HALF_HEAPS	
	half_heap_size = thin_vector(  half_match_heap, 
								   dist_sum / half_heap_size, half_heap_size );
	heapify( half_match_heap, half_heap_size );
	update_threshold( &twopel_threshold, half_match_heap[0].weight );
#else
	mean_partition( halfres, half_heap_size, 3, &half_heap_size, &min_weight );
#endif  
	/* Update the fast motion match average using the best 2*2 match */
	return half_heap_size;
}

/*
  Search for the best 1-pel match with 1-pel of a good 2*2-pel match.
*/



static void find_best_one_pel( uint8_t *org, uint8_t *blk,
							   int searched_size,
							   int i0, int j0,
							   int ilow, int jlow,
							   int xmax, int ymax,
							   int lx, int h, 
							   mb_motion_s *res
	)

{
	int k;
	int d;
	blockxy minpos = res->pos;
	int dmin = INT_MAX;
	uint8_t *orgblk;
#ifdef HALF_HEAPS
	sortelt distrec;
#endif
	int penalty;
	int init_search;
	int init_size;
	blockxy matchrec;
  
 
	/* Invariant:  qorgblk = qorg+(i>>2)+qlx*(j>>2) */
	/* It can happen (rarely) that the best matches are actually illegal.
	   In that case we have to carry on looking... */
#ifdef HALF_HEAPS
	do {
#endif
		init_search = searched_size;
		init_size = half_heap_size;
		for( k = 0; k < searched_size; ++k )
		{	
#ifdef HALF_HEAPS
			heap_extract( half_match_heap, &half_heap_size, &distrec );
			matchrec = half_matches[distrec.index];
#else
			matchrec.x = ilow + halfres[k].x;
			matchrec.y = jlow + halfres[k].y;
#endif
			orgblk = org + matchrec.x+lx*matchrec.y;
			penalty = abs(matchrec.x-i0)+abs(matchrec.y-j0);
		  
			if( matchrec.x <= xmax && matchrec.y <= ymax )
			{

				d = penalty+(*pdist1_00)(orgblk,blk,lx,h, dmin);
				if (d<dmin)
				{
					dmin = d;
					minpos = matchrec;
				}
				d = penalty+(*pdist1_00)(orgblk+1,blk,lx,h, dmin);
				if (d<dmin && (matchrec.x < xmax))
				{
					dmin = d;
					minpos.x = matchrec.x+1;
					minpos.y = matchrec.y;
				}

				d = penalty+(*pdist1_00)(orgblk+lx,blk,lx,h, dmin);
				if( (d<dmin) && (matchrec.y < ymax))
				{
					dmin = d;
					minpos.x = matchrec.x;
					minpos.y = matchrec.y+1;
				}
				d = penalty+(*pdist1_00)(orgblk+lx+1,blk,lx,h, dmin);
				if ( (d<dmin) && (matchrec.y < ymax) && (matchrec.x < xmax))
				{
					dmin = d;
					minpos.x = matchrec.x+1;
					minpos.y = matchrec.y+1;
				}      
			}

		}
#ifdef HALF_HEAPS
		searched_size = half_heap_size;

	} while (half_heap_size>0 && dmin == INT_MAX);
#endif
	res->pos = minpos;
	res->blk = org+minpos.x+lx*minpos.y;
	res->sad = dmin;

}
 
/*
 * full search block matching
 *
 * A.Stevens 2000: This is now a big misnomer.  The search is now a hierarchical/sub-sampling
 * search not a full search.  However, experiments have shown it is always close to
 * optimal and almost always very close or optimal.
 *
 * blk: top left pel of (16*h) block
 * fblk: top element of fast motion compensation block corresponding to blk
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in ref,blk
 * org: top left pel of source reference picture
 * ref: top left pel of reconstructed reference picture
 * i0,j0: center of search window
 * sx,sy: half widths of search window
 * xmax,ymax: right/bottom limits of search area
 * iminp,jminp: pointers to where the result is stored
 *              result is given as half pel offset from ref(0,0)
 *              i.e. NOT relative to (i0,j0)
 */


static void fullsearch(
	unsigned char *org,
	unsigned char *ref,
	subsampled_mb_s *ssblk,
	int lx, int i0, int j0, 
	int sx, int sy, int h,
	int xmax, int ymax,
	/* int *iminp, int *jminp, int *sadminp, */
	mb_motion_s *res
	)
{
	mb_motion_s best;
	/* int imin, jmin, dmin */
	int i,j,ilow,ihigh,jlow,jhigh;
	int d;
	int sxy;
	int searched_size;

	/* NOTE: Surprisingly, the initial motion compensation search
	   works better when the original image not the reference (reconstructed)
	   image is used. 
	*/
	uint8_t *forg = (uint8_t*)(org+fsubsample_offset);
	uint8_t *qorg = (uint8_t*)(org+qsubsample_offset);
	uint8_t *orgblk;
	int flx = lx >> 1;
	int qlx = lx >> 2;
	int fh = h >> 1;
	int qh = h >> 2;

	sxy = (sx>sy) ? sx : sy;

	/* xmax and ymax into more useful form... */
	xmax -= 16;
	ymax -= h;

	best.pos.x = i0;
	best.pos.y = j0;
  
  
  	/* The search radii are *always* multiples of 4 to avoid messiness in the initial
	   4*4 pel search.  This is handled by the parameter checking/processing code in readparmfile()
	*/
  
	if( (sx>>1)*(sy>>1) > MAX_COARSE_HEAP_SIZE )
	{
		fprintf( stderr, "Search radius %d too big for search heap!\n", sxy );
		exit(1);
	}

	/* Create a distance-order heap of possible motion compensations
	  based on the fast estimation data - 4*4 pel sums (4*4
	  sub-sampled) rather than actual pel's.  1/16 the size...  */
	jlow = j0-sy;
	jlow = jlow < 0 ? 0 : jlow;
	jhigh =  j0+sy;
	jhigh = jhigh > ymax ? ymax : jhigh;
	ilow = i0-sx;
	ilow = ilow < 0 ? 0 : ilow;
	ihigh =  i0+sx;
	ihigh = ihigh > xmax ? xmax : ihigh;


	quad_heap_size = build_quad_heap( ilow, ihigh, jlow, jhigh, 
									  i0, j0,
									  qorg, 
									  ssblk->qmb, qlx, qh );
	if( quad_heap_size == 0 )
		printf( "NULL QH REUTRN\n");
	/*
	if( i0 != quad_matches[quad_match_heap[0].index].x ||
		j0 != 	quad_matches[quad_match_heap[0].index].y )
		printf( "BEST QUAD OF %d FROM %d %d is %d %d\n",
				quad_heap_size,
				i0, j0, 
				quad_matches[quad_match_heap[0].index].x,
				quad_matches[quad_match_heap[0].index].y );
	*/
	/* Now create a distance-ordered heap of possible motion
	   compensations based on the fast estimation data - 2*2 pel sums
	   using the best fraction of the 4*4 estimates However we cover
	   only coarsely... on 4-pel boundaries...  */


#ifdef HEAPS
	searched_size = 1 + quad_heap_size / 8;
	if( searched_size > quad_heap_size )
		searched_size = quad_heap_size;
#else
	searched_size = quad_heap_size;
#endif

	half_heap_size = build_half_heap( ilow, ihigh, jlow, jhigh, 
									  forg, ssblk->fmb, flx, fh, 
									  searched_size );

    /* Now choose best 1-pel match from what approximates (not exact
	   due to the pre-processing trick with the mean) the top 1/2 of
	   the 2*2 matches */
#ifdef HALF_HEAPS  
	searched_size = 1+ (half_heap_size / fast_mc_frac);
	if( searched_size > half_heap_size )
		searched_size = half_heap_size;
#else
	searched_size = half_heap_size;
#endif

/*
	if( i0 != half_matches[half_match_heap[0].index].x ||
		j0 != 	half_matches[half_match_heap[0].index].y )
	printf( "BEST HALF FROM %d %d is %d %d\n", i0, j0, 
			half_matches[half_match_heap[0].index].x,
			half_matches[half_match_heap[0].index].y );
*/
	best.sad = INT_MAX;
	best.pos.x = i0;
	best.pos.y = j0;
 	/* Very rarely this may fail to find matchs due to all the good
	   looking ones being "off the edge".... I ignore this as its so
	   rare and the necessary tests costing significant time */
	find_best_one_pel( org, ssblk->mb, searched_size,
					   i0, j0,
					   ilow, jlow, xmax, ymax, 
					   lx, h, &best );

	/* Final polish: half-pel search of best candidate against
	   reconstructed image. 
	*/

	best.pos.x <<= 1; 
	best.pos.y <<= 1;
	best.hx = 0;
	best.hy = 0;

	ilow = best.pos.x - (best.pos.x>0);
	ihigh = best.pos.x + (best.pos.x<((xmax)<<1));
	jlow = best.pos.y - (best.pos.y>0);
	jhigh =  best.pos.y+ (best.pos.y<((ymax)<<1));

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

	*res = best;
}

/*
 * total absolute difference between two (16*h) blocks
 * including optional half pel interpolation of blk1 (hx,hy)
 * blk1,blk2: addresses of top left pels of both blocks
 * lx:        distance (in bytes) of vertically adjacent pels
 * hx,hy:     flags for horizontal and/or vertical interpolation
 * h:         height of block (usually 8 or 16)
 * distlim:   bail out if sum exceeds this value
 */

/* A.Stevens 2000: New version for highly pipelined CPUs where branching is
   costly.  Really it sucks that C doesn't define a stdlib abs that could
   be realised as a compiler intrinsic using appropriate CPU instructions.
   That 1970's heritage...
*/


static int dist1_00(unsigned char *blk1,unsigned char *blk2,
					int lx, int h,int distlim)
{
	unsigned char *p1,*p2;
	int j;
	int s;
	register int v;

	s = 0;
	p1 = blk1;
	p2 = blk2;
	for (j=0; j<h; j++)
	{

#define pipestep(o) v = p1[o]-p2[o]; s+= fastabs(v);
		pipestep(0);  pipestep(1);  pipestep(2);  pipestep(3);
		pipestep(4);  pipestep(5);  pipestep(6);  pipestep(7);
		pipestep(8);  pipestep(9);  pipestep(10); pipestep(11);
		pipestep(12); pipestep(13); pipestep(14); pipestep(15);
#undef pipestep

		if (s >= distlim)
			break;
			
		p1+= lx;
		p2+= lx;
	}
	return s;
}

static int dist1_01(unsigned char *blk1,unsigned char *blk2,
					int lx, int h)
{
	unsigned char *p1,*p2;
	int i,j;
	int s;
	register int v;

	s = 0;
	p1 = blk1;
	p2 = blk2;
	for (j=0; j<h; j++)
	{
		for (i=0; i<16; i++)
		{

			v = ((unsigned int)(p1[i]+p1[i+1])>>1) - p2[i];
			/*
			  v = ((p1[i]>>1)+(p1[i+1]>>1)>>1) - (p2[i]>>1);
			*/
			s+=fastabs(v);
		}
		p1+= lx;
		p2+= lx;
	}
	return s;
}

static int dist1_10(unsigned char *blk1,unsigned char *blk2,
					int lx, int h)
{
	unsigned char *p1,*p1a,*p2;
	int i,j;
	int s;
	register int v;

	s = 0;
	p1 = blk1;
	p2 = blk2;
	p1a = p1 + lx;
	for (j=0; j<h; j++)
	{
		for (i=0; i<16; i++)
		{
			v = ((unsigned int)(p1[i]+p1a[i])>>1) - p2[i];
			s+=fastabs(v);
		}
		p1 = p1a;
		p1a+= lx;
		p2+= lx;
	}

	return s;
}

static int dist1_11(unsigned char *blk1,unsigned char *blk2,
					int lx, int h)
{
	unsigned char *p1,*p1a,*p2;
	int i,j;
	int s;
	register int v;

	s = 0;
	p1 = blk1;
	p2 = blk2;
	p1a = p1 + lx;
	  
	for (j=0; j<h; j++)
	{
		for (i=0; i<16; i++)
		{
			v = ((unsigned int)(p1[i]+p1[i+1]+p1a[i]+p1a[i+1])>>2) - p2[i];
			s+=fastabs(v);
		}
		p1 = p1a;
		p1a+= lx;
		p2+= lx;
	}
	return s;
}

/* USED only during debugging...
void check_fast_motion_data(unsigned char *blk, char *label )
{
  unsigned char *b, *nb;
  uint8_t *pb,*p;
  uint8_t *qb;
  uint8_t *start_fblk, *start_qblk;
  int i;
  unsigned int mismatch;
  int nextfieldline;
  start_fblk = (blk+height*width);
  start_qblk = (blk+height*width+(height>>1)*(width>>1));

  if (pict_struct==FRAME_PICTURE)
	{
	  nextfieldline = width;
	}
  else
	{
	  nextfieldline = 2*width;
	}

	mismatch = 0;
	pb = start_fblk;
	qb = start_qblk;
	p = blk;
	while( pb < qb )
	{
		for( i = 0; i < nextfieldline/2; ++i )
		{
			mismatch |= ((p[0] + p[1] + p[nextfieldline] + p[nextfieldline+1])>>2) != *pb;
			p += 2;
			++pb;			
		}
		p += nextfieldline;
	}
		if( mismatch )
			{
				printf( "Mismatch detected check %s for buffer %08x\n", label,  blk );
					exit(1);
			}
}
*/

/* 
   Append fast motion estimation data to original luminance
   data.  N.b. memory allocation for luminance data allows space
   for this information...
 */

void fast_motion_data(unsigned char *blk, int picture_struct )
{
	unsigned char *b, *nb;
	uint8_t *pb;
	uint8_t *qb;
	uint8_t *start_fblk, *start_qblk;
	uint16_t *start_rowblk, *start_colblk;
	int i;
	int nextfieldline;
#ifdef TEST_RCSEARCH
	uint16_t *pc, *pr,*p;
	int rowsum;
	int j,s;
	int down16 = width*16;
	uint16_t sums[32];
	uint16_t rowsums[2048];
	uint16_t colsums[2048];  /* TODO: BUG: should resize with width */
#endif 
  

	/* In an interlaced field the "next" line is 2 width's down 
	   rather than 1 width down                                 */

	if (picture_struct==FRAME_PICTURE)
	{
		nextfieldline = width;
	}
	else
	{
		nextfieldline = 2*width;
	}

	start_fblk   = blk+fsubsample_offset;
	start_qblk   = blk+qsubsample_offset;
	start_rowblk = (uint16_t *)blk+rowsums_offset;
	start_colblk = (uint16_t *)blk+colsums_offset;
	b = blk;
	nb = (blk+nextfieldline);
	/* Sneaky stuff here... we can do lines in both fields at once */
	pb = (uint8_t *) start_fblk;

	while( nb < start_fblk )
	{
		for( i = 0; i < nextfieldline/4; ++i ) /* We're doing 4 pels horizontally at once */
		{
			/* TODO: A.Stevens this has to be the most word-length dependent
			   code in the world.  Better than MMX assembler though I guess... */
			pb[0] = (b[0]+b[1]+nb[0]+nb[1])>>2;
			pb[1] = (b[2]+b[3]+nb[2]+nb[3])>>2;	
			pb += 2;
			b += 4;
			nb += 4;
		}
		b += nextfieldline;
		nb = b + nextfieldline;
	}


	/* Now create the 4*4 sub-sampled data from the 2*2 
	   N.b. the 2*2 sub-sampled motion data preserves the interlace structure of the
	   original.  Albeit half as many lines and pixels...
	*/

	nextfieldline = nextfieldline >> 1;

	qb = start_qblk;
	b  = start_fblk;
	nb = (start_fblk+nextfieldline);

	while( nb < start_qblk )
	{
		for( i = 0; i < nextfieldline/4; ++i )
		{
			/* TODO: BRITTLE: A.Stevens - this only works for uint8_t = unsigned char */
			qb[0] = (b[0]+b[1]+nb[0]+nb[1])>>2;
			qb[1] = (b[2]+b[3]+nb[2]+nb[3])>>2;
			qb += 2;
			b += 4;
			nb += 4;
		}
		b += nextfieldline;
		nb = b + nextfieldline;
	}

#ifdef TEST_RCSEARCH
	/* TODO: BUG: THIS CODE DOES NOT YET ALLOW FOR INTERLACED FIELDS.... */
  
	/*
	  Initial row sums....
	*/
	pb = blk;
	for(j = 0; j < height; ++j )
	{
		rowsum = 0;
		for( i = 0; i < 16; ++ i )
		{
			rowsum += pb[i];
		}
		rowsums[j] = rowsum;
		pb += width;
	}
  
	/*
	  Initial column sums
	*/
	for( i = 0; i < width; ++i )
	{
		colsums[i] = 0;
	}
	pb = blk;
	for( j = 0; j < 16; ++j )
	{
		for( i = 0; i < width; ++i )
		{
			colsums[i] += *pb;
			++pb;
		}
	}
  
	/* Now fill in the row/column sum tables...
	   Note: to allow efficient construction of sum/col differences for a
	   given position row sums are held in a *column major* array
	   (changing y co-ordinate makes for small index changes)
	   the col sums are held in a *row major* array
	*/
  
	pb = blk;
	pc = start_colblk;
	for(j = 0; j <32; ++j )
	{
		pr = start_rowblk;
		rowsum = rowsums[j];
		for( i = 0; i < width-16; ++i )
		{
			pc[i] = colsums[i];
			pr[j] = rowsum;
			colsums[i] = (colsums[i] + pb[down16] )-pb[0];
			rowsum = (rowsum + pb[16]) - pb[0];
			++pb;
			pr += height;
		}
		pb += 16;   /* move pb on to next row... rememember we only did width-16! */
		pc += width;
	}
#endif 		
}


static int fdist1( uint8_t *fblk1, uint8_t *fblk2,int flx,int fh)
{
	uint8_t *p1 = fblk1;
	uint8_t *p2 = fblk2;
	int s = 0;
	int j;

	for( j = 0; j < fh; ++j )
	{
		register int diff;
#define pipestep(o) diff = p1[o]-p2[o]; s += fastabs(diff)
		pipestep(0); pipestep(1);
		pipestep(2); pipestep(3);
		pipestep(4); pipestep(5);
		pipestep(6); pipestep(7);
		p1 += flx;
		p2 += flx;
#undef pipestep
	}

	return s;
}



/*
  Sum absolute differences for 4*4 sub-sampled data.  

  TODO: currently assumes  only 16*16 or 16*8 motion compensation will be used...
  I.e. 4*4 or 4*2 sub-sampled blocks will be compared.
 */


static int qdist1( uint8_t *qblk1, uint8_t *qblk2,int qlx,int qh)
{
	register uint8_t *p1 = qblk1;
	register uint8_t *p2 = qblk2;
	int s = 0;
	register int diff;

	/* #define pipestep(o) diff = p1[o]-p2[o]; s += fastabs(diff) */
#define pipestep(o) diff = p1[o]-p2[o]; s += diff < 0 ? -diff : diff;
	pipestep(0); pipestep(1);	 pipestep(2); pipestep(3);
	if( qh > 1 )
	{
		p1 += qlx; p2 += qlx;
		pipestep(0); pipestep(1);	 pipestep(2); pipestep(3);
		if( qh > 2 )
		{
			p1 += qlx; p2 += qlx;
			pipestep(0); pipestep(1);	 pipestep(2); pipestep(3);
			p1 += qlx; p2 += qlx;
			pipestep(0); pipestep(1);	 pipestep(2); pipestep(3);
		}
	}


	return s;
}


/*
 * total squared difference between two (16*h) blocks
 * including optional half pel interpolation of blk1 (hx,hy)
 * blk1,blk2: addresses of top left pels of both blocks
 * lx:        distance (in bytes) of vertically adjacent pels
 * hx,hy:     flags for horizontal and/or vertical interpolation
 * h:         height of block (usually 8 or 16)
 */
 

static int dist2(blk1,blk2,lx,hx,hy,h)
	unsigned char *blk1,*blk2;
	int lx,hx,hy,h;
{
	unsigned char *p1,*p1a,*p2;
	int i,j;
	int s,v;

	s = 0;
	p1 = blk1;
	p2 = blk2;
	if (!hx && !hy)
		for (j=0; j<h; j++)
		{
			for (i=0; i<16; i++)
			{
				v = p1[i] - p2[i];
				s+= v*v;
			}
			p1+= lx;
			p2+= lx;
		}
	else if (hx && !hy)
		for (j=0; j<h; j++)
		{
			for (i=0; i<16; i++)
			{
				v = ((unsigned int)(p1[i]+p1[i+1]+1)>>1) - p2[i];
				s+= v*v;
			}
			p1+= lx;
			p2+= lx;
		}
	else if (!hx && hy)
	{
		p1a = p1 + lx;
		for (j=0; j<h; j++)
		{
			for (i=0; i<16; i++)
			{
				v = ((unsigned int)(p1[i]+p1a[i]+1)>>1) - p2[i];
				s+= v*v;
			}
			p1 = p1a;
			p1a+= lx;
			p2+= lx;
		}
	}
	else /* if (hx && hy) */
	{
		p1a = p1 + lx;
		for (j=0; j<h; j++)
		{
			for (i=0; i<16; i++)
			{
				v = ((unsigned int)(p1[i]+p1[i+1]+p1a[i]+p1a[i+1]+2)>>2) - p2[i];
				s+= v*v;
			}
			p1 = p1a;
			p1a+= lx;
			p2+= lx;
		}
	}
 
	return s;
}


/*
 * absolute difference error between a (16*h) block and a bidirectional
 * prediction
 *
 * p2: address of top left pel of block
 * pf,hxf,hyf: address and half pel flags of forward ref. block
 * pb,hxb,hyb: address and half pel flags of backward ref. block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in p2,pf,pb
 */
 

static int bdist1(pf,pb,p2,lx,hxf,hyf,hxb,hyb,h)
	unsigned char *pf,*pb,*p2;
	int lx,hxf,hyf,hxb,hyb,h;
{
	unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
	int i,j;
	int s,v;

	pfa = pf + hxf;
	pfb = pf + lx*hyf;
	pfc = pfb + hxf;

	pba = pb + hxb;
	pbb = pb + lx*hyb;
	pbc = pbb + hxb;

	s = 0;

	for (j=0; j<h; j++)
	{
		for (i=0; i<16; i++)
		{
			v = ((((unsigned int)(*pf++ + *pfa++ + *pfb++ + *pfc++ + 2)>>2) +
				  ((unsigned int)(*pb++ + *pba++ + *pbb++ + *pbc++ + 2)>>2) + 1)>>1)
				- *p2++;
			s += fastabs(v);
		}
		p2+= lx-16;
		pf+= lx-16;
		pfa+= lx-16;
		pfb+= lx-16;
		pfc+= lx-16;
		pb+= lx-16;
		pba+= lx-16;
		pbb+= lx-16;
		pbc+= lx-16;
	}

	return s;
}

/*
 * squared error between a (16*h) block and a bidirectional
 * prediction
 *
 * p2: address of top left pel of block
 * pf,hxf,hyf: address and half pel flags of forward ref. block
 * pb,hxb,hyb: address and half pel flags of backward ref. block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in p2,pf,pb
 */
 

static int bdist2(pf,pb,p2,lx,hxf,hyf,hxb,hyb,h)
	unsigned char *pf,*pb,*p2;
	int lx,hxf,hyf,hxb,hyb,h;
{
	unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
	int i,j;
	int s,v;

	pfa = pf + hxf;
	pfb = pf + lx*hyf;
	pfc = pfb + hxf;

	pba = pb + hxb;
	pbb = pb + lx*hyb;
	pbc = pbb + hxb;

	s = 0;

	for (j=0; j<h; j++)
	{
		for (i=0; i<16; i++)
		{
			v = ((((unsigned int)(*pf++ + *pfa++ + *pfb++ + *pfc++ + 2)>>2) +
				  ((unsigned int)(*pb++ + *pba++ + *pbb++ + *pbc++ + 2)>>2) + 1)>>1)
				- *p2++;
			s+=v*v;
		}
		p2+= lx-16;
		pf+= lx-16;
		pfa+= lx-16;
		pfb+= lx-16;
		pfc+= lx-16;
		pb+= lx-16;
		pba+= lx-16;
		pbb+= lx-16;
		pbc+= lx-16;
	}

	return s;
}


/*
 * variance of a (16*16) block, multiplied by 256
 * p:  address of top left pel of block
 * lx: distance (in bytes) of vertically adjacent pels
 */
static int variance(p,lx)
	unsigned char *p;
	int lx;
{
	int i,j;
	unsigned int v,s,s2;

	s = s2 = 0;

	for (j=0; j<16; j++)
	{
		for (i=0; i<16; i++)
		{
			v = *p++;
			s+= v;
			s2+= v*v;
		}
		p+= lx-16;
	}
	return s2 - (s*s)/256;
}

/*
  Compute the variance of the residual of uni-directionally motion
  compensated block.
 */

static int unidir_pred_var( const mb_motion_s *motion,
							uint8_t *mb,  
							int lx, 
							int h)
{
	return (*pdist2)(motion->blk, mb, lx, motion->hx, motion->hy, h);
}


/*
  Compute the variance of the residual of bi-directionally motion
  compensated block.
 */

static int bidir_pred_var( const mb_motion_s *motion_f, 
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

/*
  Compute SAD for bi-directionally motion compensated blocks...
 */

static int bidir_pred_sad( const mb_motion_s *motion_f, 
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
