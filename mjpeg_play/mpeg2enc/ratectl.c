/* ratectl.c, bitrate control routines (linear quantization only currently) */

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

#include <config.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#include "global.h"
#include "fastintfns.h"

/* private prototypes */

static double calc_actj ( pict_data_s *picture);

/*Constant bit-rate rate control variables */
/* X's global complexity (Chi! not X!) measures.
   Actually: average quantisation * bits allocated in *previous* frame
   N.b. the choice of measure is *not* arbitrary.  The feedback bit rate
   control gets horribly messed up if it is *not* proportionate to bit demand
   i.e. bits used scaled for quantisation.
   d's  virtual reciever buffer fullness
   r - Rate control feedback gain (in bits/frame)  I.e if the "virtual
   buffer" is x% full and
*/

double Xi = 0, Xp = 0, Xb = 0;
int r;
int	d0i = 0, d0pb = 0;


/* R - Remaining bits available in GOP
   T - Target bits for current frame 
   d - Current d for quantisation purposes updated using 
   scaled difference of target bit usage and actual usage
   NOTE: We use double's for these as 48 bits will suffice and
   there's some tricky float arithmetic required/

*/
static double R;
static int d;
static double T;

/* bitcnt_EOP - Position in generated bit-stream for latest
				end-of-picture Comparing these values with the
				bit-stream position for when the picture is due to be
				displayed allows us to see what the vbv buffer is up
				to.

   gop_undershoot - If we *undershoot* our bit target the vbv buffer
   				calculations based on the actual length of the
   				bitstream will be wrong because in the final system
   				stream these bits will be padded away.  I.e. frames
   				*won't* arrive as early as the length of the video
   				stream would suggest they would.  To get it right we
   				have to keep track of the bits that would appear in
   				padding.
				
										
*/

static int64_t bitcnt_EOP = 0;
static int gop_undershoot = 0;

/*
  actsum - Total activity (sum block variances) in frame
  actcovered - Activity macroblocks so far quantised (used to
               fine tune quantisation to avoid starving highly
			   active blocks appearing late in frame...) UNUSED
  avg_act - Current average activity...
*/
static double actsum;
static double actcovered;
double sum_avg_act = 0.0;
double avg_act;
double peak_act;

static int Np, Nb;
static int64_t S;

/* Note: eventually we may wish to tweak these to suit image content */
static double Ki;    	/* Down-scaling of I/B/P-frame complexity */
static double Kb;	    /* relative to others in bit-allocation   */
static double Kp;	    /* calculations.  We only need 2 but have all
							   3 for readability */


static int min_d,max_d;
static int min_q, max_q;

/* TODO EXPERIMENT */
static double avg_KI = 2.5;	/* TODO: These values empirically determined 		*/
static double avg_KB = 10.0;	/* for MPEG-1, may need tuning for MPEG-2	*/
static double avg_KP = 10.0;
#define K_AVG_WINDOW_I 4.0		/* TODO: MPEG-1, hard-wired settings */
#define K_AVG_WINDOW_P   10.0
#define K_AVG_WINDOW_B   20.0
static double bits_per_mb;

static double SQ = 0.0;
static double AQ = 0.0;


/* Scaling and clipping of quantisation factors
   One floating point used in predictive calculations.  
   The second integerises the results of the first for use in the
   actual stream...
*/

#ifdef REDUNDANT_BUT_MAY_COME_ION_HANDY

static double scale_quantf( pict_data_s *picture, double quant )
{
	double quantf;

	if ( picture->q_scale_type )
	{
		int iquantl, iquanth;
		double wl, wh;

		/* BUG TODO: This should interpolate the table... */
		wh = quant-floor(quant);
		wl = 1.0 - wh;
		iquantl = (int) floor(quant);
		iquanth = iquantl+1;
		/* clip to legal (linear) range */
		if (iquantl<1)
			iquantl = 1;
		if (iquanth>112)
			iquanth = 112;
		
		quantf = (double)
		  wl * (double)non_linear_mquant_table[map_non_linear_mquant[iquantl]] 
			+ 
		  wh * (double)non_linear_mquant_table[map_non_linear_mquant[iquantl]]
			;
	}
	else
	{
		/* clip mquant to legal (linear) range */
		quantf = quant;
		if (quantf<2.0)
			quantf = 2;
		if (quantf>62.0)
			quantf = 62.0;
	}
	return quantf;
}

