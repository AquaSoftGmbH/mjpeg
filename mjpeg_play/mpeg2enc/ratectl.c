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

#include <stdio.h>
#include <math.h>
#include <limits.h>

#include "config.h"
#include "global.h"


/* private prototypes */

static double calc_actj _ANSI_ARGS_((void));

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
int	d0i = 0, d0p = 0, d0b = 0;

/* Virtual reciever fullness moving averages.  Used to allow sensible
 rate-control recovery after a period of bit-rate undershoot
*/
static int sliding_d_avg = 0;
static int sliding_di_avg = 0;
static int sliding_db_avg = 0;
static int sliding_dp_avg = 0;

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
static int CarryR;
static int CarryRLim;

/* bitcnt_EOP -	Position in generated bit-stream for latest end-of-picture
				Comparing these values with the bit-stream position for when the picture
				is due to be displayed allows us to see what the vbv buffer
				is up to.
   gop_undershoot -
   				If we *undershoot* our bit target the vbv buffer calculations
				based on the actual length of the bitstream will be wrong because
				in the final system	stream these bits will be padded away. 
				I.e. frames *won't* arrive as early as the length of the video
				stream would suggest they would.  To get it
				right we have to keep track of the bits that would appear in padding.
				
				"padded_bits" is the cumulative total of bits (in a CBR stream)
				that will *definately* have been padded away.
				gop_undershoot is the undershoot in the previously generated
				frame.  It is handled seperately because the in-GOP calculations
				involve rounding.	
										
*/	

static bitcount_t bitcnt_EOP = 0;
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
double avg_act;
double peak_act;

static int Np, Nb;
static bitcount_t S;
static int prev_mquant;

/* Note: eventually we may wish to tweak these to suit image content */
static double Ki;    	/* Down-scaling of I/B/P-frame complexity */
static double Kb;	    /* relative to others in bit-allocation   */
static double Kp;	    /* calculations.  We only need 2 but have all
							   3 for readability */


static int min_d,max_d;
static int min_q, max_q;

/* TODO DEBUG */
static double avg_KI = 5.0;	/* These values empirically determined 		*/
static double avg_KB = 8.0;	/* for MPEG-1, may need tuning for MPEG-2	*/
static double avg_KP = 8.0;
static double avgsq_KI = 8.0*8.0;
static double avgsq_KB = 13.0*13.0;
static double avgsq_KP = 12.0*12.0;
#define K_AVG_WINDOW_I 4.0		/* MPEG-1, hard-wired settings */
#define K_AVG_WINDOW_P   10.0
#define K_AVG_WINDOW_B   20.0
static double bits_per_mb;

static double SQ = 0.0;
static double AQ = 0.0;


/* Scaling and clipping of quantisation factors
   One floating point for predictive calculations
   one producing actual quantisation factors...
*/

static double scale_quant( pict_data_s *picture, double quant )
{
	double squant;
	int iquant;
	if (picture->q_scale_type)
	{
		/* BUG TODO: This should interpolate the table... */
		iquant = (int) floor(quant + 0.5);

		/* clip mquant to legal (linear) range */
		if (iquant<1)
			iquant = 1;
		if (iquant>112)
			iquant = 112;

		squant = (double)
			non_linear_mquant_table[map_non_linear_mquant[iquant]];
	}
	else
	{
		/* clip mquant to legal (linear) range */
		squant = quant;
		if (squant<2.0)
			squant = 2;
		if (squant>62.0)
			squant = 62.0;
	}
	return squant;
}

