/* mpeg2enc.c, main() and parameter file reading                            */

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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif


#define GLOBAL /* used by global.h */

#include "global.h"
#include "simd.h"
#include "motionsearch.h"
#include "format_codes.h"
#include "mpegconsts.h"
#include "fastintfns.h"
#ifdef HAVE_ALTIVEC
/* needed for ALTIVEC_BENCHMARK and print_benchmark_statistics() */
#include "../utils/altivec/altivec_conf.h"
#endif
int verbose = 1;

#include "mpeg2encoder.hh"
#include "picturereader.hh"
#include "elemstrmwriter.hh"
#include "quantize.hh"
#include "ratectl.hh"
#include "seqencoder.hh"
#include "mpeg2coder.hh"


MPEG2Encoder::MPEG2Encoder( int istrm_fd, MPEG2EncOptions &_options ) :
    options( _options ),
    parms( *new EncoderParams ),
    reader( *new Y4MPipeReader( *this, istrm_fd ) ),
    // TODO: Concrete type for writing to fd...
    writer( *new ElemStrmWriter( *this ) ),
    quantizer( *new Quantizer( *this ) ),
    bitrate_controller( *new OnTheFlyRateCtl( *this ) ),
    seqencoder( *new SeqEncoder( *this ) ),
    coder( *new MPEG2Coder( *this ) )
{}

MPEG2Encoder::~MPEG2Encoder()
{
    delete &coder;
    delete &seqencoder;
    delete &bitrate_controller;
    delete &quantizer;
    delete &reader;
    delete &parms;
}
    
    

MPEG2Encoder *enc;


#define MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define MIN(a,b) ( (a)<(b) ? (a) : (b) )

#ifndef O_BINARY
# define O_BINARY 0
#endif





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
    Bgrp_size = 3;
    num_cpus = 1;
    vid32_pulldown = 0;
    svcd_scan_data = -1;
    seq_hdr_every_gop = 0;
    seq_end_every_gop = 0;
    still_size = 0;
    pad_stills_to_vbv_buffer_size = 0;
    vbv_buffer_still_size = 0;
    force_interlacing = Y4M_UNKNOWN;
    input_interlacing = 0;
    hack_svcd_hds_bug = 1;
    hack_altscan_bug = 0;
    mpeg2_dc_prec = 1;
    ignore_constraints = 0;
    unit_coeff_elim = 0;
};


void MPEG2EncOptions::SetFormatPresets( const MPEG2EncInVidParams &strm )
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
		if( fieldenc == -1 )
			fieldenc = 1;
		if( video_buffer_size == 0 )
			video_buffer_size = 46 * bitrate / 1151929;
		break;

	case MPEG_FORMAT_SVCD :
		mjpeg_info("SVCD standard settings selected");
		bitrate = 2500000;
		max_GOP_size = norm == 'n' ? 18 : 15;
		video_buffer_size = 230;

	case  MPEG_FORMAT_SVCD_NSR :		/* Non-standard data-rate */
		mjpeg_info( "Selecting SVCD output profile");
		mpeg = 2;
		if( fieldenc == -1 )
			fieldenc = 1;
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
		if( fieldenc == -1 )
			fieldenc = 1;
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
		if( fieldenc == -1 )
			fieldenc = 1;
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
}

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
			strm.frame_rate_code > 0 && 
			strm.frame_rate_code < mpeg_num_framerates )
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
	


	if(  aspect_ratio > mpeg_num_aspect_ratios[mpeg-1] ) 
	{
		mjpeg_error("For MPEG-%d aspect ratio code  %d > %d illegal", 
					mpeg, aspect_ratio, 
					mpeg_num_aspect_ratios[mpeg-1]);
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

/**************************
 *
 * Derived class for options set from command line
 *
 *************************/

class MPEG2EncCmdLineOptions : public MPEG2EncOptions
{
public:
    MPEG2EncCmdLineOptions();
    int SetFromCmdLine(int argc, char *argv[] );
    void Usage(const char *str);
private:
    void DisplayFrameRates();
    void DisplayAspectRatios();
    int ParseCustomMatrixFile(const char *fname, int dbug);
    void ParseCustomOption(const char *arg);
public:
    int istrm_fd;
    char *outfilename;

};

MPEG2EncCmdLineOptions::MPEG2EncCmdLineOptions() :
    MPEG2EncOptions()
{
    outfilename = 0;
    istrm_fd = 0;
        
}

void MPEG2EncCmdLineOptions::DisplayFrameRates(void)
{
 	unsigned int i;
	printf("Frame-rate codes:\n");
	for( i = 0; i < mpeg_num_framerates; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_framerate_code_definition(i));
	}
	exit(0);
}

void MPEG2EncCmdLineOptions::DisplayAspectRatios(void)
{
 	unsigned int i;
	printf("\nDisplay aspect ratio codes:\n");
	for( i = 1; i <= mpeg_num_aspect_ratios[1]; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_aspect_code_definition(2,i));
	}
	exit(0);
}

int MPEG2EncCmdLineOptions::ParseCustomMatrixFile(const char *fname, int dbug)
{
    FILE  *fp;
    uint16_t  q[128];
    int  i, j, row;
    char line[80];

    fp = fopen(fname, "r");
    if  (!fp)
    {
        mjpeg_error("can not open custom matrix file '%s'", fname);
        return(-1);
    }

    row = i = 0;
    while   (fgets(line, sizeof(line), fp))
    {
        row++;
        /* Empty lines (\n only) and comments are ignored */
        if  ((strlen(line) == 1) || line[0] == '#')
            continue;
        j = sscanf(line, "%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu\n",
                   &q[i+0], &q[i+1], &q[i+2], &q[i+3],
                   &q[i+4], &q[i+5], &q[i+6], &q[i+7]);
        if  (j != 8)
        {
            mjpeg_error("line %d ('%s') of '%s' malformed", row, line, fname);
            break;
        }
        for (j = 0; j < 8; j++)
        {
            if  (q[i + j] < 1 || q[i + j] > 255)
            {
                mjpeg_error("entry %d (%u) in line %d from '%s' invalid",
                            j, q[i + j], row, fname);
                i = -1;
                break;
            }
        }
        i += 8;
    }

    fclose(fp);

    if  (i != 128)
    {
        mjpeg_error("file '%s' did NOT have 128 values - ignoring custom matrix file", fname);
        return(-1);
    }

    for (j = 0; j < 64; j++)
    {
        custom_intra_quantizer_matrix[j] = q[j];
        custom_nonintra_quantizer_matrix[j] = q[j + 64];
    }

    if  (dbug)
    {
        mjpeg_info("INTRA and NONINTRA tables from '%s'",fname);
        for (j = 0; j < 128; j += 8)
        {
            mjpeg_info("%u %u %u %u %u %u %u %u", 
                       q[j + 0], q[j + 1], q[j + 2], q[j + 3], 
                       q[j + 4], q[j + 5], q[j + 6], q[j + 7]);
        }
    }
    return(0);
}

