/* mpeg2encoptions.cc - Encoder control parameter class   */

/* (C) 2000/2001/2003 Andrew Stevens, Rainer Johanni */

/* This software is free software; you can redistribute it
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
#include "format_codes.h"
#include "mpegconsts.h"
#include "mpeg2encoder.hh"

MPEG2EncOptions::MPEG2EncOptions()
{
    // Parameters initialised to -1 indicate a format-dependent
    // or stream inferred default.
    format = MPEG_FORMAT_MPEG1;
    bitrate    = 0;
    nonvid_bitrate = 0;
    quant      = 0;
    searchrad  = 16;
    mpeg       = 1;
    aspect_ratio = 0;
    frame_rate  = 0;
    fieldenc   = -1; /* 0: progressive, 1 = frame pictures,
                        interlace frames with field MC and DCT
                        in picture 2 = field pictures
                     */
    norm       = 0;  /* 'n': NTSC, 'p': PAL, 's': SECAM, else unspecified */
    me44_red	= 2;
    me22_red	= 3;	
    hf_quant = 0;
    hf_q_boost = 0.0;
    act_boost = 0.0;
    boost_var_ceil = 10*10;
    video_buffer_size = 0;
    seq_length_limit = 0;
    min_GOP_size = -1;
    max_GOP_size = -1;
    closed_GOPs = 0;
    preserve_B = 0;
    Bgrp_size = 1;
#ifdef _SC_NPROCESSORS_ONLN
    num_cpus = sysconf (_SC_NPROCESSORS_ONLN);
#else
    num_cpus = 1;
#endif
    if (num_cpus < 0)
      num_cpus = 1;
    if (num_cpus > 32)
      num_cpus = 32;
    vid32_pulldown = 0;
    svcd_scan_data = -1;
    seq_hdr_every_gop = 0;
    seq_end_every_gop = 0;
    still_size = 0;
    pad_stills_to_vbv_buffer_size = 0;
    vbv_buffer_still_size = 0;
    force_interlacing = Y4M_UNKNOWN;
    input_interlacing = Y4M_UNKNOWN;
    hack_svcd_hds_bug = 1;
    hack_altscan_bug = 0;
    mpeg2_dc_prec = 1;
    ignore_constraints = 0;
    unit_coeff_elim = 0;
	verbose = 1;
};




