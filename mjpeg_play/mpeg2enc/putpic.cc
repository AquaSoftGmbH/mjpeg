/* putpic.c, block and motion vector encoding routines                      */

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
#include <cassert>
#include "mpeg2syntaxcodes.h"
#include "tables.h"
#include "simd.h"
#include "mpeg2encoder.hh"
#include "mpeg2coder.hh"
#include "ratectl.hh"
#include "macroblock.hh"
#include "picture.hh"

#ifdef DEBUG_DPME
static int dp_mv = 0;
void calc_DMV( const Picture &picture, /*int pict_struct,  int topfirst,*/
                      MotionVector DMV[Parity::dim],
                      MotionVector &dmvector, 
                      int mvx, int mvy
    );
#endif
    
/* output motion vectors (6.2.5.2, 6.3.16.2)
 *
 * this routine also updates the predictions for motion vectors (PMV)
 */
void Picture::PutMVs( MotionEst &me, bool back )

{
	int hor_f_code;
	int vert_f_code;

	if( back )
	{
		hor_f_code = back_hor_f_code;
		vert_f_code = back_vert_f_code;
	}
	else
	{
		hor_f_code = forw_hor_f_code;
		vert_f_code = forw_vert_f_code;
	}

	if (pict_struct==FRAME_PICTURE)
	{
		if (me.motion_type==MC_FRAME)
		{
			/* frame prediction */
			coder.PutMV(me.MV[0][back][0]-PMV[0][back][0],hor_f_code);
			coder.PutMV(me.MV[0][back][1]-PMV[0][back][1],vert_f_code);
			PMV[0][back][0]=PMV[1][back][0]=me.MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=me.MV[0][back][1];
		}
		else if (me.motion_type==MC_FIELD)
		{
			/* field prediction */

			coder.PutBits(me.field_sel[0][back],1);
			coder.PutMV(me.MV[0][back][0]-PMV[0][back][0],hor_f_code);
			coder.PutMV((me.MV[0][back][1]>>1)-(PMV[0][back][1]>>1),vert_f_code);
			coder.PutBits(me.field_sel[1][back],1);
			coder.PutMV(me.MV[1][back][0]-PMV[1][back][0],hor_f_code);
			coder.PutMV((me.MV[1][back][1]>>1)-(PMV[1][back][1]>>1),vert_f_code);
			PMV[0][back][0]=me.MV[0][back][0];
			PMV[0][back][1]=me.MV[0][back][1];
			PMV[1][back][0]=me.MV[1][back][0];
			PMV[1][back][1]=me.MV[1][back][1];

		}
		else
		{
#ifdef DEBUG_DPME
            MotionVector DMV[Parity::dim /*pred*/];
                        calc_DMV(*this,
                         DMV,
                         me.dualprimeMV,
                         me.MV[0][0][0],
                         me.MV[0][0][1]>>1);
                         
            printf( "PR%06d: %03d %03d %03d %03d %03d %03d\n", dp_mv,
                me.MV[0][0][0], (me.MV[0][0][1]>>1), DMV[0][0], DMV[0][1], DMV[1][0], DMV[1][1] );
            ++dp_mv;
            if( dp_mv == 45000 )
                exit(0);
#endif
			/* dual prime prediction */
			coder.PutMV(me.MV[0][back][0]-PMV[0][back][0],hor_f_code);
			coder.PutDMV(me.dualprimeMV[0]);
			coder.PutMV((me.MV[0][back][1]>>1)-(PMV[0][back][1]>>1),vert_f_code);
			coder.PutDMV(me.dualprimeMV[1]);
			PMV[0][back][0]=PMV[1][back][0]=me.MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=me.MV[0][back][1];
		}
	}
	else
	{
		/* field picture */
		if (me.motion_type==MC_FIELD)
		{
			/* field prediction */
			coder.PutBits(me.field_sel[0][back],1);
			coder.PutMV(me.MV[0][back][0]-PMV[0][back][0],hor_f_code);
			coder.PutMV(me.MV[0][back][1]-PMV[0][back][1],vert_f_code);
			PMV[0][back][0]=PMV[1][back][0]=me.MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=me.MV[0][back][1];
		}
		else if (me.motion_type==MC_16X8)
		{
			/* 16x8 prediction */
			coder.PutBits(me.field_sel[0][back],1);
			coder.PutMV(me.MV[0][back][0]-PMV[0][back][0],hor_f_code);
			coder.PutMV(me.MV[0][back][1]-PMV[0][back][1],vert_f_code);
			coder.PutBits(me.field_sel[1][back],1);
			coder.PutMV(me.MV[1][back][0]-PMV[1][back][0],hor_f_code);
			coder.PutMV(me.MV[1][back][1]-PMV[1][back][1],vert_f_code);
			PMV[0][back][0]=me.MV[0][back][0];
			PMV[0][back][1]=me.MV[0][back][1];
			PMV[1][back][0]=me.MV[1][back][0];
			PMV[1][back][1]=me.MV[1][back][1];
		}
		else
		{
			/* dual prime prediction */
			coder.PutMV(me.MV[0][back][0]-PMV[0][back][0],hor_f_code);
			coder.PutDMV(me.dualprimeMV[0]);
			coder.PutMV(me.MV[0][back][1]-PMV[0][back][1],vert_f_code);
			coder.PutDMV(me.dualprimeMV[1]);
			PMV[0][back][0]=PMV[1][back][0]=me.MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=me.MV[0][back][1];
		}
	}
}

