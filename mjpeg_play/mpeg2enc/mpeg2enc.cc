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
#include "predict_ref.h"
#include "transfrm_ref.h"
#include "quantize_ref.h"
#include "format_codes.h"
#include "mpegconsts.h"
#include "fastintfns.h"
#ifdef HAVE_ALTIVEC
/* needed for ALTIVEC_BENCHMARK and print_benchmark_statistics() */
#include "../utils/altivec/altivec_conf.h"
#endif
int verbose = 1;

#include "ratectl.hh"

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
   defaults etc after parsing.  The resulting values then set the encdims->
   variables that control the actual encoder.
*/

static int param_format = MPEG_FORMAT_MPEG1;
static int param_bitrate    = 0;
static int param_nonvid_bitrate = 0;
static int param_quant      = 0;
static int param_searchrad  = 16;
static int param_mpeg       = 1;
static unsigned int param_aspect_ratio = 0;
static unsigned int param_frame_rate  = 0;
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
static double param_hf_q_boost = 0.0;
static double param_act_boost = 0.0;
static double param_boost_var_ceil = 10*10;
static int param_video_buffer_size = 0;
static int param_seq_length_limit = 0;
static int param_min_GOP_size = -1;
static int param_max_GOP_size = -1;
static int param_closed_GOPs = 0;
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
static unsigned int param_input_interlacing;
static int param_hack_svcd_hds_bug = 1;
static int param_hack_altscan_bug = 0;
static int param_mpeg2_dc_prec = 1;
static int param_ignore_constraints = 0;
static int param_unit_coeff_elim = 0;

/* Input Stream parameter values that have to be further processed to
   set encoding options */

static mpeg_aspect_code_t strm_aspect_ratio;
static unsigned int strm_frame_rate_code;


static uint16_t custom_intra_quantizer_matrix[64];
static uint16_t custom_nonintra_quantizer_matrix[64];

struct EncoderParams encparams;

static void DisplayFrameRates(void)
{
 	unsigned int i;
	printf("Frame-rate codes:\n");
	for( i = 0; i < mpeg_num_framerates; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_framerate_code_definition(i));
	}
	exit(0);
}

static void DisplayAspectRatios(void)
{
 	unsigned int i;
	printf("\nDisplay aspect ratio codes:\n");
	for( i = 1; i <= mpeg_num_aspect_ratios[1]; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_aspect_code_definition(2,i));
	}
	exit(0);
}

static int 
parse_custom_matrixfile(char *fname, int dbug)
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

