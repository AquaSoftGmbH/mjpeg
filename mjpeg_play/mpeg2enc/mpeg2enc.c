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

int verbose = 1;

/* private prototypes */
static void init_mpeg_parms (void);
static void init_encoder(void);
static void init_quantmat (void);

#define MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define MIN(a,b) ( (a)<(b) ? (a) : (b) )




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
static int param_fieldenc   = 0;  /* 0: progressive, 1: bottom first, 2: top first, 3 = progressive seq, interlace frames with field MC and DCT in picture */
static int param_norm       = 0;  /* 'n': NTSC, 'p': PAL, 's': SECAM, else unspecified */
static int param_44_red	= 2;
static int param_22_red	= 3;	
static int param_hfnoise_quant = 0;
static int param_hires_quant = 0;
static double param_act_boost = 0.0;
static int param_video_buffer_size = 0;
static int param_seq_length_limit = 2000;
static int param_min_GOP_size = 12;
static int param_max_GOP_size = 12;
static bool param_preserve_B = false;
static int param_Bgrp_size = 3;
static int param_num_cpus = 1;
static int param_32_pulldown = 0;
static int param_svcd_scan_data = 0;
static int param_seq_hdr_every_gop = 0;
static int param_seq_end_every_gop = 0;
static int param_still_size = 40*1024;
static int param_pad_stills_to_vbv_buffer_size = 0;
static int param_vbv_buffer_still_size = 0;
static int param_force_interlacing = Y4M_UNKNOWN;
static int param_input_interlacing;

/* Input Stream parameter values that have to be further processed to
   set encoding options */

static mpeg_aspect_code_t strm_aspect_ratio;

/* reserved: for later use */
int param_422 = 0;


static void DisplayFrameRates()
{
 	int i;
	printf("Frame-rate codes:\n");
	for( i = 0; i < mpeg_num_framerates; ++i )
	{
		printf( "%2d - %s\n", i, mpeg_framerate_code_definition(i));
	}
	exit(0);
}

static void DisplayAspectRatios()
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
	fprintf(stderr,"mjpegtools mpeg2enc version " VERSION "\n" );
	fprintf(stderr,"Usage: %s [params]\n",str);
	fprintf(stderr,"   where possible params are:\n");
	fprintf(stderr, " -v num  Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n");
	fprintf( stderr, " -f fmt  Set pre-defined mux format.\n");
	fprintf( stderr, "         [0 = Generic MPEG1, 1 = standard VCD, 2 = VCD,\n");
	fprintf( stderr, "          3 = Generic MPEG2, 4 = standard SVCD, 5 = user SVCD,\n"
			         "          6 = VCD Stills sequences, 7 = SVCD Stills sequences, 8 = DVD]\n");	
	fprintf(stderr,"   -a num     Aspect ratio displayed image [1..4] (default:  4:3)\n" );
	fprintf(stderr,"              0 - Display aspect ratio code tables\n");
	fprintf(stderr,"   -F num     Playback frame rate of encoded video\n"
			       "      (default: frame rate of input stream)\n");
	fprintf(stderr,"              0 - Display frame rate code table\n");
	fprintf(stderr,"   -b num     Bitrate in KBit/sec (default: 1152 KBit/s for VCD)\n");
	fprintf(stderr,"   -B num     Non-video data bitrate to use for sequence splitting\n");
	fprintf(stderr,"              calculations (see -S).\n");
	fprintf(stderr,"   -q num     Quality factor [1..31] (1 is best, no default)\n");
	fprintf(stderr,"              Bitrate sets upper-bound rate when q is specified\n");
	fprintf(stderr,"   -o name    Outputfile name (REQUIRED!!!)\n");
	fprintf(stderr,"   -T size    Target Size in KB for VCD stills\n");
	fprintf(stderr,"   -I num     only for MPEG 2 output:\n");
	fprintf(stderr,"               0 = encode by frame (progressive - per frame MC/ MDCT)\n");
	fprintf(stderr,"               1 = encode by field, bottom fields before top first\n");
	fprintf(stderr,"               2 = encode by field, top fields before bottom\n");
	fprintf(stderr,"               3 = encode by frame (per-field MC and MDCT\n)");
	fprintf(stderr,"   -r num     Search radius for motion compensation [0..32] (default 16)\n");
	fprintf(stderr,"   -4 num     (default: 2)\n");
	fprintf(stderr,"   	  Population halving passes 4*4-pel subsampled motion compensation\n" );
	fprintf(stderr,"   -2 num     (default: 3)\n");
	fprintf(stderr,"   	  Population halving passes 2*2-pel subsampled motion compensation\n" );
	fprintf(stderr,"   -g num     Minimum GOP size (default 12)\n" );
	fprintf(stderr,"   -G num     Maximum GOP size (default 12)\n" );
	fprintf(stderr,"   -P         Preserve 2 B frames between I/P even when adjusting GOP size\n");
	fprintf(stderr,"   -M num     Optimise threading for num CPU's (default: 1)\n");
	fprintf(stderr,"   -Q num     Quality factor decrement for highly active blocks"
			       "      [0.0 .. 10] (default: 2.5)\n");
	fprintf(stderr,"   -V num     Target video buffer size in KB (default 46)\n");
	fprintf(stderr,"   -n n|p|s   Force video norm (NTSC, PAL, SECAM) (default: PAL).\n");
	fprintf(stderr,"   -S num     Start a new sequence every num Mbytes in the final mux-ed\n");
	fprintf(stderr,"              stream.  -B specifies the bitrate of non-video data\n");
	fprintf(stderr,"   -p         Generate header flags for 32 pull down of 24fps movie.\n");
	fprintf(stderr,"   -N         Noise filter via quantisation adjustment\n" );
	fprintf(stderr,"   -h         Maximise high-frequency resolution\n"
			       "              (useful for high quality sources at high bit-rates)\n" );
	fprintf(stderr,"   -s         Include a sequence header every GOP.\n" );
	fprintf(stderr,"   -z b|t     Force playback field order of bottom or top first\n");
	printf("   -?         Print this lot out!\n");
	exit(0);
}