void MPEG2EncCmdLineOptions::ParseCustomOption(const char *arg)
{
    if	(strcmp(arg, "kvcd") == 0)
    	hf_quant = 3;
    else if (strcmp(arg, "hi-res") == 0)
    	hf_quant = 2;
    else if (strcmp(arg, "default") == 0)
    {
    	hf_quant = 0;
    	hf_q_boost = 0;
    }
    else if (strcmp(arg, "tmpgenc") == 0)
    	hf_quant = 4;
    else if (strncasecmp(arg, "file=", 5) == 0)
    {
    	if  (ParseCustomMatrixFile(arg + 5, arg[0] == 'F' ? 1 : 0) == 0)
    	    hf_quant = 5;
    }
    else if (strcmp(arg, "help") == 0)
    {
    	fprintf(stderr, "Quantization matrix names:\n\n");
    	fprintf(stderr, "\thelp - this message\n");
    	fprintf(stderr, "\tkvcd - matrices from http://www.kvcd.net\n");
    	fprintf(stderr, "\thi-res - high resolution tables (same as -H)\n");
    	fprintf(stderr, "\tdefault - turn off -N or -H (use standard tables)\n");
    	fprintf(stderr, "\ttmpgenc - TMPGEnc tables (http://www.tmpgenc.com)\n");
    	fprintf(stderr, "\tfile=filename - filename contains custom matrices\n");
    	fprintf(stderr, "\t\t8 comma separated values per line.  8 lines per matrix, INTRA matrix first, then NONINTRA\n");
    	exit(0);
    }
    else
    	mjpeg_error_exit1("Unknown type '%s' used with -K/--custom-quant", arg);
}

void MPEG2EncCmdLineOptions::Usage(const char *str)
{
	fprintf(stderr,
"--verbose|-v num\n" 
"    Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n"
"--format|-f fmt\n"
"    Set pre-defined mux format fmt.\n"
"    [0 = Generic MPEG1, 1 = standard VCD, 2 = user VCD,\n"
"     3 = Generic MPEG2, 4 = standard SVCD, 5 = user SVCD,\n"
"     6 = VCD Stills sequences, 7 = SVCD Stills sequences, 8|9 = DVD]\n"
"--aspect|-a num\n"
"    Set displayed image aspect ratio image (default: 2 = 4:3)\n"
"    [1 = 1:1, 2 = 4:3, 3 = 16:9, 4 = 2.21:1]\n"
"--frame-rate|-F num\n"
"    Set playback frame rate of encoded video\n"
"    (default: frame rate of input stream)\n"
"    0 = Display frame rate code table\n"
"--video-bitrate|-b num\n"
"    Set Bitrate of compressed video in KBit/sec\n"
"    (default: 1152 for VCD, 2500 for SVCD, 7500 for DVD)\n"
"--nonvideo-birate|-B num\n"
"    Non-video data bitrate to assume for sequence splitting\n"
"    calculations (see also --sequence-length).\n"
"--quantisation|-q num\n"
"    Image data quantisation factor [1..31] (1 is best quality, no default)\n"
"    When quantisation is set variable bit-rate encoding is activated and\n"
"    the --bitrate value sets an *upper-bound* video data-rate\n"
"--output|-o pathname\n"
"    pathname of output file or fifo (REQUIRED!!!)\n"
"--vcd-still-size|-T size\n"
"    Size in KB of VCD stills\n"
"--interlace-mode|-I num\n"
"    Sets MPEG 2 motion estimation and encoding modes:\n"
"    0 = Progressive (non-interlaced)(Movies)\n"
"    1 = Interlaced source material (video)\n"
"    2 = Interlaced source material, per-field-encoding (video)\n"
"--motion-search-radius|-r num\n"
"    Motion compensation search radius [0..32] (default 16)\n"
"--reduction-4x4|-4 num\n"
"    Reduction factor for 4x4 subsampled candidate motion estimates\n"
"    [1..4] [1 = max quality, 4 = max. speed] (default: 2)\n"
"--reduction-2x2|-2 num\n"
"    Reduction factor for 2x2 subsampled candidate motion estimates\n"
"    [1..4] [1 = max quality, 4 = max. speed] (default: 3)\n"
"--min-gop-size|-g num\n"
"    Minimum size Group-of-Pictures (default depends on selected format)\n"
"--max-gop-size|-G num\n"
"    Maximum size Group-of-Pictures (default depends on selected format)\n"
"    If min-gop is less than max-gop, mpeg2enc attempts to place GOP\n"
"    boundaries to coincide with scene changes\n"
"--closed-gop|-c\n"
"    All Group-of-Pictures are closed.  Useful for authoring multi-angle DVD\n"
"--force-b-b-p|-P\n"
"    Preserve two B frames between I/P frames when placing GOP boundaries\n"
"--quantisation-reduction|-Q num\n"
"    Max. quantisation reduction for highly active blocks\n"
"    [0.0 .. 5] (default: 0.0)\n"
"--video-buffer|-V num\n"
"    Target decoders video buffer size in KB (default 46)\n"
"--video-norm|-n n|p|s\n"
"    Tag output to suit playback in specified video norm\n"
"    (n = NTSC, p = PAL, s = SECAM) (default: PAL)\n"
"--sequence-length|-S num\n"
"    Place a sequence boundary in the video stream so they occur every\n"
"    num Mbytes once the video is multiplexed with audio etc.\n"
"    N.b. --non-video-bitrate is used to the bitrate of the other\n"
"    data that will be multiplexed with this video stream\n"
"--3-2-pulldown|-p\n"
"    Generate header flags for 3-2 pull down of 24fps movie material\n"
"--intra_dc_prec|-D [8..10]\n"
"    Set number of bits precision for DC (base colour) of blocks in MPEG-2\n"
"--reduce-hf|-N num\n"
"    [0.0..2.0] Reduce hf resolution (increase quantization) by num (default: 0.0)\n"
"--keep-hf|-H\n"
"    Maximise high-frequency resolution - useful for high quality sources\n"
"    and/or high bit-rates)\n"
"--sequence-header-every-gop|-s\n"
"    Include a sequence header every GOP if the selected format doesn't\n"
"    do so by default.\n"
"--no-dummy-svcd-SOF|-d\n"
"    Do not generate dummy SVCD scan-data for the ISO CD image\n"
"    generator \"vcdimager\" to fill in.\n"
"--playback-field-order|-z b|t\n"
"    Force setting of playback field order to bottom or top first\n"
"--multi-thread|-M num\n"
"    Activate multi-threading to optimise throughput on a system with num CPU's\n"
"    [0..32], 0=no multithreading, (default: 1)\n"
"--correct-svcd-hds|-C\n"
"    Force SVCD horizontal_display_size to be 480 - standards say 540 or 720\n"
"    But many DVD/SVCD players screw up with these values.\n"
"--no-altscan-mpeg2\n"
"    Force MPEG2 *not* to use alternate block scanning.  This may allow some\n"
"    buggy players to play SVCD streams\n"
"--no-constraints\n"
"    Deactivate the constraints for maximum video resolution and sample rate.\n"
"    Could expose bugs in the software at very high resolutions!\n"
"--custom-quant-matrices|-K kvcd|tmpgenc|default|hi-res|file=inputfile|help\n"
"    Request custom or userspecified (from a file) quantization matrices\n"
"--unit-coeff-elim|-E num\n"
"    Skip picture blocks satisfying which appear to carry little\n"
"    because they code to only unit coefficients. The number specifies\n"
"    how aggresively this should be done. A negative value means DC\n"
"    coefficients are included.  Reasonable values -40 to 40\n"
"--b-per-refframe| -R 0|1|2\n"
"    The number of B frames to generate between each I/P frame\n"
"    --help|-?\n"
"    Print this lot out!\n"
	);
	exit(0);
}







