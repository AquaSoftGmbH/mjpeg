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
 *
 */

#include <stdio.h>
#include <limits.h>
#include "config.h"
#include "global.h"

/*

This links in the prototype version of an experimental gradient-descent
motion compensation algorithm.   Looks good but its not ready for prime-time
yet....
#define TEST_GRAD_DESCENT
*/

/* RJ: parameter if we should not search at all */
extern int do_not_search;

/* private prototypes */

static void frame_ME _ANSI_ARGS_((unsigned char *oldorg, unsigned char *neworg,
  unsigned char *oldref, unsigned char *newref, unsigned char *cur,
  int i, int j, int sxf, int syf, int sxb, int syb, struct mbinfo *mbi));

static void field_ME _ANSI_ARGS_((unsigned char *oldorg, unsigned char *neworg,
  unsigned char *oldref, unsigned char *newref, unsigned char *cur,
  unsigned char *curref, int i, int j, int sxf, int syf, int sxb, int syb,
  struct mbinfo *mbi, int secondfield, int ipflag));

static void frame_estimate _ANSI_ARGS_((unsigned char *org,
										unsigned char *ref, unsigned char *mb,
										mcompuint* fmb,
										mcompuint* qmb,
  int i, int j,
  int sx, int sy, int *iminp, int *jminp, int *imintp, int *jmintp,
  int *iminbp, int *jminbp, int *dframep, int *dfieldp,
  int *tselp, int *bselp, int imins[2][2], int jmins[2][2]));

static void field_estimate _ANSI_ARGS_((unsigned char *toporg,
  unsigned char *topref, unsigned char *botorg, unsigned char *botref,
										unsigned char *mb, 
										mcompuint *fmb,
										mcompuint* qmb,
  int i, int j, int sx, int sy, int ipflag,
  int *iminp, int *jminp, int *imin8up, int *jmin8up, int *imin8lp,
  int *jmin8lp, int *dfieldp, int *d8p, int *selp, int *sel8up, int *sel8lp,
  int *iminsp, int *jminsp, int *dsp));

static void dpframe_estimate _ANSI_ARGS_((unsigned char *ref,
										  unsigned char *mb, 
										  mcompuint *fmb,
										  mcompuint* qmb,

  int i, int j, int iminf[2][2], int jminf[2][2],
  int *iminp, int *jminp, int *imindmvp, int *jmindmvp,
  int *dmcp, int *vmcp));

static void dpfield_estimate _ANSI_ARGS_((unsigned char *topref,
  unsigned char *botref, unsigned char *mb,
  int i, int j, int imins, int jmins, int *imindmvp, int *jmindmvp,
  int *dmcp, int *vmcp));

static int fullsearch _ANSI_ARGS_( (unsigned char *org, unsigned char *ref,
								   unsigned char *blk, 
									mcompuint *fblk,
									mcompuint* qblk,
  int lx, int i0, int j0, int sx, int sy, int h, int xmax, int ymax,
  int *iminp, int *jminp));



#ifdef SSE

int dist1_00_SSE(unsigned char *blk1, unsigned char *blk2, int lx, int h, int distlim);
int dist1_01_SSE(unsigned char *blk1, unsigned char *blk2, int lx, int h);
int dist1_10_SSE(unsigned char *blk1, unsigned char *blk2, int lx, int h);
int dist1_11_SSE(unsigned char *blk1, unsigned char *blk2, int lx, int h);

#define dist1_00 dist1_00_SSE
#define dist1_01 dist1_01_SSE
#define dist1_10 dist1_10_SSE
#define dist1_11 dist1_11_SSE


int fdist1_SSE ( mcompuint *blk1, mcompuint *blk2,  int flx, int fh);

int qdist1_SSE ( mcompuint *blk1, mcompuint *blk2,  int flx, int fh);

int dist2_mmx( unsigned char *blk1, unsigned char *blk2,
                 int lx, int hx, int hy, int h);

int bdist2_mmx _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));

int bdist1_mmx _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));

#define fdist1 fdist1_SSE
#define qdist1 qdist1_SSE

#define dist2  dist2_mmx
#define bdist2  bdist2_mmx
#define bdist1 bdist1_mmx 

#else

#ifdef MMX
int dist1_00_MMX ( mcompuint *blk1, mcompuint *blk2,  int lx, int h, int distlim);
int dist1_01_MMX(unsigned char *blk1, unsigned char *blk2, int lx, int h);
int dist1_10_MMX(unsigned char *blk1, unsigned char *blk2, int lx, int h);
int dist1_11_MMX(unsigned char *blk1, unsigned char *blk2, int lx, int h);
#define dist1_00 dist1_00_MMX
#define dist1_01 dist1_01_MMX
#define dist1_10 dist1_10_MMX
#define dist1_11 dist1_11_MMX

int fdist1_MMX ( mcompuint *blk1, mcompuint *blk2,  int flx, int fh);

int qdist1_MMX (mcompuint *blk1, mcompuint *blk2,  int qlx, int qh);

int dist2_mmx( unsigned char *blk1, unsigned char *blk2,
                 int lx, int hx, int hy, int h);

int bdist2_mmx _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));
int bdist1_mmx _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));


#define qdist1 qdist1_MMX
#define fdist1 fdist1_MMX
#define dist2  dist2_mmx
#define bdist2 bdist2_mmx	
#define bdist1 bdist1_mmx 
#else

static int fdist1 ( mcompuint *blk1, mcompuint *blk2,  int flx, int fh);
/*#define dist1_00( blk1, blk2, lx,  h, distlim ) \
        dist1( blk1, blk2, lx,  0,0,h,distlim) */
static int qdist1 ( mcompuint *blk1, mcompuint *blk2,  int qlx, int qh);
static int dist1_00( mcompuint *blk1, mcompuint *blk2,  int lx, int h, int distlim);
static int dist1_01(unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int dist1_10(unsigned char *blk1, unsigned char *blk2, int lx, int h);
static int dist1_11(unsigned char *blk1, unsigned char *blk2, int lx, int h);

static int dist2 _ANSI_ARGS_((unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h));
  
  
static int bdist2 _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));

static int bdist1 _ANSI_ARGS_((unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h));
#endif
#endif




/*
 * fast int abs function for pipelined CPU's that suffer when
 * they have unpredictable branches...
 * WARNING: Assumes 8-bit bytes...
 */
#define fabsshift ((8*sizeof(unsigned int))-1)
#define fastabs(x) (((x)-(((unsigned int)(x))>>fabsshift)) ^ ((x)>>fabsshift))




static int variance _ANSI_ARGS_((unsigned char *p, int lx));


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

#ifdef TEST_GRAD_DESCENT
static double seq_sum;
static double ratio_sum;
static double worst_seq;
static int seq_cnt;
static int miss_cnt;
static int test_cnt;
static int dist_cnt;
#endif

void motion_estimation(oldorg,neworg,oldref,newref,cur,curref,
  sxf,syf,sxb,syb,mbi,secondfield,ipflag)
unsigned char *oldorg,*neworg,*oldref,*newref,*cur,*curref;
int sxf,syf,sxb,syb;
struct mbinfo *mbi;
int secondfield,ipflag;
{
  int i, j;
#ifdef TEST_GRAD_DESCENT
  worst_seq = 0.0;
  seq_sum = 0.0;
  ratio_sum = 0.0;
  seq_cnt = 0;
  miss_cnt = 0;
  test_cnt = 0;
  dist_cnt = 0;
#endif

  /* loop through all macroblocks of the picture */
  for (j=0; j<height2; j+=16)
  {
    for (i=0; i<width; i+=16)
    {
      if (pict_struct==FRAME_PICTURE)
        frame_ME(oldorg,neworg,oldref,newref,cur,i,j,sxf,syf,sxb,syb,mbi);
      else
        field_ME(oldorg,neworg,oldref,newref,cur,curref,i,j,sxf,syf,sxb,syb,
          mbi,secondfield,ipflag);
      mbi++;
    }
  }

#ifdef TEST_GRAD_DESCENT
  if( seq_cnt != 0 )
	{
	  printf( "\nMISSES=%d TESTS=%d DISTS=%d AS=%04f RS=%04f WS=%04f\n", 
	  		miss_cnt,
				test_cnt / seq_cnt,
				dist_cnt / seq_cnt,
			  seq_sum  / miss_cnt, 
			  ratio_sum/ seq_cnt, 
			  worst_seq);
	} else
#endif

}

static void frame_ME(oldorg,neworg,oldref,newref,cur,i,j,sxf,syf,sxb,syb,mbi)
unsigned char *oldorg,*neworg,*oldref,*newref,*cur;
int i,j,sxf,syf,sxb,syb;
struct mbinfo *mbi;
{
  int imin,jmin,iminf,jminf,iminr,jminr;
  int imint,jmint,iminb,jminb;
  int imintf,jmintf,iminbf,jminbf;
  int imintr,jmintr,iminbr,jminbr;
  int var,v0;
  int dmc,dmcf,dmcr,dmci,vmc,vmcf,vmcr,vmci;
  int dmcfield,dmcfieldf,dmcfieldr,dmcfieldi;
  int tsel,bsel,tself,bself,tselr,bselr;
  unsigned char *mb;
  mcompuint  *fmb;
  mcompuint  *qmb;
  int imins[2][2],jmins[2][2];
  int imindp,jmindp,imindmv,jmindmv,dmc_dp,vmc_dp;