static void set_format_presets()
{
	switch( param_format  )
	{
	case MPEG_FORMAT_MPEG1 :  /* Generic MPEG1 */
		mjpeg_info( "Selecting generic MPEG1 output profile\n");
		if( param_video_buffer_size == 0 )
			param_video_buffer_size = 46;
		if( param_bitrate == 0 )
			param_bitrate = 1151929;
		break;

	case MPEG_FORMAT_VCD :
		param_mpeg = 1;
		param_bitrate = 1151929;
		param_video_buffer_size = 46;
		mjpeg_info("VCD default options selected\n");
		
	case MPEG_FORMAT_VCD_NSR : /* VCD format, non-standard rate */
		mjpeg_info( "Selecting VCD output profile\n");
		param_mpeg = 1;
		param_svcd_scan_data = 0;
		param_seq_hdr_every_gop = 1;
		if( param_bitrate == 0 )
			param_bitrate = 1151929;
		if( param_video_buffer_size == 0 )
			param_video_buffer_size = 46 * param_bitrate / 1151929;

		break;
		
	case  MPEG_FORMAT_MPEG2 : 
		param_mpeg = 2;
		mjpeg_info( "Selecting generic MPEG2 output profile\n");
		if( param_video_buffer_size == 0 )
			param_video_buffer_size = 46 * param_bitrate / 1151929;
		break;

	case MPEG_FORMAT_SVCD :
		mjpeg_info("SVCD standard settings selected\n");
		param_bitrate = 2500000;
		param_min_GOP_size = 6;
		param_max_GOP_size = 18;
		param_video_buffer_size = 230;

	case  MPEG_FORMAT_SVCD_NSR :		/* Non-standard data-rate */
		mjpeg_info( "Selecting SVCD output profile\n");
		param_mpeg = 2;
		param_fieldenc = 3;
		if( param_quant == 0 )
			param_quant = 12;
		param_svcd_scan_data = 1;
		param_seq_hdr_every_gop = 1;
		break;

	case MPEG_FORMAT_VCD_STILL :
		mjpeg_info( "Selecting VCD Stills output profile\n");
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
			if( param_still_size < 20*1024 || param_still_size > 42*1024 )
			{
				mjpeg_error_exit1( "VCD normal-resolution stills must be >= 20KB and <= 42KB each\n");
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
			param_vbv_buffer_still_size = param_still_size;
			param_video_buffer_size = 224;
			param_pad_stills_to_vbv_buffer_size = 1;			
		}
		else
		{
			mjpeg_error("VCD normal resolution stills must be 352x288 (PAL) or 352x240 (NTSC)\n");
			mjpeg_error_exit1( "VCD high resolution stills must be 704x576 (PAL) or 704x480 (NTSC)\n");
		}
		param_quant = 0;		/* We want to try and hit our size target */
		
		param_seq_hdr_every_gop = 1;
		param_seq_end_every_gop = 1;
		param_min_GOP_size = 1;
		param_max_GOP_size = 1;
		break;

	case MPEG_FORMAT_SVCD_STILL :
		mjpeg_info( "Selecting SVCD Stills output profile\n");
		param_mpeg = 2;
		param_fieldenc = 3;
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
			mjpeg_info( "SVCD normal-resolution stills selected.\n" );
		}
		else if( opt_horizontal_size == 704 &&
				 (opt_vertical_size == 480 || opt_vertical_size == 576) )
		{
			mjpeg_info( "SVCD high-resolution stills selected.\n" );
		}
		else
		{
			mjpeg_error("SVCD normal resolution stills must be 480x576 (PAL) or 480x480 (NTSC)\n");
			mjpeg_error_exit1( "SVCD high resolution stills must be 704x576 (PAL) or 704x480 (NTSC)\n");
		}
		if( param_still_size < 30*1024 || param_still_size > 200*1024 )
		{
			mjpeg_error_exit1( "SVCD resolution stills must be >= 30KB and <= 200KB each\n");
		}


		param_seq_hdr_every_gop = 1;
		param_seq_end_every_gop = 1;
		param_min_GOP_size = 1;
		param_max_GOP_size = 1;
		break;
	}
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

