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

/* TODO:  Currently new algorithm is switched.  Once its right tune it properly
	and make it fixed....
	*/


#define NEW_RATE_CTL

	
/* private prototypes */
#ifdef NEW_RATE_CTL
static double calc_actj _ANSI_ARGS_((unsigned char *frame));
#else
static double var_sblk _ANSI_ARGS_((unsigned char *p, int lx));
#endif

/*Constant bit-rate rate control variables */
/* X's global complexity (Chi! not X!) measures.
   Actually: average quantisation * bits allocated in *previous* frame
   d's  virtual reciever buffer fullness
   r - Rate control feedback gain (in bits/frame)  I.e if the virtual
       buffer is x% full
*/

int Xi, Xp, Xb, r, d0i, d0p, d0b;

/* R - Remaining bits available in GOP
   T - Target bits for current frame 
   d - Current d for quantisation purposes updated using 
   scaled difference of target bit usage and actual usage */
static bitcount_t R;
static int d;
static bitcount_t T;

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
void rc_init_seq()
{

  /* reaction parameter (constant) */
  if (r==0)  r = (int)floor(2.0*bit_rate/frame_rate + 0.5);

  /* average activity */
  if (avg_act==0.0)  avg_act = 400.0;

  /* remaining # of bits in GOP */
  R = 0;


  /* global complexity (Chi! not X!) measure of different frame types */
  /* These are just some sort-of sensible initial values for start-up */
  if (Xi==0) Xi = (int)floor(160.0*bit_rate/115.0 + 0.5);
  if (Xp==0) Xp = (int)floor( 60.0*bit_rate/115.0 + 0.5);
  if (Xb==0) Xb = (int)floor( 42.0*bit_rate/115.0 + 0.5);


  /* TODO: These initialisatino values are plausible for O(1500mbps)
	 VCD like MPEG-1, but will be silly for higher data-rates they need
     to be adjusted based on bit_rate. */

  /* virtual buffer fullness - Originally initialised for initial quantisation
     around 10 (1/3 maximum).  This, however proved excessive so now
     its 0.15 */
  
  if (d0i==0) d0i = (int)floor(Ki*r*0.15 + 0.5);
  if (d0p==0) d0p = (int)floor(Kp*r*0.15 + 0.5);
  if (d0b==0) d0b = (int)floor(Kb*r*0.15 + 0.5);
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
  fprintf(statfile," initial average activity: avg_act=%.1f\n", avg_act);
#endif
}

void rc_init_GOP(np,nb)
int np,nb;
{

	int  per_gop_bits = (bitcount_t) floor((1 + np + nb) * bit_rate / frame_rate + 0.5);
	/* A.Stevens Aug 2000: at last I've found the wretched rate-control overshoot bug...
		Simply "topping up" R here means that we can accumulate an indefinately large
		pool of bits "saved" from previous low-activity frames.  For CBR this is of course
		nonsense. Even if the recieve buffer is large enough to avoid the "saved" bits
		simply being thrown away by padding we can't send arbitrarily huge frames  because
		they won't arrive in time for display.
		
		Thus unless VBR (fixed quantisation) has been set we "decay" saved bits  and
		never allow more than a frames worth of carry over.
	*/		
   
  if ( R > per_gop_bits / 16 )
  {
  	printf( "R carry-over overshoot = %lld\n", R);
  	R = per_gop_bits / 16;
  }
  R += per_gop_bits;
  Np = fieldpic ? 2*np+1 : np;
  Nb = fieldpic ? 2*nb : nb;

#ifdef OUTPUT_STAT
  fprintf(statfile,"\nrate control: new group of pictures (GOP)\n");
  fprintf(statfile," target number of bits for GOP: R=%d\n",R);
  fprintf(statfile," number of P pictures in GOP: Np=%d\n",Np);
  fprintf(statfile," number of B pictures in GOP: Nb=%d\n",Nb);
#endif
}



