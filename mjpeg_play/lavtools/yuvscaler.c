/*
  *  yuvscaler.c
  *  Copyright (C) 2001 Xavier Biquard <xbiquard@free.fr>
  * 
  *  
  *  Downscales arbitrary sized yuv frame to yuv frames suitable for VCD, SVCD or specified
  * 
  *  yuvscaler is distributed in the hope that it will be useful, but
  *  WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  * 
  *  yuvscaler is distributed under the terms of the GNU General Public Licence
  *  as discribed in the COPYING file
 */
// Implementation: there are two scaling methods: one for not_interlaced output and one for interlaced output. 
// For each method, line switching may occur on input frames (by convention, line are numbered from 0 to n-1)
// 
// First version doing only downscaling with no interlacing
// June 2001: interlacing capable version
// July 2001: upscaling capable version
// September 2001: line switching
// September/October 2001: new yuv4m header
// October 2001: first MMX part for bicubic

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "lav_io.h"
#include "editlist.h"
#include "jpegutils.c"
#include "mjpeg_logging.h"
#include "yuv4mpeg.h"
#include "inttypes.h"
#include "yuvscaler.h"
#include "mpegconsts.h"
// #include "config.h"
#include "attributes.h"
#include "../utils/mmx.h"
#include "../utils/yuv4mpeg.h"


#define YUVSCALER_VERSION LAVPLAY_VERSION
// For pointer adress alignement
#define ALIGNEMENT 16		// 16 bytes alignement for mmx registers in SIMD instructions for Pentium

// For input
unsigned int input_width;
unsigned int input_height;
unsigned int input_useful = 0;	// =1 if specified
unsigned int input_useful_width = 0;
unsigned int input_useful_height = 0;
unsigned int input_discard_col_left = 0;
unsigned int input_discard_col_right = 0;
unsigned int input_discard_line_above = 0;
unsigned int input_discard_line_under = 0;
unsigned int input_black = 0;	//=1 if black borders on input frames
unsigned int input_black_line_above = 0;
unsigned int input_black_line_under = 0;
unsigned int input_black_col_right = 0;
unsigned int input_black_col_left = 0;
unsigned int input_active_height = 0;
unsigned int input_active_width = 0;
int input_interlaced = -1;
//=Y4M_ILACE_NONE for not interlaced, =Y4M_ILACE_TOP_FIRST for top interlaced, =Y4M_ILACE_BOTTOM_FIRST for bottom interlaced
// double input_aspectratio;

// Downscaling ratios
unsigned int input_height_slice;
unsigned int output_height_slice;
unsigned int input_width_slice;
unsigned int output_width_slice;

// For padded_input
unsigned int padded_width = 0;
unsigned int padded_height = 0;

// For output
unsigned int display_width;
unsigned int display_height;
unsigned int output_width;
unsigned int output_height;
unsigned int output_active_width;
unsigned int output_active_height;
unsigned int output_black_line_above = 0;
unsigned int output_black_line_under = 0;
unsigned int output_black_col_right = 0;
unsigned int output_black_col_left = 0;
unsigned int output_skip_line_above = 0;
unsigned int output_skip_line_under = 0;
unsigned int output_skip_col_right = 0;
unsigned int output_skip_col_left = 0;
unsigned int black = 0, black_line = 0, black_col = 0;	// =1 if black lines must be generated on output
unsigned int skip = 0, skip_line = 0, skip_col = 0;	// =1 if lines or columns from the active output will not be displayed on output frames
// NB: as these number may not be multiple of output_[height,width]_slice, it is not possible to remove the corresponding pixels in
// the input frame, a solution that could speed up things. 
int output_interlaced = -1;
// =Y4M_ILACE_NONE for not interlaced, =Y4M_ILACE_TOP_FIRST for top interlaced, =Y4M_ILACE_BOTTOM_FIRST for bottom interlaced
// double output_aspectratio;
int output_fd = 1;		// frames are written to stdout
unsigned int vcd = 0;		//=1 if vcd output was selected
unsigned int svcd = 0;		//=1 if svcd output was selected

// Global variables
int line_switching = 0;		// =0 for no line switching, =1 for line switching
int norm = -1;			// =0 for PAL and =1 for NTSC
int wide = 0;			// =1 for wide (16:9) input to standard (4:3) output conversion
int ratio = 0;
int infile = 0;			// =0 for stdin (default) =1 for file
int algorithm = -1;		// =0 for resample, and =1 for bicubic
unsigned int specific = 0;	// is >0 if a specific downscaling speed enhanced treatment of data is possible
unsigned int mono = 0;		// is =1 for monochrome output

// taken from lav2yuv
char **filename;
EditList el;
#define MAX_EDIT_LIST_FILES 256
#define MAX_JPEG_LEN (512*1024)
int chroma_format = CHROMA422;
static unsigned char jpeg_data[MAX_JPEG_LEN];


// Keywords for argument passing 
const char VCD_KEYWORD[] = "VCD";
const char SVCD_KEYWORD[] = "SVCD";
const char SIZE_KEYWORD[] = "SIZE_";
const char USE_KEYWORD[] = "USE_";
const char WIDE2STD_KEYWORD[] = "WIDE2STD";
const char INFILE_KEYWORD[] = "INFILE_";
const char RATIO_KEYWORD[] = "RATIO_";
const char TOP_FIRST[] = "INTERLACED_TOP_FIRST";
const char BOT_FIRST[] = "INTERLACED_BOTTOM_FIRST";
const char NOT_INTER[] = "NOT_INTERLACED";
const char MONO_KEYWORD[] = "MONOCHROME";
const char FASTVCD[] = "FASTVCD";
const char FAST_WIDE2VCD[] = "FAST_WIDE2VCD";
const char WIDE2VCD[] = "WIDE2VCD";
const char RESAMPLE[] = "RESAMPLE";
const char BICUBIC[] = "BICUBIC";
const char LINESWITCH[] = "LINE_SWITCH";
const char ACTIVE[] = "ACTIVE";
const char NO_HEADER[] = "NO_HEADER";
const char MMX[] = "MMX";

// Special BICUBIC algorithm
// 2048=2^11
#define FLOAT2INTEGER 2048
#define FLOAT2INTEGERPOWER 11
long int FLOAT2INTOFFSET = 2097152;
unsigned int bicubic_div_width = FLOAT2INTEGER, bicubic_div_height =
  FLOAT2INTEGER;
float bicubic_negative_max = 0.04, bicubic_positive_max = 1.08;
long int bicubic_offset = 0;
unsigned long int bicubic_max = 0;
unsigned int multiplicative;


// Unclassified
unsigned long int diviseur;
uint8_t *divide;
unsigned short int *u_i_p;
unsigned int out_nb_col_slice, out_nb_line_slice;
static char *legal_opt_flags = "k:I:d:n:v:M:m:O:whtg";
int verbose = 1;
#define PARAM_LINE_MAX 256
float framerates[] =
  { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };


uint8_t blacky = 16;
uint8_t blackuv = 128;
uint8_t no_header = 0;		// =1 for no stream header output 

// MMX
int16_t *mmx_padded, *mmx_cubic;
int32_t *mmx_res;
long int max_lint_neg = -2147483647;	// maximal negative number available for long int
int mmx = 0;			// =1 for mmx activated


// *************************************************************************************
int
my_y4m_read_frame (int fd, y4m_frame_info_t * frameinfo,
		   unsigned long int buflen, char *buf, int line_switching)
{
  static int err = Y4M_OK;
  unsigned int line;
  if ((err = y4m_read_frame_header (fd, frameinfo)) == Y4M_OK)
    {
      if (!line_switching)
	{
	  if ((err = y4m_read (fd, buf, buflen)) != Y4M_OK)
	    {
	      mjpeg_info ("Couldn't read FRAME content: %s!\n",
			  y4m_strerr (err));
	      return (err);
	    }
	}
      else
	{
	  // line switching during frame read 
	  // Y COMPONENT
	  for (line = 0; line < input_height; line += 2)
	    {
	      buf += input_width;	// buf points to next line on output, store input line there
	      if ((err = y4m_read (fd, buf, input_width)) != Y4M_OK)
		{
		  mjpeg_info ("Couldn't read FRAME content line %d : %s!\n",
			      line, y4m_strerr (err));
		  return (err);
		}
	      buf -= input_width;	// buf points to former line on output, store input line there
	      if ((err = y4m_read (fd, buf, input_width)) != Y4M_OK)
		{
		  mjpeg_info ("Couldn't read FRAME content line %d : %s!\n",
			      line + 1, y4m_strerr (err));
		  return (err);
		}
	      buf += 2 * input_width;	// two line were read and stored
	    }
	  // U and V component
	  for (line = 0; line < input_height; line += 2)
	    {
	      buf += input_width / 2;	// buf points to next line on output, store input line there
	      if ((err = y4m_read (fd, buf, input_width / 2)) != Y4M_OK)
		{
		  mjpeg_info ("Couldn't read FRAME content line %d : %s!\n",
			      line, y4m_strerr (err));
		  return (err);
		}
	      buf -= input_width / 2;	// buf points to former line on output, store input line there
	      if ((err = y4m_read (fd, buf, input_width / 2)) != Y4M_OK)
		{
		  mjpeg_info ("Couldn't read FRAME content line %d : %s!\n",
			      line + 1, y4m_strerr (err));
		  return (err);
		}
	      buf += input_width;	// two line were read and stored
	    }
	}
    }
  else
    {
      if (err != Y4M_ERR_EOF)
	mjpeg_info ("Couldn't read FRAME header: %s!\n", y4m_strerr (err));
      else
	mjpeg_info ("End of stream!\n");
      return (err);
    }
  return Y4M_OK;
}

// *************************************************************************************
int
line_switch (uint8_t * input, uint8_t * line)
{
  int ligne;
  // This function does line_switching on matrix input which contains a full frame 
  // Y COMPONENT
  for (ligne = 0; ligne < input_height; ligne += 2)
    {
      memcpy (line, input, input_width);
      memcpy (input, input + input_width, input_width);
      memcpy (input + input_width, line, input_width);
      input += (input_width << 1);
    }
  // U and V COMPONENTS
  for (ligne = 0; ligne < input_height; ligne += 2)
    {
      memcpy (line, input, input_width >> 1);
      memcpy (input, input + (input_width >> 1), input_width >> 1);
      memcpy (input + (input_width >> 1), line, input_width >> 1);
      input += input_width;
    }
  return (0);
}