#endif

static int scale_quant( pict_data_s *picture, double quant )
{
	int iquant;
	
	if ( picture->q_scale_type  )
	{
		iquant = (int) floor(quant+0.5);

		/* clip mquant to legal (linear) range */
		if (iquant<1)
			iquant = 1;
		if (iquant>112)
			iquant = 112;

		iquant =
			non_linear_mquant_table[map_non_linear_mquant[iquant]];
	}
	else
	{
		/* clip mquant to legal (linear) range */
		iquant = (int)floor(quant+0.5);
		if (iquant<2)
			iquant = 2;
		if (iquant>62)
			iquant = 62;
		iquant = (iquant/2)*2; // Must be *even*
	}
	return iquant;
}

/*********************

 Initialise rate control parameters
 params:  reinit - Rate control is being re-initialised during the middle
                   of a run.  Don't reset adaptive parameters.
 ********************/

void rc_init_seq(int reinit)
{

	/* No bits unused from previous GOP....*/
	R = 0;

	/* Everything else already set or adaptive */
	if( reinit )
		return;

	bits_per_mb = (double)bit_rate / (mb_per_pict);

	/* reaction parameter (constant) decreased to increase response
	   rate as encoder is currently tending to under/over-shoot... in
	   rate TODO: Reaction parameter is *same* for every frame type
	   despite different weightings...  */
	if (r==0)  
		r = (int)floor(2.0*bit_rate/frame_rate + 0.5);

	Ki = 1.2;
	Kb = 1.4;
	Kp = 1.1;



	/* Heuristic: In constant bit-rate streams we assume buffering
	   will allow us to pre-load some (probably small) fraction of the
	   buffers size worth of following data if earlier data was
	   undershot its bit-rate allocation
	 
	*/


	/* global complexity (Chi! not X!) measure of different frame types */
	/* These are just some sort-of sensible initial values for start-up */

	Xi = 1500*mb_per_pict;   /* Empirically derived values */
	Xp = 550*mb_per_pict;
	Xb = 170*mb_per_pict;
	d0i = -1;				/* Force initial Quant prediction */
	d0pb = -1;


#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: sequence initialization\n");
	fprintf(statfile,
			" initial global complexity measures (I,P,B): Xi=%.0f, Xp=%.0f, Xb=%.0f\n",
			Xi, Xp, Xb);
	fprintf(statfile," reaction parameter: r=%d\n", r);
	fprintf(statfile,
			" initial virtual buffer fullness (I,P,B): d0i=%d, d0pb=%d",
			d0i, d0pb);
#endif

}

void rc_init_GOP(np,nb)
	int np,nb;
{
	double per_gop_bits = 
		(double)(1 + np + nb) * (double)bit_rate / frame_rate;

	/* A.Stevens Aug 2000: at last I've found the wretched
	   rate-control overshoot bug...  Simply "topping up" R here means
	   that we can accumulate an indefinately large pool of bits
	   "saved" from previous low-activity frames.  This is of
	   course nonsense.

	   In CBR we can only accumulate as much as our buffer allows, after that
	   the eventual system stream will have to be padded.  The automatic padding
	   will make this calculation fairly reasonable but since that's based on 
	   estimates we again impose our rough and ready heuristic that we can't
	   accumulate more than approximately half  a video buffer full.

	   In VBR we actually do nothing different.  Here the bitrate is
	   simply a ceiling rate which we expect to undershoot a lot as
	   our quantisation floor cuts in. We specify a great big buffer
	   and simply don't pad when we undershoot.

	   However, we don't want to carry over absurd undershoots as when it
	   does get hectic we'll breach our maximum.
		
	   TODO: For CBR we should do a proper video buffer model and use
	   it to make bit allocation decisions.

	*/

	if( R > 0  )
	{
		/* We replacing running estimate of undershoot with
		   *exact* value and use that for calculating how much we
		   may "carry over"
		*/
		gop_undershoot = intmin( video_buffer_size/2, (int)R );

		R = gop_undershoot + per_gop_bits;		
	}
	else 
	{
		/* Overshoots are easy - we have to make up the bits */
		R +=  per_gop_bits;
		gop_undershoot = 0;
	}

	Np = fieldpic ? 2*np+1 : np;
	Nb = fieldpic ? 2*nb : nb;

#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: new group of pictures (GOP)\n");
	fprintf(statfile," target number of bits for GOP: R=%.0f\n",R);
	fprintf(statfile," number of P pictures in GOP: Np=%d\n",Np);
	fprintf(statfile," number of B pictures in GOP: Nb=%d\n",Nb);
#endif
}