int MPEG2EncCmdLineOptions::SetFromCmdLine( int argc,	char *argv[] )
{
    static const char	short_options[]=
        "m:a:f:n:b:z:T:B:q:o:S:I:r:M:4:2:Q:X:D:g:G:v:V:F:N:tpdsZHOcCPK:E:R:";

#ifdef HAVE_GETOPT_LONG
    static struct option long_options[]={
        { "verbose",           1, 0, 'v' },
        { "format",            1, 0, 'f' },
        { "aspect",            1, 0, 'a' },
        { "frame-rate",        1, 0, 'F' },
        { "video-bitrate",     1, 0, 'b' },
        { "nonvideo-bitrate",  1, 0, 'B' },
        { "intra_dc_prec",     1, 0, 'D' },
        { "quantisation",      1, 0, 'q' },
        { "output",            1, 0, 'o' },
        { "target-still-size", 1, 0, 'T' },
        { "interlace-mode",    1, 0, 'I' },
        { "motion-search-radius", 1, 0, 'r'},
        { "reduction-4x4",  1, 0, '4'},
        { "reduction-2x2",  1, 0, '2'},
        { "min-gop-size",      1, 0, 'g'},
        { "max-gop-size",      1, 0, 'G'},
        { "closed-gop",        1, 0, 'c'},
        { "force-b-b-p", 0, &preserve_B, 1},
        { "quantisation-reduction", 1, 0, 'Q' },
        { "quant-reduction-max-var", 1, 0, 'X' },
        { "video-buffer",      1, 0, 'V' },
        { "video-norm",        1, 0, 'n' },
        { "sequence-length",   1, 0, 'S' },
        { "3-2-pulldown",      1, &vid32_pulldown, 1 },
        { "keep-hf",           0, 0, 'H' },
        { "reduce-hf",         1, 0, 'N' },
        { "sequence-header-every-gop", 0, &seq_hdr_every_gop, 1},
        { "no-dummy-svcd-SOF", 0, &svcd_scan_data, 0 },
        { "correct-svcd-hds", 0, &hack_svcd_hds_bug, 0},
        { "no-constraints", 0, &ignore_constraints, 1},
        { "no-altscan-mpeg2", 0, &hack_altscan_bug, 1},
        { "playback-field-order", 1, 0, 'z'},
        { "multi-thread",      1, 0, 'M' },
        { "custom-quant-matrices", 1, 0, 'K'},
        { "unit-coeff-elim",   1, 0, 'E'},
        { "b-per-refframe",           1, 0, 'R' },
        { "help",              0, 0, '?' },
        { 0,                   0, 0, 0 }
    };


    int n;
    int nerr = 0;
    while( (n=getopt_long(argc,argv,short_options,long_options, NULL)) != -1 )
#else
    while( (n=getopt(argc,argv,short_options)) != -1)
#endif
	{
		switch(n) {
        case 0 :                /* Flag setting handled by getopt-long */
            break;
		case 'b':
			bitrate = atoi(optarg)*1000;
			break;

		case 'T' :
			still_size = atoi(optarg)*1024;
			if( still_size < 20*1024 || still_size > 500*1024 )
			{
				mjpeg_error( "-T requires arg 20..500" );
				++nerr;
			}
			break;

		case 'B':
			nonvid_bitrate = atoi(optarg);
			if( nonvid_bitrate < 0 )
			{
				mjpeg_error("-B requires arg > 0");
				++nerr;
			}
			break;

        case 'D':
            mpeg2_dc_prec = atoi(optarg)-8;
            if( mpeg2_dc_prec < 0 || mpeg2_dc_prec > 2 )
            {
                mjpeg_error( "-D requires arg [8..10]" );
                ++nerr;
            }
            break;
        case 'C':
            hack_svcd_hds_bug = 0;
            break;

		case 'q':
			quant = atoi(optarg);
			if(quant<1 || quant>32)
			{
				mjpeg_error("-q option requires arg 1 .. 32");
				++nerr;
			}
			break;

        case 'a' :
			aspect_ratio = atoi(optarg);
            if( aspect_ratio == 0 )
				DisplayAspectRatios();
			/* Checking has to come later once MPEG 1/2 has been selected...*/
			if( aspect_ratio < 0 )
			{
				mjpeg_error( "-a option must be positive");
				++nerr;
			}
			break;

       case 'F' :
			frame_rate = atoi(optarg);
            if( frame_rate == 0 )
				DisplayFrameRates();
			if( frame_rate < 0 || 
				frame_rate >= mpeg_num_framerates)
			{
				mjpeg_error( "-F option must be [0..%d]", 
						 mpeg_num_framerates-1);
				++nerr;
			}
			break;

		case 'o':
			outfilename = optarg;
			break;

		case 'I':
			fieldenc = atoi(optarg);
			if( fieldenc < 0 || fieldenc > 2 )
			{
				mjpeg_error("-I option requires 0,1 or 2");
				++nerr;
			}
			break;

		case 'r':
			searchrad = atoi(optarg);
			if(searchrad<0 || searchrad>32)
			{
				mjpeg_error("-r option requires arg 0 .. 32");
				++nerr;
			}
			break;

		case 'M':
			num_cpus = atoi(optarg);
			if(num_cpus<0 || num_cpus>32)
			{
				mjpeg_error("-M option requires arg 0..32");
				++nerr;
			}
			break;

		case '4':
			me44_red = atoi(optarg);
			if(me44_red<0 || me44_red>4)
			{
				mjpeg_error("-4 option requires arg 0..4");
				++nerr;
			}
			break;
			
		case '2':
			me22_red = atoi(optarg);
			if(me22_red<0 || me22_red>4)
			{
				mjpeg_error("-2 option requires arg 0..4");
				++nerr;
			}
			break;

		case 'v':
			verbose = atoi(optarg);
			if( verbose < 0 || verbose > 2 )
				++nerr;
			break;
		case 'V' :
			video_buffer_size = atoi(optarg);
			if(video_buffer_size<20 || video_buffer_size>4000)
			{
				mjpeg_error("-v option requires arg 20..4000");
				++nerr;
			}
			break;

		case 'S' :
			seq_length_limit = atoi(optarg);
			if(seq_length_limit<1 )
			{
				mjpeg_error("-S option requires arg > 1");
				++nerr;
			}
			break;
		case 'p' :
			vid32_pulldown = 1;
			break;

		case 'z' :
			if( strlen(optarg) != 1 || (optarg[0] != 't' && optarg[0] != 'b' ) )
			{
				mjpeg_error("-z option requires arg b or t" );
				++nerr;
			}
			else if( optarg[0] == 't' )
				force_interlacing = Y4M_ILACE_TOP_FIRST;
			else if( optarg[0] == 'b' )
				force_interlacing = Y4M_ILACE_BOTTOM_FIRST;
			break;

		case 'f' :
			format = atoi(optarg);
			if( format < MPEG_FORMAT_FIRST ||
				format > MPEG_FORMAT_LAST )
			{
				mjpeg_error("-f option requires arg [%d..%d]", 
							MPEG_FORMAT_FIRST, MPEG_FORMAT_LAST);
				++nerr;
			}
				
			break;

		case 'n' :
			switch( optarg[0] )
			{
			case 'p' :
			case 'n' :
			case 's' :
				norm = optarg[0];
				break;
			default :
				mjpeg_error("-n option requires arg n or p, or s.");
				++nerr;
			}
			break;
		case 'g' :
			min_GOP_size = atoi(optarg);
			break;
		case 'G' :
			max_GOP_size = atoi(optarg);
			break;
        	case 'c' :
            		closed_GOPs = true;
            		break;
		case 'P' :
			preserve_B = true;
			break;
		case 'N':
            hf_q_boost = atof(optarg);
            if (hf_q_boost <0.0 || hf_q_boost > 2.0)
            {
                mjpeg_error( "-N option requires arg 0.0 .. 2.0" );
                ++nerr;
                hf_q_boost = 0.0;
            }
			if (hf_quant == 0 && hf_q_boost != 0.0)
			   hf_quant = 1;
			break;
		case 'H':
			hf_quant = 2;
            		break;
		case 'K':
			ParseCustomOption(optarg);
			break;
        case 'E':
            unit_coeff_elim = atoi(optarg);
            if (unit_coeff_elim < -40 || unit_coeff_elim > 40)
            {
                mjpeg_error( "-E option range arg -40 to 40" );
                ++nerr;
            }
            break;
        case 'R' :
            Bgrp_size = atoi(optarg)+1;
            if( Bgrp_size<1 || Bgrp_size>3)
            {
                mjpeg_error( "-R option arg 0|1|2" );
                ++nerr;
            }
            break;
		case 's' :
			seq_hdr_every_gop = 1;
			break;
		case 'd' :
			svcd_scan_data = 0;
			break;
		case 'Q' :
			act_boost = atof(optarg);
			if( act_boost <-4.0 || act_boost > 10.0)
			{
				mjpeg_error( "-q option requires arg -4.0 .. 10.0");
				++nerr;
			}
			break;
		case 'X' :
			boost_var_ceil = atof(optarg);
			if( boost_var_ceil <0 || boost_var_ceil > 50*50 )
			{
				mjpeg_error( "-X option requires arg 0 .. 2500" );
				++nerr;
			}
			break;
		case ':' :
			mjpeg_error( "Missing parameter to option!" );
		case '?':
		default:
			++nerr;
		}
	}

	/* Select input stream */
	if(optind!=argc)
	{
		if( optind == argc-1 )
		{
			istrm_fd = open( argv[optind], O_RDONLY | O_BINARY );
			if( istrm_fd < 0 )
			{
				mjpeg_error( "Unable to open: %s: ",argv[optind] );
				perror("");
				++nerr;
			}
		}
		else
			++nerr;
	}
	else
		istrm_fd = 0; /* stdin */

	if(!outfilename)
	{
		mjpeg_error("Output file name (-o option) is required!");
		++nerr;
	}

    return nerr;
}