void MacroBlock::PutBlocks( )
{
    int comp;
    int cc;
    for (comp=0; comp<BLOCK_COUNT; comp++)
    {
        /* block loop */
        if( cbp & (1<<(BLOCK_COUNT-1-comp)))
        {
            if (final_me.mb_type & MB_INTRA)
            {
                // TODO: 420 Only?
                cc = (comp<4) ? 0 : (comp&1)+1;
                picture->coder.PutIntraBlk(picture, qdctblocks[comp],cc);
            }
            else
            {
                picture->coder.PutNonIntraBlk(picture,qdctblocks[comp]);
            }
        }
    }
}

void MacroBlock::SkippedCoding( bool slice_begin_end )
{
    skipped = false;
    if( slice_begin_end || cbp )
    {
        /* there's no VLC for 'No MC, Not Coded':
         * we have to transmit (0,0) motion vectors
         */
        if (picture->pict_type==P_TYPE && !cbp)
            final_me.mb_type|= MB_FORWARD;
        return;
    }

    MacroBlock *prev_mb = picture->prev_mb;
    /* P picture, no motion vectors -> skip */
    if (picture->pict_type==P_TYPE && !(final_me.mb_type&MB_FORWARD))
    {
        /* reset predictors */
        picture->Reset_DC_DCT_Pred();
        picture->Reset_MV_Pred();
        skipped = true;
        return;
    }
    
    if(picture->pict_type==B_TYPE )
    {
        /* B frame picture with same prediction type
         * (forward/backward/interp.)  and same active vectors
         * as in previous macroblock -> skip
         */

        if (  picture->pict_struct==FRAME_PICTURE
              && final_me.motion_type==MC_FRAME
              && ((prev_mb->final_me.mb_type ^ final_me.mb_type) &(MB_FORWARD|MB_BACKWARD))==0
              && (!(final_me.mb_type&MB_FORWARD) ||
                  (picture->PMV[0][0][0]==final_me.MV[0][0][0] &&
                   picture->PMV[0][0][1]==final_me.MV[0][0][1]))
              && (!(final_me.mb_type&MB_BACKWARD) ||
                  (picture->PMV[0][1][0]==final_me.MV[0][1][0] &&
                   picture->PMV[0][1][1]==final_me.MV[0][1][1])))
        {
            skipped = true;
            return;
        }

        /* B field picture macroblock with same prediction
         * type (forward/backward/interp.) and active
         * vectors as previous macroblock and same
         * vertical field selects as current field -> skio
         */

        if (picture->pict_struct!=FRAME_PICTURE
            && final_me.motion_type==MC_FIELD
            && ((prev_mb->final_me.mb_type^final_me.mb_type)&(MB_FORWARD|MB_BACKWARD))==0
            && (!(final_me.mb_type&MB_FORWARD) ||
                (picture->PMV[0][0][0]==final_me.MV[0][0][0] &&
                 picture->PMV[0][0][1]==final_me.MV[0][0][1] &&
                 final_me.field_sel[0][0]==(picture->pict_struct==BOTTOM_FIELD)))
            && (!(final_me.mb_type&MB_BACKWARD) ||
                (picture->PMV[0][1][0]==final_me.MV[0][1][0] &&
                 picture->PMV[0][1][1]==final_me.MV[0][1][1] &&
                 final_me.field_sel[0][1]==(picture->pict_struct==BOTTOM_FIELD))))
        {
            skipped = true;
            return;
        }
    }

}