// *************************************************************************************
int
blackout (uint8_t * input_y, uint8_t * input_u, uint8_t * input_v)
{
  unsigned int line;
  uint8_t *right;
  // Y COMPONENT

  for (line = 0; line < input_black_line_above; line++)
    {
      memset (input_y, blacky, input_useful_width);
      input_y += input_width;
    }
  right = input_y + input_black_col_left + input_active_width;
  for (line = 0; line < input_active_height; line++)
    {
      memset (input_y, blacky, input_black_col_left);
      memset (right, blacky, input_black_col_right);
      input_y += input_width;
      right += input_width;
    }
  for (line = 0; line < input_black_line_under; line++)
    {
      memset (input_y, blacky, input_useful_width);
      input_y += input_width;
    }
  // U COMPONENT
  for (line = 0; line < (input_black_line_above >> 1); line++)
    {
      memset (input_u, blackuv, input_useful_width >> 1);
      input_u += input_width >> 1;
    }
  right = input_u + ((input_black_col_left + input_active_width) >> 1);
  for (line = 0; line < (input_active_height >> 1); line++)
    {
      memset (input_u, blackuv, input_black_col_left >> 1);
      memset (right, blackuv, input_black_col_right >> 1);
      input_u += input_width >> 1;
      right += input_width >> 1;
    }
  for (line = 0; line < (input_black_line_under >> 1); line++)
    {
      memset (input_u, blackuv, input_useful_width >> 1);
      input_u += input_width >> 1;
    }
  // V COMPONENT
  for (line = 0; line < (input_black_line_above >> 1); line++)
    {
      memset (input_v, blackuv, input_useful_width >> 1);
      input_v += input_width >> 1;
    }
  right = input_v + ((input_black_col_left + input_active_width) >> 1);
  for (line = 0; line < (input_active_height >> 1); line++)
    {
      memset (input_v, blackuv, input_black_col_left >> 1);
      memset (right, blackuv, input_black_col_right >> 1);
      input_v += input_width >> 1;
      right += input_width >> 1;
    }
  for (line = 0; line < (input_black_line_under >> 1); line++)
    {
      memset (input_v, blackuv, input_useful_width >> 1);
      input_v += input_width >> 1;
    }
  return (0);
}


// *************************************************************************************
void
print_usage (char *argv[])
{
  fprintf (stderr,
	   "usage: %s -I [input_keyword] -M [mode_keyword] -O [output_keyword] [-n p|s|n] [-v 0-2] [-h/?] [inputfiles]\n"
	   "%s UPscales or DOWNscales arbitrary sized yuv frames (stdin by default) to a specified format (to stdout)\n"
	   "please note that %s implements two dowscaling algorithms but only one upscaling algorithm\n"
	   "\n" "%s is keyword driven :\n"
	   "\t inputfiles selects yuv frames coming from Editlist files\n"
	   "\t -I for keyword concerning INPUT  frame characteristics\n"
	   "\t -M for keyword concerning the downscaling MODE of yuvscaler\n"
	   "\t -O for keyword concerning OUTPUT frame characteristics\n"
	   "\t ((the former syntax with -k and -w is still supported but depreciated))\n"
	   "\n" "Possible input keyword are:\n"
	   "\t USE_WidthxHeight+WidthOffset+HeightOffset to select a useful area of the input frame (all multiple of 2,\n"
	   "\t    Height and HeightOffset multiple of 4 if interlaced), the rest of the image being discarded\n"
	   "\t ACTIVE_WidthxHeight+WidthOffset+HeightOffset to select an active area of the input frame (all multiple of 2,\n"
	   "\t    Height and HeightOffset multiple of 4 if interlaced), the rest of the image being made black\n"
	   "\n"
	   "Possible mode keyword are:\n"
	   "\t BICUBIC to use the (Mitchell-Netravalli) high-quality bicubic upsacling and/or downscaling algorithm\n"
	   "\t RESAMPLE to use a classical resampling algorithm -only for downscaling- that goes much faster than bicubic\n"
	   "\t For coherence reason, %s will use RESAMPLE if only downscaling is necessary, BICUBIC if upscaling is necessary\n"
	   "\t WIDE2STD to converts widescreen (16:9) input frames into standard output (4:3), generating necessary black lines\n"
	   "\t RATIO_WidthIn_WidthOut_HeightIn_HeightOut to specified conversion ratios of\n"
	   "\t     WidthIn/WidthOut for width and HeightIN/HeightOut for height to be applied to the useful area.\n"
	   "\t     The output active area that results from scalingthe input useful area might be different\n"
	   "\t     from the display area specified thereafter using the -O KEYWORD syntax.\n"
	   "\t     In that case, %s will automatically generate necessary black lines and columns and/or skip necessary\n"
	   "\t     lines and columns to get an active output centered within the display size.\n"
	   "\t WIDE2VCD to transcode wide (16:9) frames  to VCD (equivalent to -M WIDE2STD -O VCD)\n"
	   "\t FASTVCD to transcode full sized frames to VCD (equivalent to -M RATIO_2_1_2_1 -O VCD, see after)\n"
	   "\t FAST_WIDE2VCD to transcode full sized wide (16:9) frames to VCD (equivalent to -M WIDE2STD -M RATIO_2_1_2_1 -O VCD, see after)\n"
	   "\t LINE_SWITCH to switch lines two by two between input and output, no line switching by default\n"
	   "\t NO_HEADER to suppress stream header generation on output\n"
	   "\t MMX to use MMX functions for BICUBIC scaling (experimental feature!!)\n"
	   "\n"
	   "Possible output keywords are:\n"
	   "\t MONOCHROME to generate monochrome frames on output\n"
	   "\t  VCD to generate  VCD compliant frames on output\n"
	   "\t     (taking care of PAL and NTSC standards, not-interlaced frames)\n"
	   "\t SVCD to generate SVCD compliant frames on output\n"
	   "\t     (taking care of PAL and NTSC standards, interlaced frames, top first by default)\n"
	   "\t SIZE_WidthxHeight to generate frames of size WidthxHeight on output (multiple of 2, Height of 4 if interlaced)\n"
	   "\t If VCD or SVCD keywords are not used, output interlacing is taken of the same type as input.\n"
	   "\t By default, output frames size will be the same as input frame sizes\n"
	   "\n"
	   "The most useful downscaling ratios receive a dedicated and accelerated treatment is RESAMPLE mode. They are:\n"
	   "\t RATIO_WidthIn_WidthOut_1_1 => downscaling only concerns width, not height\n"
	   "\t RATIO_3_2_1_1 => SVCD downscaling\n"
	   "\t RATIO_1_1_HeightIn_HeightOut => downscaling only concerns height, not width\n"
	   "\t RATIO_1_1_4_3 => WIDE2STD downscaling mode\n"
	   "\t RATIO_WidthIn_WidthOut_2_1 => VCD downscaling\n"
	   "\t RATIO_2_1_2_1 => FASTVCD, slightly width distorted (reale ratio 45 to 22) but faster VCD downscaling (-M RATIO_2_1_2_1 -O VCD)\n"
	   "\t RATIO_WidthIn_WidthOut_8_3 => specific to WIDE2VCD downscaling (-M WIDE2STD -O VCD)\n"
	   "\t RATIO_2_1_8_3 => specific to (slighly distorted) FAST_WIDE2VCD downscaling (-M WIDE2STD -M RATIO_2_1_2_1 -O VCD)\n"
	   "\t RATIO_1_1_1_1 => copy useful input part of possible several files into output frames\n"
	   "\n"
	   "-n  (usually not necessary) if norm could not be determined from data flux, specifies the OUTPUT norm for VCD/SVCD p=pal,s=secam,n=ntsc\n"
	   "-v  Specifies the degree of verbosity: 0=quiet, 1=normal, 2=verbose/debug\n"
	   "-h/? : print this lot!\n", argv[0], argv[0], argv[0], argv[0],
	   argv[0], argv[0]);
  exit (1);
}

// *************************************************************************************

// *************************************************************************************
void
handle_args_global (int argc, char *argv[])
{
  // This function takes care of the global variables 
  // initialisation that are independent of the input stream
  // Main function is to know wether input frames originate from file or stdin
  int c;

  while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{
	case 'v':
	  verbose = atoi (optarg);
	  if (verbose < 0 || verbose > 2)
	    {
	      mjpeg_error_exit1 ("Verbose level must be [0..2]\n");
	    }
	  break;


	case 'n':		// TV norm for SVCD/VCD output
	  switch (*optarg)
	    {
	    case 'p':
	    case 's':
	      norm = 0;
	      break;
	    case 'n':
	      norm = 1;
	      break;
	    default:
	      mjpeg_error_exit1 ("Illegal norm letter specified: %c\n",
				 *optarg);
	    }
	  break;


	case 'h':
	case '?':
	  print_usage (argv);
	  break;

	default:
	  break;
	}

    }
//   fprintf(stderr,"optind=%d,argc=%d\n",optind,argc);
  if (optind > argc)
    print_usage (argv);
  if (optind == argc)
    infile = 0;
  else
    {
      infile = argc - optind;
      filename = argv + optind;
    }

}


// *************************************************************************************