static int infer_default_params()
{
	int nerr = 0;


	/* Infer norm, aspect ratios and frame_rate if not specified */

	if( param_norm == 0 && (opt_frame_rate_code==3 || opt_frame_rate_code == 2) )
	{
		mjpeg_warn("Assuming norm PAL\n");
		param_norm = 'p';
	}
	if( param_norm == 0 && (opt_frame_rate_code==4 || opt_frame_rate_code == 1) )
	{
		mjpeg_warn("Assuming norm NTSC\n");
		param_norm = 'n';
	}

	if( param_frame_rate != 0 )
	{
		if( opt_frame_rate_code != param_frame_rate && 
			opt_frame_rate_code > 0 && 
			opt_frame_rate_code < mpeg_num_framerates )
		{
			mjpeg_warn( "Specified display frame-rate %3.2f will over-ride\n", 
						Y4M_RATIO_DBL(mpeg_framerate(param_frame_rate)));
			mjpeg_warn( "(different!) frame-rate %3.2f of the input stream\n",
						Y4M_RATIO_DBL(mpeg_framerate(opt_frame_rate_code)));
		}
		opt_frame_rate_code = param_frame_rate;
	}

	if( param_aspect_ratio == 0 )
	{
		param_aspect_ratio = strm_aspect_ratio;
	}

	if( param_aspect_ratio == 0 )
	{
		mjpeg_warn( "Aspect ratio specifed and no guess possible: assuming 4:3 display aspect!\n");
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

static int check_param_constraints()
{
	int nerr = 0;
	if( param_32_pulldown )
	{
		if( opt_frame_rate_code != 4 && opt_frame_rate_code != 5  )
		{
			mjpeg_error( "3:2 movie pulldown only sensible for 30000/1001 or 30fps output stream\n" );
			++nerr;
		}
		if( param_fieldenc != 0 && param_fieldenc != 3 )
		{
			mjpeg_error( "3:2 movie pulldown only sensible for Frame pictures\n");
			++nerr;
		}
	}
	


	if(  param_aspect_ratio > mpeg_num_aspect_ratios[param_mpeg-1] ) 
	{
		mjpeg_error("For MPEG-%d aspect ratio code  %d > %d illegal\n", 
					param_mpeg, param_aspect_ratio, 
					mpeg_num_aspect_ratios[param_mpeg-1]);
		++nerr;
	}
		


	if( param_min_GOP_size > param_max_GOP_size )
	{
		mjpeg_error( "Min GOP size must be <= Max GOP size\n" );
		++nerr;
	}

	if( param_preserve_B && 
		( param_min_GOP_size % param_Bgrp_size != 0 ||
		  param_max_GOP_size % param_Bgrp_size != 0 )
		)
	{
		mjpeg_error("Preserving I/P frame spacing is impossible if min and max GOP sizes are\n" );
		mjpeg_error_exit1("Not both divisible by %d\n", param_Bgrp_size );
	}

	if(	( param_format != MPEG_FORMAT_VCD_STILL &&
		  param_format != MPEG_FORMAT_SVCD_STILL &&
		  param_min_GOP_size < 2*param_Bgrp_size 
			)  ||
		param_max_GOP_size+param_max_GOP_size+param_Bgrp_size+1 > 
		FRAME_BUFFER_SIZE-READ_CHUNK_SIZE  )
	{
		mjpeg_error( 
				"Min and max GOP sizes must be in range [%d..%d]\n",
				2*param_Bgrp_size,
				(FRAME_BUFFER_SIZE-READ_CHUNK_SIZE-1-param_Bgrp_size)/2);
		++nerr;
	}


	switch( param_format )
	{
	case MPEG_FORMAT_SVCD_STILL :
	case MPEG_FORMAT_SVCD_NSR :
	case MPEG_FORMAT_SVCD : 
		if( param_aspect_ratio != 2 && param_aspect_ratio != 3 )
			mjpeg_error_exit1("SVCD only supports 4:3 and 16:9 aspect ratios\n");
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
	
	while( (n=getopt(argc,argv,"m:a:f:n:b:z:T:B:q:o:S:I:r:M:4:2:Q:g:G:v:V:F:tpsZNhOP")) != EOF)
	{
		switch(n) {

		case 'b':
			param_bitrate = atoi(optarg)*1000;
			break;

		case 'T' :
			param_still_size = atoi(optarg)*1024;
			if( param_still_size < 20*1024 || param_still_size > 500*1024 )
			{
				mjpeg_error( "-T requires arg 20..500\n" );
				++nerr;
			}
			break;

		case 'B':
			param_nonvid_bitrate = atoi(optarg);
			if( param_nonvid_bitrate < 0 )
			{
				mjpeg_error("-B requires arg > 0\n");
				++nerr;
			}
			break;
		case 'q':
			param_quant = atoi(optarg);
			if(param_quant<1 || param_quant>32)
			{
				mjpeg_error("-q option requires arg 1 .. 32\n");
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
				mjpeg_error( "-a option must be positive\n");
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
				mjpeg_error( "-F option must be [0..%d]\n", 
						 mpeg_num_framerates-1);
				++nerr;
			}
			break;

		case 'o':
			outfilename = optarg;
			break;

		case 'I':
			param_fieldenc = atoi(optarg);
			if(param_fieldenc<0 || param_fieldenc>3)
			{
				mjpeg_error("-I option requires 0 ... 3\n");
				++nerr;
			}
			break;

		case 'r':
			param_searchrad = atoi(optarg);
			if(param_searchrad<0 || param_searchrad>32)
			{
				mjpeg_error("-r option requires arg 0 .. 32\n");
				++nerr;
			}
			break;

		case 'M':
			param_num_cpus = atoi(optarg);
			if(param_num_cpus<0 || param_num_cpus>32)
			{
				mjpeg_error("-M option requires arg 0..32\n");
				++nerr;
			}
			break;

		case '4':
			param_44_red = atoi(optarg);
			if(param_44_red<0 || param_44_red>4)
			{
				mjpeg_error("-4 option requires arg 0..4\n");
				++nerr;
			}
			break;
			
		case '2':
			param_22_red = atoi(optarg);
			if(param_22_red<0 || param_22_red>4)
			{
				mjpeg_error("-2 option requires arg 0..4\n");
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
				mjpeg_error("-v option requires arg 20..4000\n");
				++nerr;
			}
			break;

		case 'S' :
			param_seq_length_limit = atoi(optarg);
			if(param_seq_length_limit<1 )
			{
				mjpeg_error("-S option requires arg > 1\n");
				++nerr;
			}
			break;
		case 'p' :
			param_32_pulldown = 1;
			break;

		case 'z' :
			if( strlen(optarg) != 1 || (optarg[0] != 't' && optarg[0] != 'b' ) )
			{
				mjpeg_error("-z option requires arg b or t\n" );
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
				mjpeg_error("-n option requires arg n or p, or s.\n");
				++nerr;
			}
			break;
		case 'g' :
			param_min_GOP_size = atoi(optarg);
			break;
		case 'G' :
			param_max_GOP_size = atoi(optarg);
			break;
		case 'P' :
			param_preserve_B = true;
			break;
		case 'N':
			param_hfnoise_quant = 1;
			break;
		case 'h':
			param_hires_quant = 1;
			break;
		case 's' :
			param_seq_hdr_every_gop = 1;
			break;
		case 'Q' :
			param_act_boost = atof(optarg);
			if( param_act_boost <0.0 || param_act_boost > 10.0)
			{
				mjpeg_error( "-q option requires arg 0.1 .. 10.0\n");
				++nerr;
			}
			break;
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
			istrm_fd = open( argv[optind], O_RDONLY );
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
						&opt_frame_rate_code, &param_input_interlacing,
						&strm_aspect_ratio
						);
	
	if(opt_horizontal_size<=0)
	{
		mjpeg_error("Horizontal size from input stream illegal\n");
		++nerr;
	}
	if(opt_vertical_size<=0)
	{
		mjpeg_error("Vertical size from input stream illegal\n");
		++nerr;
	}

	
	/* Check parameters that cannot be checked when parsed/read */

	if(!outfilename)
	{
		mjpeg_error("Output file name (-o option) is required!\n");
		++nerr;
	}

	if(opt_frame_rate_code<1 || opt_frame_rate_code>8)
	{
		if( param_frame_rate == 0 )
		{
			mjpeg_error("Input stream with unknown frame-rate and no frame-rate specified with -a!\n");
			++nerr;
		}
		else
			opt_frame_rate_code = param_frame_rate;
	}


	set_format_presets();

	nerr += infer_default_params();

	nerr += check_param_constraints();

	if(nerr) 
	{
		Usage(argv[0]);
		exit(1);
	}


	mjpeg_info("Encoding MPEG-%d video to %s\n",param_mpeg,outfilename);
	mjpeg_info("Horizontal size: %d pel\n",opt_horizontal_size);
	mjpeg_info("Vertical size: %d pel\n",opt_vertical_size);
	mjpeg_info("Aspect ratio code: %d = %s\n", 
			param_aspect_ratio,
			mpeg_aspect_code_definition(param_mpeg,param_aspect_ratio));
	mjpeg_info("Frame rate code:   %d = %s\n",
			opt_frame_rate_code,
			mpeg_framerate_code_definition(opt_frame_rate_code));

	if(param_bitrate) 
		mjpeg_info("Bitrate: %d KBit/s\n",param_bitrate/1000);
	else
		mjpeg_info( "Bitrate: VCD\n");
	if(param_quant) 
		mjpeg_info("Quality factor: %d (1=best, 31=worst)\n",param_quant);

	mjpeg_info("Field order for input: %s\n", 
			   mpeg_interlace_code_definition(param_input_interlacing) );

	if( param_seq_length_limit )
	{
		mjpeg_info( "New Sequence every %d Mbytes\n", param_seq_length_limit );
		mjpeg_info( "Assuming non-video stream of %d Kbps\n", param_nonvid_bitrate );
	}
	else
		mjpeg_info( "Sequence unlimited length\n" );

	mjpeg_info("Search radius: %d\n",param_searchrad);

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
		mjpeg_error_exit1("malloc failed\n");
	}
	adjust = BUFFER_ALIGN-((unsigned long)buf)%BUFFER_ALIGN;
	if( adjust == BUFFER_ALIGN )
		adjust = 0;
	return (uint8_t*)(buf+adjust);
}

static void init_encoder()
{
	int i, n;
	static int block_count_tab[3] = {6,8,12};

	initbits(); 
	init_fdct();
	init_idct();

	/* Tune threading and motion compensation for specified number of CPU's 
	   and specified speed parameters.
	 */
	ctl_act_boost = (param_act_boost+1.0);
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
	width = 16*mb_width;
	height = 16*mb_height;

	/* Calculate the sizes and offsets in to luminance and chrominance
	   buffers.  A.Stevens 2000 for luminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel
	   sub-sampled uint8_t followed by 4*4 sub-sampled.  We add an
	   extra row to act as a margin to allow us to neglect / postpone
	   edge condition checking in time-critical loops...  */

	chrom_width = (opt_chroma_format==CHROMA444) ? width : width>>1;
	chrom_height = (opt_chroma_format!=CHROMA420) ? height : height>>1;



	height2 = opt_fieldpic ? height>>1 : height;
	width2 = opt_fieldpic ? width<<1 : width;
	chrom_width2 = opt_fieldpic ? chrom_width<<1 : chrom_width;
 
	block_count = block_count_tab[opt_chroma_format-1];
	lum_buffer_size = (width*height) + 
					 sizeof(uint8_t) *(width/2)*(height/2) +
					 sizeof(uint8_t) *(width/4)*(height/4+1);
	chrom_buffer_size = chrom_width*chrom_height;


	fsubsample_offset = (width)*(height) * sizeof(uint8_t);
	qsubsample_offset =  fsubsample_offset + (width/2)*(height/2)*sizeof(uint8_t);

	mb_per_pict = mb_width*mb_height2;


	/* clip table */
	if (!(clp_0_255 = (uint8_t *)malloc(1024)))
		mjpeg_error_exit1("malloc failed\n");
	clp_0_255 += 384;
	for (i=-384; i<640; i++)
		clp_0_255[i] = (i<0) ? 0 : ((i>255) ? 255 : i);
	
	/* Allocate the frame data buffers */

	frame_buffers = (uint8_t ***) 
		bufalloc(FRAME_BUFFER_SIZE*sizeof(uint8_t**));
	
	for(n=0;n<FRAME_BUFFER_SIZE;n++)
	{
         frame_buffers[n] = (uint8_t **) bufalloc(3*sizeof(uint8_t*));
		 for (i=0; i<3; i++)
		 {
			 frame_buffers[n][i] = 
				 bufalloc( (i==0) ? lum_buffer_size : chrom_buffer_size );
		 }
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

	inputtype = 0;  /* doesnt matter */
	istrm_nframes = 999999999; /* determined by EOF of stdin */

	ctl_N_min = param_min_GOP_size;      /* I frame distance */
	ctl_N_max = param_max_GOP_size;
	mjpeg_info( "GOP SIZE RANGE %d TO %d\n", ctl_N_min, ctl_N_max );
	ctl_M = param_Bgrp_size;             /* I or P frame distance */
	ctl_M_min = param_preserve_B ? ctl_M : 1;
	if( ctl_M >= ctl_N_min )
		ctl_M = ctl_N_min-1;
	opt_mpeg1           = (param_mpeg == 1);
	opt_fieldpic        = (param_fieldenc!=0 && param_fieldenc != 3);
	opt_prog_seq        = (param_mpeg == 1 || param_fieldenc == 0);
	opt_pulldown_32     = param_32_pulldown;

	opt_aspectratio     = param_aspect_ratio;
	opt_dctsatlim		= opt_mpeg1 ? 255 : 2047;

	/* If we're using a non standard (VCD?) profile bit-rate adjust	the vbv
		buffer accordingly... */

	if( param_bitrate == 0 )
	{
		mjpeg_error_exit1( "Generic format - must specify bit-rate!\n" );
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
		ctl_quant_floor = (double)param_quant;
	}
	else
	{
		ctl_quant_floor = 0.0;		/* Larger than max quantisation */
	}

	ctl_video_buffer_size = param_video_buffer_size * 1024 * 8;
	
	opt_seq_hdr_every_gop = param_seq_hdr_every_gop;
	opt_seq_end_every_gop = param_seq_end_every_gop;
	opt_svcd_scan_data = param_svcd_scan_data;
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
	opt_color_primaries = 5;
	opt_transfer_characteristics = 5;
	switch(param_norm)
	{
	case 'p': opt_matrix_coefficients = 5; break;
	case 'n': opt_matrix_coefficients = 4; break;
	case 's': opt_matrix_coefficients = 5; break; /* ???? */
	default:  opt_matrix_coefficients = 2; break; /* unspec. */
	}
	opt_display_horizontal_size  = opt_horizontal_size;
	opt_display_vertical_size    = opt_vertical_size;

	opt_dc_prec         = 0;  /* 8 bits */
	if( param_fieldenc == 2 )
		opt_topfirst  = 1;
	else if (param_fieldenc == 1)
		opt_topfirst = 0;
	else if( ! opt_prog_seq )
	{
		int fieldorder;
		if( param_force_interlacing != Y4M_UNKNOWN ) 
		{
			mjpeg_info( "Forcing playback video to be: %s\n",
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

	opt_repeatfirst     = 0;
	opt_prog_frame      = (param_fieldenc==0);

	opt_frame_pred_dct_tab[0] = param_mpeg == 1 ? 1 : (param_fieldenc == 0);
	opt_frame_pred_dct_tab[1] = param_mpeg == 1 ? 1 : (param_fieldenc == 0);
	opt_frame_pred_dct_tab[2] = param_mpeg == 1 ? 1 : (param_fieldenc == 0);

	opt_qscale_tab[0]   = param_mpeg == 1 ? 0 : 1;
	opt_qscale_tab[1]   = param_mpeg == 1 ? 0 : 1;
	opt_qscale_tab[2]   = param_mpeg == 1 ? 0 : 1;

	opt_intravlc_tab[0] = param_mpeg == 1 ? 0 : 1;
	opt_intravlc_tab[1] = param_mpeg == 1 ? 0 : 1;
	opt_intravlc_tab[2] = param_mpeg == 1 ? 0 : 1;

	opt_altscan_tab[0]  = param_mpeg == 1 ? 0 : (opt_fieldpic!=0);
	opt_altscan_tab[1]  = param_mpeg == 1 ? 0 : (opt_fieldpic!=0);
	opt_altscan_tab[2]  = param_mpeg == 1 ? 0 : (opt_fieldpic!=0);


	/*  A.Stevens 2000: The search radius *has* to be a multiple of 8
		for the new fast motion compensation search to work correctly.
		We simply round it up if needs be.  */

	if(param_searchrad*ctl_M>127)
	{
		param_searchrad = 127/ctl_M;
		mjpeg_warn("Search radius reduced to %d\n",param_searchrad);
	}
	
	{ 
		int radius_x = param_searchrad;
		int radius_y = param_searchrad*opt_vertical_size/opt_horizontal_size;

		/* TODO: These f-codes should really be adjusted for each
		   picture type... */

		opt_motion_data = (struct motion_data *)malloc(ctl_M*sizeof(struct motion_data));
		if (!opt_motion_data)
			mjpeg_error_exit1("malloc failed\n");

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
		mjpeg_info( "3:2 Pulldown selected frame decode rate = %3.3f fps\n", 
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
				mjpeg_info( "size - setting constrained_parameters_flag = 0\n");
				opt_constrparms = 0;
			}
		}

		if (opt_constrparms)
		{
			for (i=0; i<ctl_M; i++)
			{
				if (opt_motion_data[i].forw_hor_f_code>4)
				{
					mjpeg_info("Hor. motion search forces constrained_parameters_flag = 0\n");
					opt_constrparms = 0;
					break;
				}

				if (opt_motion_data[i].forw_vert_f_code>4)
				{
					mjpeg_info("Ver. motion search forces constrained_parameters_flag = 0\n");
					opt_constrparms = 0;
					break;
				}

				if (i!=0)
				{
					if (opt_motion_data[i].back_hor_f_code>4)
					{
						mjpeg_info("Hor. motion search setting constrained_parameters_flag = 0\n");
						opt_constrparms = 0;
						break;
					}

					if (opt_motion_data[i].back_vert_f_code>4)
					{
						mjpeg_info("Ver. motion search setting constrained_parameters_flag = 0\n");
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
			mjpeg_warn("opt_mpeg1 specified - setting progressive_sequence = 1\n");
			opt_prog_seq = 1;
		}

		if (opt_chroma_format!=CHROMA420)
		{
			mjpeg_info("mpeg1 - forcing chroma_format = 1 (4:2:0) - others not supported\n");
			opt_chroma_format = CHROMA420;
		}

		if (opt_dc_prec!=0)
		{
			mjpeg_info("mpeg1 - setting intra_dc_precision = 0\n");
			opt_dc_prec = 0;
		}

		for (i=0; i<3; i++)
			if (opt_qscale_tab[i])
			{
				mjpeg_info("mpeg1 - setting qscale_tab[%d] = 0\n",i);
				opt_qscale_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (opt_intravlc_tab[i])
			{
				mjpeg_info("mpeg1 - setting intravlc_tab[%d] = 0\n",i);
				opt_intravlc_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (opt_altscan_tab[i])
			{
				mjpeg_info("mpeg1 - setting altscan_tab[%d] = 0\n",i);
				opt_altscan_tab[i] = 0;
			}
	}

	if ( !opt_mpeg1 && opt_constrparms)
	{
		mjpeg_info("not mpeg1 - setting constrained_parameters_flag = 0\n");
		opt_constrparms = 0;
	}


	if( (!opt_prog_seq || !opt_prog_frame ) &&
		( (opt_vertical_size+15) / 16)%2 != 0 )
	{
		mjpeg_warn( "Frame height won't split into two equal field pictures...\n");
		mjpeg_warn( "forcing encoding as progressive video\n");
		opt_prog_seq = 1;
		opt_fieldpic = 0;
		opt_prog_frame = 1;
	}


	if (opt_prog_seq && !opt_prog_frame)
	{
		mjpeg_info("prog sequence - setting progressive_frame = 1\n");
		opt_prog_frame = 1;
	}

	if (opt_prog_frame && opt_fieldpic)
	{
		mjpeg_info("prog frame - setting field_pictures = 0\n");
		opt_fieldpic = 0;
	}

	if (!opt_prog_frame && opt_repeatfirst)
	{
		mjpeg_info("not prog frame setting repeat_first_field = 0\n");
		opt_repeatfirst = 0;
	}

	if (opt_prog_seq && !opt_repeatfirst && opt_topfirst)
	{
		mjpeg_info("prog sequence setting top_field_first = 0\n");
		opt_topfirst = 0;
	}

	/* search windows */
	for (i=0; i<ctl_M; i++)
	{
		if (opt_motion_data[i].sxf > (4<<opt_motion_data[i].forw_hor_f_code)-1)
		{
			mjpeg_info(
				"reducing forward horizontal search width to %d\n",
						(4<<opt_motion_data[i].forw_hor_f_code)-1);
			opt_motion_data[i].sxf = (4<<opt_motion_data[i].forw_hor_f_code)-1;
		}

		if (opt_motion_data[i].syf > (4<<opt_motion_data[i].forw_vert_f_code)-1)
		{
			mjpeg_info(
				"reducing forward vertical search width to %d\n",
				(4<<opt_motion_data[i].forw_vert_f_code)-1);
			opt_motion_data[i].syf = (4<<opt_motion_data[i].forw_vert_f_code)-1;
		}

		if (i!=0)
		{
			if (opt_motion_data[i].sxb > (4<<opt_motion_data[i].back_hor_f_code)-1)
			{
				mjpeg_info(
					"reducing backward horizontal search width to %d\n",
					(4<<opt_motion_data[i].back_hor_f_code)-1);
				opt_motion_data[i].sxb = (4<<opt_motion_data[i].back_hor_f_code)-1;
			}

			if (opt_motion_data[i].syb > (4<<opt_motion_data[i].back_vert_f_code)-1)
			{
				mjpeg_info(
					"reducing backward vertical search width to %d\n",
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
	int x = qmat_pos % 8;
	int y = qmat_pos / 8;
	int qboost = 1024;

	if( ! param_hfnoise_quant )
	{
		return orgquant;
	}

	/* Maximum 50% quantisation boost for HF components... */
	if( x > 4 )
		qboost += (256*(x-4)/3);
	if( y > 4 )
		qboost += (256*(y-4)/3);

	return (orgquant * qboost + 512)/ 1024;
}

static void init_quantmat()
{
	int i,v, q;
	opt_load_iquant = 0;
	opt_load_niquant = 0;
	if( param_hires_quant )
	{
		opt_load_iquant |= 1;
		mjpeg_info( "Setting hi-res intra Quantisation matrix\n" );
		for (i=0; i<64; i++)
		{
			opt_intra_q[i] = hires_intra_quantizer_matrix[i];
		}	
	}
	else
	{
		/* use default intra matrix */
		opt_load_iquant = param_hfnoise_quant;
		for (i=0; i<64; i++)
		{
			v = quant_hfnoise_filt( default_intra_quantizer_matrix[i], i);
			if (v<1 || v>255)
				mjpeg_error_exit1("value in intra quant matrix invalid (after noise filt adjust)");
				opt_intra_q[i] = v;
				
		} 
	}
	/* TODO: Inv Quant matrix initialisation should check if the fraction fits in 16 bits! */
	if( param_hires_quant )
	{
		mjpeg_info( "Setting hi-res non-intra quantiser matrix\n" );
		for (i=0; i<64; i++)
		{
			opt_inter_q[i] = hires_nonintra_quantizer_matrix[i];
		}	
	}
	else
	{
		/* default non-intra matrix is all 16's. For *our* default we use something
		   more suitable for domestic analog sources... which is non-standard...*/
		opt_load_niquant |= 1;
		for (i=0; i<64; i++)
		{
			v = quant_hfnoise_filt(default_nonintra_quantizer_matrix[i],i);
			if (v<1 || v>255)
				mjpeg_error_exit1("value in non-intra quant matrix invalid (after noise filt adjust)");
			opt_inter_q[i] = v;
		}
	}
  
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