/* Step 1: compute target bits for current picture being coded */
void rc_init_pict(frame)
unsigned char *frame;
{
  double Tmin;

  /* Allocate target bits for frame based on frames numbers in GOP
	 weighted by global complexity estimates and B-frame scale factor
	 T = (Nx * Xx/Kx) / Sigma_j (Nj * Xj / Kj)
  */
  min_q = min_d = INT_MAX;
  max_q = max_d = INT_MIN;
  switch (pict_type)
  {
  case I_TYPE:

	/* This *looks* inconsistent (the cost of the I frame doesn't
	   appear in the calculations for P and B).  However, it works
	   fine because the I frame calculation is done 1st in each GOP,
	   so once we hit P and B frames there are no I frames left to
	   account for with the remaining pool of bits R.
	*/

    T = (bitcount_t) floor(R/(1.0+Np*Xp*Ki/(Xi*Kp)+Nb*Xb*Ki/(Xi*Kb)) + 0.5);
    d = d0i;
    break;
  case P_TYPE:
    T = (bitcount_t) floor(R/(Np+Nb*Kp*Xb/(Kb*Xp)) + 0.5);
    d = d0p;
    break;
  case B_TYPE:
    T = (bitcount_t) floor(R/(Nb+Np*Kb*Xp/(Kp*Xb)) + 0.5);
    d = d0b;
    break;
  }

  /* Never let any frame type bit target drop below 1/8th share to
	 prevent starvation under extreme circumstances
  */
  Tmin = (int) floor(bit_rate/(8.0*frame_rate) + 0.5);

  if (T<Tmin)
    T = Tmin;

  S = bitcount();
  Q = 0;

  /* TODO: A.Stevens Jul 2000 - This modification needs testing visually.

	 Weird.  The original code used the average activity of the
	 *previous* frame as the basis for quantisation calculations for
	 rather than the activity in the *current* frame.  That *has* to
	 be a bad idea..., surely, here we try to be smarter by using the
	 current values and keeping track of how much of the frames
	 activitity has been covered as we go along.
  */

  actsum =  calc_actj(frame );
  avg_act = actsum/(mb_width*mb_height2);
  actcovered = 0.0;

#ifdef OUTPUT_STAT
  fprintf(statfile,"\nrate control: start of picture\n");
  fprintf(statfile," target number of bits: T=%d\n",T);
#endif
}



static double calc_actj(frame)
unsigned char *frame;
{
  int i,j,k;
#ifndef NEW_RATE_CTL
  unsigned char *p;
  double var;
#endif
  double actj,sum;
  unsigned short *i_q_mat;

  sum = 0.0;
  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
#ifndef NEW_RATE_CTL
      p = frame + ((pict_struct==BOTTOM_FIELD)?width:0) + i + width2*j;

      /* take minimum spatial activity measure of luminance blocks */

      actj = var_sblk(p,width2);
      var = var_sblk(p+8,width2);
      if (var<actj) actj = var;
      var = var_sblk(p+8*width2,width2);
      if (var<actj) actj = var;
      var = var_sblk(p+8*width2+8,width2);
      if (var<actj) actj = var;

      if (!fieldpic && !prog_seq)
      {
        /* field */
        var = var_sblk(p,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+8,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+width,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+width+8,width<<1);
        if (var<actj) actj = var;
      }

      actj+= 1.0;

#else
	  /* A.Stevens Jul 2000 Luminance variance *has* to be a rotten measure
		 of how active a block in terms of bits needed to code a lossless DCT.
		 E.g. a half-white half-black block has a maximal variance but 
		 pretty small DCT coefficients.

		 So.... we use the absolute sum of DCT coefficients as our
		 variance measure.  
	  */
	  if( mbinfo[k].mb_type  & MB_INTRA )
	  {
			i_q_mat = i_intra_q;
		}
	  else
	  {
			i_q_mat = i_inter_q;
		}

	  actj  = (double) (*pquant_weight_coeff_sum)( &mbinfo[k].dctblocks[0], i_q_mat ) /
	  	       (double) COEFFSUM_SCALE ;
	  actj += (double) (*quant_weight_coeff_sum)( &mbinfo[k].dctblocks[1], i_q_mat ) / 
	  		(double) COEFFSUM_SCALE;
	  actj += (double) (*quant_weight_coeff_sum)( &mbinfo[k].dctblocks[1], i_q_mat ) / 
	  		(double) COEFFSUM_SCALE;

	  actj += (double) (*quant_weight_coeff_sum)( &mbinfo[k].dctblocks[1], i_q_mat ) /
	  		(double) COEFFSUM_SCALE;
#endif
      mbinfo[k].act = actj;
	  sum += actj;
	  ++k;
    }
  return sum;
}