// *************************************************************************************
void
handle_args_dependent (int argc, char *argv[])
{
  // This function takes care of the global variables 
  // initialisation that may depend on the input stream
  // It does also coherence check on input, useful_input, display, output_active sizes and ratio sizes
  int c;
  unsigned int ui1, ui2, ui3, ui4;
  int output, input, mode;

  // By default, display sizes is the same as input size
  display_width = input_width;
  display_height = input_height;

  optind = 1;
  while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{
	case 'k':		// Compatibility reasons
	case 'O':		// OUTPUT
	  output = 0;
	  if (strcmp (optarg, VCD_KEYWORD) == 0)
	    {
	      output = 1;
	      vcd = 1;
	      svcd = 0;		// if user gives VCD and SVCD keyword, take last one only into account
	      display_width = 352;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("VCD output format requested in PAL/SECAM norm\n");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm\n");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!\n");

	    }
	  if (strcmp (optarg, SVCD_KEYWORD) == 0)
	    {
	      output = 1;
	      svcd = 1;
	      vcd = 0;		// if user gives VCD and SVCD keyword, take last one only into account
	      display_width = 480;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("SVCD output format requested in PAL/SECAM norm\n");
		  display_height = 576;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("SVCD output format requested in NTSC norm\n");
		  display_height = 480;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine SVCD output size. Please use the -n option!\n");
	    }
	  if (strncmp (optarg, SIZE_KEYWORD, 5) == 0)
	    {
	      output = 1;
	      if (sscanf (optarg, "SIZE_%ux%u", &ui1, &ui2) == 2)
		{
		  // Coherence check: sizes must be multiple of 2
		  if ((ui1 % 2 == 0) && (ui2 % 2 == 0))
		    {
		      display_width = ui1;
		      display_height = ui2;
		    }
		  else
		    mjpeg_error_exit1
		      ("Unconsistent SIZE keyword, not multiple of 2: %s\n",
		       optarg);
		  if (output_interlaced != Y4M_ILACE_NONE)
		    {
		      if (display_height % 4 != 0)
			mjpeg_error_exit1
			  ("Unconsistent SIZE keyword, Height not multiple of 4 but output interlaced!!");
		    }
		}
	      else
		mjpeg_error_exit1 ("Uncorrect SIZE keyword: %s\n", optarg);
	    }
/*	  if (strcmp (optarg, TOP_FIRST) == 0)
	    {
	      output = 1;
	      output_interlaced = Y4M_ILACE_TOP_FIRST;
	    }

	  if (strcmp (optarg, BOT_FIRST) == 0)
	    {
	      output = 1;
	      output_interlaced = Y4M_ILACE_BOTTOM_FIRST;
	    }
	  if (strcmp (optarg, NOT_INTER) == 0)
	    {
	      output = 1;
	      output_interlaced = Y4M_ILACE_NONE;
	    }
*/// Theoritically, this should go into handle_args_global
	  if (strcmp (optarg, MONO_KEYWORD) == 0)
	    {
	      output = 1;
	      mono = 1;
	    }

	  if (output == 0)
	    mjpeg_error_exit1 ("Uncorrect output keyword: %s\n", optarg);
	  break;

	case 'w':		// Compatibility reasons
	  wide = 1;
	  break;

	case 'M':		// MODE
	  mode = 0;
	  if (strcmp (optarg, WIDE2STD_KEYWORD) == 0)
	    {
	      wide = 1;
	      mode = 1;
	    }

	  if (strcmp (optarg, RESAMPLE) == 0)
	    {
	      mode = 1;
	      algorithm = 0;
	    }

	  if (strcmp (optarg, MMX) == 0)
	    {
	      mode = 1;
	      mmx = 1;
	    }

	  if (strcmp (optarg, BICUBIC) == 0)
	    {
	      mode = 1;
	      algorithm = 1;
	    }

	  if (strcmp (optarg, LINESWITCH) == 0)
	    {
	      mode = 1;
	      line_switching = 1;
	    }

	  if (strcmp (optarg, NO_HEADER) == 0)
	    {
	      mode = 1;
	      no_header = 1;
	    }


	  if (strncmp (optarg, RATIO_KEYWORD, 6) == 0)
	    {
	      if (sscanf (optarg, "RATIO_%u_%u_%u_%u", &ui1, &ui2, &ui3, &ui4)
		  == 4)
		{
		  // A coherence check will be done when the useful input sizes are known
		  ratio = 1;
		  mode = 1;
		  input_width_slice = ui1;
		  output_width_slice = ui2;
		  input_height_slice = ui3;
		  output_height_slice = ui4;
		}
	      if (ratio == 0)
		mjpeg_error_exit1 ("Unconsistent RATIO keyword: %s\n",
				   optarg);
	    }

	  if (strcmp (optarg, FAST_WIDE2VCD) == 0)
	    {
	      wide = 1;
	      mode = 1;
	      ratio = 1;
	      input_width_slice = 2;
	      output_width_slice = 1;
	      input_height_slice = 2;
	      output_height_slice = 1;
	      vcd = 1;
	      svcd = 0;		// if user gives VCD and SVCD keyword, take last one only into account
	      display_width = 352;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("VCD output format requested in PAL/SECAM norm\n");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm\n");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!\n");
	    }

	  if (strcmp (optarg, WIDE2VCD) == 0)
	    {
	      wide = 1;
	      mode = 1;
	      vcd = 1;
	      svcd = 0;		// if user gives VCD and SVCD keyword, take last one only into account
	      display_width = 352;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("VCD output format requested in PAL/SECAM norm\n");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm\n");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!\n");
	    }

	  if (strcmp (optarg, FASTVCD) == 0)
	    {
	      mode = 1;
	      vcd = 1;
	      svcd = 0;		// if user gives VCD and SVCD keyword, take last one only into account
	      ratio = 1;
	      input_width_slice = 2;
	      output_width_slice = 1;
	      input_height_slice = 2;
	      output_height_slice = 1;
	      display_width = 352;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("VCD output format requested in PAL/SECAM norm\n");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm\n");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!\n");
	    }

	  if (mode == 0)
	    mjpeg_error_exit1 ("Uncorrect mode keyword: %s\n", optarg);
	  break;


	case 'I':		// INPUT
	  input = 0;
	  if (strncmp (optarg, USE_KEYWORD, 4) == 0)
	    {
	      input = 1;
	      if (sscanf (optarg, "USE_%ux%u+%u+%u", &ui1, &ui2, &ui3, &ui4)
		  == 4)
		{
		  // Coherence check: 
		  // every values must be multiple of 2
		  // and if input is interlaced, height offsets must be multiple of 4
		  // since U and V have half Y resolution and are interlaced
		  // and the required zone must be inside the input size
		  if ((ui1 % 2 == 0) && (ui2 % 2 == 0) && (ui3 % 2 == 0)
		      && (ui4 % 2 == 0) && (ui1 + ui3 <= input_width)
		      && (ui2 + ui4 <= input_height))
		    {
		      input_useful_width = ui1;
		      input_useful_height = ui2;
		      input_discard_col_left = ui3;
		      input_discard_line_above = ui4;
		      input_discard_col_right =
			input_width - input_useful_width -
			input_discard_col_left;
		      input_discard_line_under =
			input_height - input_useful_height -
			input_discard_line_above;
		      input_useful = 1;
		    }
		  else
		    mjpeg_error_exit1
		      ("Unconsistent USE keyword: %s, offsets/sizes not multiple of 2 or offset+size>input size\n",
		       optarg);
		  if (input_interlaced != Y4M_ILACE_NONE)
		    {
		      if ((input_useful_height % 4 != 0)
			  || (input_discard_line_above % 4 != 0))
			mjpeg_error_exit1
			  ("Unconsistent USE keyword: %s, height offset or size not multiple of 4 but input is interlaced!!\n",
			   optarg);
		    }

		}
	    }


	  if (strncmp (optarg, ACTIVE, 6) == 0)
	    {
	      input = 1;
	      if (sscanf
		  (optarg, "ACTIVE_%ux%u+%u+%u", &ui1, &ui2, &ui3, &ui4) == 4)
		{
		  // Coherence check : offsets must be multiple of 2 since U and V have half Y resolution 
		  // if interlaced, height must be multiple of 4
		  // and the required zone must be inside the input size
		  if ((ui1 % 2 == 0) && (ui2 % 2 == 0) && (ui3 % 2 == 0)
		      && (ui4 % 2 == 0) && (ui1 + ui3 <= input_width)
		      && (ui2 + ui4 <= input_height))
		    {
		      input_active_width = ui1;
		      input_active_height = ui2;
		      input_black_col_left = ui3;
		      input_black_line_above = ui4;
		      input_black_col_right =
			input_width - input_active_width -
			input_black_col_left;
		      input_black_line_under =
			input_height - input_active_height -
			input_black_line_above;
		      input_black = 1;
		    }
		  else
		    mjpeg_error_exit1
		      ("Unconsistent ACTIVE keyword: %s, offsets/sizes not multiple of 4 or offset+size>input size\n",
		       optarg);
		  if (input_interlaced != Y4M_ILACE_NONE)
		    {
		      if ((input_active_height % 4 != 0)
			  || (input_black_line_above % 4 != 0))
			mjpeg_error_exit1
			  ("Unconsistent ACTIVE keyword: %s, height offset or size not multiple of 4 but input is interlaced!!\n",
			   optarg);
		    }

		}
	      else
		mjpeg_error_exit1 ("Uncorrect input flag argument: %s\n",
				   optarg);
	    }
//        if (strcmp (optarg, NOT_INTER) == 0)
//          {
//            input = 1;
//            input_interlaced = 0;
//          }
//        if (strcmp (optarg, ODD_FIRST) == 0)
//          {
//            input = 1;
//            input_interlaced = 1;
//          }

//        if (strcmp (optarg, EVEN_FIRST) == 0)
//          {
//            input = 1;
//            input_interlaced = 2;
//          }
	  if (input == 0)
	    mjpeg_error_exit1 ("Uncorrect input keyword: %s\n", optarg);
	  break;

	default:
	  break;
	}
    }