/* Step 1: compute target bits for current picture being coded */
void rc_init_pict(pict_data_s *picture)
{
	double avg_K;
	double target_Q;
	double current_Q;
	double Si, Sp, Sb;
	/* TODO: A.Stevens  Nov 2000 - This modification needs testing visually.

	   Weird.  The original code used the average activity of the
	   *previous* frame as the basis for quantisation calculations for
	   rather than the activity in the *current* frame.  That *has* to
	   be a bad idea..., surely, here we try to be smarter by using the
	   current values and keeping track of how much of the frames
	   activitity has been covered as we go along.

	   We also guesstimate the relationship between  (sum
	   of DCT coefficients) and actual quantisation weighted activty.
	   We use this to try to predict the activity of each frame.
	*/

	actsum =  calc_actj(picture );
	avg_act = (double)actsum/(double)(mb_per_pict);
	sum_avg_act += avg_act;
	actcovered = 0.0;

	/* Allocate target bits for frame based on frames numbers in GOP
	   weighted by global complexity estimates and B-frame scale factor
	   T = (Nx * Xx/Kx) / Sigma_j (Nj * Xj / Kj)
	*/
	min_q = min_d = INT_MAX;
	max_q = max_d = INT_MIN;
	switch (picture->pict_type)
	{
	case I_TYPE:
		
		/* There is little reason to rely on the *last* I-frame
		   as they're not closely related.  The slow correction of
		   K should be enough to fine-tune...
		*/

		d = d0i;
		avg_K = avg_KI;
		Si = (Xi + 3.0*avg_K*actsum)/4.0;
		T = R/(1.0+Np*Xp*Ki/(Si*Kp)+Nb*Xb*Ki/(Si*Kb));

		break;
	case P_TYPE:
		d = d0pb;
		avg_K = avg_KP;
		Sp = (Xp + avg_K*actsum) / 2.0;
		T =  R/(Np+Nb*Kp*Xb/(Kb*Sp)) + 0.5;
		break;
	case B_TYPE:
		d = d0pb;            // I and P frame share ratectl virtual buffer
		avg_K = avg_KB;
		Sb = Xb /* + avg_K * actsum) / 2.0 */;
		T =  R/(Nb+Np*Kb*Xp/(Kp*Sb));
		break;
	}

	/* Undershot bits have been "returned" via R */
	if( d < 0 )
		d = 0;

	/* We don't let the target volume get absurdly low as it makes some
	   of the prediction maths ill-condtioned.  At these levels quantisation
	   is always minimum anyway
	*/
	if( T < 4000.0 )
	{
		T = 4000.0;
	}
	target_Q = scale_quant(picture, 
						   avg_K * avg_act *(mb_per_pict) / T);
	current_Q = scale_quant(picture,62.0*d / r);
#ifdef DEBUG
	if( !quiet )
	{
		/* printf( "AA=%3.4f T=%6.0f K=%.1f ",avg_act, (double)T, avg_K  ); */
		printf( "AA=%3.4f SA==%3.4f ",avg_act, sum_avg_act  ); 
	}
#endif	
	
	if ( current_Q < 3 && target_Q > 12 )
	{
		/* We're undershooting and a serious surge in the data_flow
		   due to lagging adjustment is possible...
		*/
		d = (int) (target_Q * r / 62.0);
	}

	S = bitcount();


#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: start of picture\n");
	fprintf(statfile," target number of bits: T=%.0f\n",T/8);
#endif
}