int main( int argc,	char *argv[] )
{
	int n;

	/* Set up error logging.  The initial handling level is LOG_INFO
	 */
    MPEG2EncCmdLineOptions options;
    if( options.SetFromCmdLine( argc, argv ) != 0 )
		options.Usage(argv[0]);

	mjpeg_default_handler_verbosity(verbose);


    MPEG2Encoder encoder( options.istrm_fd, options );
    istrm_fd = options.istrm_fd;
    enc = &encoder;

	/* Read parameters of input stream used for infering MPEG parameters */
    MPEG2EncInVidParams strm;
	encoder.reader.StreamPictureParams(strm);
	
	/* Check parameters that cannot be checked when parsed/read */


	
    int nerr = 0;
	options.SetFormatPresets( strm );
	nerr += options.InferStreamDataParams(strm);
	nerr += options.CheckBasicConstraints();

	if(nerr) 
	{
		options.Usage(argv[0]);
	}


	mjpeg_info("Encoding MPEG-%d video to %s",options.mpeg,options.outfilename);
	mjpeg_info("Horizontal size: %d pel",options.in_img_width);
	mjpeg_info("Vertical size: %d pel",options.in_img_height);
	mjpeg_info("Aspect ratio code: %d = %s", 
               options.aspect_ratio,
               mpeg_aspect_code_definition(options.mpeg,options.aspect_ratio));
	mjpeg_info("Frame rate code:   %d = %s",
               options.frame_rate,
               mpeg_framerate_code_definition(options.frame_rate));

	if(options.bitrate) 
		mjpeg_info("Bitrate: %d KBit/s",options.bitrate/1000);
	else
		mjpeg_info( "Bitrate: VCD");
	if(options.quant) 
		mjpeg_info("Quality factor: %d (Quantisation = %d) (1=best, 31=worst)",
                   options.quant, 
                   (int)(inv_scale_quant( options.mpeg == 1 ? 0 : 1, 
                                          options.quant))
            );

	mjpeg_info("Field order for input: %s", 
			   mpeg_interlace_code_definition(options.input_interlacing) );

	if( options.seq_length_limit )
	{
		mjpeg_info( "New Sequence every %d Mbytes", options.seq_length_limit );
		mjpeg_info( "Assuming non-video stream of %d Kbps", options.nonvid_bitrate );
	}
	else
		mjpeg_info( "Sequence unlimited length" );

	mjpeg_info("Search radius: %d",options.searchrad);

	/* open output file */
	if (!(outfile=fopen(options.outfilename,"wb")))
	{
		mjpeg_error_exit1("Couldn't create output file %s",options.outfilename);
	}


    encoder.parms.Init( options );

	initbits(); 

    encoder.reader.Init();
    encoder.quantizer.Init();
	init_motion();
	init_transform();
	init_predict();
    encoder.seqencoder.Encode();

	fclose(outfile);
#ifdef OUTPUT_STAT
	if( statfile != NULL )
		fclose(statfile);
#endif
#ifdef ALTIVEC_BENCHMARK
	print_benchmark_statistics();
#endif
	return 0;
}

