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
#include "mjpeg_types.h"
#include "mpeg2syntaxcodes.h"
#include "cpu_accel.h"
#include "motionsearch.h"
#include "encoderparams.hh"
#include "mpeg2coder.hh"
#include "quantize.hh"
#include "seqencoder.hh"
#include "tables.h"

Picture::Picture( EncoderParams &_encparams, 
                  MPEG2Coder &_coder, 
                  Quantizer &_quantizer ) :
    encparams( _encparams ),
    coder( _coder ),
    quantizer( _quantizer )
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


	curref = new uint8_t *[5];
	curorg = new uint8_t *[5];
	pred   = new uint8_t *[5];

	for( i = 0 ; i<3; i++)
	{
		int size =  (i==0) ? encparams.lum_buffer_size : encparams.chrom_buffer_size;
		curref[i] = static_cast<uint8_t *>(bufalloc(size));
		curorg[i] = NULL;       // Will point to input frame data buffered by
                                // PictureReader
		pred[i]   = static_cast<uint8_t *>(bufalloc(size));
	}

	/* The (non-existent) previous encoding using an as-yet un-used
	   picture encoding data buffers is "completed"
	*/
	sync_guard_init( &completion, 1 );
}

Picture::~Picture()
{
    int i;
	for( i = 0 ; i<3; i++)
	{
		free( curref[i] );
		free( pred[i] );
	}
    delete curref;
    delete curorg;
    delete pred;
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

void Picture::SetSeqPos(int _decode,int b_index )
{
	decode = _decode;
	dc_prec = encparams.dc_prec;
	secondfield = false;
	ipflag = 0;

		
	/* Handle picture structure... */
	if( encparams.fieldpic )
	{
		pict_struct = encparams.topfirst ? TOP_FIELD : BOTTOM_FIELD;
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


	switch ( pict_type )
	{
	case I_TYPE :
		forw_hor_f_code = 15;
		forw_vert_f_code = 15;
		back_hor_f_code = 15;
		back_vert_f_code = 15;
		sxf = encparams.motion_data[0].sxf;
		syf = encparams.motion_data[0].syf;
		break;
	case P_TYPE :
		forw_hor_f_code = encparams.motion_data[0].forw_hor_f_code;
		forw_vert_f_code = encparams.motion_data[0].forw_vert_f_code;
		back_hor_f_code = 15;
		back_vert_f_code = 15;
		sxf = encparams.motion_data[0].sxf;
		syf = encparams.motion_data[0].syf;
		break;
	case B_TYPE :
		forw_hor_f_code = encparams.motion_data[b_index].forw_hor_f_code;
		forw_vert_f_code = encparams.motion_data[b_index].forw_vert_f_code;
		back_hor_f_code = encparams.motion_data[b_index].back_hor_f_code;
		back_vert_f_code = encparams.motion_data[b_index].back_vert_f_code;
		sxf = encparams.motion_data[b_index].sxf;
		syf = encparams.motion_data[b_index].syf;
		sxb = encparams.motion_data[b_index].sxb;
		syb = encparams.motion_data[b_index].syb;

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
        

#ifdef OUTPUT_STAT
	fprintf(statfile,"\nFrame %d (#%d in display order):\n",decode,display);
	fprintf(statfile," picture_type=%c\n",pict_type_char[pict_type]);
	fprintf(statfile," temporal_reference=%d\n",temp_ref);
	fprintf(statfile," frame_pred_frame_dct=%d\n",frame_pred_dct);
	fprintf(statfile," q_scale_type=%d\n",q_scale_type);
	fprintf(statfile," intra_vlc_format=%d\n",intravlc);
	fprintf(statfile," alternate_scan=%d\n",altscan);

	if (pict_type!=I_TYPE)
	{
		fprintf(statfile," forward search window: %d...%d / %d...%d\n",
				-sxf,sxf,-syf,syf);
		fprintf(statfile," forward vector range: %d...%d.5 / %d...%d.5\n",
				-(4<<forw_hor_f_code),(4<<forw_hor_f_code)-1,
				-(4<<forw_vert_f_code),(4<<forw_vert_f_code)-1);
	}

	if (pict_type==B_TYPE)
	{
		fprintf(statfile," backward search window: %d...%d / %d...%d\n",
				-sxb,sxb,-syb,syb);
		fprintf(statfile," backward vector range: %d...%d.5 / %d...%d.5\n",
				-(4<<back_hor_f_code),(4<<back_hor_f_code)-1,
				-(4<<back_vert_f_code),(4<<back_vert_f_code)-1);
	}
#endif


}

/*
 * Adjust picture parameters for the second field in a pair of field
 * pictures.
 *
 */

void Picture::Set2ndField()
{
	secondfield = true;
    gop_start = false;
	if( pict_struct == TOP_FIELD )
		pict_struct =  BOTTOM_FIELD;
	else
		pict_struct =  TOP_FIELD;
	
	if( pict_type == I_TYPE )
	{
		ipflag = 1;
		pict_type = P_TYPE;
		
		forw_hor_f_code = encparams.motion_data[0].forw_hor_f_code;
		forw_vert_f_code = encparams.motion_data[0].forw_vert_f_code;
		back_hor_f_code = 15;
		back_vert_f_code = 15;
		sxf = encparams.motion_data[0].sxf;
		syf = encparams.motion_data[0].syf;	
	}
}




/* Set the sequencing structure information
   of a picture (type and temporal reference)
   based on the specified sequence state
*/

void Picture::Set_IP_Frame( StreamState *ss, int num_frames )
{
	/* Temp ref of I frame in closed GOP of sequence is 0 We have to
	   be a little careful with the end of stream special-case.
	*/
	if( ss->g == 0 && ss->closed_gop )
	{
		temp_ref = 0;
	}
	else 
	{
		temp_ref = ss->g+(ss->bigrp_length-1);
	}

	if (temp_ref >= (num_frames-ss->gop_start_frame))
		temp_ref = (num_frames-ss->gop_start_frame) - 1;

	present = (ss->i-ss->g)+temp_ref;
	if (ss->g==0) /* first displayed frame in GOP is I */
	{
		pict_type = I_TYPE;
	}
	else 
	{
		pict_type = P_TYPE;
	}

	/* Start of GOP - set GOP data for picture */
	if( ss->g == 0 )
	{
		gop_start = true;
        closed_gop = ss->closed_gop;
		new_seq = ss->new_seq;
		nb = ss->nb;
		np = ss->np;
	}		
	else
	{
		gop_start = false;
        closed_gop = false;
		new_seq = false;
	}
}


void Picture::Set_B_Frame(  StreamState *ss )
{
	temp_ref = ss->g - 1;
	present = ss->i-1;
	pict_type = B_TYPE;
	gop_start = false;
	new_seq = false;
}

void Picture::EncodeMacroBlocks()
{ 
    vector<MacroBlock>::iterator mbi = mbinfo.begin();

	for( mbi = mbinfo.begin(); mbi < mbinfo.end(); ++mbi)
	{
        mbi->MotionEstimate();
        // TODO: Eventually we will allow alternative selectors to be used!
        mbi->SelectCodingModeOnVariance();
        mbi->Predict();
        mbi->Transform();
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

void Picture::ActivityMeasures( double &act_sum, double &var_sum)
{
	int i,j,k,l;
	double actj,sum;
	double varsum;
	int blksum;
	sum = 0.0;
	varsum = 0.0;
	k = 0;
	for (j=0; j<encparams.enc_height2; j+=16)
		for (i=0; i<encparams.enc_width; i+=16)
		{
			/* A.Stevens Jul 2000 Luminance variance *has* to be a
			   rotten measure of how active a block in terms of bits
			   needed to code a lossless DCT.  E.g. a half-white
			   half-black block has a maximal variance but pretty
			   small DCT coefficients.

			   So.... instead of luminance variance as used in the
			   original we use the absolute sum of DCT coefficients as
			   our block activity measure.  */

			varsum += (double)mbinfo[k].final_me.var;
			if( mbinfo[k].final_me.mb_type  & MB_INTRA )
			{
				/* Compensate for the wholly disproprotionate weight
				 of the DC coefficients.  Shold produce more sensible
				 results...  yes... it *is* an mostly empirically derived
				 fudge factor ;-)
				*/
				blksum =  -80*COEFFSUM_SCALE;
				for( l = 0; l < 6; ++l )
					blksum += 
						quantizer.WeightCoeffIntra( mbinfo[k].RawDCTblocks()[l] ) ;
			}
			else
			{
				blksum = 0;
				for( l = 0; l < 6; ++l )
					blksum += 
						quantizer.WeightCoeffInter( mbinfo[k].RawDCTblocks()[l] ) ;
			}
			/* It takes some bits to code even an entirely zero block...
			   It also makes a lot of calculations a lot better conditioned
			   if it can be guaranteed that activity is always distinctly
			   non-zero.
			 */


			actj = (double)blksum / (double)COEFFSUM_SCALE;
			if( actj < 12.0 )
				actj = 12.0;

			mbinfo[k].act = (double)actj;
			sum += (double)actj;
			++k;
		}
	act_sum = sum;
	var_sum = varsum;
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

	psubsample_image( curorg[0], 
					 linestride,
					 curorg[0]+eparams.fsubsample_offset, 
					 curorg[0]+eparams.qsubsample_offset );
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
