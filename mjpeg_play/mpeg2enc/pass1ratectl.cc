/* ratectl.c, bitrate control routines  */

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

/* Modifications and enhancements (C) 2000,2001,2002,2003 Andrew Stevens */

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

#include "config.h"
#include <cassert>
#include <math.h>
#include <limits.h>
#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "mpeg2syntaxcodes.h"
#include "tables.h"
#include "simd.h"
#include "fastintfns.h"
#include "mpeg2encoder.hh"
#include "picture.hh"
#include "ratectl.hh"
#include "quantize.hh"

/* private prototypes */




/*****************************
 *
 * On-the-fly rate controller.  The constructor sets up the initial
 * control and estimator parameter values to values that experience
 * suggest make sense.  All the important ones are dynamically 
 * tuned anyway so these values are not too critical.
 *
 ****************************/
Pass1RateCtl::Pass1RateCtl(EncoderParams &encparams ) :
	RateCtl(encparams)
{
	buffer_variation = 0;
	bits_transported = 0;
	bits_used = 0;
	prev_bitcount = 0;
	bitcnt_EOP = 0;
	frame_overshoot_margin = 0;
	sum_avg_act = 0.0;
	sum_avg_var = 0.0;
	
	/* TODO: These values should are really MPEG-1/2 and material type
	   dependent.  The encoder should probably run over the first 100
	   frames or so look-ahead to tune theses dynamically before doing
	   real encoding... alternative a config file should  be written!
	*/

	sum_avg_quant = 0.0;

}

/*********************
 *
 * Initialise rate control parameters
 * params:  reinit - Rate control is being re-initialised during the middle
 *                   of a run.  Don't reset adaptive parameters.
 *
 ********************/

void Pass1RateCtl::InitSeq(bool reinit)
{
	double init_quant;
	/* If its stills with a size we have to hit then make the
	   guesstimates of for initial quantisation pessimistic...
	*/
	bits_transported = bits_used = 0;
	field_rate = 2*encparams.decode_frame_rate;
	fields_per_pict = encparams.fieldpic ? 1 : 2;
    
    assert( encparams.still_size == 0 );
    per_pict_bits = 
        static_cast<int32_t>(encparams.fieldpic
                             ? encparams.bit_rate / field_rate
                             : encparams.bit_rate / encparams.decode_frame_rate
            );

	/* Everything else already set or adaptive */
	if( reinit )
		return;

	first_gop = true;
	K_AVG_WINDOW[I_TYPE] = 2.0;
    switch( encparams.M )
    {
    case 1 : // P
        K_AVG_WINDOW[P_TYPE] = 8.0;
        K_AVG_WINDOW[B_TYPE] = 1.0; // dummy
        break;
    case 2 : // BP
        K_AVG_WINDOW[P_TYPE] = 4.0;
        K_AVG_WINDOW[B_TYPE] = 4.0;
        break;
    default: // BBP
        K_AVG_WINDOW[P_TYPE] = 3.0;
        K_AVG_WINDOW[B_TYPE] = 7.0;
        break;
    }        


    int buffer_safe = 3 * per_pict_bits ;
    undershoot_carry = (encparams.video_buffer_size - buffer_safe)/6;
    if( undershoot_carry < 0 )
        mjpeg_error_exit1("Rate control can't cope with a video buffer smaller 4 frame intervals");
    overshoot_gain =  encparams.bit_rate / (encparams.video_buffer_size-buffer_safe);

	bits_per_mb = (double)encparams.bit_rate / (encparams.mb_per_pict);

	/*
	  Reaction paramer - i.e. quantisation feedback gain relative
	  to bit over/undershoot.
	  For normal frames it is fairly modest as we can compensate
	  over multiple frames and can average out variations in image
	  complexity.

	*/
    fb_gain = (int)floor(2.0*encparams.bit_rate/encparams.decode_frame_rate);


	/* Set the virtual buffers for per-frame rate control feedback to
	   values corresponding to the quantisation floor (if specified)
	   or a "reasonable" quantisation (6.0) if not.
	*/

	init_quant = (encparams.quant_floor > 0.0 ? encparams.quant_floor : 6.0);
    int i;
    for( i = FIRST_PICT_TYPE; i <= LAST_PICT_TYPE; ++i )
        ratectl_vbuf[i] = static_cast<int>(init_quant * fb_gain / 62.0);

    vbuf_fullness = ratectl_vbuf[I_TYPE];
	next_ip_delay = 0.0;
	decoding_time = 0.0;
}


