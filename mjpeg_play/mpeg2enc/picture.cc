/* Picture encoding object */


/* (C) Andrew Stevens 2003
 *  This file is free software; you can redistribute it
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

#include "config.h"
#include <cassert>
#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "mpeg2syntaxcodes.h"
#include "cpu_accel.h"
#include "motionsearch.h"
#include "encoderparams.hh"
#include "mpeg2coder.hh"
#include "quantize.hh"
#include "seqencoder.hh"
#include "ratectl.hh"
#include "tables.h"
#include "imageplanes.hh"


Picture::Picture( EncoderParams &_encparams, 
                  ElemStrmWriter &writer, 
                  Quantizer &_quantizer ) :
    encparams( _encparams ),
    quantizer( _quantizer ),
    coding( new MPEG2CodingBuf( _encparams, writer) )
{
	int i,j;
	/* Allocate buffers for picture transformation */
	blocks = 
        static_cast<DCTblock*>(
            bufalloc(encparams.mb_per_pict*BLOCK_COUNT*sizeof(DCTblock)));
	qblocks =
		static_cast<DCTblock *>(
            bufalloc(encparams.mb_per_pict*BLOCK_COUNT*sizeof(DCTblock)));
    DCTblock *block = blocks;
    DCTblock *qblock = qblocks;
    for (j=0; j<encparams.enc_height2; j+=16)
    {
        for (i=0; i<encparams.enc_width; i+=16)
        {
            mbinfo.push_back(MacroBlock(*this, i,j, block,qblock ));
            block += BLOCK_COUNT;
            qblock += BLOCK_COUNT;
        }
    }


    rec_img = new ImagePlanes( encparams );
    pred   = new ImagePlanes( encparams );

    // Initialise the reference image pointers to NULL to ensure errors show
    org_img = 0;
    fwd_rec = fwd_org = 0;
    bwd_rec = bwd_org = 0;
}


Picture::~Picture()
{
    int i;
    delete rec_img;
    delete pred;
    delete coding;
}

/*
 *
 * Reconstruct the decoded image for references images and
 * for statistics
 *
 */


void Picture::Reconstruct()
{

#ifndef OUTPUT_STAT
	if( pict_type!=B_TYPE)
	{
#endif
		IQuantize();
		ITransform();
		CalcSNR();
		Stats();
#ifndef OUTPUT_STAT
	}
#endif
}

/*******************************************
 *
 * Set picture encoding parameters that depend
 * on the frame being encoded.
 *
 ******************************************/

void Picture::SetFrameParams( const StreamState &ss )
{
    new_seq = ss.new_seq;
    end_seq = ss.end_seq;
    gop_decode = ss.g_idx;
    bgrp_decode = ss.b_idx;
    decode = ss.DecodeNum();
    present = ss.PresentationNum();
    temp_ref = ss.TemporalReference();
    last_picture = ss.EndOfStream();
    nb = ss.nb;
    np = ss.np;
    closed_gop = ss.closed_gop;
    dc_prec = encparams.dc_prec;
}



/************************************************************************
 *
 * Set encoding parameters that depend on which field of a frame
 * is being encoded.
 *
 ************************************************************************/