static void
parse_custom_option(char *arg)
    {

    if	(strcmp(arg, "kvcd") == 0)
    	param_hf_quant = 3;
    else if (strcmp(arg, "hi-res") == 0)
    	param_hf_quant = 2;
    else if (strcmp(arg, "default") == 0)
    	{
    	param_hf_quant = 0;
    	param_hf_q_boost = 0;
    	}
    else if (strcmp(arg, "tmpgenc") == 0)
    	param_hf_quant = 4;
    else if (strncasecmp(arg, "file=", 5) == 0)
    	{
    	if  (parse_custom_matrixfile(arg + 5, arg[0] == 'F' ? 1 : 0) == 0)
    	    param_hf_quant = 5;
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
    return;
    }

static void Usage(char *str)
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
		
		if( encparams.horizontal_size == 352 && 
			(encparams.vertical_size == 240 || encparams.vertical_size == 288 ) )
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
		else if( encparams.horizontal_size == 704 &&
				 (encparams.vertical_size == 480 || encparams.vertical_size == 576) )
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
		
		if( encparams.horizontal_size == 480 && 
			(encparams.vertical_size == 480 || encparams.vertical_size == 576 ) )
		{
			mjpeg_info( "SVCD normal-resolution stills selected." );
			if( param_still_size == 0 )
				param_still_size = 90*1024;
		}
		else if( encparams.horizontal_size == 704 &&
				 (encparams.vertical_size == 480 || encparams.vertical_size == 576) )
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
		encparams.frame_rate_code = param_frame_rate;
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

    if( param_preserve_B && param_Bgrp_size == 0 )
    {
		mjpeg_error_exit1("Preserving I/P frame spacing is impossible for still encoding" );
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


int main( int argc,	char *argv[] )
{
	char *outfilename=0;
	int nerr = 0;
	int n;

	/* Set up error logging.  The initial handling level is LOG_INFO
	 */

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
     { "force-b-b-p", 0, &param_preserve_B, 1},
     { "quantisation-reduction", 1, 0, 'Q' },
     { "quant-reduction-max-var", 1, 0, 'X' },
     { "video-buffer",      1, 0, 'V' },
     { "video-norm",        1, 0, 'n' },
     { "sequence-length",   1, 0, 'S' },
     { "3-2-pulldown",      1, &param_32_pulldown, 1 },
     { "keep-hf",           0, 0, 'H' },
     { "reduce-hf",         1, 0, 'N' },
     { "sequence-header-every-gop", 0, &param_seq_hdr_every_gop, 1},
     { "no-dummy-svcd-SOF", 0, &param_svcd_scan_data, 0 },
     { "correct-svcd-hds", 0, &param_hack_svcd_hds_bug, 0},
     { "no-constraints", 0, &param_ignore_constraints, 1},
     { "no-altscan-mpeg2", 0, &param_hack_altscan_bug, 1},
     { "playback-field-order", 1, 0, 'z'},
     { "multi-thread",      1, 0, 'M' },
     { "custom-quant-matrices", 1, 0, 'K'},
     { "unit-coeff-elim",   1, 0, 'E'},
     { "b-per-refframe",           1, 0, 'R' },
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
            param_hf_q_boost = atof(optarg);
            if (param_hf_q_boost <0.0 || param_hf_q_boost > 2.0)
            {
                mjpeg_error( "-N option requires arg 0.0 .. 2.0" );
                ++nerr;
                param_hf_q_boost = 0.0;
            }
			if (param_hf_quant == 0 && param_hf_q_boost != 0.0)
			   param_hf_quant = 1;
			break;
		case 'H':
			param_hf_quant = 2;
            		break;
		case 'K':
			parse_custom_option(optarg);
			break;
        case 'E':
            param_unit_coeff_elim = atoi(optarg);
            if (param_unit_coeff_elim < -40 || param_unit_coeff_elim > 40)
            {
                mjpeg_error( "-E option range arg -40 to 40" );
                ++nerr;
            }
            break;
        case 'R' :
            param_Bgrp_size = atoi(optarg)+1;
            if( param_Bgrp_size<1 || param_Bgrp_size>3)
            {
                mjpeg_error( "-R option arg 0|1|2" );
                ++nerr;
            }
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
		case 'X' :
			param_boost_var_ceil = atof(optarg);
			if( param_boost_var_ceil <0 || param_boost_var_ceil > 50*50 )
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
	read_stream_params( &encparams.horizontal_size, &encparams.vertical_size, 
						&strm_frame_rate_code, &param_input_interlacing,
						&strm_aspect_ratio
						);
	
	if(encparams.horizontal_size<=0)
	{
		mjpeg_error("Horizontal size from input stream illegal");
		++nerr;
	}
	if(encparams.vertical_size<=0)
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
	mjpeg_info("Horizontal size: %d pel",encparams.horizontal_size);
	mjpeg_info("Vertical size: %d pel",encparams.vertical_size);
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
	init_quantizer( static_cast<int>(encparams.mpeg1), encparams.intra_q, encparams.inter_q );
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
void *bufalloc( size_t size )
{
	uint8_t *buf = static_cast<uint8_t *>(malloc( size + BUFFER_ALIGN ));
	unsigned long adjust;

	if( buf == NULL )
	{
		mjpeg_error_exit1("malloc failed");
	}
	adjust = BUFFER_ALIGN-((unsigned long)buf)%BUFFER_ALIGN;
	if( adjust == BUFFER_ALIGN )
		adjust = 0;
	return (void *)(buf+adjust);
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
	int i;
    unsigned int n;
    int enc_chrom_width, enc_chrom_height;
	initbits(); 

	/* Tune threading and motion compensation for specified number of CPU's 
	   and specified speed parameters.
	 */
    
	ctl_act_boost = param_act_boost >= 0.0 
        ? (param_act_boost+1.0)
        : (param_act_boost-1.0);
    ctl_boost_var_ceil = param_boost_var_ceil;
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

    ctl_max_active_ref_frames = 
        ctl_M == 0 ? ctl_max_encoding_frames : (ctl_max_encoding_frames+2);
    ctl_max_active_b_frames = 
        ctl_M <= 1 ? 0 : ctl_max_encoding_frames+1;

	ctl_44_red		= param_44_red;
	ctl_22_red		= param_22_red;

    ctl_unit_coeff_elim	= param_unit_coeff_elim;

	/* round picture dimensions to nearest multiple of 16 or 32 */
	encparams.mb_width = (encparams.horizontal_size+15)/16;
	encparams.mb_height = encparams.prog_seq ? (encparams.vertical_size+15)/16 : 2*((encparams.vertical_size+31)/32);
	encparams.mb_height2 = encparams.fieldpic ? encparams.mb_height>>1 : encparams.mb_height; /* for field pictures */
	encparams.enc_width = 16*encparams.mb_width;
	encparams.enc_height = 16*encparams.mb_height;

#ifdef DEBUG_MOTION_EST
    static const int MARGIN = 64;
#else
    static const int MARGIN = 0;
#endif
    
#ifdef HAVE_ALTIVEC
	/* Pad encparams.phy_width to 64 so that the rowstride of 4*4
	 * sub-sampled data will be a multiple of 16 (ideal for AltiVec)
	 * and the rowstride of 2*2 sub-sampled data will be a multiple
	 * of 32. Height does not affect rowstride, no padding needed.
	 */
	encparams.phy_width = (encparams.enc_width + 63) & (~63);
#else
	encparams.phy_width = encparams.enc_width+MARGIN;
#endif
	encparams.phy_height = encparams.enc_height+MARGIN;

	/* Calculate the sizes and offsets in to luminance and chrominance
	   buffers.  A.Stevens 2000 for luminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel
	   sub-sampled uint8_t followed by 4*4 sub-sampled.  We add an
	   extra row to act as a margin to allow us to neglect / postpone
	   edge condition checking in time-critical loops...  */

	encparams.phy_chrom_width = (CHROMA420==CHROMA444) 
        ? encparams.phy_width 
        : encparams.phy_width>>1;
	encparams.phy_chrom_height = (CHROMA420!=CHROMA420) 
        ? encparams.phy_height 
        : encparams.phy_height>>1;
	enc_chrom_width = (CHROMA420==CHROMA444) 
        ? encparams.enc_width 
        : encparams.enc_width>>1;
	enc_chrom_height = (CHROMA420!=CHROMA420) 
        ? encparams.enc_height 
        : encparams.enc_height>>1;

	encparams.phy_height2 = encparams.fieldpic ? encparams.phy_height>>1 : encparams.phy_height;
	encparams.enc_height2 = encparams.fieldpic ? encparams.enc_height>>1 : encparams.enc_height;
	encparams.phy_width2 = encparams.fieldpic ? encparams.phy_width<<1 : encparams.phy_width;
	encparams.phy_chrom_width2 = encparams.fieldpic 
        ? encparams.phy_chrom_width<<1 
        : encparams.phy_chrom_width;
 
	encparams.lum_buffer_size = (encparams.phy_width*encparams.phy_height) +
					 sizeof(uint8_t) *(encparams.phy_width/2)*(encparams.phy_height/2) +
					 sizeof(uint8_t) *(encparams.phy_width/4)*(encparams.phy_height/4);
	encparams.chrom_buffer_size = encparams.phy_chrom_width*encparams.phy_chrom_height;
    

	encparams.fsubsample_offset = (encparams.phy_width)*(encparams.phy_height) * sizeof(uint8_t);
	encparams.qsubsample_offset =  encparams.fsubsample_offset 
        + (encparams.phy_width/2)*(encparams.phy_height/2)*sizeof(uint8_t);

	encparams.mb_per_pict = encparams.mb_width*encparams.mb_height2;


	/* Allocate the frame data buffers: if we're not going to scan ahead
     for GOP size we can save a *lot* of memory... */
    if( param_max_GOP_size == param_min_GOP_size )
        frame_buffer_size = 2*param_Bgrp_size+READ_CHUNK_SIZE;
    else
        frame_buffer_size = 2*param_max_GOP_size+READ_CHUNK_SIZE;
    mjpeg_info( "Buffering %d frames", frame_buffer_size );
	frame_buffers = (uint8_t ***) 
		bufalloc(frame_buffer_size*sizeof(uint8_t**));
	
	for(n=0;n<frame_buffer_size;n++)
	{
         frame_buffers[n] = (uint8_t **) bufalloc(3*sizeof(uint8_t*));
		 for (i=0; i<3; i++)
		 {
			 frame_buffers[n][i] = 
                 static_cast<uint8_t *>( bufalloc( (i==0) 
                                                   ? encparams.lum_buffer_size 
                                                   : encparams.chrom_buffer_size ) 
                     );
		 }

         border_mark(frame_buffers[n][0],
                     encparams.enc_width,encparams.enc_height,
                     encparams.phy_width,encparams.phy_height);
         border_mark(frame_buffers[n][1],
                     enc_chrom_width, enc_chrom_height,
                     encparams.phy_chrom_width,encparams.phy_chrom_height);
         border_mark(frame_buffers[n][2],
                     enc_chrom_width, enc_chrom_height,
                     encparams.phy_chrom_width,encparams.phy_chrom_height);

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
	encparams.mpeg1           = (param_mpeg == 1);
	encparams.fieldpic        = (param_fieldenc == 2);

    // SVCD and probably DVD? mandate progressive_sequence = 0 
    switch( param_format )
    {
    case MPEG_FORMAT_SVCD :
    case MPEG_FORMAT_SVCD_NSR :
    case MPEG_FORMAT_SVCD_STILL :
    case MPEG_FORMAT_DVD :
    case MPEG_FORMAT_DVD_NAV :
        encparams.prog_seq = 0;
        break;
    default :
        encparams.prog_seq        = (param_mpeg == 1 || param_fieldenc == 0);
        break;
    }
	encparams.pulldown_32     = param_32_pulldown;

	encparams.aspectratio     = param_aspect_ratio;
	encparams.frame_rate_code = param_frame_rate;
	encparams.dctsatlim		= encparams.mpeg1 ? 255 : 2047;

	/* If we're using a non standard (VCD?) profile bit-rate adjust	the vbv
		buffer accordingly... */

	if( param_bitrate == 0 )
	{
		mjpeg_error_exit1( "Generic format - must specify bit-rate!" );
	}

	encparams.still_size = 0;
	if( MPEG_STILLS_FORMAT(param_format) )
	{
		encparams.vbv_buffer_code = param_vbv_buffer_still_size / 2048;
		encparams.vbv_buffer_still_size = param_pad_stills_to_vbv_buffer_size;
		encparams.bit_rate = param_bitrate;
		encparams.still_size = param_still_size;
	}
	else if( param_mpeg == 1 )
	{
		/* Scale VBV relative to VCD  */
		encparams.bit_rate = MAX(10000, param_bitrate);
		encparams.vbv_buffer_code = (20 * param_bitrate  / 1151929);
	}
	else
	{
		encparams.bit_rate = MAX(10000, param_bitrate);
		encparams.vbv_buffer_code = MIN(112,param_video_buffer_size / 2);
	}
	encparams.vbv_buffer_size = encparams.vbv_buffer_code*16384;

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
	
	encparams.seq_hdr_every_gop = param_seq_hdr_every_gop;
	encparams.seq_end_every_gop = param_seq_end_every_gop;
	encparams.svcd_scan_data = param_svcd_scan_data;
	encparams.ignore_constraints = param_ignore_constraints;
	ctl_seq_length_limit = param_seq_length_limit;
	ctl_nonvid_bit_rate = param_nonvid_bitrate * 1000;
	encparams.low_delay       = 0;
	encparams.constrparms     = (param_mpeg == 1 && 
						   !MPEG_STILLS_FORMAT(param_format));
	encparams.profile         = 4; /* Main profile resp. */
	encparams.level           = 8; /* Main Level      CCIR 601 rates */
	switch(param_norm)
	{
	case 'p': encparams.video_format = 1; break;
	case 'n': encparams.video_format = 2; break;
	case 's': encparams.video_format = 3; break;
	default:  encparams.video_format = 5; break; /* unspec. */
	}
	switch(param_norm)
	{
	case 's':
	case 'p':  /* ITU BT.470  B,G */
		encparams.color_primaries = 5;
		encparams.transfer_characteristics = 5; /* Gamma = 2.8 (!!) */
		encparams.matrix_coefficients = 5; 
        msg = "PAL B/G";
		break;
	case 'n': /* SMPTPE 170M "modern NTSC" */
		encparams.color_primaries = 6;
		encparams.matrix_coefficients = 6; 
		encparams.transfer_characteristics = 6;
        msg = "NTSC";
		break; 
	default:   /* unspec. */
		encparams.color_primaries = 2;
		encparams.matrix_coefficients = 2; 
		encparams.transfer_characteristics = 2;
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
    case MPEG_FORMAT_DVD_NAV :
        /* It would seem DVD and perhaps SVCD demand a 540 pixel display size
           for 4:3 aspect video. However, many players expect 480 and go weird
           if this isn't set...
        */
        if( param_hack_svcd_hds_bug )
        {
            encparams.display_horizontal_size  = encparams.horizontal_size;
            encparams.display_vertical_size    = encparams.vertical_size;
        }
        else
        {
            encparams.display_horizontal_size  = encparams.aspectratio == 2 ? 540 : 720;
            encparams.display_vertical_size    = encparams.vertical_size;
        }
		break;
	default:
		encparams.display_horizontal_size  = encparams.horizontal_size;
		encparams.display_vertical_size    = encparams.vertical_size;
		break;
	}

	encparams.dc_prec         = param_mpeg2_dc_prec;  /* 9 bits */
    encparams.topfirst = 0;
	if( ! encparams.prog_seq )
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

		encparams.topfirst = (fieldorder == Y4M_ILACE_TOP_FIRST || 
                              fieldorder ==Y4M_ILACE_NONE );
	}
	else
		encparams.topfirst = 0;

    // Restrict to frame motion estimation and DCT modes only when MPEG1
    // or when progressive content is specified for MPEG2.
    // Note that for some profiles although we have progressive sequence 
    // header bit = 0 we still only encode with frame modes (for speed).
	encparams.frame_pred_dct_tab[0] 
		= encparams.frame_pred_dct_tab[1] 
		= encparams.frame_pred_dct_tab[2] 
        = (param_mpeg == 1 || param_fieldenc == 0) ? 1 : 0;

    mjpeg_info( "Progressive format frames = %d", 	encparams.frame_pred_dct_tab[0] );
	encparams.qscale_tab[0] 
		= encparams.qscale_tab[1] 
		= encparams.qscale_tab[2] 
		= param_mpeg == 1 ? 0 : 1;

	encparams.intravlc_tab[0] 
		= encparams.intravlc_tab[1] 
		= encparams.intravlc_tab[2] 
		= param_mpeg == 1 ? 0 : 1;

	encparams.altscan_tab[2]  
		= encparams.altscan_tab[1]  
		= encparams.altscan_tab[0]  
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
		int radius_y = param_searchrad*encparams.vertical_size/encparams.horizontal_size;

		/* TODO: These f-codes should really be adjusted for each
		   picture type... */

		encparams.motion_data = (struct motion_data *)malloc(ctl_M*sizeof(struct motion_data));
		if (!encparams.motion_data)
			mjpeg_error_exit1("malloc failed");

		for (i=0; i<ctl_M; i++)
		{
			if(i==0)
			{
				encparams.motion_data[i].sxf = round_search_radius(radius_x*ctl_M);
				encparams.motion_data[i].forw_hor_f_code  = f_code(encparams.motion_data[i].sxf);
				encparams.motion_data[i].syf = round_search_radius(radius_y*ctl_M);
				encparams.motion_data[i].forw_vert_f_code  = f_code(encparams.motion_data[i].syf);
			}
			else
			{
				encparams.motion_data[i].sxf = round_search_radius(radius_x*i);
				encparams.motion_data[i].forw_hor_f_code  = f_code(encparams.motion_data[i].sxf);
				encparams.motion_data[i].syf = round_search_radius(radius_y*i);
				encparams.motion_data[i].forw_vert_f_code  = f_code(encparams.motion_data[i].syf);
				encparams.motion_data[i].sxb = round_search_radius(radius_x*(ctl_M-i));
				encparams.motion_data[i].back_hor_f_code  = f_code(encparams.motion_data[i].sxb);
				encparams.motion_data[i].syb = round_search_radius(radius_y*(ctl_M-i));
				encparams.motion_data[i].back_vert_f_code  = f_code(encparams.motion_data[i].syb);
			}

			/* MPEG-1 demands f-codes for vertical and horizontal axes are
			   identical!!!!
			*/
			if( encparams.mpeg1 )
			{
				encparams.motion_data[i].syf = encparams.motion_data[i].sxf;
				encparams.motion_data[i].syb  = encparams.motion_data[i].sxb;
				encparams.motion_data[i].forw_vert_f_code  = 
					encparams.motion_data[i].forw_hor_f_code;
				encparams.motion_data[i].back_vert_f_code  = 
					encparams.motion_data[i].back_hor_f_code;
				
			}
		}
		
	}
	


	/* make sure MPEG specific parameters are valid */
	range_checks();

	/* Set the frame decode rate and frame display rates.
	   For 3:2 movie pulldown decode rate is != display rate due to
	   the repeated field that appears every other frame.
	*/
	encparams.frame_rate = Y4M_RATIO_DBL(mpeg_framerate(encparams.frame_rate_code));
	if( param_32_pulldown )
	{
		ctl_decode_frame_rate = encparams.frame_rate * (2.0 + 2.0) / (3.0 + 2.0);
		mjpeg_info( "3:2 Pulldown selected frame decode rate = %3.3f fps", 
					ctl_decode_frame_rate);
	}
	else
		ctl_decode_frame_rate = encparams.frame_rate;

	if ( !encparams.mpeg1)
	{
		profile_and_level_checks();
	}
	else
	{
		/* MPEG-1 */
		if (encparams.constrparms)
		{
			if (encparams.horizontal_size>768
				|| encparams.vertical_size>576
				|| ((encparams.horizontal_size+15)/16)*((encparams.vertical_size+15)/16)>396
				|| ((encparams.horizontal_size+15)/16)*((encparams.vertical_size+15)/16)*encparams.frame_rate>396*25.0
				|| encparams.frame_rate>30.0)
			{
				mjpeg_info( "size - setting constrained_parameters_flag = 0");
				encparams.constrparms = 0;
			}
		}

		if (encparams.constrparms)
		{
			for (i=0; i<ctl_M; i++)
			{
				if (encparams.motion_data[i].forw_hor_f_code>4)
				{
					mjpeg_info("Hor. motion search forces constrained_parameters_flag = 0");
					encparams.constrparms = 0;
					break;
				}

				if (encparams.motion_data[i].forw_vert_f_code>4)
				{
					mjpeg_info("Ver. motion search forces constrained_parameters_flag = 0");
					encparams.constrparms = 0;
					break;
				}

				if (i!=0)
				{
					if (encparams.motion_data[i].back_hor_f_code>4)
					{
						mjpeg_info("Hor. motion search setting constrained_parameters_flag = 0");
						encparams.constrparms = 0;
						break;
					}

					if (encparams.motion_data[i].back_vert_f_code>4)
					{
						mjpeg_info("Ver. motion search setting constrained_parameters_flag = 0");
						encparams.constrparms = 0;
						break;
					}
				}
			}
		}
	}

	/* relational checks */
	if ( encparams.mpeg1 )
	{
		if (!encparams.prog_seq)
		{
			mjpeg_warn("encparams.mpeg1 specified - setting progressive_sequence = 1");
			encparams.prog_seq = 1;
		}

		if (encparams.dc_prec!=0)
		{
			mjpeg_info("mpeg1 - setting intra_dc_precision = 0");
			encparams.dc_prec = 0;
		}

		for (i=0; i<3; i++)
			if (encparams.qscale_tab[i])
			{
				mjpeg_info("mpeg1 - setting qscale_tab[%d] = 0",i);
				encparams.qscale_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (encparams.intravlc_tab[i])
			{
				mjpeg_info("mpeg1 - setting intravlc_tab[%d] = 0",i);
				encparams.intravlc_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (encparams.altscan_tab[i])
			{
				mjpeg_info("mpeg1 - setting altscan_tab[%d] = 0",i);
				encparams.altscan_tab[i] = 0;
			}
	}

	if ( !encparams.mpeg1 && encparams.constrparms)
	{
		mjpeg_info("not mpeg1 - setting constrained_parameters_flag = 0");
		encparams.constrparms = 0;
	}


	if( (!encparams.prog_seq || encparams.fieldpic != 0 ) &&
		( (encparams.vertical_size+15) / 16)%2 != 0 )
	{
		mjpeg_warn( "Frame height won't split into two equal field pictures...");
		mjpeg_warn( "forcing encoding as progressive video");
		encparams.prog_seq = 1;
		encparams.fieldpic = 0;
	}


	if (encparams.prog_seq && encparams.fieldpic != 0)
	{
		mjpeg_info("prog sequence - forcing progressive frame encoding");
		encparams.fieldpic = 0;
	}


	if (encparams.prog_seq && encparams.topfirst)
	{
		mjpeg_info("prog sequence setting top_field_first = 0");
		encparams.topfirst = 0;
	}

	/* search windows */
	for (i=0; i<ctl_M; i++)
	{
		if (encparams.motion_data[i].sxf > (4U<<encparams.motion_data[i].forw_hor_f_code)-1)
		{
			mjpeg_info(
				"reducing forward horizontal search width to %d",
						(4<<encparams.motion_data[i].forw_hor_f_code)-1);
			encparams.motion_data[i].sxf = (4U<<encparams.motion_data[i].forw_hor_f_code)-1;
		}

		if (encparams.motion_data[i].syf > (4U<<encparams.motion_data[i].forw_vert_f_code)-1)
		{
			mjpeg_info(
				"reducing forward vertical search width to %d",
				(4<<encparams.motion_data[i].forw_vert_f_code)-1);
			encparams.motion_data[i].syf = (4U<<encparams.motion_data[i].forw_vert_f_code)-1;
		}

		if (i!=0)
		{
			if (encparams.motion_data[i].sxb > (4U<<encparams.motion_data[i].back_hor_f_code)-1)
			{
				mjpeg_info(
					"reducing backward horizontal search width to %d",
					(4<<encparams.motion_data[i].back_hor_f_code)-1);
				encparams.motion_data[i].sxb = (4U<<encparams.motion_data[i].back_hor_f_code)-1;
			}

			if (encparams.motion_data[i].syb > (4U<<encparams.motion_data[i].back_vert_f_code)-1)
			{
				mjpeg_info(
					"reducing backward vertical search width to %d",
					(4<<encparams.motion_data[i].back_vert_f_code)-1);
				encparams.motion_data[i].syb = (4U<<encparams.motion_data[i].back_vert_f_code)-1;
			}
		}
	}

    encparams.bitrate_controller = new OnTheFlyRateCtl;

}

/*
  If the use has selected suppression of hf noise via quantisation
  then we boost quantisation of hf components EXPERIMENTAL: currently
  a linear ramp from 0 at 4pel to param_hf_q_boost increased
  quantisation...

*/

static int quant_hfnoise_filt(int orgquant, int qmat_pos )
    {
    int orgdist = intmax(qmat_pos % 8, qmat_pos/8);
    double qboost;

    /* Maximum param_hf_q_boost quantisation boost for HF components.. */
    qboost = 1.0 + ((param_hf_q_boost * orgdist) / 8);
    return static_cast<int>(orgquant * qboost);
    }

static void
init_quantmat(void)
{
    int i, v, q;
    const char *msg = NULL;
    const uint16_t *qmat, *niqmat;
    encparams.load_iquant = 0;
    encparams.load_niquant = 0;

    /* bufalloc to ensure alignment */
    encparams.intra_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));
    encparams.inter_q = (uint16_t*)bufalloc(64*sizeof(uint16_t));

    switch  (param_hf_quant)
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
            encparams.load_iquant = 1;
            encparams.load_niquant = 1;
            break;
        case  2:    /* -H used OR -H followed by "-N value" */
            msg = "Setting hi-res intra Quantisation matrix";
            qmat = hires_intra_quantizer_matrix;
            niqmat = hires_nonintra_quantizer_matrix;
            encparams.load_iquant = 1;
            if  (param_hf_q_boost)
                encparams.load_niquant = 1;   /* Custom matrix if -N used */
            break;
        case  3:
            msg = "KVCD Notch Quantization Matrix";
            qmat = kvcd_intra_quantizer_matrix;
            niqmat = kvcd_nonintra_quantizer_matrix;
            encparams.load_iquant = 1;
            encparams.load_niquant = 1;
            break;
        case  4:
            msg = "TMPGEnc Quantization matrix";
            qmat = tmpgenc_intra_quantizer_matrix;
            niqmat = tmpgenc_nonintra_quantizer_matrix;
            encparams.load_iquant = 1;
            encparams.load_niquant = 1;
            break;
        case  5:            /* -K file=qmatrixfilename */
            msg = "Loading custom matrices from user specified file";
            encparams.load_iquant = 1;
            encparams.load_niquant = 1;
            qmat = custom_intra_quantizer_matrix;
            niqmat = custom_nonintra_quantizer_matrix;
            break;
        default:
            mjpeg_error_exit1("Help!  Unknown param_hf_quant value %d",
		param_hf_quant);
            /* NOTREACHED */
        }

    if  (msg)
        mjpeg_info(msg);
    
    for (i = 0; i < 64; i++)
        {
        v = quant_hfnoise_filt(qmat[i], i);
        if  (v < 1 || v > 255)
            mjpeg_error_exit1("bad intra value after -N adjust");
        encparams.intra_q[i] = v;

        v = quant_hfnoise_filt(niqmat[i], i);
        if  (v < 1 || v > 255)
            mjpeg_error_exit1("bad nonintra value after -N adjust");
        encparams.inter_q[i] = v;
        }

}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