// Interlacing and line-switching
// 
// Line_switching is not equivalent to a change in interlacing type from TOP_FIRST to BOTTOM_FIRST or vice_versa since
// such conversion needs to interleave field 1 from frame 0 with field 0 from frame 1. Whereas line switching only works with frame 0
// 
// By default, there is no line switching. Line switching will occur only if specified

  if (vcd == 1)
    {
//      if (output_interlaced > 0)
//      mjpeg_warn
//        ("VCD requires non-interlaced output frames. Ignoring interlaced specification\n");
      output_interlaced = Y4M_ILACE_NONE;
    }
  if (svcd == 1)
    {
//      if (output_interlaced == 0)
//       {
//          mjpeg_warn
//            ("SVCD requires interlaced output frames. Ignoring non-interlaced specification\n");
//          output_interlaced = 2;
//       }
//      
//      if (output_interlaced == -1)
//       {
//          if (input_interlaced == -1)
      // default to even interlaced frames
//            output_interlaced = 2;    
//          else 
      // No line switching by default
//            output_interlaced = input_interlaced; 
//       }
      if (input_interlaced == Y4M_ILACE_BOTTOM_FIRST)
	output_interlaced = Y4M_ILACE_BOTTOM_FIRST;
      else
	output_interlaced = Y4M_ILACE_TOP_FIRST;
    }
  if (output_interlaced == -1)
    output_interlaced = input_interlaced;


  // Unspecified input variables specification
  if (input_useful_width == 0)
    input_useful_width = input_width;
  if (input_useful_height == 0)
    input_useful_height = input_height;


  // Ratio coherence check against input_useful size
  if (ratio == 1)
    {
      if ((input_useful_width % input_width_slice == 0)
	  && (input_useful_height % input_height_slice == 0))
	{
	  output_active_width =
	    (input_useful_width / input_width_slice) * output_width_slice;
	  output_active_height =
	    (input_useful_height / input_height_slice) * output_height_slice;
	}
      else
	mjpeg_error_exit1
	  ("Specified input ratios (%u and %u) does not divide input useful size (%u and %u)!\n",
	   input_width_slice, input_height_slice, input_useful_width,
	   input_useful_height);
    }

  // if USE and ACTIVE keywords were used, redefined input ACTIVE size relative to USEFUL zone
  if ((input_black == 1) && (input_useful == 1))
    {
      input_black_line_above =
	input_black_line_above >
	input_discard_line_above ? input_black_line_above -
	input_discard_line_above : 0;
      input_black_line_under =
	input_black_line_under >
	input_discard_line_under ? input_black_line_under -
	input_discard_line_under : 0;
      input_black_col_left =
	input_black_col_left >
	input_discard_col_left ? input_black_col_left -
	input_discard_col_left : 0;
      input_black_col_right =
	input_black_col_right >
	input_discard_col_right ? input_black_col_right -
	input_discard_col_right : 0;
      input_active_width =
	input_useful_width - input_black_col_left - input_black_col_right;
      input_active_height =
	input_useful_height - input_black_line_above - input_black_line_under;
      if ((input_active_width == input_useful_width)
	  && (input_active_height == input_useful_height))
	input_black = 0;	// black zone is not inside useful zone
    }


  // Unspecified output variables specification
  if (output_active_width == 0)
    output_active_width = display_width;
  if (output_active_height == 0)
    output_active_height = display_height;
//  if (display_width == 0)
//    display_width = output_active_width;
//  if (display_height == 0)
//    display_height = output_active_height;

  if (wide == 1)
    output_active_height = (output_active_height * 3) / 4;
  // Common pitfall! it is 3/4 not 9/16!
  // Indeed, Standard ratio is 4:3, so 16:9 has an height that is 3/4 smaller than the display_height

  // At this point, input size, input_useful size, output_active size and display size are specified
  // Time for the final coherence check and black and skip initialisations
  // Final check
  output_width =
    output_active_width > display_width ? output_active_width : display_width;
  output_height =
    output_active_height >
    display_height ? output_active_height : display_height;
  if ((output_active_width % 4 != 0) || (output_active_height % 4 != 0)
      || (display_width % 4 != 0) || (display_height % 4 != 0))
    mjpeg_error_exit1
      ("Output sizes are not multiple of 4! %ux%u, %ux%u being displayed\n",
       output_active_width, output_active_height, display_width,
       display_height);

  // Skip and black initialisations
  // 
  if (output_active_width > display_width)
    {
      skip = 1;
      skip_col = 1;
      output_skip_col_right = (output_active_width - display_width) / 2;
      output_skip_col_left =
	output_active_width - display_width - output_skip_col_right;
    }
  if (output_active_width < display_width)
    {
      black = 1;
      black_col = 1;
      output_black_col_right = (display_width - output_active_width) / 2;
      output_black_col_left =
	display_width - output_active_width - output_black_col_right;
    }
  if (output_active_height > display_height)
    {
      skip = 1;
      skip_line = 1;
      output_skip_line_above = (output_active_height - display_height) / 2;
      output_skip_line_under =
	output_active_height - display_height - output_skip_line_above;
    }
  if (output_active_height < display_height)
    {
      black = 1;
      black_line = 1;
      output_black_line_above = (display_height - output_active_height) / 2;
      output_black_line_under =
	display_height - output_active_height - output_black_line_above;
    }
}

// *************************************************************************************
int
main (int argc, char *argv[])
{
  // Function MAIN : we align (regularly used) pointers adress on 64 bits so that MMX instructions may be easely used
  //  like that: a = ((unsigned int)a & (unsigned int) ~63) + 64;
  // and we reserve 64 suppl�mentairy bits during memory allocation 
  int n, res, len, err = Y4M_OK, nb;
  unsigned long int i, j;
//  int frame_rate_code = 0;
  y4m_ratio_t frame_rate = y4m_fps_UNKNOWN;

//  int input_fd = 0;
  long int frame_num = 0;
//  uint8_t magic[] = "123456"; // : the last character of magic is the string termination character
//  const uint8_t key[] = "FRAME\n";
//   uint8_t header[] = "YUV4MPEG XXX XXX X\n";
  unsigned int *height_coeff = NULL, *width_coeff = NULL;
  uint8_t *input, *output, *line, *padded_input = NULL, *padded_bottom =
    NULL, *padded_top = NULL;
  uint8_t *input_y, *input_u, *input_v;
  uint8_t *input_y_infile, *input_u_infile, *input_v_infile;	// when input frames come from files
  uint8_t *output_y, *output_u, *output_v;
  uint8_t *frame_y, *frame_u, *frame_v;
  uint8_t **frame_y_p, **frame_u_p, **frame_v_p;	// size is not yet known
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  unsigned int divider;

  // SPECIFIC TO BICUBIC
  unsigned int *in_line = NULL, *in_col = NULL, out_line, out_col;
  unsigned long int somme;
  int m;
//  int dummy;
  float *a = NULL, *b = NULL;
  int16_t *cubic_spline_m = NULL, *cubic_spline_n = NULL;
  long int value, lint;

  // SPECIFIC TO YUV4MPEG 
  unsigned long int nb_pixels;
  y4m_frame_info_t frameinfo;
  y4m_stream_info_t streaminfo;


  mjpeg_info ("yuvscaler (version " YUVSCALER_VERSION
	      ") is a general scaling utility for yuv frames\n");
  mjpeg_info ("(C) 2001 Xavier Biquard <xbiquard@free.fr>\n");
  mjpeg_info ("%s -h for help\n", argv[0]);

  handle_args_global (argc, argv);
  mjpeg_default_handler_verbosity (verbose);
  y4m_init_stream_info (&streaminfo);
  y4m_init_frame_info (&frameinfo);


  if (infile == 0)
    {
      // INPUT comes from stdin, we check for a correct file header
      if (y4m_read_stream_header (0, &streaminfo) != Y4M_OK)
	{
	  mjpeg_error ("Could'nt read YUV4MPEG header!\n");
	  exit (1);
	}
      input_width = y4m_si_get_width (&streaminfo);
      input_height = y4m_si_get_height (&streaminfo);
      frame_rate = y4m_si_get_framerate (&streaminfo);
      input_interlaced = y4m_si_get_interlace (&streaminfo);
//     input_aspectratio=streaminfo->aspectratio;  
    }
  else
    {
      // INPUT comes from FILES
      read_video_files (filename, infile, &el);
      chroma_format = el.MJPG_chroma;
      if (chroma_format != CHROMA422)
	mjpeg_error_exit1 ("Editlist not in chroma 422 format, exiting...\n");
      input_width = el.video_width;
      input_height = el.video_height;
      frame_rate = mpeg_conform_framerate (el.video_fps);
      input_interlaced =
	(el.video_inter == LAV_NOT_INTERLACED) ? Y4M_ILACE_NONE :
	(el.video_inter == LAV_INTER_TOP_FIRST) ? Y4M_ILACE_TOP_FIRST :
	(el.video_inter == LAV_INTER_BOTTOM_FIRST) ? Y4M_ILACE_BOTTOM_FIRST :
	Y4M_UNKNOWN;
      input_interlaced = el.video_inter;
      // this will be  eventually overrided by user's specification
      // Let's determine the frame rate code
//      frame_rate_code = 0;
//      while ((framerates[++frame_rate_code] != frame_rate)
//           && (frame_rate_code <= 8));
      y4m_si_set_width (&streaminfo, input_width);
      y4m_si_set_height (&streaminfo, input_height);
      y4m_si_set_interlace (&streaminfo, input_interlaced);
      y4m_si_set_framerate (&streaminfo, frame_rate);
      y4m_si_set_sampleaspect (&streaminfo, y4m_sar_UNKNOWN);
    }

  // INITIALISATIONS
  // Norm determination (we eventually overwrite user's specification through the -n flag
  if (Y4M_RATIO_EQL (frame_rate, y4m_fps_PAL))
    norm = 0;
  if (Y4M_RATIO_EQL (frame_rate, y4m_fps_NTSC))
    norm = 1;
  if (norm < 0)
    {
      mjpeg_warn
	("Could not infer norm (PAL/SECAM or NTSC) from input data (frame size=%dx%d, frame rate=%d:%d fps)!!\n",
	 input_width, input_height, frame_rate.n, frame_rate.d);
    }



  // Deal with args that depend on input stream
  handle_args_dependent (argc, argv);

  // Scaling algorithm determination
  if ((algorithm == 0) || (algorithm == -1))
    {
      // Coherences check: downscale not upscale
      if ((input_useful_width < output_active_width)
	  || (input_useful_height < output_active_height))
	{
	  if (algorithm == 0)
	    mjpeg_info
	      ("Resampling algorithm can only downscale, not upscale => switching to bicubic algorithm\n");
	  algorithm = 1;
	}
      else
	algorithm = 0;
    }

  // USER'S INFORMATION OUTPUT

  y4m_log_stream_info (LOG_INFO, "yuvscaler: ", &streaminfo);
  //  y4m_print_stream_info(output_fd,streaminfo);

  switch (input_interlaced)
    {
    case Y4M_ILACE_NONE:
      mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, %s\n",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above,
		  NOT_INTER);
      break;
    case Y4M_ILACE_TOP_FIRST:
      mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, %s\n",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above,
		  TOP_FIRST);
      break;
    case Y4M_ILACE_BOTTOM_FIRST:
      mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, %s\n",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above,
		  BOT_FIRST);
      break;
    default:
      mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u\n",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above);

    }
  if (input_black == 1)
    {
      mjpeg_info ("yuvscaler: with %u and %u black line above and under\n",
		  input_black_line_above, input_black_line_under);
      mjpeg_info ("yuvscaler: and %u and %u black col left and right\n",
		  input_black_col_left, input_black_col_right);
      mjpeg_info ("yuvscaler: %u %u\n", input_active_width,
		  input_active_height);
    }


  switch (output_interlaced)
    {
    case Y4M_ILACE_NONE:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s\n",
		  output_active_width, output_active_height, display_width,
		  display_height, NOT_INTER);
      break;
    case Y4M_ILACE_TOP_FIRST:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s\n",
		  output_active_width, output_active_height, display_width,
		  display_height, TOP_FIRST);
      break;
    case Y4M_ILACE_BOTTOM_FIRST:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s\n",
		  output_active_width, output_active_height, display_width,
		  display_height, BOT_FIRST);
      break;
    default:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed\n",
		  output_active_width, output_active_height, display_width,
		  display_height);
    }

  switch (algorithm)
    {
    case 0:
      mjpeg_info ("Scaling uses the %s algorithm, ", RESAMPLE);
      break;
    case 1:
      mjpeg_info ("Scaling uses the %s algorithm, ", BICUBIC);
      break;
    default:
      mjpeg_error_exit1 ("Unknown algorithm %d\n", algorithm);
    }

  switch (line_switching)
    {
    case 0:
      mjpeg_info ("without line switching\n");
      break;
    case 1:
      mjpeg_info ("with line switching\n");
      break;
    default:
      mjpeg_error_exit1 ("Unknown line switching status: %d\n",
			 line_switching);
    }



  if (black == 1)
    {
      mjpeg_info ("black lines: %u above and %u under\n",
		  output_black_line_above, output_black_line_under);
      mjpeg_info ("black columns: %u left and %u right\n",
		  output_black_col_left, output_black_col_right);
    }
  if (skip == 1)
    {
      mjpeg_info ("skipped lines: %u above and %u under\n",
		  output_skip_line_above, output_skip_line_under);
      mjpeg_info ("skipped columns: %u left and %u right\n",
		  output_skip_col_left, output_skip_col_right);
    }
  mjpeg_info ("yuvscaler frame rate: %.3f fps\n", Y4M_RATIO_DBL (frame_rate));


  divider = pgcd (input_useful_width, output_active_width);
  input_width_slice = input_useful_width / divider;
  output_width_slice = output_active_width / divider;