/* generate picture header (6.2.3, 6.3.10) */
void Picture::PutHeader()
{
	assert( coder.Aligned() );
	coder.PutBits(PICTURE_START_CODE,32); /* picture_start_code */
	coder.PutBits(temp_ref,10); /* temporal_reference */
	coder.PutBits(pict_type,3); /* picture_coding_type */
	coder.PutBits(vbv_delay,16); /* vbv_delay */

	if (pict_type==P_TYPE || pict_type==B_TYPE)
	{
		coder.PutBits(0,1); /* full_pel_forward_vector */
		if (encparams.mpeg1)
			coder.PutBits(forw_hor_f_code,3);
		else
			coder.PutBits(7,3); /* forward_f_code */
	}

	if (pict_type==B_TYPE)
	{
		coder.PutBits(0,1); /* full_pel_backward_vector */
		if (encparams.mpeg1)
			coder.PutBits(back_hor_f_code,3);
		else
			coder.PutBits(7,3); /* backward_f_code */
	}
	coder.PutBits(0,1); /* extra_bit_picture */
    coder.AlignBits();
	if ( !encparams.mpeg1 )
	{
		PutCodingExt();
	}

}

/* generate picture coding extension (6.2.3.1, 6.3.11)
 *
 * composite display information (v_axis etc.) not implemented
 */
void Picture::PutCodingExt()
{
	assert( coder.Aligned() );
	coder.PutBits(EXT_START_CODE,32); /* extension_start_code */
	coder.PutBits(CODING_ID,4); /* extension_start_code_identifier */
	coder.PutBits(forw_hor_f_code,4); /* forward_horizontal_f_code */
	coder.PutBits(forw_vert_f_code,4); /* forward_vertical_f_code */
	coder.PutBits(back_hor_f_code,4); /* backward_horizontal_f_code */
	coder.PutBits(back_vert_f_code,4); /* backward_vertical_f_code */
	coder.PutBits(dc_prec,2); /* intra_dc_precision */
	coder.PutBits(pict_struct,2); /* picture_structure */
	coder.PutBits((pict_struct==FRAME_PICTURE)?topfirst : 0, 1); /* top_field_first */
	coder.PutBits(frame_pred_dct,1); /* frame_pred_frame_dct */
	coder.PutBits(0,1); /* concealment_motion_vectors  -- currently not implemented */
	coder.PutBits(q_scale_type,1); /* q_scale_type */
	coder.PutBits(intravlc,1); /* intra_vlc_format */
	coder.PutBits(altscan,1); /* alternate_scan */
	coder.PutBits(repeatfirst,1); /* repeat_first_field */

	coder.PutBits(prog_frame,1); /* chroma_420_type */
	coder.PutBits(prog_frame,1); /* progressive_frame */
	coder.PutBits(0,1); /* composite_display_flag */
    coder.AlignBits();
}


void Picture::PutSliceHdr( int slice_mb_y )
{
    /* slice header (6.2.4) */
    coder.AlignBits();
    
    if (encparams.mpeg1 || encparams.vertical_size<=2800)
        coder.PutBits(SLICE_MIN_START+slice_mb_y,32); /* slice_start_code */
    else
    {
        coder.PutBits(SLICE_MIN_START+(slice_mb_y&127),32); /* slice_start_code */
        coder.PutBits(slice_mb_y>>7,3); /* slice_vertical_position_extension */
    }
    
    /* quantiser_scale_code */
    coder.PutBits(q_scale_type 
            ? map_non_linear_mquant[mquant_pred] 
            : mquant_pred >> 1, 5);
    
    coder.PutBits(0,1); /* extra_bit_slice */
    
} 



/* *****************
 *
 * putpict - Quantise and encode picture with Sequence and GOP headers
 * as required.
 *
 ******************/

void Picture::PutHeadersAndEncoding( RateCtl &ratecontrol )
{

	/* Handle splitting of output stream into sequences of desired size */
	if( new_seq )
	{
		coder.PutSeqEnd();
		ratecontrol.InitSeq(true);
	}
	/* Handle start of GOP stuff:
       We've reach a new GOP so we emit what we coded for the
       previous one as (for the moment) and mark the resulting coder
       state for eventual backup.
       Currently, we never backup more that to the start of the current GOP.
     */
	if( gop_start )
	{
        coder.EmitCoded();
		ratecontrol.InitGOP( np, nb);
	}

    MPEG2CoderState pre_picture_state = coder.CurrentState();
	ratecontrol.CalcVbvDelay(*this);
    ratecontrol.InitNewPict(*this, coder.BitCount()); /* set up rate control */
    bool recoding_suggested = TryEncoding(ratecontrol);
    if( recoding_suggested )
    {
        mjpeg_info( "RECODING!");
        coder.RestoreCodingState( pre_picture_state );
        ratecontrol.InitKnownPict(*this);
        TryEncoding(ratecontrol);
    }
}

/* ************************************************
 *
 * QuantiseAndEncode - Quantise and Encode a picture.
 *
 * NOTE: It may seem perverse to quantise at the same time as
 * coding. However, actually makes (limited) sense: feedback from the
 * *actual* bit-allocation may be used to adjust quantisation "on the
 * fly".  We, of course, need the quantised DCT blocks to construct
 * the reference picture for future motion compensation etc.
 *
 * *********************************************** */