/*
	Wrapper for memory allocation that checks for failure.
*/

void *bufalloc( size_t size )
{
	void *buf;
    if( posix_memalign( &buf, static_cast<size_t>(BUFFER_ALIGN), size ) !=0 )
		mjpeg_error_exit1("malloc failed");
    return buf;
}

EncoderParams::EncoderParams()
{
    memset( this, -1, sizeof(EncoderParams) );
}

void EncoderParams::InitEncodingControls( const MPEG2EncOptions &options)
{
	int i;
    unsigned int n;

	/* Tune threading and motion compensation for specified number of CPU's 
	   and specified speed parameters.
	 */
    
	act_boost =  options.act_boost >= 0.0 
        ? (options.act_boost+1.0)
        : (options.act_boost-1.0);
    boost_var_ceil = options.boost_var_ceil;
	switch( options.num_cpus )
	{

	case 0 : /* Special case for debugging... turns of multi-threading */
		max_encoding_frames = 1;
		refine_from_rec = true;
		parallel_read = false;
		break;

	case 1 :
		max_encoding_frames = 1;
		refine_from_rec = true;
		parallel_read = true;
		break;
	case 2:
		max_encoding_frames = 2;
		refine_from_rec = true;
		parallel_read = true;
		break;
	default :
		max_encoding_frames = options.num_cpus > MAX_WORKER_THREADS-1 ?
			                  MAX_WORKER_THREADS-1 :
			                  options.num_cpus;
		refine_from_rec = false;
		parallel_read = true;
		break;
	}

    max_active_ref_frames = 
        M == 0 ? max_encoding_frames : (max_encoding_frames+2);
    max_active_b_frames = 
        M <= 1 ? 0 : max_encoding_frames+1;

	me44_red		= options.me44_red;
	me22_red		= options.me22_red;

    unit_coeff_elim	= options.unit_coeff_elim;

	/* round picture dimensions to nearest multiple of 16 or 32 */
	mb_width = (horizontal_size+15)/16;
	mb_height = prog_seq ? (vertical_size+15)/16 : 2*((vertical_size+31)/32);
	mb_height2 = fieldpic ? mb_height>>1 : mb_height; /* for field pictures */
	enc_width = 16*mb_width;
	enc_height = 16*mb_height;

#ifdef DEBUG_MOTION_EST
    static const int MARGIN = 64;
#else
    static const int MARGIN = 0;
#endif
    
#ifdef HAVE_ALTIVEC
	/* Pad phy_width to 64 so that the rowstride of 4*4
	 * sub-sampled data will be a multiple of 16 (ideal for AltiVec)
	 * and the rowstride of 2*2 sub-sampled data will be a multiple
	 * of 32. Height does not affect rowstride, no padding needed.
	 */
	phy_width = (enc_width + 63) & (~63);
#else
	phy_width = enc_width+MARGIN;
#endif
	phy_height = enc_height+MARGIN;

	/* Calculate the sizes and offsets in to luminance and chrominance
	   buffers.  A.Stevens 2000 for luminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel
	   sub-sampled uint8_t followed by 4*4 sub-sampled.  We add an
	   extra row to act as a margin to allow us to neglect / postpone
	   edge condition checking in time-critical loops...  */

	phy_chrom_width = (CHROMA420==CHROMA444) 
        ? phy_width 
        : phy_width>>1;
	phy_chrom_height = (CHROMA420!=CHROMA420) 
        ? phy_height 
        : phy_height>>1;
	enc_chrom_width = (CHROMA420==CHROMA444) 
        ? enc_width 
        : enc_width>>1;
	enc_chrom_height = (CHROMA420!=CHROMA420) 
        ? enc_height 
        : enc_height>>1;

	phy_height2 = fieldpic ? phy_height>>1 : phy_height;
	enc_height2 = fieldpic ? enc_height>>1 : enc_height;
	phy_width2 = fieldpic ? phy_width<<1 : phy_width;
	phy_chrom_width2 = fieldpic 
        ? phy_chrom_width<<1 
        : phy_chrom_width;
 
	lum_buffer_size = (phy_width*phy_height) +
					 sizeof(uint8_t) *(phy_width/2)*(phy_height/2) +
					 sizeof(uint8_t) *(phy_width/4)*(phy_height/4);
	chrom_buffer_size = phy_chrom_width*phy_chrom_height;
    

	fsubsample_offset = (phy_width)*(phy_height) * sizeof(uint8_t);
	qsubsample_offset =  fsubsample_offset 
        + (phy_width/2)*(phy_height/2)*sizeof(uint8_t);

	mb_per_pict = mb_width*mb_height2;


#ifdef OUTPUT_STAT
	/* open statistics output file */
	if (!(statfile = fopen(statname,"w")))
	{
		mjpeg_error_exit1( "Couldn't create statistics output file %s",
						   statname);
	}
#endif
}


