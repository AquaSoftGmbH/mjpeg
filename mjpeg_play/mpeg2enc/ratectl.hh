#ifndef _RATECTL_HH
#define _RATECTL_HH

/*  (C) 2003 Andrew Stevens */

/*  This is free software; you can redistribute it
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
#include "mjpeg_types.h"
#include "mpeg2syntaxcodes.h"

class MacroBlock;
class EncoderParams;
class Picture;


class RateCtl
{
public:
    RateCtl( EncoderParams &encoder );
	virtual void InitSeq( bool reinit ) = 0;
	virtual void InitGOP( int nb, int np ) = 0;
	virtual void InitNewPict (Picture &picture) = 0;
	virtual void InitKnownPict (Picture &picture) = 0;
	virtual void UpdatePict (Picture &picture, int &padding_needed ) = 0;
    virtual int MacroBlockQuant(  const MacroBlock &mb) = 0;
	virtual int  InitialMacroBlockQuant(Picture &picture) = 0;
	virtual void CalcVbvDelay (Picture &picture) = 0;

    static double InvScaleQuant(  int q_scale_type, int raw_code );
    static int ScaleQuant( int q_scale_type, double quant );
protected:
	virtual void VbvEndOfPict (Picture &picture) = 0;
    double ScaleQuantf( int q_scale_type, double quant );
    EncoderParams &encparams;
};

class OnTheFlyRateCtl : public RateCtl
{
public:
	OnTheFlyRateCtl( EncoderParams &encoder );
	virtual void InitSeq( bool reinit );
	virtual void InitGOP( int nb, int np );
	virtual void InitNewPict (Picture &picture);
	virtual void InitKnownPict (Picture &picture);
	virtual void UpdatePict ( Picture &picture, int &padding_needed );
	virtual int  MacroBlockQuant( const MacroBlock &mb);
	virtual int  InitialMacroBlockQuant(Picture &picture);
	virtual void CalcVbvDelay (Picture &picture);
private:
	virtual void VbvEndOfPict (Picture &picture);

    int     cur_mquant;
    int     mquant_change_ctr;
    
	int32_t fb_gain;
	
	/* R - Remaining bits available in the next one second period.
	   T - Target bits for current frame 
	   d - Current virtual reciever buffer fullness for quantisation
	   purposes updated using scaled difference of target bit usage
	   and actual usage
       d0[picture_type] - Virtual buffer for each frame type.
	*/
	int32_t target_bits; // target_bits
	int32_t vbuf_fullness;
    int32_t ratectl_vbuf[NUM_PICT_TYPES];

	int32_t per_pict_bits;
	int     fields_in_gop;
	double  field_rate;
	int     fields_per_pict;

	int32_t buffer_variation;
	int64_t bits_transported;
	int64_t bits_used;
	int32_t gop_buffer_correction;

    /* bitcnt_EOP - Position in generated bit-stream for latest
	   end-of-picture Comparing these values with the
	   bit-stream position for when the picture is due to be
	   displayed allows us to see what the vbv buffer is up
	   to.
	*/

	int frame_overshoot_margin;
	int undershoot_carry;
	double overshoot_gain;

    /*
	  actsum - Total activity (sum block variances) in frame
	  actcovered - Activity macroblocks so far quantised (used to
	  fine tune quantisation to avoid starving highly
	  active blocks appearing late in frame...) UNUSED
	  avg_act - Current average activity...
	*/
	double actsum;
	double actcovered;
	double sum_avg_act;
	double avg_act;
	double avg_var;
	double sum_avg_var;
	double sum_avg_quant;
	double sum_vbuf_Q;

    int N[NUM_PICT_TYPES];


	int min_d, max_d;
	int min_q, max_q;

	double bits_per_mb;
	bool fast_tune;
	bool first_gop;
	

    /* X's measure global complexity (Chi! not X!) of frame types.
	* Actually: X = average quantisation * bits allocated in *previous* frame
	* N.b. the choice of measure is *not* arbitrary.  The feedback bit
	* rate control gets horribly messed up if it is *not* proportionate
	* to bit demand i.e. bits used scaled for quantisation.  
	* d's are virtual reciever buffer fullness 
	* r is Rate control feedback gain (in* bits/frame) 
	*/
    
    double Xhi[NUM_PICT_TYPES];

	/* The average complexity of frames of the different types is used
     * to predict a reasonable bit-allocation for these types.
	 * The AVG_WINDOW set the size of the sliding window for these
     * averages.  Basically I Frames respond very quickly.
     * B / P frames more or less quickly depending on the target number
     * of B frames per P frame.
	 */
    double K_AVG_WINDOW[NUM_PICT_TYPES];

	/*
     * 'Typical' sizes of the different types of picture in a GOP - these
     * sizes are needed so that buffer management can compensate for the
     * 'normal' ebb and flow of buffer space in a GOP (low after a big I frame)
     * nearly full at the end after lots of smaller B/P frames.
     *
     */
    int32_t pict_base_bits[NUM_PICT_TYPES];
    bool first_encountered[NUM_PICT_TYPES];

    
    /*
     * Reinitialisation data for recoding pictures where prediction is too
     * far off.
     *
     */
    double actual_Xhi;
    double actual_avg_Q;


    // Some statistics for measuring if things are going well.
    double sum_size[NUM_PICT_TYPES];
    int pict_count[NUM_PICT_TYPES];

	// VBV calculation data
	double picture_delay;
	double next_ip_delay; /* due to frame reordering delay */
	double decoding_time;

};