bool Picture::TryEncoding(RateCtl &ratectl)
{
	/* Sequence header if new sequence or we're generating for a
       format like (S)VCD that mandates sequence headers every GOP to
       do fast forward, rewind etc.
	*/

    if( new_seq || decode == 0 ||
        (gop_start && encparams.seq_hdr_every_gop) )
    {
		coder.PutSeqHdr();
    }
	if( gop_start )
	{
		coder.PutGopHdr( decode,  closed_gop );
	}
    
	int i, j, k;
	int MBAinc;
	MacroBlock *cur_mb = 0;

    
	/* picture header and picture coding extension */
    PutHeader();

    /* TODO: This should really be a member of the picture object */
	if( encparams.svcd_scan_data && pict_type == I_TYPE )
	{
		coder.PutUserData( dummy_svcd_scan_data, sizeof(dummy_svcd_scan_data) );
	}

	mquant_pred = ratectl.InitialMacroBlockQuant(*this);

	k = 0;
    
    /* TODO: We're currently hard-wiring each macroblock row as a
       slice.  For MPEG-2 we could do this better and reduce slice
       start code coverhead... */

	for (j=0; j<encparams.mb_height2; j++)
	{

        PutSliceHdr(j);
        Reset_DC_DCT_Pred();
        Reset_MV_Pred();

        MBAinc = 1; /* first MBAinc denotes absolute position */

        /* Slice macroblocks... */
		for (i=0; i<encparams.mb_width; i++)
		{
            prev_mb = cur_mb;
			cur_mb = &mbinfo[k];

			/* determine mquant (rate control) */
            cur_mb->mquant = ratectl.MacroBlockQuant( *cur_mb, coder.BitCount() );

			/* quantize macroblock : N.b. the MB_PATTERN bit may be
               set as a side-effect of this call. */
            cur_mb->Quantize( quantizer);

			/* output mquant if it has changed */
			if (cur_mb->cbp && mquant_pred!=cur_mb->mquant)
				cur_mb->final_me.mb_type|= MB_QUANT;

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
                coder.PutAddrInc(MBAinc); /* macroblock_address_increment */
                MBAinc = 1;
                
                coder.PutMBType(pict_type,cur_mb->final_me.mb_type); /* macroblock type */

                if ( (cur_mb->final_me.mb_type & (MB_FORWARD|MB_BACKWARD)) && !frame_pred_dct)
                    coder.PutBits(cur_mb->final_me.motion_type,2);

                if (pict_struct==FRAME_PICTURE 	&& cur_mb->cbp && !frame_pred_dct)
                    coder.PutBits(cur_mb->field_dct,1);

                if (cur_mb->final_me.mb_type & MB_QUANT)
                {
                    coder.PutBits(q_scale_type 
                            ? map_non_linear_mquant[cur_mb->mquant]
                            : cur_mb->mquant>>1,5);
                    mquant_pred = cur_mb->mquant;
                }


                if (cur_mb->final_me.mb_type & MB_FORWARD)
                {
                    /* forward motion vectors, update predictors */
                    PutMVs( cur_mb->final_me, false );
                }

                if (cur_mb->final_me.mb_type & MB_BACKWARD)
                {
                    /* backward motion vectors, update predictors */
                    PutMVs( cur_mb->final_me,  true );
                }

                if (cur_mb->final_me.mb_type & MB_PATTERN)
                {
                    coder.PutCPB((cur_mb->cbp >> (BLOCK_COUNT-6)) & 63);
                }
            
                /* Output VLC DCT Blocks for Macroblock */

                cur_mb->PutBlocks( );
                /* reset predictors */
                if (!(cur_mb->final_me.mb_type & MB_INTRA))
                    Reset_DC_DCT_Pred();

                if (cur_mb->final_me.mb_type & MB_INTRA || 
                    (pict_type==P_TYPE && !(cur_mb->final_me.mb_type & MB_FORWARD)))
                {
                    Reset_MV_Pred();
                }
            }
            ++k;
        } /* Slice MB loop */
    } /* Slice loop */
    int64_t bitcount_EOP = coder.BitCount();
	int padding_needed;
    bool recoding_suggested;
    ratectl.UpdatePict( *this, bitcount_EOP, 
                        padding_needed, recoding_suggested );
    coder.AlignBits();
    if( padding_needed > 0 )
    {
        mjpeg_debug( "Padding coded picture to size: %d extra bytes", 
                     padding_needed );
        for( i = 0; i < padding_needed; ++i )
        {
            coder.PutBits(0, 8);
        }
    }
    return recoding_suggested;
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
