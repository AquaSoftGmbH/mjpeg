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
#include "motionsearch.h"
#include "format_codes.h"
#include "mpegconsts.h"
#include "fastintfns.h"
#ifdef HAVE_ALTIVEC
/* needed for ALTIVEC_BENCHMARK and print_benchmark_statistics() */
#include "../utils/altivec/altivec_conf.h"
#endif
int verbose = 1;

/* private prototypes */
static void init_mpeg_parms (void);
static void init_encoder(void);
static void init_quantmat (void);

#define MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define MIN(a,b) ( (a)<(b) ? (a) : (b) )

#ifndef O_BINARY
# define O_BINARY 0
#endif

/* Command Parameter values.  These are checked and processed for
   defaults etc after parsing.  The resulting values then set the opt_
   variables that control the actual encoder.
*/

static int param_format = MPEG_FORMAT_MPEG1;
static int param_bitrate    = 0;
static int param_nonvid_bitrate = 0;
static int param_quant      = 0;
static int param_searchrad  = 16;
static int param_mpeg       = 1;
static int param_aspect_ratio = 0;
static int param_frame_rate  = 0;
static int param_fieldenc   = -1; /* 0: progressive, 
                                     1 = frame pictures, 
                                     interlace frames with field
                                     MC and DCT in picture 
                                     2 = field pictures
                                  */
static int param_norm       = 0;  /* 'n': NTSC, 'p': PAL, 's': SECAM, else unspecified */
static int param_44_red	= 2;
static int param_22_red	= 3;	
static int param_hf_quant = 0;
static double param_act_boost = 0.0;
static int param_video_buffer_size = 0;
static int param_seq_length_limit = 0;
static int param_min_GOP_size = -1;
static int param_max_GOP_size = -1;
static bool param_closed_GOPs = false;
static int param_preserve_B = 0;
static int param_Bgrp_size = 3;
static int param_num_cpus = 1;
static int param_32_pulldown = 0;
static int param_svcd_scan_data = -1;
static int param_seq_hdr_every_gop = 0;
static int param_seq_end_every_gop = 0;
static int param_still_size = 0;
static int param_pad_stills_to_vbv_buffer_size = 0;
static int param_vbv_buffer_still_size = 0;
static int param_force_interlacing = Y4M_UNKNOWN;
static int param_input_interlacing;
static int param_hack_svcd_hds_bug = 1;
static int param_hack_altscan_bug = 0;
static int param_mpeg2_dc_prec = 1;
static int param_ignore_constraints = 0;

/* Input Stream parameter values that have to be further processed to
   set encoding options */

static mpeg_aspect_code_t strm_aspect_ratio;
static int strm_frame_rate_code;

/* reserved: for later use */
int param_422 = 0;


static void DisplayFrameRates(void)
{
 	int i;
	printf("Frame-rate codes:\n");
	for( i = 0; i < mpeg_num_framerates; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_framerate_code_definition(i));
	}
	exit(0);
}

static void DisplayAspectRatios(void)
{
 	int i;
	printf("\nDisplay aspect ratio codes:\n");
	for( i = 1; i <= mpeg_num_aspect_ratios[1]; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_aspect_code_definition(2,i));
	}
	exit(0);
}

static void Usage(char *str)
{
	fprintf(stderr,
"--verbose|-v num\n" 
"    Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n"
"--format-f fmt\n"
"    Set pre-defined mux format fmt.\n"
"    [0 = Generic MPEG1, 1 = standard VCD, 2 = VCD,\n"
"     3 = Generic MPEG2, 4 = standard SVCD, 5 = user SVCD,\n"
"     6 = VCD Stills sequences, 7 = SVCD Stills sequences, 8 = DVD]\n"
"--aspect|-a num\n"
"    Set displayed image aspect ratio image (default: 2 = 4:3)\n"
"    [1 = 1:1, 2 = 4:3, 3 = 16:9, 4 = 2.21:1]\n"
"--frame-rate|-F num\n"
"    Set playback frame rate of encoded video\n"
"    (default: frame rate of input stream)\n"
"    0 = Display frame rate code table\n"
"--video-bitrate|-b num\n"
"    Set Bitrate of compress video in KBit/sec\n"
"    (default: 1152 for VCD, 2500 for SVCD, 3800 for DVD)\n"
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
"    Sets MPEG 2 motino estimation and encoding modes:\n"
"    0 = Progressive (non-interlaced)(Movies)\n"
"    1 = Interlaced source material (video)\n"
"--motion-search-radius|-r num\n"
"    Motion compensation search radius [0..32] (default 16)\n"
"--reduction-4x4|num\n"
"    Reduction factor for 4x4 subsampled candidate motion estimates\n"
"    [1..4] [1 = max quality, 4 = max. speed] (default: 2)\n"
"--reduction-2x2|-2 num\n"
"    Reduction factor for 2x2 subsampled candidate motion estimates\n"
"    [1..4] [1 = max quality, 4 = max. speed] (default: 3)\n"
"--min-gop-size|-g num\n"
"    Minimum size Group-of-Pictures (default depends on selected format)\n"
"--max-gop-size|-G num\n"
"    Maximum size Group-of-Pictures (default depends on selected format)\n"
"    If min-gop is less than  max-gop, mpeg2enc attempts to place GOP\n"
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
"--reduce-hf|-N\n"
"    Reduce high frequency resolution - useful as a mild noise reduction\n"
"--keep-hf|-h\n"
"    Maximise high-frequency resolution - useful for high quality sources\n"
"    and/or high bit-rates)\n"
"--sequence-header-every-gop|-s\n"
"    Include a sequence header every GOP if the selected format doesn't\n"
"    do so by default.\n"
"--no-dummy-svcd-SOF|-d\n"
"    Don't generate of dummy SVCD scan-data for the ISO CD image\n"
"    generator \"vcdimager\" to fill in.\n"
"--playback-field-order|-z b|t\n"
"    Force setting of playback field order to bottom or top first\n"
"--multi-thread|-M num\n"
"    Activate multi-threading to optimise through on a system with num CPU's\n""    [0..32], 0=no multithreading, (default: 1)\n"
"--correct-svcd-hds|-C\n"
"    Force SVCD horizontal_display_size to be 480 - standards say 540 or 720\n"
"    But many DVD/SVCD players screw up with these values.\n"
"--no-altscan-mpeg2\n"
"    Force MPEG2 *not* to use alternate block scanning.  This may allow some\n"
"    buggy players to play SVCD streams\n"
"--no-constraints\n"
"    Deactivate the constraints for maximum video resolution and sample rate.\n"
"    Could expose bugs in the software at very high resolutions !\n"
"--help|-?\n"
"    Print this lot out!\n"
	);
	exit(0);
}