static int f_code( int max_radius )
{
	int c=5;
	if( max_radius < 64) c = 4;
	if( max_radius < 32) c = 3;
	if( max_radius < 16) c = 2;
	if( max_radius < 8) c = 1;
	return c;
}

void EncoderParams::Init( const MPEG2EncOptions &options )
{
	int i;
    const char *msg;

	inputtype = 0;  /* doesnt matter */
	istrm_nframes = 999999999; /* determined by EOF of stdin */

	N_min = options.min_GOP_size;      /* I frame distance */
	N_max = options.max_GOP_size;
    closed_GOPs = options.closed_GOPs;
	mjpeg_info( "GOP SIZE RANGE %d TO %d %s", 
                N_min, N_max,
                closed_GOPs ? "(all GOPs closed)" : "" 
                );
	M = options.Bgrp_size;             /* I or P frame distance */
	M_min = options.preserve_B ? M : 1;
	if( M >= N_min )
		M = N_min-1;
	mpeg1           = (options.mpeg == 1);
	fieldpic        = (options.fieldenc == 2);

    // SVCD and probably DVD? mandate progressive_sequence = 0 
    switch( options.format )
    {
    case MPEG_FORMAT_SVCD :
    case MPEG_FORMAT_SVCD_NSR :
    case MPEG_FORMAT_SVCD_STILL :
    case MPEG_FORMAT_DVD :
    case MPEG_FORMAT_DVD_NAV :
        prog_seq = 0;
        break;
    default :
        prog_seq        = (options.mpeg == 1 || options.fieldenc == 0);
        break;
    }
	pulldown_32     = options.vid32_pulldown;

	aspectratio     = options.aspect_ratio;
	frame_rate_code = options.frame_rate;
	dctsatlim		= mpeg1 ? 255 : 2047;

	/* If we're using a non standard (VCD?) profile bit-rate adjust	the vbv
		buffer accordingly... */

	if(options.bitrate == 0 )
	{
		mjpeg_error_exit1( "Generic format - must specify bit-rate!" );
	}

	still_size = 0;
	if( MPEG_STILLS_FORMAT(options.format) )
	{
		vbv_buffer_code = options.vbv_buffer_still_size / 2048;
		vbv_buffer_still_size = options.pad_stills_to_vbv_buffer_size;
		bit_rate = options.bitrate;
		still_size = options.still_size;
	}
	else if( options.mpeg == 1 )
	{
		/* Scale VBV relative to VCD  */
		bit_rate = MAX(10000, options.bitrate);
		vbv_buffer_code = (20 * options.bitrate  / 1151929);
	}
	else
	{
		bit_rate = MAX(10000, options.bitrate);
		vbv_buffer_code = MIN(112,options.video_buffer_size / 2);
	}
	vbv_buffer_size = vbv_buffer_code*16384;

	if( options.quant )
	{
		quant_floor = inv_scale_quant( options.mpeg == 1 ? 0 : 1, 
                                           options.quant );
	}
	else
	{
		quant_floor = 0.0;		/* Larger than max quantisation */
	}

	video_buffer_size = options.video_buffer_size * 1024 * 8;
	
	seq_hdr_every_gop = options.seq_hdr_every_gop;
	seq_end_every_gop = options.seq_end_every_gop;
	svcd_scan_data = options.svcd_scan_data;
	ignore_constraints = options.ignore_constraints;
	seq_length_limit = options.seq_length_limit;
	nonvid_bit_rate = options.nonvid_bitrate * 1000;
	low_delay       = 0;
	constrparms     = (options.mpeg == 1 && 
						   !MPEG_STILLS_FORMAT(options.format));
	profile         = 4; /* Main profile resp. */
	level           = 8; /* Main Level      CCIR 601 rates */
	switch(options.norm)
	{
	case 'p': video_format = 1; break;
	case 'n': video_format = 2; break;
	case 's': video_format = 3; break;
	default:  video_format = 5; break; /* unspec. */
	}
	switch(options.norm)
	{
	case 's':
	case 'p':  /* ITU BT.470  B,G */
		color_primaries = 5;
		transfer_characteristics = 5; /* Gamma = 2.8 (!!) */
		matrix_coefficients = 5; 
        msg = "PAL B/G";
		break;
	case 'n': /* SMPTPE 170M "modern NTSC" */
		color_primaries = 6;
		matrix_coefficients = 6; 
		transfer_characteristics = 6;
        msg = "NTSC";
		break; 
	default:   /* unspec. */
		color_primaries = 2;
		matrix_coefficients = 2; 
		transfer_characteristics = 2;
        msg = "unspecified";
		break;
	}
    mjpeg_info( "Setting colour/gamma parameters to \"%s\"", msg);

    horizontal_size = options.in_img_width;
    vertical_size = options.in_img_height;
	switch( options.format )
	{
	case MPEG_FORMAT_SVCD_STILL :
	case MPEG_FORMAT_SVCD_NSR :
	case MPEG_FORMAT_SVCD :
    case MPEG_FORMAT_DVD :
    case MPEG_FORMAT_DVD_NAV :
        /* It would seem DVD and perhaps SVCD demand a 540 pixel display size
           for 4:3 aspect video. However, many players expect 480 and go weird
           if this isn't set...
        */
        if( options.hack_svcd_hds_bug )
        {
            display_horizontal_size  = options.in_img_width;
            display_vertical_size    = options.in_img_height;
        }
        else
        {
            display_horizontal_size  = aspectratio == 2 ? 540 : 720;
            display_vertical_size    = options.in_img_height;
        }
		break;
	default:
		display_horizontal_size  =  options.in_img_width;
		display_vertical_size    =  options.in_img_height;
		break;
	}

	dc_prec         = options.mpeg2_dc_prec;  /* 9 bits */
    topfirst = 0;
	if( ! prog_seq )
	{
		int fieldorder;
		if( options.force_interlacing != Y4M_UNKNOWN ) 
		{
			mjpeg_info( "Forcing playback video to be: %s",
						mpeg_interlace_code_definition(	options.force_interlacing ) );	
			fieldorder = options.force_interlacing;
		}
		else
			fieldorder = options.input_interlacing;

		topfirst = (fieldorder == Y4M_ILACE_TOP_FIRST || 
                              fieldorder ==Y4M_ILACE_NONE );
	}
	else
		topfirst = 0;

    // Restrict to frame motion estimation and DCT modes only when MPEG1
    // or when progressive content is specified for MPEG2.
    // Note that for some profiles although we have progressive sequence 
    // header bit = 0 we still only encode with frame modes (for speed).
	frame_pred_dct_tab[0] 
		= frame_pred_dct_tab[1] 
		= frame_pred_dct_tab[2] 
        = (options.mpeg == 1 || options.fieldenc == 0) ? 1 : 0;

    mjpeg_info( "Progressive format frames = %d", 	frame_pred_dct_tab[0] );
	qscale_tab[0] 
		= qscale_tab[1] 
		= qscale_tab[2] 
		= options.mpeg == 1 ? 0 : 1;

	intravlc_tab[0] 
		= intravlc_tab[1] 
		= intravlc_tab[2] 
		= options.mpeg == 1 ? 0 : 1;

	altscan_tab[2]  
		= altscan_tab[1]  
		= altscan_tab[0]  
		= (options.mpeg == 1 || options.hack_altscan_bug) ? 0 : 1;
	

	/*  A.Stevens 2000: The search radius *has* to be a multiple of 8
		for the new fast motion compensation search to work correctly.
		We simply round it up if needs be.  */

    int searchrad = options.searchrad;
	if(searchrad*M>127)
	{
		searchrad = 127/M;
		mjpeg_warn("Search radius reduced to %d",searchrad);
	}
	
	{ 
		int radius_x = searchrad;
		int radius_y = searchrad*vertical_size/horizontal_size;

		/* TODO: These f-codes should really be adjusted for each
		   picture type... */

		motion_data = (struct motion_data *)malloc(M*sizeof(struct motion_data));
		if (!motion_data)
			mjpeg_error_exit1("malloc failed");

		for (i=0; i<M; i++)
		{
			if(i==0)
			{
				motion_data[i].sxf = round_search_radius(radius_x*M);
				motion_data[i].forw_hor_f_code  = f_code(motion_data[i].sxf);
				motion_data[i].syf = round_search_radius(radius_y*M);
				motion_data[i].forw_vert_f_code  = f_code(motion_data[i].syf);
			}
			else
			{
				motion_data[i].sxf = round_search_radius(radius_x*i);
				motion_data[i].forw_hor_f_code  = f_code(motion_data[i].sxf);
				motion_data[i].syf = round_search_radius(radius_y*i);
				motion_data[i].forw_vert_f_code  = f_code(motion_data[i].syf);
				motion_data[i].sxb = round_search_radius(radius_x*(M-i));
				motion_data[i].back_hor_f_code  = f_code(motion_data[i].sxb);
				motion_data[i].syb = round_search_radius(radius_y*(M-i));
				motion_data[i].back_vert_f_code  = f_code(motion_data[i].syb);
			}

			/* MPEG-1 demands f-codes for vertical and horizontal axes are
			   identical!!!!
			*/
			if( mpeg1 )
			{
				motion_data[i].syf = motion_data[i].sxf;
				motion_data[i].syb  = motion_data[i].sxb;
				motion_data[i].forw_vert_f_code  = 
					motion_data[i].forw_hor_f_code;
				motion_data[i].back_vert_f_code  = 
					motion_data[i].back_hor_f_code;
				
			}
		}
		
	}
	


	/* make sure MPEG specific parameters are valid */
	RangeChecks();

	/* Set the frame decode rate and frame display rates.
	   For 3:2 movie pulldown decode rate is != display rate due to
	   the repeated field that appears every other frame.
	*/
	frame_rate = Y4M_RATIO_DBL(mpeg_framerate(frame_rate_code));
	if( options.vid32_pulldown )
	{
		decode_frame_rate = frame_rate * (2.0 + 2.0) / (3.0 + 2.0);
		mjpeg_info( "3:2 Pulldown selected frame decode rate = %3.3f fps", 
					decode_frame_rate);
	}
	else
		decode_frame_rate = frame_rate;

	if ( !mpeg1)
	{
		ProfileAndLevelChecks();
	}
	else
	{
		/* MPEG-1 */
		if (constrparms)
		{
			if (horizontal_size>768
				|| vertical_size>576
				|| ((horizontal_size+15)/16)*((vertical_size+15)/16)>396
				|| ((horizontal_size+15)/16)*((vertical_size+15)/16)*frame_rate>396*25.0
				|| frame_rate>30.0)
			{
				mjpeg_info( "size - setting constrained_parameters_flag = 0");
				constrparms = 0;
			}
		}

		if (constrparms)
		{
			for (i=0; i<M; i++)
			{
				if (motion_data[i].forw_hor_f_code>4)
				{
					mjpeg_info("Hor. motion search forces constrained_parameters_flag = 0");
					constrparms = 0;
					break;
				}

				if (motion_data[i].forw_vert_f_code>4)
				{
					mjpeg_info("Ver. motion search forces constrained_parameters_flag = 0");
					constrparms = 0;
					break;
				}

				if (i!=0)
				{
					if (motion_data[i].back_hor_f_code>4)
					{
						mjpeg_info("Hor. motion search setting constrained_parameters_flag = 0");
						constrparms = 0;
						break;
					}

					if (motion_data[i].back_vert_f_code>4)
					{
						mjpeg_info("Ver. motion search setting constrained_parameters_flag = 0");
						constrparms = 0;
						break;
					}
				}
			}
		}
	}

	/* relational checks */
	if ( mpeg1 )
	{
		if (!prog_seq)
		{
			mjpeg_warn("mpeg1 specified - setting progressive_sequence = 1");
			prog_seq = 1;
		}

		if (dc_prec!=0)
		{
			mjpeg_info("mpeg1 - setting intra_dc_precision = 0");
			dc_prec = 0;
		}

		for (i=0; i<3; i++)
			if (qscale_tab[i])
			{
				mjpeg_info("mpeg1 - setting qscale_tab[%d] = 0",i);
				qscale_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (intravlc_tab[i])
			{
				mjpeg_info("mpeg1 - setting intravlc_tab[%d] = 0",i);
				intravlc_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (altscan_tab[i])
			{
				mjpeg_info("mpeg1 - setting altscan_tab[%d] = 0",i);
				altscan_tab[i] = 0;
			}
	}

	if ( !mpeg1 && constrparms)
	{
		mjpeg_info("not mpeg1 - setting constrained_parameters_flag = 0");
		constrparms = 0;
	}


	if( (!prog_seq || fieldpic != 0 ) &&
		( (vertical_size+15) / 16)%2 != 0 )
	{
		mjpeg_warn( "Frame height won't split into two equal field pictures...");
		mjpeg_warn( "forcing encoding as progressive video");
		prog_seq = 1;
		fieldpic = 0;
	}


	if (prog_seq && fieldpic != 0)
	{
		mjpeg_info("prog sequence - forcing progressive frame encoding");
		fieldpic = 0;
	}


	if (prog_seq && topfirst)
	{
		mjpeg_info("prog sequence setting top_field_first = 0");
		topfirst = 0;
	}

	/* search windows */
	for (i=0; i<M; i++)
	{
		if (motion_data[i].sxf > (4U<<motion_data[i].forw_hor_f_code)-1)
		{
			mjpeg_info(
				"reducing forward horizontal search width to %d",
						(4<<motion_data[i].forw_hor_f_code)-1);
			motion_data[i].sxf = (4U<<motion_data[i].forw_hor_f_code)-1;
		}

		if (motion_data[i].syf > (4U<<motion_data[i].forw_vert_f_code)-1)
		{
			mjpeg_info(
				"reducing forward vertical search width to %d",
				(4<<motion_data[i].forw_vert_f_code)-1);
			motion_data[i].syf = (4U<<motion_data[i].forw_vert_f_code)-1;
		}

		if (i!=0)
		{
			if (motion_data[i].sxb > (4U<<motion_data[i].back_hor_f_code)-1)
			{
				mjpeg_info(
					"reducing backward horizontal search width to %d",
					(4<<motion_data[i].back_hor_f_code)-1);
				motion_data[i].sxb = (4U<<motion_data[i].back_hor_f_code)-1;
			}

			if (motion_data[i].syb > (4U<<motion_data[i].back_vert_f_code)-1)
			{
				mjpeg_info(
					"reducing backward vertical search width to %d",
					(4<<motion_data[i].back_vert_f_code)-1);
				motion_data[i].syb = (4U<<motion_data[i].back_vert_f_code)-1;
			}
		}
	}

    InitQuantMatrices( options );
    InitEncodingControls( options );
}

/*
  If the use has selected suppression of hf noise via quantisation
  then we boost quantisation of hf components EXPERIMENTAL: currently
  a linear ramp from 0 at 4pel to hf_q_boost increased
  quantisation...

*/

static int quant_hfnoise_filt(int orgquant, int qmat_pos, double hf_q_boost )
    {
    int orgdist = intmax(qmat_pos % 8, qmat_pos/8);
    double qboost;

    /* Maximum hf_q_boost quantisation boost for HF components.. */
    qboost = 1.0 + ((hf_q_boost * orgdist) / 8);
    return static_cast<int>(orgquant * qboost);
    }


void EncoderParams::InitQuantMatrices( const MPEG2EncOptions &options )
{
    int i, v, q;
    const char *msg = NULL;
    const uint16_t *qmat, *niqmat;
    load_iquant = 0;
    load_niquant = 0;

    /* bufalloc to ensure alignment */
    intra_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));
    inter_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));

    switch  (options.hf_quant)
    {
    case  0:    /* No -N, -H or -K used.  Default matrices */
        msg = "Using default unmodified quantization matrices";
        qmat = default_intra_quantizer_matrix;
        niqmat = default_nonintra_quantizer_matrix;
        break;
    case  1:    /* "-N value" used but not -K or -H */
        msg = "Using -N modified default quantization matrices";
        qmat = default_intra_quantizer_matrix;
        niqmat = default_nonintra_quantizer_matrix;
        load_iquant = 1;
        load_niquant = 1;
        break;
    case  2:    /* -H used OR -H followed by "-N value" */
        msg = "Setting hi-res intra Quantisation matrix";
        qmat = hires_intra_quantizer_matrix;
        niqmat = hires_nonintra_quantizer_matrix;
        load_iquant = 1;
        if(options.hf_q_boost)
            load_niquant = 1;   /* Custom matrix if -N used */
        break;
    case  3:
        msg = "KVCD Notch Quantization Matrix";
        qmat = kvcd_intra_quantizer_matrix;
        niqmat = kvcd_nonintra_quantizer_matrix;
        load_iquant = 1;
        load_niquant = 1;
        break;
    case  4:
        msg = "TMPGEnc Quantization matrix";
        qmat = tmpgenc_intra_quantizer_matrix;
        niqmat = tmpgenc_nonintra_quantizer_matrix;
        load_iquant = 1;
        load_niquant = 1;
        break;
    case  5:            /* -K file=qmatrixfilename */
        msg = "Loading custom matrices from user specified file";
        load_iquant = 1;
        load_niquant = 1;
        qmat = options.custom_intra_quantizer_matrix;
        niqmat = options.custom_nonintra_quantizer_matrix;
        break;
    default:
        mjpeg_error_exit1("Help!  Unknown hf_quant value %d",
                          options.hf_quant);
        /* NOTREACHED */
    }

    if  (msg)
        mjpeg_info(msg);
    
    for (i = 0; i < 64; i++)
    {
        v = quant_hfnoise_filt(qmat[i], i, options.hf_q_boost);
        if  (v < 1 || v > 255)
            mjpeg_error_exit1("bad intra value after -N adjust");
        intra_q[i] = v;

        v = quant_hfnoise_filt(niqmat[i], i, options.hf_q_boost);
        if  (v < 1 || v > 255)
            mjpeg_error_exit1("bad nonintra value after -N adjust");
        inter_q[i] = v;
    }

}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