void rc_update_pict()
{
  double X;
  bitcount_t OLD_S = S;
  S = bitcount() - S; /* total # of bits in picture */
  R-= S; /* remaining # of bits in GOP */
  X = (int) floor((double)S*((0.5*(double)Q)/(mb_width*mb_height2)) + 0.5);
  d += (int) (S - T);

  /* Bits that never got used in the past can't be resurrected now... 
   */
  /* TODO: The actual minimum D setting should be some number tuned
	 to ensure sensible response once activity picks up...
  */
  if( d < 0 )
	d = 0;

  switch (pict_type)
  {
  case I_TYPE:
    Xi = X;
    d0i = d;
    break;
  case P_TYPE:
    Xp = X;
    d0p = d;
    Np--;
    break;
  case B_TYPE:
    Xb = X;
    d0b = d;
    Nb--;
    break;
  }

#ifdef OUTPUT_STAT
  fprintf(statfile,"\nrate control: end of picture\n");
  fprintf(statfile," actual number of bits: S=%d\n",S);
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
  if( !quiet )
  {
   	printf( "AA=%.1f AQ=%.1f  L=%.0f R=%lld T=%lld\n", 
		avg_act, ((double)Q) / (double) (mb_width*mb_height2), 
		(double)(bitcount()-OLD_S),
		R, T   );
	}
 }