static void set_format_presets(void)
{
	switch( param_format  )
	{
	case MPEG_FORMAT_MPEG1 :  /* Generic MPEG1 */
		mjpeg_info( "Selecting generic MPEG1 output profile");
		if( param_video_buffer_size == 0 )
			param_video_buffer_size = 46;
		if( param_bitrate == 0 )
			param_bitrate = 1151929;
		break;

	case MPEG_FORMAT_VCD :
		param_mpeg = 1;
		param_bitrate = 1151929;
		param_video_buffer_size = 46;
        param_preserve_B = true;
        param_min_GOP_size = 9;
		param_max_GOP_size = param_norm == 'n' ? 18 : 15;
		mjpeg_info("VCD default options selected");
		
	case MPEG_FORMAT_VCD_NSR : /* VCD format, non-standard rate */
		mjpeg_info( "Selecting VCD output profile");
		param_mpeg = 1;
		param_svcd_scan_data = 0;
		param_seq_hdr_every_gop = 1;
		if( param_bitrate == 0 )
			param_bitrate = 1151929;
		if( param_video_buffer_size == 0 )
			param_video_buffer_size = 46 * param_bitrate / 1151929;
        if( param_seq_length_limit == 0 )
            param_seq_length_limit = 700;
        if( param_nonvid_bitrate == 0 )
            param_nonvid_bitrate = 230;
		break;
		
	case  MPEG_FORMAT_MPEG2 : 
		param_mpeg = 2;
		mjpeg_info( "Selecting generic MPEG2 output profile");
		param_mpeg = 2;
		if( param_fieldenc == -1 )
			param_fieldenc = 1;
		if( param_video_buffer_size == 0 )
			param_video_buffer_size = 46 * param_bitrate / 1151929;
		break;

	case MPEG_FORMAT_SVCD :
		mjpeg_info("SVCD standard settings selected");
		param_bitrate = 2500000;
		param_max_GOP_size = param_norm == 'n' ? 18 : 15;
		param_video_buffer_size = 230;

	case  MPEG_FORMAT_SVCD_NSR :		/* Non-standard data-rate */
		mjpeg_info( "Selecting SVCD output profile");
		param_mpeg = 2;
		if( param_fieldenc == -1 )
			param_fieldenc = 1;
		if( param_quant == 0 )
			param_quant = 8;
		if( param_svcd_scan_data == -1 )
			param_svcd_scan_data = 1;
		if( param_min_GOP_size == -1 )
            param_min_GOP_size = 9;
        param_seq_hdr_every_gop = 1;
        if( param_seq_length_limit == 0 )
            param_seq_length_limit = 700;
        if( param_nonvid_bitrate == 0 )
            param_nonvid_bitrate = 230;
        break;

	case MPEG_FORMAT_VCD_STILL :
		mjpeg_info( "Selecting VCD Stills output profile");
		param_mpeg = 1;
		/* We choose a generous nominal bit-rate as its VBR anyway
		   there's only one frame per sequence ;-). It *is* too small
		   to fill the frame-buffer in less than one PAL/NTSC frame
		   period though...*/
		param_bitrate = 8000000;

		/* Now we select normal/hi-resolution based on the input stream
		   resolution. 
		*/
		
		if( opt_horizontal_size == 352 && 
			(opt_vertical_size == 240 || opt_vertical_size == 288 ) )
		{
			/* VCD normal resolution still */
			if( param_still_size == 0 )
				param_still_size = 30*1024;
			if( param_still_size < 20*1024 || param_still_size > 42*1024 )
			{
				mjpeg_error_exit1( "VCD normal-resolution stills must be >= 20KB and <= 42KB each");
			}
			/* VBV delay encoded normally */
			param_vbv_buffer_still_size = 46*1024;
			param_video_buffer_size = 46;
			param_pad_stills_to_vbv_buffer_size = 0;
		}
		else if( opt_horizontal_size == 704 &&
				 (opt_vertical_size == 480 || opt_vertical_size == 576) )
		{
			/* VCD high-resolution stills: only these use vbv_delay
			 to encode picture size...
			*/
			if( param_still_size == 0 )
				param_still_size = 125*1024;
			if( param_still_size < 46*1024 || param_still_size > 220*1024 )
			{
				mjpeg_error_exit1( "VCD normal-resolution stills should be >= 46KB and <= 220KB each");
			}
			param_vbv_buffer_still_size = param_still_size;
			param_video_buffer_size = 224;
			param_pad_stills_to_vbv_buffer_size = 1;			
		}
		else
		{
			mjpeg_error("VCD normal resolution stills must be 352x288 (PAL) or 352x240 (NTSC)");
			mjpeg_error_exit1( "VCD high resolution stills must be 704x576 (PAL) or 704x480 (NTSC)");
		}
		param_quant = 0;		/* We want to try and hit our size target */
		
		param_seq_hdr_every_gop = 1;
		param_seq_end_every_gop = 1;
		param_min_GOP_size = 1;
		param_max_GOP_size = 1;
		break;

	case MPEG_FORMAT_SVCD_STILL :
		mjpeg_info( "Selecting SVCD Stills output profile");
		param_mpeg = 2;
		if( param_fieldenc == -1 )
			param_fieldenc = 1;
		/* We choose a generous nominal bit-rate as its VBR anyway
		   there's only one frame per sequence ;-). It *is* too small
		   to fill the frame-buffer in less than one PAL/NTSC frame
		   period though...*/

		param_bitrate = 2500000;
		param_video_buffer_size = 230;
		param_vbv_buffer_still_size = 220*1024;
		param_pad_stills_to_vbv_buffer_size = 0;

		/* Now we select normal/hi-resolution based on the input stream
		   resolution. 
		*/
		
		if( opt_horizontal_size == 480 && 
			(opt_vertical_size == 480 || opt_vertical_size == 576 ) )
		{
			mjpeg_info( "SVCD normal-resolution stills selected." );
			if( param_still_size == 0 )
				param_still_size = 90*1024;
		}
		else if( opt_horizontal_size == 704 &&
				 (opt_vertical_size == 480 || opt_vertical_size == 576) )
		{
			mjpeg_info( "SVCD high-resolution stills selected." );
			if( param_still_size == 0 )
				param_still_size = 125*1024;
		}
		else
		{
			mjpeg_error("SVCD normal resolution stills must be 480x576 (PAL) or 480x480 (NTSC)");
			mjpeg_error_exit1( "SVCD high resolution stills must be 704x576 (PAL) or 704x480 (NTSC)");
		}

		if( param_still_size < 30*1024 || param_still_size > 200*1024 )
		{
			mjpeg_error_exit1( "SVCD resolution stills must be >= 30KB and <= 200KB each");
		}


		param_seq_hdr_every_gop = 1;
		param_seq_end_every_gop = 1;
		param_min_GOP_size = 1;
		param_max_GOP_size = 1;
		break;


	case MPEG_FORMAT_DVD :
	case MPEG_FORMAT_DVD_NAV :
		mjpeg_info( "Selecting DVD output profile");
		
		if( param_bitrate == 0 )
			param_bitrate = 7500000;
		if( param_fieldenc == -1 )
			param_fieldenc = 1;
		param_video_buffer_size = 230;
		param_mpeg = 2;
		if( param_quant == 0 )
			param_quant = 8;
		param_seq_hdr_every_gop = 1;
		break;
	}

    switch( param_mpeg )
    {
    case 1 :
        if( param_min_GOP_size == -1 )
            param_min_GOP_size = 12;
        if( param_max_GOP_size == -1 )
            param_max_GOP_size = 12;
        break;
    case 2:
        if( param_min_GOP_size == -1 )
            param_min_GOP_size = 9;
        if( param_max_GOP_size == -1 )
            param_max_GOP_size = (param_norm == 'n' ? 18 : 15);
        break;
    }
	if( param_svcd_scan_data == -1 )
		param_svcd_scan_data = 0;
}