void Pass1RateCtl::InitGOP( int np, int nb)
{
	N[P_TYPE] = encparams.fieldpic ? 2*np+1 : 2*np;
	N[B_TYPE] = encparams.fieldpic ? 2*nb : 2*nb;
	N[I_TYPE] = encparams.fieldpic ? 1 : 2;
	fields_in_gop = N[I_TYPE] + N[P_TYPE] + N[B_TYPE];

	/*
	  At the start of a GOP before any frames have gone the
	  actual buffer state represents a long term average. Any
	  undershoot due to the I_frame of the previous GOP
	  should by now have been caught up.
	*/

	gop_buffer_correction = 0;

    int i;
	if( first_gop)
	{
		mjpeg_debug( "FIRST GOP INIT");
		fast_tune = true;
		first_gop = false;
        for( i = FIRST_PICT_TYPE; i <= LAST_PICT_TYPE; ++i )
        {
            first_encountered[i] = true;
            pict_base_bits[i] = per_pict_bits;
        }
	}
	else
	{
		mjpeg_debug( "REST GOP INIT" );
		double recovery_fraction = field_rate/(overshoot_gain * fields_in_gop);
		double recovery_gain = 
			recovery_fraction > 1.0 ? 1.0 : overshoot_gain * recovery_fraction;
		int available_bits = 
			static_cast<int>( (encparams.bit_rate+buffer_variation*recovery_gain)
							  * fields_in_gop/field_rate);
        double Xsum = 0.0;
        for( i = FIRST_PICT_TYPE; i <= LAST_PICT_TYPE; ++i )
            Xsum += N[i]*Xhi[i];
        for( i = FIRST_PICT_TYPE; i <= LAST_PICT_TYPE; ++i )
            pict_base_bits[i] =  
                static_cast<int32_t>(fields_per_pict*available_bits*Xhi[i]/Xsum);
		fast_tune = false;

	}

}


/* Step 1: compute target bits for current picture being coded */
void Pass1RateCtl::InitNewPict(Picture &picture, int64_t bitcount_SOP)
{
 	double target_Q;
	double current_Q;
	int available_bits;
	double Xsum,varsum;

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
	
	picture.ActivityMeasures( actsum, varsum );
	avg_act = actsum/(double)(encparams.mb_per_pict);
	avg_var = varsum/(double)(encparams.mb_per_pict);
	sum_avg_act += avg_act;
	sum_avg_var += avg_var;
	actcovered = 0.0;
	sum_vbuf_Q = 0.0;


	/* Allocate target bits for frame based on frames numbers in GOP
	   weighted by:
	   - global complexity averages
	   - predicted activity measures
	   - fixed type based weightings
	   
	   T = (Nx * Xx/Kx) / Sigma_j (Nj * Xj / Kj)

	   N.b. B frames are an exception as there is *no* predictive
	   element in their bit-allocations.  The reason this is done is
	   that highly active B frames are inevitably the result of
	   transients and/or scene changes.  Psycho-visual considerations
	   suggest there's no point rendering sudden transients
	   terribly well as they're not percieved accurately anyway.  

	   In the case of scene changes similar considerations apply.  In
	   this case also we want to save bits for the next I or P frame
	   where they will help improve other frames too.  
	   TODO: Experiment with *inverse* predictive correction for B-frames
	   and turning off active-block boosting for B-frames.

	   Note that we have to calulate per-frame bits by scaling the one-second
	   bit-pool a one-GOP bit-pool.
	*/

	
	if( encparams.still_size > 0 )
		available_bits = per_pict_bits;
	else
	{
		int feedback_correction =
			static_cast<int>( fast_tune 
							  ?	buffer_variation * overshoot_gain
							  : (buffer_variation+gop_buffer_correction) 
							    * overshoot_gain
				);
		available_bits = 
			static_cast<int>( (encparams.bit_rate+feedback_correction)
							  * fields_in_gop/field_rate
							  );
	}

    Xsum = 0.0;
    int i;
    for( i = FIRST_PICT_TYPE; i <= LAST_PICT_TYPE; ++i )
        Xsum += N[i]*Xhi[i];
    //vbuf_fullness = ratectl_vbuf[picture.pict_type];
    double first_weight[NUM_PICT_TYPES];
    first_weight[I_TYPE] = 1.0;
    first_weight[P_TYPE] = 1.7;
    first_weight[B_TYPE] = 1.7*2.0;

    double gop_frac = 0.0;
    if( first_encountered[picture.pict_type] )
    {
        for( i = FIRST_PICT_TYPE; i <= LAST_PICT_TYPE; ++i )
            gop_frac += N[i]/first_weight[i];
        target_bits = 
            static_cast<int32_t>(fields_per_pict*available_bits /
                                 (gop_frac * first_weight[picture.pict_type]));
    }
    else
        target_bits =
            static_cast<int32_t>(fields_per_pict*available_bits*Xhi[picture.pict_type]/Xsum);

	/* 
	   If we're fed a sequences of identical or near-identical images
	   we can get actually get allocations for frames that exceed
	   the video buffer size!  This of course won't work so we arbitrarily
	   limit any individual frame to 3/4's of the buffer.
	*/

	target_bits = min( target_bits, encparams.video_buffer_size*3/4 );

	mjpeg_debug( "Frame %c T=%05d A=%06d  Xi=%.2f Xp=%.2f Xb=%.2f", 
                 pict_type_char[picture.pict_type],
                 (int)target_bits/8, (int)available_bits/8, 
                 Xhi[I_TYPE], Xhi[P_TYPE],Xhi[B_TYPE] );


	/* 
	   To account for the wildly different sizes of frames
	   we compute a correction to the current instantaneous
	   buffer state that accounts for the fact that all other
	   thing being equal buffer will go down a lot after the I-frame
	   decode but fill up again through the B and P frames.

	   For this we use the base bit allocations of the picture's
	   "pict_base_bits" which will pretty accurately add up to a
	   GOP-length's of bits not the more dynamic predictive T target
	   bit-allocation (which *won't* add up very well).
	*/

	gop_buffer_correction += (pict_base_bits[picture.pict_type]-per_pict_bits);


	/* Undershot bits have been "returned" via R */
    vbuf_fullness = max( vbuf_fullness, 0 );

	/* We don't let the target volume get absurdly low as it makes some
	   of the prediction maths ill-condtioned.  At these levels quantisation
	   is always minimum anyway
	*/
	target_bits = max( target_bits, 4000 );


	current_Q = ScaleQuant(picture.q_scale_type,62.0*vbuf_fullness / fb_gain);

	picture.avg_act = avg_act;
	picture.sum_avg_act = sum_avg_act;

	S = bitcount_SOP;

}