static int iscale_quant( pict_data_s *picture, double quant )
{
	int iquant;
	if (picture->q_scale_type)
	{
		/* BUG TODO: This should interpolate the table... */
		iquant = (int) floor(quant + 0.5);

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

void rc_init_seq()
{
	bits_per_mb = (double)bit_rate / (mb_width*mb_height2);

	/* reaction parameter (constant) decreased to increase response
	   rate as encoder is currently tending to under/over-shoot... in
	   rate TODO: Reaction parameter is *same* for every frame type
	   despite different weightings...  */
	if (r==0)  
		r = (int)floor(2.0*bit_rate/frame_rate + 0.5);

	if( pred_ratectl )
	{
		Ki = 1.0;
		Kb = 1.4;
		Kp = 1.0;
	}
	else
	{
		Ki = 1.0;
		Kb = 1.4;
		Kp = 1.0;
	}

	/* remaining # of bits in GOP */
	R = 0;
  
	/* Heuristic: In constant bit-rate streams we assume buffering
	   will allow us to pre-load some (probably small) fraction of the
	   buffers size worth of following data if earlier data was
	   undershot its bit-rate allocation
	 
	*/


	CarryR = 0;
	CarryRLim = video_buffer_size / 3;
	/* global complexity (Chi! not X!) measure of different frame types */
	/* These are just some sort-of sensible initial values for start-up */
	if ( pred_ratectl )
	{
		Xi = 1500*mb_per_pict;   /* Empirically derived values */
		Xp = 550*mb_per_pict;
		Xb = 170*mb_per_pict;
		d0i = -1;				/* Force initial Quant prediction */
		d0p = -1;
		d0b = -1;
	}
	else
	{
		/* A.Stevens Nov 2000 Don't know where these values came from ! */
		d0i = (int)(Ki*r*0.15 );
		d0p = (int)(Kp*r*0.15 );
		d0b = (int)(Kb*r*0.15 );

		Xi = 160.0*bit_rate/115.0;
		Xp = 60.0*bit_rate/115.0;
		Xb = 42.0*bit_rate/115.0;
		d = d0i;
	}


#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: sequence initialization\n");
	fprintf(statfile,
			" initial global complexity measures (I,P,B): Xi=%.0f, Xp=%.0f, Xb=%.0f\n",
			Xi, Xp, Xb);
	fprintf(statfile," reaction parameter: r=%d\n", r);
	fprintf(statfile,
			" initial virtual buffer fullness (I,P,B): d0i=%d, d0p=%d, d0b=%d\n",
			d0i, d0p, d0b);
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
	   "saved" from previous low-activity frames.  For CBR this is of
	   course nonsense. Typically the specified buffer sizes are
	   actually quite small compared with the size of a GOP. Thus only
	   the undershoot occuring right at the end of a GOP can safely
	   be carried over.
		
	   As a crude approximation currently simply forbid carry over
	   between GOPs.
	   TODO: We should do a proper video buffer model and use to make
	   bit allocation decisions.

	*/
	
	if( R > 0 && video_buffer_size < per_gop_bits  )
	{
		gop_undershoot = R;/* We reset it to *exact* value at end of GOP
							prevent estimation errors accumulating */

		R = per_gop_bits;		
	}
	else 
	{
		R +=  per_gop_bits;
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

	actsum =  calc_actj( );
	avg_act = (double)actsum/(double)(mb_width*mb_height2);
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
		if( pred_ratectl )
		{
			d = d0i;
			avg_K = avg_KI;
			Si = (Xi + 3.0*avg_K*actsum)/4.0;
						printf( "Si = %.2f\n", Si );
			T = R/(1.0+Np*Xp*Ki/(Si*Kp)+Nb*Xb*Ki/(Si*Kb));
		}
		else
			T = R/(1.0+Np*Xp*Ki/(Xi*Kp)+Nb*Xb*Ki/(Xi*Kb));

		sliding_d_avg = sliding_di_avg;
		break;
	case P_TYPE:
		if( pred_ratectl )
		{
			d = d0p;
			avg_K = avg_KP;
			Sp = (Xp + avg_K*actsum) / 2.0;
			T =  R/(Np+Nb*Kp*Xb/(Kb*Sp)) + 0.5;
		}
		else
			T =  R/(Np+Nb*Kp*Xb/(Kb*Xp)) + 0.5;
		sliding_d_avg = sliding_dp_avg;
		break;
	case B_TYPE:
		if( pred_ratectl )
		{
			d = d0p;            // I and P frame share ratectl virtual buffer
			avg_K = avg_KB;
			Sb = Xb /* + avg_K * actsum) / 2.0 */;
			T =  R/(Nb+Np*Kb*Xp/(Kp*Sb));
		}
		else
			T =  R/(Nb+Np*Kb*Xp/(Kp*Xb));
		sliding_d_avg = sliding_db_avg;
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
						   avg_K * avg_act *(mb_width*mb_height2) / T);
	current_Q = scale_quant(picture,62.0*d / r);
#ifdef DEBUG
	if( !quiet )
	{
		printf( "AA=%3.1f T=%6.0f K=%.1f ",avg_act, (double)T, avg_K  );
	}
#endif	
	if( pred_ratectl )
	{
		/* We can only carry over as much total undershoot as
		buffering permits */
		

		if ( current_Q < 2.5 && target_Q > 12.0 )
		{
			/* We're undershooting and a serious surge in the data_flow
			   due to lagging adjustment is possible...
			*/
			d = (int) (target_Q * r / 62.0);
		}
	}
	else
	{
		if( d < 0 )
		{
			d = sliding_d_avg / 2;
		}
		else
			sliding_d_avg = (127 * sliding_d_avg + d) / 128;
	}


	/* TODO:
	   This is a good idea but currently may lead to over-shoots...
	   Never let any frame type bit target drop below 1/8th share to
	   prevent starvation under extreme circumstances

	   Tmin = (int) floor(bit_rate/(8.0*frame_rate) + 0.5);
	   if (T<Tmin)
	   T = Tmin;
	*/
  
	S = bitcount();


#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: start of picture\n");
	fprintf(statfile," target number of bits: T=%.0f\n",T/8);
#endif
}



static double calc_actj(void)
{
	int i,j,k,l;
	double actj,sum;
	unsigned short *i_q_mat;
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
			if( cur_picture.mbinfo[k].mb_type  & MB_INTRA )
			{
				i_q_mat = i_intra_q;
			}
			else
			{
				i_q_mat = i_inter_q;
			}

			/* It takes some bits to code even an entirely zero block...
			   It also makes a lot of calculations a lot better conditioned
			   if it can be guaranteed that activity is always distinctly
			   non-zero.
			 */

			actsum = 0;
			for( l = 0; l < 6; ++l )
				actsum += 
					(*pquant_weight_coeff_sum)
					    ( &cur_picture.mbinfo[k].dctblocks[l], i_q_mat ) ;
			actj = (double)actsum / (double)COEFFSUM_SCALE;
			if( actj < 12.0 )
				actj = 12.0;

			cur_picture.mbinfo[k].act = (double)actj;
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
	bitcount_t AP,PP;		/* Actual and padded picture bit counts */
	int    i;
	int    Qsum;
	int frame_overshoot;
	
	
	AP = bitcount() - S;
	frame_overshoot = (int)AP-(int)T;

	/* Can't have negative undershoot - would imply we could "borrow"
	bits */
	
	/* If the cummulative undershoot is getting too large (as
	   a rough and ready heuristic we use 1/2 buffer size)
		we start padding the stream.
	 */

	if( gop_undershoot-frame_overshoot > video_buffer_size/2 )
	{
		int padding_2bytes = 
			((gop_undershoot-frame_overshoot)-video_buffer_size/2)/16;
	    printf( "PAD " );
		alignbits();
		for( i = 0; i < padding_2bytes; ++i )
		{
			putbits(0, 16);
		}
		
		PP = bitcount() - S;			/* total # of bits in picture */
		frame_overshoot = (int)PP-(int)T;
	}
	else
		PP = AP;
	
	/* Estimate cummulative undershoot within this gop. 
	   This is only an estimate because T includes an allocation
	   from earlier undershoots causing multiple counting.
	   Padding and an exact calculation each gop prevent the error
	   in the estimate growing
	   */
	gop_undershoot -= frame_overshoot;
	gop_undershoot = gop_undershoot > 0 ? gop_undershoot : 0;
	R-= PP;						/* remaining # of bits in GOP */

	Qsum = 0;
	for( i = 0; i < mb_per_pict; ++i )
	{
		Qsum += cur_picture.mbinfo[i].mquant;
	}


	AQ = (double)Qsum/(double)mb_per_pict;
	/* TODO: The X are used as relative activity measures... so why
	   bother dividing by 2?  
	   Note we have to be careful to measure the actual data not the
	   padding too!
	*/
	SQ += AQ;
	X = (double)AP*(AQ/2.0);
	
	d += frame_overshoot;
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
	

	switch (picture->pict_type)
	{
	case I_TYPE:
		avg_KI = (K + avg_KI * K_AVG_WINDOW_I) / (K_AVG_WINDOW_I+1.0) ;
		avgsq_KI = (K*K + avgsq_KI * K_AVG_WINDOW_I) / (K_AVG_WINDOW_I+1.0) ;
		if( pred_ratectl )
		{
			d0i = d;
		}
		Xi = X;
		sliding_di_avg = sliding_d_avg;
		break;
	case P_TYPE:
		avg_KP = (K + avg_KP * K_AVG_WINDOW_P) / (K_AVG_WINDOW_P+1.0) ;
		avgsq_KP = (K*K + avgsq_KP * K_AVG_WINDOW_P) / (K_AVG_WINDOW_P+1.0) ;;
		if( pred_ratectl )
		{
			d0p = d;
		}
		Xp = X;

		sliding_dp_avg = sliding_d_avg;
		Np--;
		break;
	case B_TYPE:
		avg_KB = (K + avg_KB * K_AVG_WINDOW_B) / (K_AVG_WINDOW_B+1.0) ;
		avgsq_KB = (K*K + avgsq_KB * K_AVG_WINDOW_B) / (K_AVG_WINDOW_B+1.0);
		if( pred_ratectl )
		{
			// DEBUG Share d and p d0b = d;
			d0p = d;
		}
		Xb = X;

		sliding_db_avg = sliding_d_avg;
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
			" virtual buffer fullness (I,P,B): d0i=%d, d0p=%d, d0b=%d\n",
			d0i, d0p, d0b);
	fprintf(statfile," remaining number of P pictures in GOP: Np=%d\n",Np);
	fprintf(statfile," remaining number of B pictures in GOP: Nb=%d\n",Nb);
	fprintf(statfile," average activity: avg_act=%.1f \n", avg_act );
#endif

}

/* compute initial quantization stepsize (at the beginning of picture) */
int rc_start_mb(pict_data_s *picture)
{
	int mquant;

	if (picture->q_scale_type)
	{
		mquant = (int) floor(2.0*d*31.0/r + 0.5);

		/* clip mquant to legal (linear) range */
		if (mquant<1)
			mquant = 1;
		if (mquant>112)
			mquant = 112;

		/* map to legal quantization level */
		mquant = non_linear_mquant_table[map_non_linear_mquant[mquant]];
	}
	else
	{
		mquant = (int) floor(d*31.0/r + 0.5);
		mquant <<= 1;

		/* clip mquant to legal (linear) range */
		if (mquant<2)
			mquant = 2;
		if (mquant>62)
			mquant = 62;


		if(fix_mquant>0) mquant = 2*fix_mquant;
	}
     
	prev_mquant = mquant;

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
	
	dj = ((double)d) + ( (double)(bitcount()-S) - actcovered * ((double)T) / actsum);

    
    /* scale against dynamic range of mquant and the bits/picture count */
	Qj = dj*62.0/r;
/*Qj = dj*(q_scale_type ? 56.0 : 31.0)/r;  */


	/* compute normalized activity */
	actj = cur_picture.mbinfo[j].act;
	actcovered += actj;

	/*  Heuristic: Decrease quantisation for blocks with lots of
		sizeable coefficients.  We assume we just get a mess if
		a complex texture's coefficients get chopped...
	*/

	/* TODO: The boost given to high activity blocks should be
	   modulated according to the generosity of the data-rate with
	   respect to average activity.  High activity to date rate means
	   its probably better to be too generous with bits 
	*/
		
	N_actj =  actj < avg_act ? 1.0 : (actj + act_boost*avg_act)/(act_boost*actj +  avg_act);

#ifdef ORIGINAL_CODE
	if (picture->q_scale_type)
	{
		/* modulate mquant with combined buffer and local activity measures */
		mquant = (int) floor(2.0*Qj*N_actj + 0.5);

		/* clip mquant to legal (linear) range */
		if (mquant<1)
			mquant = 1;
		if (mquant>112)
			mquant = 112;

	}
	else
	{
		/* modulate mquant with combined buffer and local activity measures */
		mquant = (int) floor(Qj*N_actj + 0.5);
		mquant = ((mquant)>>1)<<1;

		/* clip mquant to legal (linear) range */
		if (mquant<2)
			mquant = 2;
		if (mquant>62)
			mquant = 62;

	}
#endif
	mquant = iscale_quant(picture,Qj*N_actj);

	prev_mquant = mquant;
	if(fix_mquant>0) 
	{
		if( picture->q_scale_type )
			mquant = 
				non_linear_mquant_table[map_non_linear_mquant[2*fix_mquant]];
			else
				mquant = 2*fix_mquant;
	}

	 
#ifdef OUTPUT_STAT
/*
  fprintf(statfile,"MQ(%d): ",j);
  fprintf(statfile,"dj=%.0f, Qj=%1.1f, actj=3.1%f, N_actj=1.1%f, mquant=%03d\n",
  dj,Qj,actj,N_actj,mquant);
*/
	cur_picture.mbinfo[j].N_act = N_actj;
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
			cur_picture.vbv_delay,bitcount(),decoding_time,bitcnt_EOP);
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
	picture->vbv_delay =  90000.0/frame_rate/4;
#endif

}