void Picture::SetFieldParams(int field)
{
    secondfield = (field == 1);

    if( bgrp_decode == 0 )             // Start of a B-group: I or P frame
    {
        if (gop_decode==0 ) /* first encoded frame in GOP is I */
        {
            if( field == 0)
            {
                gop_start = true;
                ipflag = 0;
                pict_type = I_TYPE;

            }
            else // P field of I-frame
            {
                gop_start = false;
                ipflag = 1;
                pict_type = P_TYPE;
            }
        }
        else 
        {
            pict_type = P_TYPE;
            gop_start = false;
            closed_gop = false;
            new_seq = false;
        }
    }
    else
    {
        closed_gop = false;
        pict_type = B_TYPE;
        gop_start = false;
        new_seq = false;
    }

    secondfield = (field == 1);
        
    /* Handle picture structure... */
    if( encparams.fieldpic )
    {
        pict_struct = ((encparams.topfirst) ^ (field == 1)) ? TOP_FIELD : BOTTOM_FIELD;
        topfirst = 0;
        repeatfirst = 0;
    }

    /* Handle 3:2 pulldown frame pictures */
    else if( encparams.pulldown_32 )
    {
        pict_struct = FRAME_PICTURE;
        switch( present % 4 )
        {
            case 0 :
                repeatfirst = 1;
                topfirst = encparams.topfirst;          
                break;
            case 1 :
                repeatfirst = 0;
                topfirst = !encparams.topfirst;
                break;
            case 2 :
                repeatfirst = 1;
                topfirst = !encparams.topfirst;
                break;
            case 3 :
                repeatfirst = 0;
                topfirst = encparams.topfirst;
                break;
        }
    }
    /* Handle ordinary frame pictures */
    else
    {
        pict_struct = FRAME_PICTURE;
        repeatfirst = 0;
        topfirst = encparams.topfirst;
    }

    forw_hor_f_code = encparams.motion_data[bgrp_decode].forw_hor_f_code;
    forw_vert_f_code = encparams.motion_data[bgrp_decode].forw_vert_f_code;
    sxf = encparams.motion_data[bgrp_decode].sxf;
    syf = encparams.motion_data[bgrp_decode].syf;

    switch ( pict_type )
    {
        case I_TYPE :
            forw_hor_f_code = 15;
            forw_vert_f_code = 15;
            back_hor_f_code = 15;
            back_vert_f_code = 15;
            break;
        case P_TYPE :
            back_hor_f_code = 15;
            back_vert_f_code = 15;
            break;
        case B_TYPE :
            back_hor_f_code = encparams.motion_data[bgrp_decode].back_hor_f_code;
            back_vert_f_code = encparams.motion_data[bgrp_decode].back_vert_f_code;
            sxb = encparams.motion_data[bgrp_decode].sxb;
            syb = encparams.motion_data[bgrp_decode].syb;
            break;
    }

    /* We currently don't support frame-only DCT/Motion Est.  for non
    progressive frames */
    prog_frame = encparams.frame_pred_dct_tab[pict_type-1];
    frame_pred_dct = encparams.frame_pred_dct_tab[pict_type-1];
    q_scale_type = encparams.qscale_tab[pict_type-1];
    intravlc = encparams.intravlc_tab[pict_type-1];
    altscan = encparams.altscan_tab[pict_type-1];
    scan_pattern = (altscan ? alternate_scan : zig_zag_scan);

    /* If we're using B frames then we reserve unit coefficient
    dropping for them as B frames have no 'knock on' information
    loss */
    if( pict_type == B_TYPE || encparams.M == 1 )
    {
        unit_coeff_threshold = abs( encparams.unit_coeff_elim );
        unit_coeff_first = encparams.unit_coeff_elim < 0 ? 0 : 1;
    }
    else
    {
        unit_coeff_threshold = 0;
        unit_coeff_first = 0;
    }
        
}



void Picture::IQuantize()
{
    int k;
	for (k=0; k<encparams.mb_per_pict; k++)
	{
        mbinfo[k].IQuantize( quantizer );
	}
}


double Picture::VarSumBestMotionComp()
{
    double var_sum = 0.0;
    vector<MacroBlock>::iterator i;
    for( i = mbinfo.begin(); i < mbinfo.end(); ++i )
    {
        var_sum += i->best_me->var;
    }
    return var_sum;
}

double Picture::VarSumBestFwdMotionComp()
{
    double var_sum = 0.0;
    vector<MacroBlock>::iterator i;
    for( i = mbinfo.begin(); i < mbinfo.end(); ++i )
    {
        var_sum += i->best_fwd_me->var;
    }
    return var_sum;
}