#ifdef DEBUG
  mjpeg_debug ("divider,i_w_s,o_w_s = %d,%d,%d\n",
	       divider, input_width_slice, output_width_slice);
#endif

  divider = pgcd (input_useful_height, output_active_height);
  input_height_slice = input_useful_height / divider;
  output_height_slice = output_active_height / divider;
#ifdef DEBUG
  mjpeg_debug ("divider,i_w_s,o_w_s = %d,%d,%d\n",
	       divider, input_height_slice, output_height_slice);
#endif

  diviseur = input_height_slice * input_width_slice;
#ifdef DEBUG
  mjpeg_debug ("Diviseur=%ld\n", diviseur);
#endif

  mjpeg_info ("Scaling ratio for width is %u to %u\n", input_width_slice,
	      output_width_slice);
  mjpeg_info ("and is %u to %u for height\n", input_height_slice,
	      output_height_slice);


  // Now that we know about scaling ratios, we can optimize treatment of an active input zone:
  // we must also check final new size is multiple of 2 on width and 2 or 4 on height
  if (input_black == 1)
    {
      if (((nb = input_black_line_above / input_height_slice) > 0)
	  && ((nb * input_height_slice) % 2 == 0))
	{
	  if (input_interlaced == Y4M_ILACE_NONE)
	    {
	      input_useful = 1;
	      black = 1;
	      black_line = 1;
	      output_black_line_above += nb * output_height_slice;
	      input_black_line_above -= nb * input_height_slice;
	      input_discard_line_above += nb * input_height_slice;
	    }
	  if ((input_interlaced != Y4M_ILACE_NONE)
	      && ((nb * input_height_slice) % 4 == 0))
	    {
	      input_useful = 1;
	      black = 1;
	      black_line = 1;
	      output_black_line_above += nb * output_height_slice;
	      input_black_line_above -= nb * input_height_slice;
	      input_discard_line_above += nb * input_height_slice;
	    }
	}
      if (((nb = input_black_line_under / input_height_slice) > 0)
	  && ((nb * input_height_slice) % 2 == 0))
	{
	  if (input_interlaced == Y4M_ILACE_NONE)
	    {
	      input_useful = 1;
	      black = 1;
	      black_line = 1;
	      output_black_line_under += nb * output_height_slice;
	      input_black_line_under -= nb * input_height_slice;
	      input_discard_line_under += nb * input_height_slice;
	    }
	  if ((input_interlaced != Y4M_ILACE_NONE)
	      && ((nb * input_height_slice) % 4 == 0))
	    {
	      input_useful = 1;
	      black = 1;
	      black_line = 1;
	      output_black_line_under += nb * output_height_slice;
	      input_black_line_under -= nb * input_height_slice;
	      input_discard_line_under += nb * input_height_slice;
	    }
	}
      if (((nb = input_black_col_left / input_width_slice) > 0)
	  && ((nb * input_height_slice) % 2 == 0))
	{
	  input_useful = 1;
	  black = 1;
	  black_col = 1;
	  output_black_col_left += nb * output_width_slice;
	  input_black_col_left -= nb * input_width_slice;
	  input_discard_col_left += nb * input_width_slice;
	}
      if (((nb = input_black_col_right / input_width_slice) > 0)
	  && ((nb * input_height_slice) % 2 == 0))
	{
	  input_useful = 1;
	  black = 1;
	  black_col = 1;
	  output_black_col_right += nb * output_width_slice;
	  input_black_col_right -= nb * input_width_slice;
	  input_discard_col_right += nb * input_width_slice;
	}
      input_useful_height =
	input_height - input_discard_line_above - input_discard_line_under;
      input_useful_width =
	input_width - input_discard_col_left - input_discard_col_right;
      input_active_width =
	input_useful_width - input_black_col_left - input_black_col_right;
      input_active_height =
	input_useful_height - input_black_line_above - input_black_line_under;
      if ((input_active_width == input_useful_width)
	  && (input_active_height == input_useful_height))
	input_black = 0;	// black zone doesn't go beyong useful zone
      output_active_width =
	(input_useful_width / input_width_slice) * output_width_slice;
      output_active_height =
	(input_useful_height / input_height_slice) * output_height_slice;

      // USER'S INFORMATION OUTPUT

//  y4m_print_stream_info(output_fd,streaminfo);
      mjpeg_info (" --- Newly speed optimized parameters ---\n");

      switch (input_interlaced)
	{
	case Y4M_ILACE_NONE:
	  mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, %s\n",
		      input_width, input_height,
		      input_useful_width, input_useful_height,
		      input_discard_col_left, input_discard_line_above,
		      NOT_INTER);
	  break;
	case Y4M_ILACE_TOP_FIRST:
	  mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, %s\n",
		      input_width, input_height,
		      input_useful_width, input_useful_height,
		      input_discard_col_left, input_discard_line_above,
		      TOP_FIRST);
	  break;
	case Y4M_ILACE_BOTTOM_FIRST:
	  mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u, %s\n",
		      input_width, input_height,
		      input_useful_width, input_useful_height,
		      input_discard_col_left, input_discard_line_above,
		      BOT_FIRST);
	  break;
	default:
	  mjpeg_info ("yuvscaler: from %ux%u, take %ux%u+%u+%u\n",
		      input_width, input_height,
		      input_useful_width, input_useful_height,
		      input_discard_col_left, input_discard_line_above);

	}
      if (input_black == 1)
	{
	  mjpeg_info
	    ("yuvscaler: with %u and %u black line above and under\n",
	     input_black_line_above, input_black_line_under);
	  mjpeg_info ("yuvscaler: and %u and %u black col left and right\n",
		      input_black_col_left, input_black_col_right);
	  mjpeg_info ("yuvscaler: %u %u\n", input_active_width,
		      input_active_height);
	}


      switch (output_interlaced)
	{
	case Y4M_ILACE_NONE:
	  mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s\n",
		      output_active_width, output_active_height,
		      display_width, display_height, NOT_INTER);
	  break;
	case Y4M_ILACE_TOP_FIRST:
	  mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s\n",
		      output_active_width, output_active_height,
		      display_width, display_height, TOP_FIRST);
	  break;
	case Y4M_ILACE_BOTTOM_FIRST:
	  mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s\n",
		      output_active_width, output_active_height,
		      display_width, display_height, BOT_FIRST);
	  break;
	default:
	  mjpeg_info ("scale to %ux%u, %ux%u being displayed\n",
		      output_active_width, output_active_height,
		      display_width, display_height);
	}

      switch (algorithm)
	{
	case 0:
	  mjpeg_info ("Scaling uses the %s algorithm, ", RESAMPLE);
	  break;
	case 1:
	  mjpeg_info ("Scaling uses the %s algorithm, ", BICUBIC);
	  break;
	default:
	  mjpeg_error_exit1 ("Unknown algorithm %d\n", algorithm);
	}

      switch (line_switching)
	{
	case 0:
	  mjpeg_info ("without line switching\n");
	  break;
	case 1:
	  mjpeg_info ("with line switching\n");
	  break;
	default:
	  mjpeg_error_exit1 ("Unknown line switching status: %d\n",
			     line_switching);
	}



      if (black == 1)
	{
	  mjpeg_info ("black lines: %u above and %u under\n",
		      output_black_line_above, output_black_line_under);
	  mjpeg_info ("black columns: %u left and %u right\n",
		      output_black_col_left, output_black_col_right);
	}
      if (skip == 1)
	{
	  mjpeg_info ("skipped lines: %u above and %u under\n",
		      output_skip_line_above, output_skip_line_under);
	  mjpeg_info ("skipped columns: %u left and %u right\n",
		      output_skip_col_left, output_skip_col_right);
	}
      mjpeg_info ("yuvscaler frame rate: %.3f fps\n",
		  Y4M_RATIO_DBL (frame_rate));
    }


