/* puthdr.c, generation of headers                                          */

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
#include <math.h>
#include "tables.h"
#include "mpeg2coder.hh"
#include "elemstrmwriter.hh"
#include "mpeg2encoder.hh"


MPEG2Coder::MPEG2Coder( MPEG2Encoder &_encoder ) :
    encoder(_encoder),
    encparams( encoder.parms ),
    writer( encoder.writer )
{
}

/* convert frame number to time_code
 *
 * drop_frame not implemented
 */
int MPEG2Coder::FrameToTimeCode(int gop_timecode0_frame)
{
	int frame = gop_timecode0_frame;
	int fps, pict, sec, minute, hour, tc;

	/* Note: no drop_frame_flag support here, so we're simply rounding
	   the frame rate as per 6.3.8 13818-2
	*/
	fps = (int)(encparams.frame_rate+0.5);
	pict = frame%fps;
	frame = (frame-pict)/fps;
	sec = frame%60;
	frame = (frame-sec)/60;
	minute = frame%60;
	frame = (frame-minute)/60;
	hour = frame%24;
	tc = (hour<<19) | (minute<<13) | (1<<12) | (sec<<6) | pict;
	return tc;
}


/****************
 *
 * generate sequence header (6.2.2.1, 6.3.3)
 *
 ***************/

void MPEG2Coder::PutSeqHdr()
{
	int i;

	writer.AlignBits();
	writer.PutBits(SEQ_START_CODE,32); /* sequence_header_code */
	writer.PutBits(encparams.horizontal_size,12); /* horizontal_size_value */
	writer.PutBits(encparams.vertical_size,12); /* vertical_size_value */
	writer.PutBits(encparams.aspectratio,4); /* aspect_ratio_information */
	writer.PutBits(encparams.frame_rate_code,4); /* frame_rate_code */

	/* MPEG-1 VBR is FFFF rate code. 
	   MPEG-2 VBR is a matter of mux-ing.  The ceiling bit_rate is always
	   sent 
	*/
	if(encparams.mpeg1 && (encparams.quant_floor != 0 || encparams.still_size > 0) ) {
		writer.PutBits(0xfffff,18);
	} else {
		writer.PutBits((int)ceil(encparams.bit_rate/400.0),18); /* bit_rate_value */
	}
	writer.PutBits(1,1); /* marker_bit */
	writer.PutBits(encparams.vbv_buffer_code,10); /* vbv_buffer_size_value */
	writer.PutBits(encparams.constrparms,1); /* constrained_parameters_flag */

	writer.PutBits(encparams.load_iquant,1); /* load_intra_quantizer_matrix */
	if (encparams.load_iquant)
		for (i=0; i<64; i++)  /* matrices are always downloaded in zig-zag order */
			writer.PutBits(encparams.intra_q[zig_zag_scan[i]],8); /* intra_quantizer_matrix */

	writer.PutBits(encparams.load_niquant,1); /* load_non_intra_quantizer_matrix */
	if (encparams.load_niquant)
		for (i=0; i<64; i++)
			writer.PutBits(encparams.inter_q[zig_zag_scan[i]],8); /* non_intra_quantizer_matrix */
	if (!encparams.mpeg1)
	{
		PutSeqExt();
		PutSeqDispExt();
	}

}

/**************************
 *
 * generate sequence extension (6.2.2.3, 6.3.5) header (MPEG-2 only)
 *
 *************************/

void MPEG2Coder::PutSeqExt()
{
	writer.AlignBits();
	writer.PutBits(EXT_START_CODE,32); /* extension_start_code */
	writer.PutBits(SEQ_ID,4); /* extension_start_code_identifier */
	writer.PutBits((encparams.profile<<4)|encparams.level,8); /* profile_and_level_indication */
	writer.PutBits(encparams.prog_seq,1); /* progressive sequence */
	writer.PutBits(CHROMA420,2); /* chroma_format */
	writer.PutBits(encparams.horizontal_size>>12,2); /* horizontal_size_extension */
	writer.PutBits(encparams.vertical_size>>12,2); /* vertical_size_extension */
	writer.PutBits(((int)ceil(encparams.bit_rate/400.0))>>18,12); /* bit_rate_extension */
	writer.PutBits(1,1); /* marker_bit */
	writer.PutBits(encparams.vbv_buffer_code>>10,8); /* vbv_buffer_size_extension */
	writer.PutBits(0,1); /* low_delay  -- currently not implemented */
	writer.PutBits(0,2); /* frame_rate_extension_n */
	writer.PutBits(0,5); /* frame_rate_extension_d */
}

/*****************************
 * 
 * generate sequence display extension (6.2.2.4, 6.3.6)
 *
 ****************************/

void MPEG2Coder::PutSeqDispExt()
{
	writer.AlignBits();
	writer.PutBits(EXT_START_CODE,32); /* extension_start_code */
	writer.PutBits(DISP_ID,4); /* extension_start_code_identifier */
	writer.PutBits(encparams.video_format,3); /* video_format */
	writer.PutBits(1,1); /* colour_description */
	writer.PutBits(encparams.color_primaries,8); /* colour_primaries */
	writer.PutBits(encparams.transfer_characteristics,8); /* transfer_characteristics */
	writer.PutBits(encparams.matrix_coefficients,8); /* matrix_coefficients */
	writer.PutBits(encparams.display_horizontal_size,14); /* display_horizontal_size */
	writer.PutBits(1,1); /* marker_bit */
	writer.PutBits(encparams.display_vertical_size,14); /* display_vertical_size */
}

/********************************
 *
 * Output user data (6.2.2.2.2, 6.3.4.1)
 *
 * TODO: string must not embed start codes 0x00 0x00 0x00 0xXX
 *
 *******************************/

void MPEG2Coder::PutUserData(const uint8_t *userdata, int len)
{
	int i;
	writer.AlignBits();
	writer.PutBits(USER_START_CODE,32); /* user_data_start_code */
	for( i =0; i < len; ++i )
		writer.PutBits(userdata[i],8);
}

/* generate group of pictures header (6.2.2.6, 6.3.9)
 *
 * uses tc0 (timecode of first frame) and frame0 (number of first frame)
 */
void MPEG2Coder::PutGopHdr(int frame,int closed_gop )
{
	int tc;

	writer.AlignBits();
	writer.PutBits(GOP_START_CODE,32); /* group_start_code */
	tc = FrameToTimeCode(frame);
	writer.PutBits(tc,25); /* time_code */
	writer.PutBits(closed_gop,1); /* closed_gop */
	writer.PutBits(0,1); /* broken_link */
}


/* generate sequence_end_code (6.2.2) */
void MPEG2Coder::PutSeqEnd(void)
{
	writer.AlignBits();
	writer.PutBits(SEQ_END_CODE,32);
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