static int infer_mpeg1_aspect_code( char norm, mpeg_aspect_code_t mpeg2_code )
{
	switch( mpeg2_code )
	{
	case 1 :					/* 1:1 */
		return 1;
	case 2 :					/* 4:3 */
		if( norm == 'p' || norm == 's' )
			return 8;
	    else if( norm == 'n' )
			return 12;
		else 
			return 0;
	case 3 :					/* 16:9 */
		if( norm == 'p' || norm == 's' )
			return 3;
	    else if( norm == 'n' )
			return 6;
		else
			return 0;
	default :
		return 0;				/* Unknown */
	}
}

 
int MPEG2EncOptions::InferStreamDataParams( const MPEG2EncInVidParams &strm)
{
	int nerr = 0;


	/* Infer norm, aspect ratios and frame_rate if not specified */
	if( frame_rate == 0 )
	{
		if(strm.frame_rate_code<1 || strm.frame_rate_code>8)
		{
			mjpeg_error("Input stream with unknown frame-rate and no frame-rate specified with -a!");
			++nerr;
		}
		else
			frame_rate = strm.frame_rate_code;
	}

	if( norm == 0 && (strm.frame_rate_code==3 || strm.frame_rate_code == 2) )
	{
		mjpeg_info("Assuming norm PAL");
		norm = 'p';
	}
	if( norm == 0 && (strm.frame_rate_code==4 || strm.frame_rate_code == 1) )
	{
		mjpeg_info("Assuming norm NTSC");
		norm = 'n';
	}





	if( frame_rate != 0 )
	{
		if( strm.frame_rate_code != frame_rate && 
            mpeg_valid_framerate_code(strm.frame_rate_code) )
		{
			mjpeg_warn( "Specified display frame-rate %3.2f will over-ride", 
						Y4M_RATIO_DBL(mpeg_framerate(frame_rate)));
			mjpeg_warn( "(different!) frame-rate %3.2f of the input stream",
						Y4M_RATIO_DBL(mpeg_framerate(strm.frame_rate_code)));
		}
	}

	if( aspect_ratio == 0 )
	{
		aspect_ratio = strm.aspect_ratio_code;
	}

	if( aspect_ratio == 0 )
	{
		mjpeg_warn( "No aspect ratio specifed and no guess possible: assuming 4:3 display aspect!");
		aspect_ratio = 2;
	}

	/* Convert to MPEG1 coding if we're generating MPEG1 */
	if( mpeg == 1 )
	{
		aspect_ratio = infer_mpeg1_aspect_code( norm, aspect_ratio );
	}

    input_interlacing = strm.interlacing_code;
    if (input_interlacing == Y4M_UNKNOWN) {
        mjpeg_warn("Unknown input interlacing; assuming progressive.");
        input_interlacing = Y4M_ILACE_NONE;
    }

    /* 'fieldenc' is dependent on input stream interlacing:
         a) Interlaced streams are subsampled _per_field_;
             progressive streams are subsampled over whole frame.
         b) 'fieldenc' sets/clears the MPEG2 'progressive-frame' flag,
            which tells decoder how subsampling was performed.
    */
    if (fieldenc == -1) {
        /* not set yet... so set fieldenc from input interlacing */
        switch (input_interlacing) {
        case Y4M_ILACE_TOP_FIRST:
        case Y4M_ILACE_BOTTOM_FIRST:
            fieldenc = 1; /* interlaced frames */
            mjpeg_info("Interlaced input - selecting interlaced encoding.");
            break;
        case Y4M_ILACE_NONE:
            fieldenc = 0; /* progressive frames */
            mjpeg_info("Progressive input - selecting progressive encoding.");
            break;
        default:
            mjpeg_warn("Unknown input interlacing; assuming progressive.");
            fieldenc = 0;
            break;
        }
    } else {
        /* fieldenc has been set already... so double-check for user */
        switch (input_interlacing) {
        case Y4M_ILACE_TOP_FIRST:
        case Y4M_ILACE_BOTTOM_FIRST:
            if (fieldenc == 0) {
                mjpeg_warn("Progressive encoding selected with interlaced input!");
                mjpeg_warn("  (This will damage the chroma channels.)");
            }
            break;
        case Y4M_ILACE_NONE:
            if (fieldenc != 0) {
                mjpeg_warn("Interlaced encoding selected with progressive input!");
                mjpeg_warn("  (This will damage the chroma channels.)");
            }
            break;
        }
    }

	return nerr;
}

int MPEG2EncOptions::CheckBasicConstraints()
{
	int nerr = 0;
	if( vid32_pulldown )
	{
		if( mpeg == 1 )
			mjpeg_error_exit1( "MPEG-1 cannot encode 3:2 pulldown (for transcoding to VCD set 24fps)!" );

		if( frame_rate != 4 && frame_rate != 5  )
		{
			if( frame_rate == 1 || frame_rate == 2 )
			{
				frame_rate += 3;
				mjpeg_warn("3:2 movie pulldown with frame rate set to decode rate not display rate");
				mjpeg_warn("3:2 Setting frame rate code to display rate = %d (%2.3f fps)", 
						   frame_rate,
						   Y4M_RATIO_DBL(mpeg_framerate(frame_rate)));

			}
			else
			{
				mjpeg_error( "3:2 movie pulldown not sensible for %2.3f fps dispay rate",
							Y4M_RATIO_DBL(mpeg_framerate(frame_rate)));
				++nerr;
			}
		}
		if( fieldenc == 2 )
		{
			mjpeg_error( "3:2 pulldown only possible for frame pictures (-I 1 or -I 0)");
			++nerr;
		}
	}
	
    if ((mpeg == 1) && (fieldenc != 0)) {
        mjpeg_error("Interlaced encoding (-I != 0) is not supported by MPEG-1.");
        ++nerr;
    }


	if(  !mpeg_valid_aspect_code(mpeg, aspect_ratio) )
	{
		mjpeg_error("For MPEG-%d, aspect ratio code  %d is illegal", 
					mpeg, aspect_ratio);
		++nerr;
	}
		


	if( min_GOP_size > max_GOP_size )
	{
		mjpeg_error( "Min GOP size must be <= Max GOP size" );
		++nerr;
	}

    if( preserve_B && Bgrp_size == 0 )
    {
		mjpeg_error_exit1("Preserving I/P frame spacing is impossible for still encoding" );
    }

	if( preserve_B && 
		( min_GOP_size % Bgrp_size != 0 ||
		  max_GOP_size % Bgrp_size != 0 )
		)
	{
		mjpeg_error("Preserving I/P frame spacing is impossible if min and max GOP sizes are" );
		mjpeg_error_exit1("Not both divisible by %d", Bgrp_size );
	}

	switch( format )
	{
	case MPEG_FORMAT_SVCD_STILL :
	case MPEG_FORMAT_SVCD_NSR :
	case MPEG_FORMAT_SVCD : 
		if( aspect_ratio != 2 && aspect_ratio != 3 )
			mjpeg_error_exit1("SVCD only supports 4:3 and 16:9 aspect ratios");
		if( svcd_scan_data )
		{
			mjpeg_warn( "Generating dummy SVCD scan-data offsets to be filled in by \"vcdimager\"");
			mjpeg_warn( "If you're not using vcdimager you may wish to turn this off using -d");
		}
		break;
	}
	return nerr;
}