// exit(1);   



  // RESAMPLE RESAMPLE RESAMPLE   
  if (algorithm == 0)
    {
      // SPECIFIC
      // Is a specific downscaling speed enhanced treatment is available?
      if ((output_width_slice == 1) && (input_width_slice == 1))
	specific = 5;
      if ((output_width_slice == 1) && (input_width_slice == 1)
	  && (input_height_slice == 4) && (output_height_slice == 3))
	specific = 7;
      if ((input_height_slice == 2) && (output_height_slice == 1))
	specific = 3;
      if ((output_height_slice == 1) && (input_height_slice == 1))
	specific = 1;
      if ((output_height_slice == 1) && (input_height_slice == 1)
	  && (output_width_slice == 2) && (input_width_slice == 3))
	specific = 6;
      if ((output_height_slice == 1) && (input_height_slice == 1)
	  && (output_width_slice == 1) && (input_width_slice == 1))
	specific = 4;
      if ((input_height_slice == 2) && (output_height_slice == 1)
	  && (input_width_slice == 2) && (output_width_slice == 1))
	specific = 2;
      if ((input_height_slice == 8) && (output_height_slice == 3))
	specific = 9;
      if ((input_height_slice == 8) && (output_height_slice == 3)
	  && (input_width_slice == 2) && (output_width_slice == 1))
	specific = 8;
      if (specific)
	mjpeg_info ("Specific downscaling routing number %u\n", specific);

      // To speed up downscaling, we tabulate the integral part of the division by "diviseur" which is inside [0;255]
      // we use uint8_t
      // But division of integers is always made by smaller => this results in a systematic drift towards smaller values. To avoid that, we need
      // a division that takes the nearest integral part. So, prior to the division by smaller, we add 1/2 of the divider to the value to be divided
      divide =
	(uint8_t *) alloca (256 * diviseur * sizeof (uint8_t) + ALIGNEMENT);
//      fprintf (stderr, "%p\n", divide);
      // alignement instructions
      if (((unsigned int) divide % ALIGNEMENT) != 0)
	divide =
	  (uint8_t *) ((((unsigned int) divide / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
//      fprintf (stderr, "%p\n", divide);

      u_c_p = divide;
      for (i = 0; i < 256 * diviseur; i++)
	{
	  *(u_c_p++) =
	    (uint8_t) floor (((double) i + (double) diviseur / 2.0)
			     / diviseur);
	  //      mjpeg_error("%ld: %d\r",i,(unsigned short int)(i/diviseur));
	}

      // Calculate averaging coefficient
      // For the height
      height_coeff =
	alloca ((input_height_slice + 1) * output_height_slice *
		sizeof (unsigned int));
      average_coeff (input_height_slice, output_height_slice, height_coeff);

      // For the width
      width_coeff =
	alloca ((input_width_slice + 1) * output_width_slice *
		sizeof (unsigned int));
      average_coeff (input_width_slice, output_width_slice, width_coeff);

    }
  // RESAMPLE RESAMPLE RESAMPLE      


  // BICUBIC BICUBIC BICUBIC  
  if (algorithm == 1)
    {

      // SPECIFIC
      // Is a specific downscaling speed enhanced treatment is available?
      if ((output_height_slice == 1) && (input_height_slice == 1))
	{
	  specific = 1;
	  bicubic_div_height = 18;
	  bicubic_offset =
	    (unsigned long int) ceil (bicubic_div_height * bicubic_div_width *
				      bicubic_negative_max * 255.0);
	  bicubic_max =
	    (unsigned long int) ceil (bicubic_div_height * bicubic_div_width *
				      bicubic_positive_max * 255.0);
	  // en hauteur, les coefficients bicubic  cubic_spline_m valent 1/18, 16/18, 1/18 et 0
	}
      if ((output_height_slice == 1) && (input_height_slice == 1)
	  && (output_width_slice == 2) && (input_width_slice == 3))
	{
	  specific = 6;
	  bicubic_div_height = 18;
	  bicubic_div_width = 144;
	  bicubic_offset =
	    (unsigned long int) ceil (bicubic_div_height * bicubic_div_width *
				      bicubic_negative_max * 255.0);
	  bicubic_max =
	    (unsigned long int) ceil (bicubic_div_height * bicubic_div_width *
				      bicubic_positive_max * 255.0);
//          fprintf(stderr,"%ld %ld %ld %ld\n",bicubic_div_height,bicubic_div_width,bicubic_offset,bicubic_max);
	  // en hauteur, les coefficients bicubic cubic_spline_m valent 1/18, 16/18, 1/18 et 0 (a=0)
	  // en largeur, les coefficients bicubic cubic_spline_n valent pour les num�ros de colonnes impaires (b=0.5) :
	  // -5/144, 77/144, 77/144 et -5/144. M�me chose qu'en hauteur pour les num�ros de colonnes paires 
	}
      if (specific)
	{
	  mjpeg_info ("Specific downscaling routing number %u\n", specific);
	  divide =
	    (uint8_t *) malloc ((bicubic_offset + bicubic_max) *
				sizeof (uint8_t) + ALIGNEMENT);
//                fprintf (stderr, "%p\n", divide);
	  // alignement instructions
	  if (((unsigned int) divide % ALIGNEMENT) != 0)
	    divide =
	      (uint8_t *) ((((unsigned int) divide / ALIGNEMENT) + 1) *
			   ALIGNEMENT);
//                fprintf (stderr, "%p\n", divide);
	  // 
	  u_c_p = divide;

	  for (lint = 0; lint < bicubic_max + bicubic_offset; lint++)
	    {
	      value =
		(lint - bicubic_offset +
		 (long int) (bicubic_div_height * bicubic_div_width / 2)) /
		(long int) (bicubic_div_height * bicubic_div_width);
	      if (value < 0)
		value = 0;
	      if (value > 255)
		value = 255;
	      *(u_c_p++) = (uint8_t) value;
	    }
	}

/*       mjpeg_info("Specific routing for BICUBIC desactivated!!!!!!\n");       
       specific = 0;
       bicubic_div_height=FLOAT2INTEGER;
       bicubic_div_width=FLOAT2INTEGER;
*/


      if (!
	  (mmx_padded =
	   (int16_t *) malloc (4 * sizeof (int16_t) + ALIGNEMENT))
	  || !(mmx_cubic =
	       (int16_t *) malloc (4 * sizeof (int16_t) + ALIGNEMENT))
	  || !(mmx_res =
	       (int32_t *) malloc (2 * sizeof (int32_t) + ALIGNEMENT)))
	mjpeg_error_exit1
	  ("Could not allocate memory for mmx registers. STOP!\n");

      mjpeg_info ("Before alignement\n");
      mjpeg_info ("%p %p %p\n", mmx_padded, mmx_cubic, mmx_res);
      mjpeg_info ("%u %u %u\n", (unsigned int) mmx_padded,
		  (unsigned int) mmx_cubic, (unsigned int) mmx_res);

      // alignement instructions
      if (((unsigned int) mmx_padded % ALIGNEMENT) != 0)
	mmx_padded =
	  (int16_t *) ((((unsigned int) mmx_padded / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
      if (((unsigned int) mmx_cubic % ALIGNEMENT) != 0)
	mmx_cubic =
	  (int16_t *) ((((unsigned int) mmx_cubic / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
      if (((unsigned int) mmx_res % ALIGNEMENT) != 0)
	mmx_res =
	  (int32_t *) ((((unsigned int) mmx_res / ALIGNEMENT) + 1) *
		       ALIGNEMENT);

      mjpeg_info ("After Alignement\n");
      mjpeg_info ("%p %p %p\n", mmx_padded, mmx_cubic, mmx_res);
      mjpeg_info ("%u %u %u\n", (unsigned int) mmx_padded,
		  (unsigned int) mmx_cubic, (unsigned int) mmx_res);


      // Then, we can also tabulate several values
      // To the output pixel of coordinates (out_col,out_line) corresponds the input pixel (in_col,in_line), in_col and in_line being the nearest smaller values.
      in_col =
	(unsigned int *) alloca (output_active_width * sizeof (unsigned int));
      b = (float *) alloca (output_active_width * sizeof (float));
      for (out_col = 0; out_col < output_active_width; out_col++)
	{
	  in_col[out_col] =
	    (out_col * input_width_slice) / output_width_slice;
	  b[out_col] =
	    (float) ((out_col * input_width_slice) % output_width_slice) /
	    (float) output_width_slice;
//             fprintf(stderr,"out_col=%u,in_col=%u,b=%f\n",out_col,in_col[out_col],b[out_col]);
	}
      // Tabulation of the height : in_line and a;
      in_line =
	(unsigned int *) alloca (output_active_height *
				 sizeof (unsigned int));
      a = (float *) alloca (output_active_height * sizeof (float));
      for (out_line = 0; out_line < output_active_height; out_line++)
	{
	  in_line[out_line] =
	    (out_line * input_height_slice) / output_height_slice;
	  a[out_line] =
	    (float) ((out_line * input_height_slice) % output_height_slice) /
	    (float) output_height_slice;
//             fprintf(stderr,"out_line=%u,in_line=%u,a=%f\n",out_line,in_line[out_line],a[out_line]);
	}
      //   return (0);
      // Tabulation of the cubic values 
      cubic_spline_n =
	(int16_t *) alloca (4 * output_active_width * sizeof (int16_t));
      cubic_spline_m =
	(int16_t *) alloca (4 * output_active_height * sizeof (int16_t));
      for (n = -1; n <= 2; n++)
	{

	  for (out_col = 0; out_col < output_active_width; out_col++)
	    {
	      cubic_spline_n[out_col + (n + 1) * output_active_width] =
		cubic_spline (b[out_col] - (float) n, bicubic_div_width);
//                     fprintf(stderr,"n=%d,out_col=%u,cubic=%ld\n",n,out_col,cubic_spline(b[out_col]-(float)n));;
	    }
	}

      // Normalisation test and normalisation
      for (out_col = 0; out_col < output_active_width; out_col++)
	{
	  somme = cubic_spline_n[out_col + 0 * output_active_width]
	    + cubic_spline_n[out_col + 1 * output_active_width]
	    + cubic_spline_n[out_col + 2 * output_active_width]
	    + cubic_spline_n[out_col + 3 * output_active_width];
	  if (somme != bicubic_div_width)
	    {
//              fprintf(stderr,"somme = %d\n",somme);
	      cubic_spline_n[out_col + 3 * output_active_width] -=
		somme - bicubic_div_width;
	    }

	}


      for (m = -1; m <= 2; m++)
	for (out_line = 0; out_line < output_active_height; out_line++)
	  {

	    cubic_spline_m[out_line + (m + 1) * output_active_height] =
	      cubic_spline ((float) m - a[out_line], bicubic_div_height);
//                  fprintf(stderr,"m=%d,out_line=%u,cubic=%ld\n",m,out_line,cubic_spline((float)m-a[out_line]));
	  }
      // Normalisation test and normalisation
      for (out_line = 0; out_line < output_active_height; out_line++)
	{
	  somme = cubic_spline_m[out_line + 0 * output_active_height]
	    + cubic_spline_m[out_line + 1 * output_active_height]
	    + cubic_spline_m[out_line + 2 * output_active_height]
	    + cubic_spline_m[out_line + 3 * output_active_height];
	  if (somme != bicubic_div_height)
	    {
//              fprintf(stderr,"somme = %d\n",somme);
	      cubic_spline_m[out_line + 3 * output_active_height] -=
		somme - bicubic_div_height;
	    }

	}

      if ((output_interlaced == Y4M_ILACE_NONE)
	  || (input_interlaced == Y4M_ILACE_NONE))
	padded_input =
	  (uint8_t *) alloca ((input_useful_width + 3) *
			      (input_useful_height + 3));
      else
	{
	  padded_top =
	    (uint8_t *) alloca ((input_useful_width + 3) *
				(input_useful_height / 2 + 3));
	  padded_bottom =
	    (uint8_t *) alloca ((input_useful_width + 3) *
				(input_useful_height / 2 + 3));
	}
    }
  // BICUBIC BICUBIC BICUBIC     

  // Pointers allocations
//  input = alloca ((input_width * input_height * 3) / 2);
//  output = alloca ((output_width * output_height * 3) / 2);
  line = alloca (input_width);
  input = alloca (((input_width * input_height * 3) / 2) + ALIGNEMENT);
  output = alloca (((output_width * output_height * 3) / 2) + ALIGNEMENT);
//  fprintf (stderr, "%p %p\n", input, output);
  if (((unsigned int) input % ALIGNEMENT) != 0)
    input =
      (uint8_t *) ((((unsigned int) input / ALIGNEMENT) + 1) * ALIGNEMENT);
  if (((unsigned int) output % ALIGNEMENT) != 0)
    output =
      (uint8_t *) ((((unsigned int) output / ALIGNEMENT) + 1) * ALIGNEMENT);
//  fprintf (stderr, "%p %p\n", input, output);

  // if skip_col==1
  frame_y_p = (uint8_t **) alloca (display_height * sizeof (uint8_t *));
  frame_u_p = (uint8_t **) alloca (display_height / 2 * sizeof (uint8_t *));
  frame_v_p = (uint8_t **) alloca (display_height / 2 * sizeof (uint8_t *));

  // Incorporate blacks lines and columns directly into output matrix since this will never change. 
  // BLACK pixel in YUV = (16,128,128)
  if (black == 1)
    {
      u_c_p = output;
      // Y component
      for (i = 0; i < output_black_line_above * output_width; i++)
	*(u_c_p++) = blacky;
      if (black_col == 0)
	u_c_p += output_active_height * output_width;
      else
	{
	  for (i = 0; i < output_active_height; i++)
	    {
	      for (j = 0; j < output_black_col_left; j++)
		*(u_c_p++) = blacky;
	      u_c_p += output_active_width;
	      for (j = 0; j < output_black_col_right; j++)
		*(u_c_p++) = blacky;
	    }
	}
      for (i = 0; i < output_black_line_under * output_width; i++)
	*(u_c_p++) = blacky;

      // U component
      //   u_c_p=output+output_width*output_height;
      for (i = 0; i < output_black_line_above / 2 * output_width / 2; i++)
	*(u_c_p++) = blackuv;
      if (black_col == 0)
	u_c_p += output_active_height / 2 * output_width / 2;
      else
	{
	  for (i = 0; i < output_active_height / 2; i++)
	    {
	      for (j = 0; j < output_black_col_left / 2; j++)
		*(u_c_p++) = blackuv;
	      u_c_p += output_active_width / 2;
	      for (j = 0; j < output_black_col_right / 2; j++)
		*(u_c_p++) = blackuv;
	    }
	}
      for (i = 0; i < output_black_line_under / 2 * output_width / 2; i++)
	*(u_c_p++) = blackuv;

      // V component
      //   u_c_p=output+(output_width*output_height*5)/4;
      for (i = 0; i < output_black_line_above / 2 * output_width / 2; i++)
	*(u_c_p++) = blackuv;
      if (black_col == 0)
	u_c_p += output_active_height / 2 * output_width / 2;
      else
	{
	  for (i = 0; i < output_active_height / 2; i++)
	    {
	      for (j = 0; j < output_black_col_left / 2; j++)
		*(u_c_p++) = blackuv;
	      u_c_p += output_active_width / 2;
	      for (j = 0; j < output_black_col_right / 2; j++)
		*(u_c_p++) = blackuv;
	    }
	}
      for (i = 0; i < output_black_line_under / 2 * output_width / 2; i++)
	*(u_c_p++) = blackuv;
    }

  // MONOCHROME FRAMES
  if (mono == 1)
    {
      // the U and V components of output frame will always be 128
      u_c_p = output + output_width * output_height;
      for (i = 0; i < 2 * output_width / 2 * output_height / 2; i++)
	*(u_c_p++) = blackuv;
    }


  // Various initialisatiosn for input and output   
  out_nb_col_slice = output_active_width / output_width_slice;
  out_nb_line_slice = output_active_height / output_height_slice;
  input_y =
    input + input_discard_line_above * input_width + input_discard_col_left;
  input_u =
    input + input_width * input_height +
    input_discard_line_above / 2 * input_width / 2 +
    input_discard_col_left / 2;
  input_v =
    input + (input_height * input_width * 5) / 4 +
    input_discard_line_above / 2 * input_width / 2 +
    input_discard_col_left / 2;
  output_y =
    output + output_black_line_above * output_width + output_black_col_left;
  output_u =
    output + output_width * output_height +
    output_black_line_above / 2 * output_width / 2 +
    output_black_col_left / 2;
  output_v =
    output + (output_width * output_height * 5) / 4 +
    output_black_line_above / 2 * output_width / 2 +
    output_black_col_left / 2;

  // Other initialisations for frame output
  frame_y =
    output + output_skip_line_above * output_width + output_skip_col_left;
  frame_u =
    output + output_width * output_height +
    output_skip_line_above / 2 * output_width / 2 + output_skip_col_left / 2;
  frame_v =
    output + (output_width * output_height * 5) / 4 +
    output_skip_line_above / 2 * output_width / 2 + output_skip_col_left / 2;
  if (skip_col == 1)
    {
      for (i = 0; i < display_height; i++)
	frame_y_p[i] = frame_y + i * output_width;
      for (i = 0; i < display_height / 2; i++)
	{
	  frame_u_p[i] = frame_u + i * output_width / 2;
	  frame_v_p[i] = frame_v + i * output_width / 2;
	}
    }

  nb_pixels = (input_width * input_height * 3) / 2;
  mjpeg_debug ("End of Initialisation\n");
  // END OF INITIALISATION

  // Output file header
  // Should use functions y4m_copy, but I didn't find them inside yuv4mpeg.c (16 Sept 2001)
  y4m_si_set_width (&streaminfo, display_width);
  y4m_si_set_height (&streaminfo, display_height);
  y4m_si_set_interlace (&streaminfo, output_interlaced);
  if (no_header == 0)
    y4m_write_stream_header (output_fd, &streaminfo);

//   sprintf(header,"YUV4MPEG %3d %3d %1d\n", display_width, display_height,frame_rate_code);
//   if (!fwrite_complete (header, 19, out_file))
//     goto out_error;

  if (infile == 0)
    {
      // input comes from stdin
      // Master loop : continue until there is no next frame in stdin
      // Je sais pas pourquoi, mais y4m_read_frame merde, y4m_read_frame_header suivi de y4m_read marche !!!!!!!
      // Line switch if necessary
      while (my_y4m_read_frame
	     (0, &frameinfo, nb_pixels, input, line_switching) == Y4M_OK)
	{
	  frame_num++;
	  mjpeg_info ("yuvscaler Frame number %ld\r", frame_num);

	  // Output Frame Header
	  if (y4m_write_frame_header (output_fd, &frameinfo) != Y4M_OK)
	    goto out_error;

	  // Blackout if necessary
	  if (input_black == 1)
	    blackout (input_y, input_u, input_v);

	  // SCALE THE FRAME
	  // RESAMPLE ALGO       
	  if (algorithm == 0)
	    {
	      if (mono == 1)
		{
		  if (!specific)
		    average (input_y, output_y, height_coeff, width_coeff, 0);
		  else
		    average_specific (input_y, output_y, height_coeff,
				      width_coeff, 0);
		}
	      else
		{
		  if (!specific)
		    {
		      average (input_y, output_y, height_coeff, width_coeff,
			       0);
		      average (input_u, output_u, height_coeff, width_coeff,
			       1);
		      average (input_v, output_v, height_coeff, width_coeff,
			       1);
		    }
		  else
		    {
		      average_specific (input_y, output_y, height_coeff,
					width_coeff, 0);
		      average_specific (input_u, output_u, height_coeff,
					width_coeff, 1);
		      average_specific (input_v, output_v, height_coeff,
					width_coeff, 1);
		    }
		}
	    }
	  // RESAMPLE ALGO
	  // BICIBIC ALGO
	  if (algorithm == 1)
	    {
	      // INPUT FRAME PADDING BEFORE BICUBIC INTERPOLATION
	      // PADDING IS DONE SEPARATELY FOR EACH COMPONENT
	      // 
	      if (mono == 1)
		{
		  if ((output_interlaced == Y4M_ILACE_NONE)
		      || (input_interlaced == Y4M_ILACE_NONE))
		    {
		      padding (padded_input, input_y, 0);
		      cubic_scale (padded_input, output_y, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 0);
		    }
		  else
		    {
		      padding_interlaced (padded_top, padded_bottom, input_y,
					  0);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_y, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      0);
		    }
		}
	      else
		{
		  if ((output_interlaced == Y4M_ILACE_NONE)
		      || (input_interlaced == Y4M_ILACE_NONE))
		    {
		      padding (padded_input, input_y, 0);
		      cubic_scale (padded_input, output_y, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 0);
		      padding (padded_input, input_u, 1);
		      cubic_scale (padded_input, output_u, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 1);
		      padding (padded_input, input_v, 1);
		      cubic_scale (padded_input, output_v, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 1);
		    }
		  else
		    {
		      padding_interlaced (padded_top, padded_bottom, input_y,
					  0);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_y, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      0);
		      padding_interlaced (padded_top, padded_bottom, input_u,
					  1);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_u, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      1);
		      padding_interlaced (padded_top, padded_bottom, input_v,
					  1);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_v, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      1);
		    }
		}
	    }
	  // BICIBIC ALGO


	  // OUTPUT FRAME CONTENTS
	  if (skip == 0)
	    {
	      // Here, display=output_active 
//            mjpeg_debug("1\n");
	      if (y4m_write
		  (output_fd, output,
		   (display_width * display_height * 3) / 2) != Y4M_OK)
		goto out_error;
	    }
	  else
	    {
	      // skip == 1
	      if (skip_col == 0)
		{
		  // output_active_width==display_width, component per component frame output
		  if (y4m_write
		      (output_fd, frame_y,
		       display_width * display_height) != Y4M_OK)
		    goto out_error;
		  if (y4m_write
		      (output_fd, frame_u,
		       display_width / 2 * display_height / 2) != Y4M_OK)
		    goto out_error;
		  if (y4m_write
		      (output_fd, frame_v,
		       display_width / 2 * display_height / 2) != Y4M_OK)
		    goto out_error;
		}
	      else
		{
		  // output_active_width > display_width, line per line frame output for each component
		  for (i = 0; i < display_height; i++)
		    {
		      if (y4m_write (output_fd, frame_y_p[i], display_width)
			  != Y4M_OK)
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (y4m_write
			  (output_fd, frame_u_p[i],
			   display_width / 2) != Y4M_OK)
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (y4m_write
			  (output_fd, frame_v_p[i],
			   display_width / 2) != Y4M_OK)
			goto out_error;

		    }
		}
	    }
	}
      // End of master loop
      if (err != Y4M_ERR_EOF)
	mjpeg_info ("Couldn't read FRAME number %ld!\n", frame_num);
      else
	mjpeg_info ("End of stream with FRAME number %ld!\n", frame_num);
    }
  else
    {
      // input comes from files: frame by frame read 
      input_y_infile = input;
      input_u_infile = input + input_width * input_height;
      input_v_infile = input + (input_width * input_height * 5) / 4;
      for (frame_num = 0; frame_num < el.video_frames; frame_num++)
	{
	  // Read frame, taken from lav2yuv
	  len = el_get_video_frame (jpeg_data, frame_num, &el);
	  // Warning: we substitute el.video_inter with input_interlaced since el.video_inter value 
	  // may have been overwritten by user's specification
	  if ((res =
	       decode_jpeg_raw (jpeg_data, len, input_interlaced,
				chroma_format, input_width, input_height,
				input_y_infile, input_u_infile,
				input_v_infile)))
	    mjpeg_error_exit1 ("Frame %ld read failed\n", frame_num);
	  mjpeg_info ("yuvscaler Frame number %ld\r", frame_num);

	  // Line switch if necessary
	  if (line_switching)
	    line_switch (input, line);
	  // Output Frame Header
	  if (y4m_write_frame_header (output_fd, &frameinfo) != Y4M_OK)
	    goto out_error;

	  // Blackout if necessary
	  if (input_black == 1)
	    blackout (input_y, input_u, input_v);

	  // SCALE THE FRAME
	  // RESAMPLE ALGO       
	  if (algorithm == 0)
	    {
	      if (mono == 1)
		{
		  if (!specific)
		    average (input_y, output_y, height_coeff, width_coeff, 0);
		  else
		    average_specific (input_y, output_y, height_coeff,
				      width_coeff, 0);
		}
	      else
		{
		  if (!specific)
		    {
		      average (input_y, output_y, height_coeff, width_coeff,
			       0);
		      average (input_u, output_u, height_coeff, width_coeff,
			       1);
		      average (input_v, output_v, height_coeff, width_coeff,
			       1);
		    }
		  else
		    {
		      average_specific (input_y, output_y, height_coeff,
					width_coeff, 0);
		      average_specific (input_u, output_u, height_coeff,
					width_coeff, 1);
		      average_specific (input_v, output_v, height_coeff,
					width_coeff, 1);
		    }
		}
	    }
	  // RESAMPLE ALGO
	  // BICIBIC ALGO
	  if (algorithm == 1)
	    {
	      // INPUT FRAME PADDING BEFORE BICUBIC INTERPOLATION
	      // PADDING IS DONE SEPARATELY FOR EACH COMPONENT
	      // 
	      if (mono == 1)
		{
		  if ((output_interlaced == Y4M_ILACE_NONE)
		      || (input_interlaced == Y4M_ILACE_NONE))
		    {
		      padding (padded_input, input_y, 0);
		      cubic_scale (padded_input, output_y, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 0);
		    }
		  else
		    {
		      padding_interlaced (padded_top, padded_bottom, input_y,
					  0);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_y, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      0);
		    }
		}
	      else
		{
		  if ((output_interlaced == Y4M_ILACE_NONE)
		      || (input_interlaced == Y4M_ILACE_NONE))
		    {
		      padding (padded_input, input_y, 0);
		      cubic_scale (padded_input, output_y, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 0);
		      padding (padded_input, input_u, 1);
		      cubic_scale (padded_input, output_u, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 1);
		      padding (padded_input, input_v, 1);
		      cubic_scale (padded_input, output_v, in_col, b, in_line,
				   a, cubic_spline_n, cubic_spline_m, 1);
		    }
		  else
		    {
		      padding_interlaced (padded_top, padded_bottom, input_y,
					  0);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_y, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      0);
		      padding_interlaced (padded_top, padded_bottom, input_u,
					  1);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_u, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      1);
		      padding_interlaced (padded_top, padded_bottom, input_v,
					  1);
		      cubic_scale_interlaced (padded_top, padded_bottom,
					      output_v, in_col, b, in_line, a,
					      cubic_spline_n, cubic_spline_m,
					      1);
		    }
		}
	    }
	  // BICIBIC ALGO

	  // OUTPUT FRAME
	  if (skip == 0)
	    {
	      // Here, display=output_active 
	      if (y4m_write
		  (output_fd, output,
		   (display_width * display_height * 3) / 2) != Y4M_OK)
		goto out_error;
	    }
	  else
	    {
	      // skip == 1
	      if (skip_col == 0)
		{
		  // output_active_width==display_width, component per component frame output
		  if (y4m_write
		      (output_fd, frame_y,
		       display_width * display_height) != Y4M_OK)
		    goto out_error;
		  if (y4m_write
		      (output_fd, frame_u,
		       display_width / 2 * display_height / 2) != Y4M_OK)
		    goto out_error;
		  if (y4m_write
		      (output_fd, frame_v,
		       display_width / 2 * display_height / 2) != Y4M_OK)
		    goto out_error;
		}
	      else
		{
		  // output_active_width > display_width, line per line frame output for each component
		  for (i = 0; i < display_height; i++)
		    {
		      if (y4m_write (output_fd, frame_y_p[i], display_width)
			  != Y4M_OK)
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (y4m_write
			  (output_fd, frame_u_p[i],
			   display_width / 2) != Y4M_OK)
			goto out_error;
		    }

		  for (i = 0; i < display_height / 2; i++)
		    {
		      if (y4m_write
			  (output_fd, frame_v_p[i],
			   display_width / 2) != Y4M_OK)
			goto out_error;

		    }
		}
	    }
	  mjpeg_debug ("yuvscaler Frame number %ld\r", frame_num);
	}
      // End of master loop
    }
  y4m_fini_stream_info (&streaminfo);
  y4m_fini_frame_info (&frameinfo);
  return 0;

out_error:
  mjpeg_error_exit1 ("Unable to write to output - aborting!\n");
  return 1;
}


// *************************************************************************************
unsigned int
pgcd (unsigned int num1, unsigned int num2)
{
  // Calculates the biggest common divider between num1 and num2, after Euclid's
  // pgdc(a,b)=pgcd(b,a%b)
  // My thanks to Chris Atenasio <chris@crud.net>
  unsigned int c, bigger, smaller;

  if (num2 < num1)
    {
      smaller = num2;
      bigger = num1;
    }
  else
    {
      smaller = num1;
      bigger = num2;
    }

  while (smaller)
    {
      c = bigger % smaller;
      bigger = smaller;
      smaller = c;
    }
  return (bigger);
}

// *************************************************************************************



// *************************************************************************************
/*
For interlaced treatment, line numbers may be switched as a function of the interlacing type of the image.
So, if line_index varies from 0 to output_active_height, input line number is 2*(line_index/2)*input_height_slice + (line_switching+line_index)%2
and output line number is 2*(line_index/2)*output_height_slice + (0+line_index)%2
For speed reason, /half is replaced by >>half, 2*(line_index/2) by line_index&~1. Please note that %2 or &~1 take the same amount of time
Please note that interlaced==0 (non-interlaced) or interlaced==2 (even interlaced) are treated alike
       "\t if frames come from stdin, input frames interlacing type is not known from header. For interlacing specification, use:\n"
       "\t NOT_INTERLACED to select not interlaced input frames\n"
       "\t INTERLACED_ODD_FIRST  to select an interlaced, odd  first frame input stream from stdin\n"
       "\t INTERLACED_EVEN_FIRST to select an interlaced, even first frame input stream from stdin\n"
       "\t If you wish to specify interlacing of output frames, use:\n"
       "\t INTERLACED_TOP_FIRST  to select an interlaced, top first frame output stream\n"
       "\t INTERLACED_BOTTOM_FIRST to select an interlaced, bottom first frame output stream\n"
       "\t NOT_INTERLACED to select not interlaced output frames\n"
*/