double Picture::ActivityBestMotionComp()
{
	double actj,sum;
	int blksum;
	sum = 0.0;
    vector<MacroBlock>::iterator i;
    for (i = mbinfo.begin(); i < mbinfo.end(); ++i )
    {
        /* A.Stevens Jul 2000 Luminance variance *has* to be a
           rotten measure of how active a block in terms of bits
           needed to code a lossless DCT.  E.g. a half-white
           half-black block has a maximal variance but pretty
           small DCT coefficients.

           So.... instead of luminance variance as used in the
           original we use the absolute sum of DCT coefficients as
           our block activity measure.  */

        if( i->best_me->mb_type  & MB_INTRA )
        {
            /* Compensate for the wholly disproprotionate weight
             of the DC coefficients.  Shold produce more sensible
             results...  yes... it *is* an mostly empirically derived
             fudge factor ;-)
            */
            blksum =  -80*COEFFSUM_SCALE;
            for( int l = 0; l < 6; ++l )
                blksum += 
                    quantizer.WeightCoeffIntra( i->RawDCTblocks()[l] ) ;
        }
        else
        {
            blksum = 0;
            for( int l = 0; l < 6; ++l )
                blksum += 
                    quantizer.WeightCoeffInter( i->RawDCTblocks()[l] ) ;
        }
        /* It takes some bits to code even an entirely zero block...
           It also makes a lot of calculations a lot better conditioned
           if it can be guaranteed that activity is always distinctly
           non-zero.
         */


        actj = (double)blksum / (double)COEFFSUM_SCALE;
        if( actj < 12.0 )
            actj = 12.0;

        i->act = actj;
        sum += actj;
    }
    return sum;

}

/* inverse transform prediction error and add prediction */
void Picture::ITransform()
{
    vector<MacroBlock>::iterator mbi;
	for( mbi = mbinfo.begin(); mbi < mbinfo.end(); ++mbi)
	{
		mbi->ITransform();
	}
}

void Picture::MotionSubSampledLum( )
{
	int linestride;
    EncoderParams &eparams = encparams;
	/* In an interlaced field the "next" line is 2 width's down rather
	   than 1 width down  .
       TODO: Shoudn't we be treating the frame as interlaced for
       frame based interlaced encoding too... or at least for the
       interlaced ME modes?
    */

	if (!eparams.fieldpic)
	{
		linestride = eparams.phy_width;
	}
	else
	{
		linestride = 2*eparams.phy_width;
	}

    uint8_t *org_Y = org_img->Plane(0);
    psubsample_image( org_Y, 
                                 linestride,
                                 org_Y+eparams.fsubsample_offset, 
                                 org_Y+eparams.qsubsample_offset );
}


/* ************************************************
 *
 * QuantiseAndEncode - Quantise and Encode a picture.
 *
 * NOTE: It may seem perverse to quantise at the same time as
 * coding-> However, actually makes (limited) sense
 * - feedback from the *actual* bit-allocation may be used to adjust 
 * quantisation "on the fly". This is good for fast 1-pass no-look-ahead coding->
 * - The coded result is in any even only buffered not actually written
 * out. We can back off and try again with a different quantisation
 * easily.
 * - The alternative is calculating size and generating actual codes seperately.
 * The poorer cache coherence of this latter probably makes the performance gain
 * modest.
 *
 * *********************************************** */