static int infer_mpeg1_aspect_code( char norm, mpeg_aspect_code_t mpeg2_code )
{
	switch( mpeg2_code )
	{
	case 1 :					/* 1:1 */
		return 1;
	case 2 :					/* 4:3 */
		if( param_norm == 'p' || param_norm == 's' )
			return 8;
	    else if( param_norm == 'n' )
			return 12;
		else 
			return 0;
	case 3 :					/* 16:9 */
		if( param_norm == 'p' || param_norm == 's' )
			return 3;
	    else if( param_norm == 'n' )
			return 6;
		else
			return 0;
	default :
		return 0;				/* Unknown */
	}
}

static int infer_default_params(void)
{
	int nerr = 0;


	/* Infer norm, aspect ratios and frame_rate if not specified */
	if( param_frame_rate == 0 )
	{
		if(strm_frame_rate_code<1 || strm_frame_rate_code>8)
		{
			mjpeg_error("Input stream with unknown frame-rate and no frame-rate specified with -a!");
			++nerr;
		}
		else
			param_frame_rate = strm_frame_rate_code;
	}

	if( param_norm == 0 && (strm_frame_rate_code==3 || strm_frame_rate_code == 2) )
	{
		mjpeg_info("Assuming norm PAL");
		param_norm = 'p';
	}
	if( param_norm == 0 && (strm_frame_rate_code==4 || strm_frame_rate_code == 1) )
	{
		mjpeg_info("Assuming norm NTSC");
		param_norm = 'n';
	}





	if( param_frame_rate != 0 )
	{
		if( strm_frame_rate_code != param_frame_rate && 
			strm_frame_rate_code > 0 && 
			strm_frame_rate_code < mpeg_num_framerates )
		{
			mjpeg_warn( "Specified display frame-rate %3.2f will over-ride", 
						Y4M_RATIO_DBL(mpeg_framerate(param_frame_rate)));
			mjpeg_warn( "(different!) frame-rate %3.2f of the input stream",
						Y4M_RATIO_DBL(mpeg_framerate(strm_frame_rate_code)));
		}
		opt_frame_rate_code = param_frame_rate;
	}

	if( param_aspect_ratio == 0 )
	{
		param_aspect_ratio = strm_aspect_ratio;
	}

	if( param_aspect_ratio == 0 )
	{
		mjpeg_warn( "No aspect ratio specifed and no guess possible: assuming 4:3 display aspect!");
		param_aspect_ratio = 2;
	}

	/* Convert to MPEG1 coding if we're generating MPEG1 */
	if( param_mpeg == 1 )
	{
		param_aspect_ratio = infer_mpeg1_aspect_code( param_norm, 
													  param_aspect_ratio );
	}

	return nerr;
}

static int check_param_constraints(void)
{
	int nerr = 0;
	if( param_32_pulldown )
	{
		if( param_mpeg == 1 )
			mjpeg_error_exit1( "MPEG-1 cannot encode 3:2 pulldown (for transcoding to VCD set 24fps)!" );

		if( param_frame_rate != 4 && param_frame_rate != 5  )
		{
			if( param_frame_rate == 1 || param_frame_rate == 2 )
			{
				param_frame_rate += 3;
				mjpeg_warn("3:2 movie pulldown with frame rate set to decode rate not display rate");
				mjpeg_warn("3:2 Setting frame rate code to display rate = %d (%2.3f fps)", 
						   param_frame_rate,
						   Y4M_RATIO_DBL(mpeg_framerate(param_frame_rate)));

			}
			else
			{
				mjpeg_error( "3:2 movie pulldown not sensible for %2.3f fps dispay rate",
							Y4M_RATIO_DBL(mpeg_framerate(param_frame_rate)));
				++nerr;
			}
		}
		if( param_fieldenc == 2 )
		{
			mjpeg_error( "3:2 pulldown only possible for frame pictures (-I 1 or -I 0)");
			++nerr;
		}
	}
	


	if(  param_aspect_ratio > mpeg_num_aspect_ratios[param_mpeg-1] ) 
	{
		mjpeg_error("For MPEG-%d aspect ratio code  %d > %d illegal", 
					param_mpeg, param_aspect_ratio, 
					mpeg_num_aspect_ratios[param_mpeg-1]);
		++nerr;
	}
		


	if( param_min_GOP_size > param_max_GOP_size )
	{
		mjpeg_error( "Min GOP size must be <= Max GOP size" );
		++nerr;
	}

	if( param_preserve_B && 
		( param_min_GOP_size % param_Bgrp_size != 0 ||
		  param_max_GOP_size % param_Bgrp_size != 0 )
		)
	{
		mjpeg_error("Preserving I/P frame spacing is impossible if min and max GOP sizes are" );
		mjpeg_error_exit1("Not both divisible by %d", param_Bgrp_size );
	}

	switch( param_format )
	{
	case MPEG_FORMAT_SVCD_STILL :
	case MPEG_FORMAT_SVCD_NSR :
	case MPEG_FORMAT_SVCD : 
		if( param_aspect_ratio != 2 && param_aspect_ratio != 3 )
			mjpeg_error_exit1("SVCD only supports 4:3 and 16:9 aspect ratios");
		if( param_svcd_scan_data )
		{
			mjpeg_warn( "Generating dummy SVCD scan-data offsets to be filled in by \"vcdimager\"");
			mjpeg_warn( "If you're not using vcdimager you may wish to turn this off using -d");
		}
		break;
	}
	return nerr;
}


int main(argc,argv)
	int argc;
	char *argv[];
{
	char *outfilename=0;
	int nerr = 0;
	int n;

	/* Set up error logging.  The initial handling level is LOG_INFO
	 */

static const char	short_options[]=
	"m:a:f:n:b:z:T:B:q:o:S:I:r:M:4:2:Q:D:g:G:v:V:F:tpdsZNhOcCP";

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
     { "force-b-b-p", 0, &param_preserve_B, 1},
     { "quantisation-reduction", 1, 0, 'Q' },
     { "video-buffer",      1, 0, 'V' },
     { "video-norm",        1, 0, 'n' },
     { "sequence-length",   1, 0, 'S' },
     { "3-2-pulldown",      1, &param_32_pulldown, 1 },
     { "keep-hf",           0, &param_hf_quant, 2 },
     { "reduce-hf",         0, &param_hf_quant, 1 },
     { "sequence-header-every-gop", 0, &param_seq_hdr_every_gop, 1},
     { "no-dummy-svcd-SOF", 0, &param_svcd_scan_data, 0 },
     { "correct-svcd-hds", 0, &param_hack_svcd_hds_bug, 0},
     { "no-constraints", 0, &param_ignore_constraints, 0},
     { "no-altscan-mpeg2", 0, &param_hack_altscan_bug, 1},
     { "playback-field-order", 1, 0, 'z'},
     { "multi-thread",      1, 0, 'M' },
     { "help",              0, 0, '?' },
     { 0,                   0, 0, 0 }
};

    while( (n=getopt_long(argc,argv,short_options,long_options, NULL)) != -1 )