static double calc_actj(pict_data_s *picture)
{
	int i,j,k,l;
	double actj,sum;
	uint16_t *i_q_mat;
	int actsum;
	sum = 0.0;
	k = 0;

	for (j=0; j<height2; j+=16)
		for (i=0; i<width; i+=16)
		{
			/* A.Stevens Jul 2000 Luminance variance *has* to be a rotten measure
			   of how active a block in terms of bits needed to code a lossless DCT.
			   E.g. a half-white half-black block has a maximal variance but 
			   pretty small DCT coefficients.

			   So.... we use the absolute sum of DCT coefficients as our
			   variance measure.  
			*/
			if( picture->mbinfo[k].mb_type  & MB_INTRA )
			{
				i_q_mat = i_intra_q;
				/* EXPERIMENT: See what happens if we compensate for
				 the wholly disproprotionate weight of the DC
				 coefficients.  Shold produce more sensible results...  */
				actsum =  -80*COEFFSUM_SCALE;
			}
			else
			{
				i_q_mat = i_inter_q;
				actsum = 0;
			}

			/* It takes some bits to code even an entirely zero block...
			   It also makes a lot of calculations a lot better conditioned
			   if it can be guaranteed that activity is always distinctly
			   non-zero.
			 */


			for( l = 0; l < 6; ++l )
				actsum += 
					(*pquant_weight_coeff_sum)
					    ( picture->mbinfo[k].dctblocks[l], i_q_mat ) ;
			actj = (double)actsum / (double)COEFFSUM_SCALE;
			if( actj < 12.0 )
				actj = 12.0;

			picture->mbinfo[k].act = (double)actj;
			sum += (double)actj;
			++k;
		}
	return sum;
}

/*
 * Update rate-controls statistics after pictures has ended..
 *
 */