  mb = cur + i + width*j;
  /* A.Stevens fast motion estimation data is appended to actual
	 luminance information 
  */
  fmb = ((mcompuint*)(cur + width*height)) + ((i>>1) + (width>>1)*(j>>1));
  qmb = ((mcompuint*)(cur + width*height+ (width>>1)*(height>>1)))
	     + ((i>>2) + (width>>2)*(j>>2));
  var = variance(mb,width);

  if (pict_type==I_TYPE)
	{
	  mbi->mb_type = MB_INTRA;
	}
  else if (pict_type==P_TYPE)
  {
    if (frame_pred_dct)
    {
      dmc = fullsearch(oldorg,oldref,mb, fmb, qmb,
                       width,i,j,sxf,syf,16,width,height,&imin,&jmin);
      vmc = dist2(oldref+(imin>>1)+width*(jmin>>1),mb,
                  width,imin&1,jmin&1,16);
      mbi->motion_type = MC_FRAME;
    }
    else
    {
      frame_estimate(oldorg,oldref,mb,fmb,qmb, i,j,sxf,syf,
        &imin,&jmin,&imint,&jmint,&iminb,&jminb,
        &dmc,&dmcfield,&tsel,&bsel,imins,jmins);

      if (M==1)
        dpframe_estimate(oldref,mb,fmb,qmb, i,j>>1,imins,jmins,
          &imindp,&jmindp,&imindmv,&jmindmv,&dmc_dp,&vmc_dp);

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
        vmc = dist2(oldref+(imin>>1)+width*(jmin>>1),mb,
                    width,imin&1,jmin&1,16);
      }
      else
      {
        mbi->motion_type = MC_FIELD;
        dmc = dmcfield;
        vmc = dist2(oldref+(tsel?width:0)+(imint>>1)+(width<<1)*(jmint>>1),
                    mb,width<<1,imint&1,jmint&1,8);
        vmc+= dist2(oldref+(bsel?width:0)+(iminb>>1)+(width<<1)*(jminb>>1),
                    mb+width,width<<1,iminb&1,jminb&1,8);
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

    if (vmc>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
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
      v0 = dist2(oldref+i+width*j,mb,width,0,0,16);
      if (4*v0>5*vmc && v0>=9*256)
      {
        /* use MC */
        var = vmc;
        mbi->mb_type = MB_FORWARD;
        if (mbi->motion_type==MC_FRAME)
        {
          mbi->MV[0][0][0] = imin - (i<<1);
          mbi->MV[0][0][1] = jmin - (j<<1);
        }
        else if (mbi->motion_type==MC_DMV)
        {
          /* these are FRAME vectors */
          /* same parity vector */
          mbi->MV[0][0][0] = imindp - (i<<1);
          mbi->MV[0][0][1] = (jmindp<<1) - (j<<1);

          /* opposite parity vector */
          mbi->dmvector[0] = imindmv;
          mbi->dmvector[1] = jmindmv;
        }
        else
        {
          /* these are FRAME vectors */
          mbi->MV[0][0][0] = imint - (i<<1);
          mbi->MV[0][0][1] = (jmint<<1) - (j<<1);
          mbi->MV[1][0][0] = iminb - (i<<1);
          mbi->MV[1][0][1] = (jminb<<1) - (j<<1);
          mbi->mv_field_sel[0][0] = tsel;
          mbi->mv_field_sel[1][0] = bsel;
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
    if (frame_pred_dct)
    {
      /* forward */
      dmcf = fullsearch(oldorg,oldref,mb,fmb, qmb,
                        width,i,j,sxf,syf,16,width,height,&iminf,&jminf);
      vmcf = dist2(oldref+(iminf>>1)+width*(jminf>>1),mb,
                   width,iminf&1,jminf&1,16);

      /* backward */
      dmcr = fullsearch(neworg,newref,mb,fmb,qmb,
                        width,i,j,sxb,syb,16,width,height,&iminr,&jminr);
      vmcr = dist2(newref+(iminr>>1)+width*(jminr>>1),mb,
                   width,iminr&1,jminr&1,16);

      /* interpolated (bidirectional) */
      vmci = bdist2(oldref+(iminf>>1)+width*(jminf>>1),
                    newref+(iminr>>1)+width*(jminr>>1),
                    mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);

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
      frame_estimate(oldorg,oldref,mb,fmb,qmb,i,j,sxf,syf,
        &iminf,&jminf,&imintf,&jmintf,&iminbf,&jminbf,
        &dmcf,&dmcfieldf,&tself,&bself,imins,jmins);

      /* backward prediction */
      frame_estimate(neworg,newref,mb,fmb, qmb, i,j,sxb,syb,
        &iminr,&jminr,&imintr,&jmintr,&iminbr,&jminbr,
        &dmcr,&dmcfieldr,&tselr,&bselr,imins,jmins);

      /* calculate interpolated distance */
      /* frame */
      dmci = bdist1(oldref+(iminf>>1)+width*(jminf>>1),
                    newref+(iminr>>1)+width*(jminr>>1),
                    mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);

      /* top field */
      dmcfieldi = bdist1(
                    oldref+(imintf>>1)+(tself?width:0)+(width<<1)*(jmintf>>1),
                    newref+(imintr>>1)+(tselr?width:0)+(width<<1)*(jmintr>>1),
                    mb,width<<1,imintf&1,jmintf&1,imintr&1,jmintr&1,8);

      /* bottom field */
      dmcfieldi+= bdist1(
                    oldref+(iminbf>>1)+(bself?width:0)+(width<<1)*(jminbf>>1),
                    newref+(iminbr>>1)+(bselr?width:0)+(width<<1)*(jminbr>>1),
                    mb+width,width<<1,iminbf&1,jminbf&1,iminbr&1,jminbr&1,8);

      /* select prediction type of minimum distance from the
       * six candidates (field/frame * forward/backward/interpolated)
       */
      if (dmci<dmcfieldi && dmci<dmcf && dmci<dmcfieldf
          && dmci<dmcr && dmci<dmcfieldr)
      {
        /* frame, interpolated */
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
        mbi->motion_type = MC_FRAME;
        vmc = bdist2(oldref+(iminf>>1)+width*(jminf>>1),
                     newref+(iminr>>1)+width*(jminr>>1),
                     mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);
      }
      else if (dmcfieldi<dmcf && dmcfieldi<dmcfieldf
               && dmcfieldi<dmcr && dmcfieldi<dmcfieldr)
      {
        /* field, interpolated */
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
        mbi->motion_type = MC_FIELD;
        vmc = bdist2(oldref+(imintf>>1)+(tself?width:0)+(width<<1)*(jmintf>>1),
                     newref+(imintr>>1)+(tselr?width:0)+(width<<1)*(jmintr>>1),
                     mb,width<<1,imintf&1,jmintf&1,imintr&1,jmintr&1,8);
        vmc+= bdist2(oldref+(iminbf>>1)+(bself?width:0)+(width<<1)*(jminbf>>1),
                     newref+(iminbr>>1)+(bselr?width:0)+(width<<1)*(jminbr>>1),
                     mb+width,width<<1,iminbf&1,jminbf&1,iminbr&1,jminbr&1,8);
      }
      else if (dmcf<dmcfieldf && dmcf<dmcr && dmcf<dmcfieldr)
      {
        /* frame, forward */
        mbi->mb_type = MB_FORWARD;
        mbi->motion_type = MC_FRAME;
        vmc = dist2(oldref+(iminf>>1)+width*(jminf>>1),mb,
                    width,iminf&1,jminf&1,16);
      }
      else if (dmcfieldf<dmcr && dmcfieldf<dmcfieldr)
      {
        /* field, forward */
        mbi->mb_type = MB_FORWARD;
        mbi->motion_type = MC_FIELD;
        vmc = dist2(oldref+(tself?width:0)+(imintf>>1)+(width<<1)*(jmintf>>1),
                    mb,width<<1,imintf&1,jmintf&1,8);
        vmc+= dist2(oldref+(bself?width:0)+(iminbf>>1)+(width<<1)*(jminbf>>1),
                    mb+width,width<<1,iminbf&1,jminbf&1,8);
      }
      else if (dmcr<dmcfieldr)
      {
        /* frame, backward */
        mbi->mb_type = MB_BACKWARD;
        mbi->motion_type = MC_FRAME;
        vmc = dist2(newref+(iminr>>1)+width*(jminr>>1),mb,
                    width,iminr&1,jminr&1,16);
      }
      else
      {
        /* field, backward */
        mbi->mb_type = MB_BACKWARD;
        mbi->motion_type = MC_FIELD;
        vmc = dist2(newref+(tselr?width:0)+(imintr>>1)+(width<<1)*(jmintr>>1),
                    mb,width<<1,imintr&1,jmintr&1,8);
        vmc+= dist2(newref+(bselr?width:0)+(iminbr>>1)+(width<<1)*(jminbr>>1),
                    mb+width,width<<1,iminbr&1,jminbr&1,8);
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
        mbi->MV[0][0][0] = iminf - (i<<1);
        mbi->MV[0][0][1] = jminf - (j<<1);
        /* backward */
        mbi->MV[0][1][0] = iminr - (i<<1);
        mbi->MV[0][1][1] = jminr - (j<<1);
      }
      else
      {
        /* these are FRAME vectors */
        /* forward */
        mbi->MV[0][0][0] = imintf - (i<<1);
        mbi->MV[0][0][1] = (jmintf<<1) - (j<<1);
        mbi->MV[1][0][0] = iminbf - (i<<1);
        mbi->MV[1][0][1] = (jminbf<<1) - (j<<1);
        mbi->mv_field_sel[0][0] = tself;
        mbi->mv_field_sel[1][0] = bself;
        /* backward */
        mbi->MV[0][1][0] = imintr - (i<<1);
        mbi->MV[0][1][1] = (jmintr<<1) - (j<<1);
        mbi->MV[1][1][0] = iminbr - (i<<1);
        mbi->MV[1][1][1] = (jminbr<<1) - (j<<1);
        mbi->mv_field_sel[0][1] = tselr;
        mbi->mv_field_sel[1][1] = bselr;
      }
    }
  }

  mbi->var = var;


}

/*
 * motion estimation for field pictures
 *
 * oldorg: original frame for forward prediction (P and B frames)
 * neworg: original frame for backward prediction (B frames only)
 * oldref: reconstructed frame for forward prediction (P and B frames)
 * newref: reconstructed frame for backward prediction (B frames only)
 * cur:    current original frame (the one for which the prediction is formed)
 * curref: current reconstructed frame (to predict second field from first)
 * sxf,syf: forward search window (frame coordinates)
 * sxb,syb: backward search window (frame coordinates)
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
static void field_ME(oldorg,neworg,oldref,newref,cur,curref,i,j,
  sxf,syf,sxb,syb,mbi,secondfield,ipflag)
unsigned char *oldorg,*neworg,*oldref,*newref,*cur,*curref;
int i,j,sxf,syf,sxb,syb;
struct mbinfo *mbi;
int secondfield,ipflag;
{
  int w2;
  unsigned char *mb, *toporg, *topref, *botorg, *botref;
  mcompuint *fmb;
  mcompuint *qmb;
  int var,vmc,v0,dmc,dmcfieldi,dmc8i;
  int imin,jmin,imin8u,jmin8u,imin8l,jmin8l,dmcfield,dmc8,sel,sel8u,sel8l;
  int iminf,jminf,imin8uf,jmin8uf,imin8lf,jmin8lf,dmcfieldf,dmc8f,self,sel8uf,sel8lf;
  int iminr,jminr,imin8ur,jmin8ur,imin8lr,jmin8lr,dmcfieldr,dmc8r,selr,sel8ur,sel8lr;
  int imins,jmins,ds,imindmv,jmindmv,vmc_dp,dmc_dp;

  w2 = width<<1;

  mb = cur + i + w2*j;
  /* Fast motion data sits at the end of the luminance buffer */
  fmb = ((mcompuint*)(cur + width*height))+(i>>1)+(w2>>1)*(j>>1);
  qmb = ((mcompuint*)(cur + width*height + (width>>1)*(height>>1)))
	+ (i>>2)+(w2>>2)*(j>>2);
  if (pict_struct==BOTTOM_FIELD)
	{
	  mb += width;
	  fmb += (width >> 1);
	  qmb += (width >> 2);
	}


  var = variance(mb,w2);

  if (pict_type==I_TYPE)
    mbi->mb_type = MB_INTRA;
  else if (pict_type==P_TYPE)
  {
    toporg = oldorg;
    topref = oldref;
    botorg = oldorg + width;
    botref = oldref + width;

    if (secondfield)
    {
      /* opposite parity field is in same frame */
      if (pict_struct==TOP_FIELD)
      {
        /* current is top field */
        botorg = cur + width;
        botref = curref + width;
      }
      else
      {
        /* current is bottom field */
        toporg = cur;
        topref = curref;
      }
    }

    field_estimate(toporg,topref,botorg,botref,mb,fmb,qmb,i,j,sxf,syf,ipflag,
                   &imin,&jmin,&imin8u,&jmin8u,&imin8l,&jmin8l,
                   &dmcfield,&dmc8,&sel,&sel8u,&sel8l,&imins,&jmins,&ds);

    if (M==1 && !ipflag)  /* generic condition which permits Dual Prime */
      dpfield_estimate(topref,botref,mb,i,j,imins,jmins,&imindmv,&jmindmv,
        &dmc_dp,&vmc_dp);

    /* select between dual prime, field and 16x8 prediction */
    if (M==1 && !ipflag && dmc_dp<dmc8 && dmc_dp<dmcfield)
    {
      /* Dual Prime prediction */
      mbi->motion_type = MC_DMV;
      dmc = dmc_dp;     /* L1 metric */
      vmc = vmc_dp;     /* we already calculated L2 error for Dual */

    }
    else if (dmc8<dmcfield)
    {
      /* 16x8 prediction */
      mbi->motion_type = MC_16X8;
      /* upper half block */
      vmc = dist2((sel8u?botref:topref) + (imin8u>>1) + w2*(jmin8u>>1),
                  mb,w2,imin8u&1,jmin8u&1,8);
      /* lower half block */
      vmc+= dist2((sel8l?botref:topref) + (imin8l>>1) + w2*(jmin8l>>1),
                  mb+8*w2,w2,imin8l&1,jmin8l&1,8);
    }
    else
    {
      /* field prediction */
      mbi->motion_type = MC_FIELD;
      vmc = dist2((sel?botref:topref) + (imin>>1) + w2*(jmin>>1),
                  mb,w2,imin&1,jmin&1,16);
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
        v0 = dist2(((pict_struct==BOTTOM_FIELD)?botref:topref) + i + w2*j,
                   mb,w2,0,0,16);
      if (ipflag || (4*v0>5*vmc && v0>=9*256))
      {
        var = vmc;
        mbi->mb_type = MB_FORWARD;
        if (mbi->motion_type==MC_FIELD)
        {
          mbi->MV[0][0][0] = imin - (i<<1);
          mbi->MV[0][0][1] = jmin - (j<<1);
          mbi->mv_field_sel[0][0] = sel;
        }
        else if (mbi->motion_type==MC_DMV)
        {
          /* same parity vector */
          mbi->MV[0][0][0] = imins - (i<<1);
          mbi->MV[0][0][1] = jmins - (j<<1);

          /* opposite parity vector */
          mbi->dmvector[0] = imindmv;
          mbi->dmvector[1] = jmindmv;
        }
        else
        {
          mbi->MV[0][0][0] = imin8u - (i<<1);
          mbi->MV[0][0][1] = jmin8u - (j<<1);
          mbi->MV[1][0][0] = imin8l - (i<<1);
          mbi->MV[1][0][1] = jmin8l - ((j+8)<<1);
          mbi->mv_field_sel[0][0] = sel8u;
          mbi->mv_field_sel[1][0] = sel8l;
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
        mbi->mv_field_sel[0][0] = (pict_struct==BOTTOM_FIELD);
      }
    }
  }
  else /* if (pict_type==B_TYPE) */
  {
    /* forward prediction */
    field_estimate(oldorg,oldref,oldorg+width,oldref+width,mb,fmb,qmb,
                   i,j,sxf,syf,0,
                   &iminf,&jminf,&imin8uf,&jmin8uf,&imin8lf,&jmin8lf,
                   &dmcfieldf,&dmc8f,&self,&sel8uf,&sel8lf,&imins,&jmins,&ds);

    /* backward prediction */
    field_estimate(neworg,newref,neworg+width,newref+width,mb,fmb,qmb,
                   i,j,sxb,syb,0,
                   &iminr,&jminr,&imin8ur,&jmin8ur,&imin8lr,&jmin8lr,
                   &dmcfieldr,&dmc8r,&selr,&sel8ur,&sel8lr,&imins,&jmins,&ds);

    /* calculate distances for bidirectional prediction */
    /* field */
    dmcfieldi = bdist1(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                       newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                       mb,w2,iminf&1,jminf&1,iminr&1,jminr&1,16);

    /* 16x8 upper half block */
    dmc8i = bdist1(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                   newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                   mb,w2,imin8uf&1,jmin8uf&1,imin8ur&1,jmin8ur&1,8);

    /* 16x8 lower half block */
    dmc8i+= bdist1(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                   newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                   mb+8*w2,w2,imin8lf&1,jmin8lf&1,imin8lr&1,jmin8lr&1,8);

    /* select prediction type of minimum distance */
    if (dmcfieldi<dmc8i && dmcfieldi<dmcfieldf && dmcfieldi<dmc8f
        && dmcfieldi<dmcfieldr && dmcfieldi<dmc8r)
    {
      /* field, interpolated */
      mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      mbi->motion_type = MC_FIELD;
      vmc = bdist2(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                   newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                   mb,w2,iminf&1,jminf&1,iminr&1,jminr&1,16);
    }
    else if (dmc8i<dmcfieldf && dmc8i<dmc8f
             && dmc8i<dmcfieldr && dmc8i<dmc8r)
    {
      /* 16x8, interpolated */
      mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      mbi->motion_type = MC_16X8;

      /* upper half block */
      vmc = bdist2(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                   newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                   mb,w2,imin8uf&1,jmin8uf&1,imin8ur&1,jmin8ur&1,8);

      /* lower half block */
      vmc+= bdist2(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                   newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                   mb+8*w2,w2,imin8lf&1,jmin8lf&1,imin8lr&1,jmin8lr&1,8);
    }
    else if (dmcfieldf<dmc8f && dmcfieldf<dmcfieldr && dmcfieldf<dmc8r)
    {
      /* field, forward */
      mbi->mb_type = MB_FORWARD;
      mbi->motion_type = MC_FIELD;
      vmc = dist2(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                  mb,w2,iminf&1,jminf&1,16);
    }
    else if (dmc8f<dmcfieldr && dmc8f<dmc8r)
    {
      /* 16x8, forward */
      mbi->mb_type = MB_FORWARD;
      mbi->motion_type = MC_16X8;

      /* upper half block */
      vmc = dist2(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                  mb,w2,imin8uf&1,jmin8uf&1,8);

      /* lower half block */
      vmc+= dist2(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                  mb+8*w2,w2,imin8lf&1,jmin8lf&1,8);
    }
    else if (dmcfieldr<dmc8r)
    {
      /* field, backward */
      mbi->mb_type = MB_BACKWARD;
      mbi->motion_type = MC_FIELD;
      vmc = dist2(newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                  mb,w2,iminr&1,jminr&1,16);
    }
    else
    {
      /* 16x8, backward */
      mbi->mb_type = MB_BACKWARD;
      mbi->motion_type = MC_16X8;

      /* upper half block */
      vmc = dist2(newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                  mb,w2,imin8ur&1,jmin8ur&1,8);

      /* lower half block */
      vmc+= dist2(newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                  mb+8*w2,w2,imin8lr&1,jmin8lr&1,8);
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
        mbi->MV[0][0][0] = iminf - (i<<1);
        mbi->MV[0][0][1] = jminf - (j<<1);
        mbi->mv_field_sel[0][0] = self;
        /* backward */
        mbi->MV[0][1][0] = iminr - (i<<1);
        mbi->MV[0][1][1] = jminr - (j<<1);
        mbi->mv_field_sel[0][1] = selr;
      }
      else /* MC_16X8 */
      {
        /* forward */
        mbi->MV[0][0][0] = imin8uf - (i<<1);
        mbi->MV[0][0][1] = jmin8uf - (j<<1);
        mbi->mv_field_sel[0][0] = sel8uf;
        mbi->MV[1][0][0] = imin8lf - (i<<1);
        mbi->MV[1][0][1] = jmin8lf - ((j+8)<<1);
        mbi->mv_field_sel[1][0] = sel8lf;
        /* backward */
        mbi->MV[0][1][0] = imin8ur - (i<<1);
        mbi->MV[0][1][1] = jmin8ur - (j<<1);
        mbi->mv_field_sel[0][1] = sel8ur;
        mbi->MV[1][1][0] = imin8lr - (i<<1);
        mbi->MV[1][1][1] = jmin8lr - ((j+8)<<1);
        mbi->mv_field_sel[1][1] = sel8lr;
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
 * mb:  macroblock to be matched
 * fmb: Fast motion estimation image of macro block to be matched (2*2  sub-sampled)
 * quad:  " (4*4 sub-sampled)
 * i,j: location of mb relative to ref (=center of search window)
 * sx,sy: half widths of search window
 * iminp,jminp,dframep: location and value of best frame prediction
 * imintp,jmintp,tselp: location of best field pred. for top field of mb
 * iminbp,jminbp,bselp: location of best field pred. for bottom field of mb
 * dfieldp: value of field prediction
 */
static void frame_estimate(org,ref,mb,fmb,qmb,i,j,sx,sy,
  iminp,jminp,imintp,jmintp,iminbp,jminbp,dframep,dfieldp,tselp,bselp,
  imins,jmins)
unsigned char *org,*ref,*mb;
mcompuint *fmb;
mcompuint *qmb;
int i,j,sx,sy;
int *iminp,*jminp;
int *imintp,*jmintp,*iminbp,*jminbp;
int *dframep,*dfieldp;
int *tselp,*bselp;
int imins[2][2],jmins[2][2];
{
  int dt,db,dmint,dminb;
  int imint,iminb,jmint,jminb;

  /* frame prediction */
  *dframep = fullsearch(org,ref,mb,fmb,qmb,width,i,j,sx,sy,16,width,height,
                        iminp,jminp);

  /* predict top field from top field */
  dt = fullsearch(org,ref,mb,fmb,qmb,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &imint,&jmint);

  /* predict top field from bottom field */
  db = fullsearch(org+width,ref+width,mb,fmb,qmb, width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &iminb,&jminb);

  imins[0][0] = imint;
  jmins[0][0] = jmint;
  imins[1][0] = iminb;
  jmins[1][0] = jminb;

  /* select prediction for top field */
  if (dt<=db)
  {
    dmint=dt; *imintp=imint; *jmintp=jmint; *tselp=0;
  }
  else
  {
    dmint=db; *imintp=iminb; *jmintp=jminb; *tselp=1;
  }

  /* predict bottom field from top field */
  dt = fullsearch(org,ref,mb+width,fmb+(width>>1),qmb+(width>>2), 
				  width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &imint,&jmint);

  /* predict bottom field from bottom field */
  db = fullsearch(org+width,ref+width,mb+width,fmb+(width>>1),qmb+(width>>2), 
				  width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &iminb,&jminb);

  imins[0][1] = imint;
  jmins[0][1] = jmint;
  imins[1][1] = iminb;
  jmins[1][1] = jminb;

  /* select prediction for bottom field */
  if (db<=dt)
  {
    dminb=db; *iminbp=iminb; *jminbp=jminb; *bselp=1;
  }
  else
  {
    dminb=dt; *iminbp=imint; *jminbp=jmint; *bselp=0;
  }

  *dfieldp=dmint+dminb;


}

/*
 * field picture motion estimation subroutine
 *
 * toporg: address of original top reference field
 * topref: address of reconstructed top reference field
 * botorg: address of original bottom reference field
 * botref: address of reconstructed bottom reference field
 * mb:  macroblock to be matched
 * fmb, qmb - Fast motion compensation sub-sampled data
 * i,j: location of mb (=center of search window)
 * sx,sy: half width/height of search window
 *
 * iminp,jminp,selp,dfieldp: location and distance of best field prediction
 * imin8up,jmin8up,sel8up: location of best 16x8 pred. for upper half of mb
 * imin8lp,jmin8lp,sel8lp: location of best 16x8 pred. for lower half of mb
 * d8p: distance of best 16x8 prediction
 * iminsp,jminsp,dsp: location and distance of best same parity field
 *                    prediction (needed for dual prime, only valid if
 *                    ipflag==0)
 */
static void field_estimate(toporg,topref,botorg,botref,mb,fmb,qmb,i,j,sx,sy,ipflag,
  iminp,jminp,imin8up,jmin8up,imin8lp,jmin8lp,dfieldp,d8p,selp,sel8up,sel8lp,
  iminsp,jminsp,dsp)
unsigned char *toporg, *topref, *botorg, *botref, *mb;
mcompuint *fmb;
mcompuint *qmb;
int i,j,sx,sy;
int ipflag;
int *iminp, *jminp;
int *imin8up, *jmin8up, *imin8lp, *jmin8lp;
int *dfieldp,*d8p;
int *selp, *sel8up, *sel8lp;
int *iminsp, *jminsp, *dsp;
{
  int dt, db, imint, jmint, iminb, jminb, notop, nobot;

  /* if ipflag is set, predict from field of opposite parity only */
  notop = ipflag && (pict_struct==TOP_FIELD);
  nobot = ipflag && (pict_struct==BOTTOM_FIELD);

  /* field prediction */

  /* predict current field from top field */
  if (notop)
    dt = 65536; /* infinity */
  else
    dt = fullsearch(toporg,topref,mb,fmb,qmb,width<<1,
                    i,j,sx,sy>>1,16,width,height>>1,
                    &imint,&jmint);

  /* predict current field from bottom field */
  if (nobot)
    db = 65536; /* infinity */
  else
    db = fullsearch(botorg,botref,mb,fmb,qmb,width<<1,
                    i,j,sx,sy>>1,16,width,height>>1,
                    &iminb,&jminb);

  /* same parity prediction (only valid if ipflag==0) */
  if (pict_struct==TOP_FIELD)
  {
    *iminsp = imint; *jminsp = jmint; *dsp = dt;
  }
  else
  {
    *iminsp = iminb; *jminsp = jminb; *dsp = db;
  }

  /* select field prediction */
  if (dt<=db)
  {
    *dfieldp = dt; *iminp = imint; *jminp = jmint; *selp = 0;
  }
  else
  {
    *dfieldp = db; *iminp = iminb; *jminp = jminb; *selp = 1;
  }


  /* 16x8 motion compensation */

  /* predict upper half field from top field */
  if (notop)
    dt = 65536;
  else
    dt = fullsearch(toporg,topref,mb,fmb,qmb,width<<1,
                    i,j,sx,sy>>1,8,width,height>>1,
                    &imint,&jmint);

  /* predict upper half field from bottom field */
  if (nobot)
    db = 65536;
  else
    db = fullsearch(botorg,botref,mb,fmb,qmb,width<<1,
                    i,j,sx,sy>>1,8,width,height>>1,
                    &iminb,&jminb);

  /* select prediction for upper half field */
  if (dt<=db)
  {
    *d8p = dt; *imin8up = imint; *jmin8up = jmint; *sel8up = 0;
  }
  else
  {
    *d8p = db; *imin8up = iminb; *jmin8up = jminb; *sel8up = 1;
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
    dt = 65536;
  else
    dt = fullsearch(toporg,topref,mb+(width<<4),fmb+(width<<2), qmb+width,
					width<<1,
                    i,j+8,sx,sy>>1,8,width,height>>1,
                    &imint,&jmint);

  /* predict lower half field from bottom field */
  if (nobot)
    db = 65536;
  else
    db = fullsearch(botorg,botref,mb+(width<<4),fmb+(width<<2),qmb+width,width<<1,
                    i,j+8,sx,sy>>1,8,width,height>>1,
                    &iminb,&jminb);

  /* select prediction for lower half field */
  if (dt<=db)
  {
    *d8p += dt; *imin8lp = imint; *jmin8lp = jmint; *sel8lp = 0;
  }
  else
  {
    *d8p += db; *imin8lp = iminb; *jmin8lp = jminb; *sel8lp = 1;
  }
}

static void dpframe_estimate(ref,mb,fmb,qmb,i,j,iminf,jminf,
  iminp,jminp,imindmvp, jmindmvp, dmcp, vmcp)
unsigned char *ref, *mb;
mcompuint *fmb;
mcompuint *qmb;
int i,j;
int iminf[2][2], jminf[2][2];
int *iminp, *jminp;
int *imindmvp, *jmindmvp;
int *dmcp,*vmcp;
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
  vmc = 1 << 30;

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
        if (topfirst == ppred)
        {
          /* second field: scale by 1/3 */
          is = (is>=0) ? (is+1)/3 : -((-is+1)/3);
          js = (js>=0) ? (js+1)/3 : -((-js+1)/3);
        }
        else
          continue;
      }

      /* vector for prediction from field of opposite 'parity' */
      if (topfirst)
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
              local_dist = bdist2(
                ref + (is>>1) + (width<<1)*(js>>1),
                ref + width + (it>>1) + (width<<1)*(jt>>1),
                mb,             /* current mb location */
                width<<1,       /* adjacent line distance */
                is&1, js&1, it&1, jt&1, /* half-pel flags */
                8);             /* block height */
              local_dist += bdist2(
                ref + width + (is>>1) + (width<<1)*(js>>1),
                ref + (ib>>1) + (width<<1)*(jb>>1),
                mb + width,     /* current mb location */
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
  local_dist = bdist1(
    ref + (imins>>1) + (width<<1)*(jmins>>1),
    ref + width + (imint>>1) + (width<<1)*(jmint>>1),
    mb,
    width<<1,
    imins&1, jmins&1, imint&1, jmint&1,
    8);
  local_dist += bdist1(
    ref + width + (imins>>1) + (width<<1)*(jmins>>1),
    ref + (iminb>>1) + (width<<1)*(jminb>>1),
    mb + width,
    width<<1,
    imins&1, jmins&1, iminb&1, jminb&1,
    8);

  *dmcp = local_dist;
  *iminp = imins;
  *jminp = jmins;
  *imindmvp = imindmv;
  *jmindmvp = jmindmv;
  *vmcp = vmc;
}

static void dpfield_estimate(topref,botref,mb,i,j,imins,jmins,
  imindmvp, jmindmvp, dmcp, vmcp)
unsigned char *topref, *botref, *mb;
int i,j;
int imins, jmins;
int *imindmvp, *jmindmvp;
int *dmcp,*vmcp;
{
  unsigned char *sameref, *oppref;
  int io0,jo0,io,jo,delta_x,delta_y,mvxs,mvys,mvxo0,mvyo0;
  int imino,jmino,imindmv,jmindmv,vmc_dp,local_dist;

  /* Calculate Dual Prime distortions for 9 delta candidates */
  /* Note: only for P pictures! */

  /* Assign opposite and same reference pointer */
  if (pict_struct==TOP_FIELD)
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
  mvxo0 = (mvxs+(mvxs>0)) >> 1;  /* mvxs // 2 */
  mvyo0 = (mvys+(mvys>0)) >> 1;  /* mvys // 2 */

  /* vertical field shift correction */
  if (pict_struct==TOP_FIELD)
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
        local_dist = bdist2(
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
  *dmcp = bdist1(
    sameref + (imins>>1) + width2*(jmins>>1),
    oppref  + (imino>>1) + width2*(jmino>>1),
    mb,             /* current mb location */
    width2,         /* adjacent line distance */
    imins&1, jmins&1, imino&1, jmino&1, /* half-pel flags */
    16);            /* block height */

  *imindmvp = imindmv;
  *jmindmvp = jmindmv;
  *vmcp = vmc_dp;
}

/*
 * Heaps of distance estimates for motion compensation search
 */

/* A.Stevens 2000: TODO: distance measures must fit into short 
   or bad things will happen! */
struct _sortelt 
 {
   unsigned short weight;
   unsigned short index;
};

typedef struct _sortelt sortelt;

typedef struct _soln {
  short dx;
  short dy;
} matchelt;



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
  int i,j, p,s;
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
		short tag;
		short weight;
	} searchtablerec;
static short searchtable_fresh_ctr = 0xac45;

static searchtablerec searchtable[SEARCH_TABLE_SIZE];
#define searchentry(x,y) ( searchtable[(x + (y<<LG_MAX_SEARCH_RADIUS)) & (SEARCH_TABLE_SIZE-1)])
#define isfreshentry(x,y) ( searchentry(x,y).tag == searchtable_fresh_ctr  )
#define freshensearchtable searchtable_fresh_ctr++
#define freshsearchtableentry(x,y,w) { searchentry(x,y).tag = searchtable_fresh_ctr;\
                                       searchentry(x,y).weight = w; }
                                        

/*
	Shiny new distance measure absolute difference sum of row/col sums

*/

static int rcdist( unsigned char *org, unsigned char*blk, int x, int y, int lx, int h, int *pdist )
{
	int i,j;
	unsigned char *porg, *pblk;
	int dist = 0;
	int rso, rsb;
	int cso[16], csb[16];
	
	if( isfreshentry(x,y) )
	{
		*pdist = searchentry(x,y).weight;
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
	*pdist = dist;
	return 0;

}



/*
	local_optimum
	Do a 2D binary search for a locally optimal row/column sum difference 
	distance match in the 16-pel box right/below initx inity
	
	TODO: Lots of performance tweaks in the code's structure not all documented...

*/


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
				stale = rcdist( porg + deltax, blk, x+deltax, y, lx, h, &dist );
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
				stale = rcdist( porg + deltay*lx, blk, x, y+deltay, lx, h, &dist );
				allstale &= stale;	
				if( !stale && dist < bestdist && y+deltay <= yhigh)
				{
					bestdist = dist;
					bestdx = 0;
					bestdy = deltay;
				}
				stale = rcdist( porg - deltay*lx, blk, x, y-deltay, lx, h, &dist );
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
								  unsigned char *org, mcompuint *forg, 
								  unsigned char *blk,  mcompuint *fblk, 
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
				dist = dist1_00(org+x+y*lx, blk, lx, h, bestdist);
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
					mcompuint *forgblk;
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
							dist = fdist1( forgblk+(i>>1), fblk, flx, fh );	
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
					dist = dist1_00( orgblk+i, blk, lx, h, bestdist );	
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

/* 
 *   Vector of motion compensations plus a heap of the associated
 *   weights (macro-block distances).
 *  TODO: Should be put into nice tidy records...
 */

#define MAX_COARSE_HEAP_SIZE 100*100

static sortelt half_match_heap[MAX_COARSE_HEAP_SIZE];
static matchelt half_matches[MAX_COARSE_HEAP_SIZE];
static int half_heap_size;
static sortelt quad_match_heap[MAX_COARSE_HEAP_SIZE];
static matchelt quad_matches[MAX_COARSE_HEAP_SIZE];
static int quad_heap_size;
static sortelt rough_match_heap[MAX_COARSE_HEAP_SIZE];
static matchelt rough_matches[MAX_COARSE_HEAP_SIZE];
static int rough_heap_size;


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
  
static int build_quad_heap( int ilow, int ihigh, int jlow, int jhigh, 
							mcompuint *qorg, mcompuint *qblk, int qlx, int qh )
{
  mcompuint *qorgblk;
  mcompuint *old_qorgblk;
  sortelt distrec;
  matchelt matchrec;
  int i,j,k;
  int s1,s2,s3;
  int dist_sum;
  int searched_rough_size;
  int best_quad_dist;

  /* N.b. we may ignore the right hand block of the pair going over the
	 right edge as we have carefully allocated a margin to spare us
	 the check.  The *final* motion compensation calculations
	 performed on the results of this pass *do* filter out
	 out-of-range blocks though...
  */

  rough_heap_size = 0;
  dist_sum = 0;
#if (defined(SSE) || defined(MMX))
  qorgblk = qorg+(ilow>>2)+qlx*(jlow>>2);
  for( j = jlow; j <= jhigh; j += 8 )
	{
  	old_qorgblk = qorgblk;
	  k = 0;
	  for( i = ilow; i <= ihigh; i+= 8  )
		{

		  s1 = qdist1( qorgblk,qblk,qlx,qh);
		  s2 = (s1 >> 16) & 0xffff;
		  s1 = s1 & 0xffff;
		  dist_sum += s1+s2;
		 	rough_match_heap[rough_heap_size].weight = s1;
			rough_match_heap[rough_heap_size].index = rough_heap_size;	
			rough_match_heap[rough_heap_size+1].weight = s2;
		  rough_match_heap[rough_heap_size+1].index = rough_heap_size+1;	
		  rough_matches[rough_heap_size].dx = i;
		  rough_matches[rough_heap_size].dy = j;
			rough_matches[rough_heap_size+1].dx = i+16;
			rough_matches[rough_heap_size+1].dy = j;
			/* A sneaky fast way way of binning out-of-limits estimates ... */
  		rough_heap_size += 1 + (i+16 <= ihigh);

		  /* NTOE:  If you change the grid size you *must* adjust this code... too */
	
		  qorgblk+=2;
	  	  k = (k+2)&3;
		  /* Original branchy code...
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
	
  heapify( rough_match_heap, rough_heap_size );

  best_quad_dist = rough_match_heap[0].weight;	
	  /* 
	 We now use the better-than-average matches on 8-pel boundaries 
	 as starting points for matches on 4-pel boundaries...
  		*/
  quad_heap_size = 0;
  searched_rough_size = 1+rough_heap_size / 3;
  dist_sum = 0;
  for( k = 0; k < searched_rough_size; ++k )
	{
	  heap_extract( rough_match_heap, &rough_heap_size, &distrec );
	  matchrec = rough_matches[distrec.index];
	  qorgblk =  qorg + (matchrec.dy>>2)*qlx +(matchrec.dx>>2);
	  quad_matches[quad_heap_size].dx = matchrec.dx;
	  quad_matches[quad_heap_size].dy = matchrec.dy;
	  quad_match_heap[quad_heap_size].index = quad_heap_size;
	  quad_match_heap[quad_heap_size].weight = distrec.weight;
	  dist_sum += distrec.weight;	
	  ++quad_heap_size;	  

	  for( i = 1; i < 4; ++i )
	 	{
			int x, y;
			x = matchrec.dx + (i & 0x1)*4;
			y = matchrec.dy + (i >>1)*4;
  			s1 = qdist1( qorgblk+(i & 0x1),qblk,qlx,qh) & 0xffff;
			dist_sum += s1;
		    if( s1 < best_quad_dist )
		  		best_quad_dist = s1;

	  		quad_match_heap[quad_heap_size].weight = s1;
			quad_match_heap[quad_heap_size].index = quad_heap_size;
	  		quad_matches[quad_heap_size].dx = x;
	  		quad_matches[quad_heap_size].dy = y;
			quad_heap_size += (x <= ihigh && y <= jhigh);

			if( i == 2 )
				qorgblk += qlx;
	    }

	  /* If we're thresholding check  got a match below the threshold. 
	  	*and* .. stop looking... we've already found a good candidate...
		*/
/*
	  if( fast_mc_threshold && best_quad_dist < quadpel_threshold.threshold )
		{ 
		  break;
		}
*/

	}
#else


    /* Invariant:  qorgblk = qorg+(i>>2)+qlx*(j>>2) */
  qorgblk = qorg+(ilow>>2)+qlx*(jlow>>2);
  for( j = jlow; j <= jhigh; j += 4 )
	{
  	old_qorgblk = qorgblk;
	  for( i = ilow; i <= ihigh; i += 4 )
		{
		  s1 = qdist1( qorgblk,qblk,qlx,qh) & 0xffff;
	  	quad_matches[quad_heap_size].dx = i;
	  	quad_matches[quad_heap_size].dy = j;
	  	quad_match_heap[quad_heap_size].index = quad_heap_size;
	  	quad_match_heap[quad_heap_size].weight =s1;
	  	dist_sum += s1;
	  	++quad_heap_size;
		  qorgblk += 1;
		}
	  qorgblk = old_qorgblk + qlx;
	}

#endif

	/*
  quad_heap_size = thin_vector( quad_match_heap, dist_sum/quad_heap_size,
													      quad_heap_size);
	*/
  heapify( quad_match_heap, quad_heap_size );
  /* Update the fast motion match average using the best 2*2 match */
  /* update_threshold( &quadpel_threshold, quad_match_heap[0].weight ); */

  return quad_heap_size;
}

/* Build a distance based heap of the 2*2 sub-sampled motion
  compensations using the best 4*4 matches as starting points.  As
make   with with the 4*4 matches We don't collect them densely as they're
  just search starting points and ones that are 2 out should still
  give better than average matches...  

  TODO: We should add a flag to
  let the use select between dense and sparse sampling...

*/

static int org_half_heap_size;
static int build_half_heap(int ihigh, int jhigh, 
					        mcompuint *forg,  mcompuint *fblk, 
							int flx, int fh,  int searched_quad_size )
{
  int k,s;
  sortelt distrec;
  matchelt matchrec;
  mcompuint *forgblk;
  int dist_sum  = 0;
  int best_fast_dist = INT_MAX;
  
  half_heap_size = 0;
  for( k = 0; k < searched_quad_size; ++k )
	{
	  heap_extract( quad_match_heap, &quad_heap_size, &distrec );
	  matchrec = quad_matches[distrec.index];
	  forgblk =  forg + (matchrec.dy>>1)*flx +(matchrec.dx>>1);
		  

	  s = fdist1( forgblk,fblk,flx,fh);
	  half_matches[half_heap_size].dx = matchrec.dx;
	  half_matches[half_heap_size].dy = matchrec.dy;
	  half_match_heap[half_heap_size].index = half_heap_size;
	  half_match_heap[half_heap_size].weight = s;
	  if( s < best_fast_dist )
		  best_fast_dist = s;
	  ++half_heap_size;
	  dist_sum += s;
	  
		  
	 if(  matchrec.dy+2 <= jhigh )
	   {
		  s = fdist1( forgblk+flx,fblk,flx,fh);
		  half_matches[half_heap_size].dx = matchrec.dx;
		  half_matches[half_heap_size].dy = matchrec.dy+2;
		  half_match_heap[half_heap_size].index = half_heap_size;
		  half_match_heap[half_heap_size].weight = s;
		  if( s < best_fast_dist )
				best_fast_dist = s;
		  ++half_heap_size;
		  dist_sum += s;
		} 
		  
	  if( matchrec.dx+2 <= ihigh )
		  {
			  s = fdist1( forgblk+1,fblk,flx,fh);
			  half_matches[half_heap_size].dx = matchrec.dx+2;
			  half_matches[half_heap_size].dy = matchrec.dy;
			  half_match_heap[half_heap_size].index = half_heap_size;
			  half_match_heap[half_heap_size].weight = s;
			  if( s < best_fast_dist )
					best_fast_dist = s;
			  ++half_heap_size;
			  dist_sum += s;
			  
			 if( matchrec.dy+2 <= jhigh )
				{
				    s = fdist1( forgblk+flx+1,fblk,flx,fh);
					half_matches[half_heap_size].dx = matchrec.dx+2;
					half_matches[half_heap_size].dy = matchrec.dy+2;
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

  org_half_heap_size = half_heap_size;
  half_heap_size = thin_vector(  half_match_heap, 
																dist_sum / half_heap_size, half_heap_size );
  heapify( half_match_heap, half_heap_size );
  
  /* Update the fast motion match average using the best 2*2 match */
  update_threshold( &twopel_threshold, half_match_heap[0].weight );

  return half_heap_size;
}

/*
  Using the 2*2 matches as starting points we search for
  the best 1-pel.  Note that we have to explore 16 possible
  match for each 2*2 match, as they former are sampled sparsely
  on 4-pel boundaries.

  TODO: should be some flags to control whethere 4*4 and 2*2
  are sampled sparsely or densely.
*/

static void find_best_one_pel( mcompuint *org, mcompuint *blk,
							   int searched_size,
							   int xmax, int ymax,
							   int lx, int h, 
							   int *pdmin, int *pimin, int *pjmin )
{
  int i,j,k;
  int d,rd;
  int imin = *pimin;
  int jmin = *pjmin;
  int dmin = INT_MAX;
  mcompuint *orgblk;
  sortelt distrec;
  matchelt matchrec;
  int count = 0;

    /* Invariant:  qorgblk = qorg+(i>>2)+qlx*(j>>2) */
	/* It can happen (rarely) that the best matches are actually illegal.
		In that case we have to carry on looking... */
	do {
		for( k = 0; k < searched_size; ++k )
		{	

			heap_extract( half_match_heap, &half_heap_size, &distrec );
			matchrec = half_matches[distrec.index];
			orgblk = org + matchrec.dx+lx*matchrec.dy;

			/* Gross hack for pipelined CPU's: we allow the distance
			computation to "go off the edge".  We know this
			won't cause memory faults because the fast motion
			data sits off the end of the ordinary stuff. N.b. we
			*do* exclude any eventual off the edge matches though...
			*/

			if( matchrec.dx <= xmax && matchrec.dy <= ymax )
			{
				d = dist1_00(orgblk,blk,lx,h, dmin);
				if (d<dmin)
				{
					dmin = d;
					imin = matchrec.dx;
					jmin = matchrec.dy;

				}
				d = dist1_00(orgblk+1,blk,lx,h, dmin);
				if (d<dmin && (matchrec.dx < xmax))
				{
					dmin = d;
					imin = matchrec.dx+1;
					jmin = matchrec.dy;

				}

				d = dist1_00(orgblk+lx,blk,lx,h, dmin);
				if( (d<dmin) && (matchrec.dy < ymax))
				{
					dmin = d;
					imin = matchrec.dx;
					jmin = matchrec.dy+1;
	
				}
				d = dist1_00(orgblk+lx+1,blk,lx,h, dmin);
				if ( (d<dmin) && (matchrec.dy < ymax) && (matchrec.dx < xmax))
				{
					dmin = d;
					imin = matchrec.dx+1;
					jmin = matchrec.dy+1;

				}      
			}
					/* DEBUG: Remove once you've pinned down this phenomenon.. */
			if( dmin == INT_MAX )
				{
					printf( "Null updates pos %d %d lim %d %d\n", matchrec.dx, matchrec.dy, xmax, ymax );
				}

		}

		searched_size = half_heap_size;

	} while (half_heap_size>0 && dmin == INT_MAX);

  *pimin = imin;
  *pjmin = jmin;
  *pdmin = dmin;
 }
 
/*
 * full search block matching
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


static int fullsearch(org,ref,blk,fblk,qblk,lx,i0,j0,sx,sy,h,xmax,ymax,iminp,jminp)
unsigned char *org,*ref,*blk;
mcompuint *fblk;
mcompuint *qblk;
int lx,i0,j0,sx,sy,h,xmax,ymax;
int *iminp,*jminp;
{
  int i,j,imin,jmin,ilow,ihigh,jlow,jhigh;
  int ed,d,dmin;
  int dt,dmint;
  int imint,jmint;
  int k,l,sxy;
  int searched_size,s;
#ifndef ORIGINAL_CODE
  int quad_rad_x, quad_rad_y;
  mcompuint *forg = (mcompuint*)(org+width*height);
  mcompuint *qorg = (mcompuint*)(forg+(width>>1)*(height>>1));
  mcompuint *orgblk;
  int flx = lx >> 1;
  int qlx = lx >> 2;
  int fh = h >> 1;
  int qh = h >> 2;
  int quad_mean;
#endif
#ifdef TEST_GRAD_DESCENT
	int seq = 0;
	int seqd;
	double seq_ratio;
	double opt_ratio;
#endif

  sxy = (sx>sy) ? sx : sy;

  /* xmax and ymax into more useful form... */
  xmax -= 16;
  ymax -= h;

  imin = i0;
  jmin = j0;
  
#ifdef ORIGINAL_CODE
	
  ilow = i0 - sx;
  ihigh = i0 + sx;

  if (ilow<0)
    ilow = 0;

  if (ihigh>xmax)
    ihigh = xmax;

  jlow = j0 - sy;
  jhigh = j0 + sy;

  if (jlow<0)
    jlow = 0;

  if (jhigh>ymax)
    jhigh = ymax;

  if(do_not_search)
  {
    *iminp = 2*i0;
    *jminp = 2*j0;
    return dmin;
  }


  /* full pel search, spiraling outwards */

  dmin = dist1(org+imin+lx*jmin,blk,lx,0,0,h,65536);

  for (l=1; l<=sxy; l++)
  {
    i = i0 - l;
    j = j0 - l;
    for (k=0; k<8*l; k++)
    {
      if (i>=ilow && i<=ihigh && j>=jlow && j<=jhigh)
      {
        /* d = dist1(org+i+lx*j,blk,lx,0,0,h, dmin); */
				d = dist1_00(org+i+lx*j,blk,lx,h, dmin); 
        if (d<dmin)
        {
          dmin = d;
          imin = i;
          jmin = j;
        }

      }

      if      (k<2*l) i++;
      else if (k<4*l) j++;
      else if (k<6*l) i--;
      else            j--;
    }
  }
#else
  
  /* Round sx/sy up to a multiple of 8 so that to make life
	 simple in initial fast 4*4 pel searches  */
  quad_rad_x = ((sx + 3) / 4)*4;
  quad_rad_y = ((sy + 3) / 4)*4;
  
 if( (quad_rad_x>>1)*(quad_rad_y>>1) > MAX_COARSE_HEAP_SIZE )
	{
	  fprintf( stderr, "Search radius %d too big for search heap!\n", sxy );
	  exit(1);
	}

  /*
	  Create a distance-order heap of possible motion
	   compensations based on the fast estimation data  -
	   4*4 pel sums (4*4 sub-sampled) rather than actual pel's.  
	   1/16 the size...
	*/
  jlow = j0-quad_rad_y;
  jlow = jlow < 0 ? 0 : jlow;
  jhigh =  j0+quad_rad_y;
  jhigh = jhigh > ymax ? ymax : jhigh;
  ilow = i0-quad_rad_x;
  ilow = ilow < 0 ? 0 : ilow;
  ihigh =  i0+quad_rad_x;
  ihigh = ihigh > xmax ? xmax : ihigh;

  quad_heap_size = build_quad_heap( ilow, ihigh, jlow, jhigh, qorg, qblk, qlx, qh );
  

   /* Now create a distance-ordered heap of possible motion
	 compensations based on the fast estimation data  -
	 2*2 pel sums using the best fraction of the 4*4 estimates
	 However we cover only coarsely... on 4-pel boundaries...
	*/

  searched_size = 1 + quad_heap_size / 3;
  if( searched_size > quad_heap_size )
		searched_size = quad_heap_size;

  half_heap_size = build_half_heap( ihigh, jhigh, forg, fblk, flx, fh, searched_size );

  /* Now choose best 1-pel match from what approximates (not exact due
	   to the pre-processing trick with the mean) 
	   the top 1/2 of the 2*2 matches
	*/
  

#ifdef TEST_GRAD_DESCENT
  searched_size = half_heap_size;
#else
  searched_size = 1+ (half_heap_size / fast_mc_frac);
  if( searched_size > half_heap_size )
  	searched_size = half_heap_size;
#endif

	dmin = INT_MAX;
  
 	/* Very rarely this may fail to find matchs due to all the good looking ones
		being "off the edge".... I ignore this as its so rare and the necessary tests
		costing significant time
		*/
  find_best_one_pel( org, blk, searched_size,
					           ihigh, jhigh, lx, h, &dmin, &imin, &jmin );

#ifdef TEST_GRAD_DESCENT
	{
		int rcimin, rcjmin;
		int rfrcdist, rcrcdist, rcdmin ;
		best_graddesc_match( ilow, ihigh, jlow, jhigh, org, forg, blk, fblk, lx, h, &rcdmin, &rcimin, &rcjmin);
	  if( rcimin != imin || rcjmin != jmin )
		{
			++miss_cnt;
			/* Subtlety here... these calls will use info from the search table unless it has been previously
				freshened (purged) 
				*/
	
			/*rcdist( org+imin+lx*jmin, blk, imin, jmin, lx, h, &rfrcdist );
			rcdist( org+rcimin+lx*rcjmin, blk, rcimin, rcjmin, lx, h, &rcrcdist  );
			*/
			/*
			 printf( "Disagreement! %d/%d REF %d %d %d  GD  %d %d %d \n", 
				miss_cnt, seq_cnt, imin, jmin, dmin, 
				rcimin, rcjmin, rcdmin );
			*/
			seq_sum += abs(rcimin-imin) + abs(rcjmin-jmin);
	
		}
		if( rcimin == imin && rcjmin == jmin && dmin != rcdmin && dmin != INT_MAX )
			{
				printf( "\n****A %d %d dmin=%d rcdmin=%d!\n", imin, jmin, dmin, rcdmin);
			}
		if( dist1_00(org+rcimin+lx*rcjmin, blk, lx, h, INT_MAX ) != rcdmin )
			{
				printf( "INCONSISTENT DISTANCE MEASURE B!\n");
			}
  	opt_ratio = ((double)(rcdmin))/((double)dmin);
  	ratio_sum += opt_ratio;
  	if( opt_ratio > worst_seq )
			worst_seq = opt_ratio;
		++seq_cnt;
  
	}
#endif

#endif

#ifdef TEST_MOTION_SEARCH	
	/* TEST reference motion compensation... just search all 1-pel matches and compare */
	dmint = INT_MAX;
	for( j = jlow; j <= jhigh; ++j )
  		for( i = ilow; i <= ihigh; ++i )
			{
				ed = dist1_00(org+i+lx*j, blk, lx, h, dmint );
				if( ed < dmint )
					{
						dmint = ed;
						imint = i;
						jmint = j;
					}			
			
			}
	if( dmint < dmin )
	{
		printf( "(%d,%d) [%d %d %d %d] opt %d %d (%d) fnd %d %d (%d)!\n",
				i0, j0,
				ilow, ihigh, jlow, jhigh, imint, jmint, dmint, imin, jmin, dmin );
	}
#endif
  /* Final polish: half-pel search of best candidate against reconstructed
   image.
  A.Stevens 2000: Why don't we do the rest of the motion comp search against
  the reconstructed image? Weird...
  */

  imin <<= 1;
  jmin <<= 1;

  ilow = imin - (imin>0);
  ihigh = imin + (imin<((xmax)<<1));
  jlow = jmin - (jmin>0);
  jhigh = jmin + (jmin<((ymax)<<1));

  for (j=jlow; j<=jhigh; j++)
	{
	  for (i=ilow; i<=ihigh; i++)
		{
		  orgblk = org+(i>>1)+((j>>1)*lx);
		  /* ORIGINALLY: d = dist1(orgblk,blk,lx,(i&1),(j&1),h,dmin);
			 However, this provokes a bug in egcs 1.1.2...
		  */
		  if( i&1 )
			{
			  if( j & 1 )
				d = dist1_11(orgblk,blk,lx,h);
			  else
				d = dist1_01(orgblk,blk,lx,h);
			}
		  else
			{
			  if( j & 1 )
				d = dist1_10(orgblk,blk,lx,h);
			  else
				d = dist1_00(orgblk,blk,lx,h,dmin);
			}
		  if (d<dmin)
			{
			  dmin = d;
			  imin = i;
			  jmin = j;
			}
		}
	}
  
  if( imin < 0 || imin > xmax*2 || jmin > ymax*2 || jmin < 0 )
	{
	  printf( "Out of bounds suggestions B %d %d %d  %d %d %d %d X%d Y%d!\n", 
			  imin, jmin, dmin,
			  ilow, ihigh, jlow, jhigh,
				  xmax*2, ymax*2
			  );
	  exit(0);
	}


  *iminp = imin;
  *jminp = jmin;

  return dmin;
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

#if  !defined(SSE) && !defined(MMX)
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
#ifndef ORIGINAL_CODE
  for (j=0; j<h; j++)
	{
	  if ((v = p1[0]  - p2[0])<0)  v = -v; s+= v;
	  if ((v = p1[1]  - p2[1])<0)  v = -v; s+= v;
	  if ((v = p1[2]  - p2[2])<0)  v = -v; s+= v;
	  if ((v = p1[3]  - p2[3])<0)  v = -v; s+= v;
	  if ((v = p1[4]  - p2[4])<0)  v = -v; s+= v;
	  if ((v = p1[5]  - p2[5])<0)  v = -v; s+= v;
	  if ((v = p1[6]  - p2[6])<0)  v = -v; s+= v;
	  if ((v = p1[7]  - p2[7])<0)  v = -v; s+= v;
	  if ((v = p1[8]  - p2[8])<0)  v = -v; s+= v;
	  if ((v = p1[9]  - p2[9])<0)  v = -v; s+= v;
	  if ((v = p1[10] - p2[10])<0) v = -v; s+= v;
	  if ((v = p1[11] - p2[11])<0) v = -v; s+= v;
	  if ((v = p1[12] - p2[12])<0) v = -v; s+= v;
	  if ((v = p1[13] - p2[13])<0) v = -v; s+= v;
	  if ((v = p1[14] - p2[14])<0) v = -v; s+= v;
	  if ((v = p1[15] - p2[15])<0) v = -v; s+= v;
#else
#define pipestep(o) v = p1[o]-p2[o]; s+= fastabs(v);
	  pipestep(0);  pipestep(1);  pipestep(2);  pipestep(3);
	  pipestep(4);  pipestep(5);  pipestep(6);  pipestep(7);
	  pipestep(8);  pipestep(9);  pipestep(10); pipestep(11);
	  pipestep(12); pipestep(13); pipestep(14); pipestep(15);
#undef pipestep
#endif

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
#ifdef ORIGINAL_CODE
			  if (v>=0)
				s+= v;
			  else
				s-= v;
#else
			  s+=fastabs(v);
#endif
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
#ifdef ORIGINAL_CODE
			  if (v>=0)
				s+= v;
			  else
				s-= v;
#else
			  s+=fastabs(v);
#endif
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
#ifdef ORIGINAL_CODE
		  if (v>=0)
			s+= v;
		  else
			s-= v;
#else
		  s+=fastabs(v);
#endif
		}
	  p1 = p1a;
	  p1a+= lx;
	  p2+= lx;
	}
  return s;
}
#endif

static int dist1(blk1,blk2,lx,hx,hy,h,distlim)
unsigned char *blk1,*blk2;
int lx,hx,hy,h;
int distlim;
{
  int s;
  if (hx && !hy)
	{
	  s = dist1_01( blk1,blk2,lx,h);
	}
  else if (!hx && !hy)
	{
	  s = dist1_00( blk1,blk2,lx,h,distlim );
	}
  else if (!hx && hy)
	{
	  s = dist1_10( blk1,blk2,lx,h);
	}
  else /* if (hx && hy) */
	{
	  s = dist1_11( blk1,blk2,lx,h);
	}
  return s;
}

void check_fast_motion_data(unsigned char *blk, char *label )
{
  unsigned char *b, *nb;
  mcompuint *pb,*p;
  mcompuint *qb;
  mcompuint *start_fblk, *start_qblk;
  int i;
  unsigned int mismatch;
  int nextfieldline;
  start_fblk = (blk+height*width);
  start_qblk = (blk+height*width+(height>>1)*(width>>1));
 /* In an interlaced field the "next" line is 2 width's down 
     rather than 1 width down                                 */

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

/* 
   Append fast motion estimation data to original luminance
   data.  N.b. memory allocation for luminance data allows space
   for this information...
 */

void fast_motion_data(unsigned char *blk, int picture_struct )
{
  unsigned char *b, *nb;
  mcompuint *pb,*p;
  mcompuint *qb;
  mcompuint *start_fblk, *start_qblk;
  int i;
  int nextfieldline;

  

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

	/*printf( "fcomp data for block %08x\n", blk );*/
  start_fblk = (blk+height*width);
  start_qblk = (blk+height*width+(height>>1)*(width>>1));
  b = blk;
  nb = (blk+nextfieldline);
  /* Sneaky stuff here... we can do lines in both fields at once */
  pb = (mcompuint *) start_fblk;

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
		  /* TODO: A.Stevens - this only works for mcompuint = unsigned char */
		  qb[0] = (b[0]+b[1]+nb[0]+nb[1])>>2;
			qb[1] = (b[2]+b[3]+nb[2]+nb[3])>>2;
		  qb += 2;
		  b += 4;
		  nb += 4;
		}
	  b += nextfieldline;
	  nb = b + nextfieldline;
	}
	


}

#if !defined(SSE) && !defined(MMX)

static int fdist1( mcompuint *fblk1, mcompuint *fblk2,int flx,int fh)
{
  mcompuint *p1 = fblk1;
  mcompuint *p2 = fblk2;
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
	}

  return s;
}



/*
  Sum absolute differences for 4*4 sub-sampled data.  

  TODO: currently assumes  only 16*16 or 16*8 motion compensation will be used...
  I.e. 4*4 or 4*2 sub-sampled blocks will be compared.
 */


static int qdist1( mcompuint *qblk1, mcompuint *qblk2,int qlx,int qh)
{
  register mcompuint *p1 = qblk1;
  register mcompuint *p2 = qblk2;
  int s = 0;
  int s1;
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

  /* TODO DEBUGGING only remove...
  s1 =  qdist1_MMX( qblk1,qblk2,qlx,qh);
  if ( s != s1 )
	{
	  printf( "qh = %d qlx=%d I = %d M = %d\n", qh, qlx,  s, s1);
	}
  */
  return s;
}
#endif

/*
 * total squared difference between two (16*h) blocks
 * including optional half pel interpolation of blk1 (hx,hy)
 * blk1,blk2: addresses of top left pels of both blocks
 * lx:        distance (in bytes) of vertically adjacent pels
 * hx,hy:     flags for horizontal and/or vertical interpolation
 * h:         height of block (usually 8 or 16)
 */
 
#if (!defined(SSE) && !defined(MMX))
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
#ifdef ORIGINAL_CODE
      if (v>=0)
        s+= v;
      else
        s-= v;
#else
	  s += fastabs(v);
#endif
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
#endif

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