#else
    while( (n=getopt(argc,argv,short_options)) != -1)
#endif
	{
		switch(n) {
        case 0 :                /* Flag setting handled by getopt-long */
            break;
		case 'b':
			param_bitrate = atoi(optarg)*1000;
			break;

		case 'T' :
			param_still_size = atoi(optarg)*1024;
			if( param_still_size < 20*1024 || param_still_size > 500*1024 )
			{
				mjpeg_error( "-T requires arg 20..500" );
				++nerr;
			}
			break;

		case 'B':
			param_nonvid_bitrate = atoi(optarg);
			if( param_nonvid_bitrate < 0 )
			{
				mjpeg_error("-B requires arg > 0");
				++nerr;
			}
			break;

        case 'D':
            param_mpeg2_dc_prec = atoi(optarg)-8;
            if( param_mpeg2_dc_prec < 0 || param_mpeg2_dc_prec > 2 )
            {
                mjpeg_error( "-D requires arg [8..10]" );
                ++nerr;
            }
            break;
        case 'C':
            param_hack_svcd_hds_bug = 0;
            break;

		case 'q':
			param_quant = atoi(optarg);
			if(param_quant<1 || param_quant>32)
			{
				mjpeg_error("-q option requires arg 1 .. 32");
				++nerr;
			}
			break;

        case 'a' :
			param_aspect_ratio = atoi(optarg);
            if( param_aspect_ratio == 0 )
				DisplayAspectRatios();
			/* Checking has to come later once MPEG 1/2 has been selected...*/
			if( param_aspect_ratio < 0 )
			{
				mjpeg_error( "-a option must be positive");
				++nerr;
			}
			break;

       case 'F' :
			param_frame_rate = atoi(optarg);
            if( param_frame_rate == 0 )
				DisplayFrameRates();
			if( param_frame_rate < 0 || 
				param_frame_rate >= mpeg_num_framerates)
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
			param_fieldenc = atoi(optarg);
			if( param_fieldenc < 0 || param_fieldenc > 2 )
			{
				mjpeg_error("-I option requires 0,1 or 2");
				++nerr;
			}
			break;

		case 'r':
			param_searchrad = atoi(optarg);
			if(param_searchrad<0 || param_searchrad>32)
			{
				mjpeg_error("-r option requires arg 0 .. 32");
				++nerr;
			}
			break;

		case 'M':
			param_num_cpus = atoi(optarg);
			if(param_num_cpus<0 || param_num_cpus>32)
			{
				mjpeg_error("-M option requires arg 0..32");
				++nerr;
			}
			break;

		case '4':
			param_44_red = atoi(optarg);
			if(param_44_red<0 || param_44_red>4)
			{
				mjpeg_error("-4 option requires arg 0..4");
				++nerr;
			}
			break;
			
		case '2':
			param_22_red = atoi(optarg);
			if(param_22_red<0 || param_22_red>4)
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
			param_video_buffer_size = atoi(optarg);
			if(param_video_buffer_size<20 || param_video_buffer_size>4000)
			{
				mjpeg_error("-v option requires arg 20..4000");
				++nerr;
			}
			break;

		case 'S' :
			param_seq_length_limit = atoi(optarg);
			if(param_seq_length_limit<1 )
			{
				mjpeg_error("-S option requires arg > 1");
				++nerr;
			}
			break;
		case 'p' :
			param_32_pulldown = 1;
			break;

		case 'z' :
			if( strlen(optarg) != 1 || (optarg[0] != 't' && optarg[0] != 'b' ) )
			{
				mjpeg_error("-z option requires arg b or t" );
				++nerr;
			}
			else if( optarg[0] == 't' )
				param_force_interlacing = Y4M_ILACE_TOP_FIRST;
			else if( optarg[0] == 'b' )
				param_force_interlacing = Y4M_ILACE_BOTTOM_FIRST;
			break;

		case 'f' :
			param_format = atoi(optarg);
			if( param_format < MPEG_FORMAT_FIRST ||
				param_format > MPEG_FORMAT_LAST )
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
				param_norm = optarg[0];
				break;
			default :
				mjpeg_error("-n option requires arg n or p, or s.");
				++nerr;
			}
			break;
		case 'g' :
			param_min_GOP_size = atoi(optarg);
			break;
		case 'G' :
			param_max_GOP_size = atoi(optarg);
			break;
        case 'c' :
            param_closed_GOPs = true;
            break;
		case 'P' :
			param_preserve_B = true;
			break;
		case 'N':
			param_hf_quant = 1;
			break;
		case 'h':
			param_hf_quant = 2;
			break;
		case 's' :
			param_seq_hdr_every_gop = 1;
			break;
		case 'd' :
			param_svcd_scan_data = 0;
			break;
		case 'Q' :
			param_act_boost = atof(optarg);
			if( param_act_boost <-4.0 || param_act_boost > 10.0)
			{
				mjpeg_error( "-q option requires arg -4.0 .. 10.0");
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

    if( nerr )
		Usage(argv[0]);

	mjpeg_default_handler_verbosity(verbose);


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

	/* Read parameters inferred from input stream */
	read_stream_params( &opt_horizontal_size, &opt_vertical_size, 
						&strm_frame_rate_code, &param_input_interlacing,
						&strm_aspect_ratio
						);
	
	if(opt_horizontal_size<=0)
	{
		mjpeg_error("Horizontal size from input stream illegal");
		++nerr;
	}
	if(opt_vertical_size<=0)
	{
		mjpeg_error("Vertical size from input stream illegal");
		++nerr;
	}

	
	/* Check parameters that cannot be checked when parsed/read */

	if(!outfilename)
	{
		mjpeg_error("Output file name (-o option) is required!");
		++nerr;
	}

	

	set_format_presets();

	nerr += infer_default_params();

	nerr += check_param_constraints();

	if(nerr) 
	{
		Usage(argv[0]);
	}


	mjpeg_info("Encoding MPEG-%d video to %s",param_mpeg,outfilename);
	mjpeg_info("Horizontal size: %d pel",opt_horizontal_size);
	mjpeg_info("Vertical size: %d pel",opt_vertical_size);
	mjpeg_info("Aspect ratio code: %d = %s", 
			param_aspect_ratio,
			mpeg_aspect_code_definition(param_mpeg,param_aspect_ratio));
	mjpeg_info("Frame rate code:   %d = %s",
			param_frame_rate,
			mpeg_framerate_code_definition(param_frame_rate));

	if(param_bitrate) 
		mjpeg_info("Bitrate: %d KBit/s",param_bitrate/1000);
	else
		mjpeg_info( "Bitrate: VCD");
	if(param_quant) 
		mjpeg_info("Quality factor: %d (Quantisation = %d) (1=best, 31=worst)",
                   param_quant, 
                   (int)(inv_scale_quant( param_mpeg == 1 ? 0 : 1, 
                                          param_quant))
            );

	mjpeg_info("Field order for input: %s", 
			   mpeg_interlace_code_definition(param_input_interlacing) );

	if( param_seq_length_limit )
	{
		mjpeg_info( "New Sequence every %d Mbytes", param_seq_length_limit );
		mjpeg_info( "Assuming non-video stream of %d Kbps", param_nonvid_bitrate );
	}
	else
		mjpeg_info( "Sequence unlimited length" );

	mjpeg_info("Search radius: %d",param_searchrad);

	/* set params */
	init_mpeg_parms();

	/* read quantization matrices */
	init_quantmat();

	/* open output file */
	if (!(outfile=fopen(outfilename,"wb")))
	{
		mjpeg_error_exit1("Couldn't create output file %s",outfilename);
	}

	init_encoder();
	init_quantizer();
	init_motion();
	init_transform();
	init_predict();
	putseq();

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
	Wrapper for malloc that allocates pbuffers aligned to the 
	specified byte boundary and checks for failure.
	N.b.  don't try to free the resulting pointers, eh...
*/
uint8_t *bufalloc( size_t size )
{
	char *buf = malloc( size + BUFFER_ALIGN );
	unsigned long adjust;

	if( buf == NULL )
	{
		mjpeg_error_exit1("malloc failed");
	}
	adjust = BUFFER_ALIGN-((unsigned long)buf)%BUFFER_ALIGN;
	if( adjust == BUFFER_ALIGN )
		adjust = 0;
	return (uint8_t*)(buf+adjust);
}

/*********************
 *
 * Mark the border so that blocks in the frame are unlikely
 * to match blocks outside the frame.  This is useful for
 * motion estimation routines that, for speed, are a little
 * sloppy about some of the candidates they consider.
 *
 ********************/

static void border_mark( uint8_t *frame,
                         int w1, int h1, int w2, int h2)
{
  int i, j;
  uint8_t *fp;
  uint8_t mask = 0xff;
  /* horizontal pixel replication (right border) */
 
  for (j=0; j<h1; j++)
  {
    fp = frame + j*w2;
    for (i=w1; i<w2; i++)
    {
      fp[i] = mask;
      mask ^= 0xff;
    }
  }
 
  /* vertical pixel replication (bottom border) */

  for (j=h1; j<h2; j++)
  {
    fp = frame + j*w2;
    for (i=0; i<w2; i++)
    {
        fp[i] = mask;
        mask ^= 0xff;
    }
  }
}


static void init_encoder(void)
{
	int i, n;
	static int block_count_tab[3] = {6,8,12};
    int enc_chrom_width, enc_chrom_height;
	initbits(); 
	init_fdct();
	init_idct();

	/* Tune threading and motion compensation for specified number of CPU's 
	   and specified speed parameters.
	 */
    
	ctl_act_boost = param_act_boost >= 0.0 
        ? (param_act_boost+1.0)
        : (param_act_boost-1.0);
	switch( param_num_cpus )
	{

	case 0 : /* Special case for debugging... turns of multi-threading */
		ctl_max_encoding_frames = 1;
		ctl_refine_from_rec = true;
		ctl_parallel_read = false;
		break;

	case 1 :
		ctl_max_encoding_frames = 1;
		ctl_refine_from_rec = true;
		ctl_parallel_read = true;
		break;
	case 2:
		ctl_max_encoding_frames = 2;
		ctl_refine_from_rec = true;
		ctl_parallel_read = true;
		break;
	default :
		ctl_max_encoding_frames = param_num_cpus > MAX_WORKER_THREADS-1 ?
			                  MAX_WORKER_THREADS-1 :
			                  param_num_cpus;
		ctl_refine_from_rec = false;
		ctl_parallel_read = true;
		break;
	}

	ctl_44_red		= param_44_red;
	ctl_22_red		= param_22_red;
	
	/* round picture dimensions to nearest multiple of 16 or 32 */
	mb_width = (opt_horizontal_size+15)/16;
	mb_height = opt_prog_seq ? (opt_vertical_size+15)/16 : 2*((opt_vertical_size+31)/32);
	mb_height2 = opt_fieldpic ? mb_height>>1 : mb_height; /* for field pictures */
	opt_enc_width = 16*mb_width;
	opt_enc_height = 16*mb_height;

#ifdef HAVE_ALTIVEC
	/* Pad opt_phy_width to 64 so that the rowstride of 4*4
	 * sub-sampled data will be a multiple of 16 (ideal for AltiVec)
	 * and the rowstride of 2*2 sub-sampled data will be a multiple
	 * of 32. Height does not affect rowstride, no padding needed.
	 */
	opt_phy_width = (opt_enc_width + 63) & (~63);
#else
	opt_phy_width = opt_enc_width;
#endif
	opt_phy_height = opt_enc_height;

	/* Calculate the sizes and offsets in to luminance and chrominance
	   buffers.  A.Stevens 2000 for luminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel
	   sub-sampled uint8_t followed by 4*4 sub-sampled.  We add an
	   extra row to act as a margin to allow us to neglect / postpone
	   edge condition checking in time-critical loops...  */

	opt_phy_chrom_width = (opt_chroma_format==CHROMA444) 
        ? opt_phy_width 
        : opt_phy_width>>1;
	opt_phy_chrom_height = (opt_chroma_format!=CHROMA420) 
        ? opt_phy_height 
        : opt_phy_height>>1;
	enc_chrom_width = (opt_chroma_format==CHROMA444) 
        ? opt_enc_width 
        : opt_enc_width>>1;
	enc_chrom_height = (opt_chroma_format!=CHROMA420) 
        ? opt_enc_height 
        : opt_enc_height>>1;

	opt_phy_height2 = opt_fieldpic ? opt_phy_height>>1 : opt_phy_height;
	opt_enc_height2 = opt_fieldpic ? opt_enc_height>>1 : opt_enc_height;
	opt_phy_width2 = opt_fieldpic ? opt_phy_width<<1 : opt_phy_width;
	opt_phy_chrom_width2 = opt_fieldpic 
        ? opt_phy_chrom_width<<1 
        : opt_phy_chrom_width;
 
	block_count = block_count_tab[opt_chroma_format-1];
	lum_buffer_size = (opt_phy_width*opt_phy_height) +
					 sizeof(uint8_t) *(opt_phy_width/2)*(opt_phy_height/2) +
					 sizeof(uint8_t) *(opt_phy_width/4)*(opt_phy_height/4+1);
	chrom_buffer_size = opt_phy_chrom_width*opt_phy_chrom_height;


	fsubsample_offset = (opt_phy_width)*(opt_phy_height) * sizeof(uint8_t);
	qsubsample_offset =  fsubsample_offset 
        + (opt_phy_width/2)*(opt_phy_height/2)*sizeof(uint8_t);

	mb_per_pict = mb_width*mb_height2;


	/* clip table */
	if (!(clp_0_255 = (uint8_t *)malloc(1024)))
		mjpeg_error_exit1("malloc failed");
	clp_0_255 += 384;
	for (i=-384; i<640; i++)
		clp_0_255[i] = (i<0) ? 0 : ((i>255) ? 255 : i);
	
	/* Allocate the frame data buffers */
    frame_buffer_size = 2*param_max_GOP_size+param_Bgrp_size+READ_CHUNK_SIZE+1;
    mjpeg_info( "Buffering %d frames", frame_buffer_size );
	frame_buffers = (uint8_t ***) 
		bufalloc(frame_buffer_size*sizeof(uint8_t**));
	
	for(n=0;n<frame_buffer_size;n++)
	{
         frame_buffers[n] = (uint8_t **) bufalloc(3*sizeof(uint8_t*));
		 for (i=0; i<3; i++)
		 {
			 frame_buffers[n][i] = 
				 bufalloc( (i==0) ? lum_buffer_size : chrom_buffer_size );
		 }

         border_mark(frame_buffers[n][0],
                     opt_enc_width,opt_enc_height,
                     opt_phy_width,opt_phy_height);
         border_mark(frame_buffers[n][1],
                     enc_chrom_width, enc_chrom_height,
                     opt_phy_chrom_width,opt_phy_chrom_height);
         border_mark(frame_buffers[n][2],
                     enc_chrom_width, enc_chrom_height,
                     opt_phy_chrom_width,opt_phy_chrom_height);

	}
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

static void init_mpeg_parms(void)
{
	int i;
    const char *msg;

	inputtype = 0;  /* doesnt matter */
	istrm_nframes = 999999999; /* determined by EOF of stdin */

	ctl_N_min = param_min_GOP_size;      /* I frame distance */
	ctl_N_max = param_max_GOP_size;
    ctl_closed_GOPs = param_closed_GOPs;
	mjpeg_info( "GOP SIZE RANGE %d TO %d %s", 
                ctl_N_min, ctl_N_max,
                ctl_closed_GOPs ? "(all GOPs closed)" : "" 
                );
	ctl_M = param_Bgrp_size;             /* I or P frame distance */
	ctl_M_min = param_preserve_B ? ctl_M : 1;
	if( ctl_M >= ctl_N_min )
		ctl_M = ctl_N_min-1;
	opt_mpeg1           = (param_mpeg == 1);
	opt_fieldpic        = (param_fieldenc == 2);

    // SVCD and probably DVD? mandate progressive_sequence = 0 
    switch( param_format )
    {
    case MPEG_FORMAT_SVCD :
    case MPEG_FORMAT_SVCD_NSR :
    case MPEG_FORMAT_SVCD_STILL :
    case MPEG_FORMAT_DVD :
        opt_prog_seq = 0;
        break;
    default :
        opt_prog_seq        = (param_mpeg == 1 || param_fieldenc == 0);
        break;
    }
	opt_pulldown_32     = param_32_pulldown;

	opt_aspectratio     = param_aspect_ratio;
	opt_frame_rate_code = param_frame_rate;
	opt_dctsatlim		= opt_mpeg1 ? 255 : 2047;

	/* If we're using a non standard (VCD?) profile bit-rate adjust	the vbv
		buffer accordingly... */

	if( param_bitrate == 0 )
	{
		mjpeg_error_exit1( "Generic format - must specify bit-rate!" );
	}

	opt_still_size = 0;
	if( MPEG_STILLS_FORMAT(param_format) )
	{
		opt_vbv_buffer_code = param_vbv_buffer_still_size / 2048;
		opt_vbv_buffer_still_size = param_pad_stills_to_vbv_buffer_size;
		opt_bit_rate = param_bitrate;
		opt_still_size = param_still_size;
	}
	else if( param_mpeg == 1 )
	{
		/* Scale VBV relative to VCD  */
		opt_bit_rate = MAX(10000, param_bitrate);
		opt_vbv_buffer_code = (20 * param_bitrate  / 1151929);
	}
	else
	{
		opt_bit_rate = MAX(10000, param_bitrate);
		opt_vbv_buffer_code = MIN(112,param_video_buffer_size / 2);
	}
	opt_vbv_buffer_size = opt_vbv_buffer_code*16384;

	if( param_quant )
	{
		ctl_quant_floor = inv_scale_quant( param_mpeg == 1 ? 0 : 1, 
                                           param_quant );
	}
	else
	{
		ctl_quant_floor = 0.0;		/* Larger than max quantisation */
	}

	ctl_video_buffer_size = param_video_buffer_size * 1024 * 8;
	
	opt_seq_hdr_every_gop = param_seq_hdr_every_gop;
	opt_seq_end_every_gop = param_seq_end_every_gop;
	opt_svcd_scan_data = param_svcd_scan_data;
	opt_ignore_constraints = param_ignore_constraints;
	ctl_seq_length_limit = param_seq_length_limit;
	ctl_nonvid_bit_rate = param_nonvid_bitrate * 1000;
	opt_low_delay       = 0;
	opt_constrparms     = (param_mpeg == 1 && 
						   !MPEG_STILLS_FORMAT(param_format));
	opt_profile         = param_422 ? 1 : 4; /* High or Main profile resp. */
	opt_level           = 8;                 /* Main Level      CCIR 601 rates */
	opt_chroma_format   = param_422 ? CHROMA422 : CHROMA420;
	switch(param_norm)
	{
	case 'p': opt_video_format = 1; break;
	case 'n': opt_video_format = 2; break;
	case 's': opt_video_format = 3; break;
	default:  opt_video_format = 5; break; /* unspec. */
	}
	switch(param_norm)
	{
	case 's':
	case 'p':  /* ITU BT.470  B,G */
		opt_color_primaries = 5;
		opt_transfer_characteristics = 5; /* Gamma = 2.8 (!!) */
		opt_matrix_coefficients = 5; 
        msg = "PAL B/G";
		break;
	case 'n': /* SMPTPE 170M "modern NTSC" */
		opt_color_primaries = 6;
		opt_matrix_coefficients = 6; 
		opt_transfer_characteristics = 6;
        msg = "NTSC";
		break; 
	default:   /* unspec. */
		opt_color_primaries = 2;
		opt_matrix_coefficients = 2; 
		opt_transfer_characteristics = 2;
        msg = "unspecified";
		break;
	}
    mjpeg_info( "Setting colour/gamma parameters to \"%s\"", msg);

	switch( param_format )
	{
	case MPEG_FORMAT_SVCD_STILL :
	case MPEG_FORMAT_SVCD_NSR :
	case MPEG_FORMAT_SVCD :
    case MPEG_FORMAT_DVD :
        /* It would seem DVD and perhaps SVCD demand a 540 pixel display size
           for 4:3 aspect video. However, many players expect 480 and go weird
           if this isn't set...
        */
        if( param_hack_svcd_hds_bug )
        {
            opt_display_horizontal_size  = opt_horizontal_size;
            opt_display_vertical_size    = opt_vertical_size;
        }
        else
        {
            opt_display_horizontal_size  = opt_aspectratio == 2 ? 540 : 720;
            opt_display_vertical_size    = opt_vertical_size;
        }
		break;
	default:
		opt_display_horizontal_size  = opt_horizontal_size;
		opt_display_vertical_size    = opt_vertical_size;
		break;
	}

	opt_dc_prec         = param_mpeg2_dc_prec;  /* 9 bits */
    opt_topfirst = 0;
	if( ! opt_prog_seq )
	{
		int fieldorder;
		if( param_force_interlacing != Y4M_UNKNOWN ) 
		{
			mjpeg_info( "Forcing playback video to be: %s",
						mpeg_interlace_code_definition(	param_force_interlacing ) );	
			fieldorder = param_force_interlacing;
		}
		else
			fieldorder = param_input_interlacing;

		opt_topfirst = (fieldorder == Y4M_ILACE_TOP_FIRST || 
						fieldorder ==Y4M_ILACE_NONE );
	}
	else
		opt_topfirst = 0;

	opt_frame_pred_dct_tab[0] 
		= opt_frame_pred_dct_tab[1] 
		= opt_frame_pred_dct_tab[2] 
        = (param_mpeg == 1 || param_fieldenc == 0) ? 1 : 0;

    mjpeg_info( "Progressive format frames = %d", 	opt_frame_pred_dct_tab[0] );
	opt_qscale_tab[0] 
		= opt_qscale_tab[1] 
		= opt_qscale_tab[2] 
		= param_mpeg == 1 ? 0 : 1;

	opt_intravlc_tab[0] 
		= opt_intravlc_tab[1] 
		= opt_intravlc_tab[2] 
		= param_mpeg == 1 ? 0 : 1;

	opt_altscan_tab[2]  
		= opt_altscan_tab[1]  
		= opt_altscan_tab[0]  
		= (param_mpeg == 1 || param_hack_altscan_bug) ? 0 : 1;
	

	/*  A.Stevens 2000: The search radius *has* to be a multiple of 8
		for the new fast motion compensation search to work correctly.
		We simply round it up if needs be.  */

	if(param_searchrad*ctl_M>127)
	{
		param_searchrad = 127/ctl_M;
		mjpeg_warn("Search radius reduced to %d",param_searchrad);
	}
	
	{ 
		int radius_x = param_searchrad;
		int radius_y = param_searchrad*opt_vertical_size/opt_horizontal_size;

		/* TODO: These f-codes should really be adjusted for each
		   picture type... */

		opt_motion_data = (struct motion_data *)malloc(ctl_M*sizeof(struct motion_data));
		if (!opt_motion_data)
			mjpeg_error_exit1("malloc failed");

		for (i=0; i<ctl_M; i++)
		{
			if(i==0)
			{
				opt_motion_data[i].sxf = round_search_radius(radius_x*ctl_M);
				opt_motion_data[i].forw_hor_f_code  = f_code(opt_motion_data[i].sxf);
				opt_motion_data[i].syf = round_search_radius(radius_y*ctl_M);
				opt_motion_data[i].forw_vert_f_code  = f_code(opt_motion_data[i].syf);
			}
			else
			{
				opt_motion_data[i].sxf = round_search_radius(radius_x*i);
				opt_motion_data[i].forw_hor_f_code  = f_code(opt_motion_data[i].sxf);
				opt_motion_data[i].syf = round_search_radius(radius_y*i);
				opt_motion_data[i].forw_vert_f_code  = f_code(opt_motion_data[i].syf);
				opt_motion_data[i].sxb = round_search_radius(radius_x*(ctl_M-i));
				opt_motion_data[i].back_hor_f_code  = f_code(opt_motion_data[i].sxb);
				opt_motion_data[i].syb = round_search_radius(radius_y*(ctl_M-i));
				opt_motion_data[i].back_vert_f_code  = f_code(opt_motion_data[i].syb);
			}

			/* MPEG-1 demands f-codes for vertical and horizontal axes are
			   identical!!!!
			*/
			if( opt_mpeg1 )
			{
				opt_motion_data[i].syf = opt_motion_data[i].sxf;
				opt_motion_data[i].syb  = opt_motion_data[i].sxb;
				opt_motion_data[i].forw_vert_f_code  = 
					opt_motion_data[i].forw_hor_f_code;
				opt_motion_data[i].back_vert_f_code  = 
					opt_motion_data[i].back_hor_f_code;
				
			}
		}
		
	}
	


	/* make sure MPEG specific parameters are valid */
	range_checks();

	/* Set the frame decode rate and frame display rates.
	   For 3:2 movie pulldown decode rate is != display rate due to
	   the repeated field that appears every other frame.
	*/
	opt_frame_rate = Y4M_RATIO_DBL(mpeg_framerate(opt_frame_rate_code));
	if( param_32_pulldown )
	{
		ctl_decode_frame_rate = opt_frame_rate * (2.0 + 2.0) / (3.0 + 2.0);
		mjpeg_info( "3:2 Pulldown selected frame decode rate = %3.3f fps", 
					ctl_decode_frame_rate);
	}
	else
		ctl_decode_frame_rate = opt_frame_rate;

	if ( !opt_mpeg1)
	{
		profile_and_level_checks();
	}
	else
	{
		/* MPEG-1 */
		if (opt_constrparms)
		{
			if (opt_horizontal_size>768
				|| opt_vertical_size>576
				|| ((opt_horizontal_size+15)/16)*((opt_vertical_size+15)/16)>396
				|| ((opt_horizontal_size+15)/16)*((opt_vertical_size+15)/16)*opt_frame_rate>396*25.0
				|| opt_frame_rate>30.0)
			{
				mjpeg_info( "size - setting constrained_parameters_flag = 0");
				opt_constrparms = 0;
			}
		}

		if (opt_constrparms)
		{
			for (i=0; i<ctl_M; i++)
			{
				if (opt_motion_data[i].forw_hor_f_code>4)
				{
					mjpeg_info("Hor. motion search forces constrained_parameters_flag = 0");
					opt_constrparms = 0;
					break;
				}

				if (opt_motion_data[i].forw_vert_f_code>4)
				{
					mjpeg_info("Ver. motion search forces constrained_parameters_flag = 0");
					opt_constrparms = 0;
					break;
				}

				if (i!=0)
				{
					if (opt_motion_data[i].back_hor_f_code>4)
					{
						mjpeg_info("Hor. motion search setting constrained_parameters_flag = 0");
						opt_constrparms = 0;
						break;
					}

					if (opt_motion_data[i].back_vert_f_code>4)
					{
						mjpeg_info("Ver. motion search setting constrained_parameters_flag = 0");
						opt_constrparms = 0;
						break;
					}
				}
			}
		}
	}

	/* relational checks */
	if ( opt_mpeg1 )
	{
		if (!opt_prog_seq)
		{
			mjpeg_warn("opt_mpeg1 specified - setting progressive_sequence = 1");
			opt_prog_seq = 1;
		}

		if (opt_chroma_format!=CHROMA420)
		{
			mjpeg_info("mpeg1 - forcing chroma_format = 1 (4:2:0) - others not supported");
			opt_chroma_format = CHROMA420;
		}

		if (opt_dc_prec!=0)
		{
			mjpeg_info("mpeg1 - setting intra_dc_precision = 0");
			opt_dc_prec = 0;
		}

		for (i=0; i<3; i++)
			if (opt_qscale_tab[i])
			{
				mjpeg_info("mpeg1 - setting qscale_tab[%d] = 0",i);
				opt_qscale_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (opt_intravlc_tab[i])
			{
				mjpeg_info("mpeg1 - setting intravlc_tab[%d] = 0",i);
				opt_intravlc_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (opt_altscan_tab[i])
			{
				mjpeg_info("mpeg1 - setting altscan_tab[%d] = 0",i);
				opt_altscan_tab[i] = 0;
			}
	}

	if ( !opt_mpeg1 && opt_constrparms)
	{
		mjpeg_info("not mpeg1 - setting constrained_parameters_flag = 0");
		opt_constrparms = 0;
	}


	if( (!opt_prog_seq || opt_fieldpic != 0 ) &&
		( (opt_vertical_size+15) / 16)%2 != 0 )
	{
		mjpeg_warn( "Frame height won't split into two equal field pictures...");
		mjpeg_warn( "forcing encoding as progressive video");
		opt_prog_seq = 1;
		opt_fieldpic = 0;
	}


	if (opt_prog_seq && opt_fieldpic != 0)
	{
		mjpeg_info("prog sequence - forcing progressive frame encoding");
		opt_fieldpic = 0;
	}


	if (opt_prog_seq && opt_topfirst)
	{
		mjpeg_info("prog sequence setting top_field_first = 0");
		opt_topfirst = 0;
	}

	/* search windows */
	for (i=0; i<ctl_M; i++)
	{
		if (opt_motion_data[i].sxf > (4<<opt_motion_data[i].forw_hor_f_code)-1)
		{
			mjpeg_info(
				"reducing forward horizontal search width to %d",
						(4<<opt_motion_data[i].forw_hor_f_code)-1);
			opt_motion_data[i].sxf = (4<<opt_motion_data[i].forw_hor_f_code)-1;
		}

		if (opt_motion_data[i].syf > (4<<opt_motion_data[i].forw_vert_f_code)-1)
		{
			mjpeg_info(
				"reducing forward vertical search width to %d",
				(4<<opt_motion_data[i].forw_vert_f_code)-1);
			opt_motion_data[i].syf = (4<<opt_motion_data[i].forw_vert_f_code)-1;
		}

		if (i!=0)
		{
			if (opt_motion_data[i].sxb > (4<<opt_motion_data[i].back_hor_f_code)-1)
			{
				mjpeg_info(
					"reducing backward horizontal search width to %d",
					(4<<opt_motion_data[i].back_hor_f_code)-1);
				opt_motion_data[i].sxb = (4<<opt_motion_data[i].back_hor_f_code)-1;
			}

			if (opt_motion_data[i].syb > (4<<opt_motion_data[i].back_vert_f_code)-1)
			{
				mjpeg_info(
					"reducing backward vertical search width to %d",
					(4<<opt_motion_data[i].back_vert_f_code)-1);
				opt_motion_data[i].syb = (4<<opt_motion_data[i].back_vert_f_code)-1;
			}
		}
	}

}

/*
  If the use has selected suppression of hf noise via
  quantisation then we boost quantisation of hf components
  EXPERIMENTAL: currently a linear ramp from 0 at 4pel to 
  50% increased quantisation...
*/

static int quant_hfnoise_filt(int orgquant, int qmat_pos )
{
	int orgdist = intmax(qmat_pos % 8, qmat_pos/8);
	int qboost = 1024;

	if( param_hf_quant != 1)
	{
		return orgquant;
	}

	/* Maximum 150% quantisation boost for HF components... */
	qboost = 256+(384/8)*orgdist;


	return (orgquant * qboost)/ 256;
}

static void init_quantmat(void)
{
	int i,v, q;
	opt_load_iquant = 0;
	opt_load_niquant = 0;

	/* bufalloc to ensure alignment */
	opt_intra_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));
	opt_inter_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));
	i_intra_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));
	i_inter_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));

	if( param_hf_quant == 2)
	{
		opt_load_iquant |= 1;
		mjpeg_info( "Setting hi-res intra Quantisation matrix" );
		for (i=0; i<64; i++)
		{
			opt_intra_q[i] = hires_intra_quantizer_matrix[i];
		}	
	}
	else
	{
		/* use default intra matrix */
		opt_load_iquant = (param_hf_quant == 1);
		for (i=0; i<64; i++)
		{
			v = quant_hfnoise_filt( default_intra_quantizer_matrix[i], i);
			if (v<1 || v>255)
				mjpeg_error_exit1("value in intra quant matrix invalid (after noise filt adjust)");
				opt_intra_q[i] = v;
				
		} 
	}
	
    /* MPEG default non-intra matrix is all 16's. For *our* default we
	    use something more suitable for domestic analog
	    sources... which is non-standard...
	*/
	if( param_hf_quant == 2 )
	{
		mjpeg_info( "Setting hi-res non-intra quantiser matrix" );
		for (i=0; i<64; i++)
		{
			opt_inter_q[i] = hires_nonintra_quantizer_matrix[i];
		}	
	}
	else
	{
		opt_load_niquant |= (param_hf_quant == 1);
		for (i=0; i<64; i++)
		{
			v = quant_hfnoise_filt(default_nonintra_quantizer_matrix[i],i);
			if (v<1 || v>255)
				mjpeg_error_exit1("value in non-intra quant matrix invalid (after noise filt adjust)");
			opt_inter_q[i] = v;
		}
	}

	/* TODO: Inv Quant matrix initialisation should check if the
	 * fraction fits in 16 bits! */
  
	for (i=0; i<64; i++)
	{
		i_intra_q[i] = (int)(((double)IQUANT_SCALE) / ((double)opt_intra_q[i]));
		i_inter_q[i] = (int)(((double)IQUANT_SCALE) / ((double)opt_inter_q[i]));
	}

	
	for( q = 1; q <= 112; ++q )
	{
		for (i=0; i<64; i++)
		{
			intra_q_tbl[q][i] = opt_intra_q[i] * q;
			inter_q_tbl[q][i] = opt_inter_q[i] * q;
			intra_q_tblf[q][i] = (float)intra_q_tbl[q][i];
			inter_q_tblf[q][i] = (float)inter_q_tbl[q][i];
			i_intra_q_tblf[q][i] = 1.0f/ ( intra_q_tblf[q][i] * 0.98);
			i_intra_q_tbl[q][i] = (IQUANT_SCALE/intra_q_tbl[q][i]);
			i_inter_q_tblf[q][i] =  1.0f/ (inter_q_tblf[q][i] * 0.98);
			i_inter_q_tbl[q][i] = (IQUANT_SCALE/inter_q_tbl[q][i] );
		}
	}
  
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