class Pass1RateCtl : public RateCtl
{
public:
	Pass1RateCtl( EncoderParams &encoder );
	virtual void InitSeq( bool reinit );
	virtual void InitGOP( int nb, int np );
	virtual void InitNewPict (Picture &picture);
	virtual void InitKnownPict (Picture &picture);
	virtual void UpdatePict (Picture &picture, int &padding_needed);
	virtual int  MacroBlockQuant( const MacroBlock &mb );
	virtual int  InitialMacroBlockQuant(Picture &picture);
	virtual void CalcVbvDelay (Picture &picture);
private:
	virtual void VbvEndOfPict (Picture &picture);


	int32_t fb_gain;
	
	/* R - Remaining bits available in the next one second period.
	   T - Target bits for current frame 
	   d - Current virtual reciever buffer fullness for quantisation
	   purposes updated using scaled difference of target bit usage
	   and actual usage
       d0[picture_type] - Virtual buffer for each frame type.
	*/
	int32_t target_bits; // target_bits
	int32_t vbuf_fullness;
    int32_t ratectl_vbuf[NUM_PICT_TYPES];

	int32_t per_pict_bits;
	int     fields_in_gop;
	double  field_rate;
	int     fields_per_pict;

	int32_t buffer_variation;
	int64_t bits_transported;
	int64_t bits_used;
	int32_t gop_buffer_correction;

    /* bitcnt_EOP - Position in generated bit-stream for latest
	   end-of-picture Comparing these values with the
	   bit-stream position for when the picture is due to be
	   displayed allows us to see what the vbv buffer is up
	   to.
	*/

	int64_t bitcnt_EOP;
	int64_t prev_bitcount;
	int frame_overshoot_margin;
	int undershoot_carry;
	double overshoot_gain;

    /*
	  actsum - Total activity (sum block variances) in frame
	  actcovered - Activity macroblocks so far quantised (used to
	  fine tune quantisation to avoid starving highly
	  active blocks appearing late in frame...) UNUSED
	  avg_act - Current average activity...
	*/
	double actsum;
	double actcovered;
	double sum_avg_act;
	double avg_act;
	double avg_var;
	double sum_avg_var;
	double sum_avg_quant;
	double sum_vbuf_Q;

    int N[NUM_PICT_TYPES];
	int64_t S;


	double bits_per_mb;
	bool fast_tune;
	bool first_gop;
	

    /* X's measure global complexity (Chi! not X!) of frame types.
	* Actually: X = average quantisation * bits allocated in *previous* frame
	* N.b. the choice of measure is *not* arbitrary.  The feedback bit
	* rate control gets horribly messed up if it is *not* proportionate
	* to bit demand i.e. bits used scaled for quantisation.  
	* d's are virtual reciever buffer fullness 
	* r is Rate control feedback gain (in* bits/frame) 
	*/
    
    double Xhi[NUM_PICT_TYPES];

	/* The average complexity of frames of the different types is used
     * to predict a reasonable bit-allocation for these types.
	 * The AVG_WINDOW set the size of the sliding window for these
     * averages.  Basically I Frames respond very quickly.
     * B / P frames more or less quickly depending on the target number
     * of B frames per P frame.
	 */
    double K_AVG_WINDOW[NUM_PICT_TYPES];

	/*
     * 'Typical' sizes of the different types of picture in a GOP - these
     * sizes are needed so that buffer management can compensate for the
     * 'normal' ebb and flow of buffer space in a GOP (low after a big I frame)
     * nearly full at the end after lots of smaller B/P frames.
     *
     */
    int32_t pict_base_bits[NUM_PICT_TYPES];

    bool first_encountered[NUM_PICT_TYPES];

    // Some statistics for measuring if things are going well.
    double sum_size[NUM_PICT_TYPES];

    int pict_count[NUM_PICT_TYPES];

	// VBV calculation data
	double picture_delay;
	double next_ip_delay; /* due to frame reordering delay */
	double decoding_time;
    int picture_mquant;
    
};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