void rc_update_pict(pict_data_s *picture)
{
	double X;
	double K;
	int64_t AP,PP;		/* Actual and padded picture bit counts */
	int    i;
	int    Qsum;
	int frame_overshoot;
	
	
	AP = bitcount() - S;
	frame_overshoot = (int)AP-(int)T;

	/* For the virtual buffers for quantisation feedback it is the
	   actual under/overshoot that counts, not what's left after padding
	*/
	d += frame_overshoot;
	
	/* If the cummulative undershoot is getting too large (as
	   a rough and ready heuristic we use 1/2 video buffer size)
	   we start padding the stream.  Or, in the case of VBR,
	   we pretend we're padding but don't actually write anything!

	 */

	if( gop_undershoot-frame_overshoot > video_buffer_size/2 )
	{
		int padding_bytes = 
			((gop_undershoot-frame_overshoot)-video_buffer_size/2)/8;
		if( quant_floor != 0 )	/* VBR case pretend to pad */
		{
			PP = AP + padding_bytes;
		}
		else
		{
			printf( "PAD" );
			alignbits();
			for( i = 0; i < padding_bytes/2; ++i )
			{
				putbits(0, 16);
			}
			PP = bitcount() - S;			/* total # of bits in picture */
		}
		frame_overshoot = (int)PP-(int)T;
	}
	else
		PP = AP;
	
	/* Estimate cummulative undershoot within this gop. 
	   This is only an estimate because T includes an allocation
	   from earlier undershoots causing multiple counting.
	   Padding and an exact calculation each gop prevent the error
	   in the estimate growing too excessive...
	   */
	gop_undershoot -= frame_overshoot;
	gop_undershoot = gop_undershoot > 0 ? gop_undershoot : 0;
	R-= PP;						/* remaining # of bits in GOP */

	Qsum = 0;
	for( i = 0; i < mb_per_pict; ++i )
	{
		Qsum += picture->mbinfo[i].mquant;
	}


	AQ = (double)Qsum/(double)mb_per_pict;
	/* TODO: The X are used as relative activity measures... so why
	   bother dividing by 2?  
	   Note we have to be careful to measure the actual data not the
	   padding too!
	*/
	SQ += AQ;
	X = (double)AP*(AQ/2.0);
	
	K = X / actsum;
#ifdef DEBUG
	if( !quiet )
	{
		printf( "AQ=%.1f SQ=%.2f",  AQ,SQ);
	}
#endif
	/* Bits that never got used in the past can't be resurrected
	   now...  We use an average of past (positive) virtual buffer
	   fullness in the event of an under-shoot as otherwise we will
	   tend to over-shoot heavily when activity picks up.
	 
	   TODO: We should really use our estimate K[IBP] of
	   bit_usage*activity / quantisation ratio to set a "sensible"
	   initial d to achieve a reasonable initial quantisation. Rather
	   than have to cut in a huge (lagging correction).

	   Alternatively, simply requantising with mean buffer if there is
	   a big buffer swing would work nicely...

	*/
	
	/* EXPERIMENT: Xi are used as a guesstimate of likely *future* frame
	   activities based on the past.  Thus we don't want anomalous outliers
	   due to scene changes swinging things too much.  Introduce moving averages
	   for the Xi...
	   TODO: The averaging constants should be adjust to suit relative frame
	   frequencies...
	*/
	switch (picture->pict_type)
	{
	case I_TYPE:
		avg_KI = (K + avg_KI * K_AVG_WINDOW_I) / (K_AVG_WINDOW_I+1.0) ;
		d0i = d;
		Xi = (X + 3.0*Xi)/4.0;
		break;
	case P_TYPE:
		avg_KP = (K + avg_KP * K_AVG_WINDOW_P) / (K_AVG_WINDOW_P+1.0) ;
		d0pb = d;
		Xp = (X + Xp*12.0)/13.0;
		Np--;
		break;
	case B_TYPE:
		avg_KB = (K + avg_KB * K_AVG_WINDOW_B) / (K_AVG_WINDOW_B+1.0) ;
		d0pb = d;
		Xb = (X + Xb*24.0)/25.0;
		Nb--;
		break;
	}
#ifdef DEBUG
	if( !quiet )
		printf( "\n" );
#else
	if( !quiet )
	{
		printf( "\r" );
		fflush( stdout );
	}	
#endif
#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: end of picture\n");
	fprintf(statfile," actual number of bits: S=%lld\n",S);
	fprintf(statfile," average quantization parameter AQ=%.1f\n",
			(double)AQ);
	fprintf(statfile," remaining number of bits in GOP: R=%.0f\n",R);
	fprintf(statfile,
			" global complexity measures (I,P,B): Xi=%.0f, Xp=%.0f, Xb=%.0f\n",
			Xi, Xp, Xb);
	fprintf(statfile,
			" virtual buffer fullness (I,PB): d0i=%d, d0pb=%d\n",
			d0i, d0pb);
	fprintf(statfile," remaining number of P pictures in GOP: Np=%d\n",Np);
	fprintf(statfile," remaining number of B pictures in GOP: Nb=%d\n",Nb);
	fprintf(statfile," average activity: avg_act=%.1f \n", avg_act );
#endif

}

/* compute initial quantization stepsize (at the beginning of picture) 
   quant_floor != 0 is the VBR case where we set a bitrate as a (high)
   maximum and then put a floor on quantisation to achieve a reasonable
   overall size.
 */
int rc_start_mb(pict_data_s *picture)
{
	
	int mquant = scale_quant( picture, d*62.0/r );
	mquant = intmax(mquant, quant_floor);

/*
  fprintf(statfile,"rc_start_mb:\n");
  fprintf(statfile,"mquant=%d\n",mquant);
*/
	return mquant;
}