/* compute initial quantization stepsize (at the beginning of picture) */
int rc_start_mb()
{
  int mquant;

  if (q_scale_type)
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
int rc_calc_mquant(j)
int j;
{
  int mquant;
  double dj, Qj, actj, N_actj; 

#ifdef NEW_RATE_CTL
  /* A.Stevens Jul 2000 - This *has* to be dumb.  Essentially,
	 it tries to give every block a similar share of the available bits.
	 This will not properly exploit blocks which need few and save them
	 for later ones that need them.
	 
	 My proposal: we measure how much *information* (total activity)
	 has been covered and aim to release bits in proportion.  Indeed,
	 complex blocks get an disproprortionate boost of allocated bits.
	 To avoid visible "ringing" effects...

  */
	
  dj = ((double)d) + ( (double)(bitcount()-S) - actcovered * ((double)T) / actsum);

  if( (double)(bitcount()-S) < 100.0 )
  	printf( "BS double conv...%f\n", (double)(bitcount()-S) );
#else
  /* measure virtual buffer discrepancy from uniform distribution model */
	dj = d + (double)(bitcount()-S) - j*(T/(mb_width*mb_height2));

#endif

    
    /* scale against dynamic range of mquant and the bits/picture count */
  Qj = dj*31.0/r;
/*Qj = dj*(q_scale_type ? 56.0 : 31.0)/r;  */


  /* compute normalized activity */
  actj = mbinfo[j].act;
  actcovered += actj;


#ifdef NEW_RATE_CTL
  /*  Heuristic: Decrease quantisation for blocks with lots of
	  sizeable coefficients.  We assume we just get a mess if
	  a complex texture's coefficients get chopped...
  */

	/* TODO: The boosdt given to high activity blocks should be modulated according
		to the generosity of the data-rate with respect to average activity.
		High activity to date rate means its probably better to be too generous
		with bits
		*/
		
  N_actj =  actj < avg_act ? 1.0 : (actj + act_boost*avg_act)/(act_boost*actj +  avg_act);

#else

  N_actj =  (2.0*actj+avg_act)/(actj+2.0*avg_act);

  

#endif

  if (q_scale_type)
  {
    /* modulate mquant with combined buffer and local activity measures */
    mquant = (int) floor(2.0*Qj*N_actj + 0.5);

    /* clip mquant to legal (linear) range */
    if (mquant<1)
      mquant = 1;
    if (mquant>112)
      mquant = 112;

    /* map to legal quantization level */
    if(fix_mquant>0) 
    	mquant = 2*fix_mquant;
    mquant = non_linear_mquant_table[map_non_linear_mquant[mquant]];
  }
  else
  {
    /* modulate mquant with combined buffer and local activity measures */
    mquant = (int) floor(Qj*N_actj + 0.5);
    mquant <<= 1;

    /* clip mquant to legal (linear) range */
    if (mquant<2)
      mquant = 2;
    if (mquant>62)
      mquant = 62;

    /* ignore small changes in mquant */
    if (mquant>=8 && (mquant-prev_mquant)>=-4 && (mquant-prev_mquant)<=4)
      mquant = prev_mquant;

    prev_mquant = mquant;
    if(fix_mquant>0) mquant = 2*fix_mquant;
  }
	
  Q+= mquant; /* for calculation of average mquant */


	 
#ifdef OUTPUT_STAT
/*
  fprintf(statfile,"MQ(%d): ",j);
  fprintf(statfile,"dj=%.0f, Qj=%1.1f, actj=3.1%f, N_actj=1.1%f, mquant=%03d\n",
   dj,Qj,actj,N_actj,mquant);
*/
#endif

  return mquant;
}

#ifndef NEW_RATE_CTL

/* compute variance of 8x8 block */
static double var_sblk(p,lx)
unsigned char *p;
int lx;
{
  int i, j;
  unsigned int v, s, s2;

  s = s2 = 0;

  for (j=0; j<8; j++)
  {
    for (i=0; i<8; i++)
    {
      v = *p++;
      s+= v;
      s2+= v*v;
    }
    p+= lx - 8;
  }

  return s2/64.0 - (s/64.0)*(s/64.0);
}
#endif

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

void vbv_end_of_picture()
{
  bitcnt_EOP = bitcount() - BITCOUNT_OFFSET;
  bitcnt_EOP = (bitcnt_EOP + 7) & ~7; /* account for bit stuffing */
}

/* calc_vbv_delay
 *
 * has to be called directly after writing the picture start code, the
 * reference point for vbv_delay
 */

void calc_vbv_delay()
{
  double picture_delay;
  static double next_ip_delay; /* due to frame reordering delay */
  static double decoding_time;

  /* number of 1/90000 s ticks until next picture is to be decoded */
  if (pict_type == B_TYPE)
  {
    if (prog_seq)
    {
      if (!repeatfirst)
        picture_delay = 90000.0/frame_rate; /* 1 frame */
      else
      {
        if (!topfirst)
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
        if (!repeatfirst)
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
      if(topfirst==(pict_struct==TOP_FIELD))
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

    if (!fieldpic || topfirst!=(pict_struct==TOP_FIELD))
    {
      /* frame picture or second field */
      if (prog_seq)
      {
        if (!repeatfirst)
          next_ip_delay = 90000.0/frame_rate;
        else
        {
          if (!topfirst)
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
          if (!repeatfirst)
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

  vbv_delay = (int)(decoding_time - ((double)bitcnt_EOP)*90000.0/bit_rate);


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
    "\nvbv_delay=%d (bitcount=%d, decoding_time=%.2f, bitcnt_EOP=%lld)\n",
    vbv_delay,bitcount(),decoding_time,bitcnt_EOP);
#endif

  if (vbv_delay<0)
  {
    if (!quiet)
      fprintf(stderr,"vbv_delay underflow: %d\n",vbv_delay);
    vbv_delay = 0;
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
