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
   d's  virtual reciever buffer fullness
   r - Rate control feedback gain (in bits/frame)  I.e if the "virtual
   buffer" is x% full and
*/

int Xi = 0, Xp = 0, Xb = 0;
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
   scaled difference of target bit usage and actual usage */
static bitcount_t R;
static int d;
static bitcount_t T;
static bitcount_t CarryR;
static bitcount_t CarryRLim;

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

static int Np, Nb, Q;
static bitcount_t S;
static int prev_mquant;


/* Note: eventually we may wish to tweak these to suit image content */
static double Ki = 1.0;    	/* Down-scaling of I/B/P-frame complexity */
static double Kb = 1.4;	    /* relative to others in bit-allocation   */
static double Kp = 1.0;	    /* calculations.  We only need 2 but have all
							   3 for readability */


static int min_d,max_d;
static int min_q, max_q;

/* TODO DEBUG */
static double avg_KI = 13.0;	/* These values empirically determined 		*/
static double avg_KB = 20.0;	/* for MPEG-1, may need tuning for MPEG-2	*/
static double avg_KP = 20.0;
static double avgsq_KI = 13.0*13.0;
static double avgsq_KB = 20.0*20.0;
static double avgsq_KP = 20.0*20.0;
#define K_AVG_WINDOW 128.0
static double bits_per_mb;