void Pass1RateCtl::InitKnownPict(Picture &picture)
{
    abort();
}

/*
 * Update rate-controls statistics after pictures has ended..
 *
 * RETURN: The amount of padding necessary for picture to meet syntax or
 * rate constraints...
 */

void Pass1RateCtl::UpdatePict( Picture &picture, int64_t _bitcount_EOP,
                               int &padding_needed, bool &recode )
{
	double X;
	double K;
	double AQ;
	int32_t actual_bits;		/* Actual (inc. padding) picture bit counts */
	int    i;
	int    Qsum;
	int frame_overshoot;
    int64_t bitcount_EOP = _bitcount_EOP; 
	actual_bits = bitcount_EOP - S;
	frame_overshoot = (int)actual_bits-(int)target_bits;

	/* For the virtual buffers for quantisation feedback we compensate
     * for the relative size of frames (1.0 = I Frames) so that we
     * don't have too much feedback gain for small frames
	*/
    mjpeg_info( "TODO actually make this happen!!!!" );
    abort(); 
	vbuf_fullness += frame_overshoot;

	picture.pad = 0;

	/*
	  Compute the estimate of the current decoder buffer state.  We
	  use this to feedback-correct the available bit-pool with a
	  fraction of the current buffer state estimate.  If we're ahead
	  of the game we allow a small increase in the pool.  If we
	  dropping towards a dangerously low buffer we decrease the pool
	  (rather more vigorously).
	  
	  Note that since we cannot hold more than a buffer-full if we have
	  a positive buffer_variation in CBR we assume it was padded away
	  and in VBR we assume we only sent until the buffer was full.
	*/

	
	bits_used += (bitcount_EOP-prev_bitcount);
	prev_bitcount = bitcount_EOP;
    
	bits_transported += per_pict_bits;
	//mjpeg_debug( "TR=%" PRId64 " USD=%" PRId64 "", bits_transported/8, bits_used/8);
	buffer_variation  = (int32_t)(bits_transported - bits_used);

	if( buffer_variation > 0 )
	{
		if( encparams.quant_floor > 0 )
		{
			bits_transported = bits_used;
			buffer_variation = 0;	
		}
		else if( buffer_variation > undershoot_carry )
		{
			bits_used = bits_transported + undershoot_carry;
			buffer_variation = undershoot_carry;
		}
	}


	Qsum = 0;
	for( i = 0; i < encparams.mb_per_pict; ++i )
	{
		Qsum += picture.mbinfo[i].mquant;
	}

	
    /* AQ is the average Quantisation of the block.
	   Its only used for stats display as the integerisation
	   of the quantisation value makes it rather coarse for use in
	   estimating bit-demand */
	AQ = (double)Qsum/(double)encparams.mb_per_pict;
	sum_avg_quant += AQ;
	
	/* X (Chi - Complexity!) is an estimate of "bit-demand" for the
	frame.  I.e. how many bits it would need to be encoded without
	quantisation.  It is used in adaptively allocating bits to busy
	frames. It is simply calculated as bits actually used times
	average target (not rounded!) quantisation.

	K is a running estimate of how bit-demand relates to frame
	activity - bits demand per activity it is used to allow
	prediction of quantisation needed to hit a bit-allocation.
	*/

	X = actual_bits  * AQ;

    /* To handle longer sequences with little picture content
       where I, B and P frames are of unusually similar size we
       insist I frames assumed to be at least one and a half times
       as complex as typical P frames
    */
    if( picture.pict_type == I_TYPE )
        X = fmax(X, 1.5*Xhi[P_TYPE]);

	K = X / actsum;
	picture.AQ = AQ;
	picture.SQ = sum_avg_quant;

	//mjpeg_debug( "D=%d R=%d GC=%d", buffer_variation/8, (int)R/8, 
    //gop_buffer_correction/8  );

	/* Xi are used as a guesstimate of *typical* frame activities
	   based on the past.  Thus we don't want anomalous outliers due
	   to scene changes swinging things too much (this is handled by
	   the predictive complexity measure stuff) so we use moving
	   averages.  The weightings are intended so all 3 averages have
	   similar real-time decay periods based on an assumption of
	   20-30Hz frame rates.
	*/
    //ratectl_vbuf[picture.pict_type] = vbuf_fullness;
    sum_size[picture.pict_type] += actual_bits/8.0;
    pict_count[picture.pict_type] += 1;

    if( first_encountered[picture.pict_type] )
    {
        Xhi[picture.pict_type] = X;
        first_encountered[picture.pict_type] = false;
    }
    else
    {
        double win = fast_tune 
            ? K_AVG_WINDOW[picture.pict_type] / 1.7
            : K_AVG_WINDOW[picture.pict_type];
        Xhi[picture.pict_type] = (X + win*Xhi[picture.pict_type])/(win+1.0);
    }

    mjpeg_debug( "Frame %c A=%6.0f %.2f: I = %6.0f P = %5.0f B = %5.0f",
                 pict_type_char[picture.pict_type],
                 actual_bits/8.0,
                 X,
                 sum_size[I_TYPE]/pict_count[I_TYPE],
                 sum_size[P_TYPE]/pict_count[P_TYPE],
                 sum_size[B_TYPE]/pict_count[B_TYPE]
        );
    
                
	VbvEndOfPict(picture, bitcount_EOP);

    padding_needed = 0;
    recode = false;
}