bool MPEG2EncOptions::SetFormatPresets( const MPEG2EncInVidParams &strm )
{
    in_img_width = strm.horizontal_size;
    in_img_height = strm.vertical_size;
	switch( format  )
	{
	case MPEG_FORMAT_MPEG1 :  /* Generic MPEG1 */
		mjpeg_info( "Selecting generic MPEG1 output profile");
		if( video_buffer_size == 0 )
			video_buffer_size = 46;
		if( bitrate == 0 )
			bitrate = 1151929;
		break;

	case MPEG_FORMAT_VCD :
		mpeg = 1;
		bitrate = 1151929;
		video_buffer_size = 46;
        preserve_B = true;
        Bgrp_size = 3;
        min_GOP_size = 9;
		max_GOP_size = norm == 'n' ? 18 : 15;
		mjpeg_info("VCD default options selected");
		
	case MPEG_FORMAT_VCD_NSR : /* VCD format, non-standard rate */
		mjpeg_info( "Selecting VCD output profile");
		mpeg = 1;
		svcd_scan_data = 0;
		seq_hdr_every_gop = 1;
		if( bitrate == 0 )
			bitrate = 1151929;
		if( video_buffer_size == 0 )
			video_buffer_size = 46 * bitrate / 1151929;
        if( seq_length_limit == 0 )
            seq_length_limit = 700;
        if( nonvid_bitrate == 0 )
            nonvid_bitrate = 230;
		break;
		
	case  MPEG_FORMAT_MPEG2 : 
		mpeg = 2;
		mjpeg_info( "Selecting generic MPEG2 output profile");
		mpeg = 2;
		if( video_buffer_size == 0 )
			video_buffer_size = 230;
		break;

	case MPEG_FORMAT_SVCD :
		mjpeg_info("SVCD standard settings selected");
		bitrate = 2500000;
		max_GOP_size = norm == 'n' ? 18 : 15;
		video_buffer_size = 230;

	case  MPEG_FORMAT_SVCD_NSR :		/* Non-standard data-rate */
		mjpeg_info( "Selecting SVCD output profile");
		mpeg = 2;
		if( quant == 0 )
			quant = 8;
		if( svcd_scan_data == -1 )
			svcd_scan_data = 1;
		if( min_GOP_size == -1 )
            min_GOP_size = 9;
        seq_hdr_every_gop = 1;
        if( seq_length_limit == 0 )
            seq_length_limit = 700;
        if( nonvid_bitrate == 0 )
            nonvid_bitrate = 230;
        break;

	case MPEG_FORMAT_VCD_STILL :
		mjpeg_info( "Selecting VCD Stills output profile");
		mpeg = 1;
		/* We choose a generous nominal bit-rate as its VBR anyway
		   there's only one frame per sequence ;-). It *is* too small
		   to fill the frame-buffer in less than one PAL/NTSC frame
		   period though...*/
		bitrate = 8000000;

		/* Now we select normal/hi-resolution based on the input stream
		   resolution. 
		*/
		
		if( in_img_width == 352 && 
			(in_img_height == 240 || in_img_height == 288 ) )
		{
			/* VCD normal resolution still */
			if( still_size == 0 )
				still_size = 30*1024;
			if( still_size < 20*1024 || still_size > 42*1024 )
			{
				mjpeg_error_exit1( "VCD normal-resolution stills must be >= 20KB and <= 42KB each");
			}
			/* VBV delay encoded normally */
			vbv_buffer_still_size = 46*1024;
			video_buffer_size = 46;
			pad_stills_to_vbv_buffer_size = 0;
		}
		else if( in_img_width == 704 &&
				 (in_img_height == 480 || in_img_height == 576) )
		{
			/* VCD high-resolution stills: only these use vbv_delay
			 to encode picture size...
			*/
			if( still_size == 0 )
				still_size = 125*1024;
			if( still_size < 46*1024 || still_size > 220*1024 )
			{
				mjpeg_error_exit1( "VCD normal-resolution stills should be >= 46KB and <= 220KB each");
			}
			vbv_buffer_still_size = still_size;
			video_buffer_size = 224;
			pad_stills_to_vbv_buffer_size = 1;			
		}
		else
		{
			mjpeg_error("VCD normal resolution stills must be 352x288 (PAL) or 352x240 (NTSC)");
			mjpeg_error_exit1( "VCD high resolution stills must be 704x576 (PAL) or 704x480 (NTSC)");
		}
		quant = 0;		/* We want to try and hit our size target */
		
		seq_hdr_every_gop = 1;
		seq_end_every_gop = 1;
		min_GOP_size = 1;
		max_GOP_size = 1;
		break;

	case MPEG_FORMAT_SVCD_STILL :
		mjpeg_info( "Selecting SVCD Stills output profile");
		mpeg = 2;
		/* We choose a generous nominal bit-rate as its VBR anyway
		   there's only one frame per sequence ;-). It *is* too small
		   to fill the frame-buffer in less than one PAL/NTSC frame
		   period though...*/

		bitrate = 2500000;
		video_buffer_size = 230;
		vbv_buffer_still_size = 220*1024;
		pad_stills_to_vbv_buffer_size = 0;

		/* Now we select normal/hi-resolution based on the input stream
		   resolution. 
		*/
		
		if( in_img_width == 480 && 
			(in_img_height == 480 || in_img_height == 576 ) )
		{
			mjpeg_info( "SVCD normal-resolution stills selected." );
			if( still_size == 0 )
				still_size = 90*1024;
		}
		else if( in_img_width == 704 &&
				 (in_img_height == 480 || in_img_height == 576) )
		{
			mjpeg_info( "SVCD high-resolution stills selected." );
			if( still_size == 0 )
				still_size = 125*1024;
		}
		else
		{
			mjpeg_error("SVCD normal resolution stills must be 480x576 (PAL) or 480x480 (NTSC)");
			mjpeg_error_exit1( "SVCD high resolution stills must be 704x576 (PAL) or 704x480 (NTSC)");
		}

		if( still_size < 30*1024 || still_size > 200*1024 )
		{
			mjpeg_error_exit1( "SVCD resolution stills must be >= 30KB and <= 200KB each");
		}


		seq_hdr_every_gop = 1;
		seq_end_every_gop = 1;
		min_GOP_size = 1;
		max_GOP_size = 1;
		break;


	case MPEG_FORMAT_DVD :
	case MPEG_FORMAT_DVD_NAV :
		mjpeg_info( "Selecting DVD output profile");
		
		if( bitrate == 0 )
			bitrate = 7500000;
        if( video_buffer_size == 0 )
            video_buffer_size = 230;
		mpeg = 2;
		if( quant == 0 )
			quant = 8;
		seq_hdr_every_gop = 1;
		break;
	}

    switch( mpeg )
    {
    case 1 :
        if( min_GOP_size == -1 )
            min_GOP_size = 12;
        if( max_GOP_size == -1 )
            max_GOP_size = 12;
        break;
    case 2:
        if( min_GOP_size == -1 )
            min_GOP_size = 9;
        if( max_GOP_size == -1 )
            max_GOP_size = (norm == 'n' ? 18 : 15);
        break;
    }
	if( svcd_scan_data == -1 )
		svcd_scan_data = 0;

	int nerr = 0;
	nerr += InferStreamDataParams(strm);
	nerr += CheckBasicConstraints();

	return nerr != 0;
    
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