static double scale_quant( pict_data_s *picture, double quant )
{
	double squant;
	int iquant;
	if (picture->q_scale_type)
	{
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

void rc_init_seq()
{
	bits_per_mb = (double)bit_rate / (mb_width*mb_height2);

	/* reaction parameter (constant) decreased to increase response
	   rate as encoder is currently tending to under/over-shoot... in
	   rate TODO: Reaction parameter is *same* for every frame type
	   despite different weightings...  */
	if (r==0)  
		r = (int)floor(2.0*bit_rate/frame_rate + 0.5);


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
		Xi = 120;				/* Empirically derived values */
		Xp = 45;
		Xb = 20;
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

		Xi = (int)floor(160.0*bit_rate/115.0 + 0.5);
		Xp = (int)floor( 60.0*bit_rate/115.0 + 0.5);
		Xb = (int)floor( 42.0*bit_rate/115.0 + 0.5);
	}

		
/*
  if (d0i==0) d0i = (int)floor(10.0*r/(qscale_tab[0] ? 56.0 : 31.0) + 0.5);
  if (d0p==0) d0p = (int)floor(10.0*r/(qscale_tab[1] ? 56.0 : 31.0) + 0.5);
  if (d0b==0) d0b = (int)floor(K*10.0*r/(qscale_tab[2] ? 56.0 : 31.0) + 0.5);
*/

#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: sequence initialization\n");
	fprintf(statfile,
			" initial global complexity measures (I,P,B): Xi=%d, Xp=%d, Xb=%d\n",
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
	int per_gop_bits = (bitcount_t) floor((1 + np + nb) * bit_rate / frame_rate + 0.5);

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
	printf( "GOP under/over run = %lld\n", -R );
	
	if( R > 0 && video_buffer_size < per_gop_bits  )
	{
		R = per_gop_bits;
	}
	else 
	{
		R += per_gop_bits;
	}
	Np = fieldpic ? 2*np+1 : np;
	Nb = fieldpic ? 2*nb : nb;

#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: new group of pictures (GOP)\n");
	fprintf(statfile," target number of bits for GOP: R=%lld\n",R);
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

	/* TODO: A.Stevens  Nov 2000 - This modification needs testing visually.

	   Weird.  The original code used the average activity of the
	   *previous* frame as the basis for quantisation calculations for
	   rather than the activity in the *current* frame.  That *has* to
	   be a bad idea..., surely, here we try to be smarter by using the
	   current values and keeping track of how much of the frames
	   activitity has been covered as we go along.
	*/

	actsum =  calc_actj( );
	avg_act = actsum/(mb_width*mb_height2);
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

		/* This *looks* inconsistent (the cost of the I frame doesn't
		   appear in the calculations for P and B).  However, it works
		   fine because the I frame calculation is done 1st in each GOP,
		   so once we hit P and B frames there are no I frames left to
		   account for with the remaining pool of bits R.
		*/

		if( pred_ratectl )
		{
			Xi = avg_act;
			avg_K = avg_KI;
		}
		T = (bitcount_t) floor(R/(1.0+Np*Xp*Ki/(Xi*Kp)+Nb*Xb*Ki/(Xi*Kb)) + 0.5);
		d = d0i;
		sliding_d_avg = sliding_di_avg;
		break;
	case P_TYPE:
		if( pred_ratectl )
		{
			Xp = avg_act;
			avg_K = avg_KP;
		}
		T = (bitcount_t) floor(R/(Np+Nb*Kp*Xb/(Kb*Xp)) + 0.5);
		d = d0p;
		sliding_d_avg = sliding_dp_avg;
		break;
	case B_TYPE:
		if( pred_ratectl )
		{
			Xb = avg_act;
			avg_K = avg_KB;
		}
		T = (bitcount_t) floor(R/(Nb+Np*Kb*Xp/(Kp*Xb)) + 0.5);
		d = d0b;
		sliding_d_avg = sliding_db_avg;
		break;
	}

	/* Undershot bits have been "returned" via R */
	if( d < 0 )
		d = 0;

	if( T < 4000 )
	{
		printf( "\nWARNING VERY LOW T=%lld\n", T);
		T = 4000;
	}
	target_Q = scale_quant(picture, 
						   avg_K * avg_act *(mb_width*mb_height2) / T);
	current_Q = scale_quant(picture,62.0*d / r);
	if( !quiet )
	{
		printf( "d=%6d T=%lld K=%.1f BQ=%2.1f TQ=%2.1f ",d, T,avg_K, current_Q, target_Q  );
	}
	
	if( pred_ratectl )
	{
		/* We can only carry over as much total undershoot as
		buffering permits */
		
		/* Deliberately allow quantisation to fall to minimum...
		*/
		if( target_Q > 6.0 )
		{	
		/* If the discrepancy is because we're undershooting we only
			   adjust if it looks like a serious overshoot may happen */
			target_Q = (target_Q + current_Q)/2.0;
			d = (int) (target_Q * r / 62.0);
		} else if ( current_Q < 2.5 && target_Q > 12.0 )
		{
			/* We're undershooting and a serious surge in the data_flow
			   due to lagging adjustment is possible...
			*/
			d = (int) (target_Q * r / 62.0);
		}
		printf( "td=%6d ",  d );


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
	Q = 0;


#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: start of picture\n");
	fprintf(statfile," target number of bits: T=%lld\n",T);
#endif
}



static double calc_actj(void)
{
	int i,j,k;
	double actj,sum;
	unsigned short *i_q_mat;

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

			actj  = (double) (*pquant_weight_coeff_sum)( &cur_picture.mbinfo[k].dctblocks[0], i_q_mat ) /
				(double) COEFFSUM_SCALE ;
			actj += (double) (*quant_weight_coeff_sum)( &cur_picture.mbinfo[k].dctblocks[1], i_q_mat ) / 
				(double) COEFFSUM_SCALE;
			actj += (double) (*quant_weight_coeff_sum)( &cur_picture.mbinfo[k].dctblocks[1], i_q_mat ) / 
				(double) COEFFSUM_SCALE;

			actj += (double) (*quant_weight_coeff_sum)( &cur_picture.mbinfo[k].dctblocks[1], i_q_mat ) /
				(double) COEFFSUM_SCALE;

			cur_picture.mbinfo[k].act = actj;
			sum += actj;
			++k;
		}
	return sum;
}

void rc_update_pict(pict_data_s *picture)
{
	double X;
	double AQ;
	double K;
	S = bitcount() - S;			/* total # of bits in picture */
	R-= S;						/* remaining # of bits in GOP */
	AQ = (double)Q/(mb_width*mb_height2);  
	/* TODO: The X are used as relative activity measures... so why
	   bother dividing by 2?  */
	X = (int) floor((double)S*(AQ/2.0) + 0.5);
  
	d += (int) (S - T);
	K = (double)(S) / (double) (mb_width*mb_height2) * AQ / avg_act;

	if( !quiet )
	{
		printf( "AQ=%.1f ",  AQ);
	}

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
		d0i = d;
		avg_KI = (K + avg_KI * K_AVG_WINDOW) / (K_AVG_WINDOW+1.0) ;
		avgsq_KI = (K*K + avgsq_KI * K_AVG_WINDOW) / (K_AVG_WINDOW+1.0) ;
		/* DEBUG */
		if( ! pred_ratectl )
			Xi = X;
		sliding_di_avg = sliding_d_avg;
		break;
	case P_TYPE:
		d0p = d;
		avg_KP = (K + avg_KP * K_AVG_WINDOW) / (K_AVG_WINDOW+1.0) ;
		avgsq_KP = (K*K + avgsq_KP * K_AVG_WINDOW) / (K_AVG_WINDOW+1.0) ;;
		if( ! pred_ratectl )
			Xp = X;

		sliding_dp_avg = sliding_d_avg;
		Np--;
		break;
	case B_TYPE:
		d0b = d;
		avg_KB = (K + avg_KB * K_AVG_WINDOW) / (K_AVG_WINDOW+1.0) ;
		avgsq_KB = (K*K + avgsq_KB * K_AVG_WINDOW) / (K_AVG_WINDOW+1.0) ;;
		if( ! pred_ratectl )
			Xb = X;
		sliding_db_avg = sliding_d_avg;
		Nb--;
		break;
	}

	if( !quiet )
		printf( "\n" );

#ifdef OUTPUT_STAT
	fprintf(statfile,"\nrate control: end of picture\n");
	fprintf(statfile," actual number of bits: S=%lld\n",S);
	fprintf(statfile," average quantization parameter Q=%.1f\n",
			(double)Q/(mb_width*mb_height2));
	fprintf(statfile," remaining number of bits in GOP: R=%lld\n",R);
	fprintf(statfile,
			" global complexity measures (I,P,B): Xi=%d, Xp=%d, Xb=%d\n",
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
	mquant = (int)floor(scale_quant(picture,Qj*N_actj)+0.5);

	/* ignore small changes in mquant */
	if (mquant>=8 && (mquant-prev_mquant)>=-4 && (mquant-prev_mquant)<=4)
		mquant = prev_mquant;
	
	prev_mquant = mquant;
	if(fix_mquant>0) 
	{
		if( picture->q_scale_type )
			mquant = 
				non_linear_mquant_table[map_non_linear_mquant[2*fix_mquant]];
			else
				mquant = 2*fix_mquant;
	}

	Q += mquant; /* for calculation of average mquant */


	 
#ifdef OUTPUT_STAT
/*
  fprintf(statfile,"MQ(%d): ",j);
  fprintf(statfile,"dj=%.0f, Qj=%1.1f, actj=3.1%f, N_actj=1.1%f, mquant=%03d\n",
  dj,Qj,actj,N_actj,mquant);
*/
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
 */

static bitcount_t bitcnt_EOP;

void vbv_end_of_picture(pict_data_s *picture)
{
	bitcnt_EOP = bitcount() - BITCOUNT_OFFSET;
	bitcnt_EOP = (bitcnt_EOP + 7) & ~7; /* account for bit stuffing */
}

/* calc_vbv_delay
 *
 * has to be called directly after writing the picture start code, the
 * reference point for vbv_delay
 */

void calc_vbv_delay(pict_data_s *picture)
{
	double picture_delay;
	static double next_ip_delay; /* due to frame reordering delay */
	static double decoding_time;

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
		picture_delay = ((vbv_buffer_size*16384*7)/8)*90000.0/bit_rate;
		if (fieldpic)
			next_ip_delay = (int)(90000.0/frame_rate+0.5);
	}

	/* VBV checks */


	/* check for underflow (previous picture) */
#ifdef THIS_CRIES_WOLF_ALL_THE_TIME
	if (!low_delay && (decoding_time < bitcnt_EOP*90000.0/bit_rate))
	{
		/* picture not completely in buffer at intended decoding time */
		if (!quiet)
			fprintf(stderr,"vbv_delay underflow! (decoding_time=%.1f, t_EOP=%.1f\n)",
					decoding_time, bitcnt_EOP*90000.0/bit_rate);
	}
#endif

	/* when to decode current frame */
	decoding_time += picture_delay;

	picture->vbv_delay = (int)(decoding_time - ((double)bitcnt_EOP)*90000.0/bit_rate);


	/* check for overflow (current picture) 
	   A.Stevens Aug 2000: This is very broken as if we heavily undershoot
	   our data-rate target (e.g. very inactive scenes) we will then permanently
	   generate vbv_delay overflows.  The problem is that we're (in effect) treating
	   padding as if it would take up space in the buffer...  Hence we comment
	   it out until we replace it with a more sensible calculation.
	*/
#ifdef TODO_VBV_NEEDS_FIXING
	if ((decoding_time - ((double)bitcnt_EOP)*90000.0/bit_rate)
		> (vbv_buffer_size*16384)*90000.0/bit_rate)
	{
		if (!quiet)
			fprintf(stderr,"vbv_delay overflow!\n");
	}
#endif

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

#ifdef TODO_VBV_NEEDS_FIXING
	if (vbv_delay>65535)
	{
		if (!quiet)
			fprintf(stderr,"vbv_delay overflow: %d\n",vbv_delay);
		vbv_delay = 65535;
	}
#endif
}