/* compute initial quantization stepsize (at the beginning of picture) 
   encparams.quant_floor != 0 is the VBR case where we set a bitrate as a (high)
   maximum and then put a floor on quantisation to achieve a reasonable
   overall size.
 */

int Pass1RateCtl::InitialMacroBlockQuant(Picture &picture)
{
	
	int mquant = ScaleQuant( picture.q_scale_type, vbuf_fullness*62.0/fb_gain );
    picture_mquant = max(mquant, static_cast<int>(encparams.quant_floor));
    return picture_mquant;
}


/*************
 *
 * SelectQuantization - we keep the same quantisation for the entire frame.
 * This is definately sensible for pass-1 work, and probably useful for
 * even for normal work - if we have a mechanism for recovery from unexpectedly
 * spikes in complexity
 *
 ************/

int Pass1RateCtl::MacroBlockQuant( const MacroBlock &mb, int64_t bitcount )
{
	return picture_mquant;
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

void Pass1RateCtl::VbvEndOfPict(Picture &picture, int64_t bitcount)
{
	bitcnt_EOP = bitcount - BITCOUNT_OFFSET;

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

void Pass1RateCtl::CalcVbvDelay(Picture &picture)
{
	
	/* number of 1/90000 s ticks until next picture is to be decoded */
	if (picture.pict_type == B_TYPE)
	{
		if (encparams.prog_seq)
		{
			if (!picture.repeatfirst)
				picture_delay = 90000.0/encparams.frame_rate; /* 1 frame */
			else
			{
				if (!picture.topfirst)
					picture_delay = 90000.0*2.0/encparams.frame_rate; /* 2 frames */
				else
					picture_delay = 90000.0*3.0/encparams.frame_rate; /* 3 frames */
			}
		}
		else
		{
			/* interlaced */
			if (encparams.fieldpic)
				picture_delay = 90000.0/(2.0*encparams.frame_rate); /* 1 field */
			else
			{
				if (!picture.repeatfirst)
					picture_delay = 90000.0*2.0/(2.0*encparams.frame_rate); /* 2 flds */
				else
					picture_delay = 90000.0*3.0/(2.0*encparams.frame_rate); /* 3 flds */
			}
		}
	}
	else
	{
		/* I or P picture */
		if (encparams.fieldpic)
		{
			if(picture.topfirst && (picture.pict_struct==TOP_FIELD))
			{
				/* first field */
				picture_delay = 90000.0/(2.0*encparams.frame_rate);
			}
			else
			{
				/* second field */
				/* take frame reordering delay into account */
				picture_delay = next_ip_delay - 90000.0/(2.0*encparams.frame_rate);
			}
		}
		else
		{
			/* frame picture */
			/* take frame reordering delay into account*/
			picture_delay = next_ip_delay;
		}

		if (!encparams.fieldpic || 
			picture.topfirst!=(picture.pict_struct==TOP_FIELD))
		{
			/* frame picture or second field */
			if (encparams.prog_seq)
			{
				if (!picture.repeatfirst)
					next_ip_delay = 90000.0/encparams.frame_rate;
				else
				{
					if (!picture.topfirst)
						next_ip_delay = 90000.0*2.0/encparams.frame_rate;
					else
						next_ip_delay = 90000.0*3.0/encparams.frame_rate;
				}
			}
			else
			{
				if (encparams.fieldpic)
					next_ip_delay = 90000.0/(2.0*encparams.frame_rate);
				else
				{
					if (!picture.repeatfirst)
						next_ip_delay = 90000.0*2.0/(2.0*encparams.frame_rate);
					else
						next_ip_delay = 90000.0*3.0/(2.0*encparams.frame_rate);
				}
			}
		}
	}

	if (decoding_time==0.0)
	{
		/* first call of calc_vbv_delay */
		/* we start with a 7/8 filled VBV buffer (12.5% back-off) */
		picture_delay = ((encparams.vbv_buffer_size*7)/8)*90000.0/encparams.bit_rate;
		if (encparams.fieldpic)
			next_ip_delay = (int)(90000.0/encparams.frame_rate+0.5);
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
	if (!encparams.low_delay && (decoding_time < (double)bitcnt_EOP*90000.0/encparams.bit_rate))
	{
		/* picture not completely in buffer at intended decoding time */
		mjpeg_warn("vbv_delay underflow frame %d (target=%.1f, actual=%.1f)",
				   frame_num-1, decoding_time, bitcnt_EOP*90000.0/encparams.bit_rate);
	}


	/* when to decode current frame */
	decoding_time += picture_delay;


	/* check for overflow (current picture).  Unless verbose warn
	   only if overflow must be at least in part due to an oversize
	   frame (rather than undersize predecessor).
	   	*/

	picture.vbv_delay = (int)(decoding_time - ((double)bitcnt_EOP)*90000.0/bit_rate);

	if ( decoding_time * ((double)bit_rate  / 90000.0) - ((double)bitcnt_EOP)
		> vbv_buffer_size )
	{
		double oversize = encparams.vbv_buffer_size -
			(decoding_time / 90000.0 * bit_rate - (double)(bitcnt_EOP+frame_undershoot));
		if(!quiet || oversize > 0.0  )
			mjpeg_warn("vbv_delay overflow frame %d - %f.0 bytes!", 
					   frame_num,
					   oversize / 8.0
				);
	}



	if (picture.vbv_delay<0)
	{
		mjpeg_warn("vbv_delay underflow: %d",picture.vbv_delay);
		picture.vbv_delay = 0;
	}



	if (picture.vbv_delay>65535)
	{
		mjpeg_warn("vbv_delay frame %d exceeds permissible range: %d",
				   frame_num, picture.vbv_delay);
		picture.vbv_delay = 65535;
	}
#else
	if( !encparams.mpeg1 || encparams.quant_floor != 0 )
		picture.vbv_delay =  0xffff;
	else if( encparams.still_size > 0 )
		picture.vbv_delay =  static_cast<int>(90000.0/encparams.frame_rate/4);
#endif

}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