/* Step 2: measure virtual buffer - estimated buffer discrepancy */
int rc_calc_mquant( pict_data_s *picture,int j)
{
	int mquant;
	double dj, Qj, actj, N_actj; 

	/* A.Stevens 2000 : we measure how much *information* (total activity)
	   has been covered and aim to release bits in proportion.  Indeed,
	   complex blocks get an disproprortionate boost of allocated bits.
	   To avoid visible "ringing" effects...

	*/

	actj = picture->mbinfo[j].act;
	/* Guesstimate a virtual buffer fullness based on
	   bits used vs. bits in proportion to activity encoded
	*/


	dj = ((double)d) + 
		((double)(bitcount()-S) - actcovered * ((double)T) / actsum);



	/* scale against dynamic range of mquant and the bits/picture count.
	   quant_floor != 0.0 is the VBR case where we set a bitrate as a (high)
	   maximum and then put a floor on quantisation to achieve a reasonable
	   overall size.
	   Not that this *is* baseline quantisation.  Not adjust for local activity.
	   Otherwise we end up blurring active macroblocks. Silly in a VBR context.
	*/

	Qj = dj*62.0/r;

	Qj = (Qj > quant_floor) ? Qj : quant_floor;
	/*  Heuristic: Decrease quantisation for blocks with lots of
		sizeable coefficients.  We assume we just get a mess if
		a complex texture's coefficients get chopped...
	*/
		
	N_actj =  actj < avg_act ? 
		1.0 : 
		(actj + act_boost*avg_act)/(act_boost*actj +  avg_act);
   
	mquant = scale_quant(picture,Qj*N_actj);


	/* Update activity covered */

	actcovered += actj;
	 
#ifdef OUTPUT_STAT
/*
  fprintf(statfile,"MQ(%d): ",j);
  fprintf(statfile,"dj=%.0f, Qj=%1.1f, actj=3.1%f, N_actj=1.1%f, mquant=%03d\n",
  dj,Qj,actj,N_actj,mquant);
*/
	picture->mbinfo[j].N_act = N_actj;
#endif
	return mquant;
}


/* VBV calculations
 *
 * generates warnings if underflow or overflow occurs
 */

/* vbv_end_of_picture
 *
 * - has to be called directly after writing picture_data()
 * - needed for accurate VBV buffer overflow calculation
 * - assumes there is no byte stuffing prior to the next start code
 *
 * Note correction for bytes that will be stuffed away in the eventual CBR
 * bit-stream.
 */

void vbv_end_of_picture(pict_data_s *picture)
{
	bitcnt_EOP = (bitcount()) - BITCOUNT_OFFSET;

	/* A.Stevens why bother... accuracy to 7 bits... ha ha!
	vbitcnt_EOP = (vbitcnt_EOP + 7) & ~7; 
	*/
}

/* calc_vbv_delay
 *
 * has to be called directly after writing the picture start code, the
 * reference point for vbv_delay
 *
 * A.Stevens 2000: 
 * Actually we call it just before the start code is written, but anyone
 * who thinks 32 bits +/- in all these other approximations matters is fooling
 * themselves.
 */