void Picture::QuantiseAndCode(RateCtl &ratectl)
{
    // Set rate control for new picture
    ratectl.InitPict(*this);

    // Generate Sequence/GOP/Picture headers for this picture
    PutHeaders();

    /* Now the actual quantisation and encoding->.. */     
 
    int i, j, k;
    int MBAinc;
    MacroBlock *cur_mb = 0;
	int mquant_pred = ratectl.InitialMacroBlockQuant(*this);

	k = 0;
    
    /* TODO: We're currently hard-wiring each macroblock row as a
       slice.  For MPEG-2 we could do this better and reduce slice
       start code coverhead... */

	for (j=0; j<encparams.mb_height2; j++)
	{
        PutSliceHdr(j, mquant_pred);
        Reset_DC_DCT_Pred();
        Reset_MV_Pred();

        MBAinc = 1; /* first MBAinc denotes absolute position */

        /* Slice macroblocks... */
		for (i=0; i<encparams.mb_width; i++)
		{
            prev_mb = cur_mb;
			cur_mb = &mbinfo[k];

            int suggested_mquant = ratectl.MacroBlockQuant( *cur_mb );
            cur_mb->mquant = suggested_mquant;
			/* Quantize macroblock : N.b. the MB_PATTERN bit may be
               set as a side-effect of this call. */
            cur_mb->Quantize( quantizer);
            
            /* Output mquant and update prediction if it changed in this macroblock */
            if( cur_mb->cbp && suggested_mquant != mquant_pred )
            {
                mquant_pred = suggested_mquant;
                cur_mb->best_me->mb_type |= MB_QUANT;
            }
                
            /* Check to see if Macroblock is skippable, this may set
               the MB_FORWARD bit... */
            bool slice_begin_or_end = (i==0 || i==encparams.mb_width-1);
            cur_mb->SkippedCoding(slice_begin_or_end);
            if( cur_mb->skipped )
            {
                ++MBAinc;
            }
            else
            {
                coding->PutAddrInc(MBAinc); /* macroblock_address_increment */
                MBAinc = 1;
                
                coding->PutMBType(pict_type,cur_mb->best_me->mb_type); /* macroblock type */

                if ( (cur_mb->best_me->mb_type & (MB_FORWARD|MB_BACKWARD)) && !frame_pred_dct)
                    coding->PutBits(cur_mb->best_me->motion_type,2);

                if (pict_struct==FRAME_PICTURE 	&& cur_mb->cbp && !frame_pred_dct)
                    coding->PutBits(cur_mb->field_dct,1);

                if (cur_mb->best_me->mb_type & MB_QUANT)
                {
                    coding->PutBits(q_scale_type 
                            ? map_non_linear_mquant[cur_mb->mquant]
                            : cur_mb->mquant>>1,5);
                }


                if (cur_mb->best_me->mb_type & MB_FORWARD)
                {
                    /* forward motion vectors, update predictors */
                    PutMVs( *cur_mb->best_me, false );
                }

                if (cur_mb->best_me->mb_type & MB_BACKWARD)
                {
                    /* backward motion vectors, update predictors */
                    PutMVs( *cur_mb->best_me,  true );
                }

                if (cur_mb->best_me->mb_type & MB_PATTERN)
                {
                    coding->PutCPB((cur_mb->cbp >> (BLOCK_COUNT-6)) & 63);
                }
            
                /* Output VLC DCT Blocks for Macroblock */

                cur_mb->PutBlocks( );
                /* reset predictors */
                if (!(cur_mb->best_me->mb_type & MB_INTRA))
                    Reset_DC_DCT_Pred();

                if (cur_mb->best_me->mb_type & MB_INTRA || 
                    (pict_type==P_TYPE && !(cur_mb->best_me->mb_type & MB_FORWARD)))
                {
                    Reset_MV_Pred();
                }
            }
            ++k;
        } /* Slice MB loop */
    } /* Slice loop */
	int padding_needed;
    bool recoding_suggested;
    ratectl.UpdatePict( *this, padding_needed);
    coding->AlignBits();
    if( padding_needed > 0 )
    {
        mjpeg_debug( "Padding coded picture to size: %d extra bytes", 
                     padding_needed );
        for( i = 0; i < padding_needed; ++i )
        {
            coding->PutBits(0, 8);
        }
    }
    
    /* Handle splitting of output stream into sequences of desired size */
    if( end_seq )
    {
        coding->PutSeqEnd();
    }
    
}



int Picture::SizeCodedMacroBlocks() const
{ 
    return coding->ByteCount() * 8; 
}

double Picture::IntraCodedBlocks() const
{ 
    vector<MacroBlock>::const_iterator mbi = mbinfo.begin();
    int intra = 0;
    for( mbi = mbinfo.begin(); mbi < mbinfo.end(); ++mbi)
    {
        if( mbi->best_me->mb_type&MB_INTRA )
            ++intra;
    }
    return static_cast<double>(intra) / mbinfo.size();
}

/* *********************
 *
 * Commit   -   Commit to the current encoding of the frame
 * flush the coder buffer content to the elementary stream output.
 *
 * *********************/
 
 void Picture::CommitCoding()
 {
    coding->FlushBuffer();
 }

/* *********************
 *
 * DiscardCoding   -   Discard the current encoding of the frame
 * set coder buffer empty discarding current contents.
 *
 * *********************/
 
 void Picture::DiscardCoding()
 {
    coding->ResetBuffer();
 }

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