void calc_vbv_delay(pict_data_s *picture)
{
	static double picture_delay;
	static double next_ip_delay = 0.0; /* due to frame reordering delay */
	static double decoding_time = 0.0;

	
	/* number of 1/90000 s ticks until next picture is to be decoded */
	if (picture->pict_type == B_TYPE)
	{
		if (prog_seq)
		{
			if (!picture->repeatfirst)
				picture_delay = 90000.0/frame_rate; /* 1 frame */
			else
			{
				if (!picture->topfirst)
					picture_delay = 90000.0*2.0/frame_rate; /* 2 frames */
				else
					picture_delay = 90000.0*3.0/frame_rate; /* 3 frames */
			}
		}
		else
		{
			/* interlaced */
			if (fieldpic)
				picture_delay = 90000.0/(2.0*frame_rate); /* 1 field */
			else
			{
				if (!picture->repeatfirst)
					picture_delay = 90000.0*2.0/(2.0*frame_rate); /* 2 flds */
				else
					picture_delay = 90000.0*3.0/(2.0*frame_rate); /* 3 flds */
			}
		}
	}
	else
	{
		/* I or P picture */
		if (fieldpic)
		{
			if(picture->topfirst==(picture->pict_struct==TOP_FIELD))
			{
				/* first field */
				picture_delay = 90000.0/(2.0*frame_rate);
			}
			else
			{
				/* second field */
				/* take frame reordering delay into account */
				picture_delay = next_ip_delay - 90000.0/(2.0*frame_rate);
			}
		}
		else
		{
			/* frame picture */
			/* take frame reordering delay into account*/
			picture_delay = next_ip_delay;
		}

		if (!fieldpic || 
			picture->topfirst!=(picture->pict_struct==TOP_FIELD))
		{
			/* frame picture or second field */
			if (prog_seq)
			{
				if (!picture->repeatfirst)
					next_ip_delay = 90000.0/frame_rate;
				else
				{
					if (!picture->topfirst)
						next_ip_delay = 90000.0*2.0/frame_rate;
					else
						next_ip_delay = 90000.0*3.0/frame_rate;
				}
			}
			else
			{
				if (fieldpic)
					next_ip_delay = 90000.0/(2.0*frame_rate);
				else
				{
					if (!picture->repeatfirst)
						next_ip_delay = 90000.0*2.0/(2.0*frame_rate);
					else
						next_ip_delay = 90000.0*3.0/(2.0*frame_rate);
				}
			}
		}
	}

	if (decoding_time==0.0)
	{
		/* first call of calc_vbv_delay */
		/* we start with a 7/8 filled VBV buffer (12.5% back-off) */
		picture_delay = ((vbv_buffer_size*7)/8)*90000.0/bit_rate;
		if (fieldpic)
			next_ip_delay = (int)(90000.0/frame_rate+0.5);
	}

	/* VBV checks */

	/*
	   TODO: This is currently disabled because it is hopeless wrong
	   most of the time. It generates 20 warnings for frames with small
	   predecessors (small bitcnt_EOP) that in reality would be padded
	   away by the multiplexer for every realistic warning for an
	   oversize packet.
	*/

#ifdef CRIES_WOLF

	/* check for underflow (previous picture).
	*/
	if (!low_delay && (decoding_time < (double)bitcnt_EOP*90000.0/bit_rate))
	{
		/* picture not completely in buffer at intended decoding time */
		fprintf(stderr,"WARNING: vbv_delay underflow frame %d (target=%.1f, actual=%.1f\n)",
				frame_num-1, decoding_time, bitcnt_EOP*90000.0/bit_rate);
		printf( "gu=%d\n", gop_undershoot );
	}


	/* when to decode current frame */
	decoding_time += picture_delay;


	/* check for overflow (current picture).  Unless verbose warn
	   only if overflow must be at least in part due to an oversize
	   frame (rather than undersize predecessor).
	   	*/

	picture->vbv_delay = (int)(decoding_time - ((double)bitcnt_EOP)*90000.0/bit_rate);

	if ( decoding_time * ((double)bit_rate  / 90000.0) - ((double)bitcnt_EOP)
		> vbv_buffer_size )
	{
		double oversize = vbv_buffer_size -
			(decoding_time / 90000.0 * bit_rate - (double)(bitcnt_EOP+gop_undershoot));
		if(!quiet || oversize > 0.0  )
			fprintf(stderr,"vbv_delay overflow frame %d - %f.0 bytes!\n", 
					frame_num,
					oversize / 8.0
				);
	}


#ifdef OUTPUT_STAT
	fprintf(statfile,
			"\nvbv_delay=%d (bitcount=%lld, decoding_time=%.2f, bitcnt_EOP=%lld)\n",
			picture->vbv_delay,bitcount(),decoding_time,bitcnt_EOP);
#endif

	if (picture->vbv_delay<0)
	{
		if (!quiet)
			fprintf(stderr,"vbv_delay underflow: %d\n",picture->vbv_delay);
		picture->vbv_delay = 0;
	}



	if (picture->vbv_delay>65535)
	{
		fprintf(stderr,"vbv_delay frame %d exceeds permissible range: %d\n",
				frame_num, picture->vbv_delay);
		picture->vbv_delay = 65535;
	}
#else
	if( !mpeg1 )
		picture->vbv_delay =  0xffff;
	else
		picture->vbv_delay =  90000.0/frame_rate/4;
#endif

}
