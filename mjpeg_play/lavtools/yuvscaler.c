/*
  *  yuvscaler.c
  *  Copyright (C) 2001 Xavier Biquard <xbiquard@free.fr>
  * 
  *  
  *  Scales arbitrary sized yuv frame to yuv frames suitable for VCD, SVCD or specified
  * 
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  */
// Implementation: there are two scaling methods: one for not_interlaced output and one for interlaced output. 
// For each method, line switching may occur on input frames (by convention, line are numbered from 0 to n-1)
// 
// First version doing only downscaling with no interlacing
// June 2001: interlacing capable version
// July 2001: upscaling capable version
// September 2001: line switching
// September/October 2001: new yuv4mpeg header
// October 2001: first MMX part for bicubic
// September/November 2001: what a mess this code! => cleaning and splitting
// December 2001: implementation of time reordering of frames
// January 2002: sample aspect ratio calculation by Matto
// February 2002: interlacing specification now possible. Replaced alloca with malloc
// Mars 2002: sample aspect ratio calculations are back!
// May/June 2002: remove file reading capabilities (do not duplicate lav2yuv), add -O DVD, add color chrominance correction 
// as well as luminance linear reequilibrium. Lots of code cleaning, function renaming, etc...
// Keywords concerning interlacing/preprocessing now under INPUT case
//  
// TODO:
// no more global variables for librarification
// a really working MMX subroutine for bicubic
// bott_forward, top_forward, luma, chroma, blackout included inside read_frame (all preprocessing)
// Slow down possibility at 1:2 => all preprocessing in a new utility called yuvcorrect

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
#include "mjpeg_types.h"
#include "yuvscaler.h"
#include "mpegconsts.h"
// #include "config.h"
#include "attributes.h"
#include "../utils/mmx.h"



#define YUVSCALER_VERSION LAVPLAY_VERSION
// For pointer adress alignement
#define ALIGNEMENT 16		// 16 bytes alignement for mmx registers in SIMD instructions for Pentium

// For input
unsigned int input_width;
unsigned int input_height;
y4m_ratio_t input_sar;		// see yuv4mpeg.h and yuv4mpeg_intern.h for possible values
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
// =Y4M_ILACE_NONE for progressive/not-interlaced, 
// =Y4M_ILACE_TOP_FIRST for top interlaced, 
// =Y4M_ILACE_BOTTOM_FIRST for bottom interlaced
uint8_t input_y_min, input_y_max;
float Ufactor, Vfactor;

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
// =Y4M_ILACE_NONE for progressive/not-interlaced, 
// =Y4M_ILACE_TOP_FIRST for top interlaced, 
// =Y4M_ILACE_BOTTOM_FIRST for bottom interlaced
int output_fd = 1;		// frames are written to stdout
unsigned int vcd = 0;		//=1 if vcd output was selected
unsigned int svcd = 0;		//=1 if svcd output was selected
unsigned int dvd = 0;		//=1 if dvd output was selected
unsigned int luma = 0;		//=1 if luminance correction is selected
uint8_t output_y_min, output_y_max;
unsigned int chroma = 0;	//=1 if chrominance correction is selected
uint8_t Ucenter, Vcenter, UVmin, UVmax;



// Global variables
int line_switching = 0;		// =0 for no line switching, =1 for line switching
int norm = -1;			// =0 for PAL and =1 for NTSC
int wide = 0;			// =1 for wide (16:9) input to standard (4:3) output conversion
int ratio = 0;
int size_keyword = 0;		// =1 is the SIZE keyword has been used
int infile = 0;			// =0 for stdin (default) =1 for file
int algorithm = -1;		// =0 for resample, and =1 for bicubic
unsigned int specific = 0;	// is >0 if a specific downscaling speed enhanced treatment of data is possible
unsigned int mono = 0;		// is =1 for monochrome output

// Keywords for argument passing 
const char VCD_KEYWORD[] = "VCD";
const char SVCD_KEYWORD[] = "SVCD";
const char DVD_KEYWORD[] = "DVD";
const char SIZE_KEYWORD[] = "SIZE_";
const char USE_KEYWORD[] = "USE_";
const char WIDE2STD_KEYWORD[] = "WIDE2STD";
const char INFILE_KEYWORD[] = "INFILE_";
const char RATIO_KEYWORD[] = "RATIO_";
const char TOP_FIRST[] = "INTERLACED_TOP_FIRST";
const char BOT_FIRST[] = "INTERLACED_BOTTOM_FIRST";
const char NOT_INTER[] = "NOT_INTERLACED";
const char PROGRESSIVE[] = "PROGRESSIVE";
const char MONO_KEYWORD[] = "MONOCHROME";
const char FASTVCD[] = "FASTVCD";
const char FAST_WIDE2VCD[] = "FAST_WIDE2VCD";
const char WIDE2VCD[] = "WIDE2VCD";
const char RESAMPLE[] = "RESAMPLE";
const char BICUBIC[] = "BICUBIC";
const char LINESWITCH[] = "LINE_SWITCH";
const char ACTIVE[] = "ACTIVE";
const char NO_HEADER[] = "NO_HEADER";
#ifdef HAVE_ASM_MMX
const char MMX[] = "MMX";
#endif
const char TOP_FORWARD[] = "TOP_FORWARD";
const char BOTT_FORWARD[] = "BOTT_FORWARD";
const char LUMINANCE[] = "LUMINANCE_";
const char CHROMINANCE[] = "CHROMINANCE_";

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
const static char *legal_opt_flags = "k:I:d:n:v:M:m:O:whtg";
int verbose = 1;
#define PARAM_LINE_MAX 256

uint8_t blacky = 16;
uint8_t blackuv = 128;
uint8_t no_header = 0;		// =1 for no stream header output 
int8_t field_move = 0;		// =1 to move bottom field one frame forward, =-1 to move top field forward one frame

#ifdef HAVE_ASM_MMX
// MMX
int16_t *mmx_padded, *mmx_cubic;
int32_t *mmx_res;
long int max_lint_neg = -2147483647;	// maximal negative number available for long int
int mmx = 0;			// =1 for mmx activated
#endif


// *************************************************************************************
void
yuvstat (uint8_t * input)
{
  uint8_t y, u, v;
  int r, g, b;
  unsigned long int somme_y = 0, somme_u = 0, somme_v = 0, moy_y = 0, moy_u =
    0, moy_v = 0;
  uint16_t histo_y[256], histo_u[256], histo_v[256];
  uint16_t histo_r[256], histo_g[256], histo_b[256];
  unsigned long int i;
  unsigned long int decalage = input_width * input_height / 4;
  unsigned long int decalage_y_u = decalage * 4;
  unsigned long int decalage_y_v = decalage * 5;
  for (i = 0; i < 256; i++)
    histo_y[i] = histo_u[i] = histo_v[i] = histo_r[i] = histo_g[i] =
      histo_b[i] = 0;
  for (i = 0; i < decalage; i++)
    {
      y = input[i * 4];
      u = input[i + decalage_y_u];
      v = input[i + decalage_y_v];
      histo_y[y]++;
      histo_u[u]++;
      histo_v[v]++;
      r = (int) y + (int) floor (1.375 * (float) (v - 128));
      g = (int) y + (int) floor (-0.698 * (v - 128) - 0.336 * (u - 128));
      b = (int) y + (int) floor (1.732 * (float) (u - 128));
      if ((r < 0) || (r > 255))
	{
//           mjpeg_debug("r out of range %03d, %03u, %03u, %03u",r,y,u,v);
	  if (r < 0)
	    r = 0;
	  else
	    r = 255;
	}
      if ((g < 0) || (g > 255))
	{
//           mjpeg_debug("g out of range %03d, %03u, %03u, %03u",g,y,u,v);
	  if (g < 0)
	    g = 0;
	  else
	    g = 255;
	}
      if ((b < 0) || (b > 255))
	{
//           mjpeg_debug("b out of range %03d, %03u, %03u, %03u",b,y,u,v);
	  if (b < 0)
	    b = 0;
	  else
	    b = 255;
	}
      histo_r[r]++;
      histo_g[g]++;
      histo_b[b]++;
    }
  mjpeg_debug ("Histogramme\ni Y U V");
  for (i = 0; i < 256; i++)
    {
      mjpeg_debug ("%03lu %04u %04u %04u", i, histo_y[i], histo_u[i],
		   histo_v[i]);
      somme_y += histo_y[i];
      somme_u += histo_u[i];
      somme_v += histo_v[i];
    }
  i = 0;
  while (moy_y < somme_y / 2)
    moy_y += histo_y[i++];
  moy_y = i;
  i = 0;
  while (moy_u < somme_u / 2)
    moy_u += histo_u[i++];
  moy_u = i;
  i = 0;
  while (moy_v < somme_v / 2)
    moy_v += histo_v[i++];
  moy_v = i;

  mjpeg_debug ("moyY=%03lu moyU=%03lu moyV=%03lu", moy_y, moy_u, moy_v);


  mjpeg_debug ("Histogramme\ni R G B");
  for (i = 0; i < 256; i++)
    mjpeg_debug ("%03lu %04u %04u %04u", i, histo_r[i], histo_g[i],
		 histo_b[i]);

}

// *************************************************************************************



// *************************************************************************************
void
yuvscaler_print_usage (char *argv[])
{
  fprintf (stderr,
	   "usage: yuvscaler -I [input_keyword] -M [mode_keyword] -O [output_keyword] [-n p|s|n] [-v 0-2] [-h]\n"
	   "yuvscaler UPscales or DOWNscales arbitrary-sized YUV frames coming from stdin (in YUV4MPEG 4:2:2 format)\n"
	   "to a specified YUV frame sizes to stdout\n"
	   "yuvscaler implements two dowscaling algorithms but only one upscaling algorithm\n"
	   "\n"
	   "yuvscaler is keyword driven :\n"
	   "\t -I for keyword concerning INPUT  frame characteristics\n"
	   "\t -M for keyword concerning the scaling MODE of yuvscaler\n"
	   "\t -O for keyword concerning OUTPUT frame characteristics\n"
	   "By default, yuvscaler will keep input frames characteristics, that is same size, same interlacing type\n"
	   "and same sample aspect ratio\n"
	   "\n"
	   "Possible input keyword are:\n"
	   "\t USE_WidthxHeight+WidthOffset+HeightOffset to select a useful area of the input frame (all multiple of 2,\n"
	   "\t    Height and HeightOffset multiple of 4 if interlaced), the rest of the image being discarded\n"
	   "\t ACTIVE_WidthxHeight+WidthOffset+HeightOffset to select an active area of the input frame (all multiple of 2,\n"
	   "\t    Height and HeightOffset multiple of 4 if interlaced), the rest of the image being made black\n"
	   "\t If you suspect that your video capture was given a wrong interlacing type,\n"
	   "\t spatially or temporarly missed up, please use and combine:\n"
	   "\t INTERLACED_TOP_FIRST    to specify top_field_first as input interlacing\n"
	   "\t INTERLACED_BOTTOM_FIRST to specify bottom_field_first as input interlacing\n"
	   "\t NOT_INTERLACED          or PROGRESSIVE to specify not-interlaced/progressive as input interlacing\n"
	   "\t LINE_SWITCH  to preprocess frames by switching lines two by two\n"
	   "\t BOTT_FORWARD to preprocess frames by moving bott field one frame forward\n"
	   "\t TOP_FORWARD  to preprocess frames by moving top  field one frame forward\n"
	   "\t If you want to correct the look of your image, you may use\n"
	   "\t LUMINANCE_InputYmin_InputYmax_OutputYmin_OutputYmax to linearly correct the input frame luminance by clipping it\n"
	   "\t    inside range [InputYmin;InputYmax] and scale this range to [OutputYmin;OutputYmax]\n"
	   "\t CHROMINANCE_Ufactor_Ucenter_Vfactor_Vcenter_UVmin_UVmax to multiply the input frame chroma component U-Ucenter or V-Vcenter\n"
	   "\t     by (float) Ufactor or Vfactor, recenter it to the normalize 128 center, and clip the result to range [UVmin;UVmax]\n"
	   "\t R_InRmin_InRmax_OutRmin_OutRmax\n"
	   "\t G_InGmin_InGmax_OutGmin_OutGmax\n"
	   "\t B_InBmin_InBmax_OutBmin_OutBmax\n"
	   "\n"
	   "Possible mode keyword are:\n"
	   "\t BICUBIC       to use the (Mitchell-Netravalli) high-quality bicubic upscaling and/or downscaling algorithm\n"
	   "\t RESAMPLE      to use a classical resampling algorithm -only for downscaling- that goes much faster than bicubic\n"
	   "\t For coherence reason, yuvscaler will use RESAMPLE if only downscaling is necessary, BICUBIC otherwise\n"
	   "\t WIDE2STD      to converts widescreen (16:9) input frames to standard output (4:3), generating necessary black lines\n"
	   "\t RATIO_WidthIn_WidthOut_HeightIn_HeightOut to specified conversion ratios of\n"
	   "\t     WidthIn/WidthOut for width and HeightIN/HeightOut for height to be applied to the useful area.\n"
	   "\t     The output active area that results from scaling the input useful area might be different\n"
	   "\t     from the display area specified thereafter using the -O KEYWORD syntax.\n"
	   "\t     In that case, yuvscaler will automatically generate necessary black lines and columns and/or skip necessary\n"
	   "\t     lines and columns to get an active output centered within the display size.\n"
	   "\t WIDE2VCD      to transcode wide (16:9) frames  to VCD (equivalent to -M WIDE2STD -O VCD)\n"
	   "\t FASTVCD       to transcode full sized frames to VCD (equivalent to -M RATIO_2_1_2_1 -O VCD)\n"
	   "\t FAST_WIDE2VCD to transcode full sized wide (16:9) frames to VCD (-M WIDE2STD -M RATIO_2_1_2_1 -O VCD)\n"
	   "\t NO_HEADER     to suppress stream header generation on output (chaining yuvscaler conversions)\n"
#ifdef HAVE_ASM_MMX
	   "\t MMX to use MMX functions for BICUBIC scaling (experimental feature!!)\n"
#endif
	   "\n"
	   "Possible output keywords are:\n"
	   "\t MONOCHROME to generate monochrome frames on output\n"
	   "\t  VCD to generate  VCD compliant frames, taking care of PAL and NTSC standards, not-interlaced/progressive frames\n"
	   "\t SVCD to generate SVCD compliant frames, taking care of PAL and NTSC standards, any interlacing types\n"
	   "\t  DVD to generate  DVD compliant frames, taking care of PAL and NTSC standards, any interlacing types\n"
	   "\t      (SVCD and DVD: if input is not-interlaced/progressive, output interlacing will be taken as top_first)\n"
	   "\t SIZE_WidthxHeight to generate frames of size WidthxHeight on output (multiple of 2, Height of 4 if interlaced)\n"
	   "\t if output supports any kind of interlacing (like SVCD and DVD), and you want to always get the same\n"
	   "\t interlacing type regardless of input type, use the following three keywords:\n"
	   "\t INTERLACED_TOP_FIRST to get top_field_first output interlacing\n"
	   "\t INTERLACED_BOTTOM_FIRST to get bottom_field_first output interlacing\n"
	   "\t NOT_INTERLACED or PROGRESSIVE to get not-interlaced/progressive interlacing\n"
	   "\n"
	   "-n  (usually not necessary) if norm could not be determined from data flux, specifies the OUTPUT norm for VCD/SVCD p=pal,s=secam,n=ntsc\n"
	   "-v  Specifies the degree of verbosity: 0=quiet, 1=normal, 2=verbose/debug\n"
	   "-h : print this lot!\n");
  exit (1);
}

// *************************************************************************************


// *************************************************************************************
void
yuvscaler_print_information (y4m_stream_info_t in_streaminfo,
			     y4m_ratio_t frame_rate)
{
  // This function print USER'S INFORMATION

  y4m_log_stream_info (LOG_INFO, "input: ", &in_streaminfo);
  //  y4m_print_stream_info(output_fd,streaminfo);

  switch (input_interlaced)
    {
    case Y4M_ILACE_NONE:
      mjpeg_info ("from %ux%u, take %ux%u+%u+%u, %s/%s",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above,
		  NOT_INTER, PROGRESSIVE);
      break;
    case Y4M_ILACE_TOP_FIRST:
      mjpeg_info ("from %ux%u, take %ux%u+%u+%u, %s",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above,
		  TOP_FIRST);
      break;
    case Y4M_ILACE_BOTTOM_FIRST:
      mjpeg_info ("from %ux%u, take %ux%u+%u+%u, %s",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above,
		  BOT_FIRST);
      break;
    default:
      mjpeg_info ("from %ux%u, take %ux%u+%u+%u",
		  input_width, input_height,
		  input_useful_width, input_useful_height,
		  input_discard_col_left, input_discard_line_above);

    }
  if (input_black == 1)
    {
      mjpeg_info ("with %u and %u black line above and under",
		  input_black_line_above, input_black_line_under);
      mjpeg_info ("and %u and %u black col left and right",
		  input_black_col_left, input_black_col_right);
      mjpeg_info ("%u %u", input_active_width, input_active_height);
    }


  switch (output_interlaced)
    {
    case Y4M_ILACE_NONE:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s/%s",
		  output_active_width, output_active_height, display_width,
		  display_height, NOT_INTER, PROGRESSIVE);
      break;
    case Y4M_ILACE_TOP_FIRST:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s",
		  output_active_width, output_active_height, display_width,
		  display_height, TOP_FIRST);
      break;
    case Y4M_ILACE_BOTTOM_FIRST:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed, %s",
		  output_active_width, output_active_height, display_width,
		  display_height, BOT_FIRST);
      break;
    default:
      mjpeg_info ("scale to %ux%u, %ux%u being displayed",
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
      mjpeg_error_exit1 ("Unknown algorithm %d", algorithm);
    }

  switch (line_switching)
    {
    case 0:
      mjpeg_info ("without line switching");
      break;
    case 1:
      mjpeg_info ("with line switching");
      break;
    default:
      mjpeg_error_exit1 ("Unknown line switching status: %d", line_switching);
    }

  switch (field_move)
    {
    case 0:
      mjpeg_info ("without time forwarding");
      break;
    case 1:
      mjpeg_info ("with bottom field one frame forward");
      break;
    case -1:
      mjpeg_info ("with top field one frame forward");
      break;
    default:
      mjpeg_error_exit1 ("Unknown time reordering status: %d", field_move);
    }

  switch (luma)
    {
    case 0:
      mjpeg_info ("Without luminance correction");
      break;
    case 1:
      mjpeg_info ("With luminance correction");
      break;
    default:
      mjpeg_error_exit1 ("Unknown luminance correction status %u", luma);
    }

  switch (chroma)
    {
    case 0:
      mjpeg_info ("Without chrominance correction");
      break;
    case 1:
      mjpeg_info ("With chrominance correction");
      break;
    default:
      mjpeg_error_exit1 ("Unknown chrominance correction status %u", chroma);
    }


  if (black == 1)
    {
      mjpeg_info ("black lines: %u above and %u under",
		  output_black_line_above, output_black_line_under);
      mjpeg_info ("black columns: %u left and %u right",
		  output_black_col_left, output_black_col_right);
    }
  if (skip == 1)
    {
      mjpeg_info ("skipped lines: %u above and %u under",
		  output_skip_line_above, output_skip_line_under);
      mjpeg_info ("skipped columns: %u left and %u right",
		  output_skip_col_left, output_skip_col_right);
    }
  mjpeg_info ("frame rate: %.3f fps", Y4M_RATIO_DBL (frame_rate));

}

// *************************************************************************************


// *************************************************************************************
uint8_t
yuvscaler_nearest_integer_division (unsigned long int p, unsigned long int q)
{
  // This function returns the nearest integer of the ratio p/q. 
  // As this ratio in yuvscaler corresponds to a pixel value, it should be between 0 and 255
  unsigned long int ratio = p / q;
  unsigned long int reste = p % q;
  unsigned long int frontiere = q - q / 2;	// Do **not** change this into q/2 => it is not the same for odd q numbers

  if (reste >= frontiere)
    ratio++;

  if ((ratio < 0) || (ratio > 255))
    mjpeg_error_exit1 ("Division error: %lu/%lu not in [0;255] range !!\n", p,
		       q);
  return ((uint8_t) ratio);
}

// *************************************************************************************



// *************************************************************************************
static y4m_ratio_t
yuvscaler_calculate_output_sar (int out_w, int out_h,
				int in_w, int in_h, y4m_ratio_t in_sar)
{
// This function calculates the sample aspect ratio (SAR) for the output stream,
//    given the input->output scale factors, and the input SAR.
  if (Y4M_RATIO_EQL (in_sar, y4m_sar_UNKNOWN))
    {
      return y4m_sar_UNKNOWN;
    }
  else
    {
      y4m_ratio_t out_sar;
      /*
         out_SAR_w     in_SAR_w    input_W   output_H
         ---------  =  -------- *  ------- * --------
         out_SAR_h     in_SAR_h    input_H   output_W
       */
      out_sar.n = in_sar.n * in_w * out_h;
      out_sar.d = in_sar.d * in_h * out_w;
      y4m_ratio_reduce (&out_sar);
      return out_sar;
    }
}

// *************************************************************************************


// *************************************************************************************
int
yuvscaler_y4m_read_frame (int fd, y4m_frame_info_t * frameinfo,
			  unsigned long int buflen, uint8_t * buf,
			  int line_switching)
{
  // This function reads a frame from input stream. 
  // It is the same as the y4m_read_frame function (from y4mpeg.c) except that line switching is implemented
  static int err = Y4M_OK;
  unsigned int line;
  if ((err = y4m_read_frame_header (fd, frameinfo)) == Y4M_OK)
    {
      if (!line_switching)
	{
	  if ((err = y4m_read (fd, buf, buflen)) != Y4M_OK)
	    {
	      mjpeg_info ("Couldn't read FRAME content: %s!",
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
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
		     line, y4m_strerr (err));
		  return (err);
		}
	      buf -= input_width;	// buf points to former line on output, store input line there
	      if ((err = y4m_read (fd, buf, input_width)) != Y4M_OK)
		{
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
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
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
		     line, y4m_strerr (err));
		  return (err);
		}
	      buf -= input_width / 2;	// buf points to former line on output, store input line there
	      if ((err = y4m_read (fd, buf, input_width / 2)) != Y4M_OK)
		{
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
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
	mjpeg_info ("Couldn't read FRAME header: %s!", y4m_strerr (err));
      else
	mjpeg_info ("End of stream!");
      return (err);
    }
  return Y4M_OK;
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
yuvscaler_luminance_init (uint8_t * luminance,
			  uint8_t input_y_min, uint8_t input_y_max,
			  uint8_t output_y_min, uint8_t output_y_max)
{
  // This function initialises the luminance vector
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  uint16_t i;


  // Filling in the luminance vector
  u_c_p = luminance;
  for (i = 0; i < 256; i++)
    {
      if (i <= input_y_min)
	*(u_c_p++) = output_y_min;
      else
	{
	  if (i >= input_y_max)
	    *(u_c_p++) = output_y_max;
	  else
	    *(u_c_p++) = output_y_min +
	      yuvscaler_nearest_integer_division ((unsigned long
						   int) (output_y_max -
							 output_y_min) *
						  (unsigned long int) (i -
								       input_y_min),
						  (input_y_max -
						   input_y_min));
	}
      mjpeg_debug ("Luminance[%u]=%u", i, luminance[i]);
    }

  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
yuvscaler_chrominance_init (uint8_t * Uchroma, uint8_t * Vchroma,
			    float Ufactor, float Vfactor,
			    uint8_t UVmin, uint8_t UVmax)
{
  // This function initialises the Uchroma and Vchroma vectors
  uint8_t *Uu_c_p, *Vu_c_p;	//u_c_p = uint8_t pointer
  uint16_t i;
  float newU, newV;

  // Filling in the 2 chrominance vectors
  Uu_c_p = Uchroma;
  Vu_c_p = Vchroma;
  for (i = 0; i < 256; i++)
    {
      newU = (float) (i - Ucenter) * Ufactor + 128.0;
      newU = (float) floor (0.5 + newU);	// nearest integer in double format
      newV = (float) (i - Vcenter) * Vfactor + 128.0;
      newV = (float) floor (0.5 + newV);	// nearest integer in double format
      if (newU < UVmin)
	*(Uu_c_p++) = UVmin;
      else
	{
	  if (newU > UVmax)
	    *(Uu_c_p++) = UVmax;
	  else
	    *(Uu_c_p++) = (uint8_t) newU;
	}
      mjpeg_debug ("Uchroma[%u]=%u", i, Uchroma[i]);
      if (newV < UVmin)
	*(Vu_c_p++) = UVmin;
      else
	{
	  if (newV > UVmax)
	    *(Vu_c_p++) = UVmax;
	  else
	    *(Vu_c_p++) = (uint8_t) newV;
	}
      mjpeg_debug ("Vchroma[%u]=%u", i, Vchroma[i]);
    }
  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
yuvscaler_luminance_treatment (uint8_t * input, unsigned long size,
			       uint8_t * luminance)
{
  // This function corrects the luminance of input
  uint8_t *u_c_p;
  uint32_t i;

  u_c_p = input;
  // Luminance (Y component)
  for (i = 0; i < size; i++)
    *(u_c_p++) = luminance[*(u_c_p)];


  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
yuvscaler_chrominance_treatment (uint8_t * input, unsigned long size,
				 uint8_t * Uchroma, uint8_t * Vchroma)
{
  // This function corrects the chrominance of input
  uint8_t *u_c_p;
  uint32_t i;

//   mjpeg_debug("Start of yuvscaler_chrominance_treatment(%p, %lu, %p, %p)",input,size,Uchroma,Vchroma); 
  // Coherence check
  if (size % 2 != 0)
    mjpeg_error_exit1
      ("yuvscaler_chrominance_treatment: size=%lu, not even!!", size);

  u_c_p = input;
  // Chroma
  // U component
  for (i = 0; i < size / 2; i++)
    *(u_c_p++) = Uchroma[*(u_c_p)];
  // V component
  for (i = 0; i < size / 2; i++)
    *(u_c_p++) = Vchroma[*(u_c_p)];
//   mjpeg_debug("End of yuvscaler_chrominance_treatment");
  return (0);
}

// *************************************************************************************



// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
bottom_field_storage (uint8_t * input, int numframe, uint8_t * field1,
		      uint8_t * field2)
{
  int ligne;
  // This function stores the current bottom field into tabular field[1 or 2] 
  input += input_width;
  if (numframe % 2 == 0)
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field1, input, input_width);
	  input += (input_width << 1);
	  field1 += input_width;
	}
      input -= (input_width >> 1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field1, input, (input_width >> 1));
	  input += input_width;
	  field1 += (input_width >> 1);
	}
    }
  else
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field2, input, input_width);
	  input += (input_width << 1);
	  field2 += input_width;
	}
      input -= (input_width >> 1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field2, input, (input_width >> 1));
	  input += input_width;
	  field2 += (input_width >> 1);
	}
    }
  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
top_field_storage (uint8_t * input, int numframe, uint8_t * field1,
		   uint8_t * field2)
{
  int ligne;
  // This function stores the current top field into tabular field[1 or 2] 
//   input+=input_width;
  if (numframe % 2 == 0)
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field1, input, input_width);
	  input += (input_width << 1);
	  field1 += input_width;
	}
//      input-=(input_width>>1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field1, input, (input_width >> 1));
	  input += input_width;
	  field1 += (input_width >> 1);
	}
    }
  else
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field2, input, input_width);
	  input += (input_width << 1);
	  field2 += input_width;
	}
//      input-=(input_width>>1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (field2, input, (input_width >> 1));
	  input += input_width;
	  field2 += (input_width >> 1);
	}
    }
  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
bottom_field_replace (uint8_t * input, int numframe, uint8_t * field1,
		      uint8_t * field2)
{
  int ligne;
  // This function stores the current bottom field into tabular field[1 or 2] 
  input += input_width;
  // This function replaces the current bottom field with tabular field[1 or 2] 
  if (numframe % 2 == 0)
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field2, input_width);
	  input += (input_width << 1);
	  field2 += input_width;
	}
      input -= (input_width >> 1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field2, (input_width >> 1));
	  input += input_width;
	  field2 += (input_width >> 1);
	}
    }
  else
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field1, input_width);
	  input += (input_width << 1);
	  field1 += input_width;
	}
      input -= (input_width >> 1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field1, (input_width >> 1));
	  input += input_width;
	  field1 += (input_width >> 1);
	}
    }
  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
top_field_replace (uint8_t * input, int numframe, uint8_t * field1,
		   uint8_t * field2)
{
  int ligne;
  // This function stores the current top field into tabular field[1 or 2] 
  if (numframe % 2 == 0)
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field2, input_width);
	  input += (input_width << 1);
	  field2 += input_width;
	}
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field2, (input_width >> 1));
	  input += input_width;
	  field2 += (input_width >> 1);
	}
    }
  else
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field1, input_width);
	  input += (input_width << 1);
	  field1 += input_width;
	}
      // U and V COMPONENTS
      for (ligne = 0; ligne < input_height; ligne += 2)
	{
	  memcpy (input, field1, (input_width >> 1));
	  input += input_width;
	  field1 += (input_width >> 1);
	}
    }
  return (0);
}

// *************************************************************************************


// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
blackout (uint8_t * input_y, uint8_t * input_u, uint8_t * input_v)
{
  // The blackout function makes input borders pixels become black
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


// *************************************************************************************
void
handle_args_global (int argc, char *argv[])
{
  // This function takes care of the global variables 
  // initialisation that are independent of the input stream
  // The main goal is to know whether input frames originate from file or stdin
  int c;

  while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{
	case 'v':
	  verbose = atoi (optarg);
	  if (verbose < 0 || verbose > 2)
	    {
	      mjpeg_error_exit1 ("Verbose level must be [0..2]");
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
	      mjpeg_error_exit1 ("Illegal norm letter specified: %c",
				 *optarg);
	    }
	  break;


	case 'h':
//      case '?':
	  yuvscaler_print_usage (argv);
	  break;

	default:
	  break;
	}

    }
  if (optind != argc)
    yuvscaler_print_usage (argv);

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
  float f1, f2;

  // By default, display sizes is the same as input size
  display_width = input_width;
  display_height = input_height;

  optind = 1;
  while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{


	  // **************               
	  // OUTPUT KEYWORD
	  // **************               
	case 'O':
	  output = 0;
	  if (strcmp (optarg, VCD_KEYWORD) == 0)
	    {
	      output = 1;
	      vcd = 1;
	      svcd = 0;		// if user gives VCD, SVCD and DVD keywords, take last one only into account
	      dvd = 0;
	      display_width = 352;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("VCD output format requested in PAL/SECAM norm");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!");
	    }
	  if (strcmp (optarg, SVCD_KEYWORD) == 0)
	    {
	      output = 1;
	      svcd = 1;
	      vcd = 0;		// if user gives VCD, SVCD and DVD keywords, take last one only into account
	      dvd = 0;
	      display_width = 480;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("SVCD output format requested in PAL/SECAM norm");
		  display_height = 576;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("SVCD output format requested in NTSC norm");
		  display_height = 480;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine SVCD output size. Please use the -n option!");
	    }
	  if (strcmp (optarg, DVD_KEYWORD) == 0)
	    {
	      output = 1;
	      vcd = 0;
	      svcd = 0;		// if user gives VCD, SVCD and DVD keywords, take last one only into account
	      dvd = 1;
	      display_width = 720;
	      if (norm == 0)
		{
		  mjpeg_info
		    ("DVD output format requested in PAL/SECAM norm");
		  display_height = 576;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("DVD output format requested in NTSC norm");
		  display_height = 480;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine DVD output size. Please use the -n option!");
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
		      size_keyword = 1;
		    }
		  else
		    mjpeg_error_exit1
		      ("Unconsistent SIZE keyword, not multiple of 2: %s",
		       optarg);
		  // A second check will eventually be done when output interlacing is finally known
		}
	      else
		mjpeg_error_exit1 ("Uncorrect SIZE keyword: %s", optarg);
	    }
	  if (strcmp (optarg, TOP_FIRST) == 0)
	    {
	      output = 1;
	      output_interlaced = Y4M_ILACE_TOP_FIRST;
	    }

	  if (strcmp (optarg, BOT_FIRST) == 0)
	    {
	      output = 1;
	      output_interlaced = Y4M_ILACE_BOTTOM_FIRST;
	    }
	  if ((strcmp (optarg, NOT_INTER) == 0)
	      || (strcmp (optarg, PROGRESSIVE) == 0))
	    {
	      output = 1;
	      output_interlaced = Y4M_ILACE_NONE;
	    }

	  // Theoritically, this should go into handle_args_global
	  if (strcmp (optarg, MONO_KEYWORD) == 0)
	    {
	      output = 1;
	      mono = 1;
	    }

	  if (output == 0)
	    mjpeg_error_exit1 ("Uncorrect output keyword: %s", optarg);
	  break;



	  // **************            
	  // MODE KEYOWRD
	  // *************
	case 'M':
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
#ifdef HAVE_ASM_MMX
	  if (strcmp (optarg, MMX) == 0)
	    {
	      mode = 1;
	      mmx = 1;
	    }
#endif
	  if (strcmp (optarg, BICUBIC) == 0)
	    {
	      mode = 1;
	      algorithm = 1;
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
		mjpeg_error_exit1 ("Unconsistent RATIO keyword: %s", optarg);
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
		    ("VCD output format requested in PAL/SECAM norm");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!");
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
		    ("VCD output format requested in PAL/SECAM norm");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!");
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
		    ("VCD output format requested in PAL/SECAM norm");
		  display_height = 288;
		}
	      else if (norm == 1)
		{
		  mjpeg_info ("VCD output format requested in NTSC norm");
		  display_height = 240;
		}
	      else
		mjpeg_error_exit1
		  ("No norm specified, cannot determine VCD output size. Please use the -n option!");
	    }

	  if (mode == 0)
	    mjpeg_error_exit1 ("Uncorrect mode keyword: %s", optarg);
	  break;



	  // **************            
	  // INPUT KEYOWRD
	  // *************
	case 'I':
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
		      ("Unconsistent USE keyword: %s, offsets/sizes not multiple of 2 or offset+size>input size",
		       optarg);
		  if (input_interlaced != Y4M_ILACE_NONE)
		    {
		      if ((input_useful_height % 4 != 0)
			  || (input_discard_line_above % 4 != 0))
			mjpeg_error_exit1
			  ("Unconsistent USE keyword: %s, height offset or size not multiple of 4 but input is interlaced!!",
			   optarg);
		    }

		}
	    }
	  if (strncmp (optarg, LUMINANCE, 10) == 0)
	    {
	      input = 1;
	      luma = 1;
	      if (sscanf
		  (optarg, "LUMINANCE_%u_%u_%u_%u", &ui1, &ui2, &ui3,
		   &ui4) == 4)
		{
		  // Coherence check:
		  if ((ui1 < 0) || (ui1 > 255) ||
		      (ui2 < 0) || (ui2 > 255) ||
		      (ui3 < 0) || (ui3 > 255) ||
		      (ui4 < 0) || (ui4 > 255) || (ui1 > ui2) || (ui3 > ui4))
		    mjpeg_error_exit1
		      ("Uncoherent luminance correction (0<>255, small, large): InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u\n",
		       ui1, ui2, ui3, ui4);
		  input_y_min = (uint8_t) ui1;
		  input_y_max = (uint8_t) ui2;
		  output_y_min = (uint8_t) ui3;
		  output_y_max = (uint8_t) ui4;
		}
	      else
		mjpeg_error_exit1
		  ("Unconsistent LUMINANCE keyword: %s\n", optarg);
	    }
	  if (strncmp (optarg, CHROMINANCE, 12) == 0)
	    {
	      input = 1;
	      chroma = 1;
	      if (sscanf
		  (optarg, "CHROMINANCE_%f_%u_%f_%u_%u_%u", &f1, &ui1, &f2,
		   &ui2, &ui3, &ui4) == 6)
		{
		  // Coherence check:
		  if ((ui1 < 0) || (ui1 > 255) ||
		      (ui2 < 0) || (ui2 > 255) ||
		      (ui3 < 0) || (ui3 > 255) ||
		      (ui4 < 0) || (ui4 > 255) ||
		      (ui3 > ui4) || (ui1 > ui4) || (ui2 > ui4))
		    mjpeg_error_exit1
		      ("Uncoherent chrominance correction (0<>255, small, large): Ufactor=%f, Ucenter=%u, Vfactor=%f, Vcenter=%u, UVmin=%u, UVmax=%u, \n",
		       f1, ui1, f2, ui2, ui3, ui4);
		  Ucenter = (uint8_t) ui1;
		  Vcenter = (uint8_t) ui2;
		  Ufactor = f1;
		  Vfactor = f2;
		  UVmin = (uint8_t) ui3;
		  UVmax = (uint8_t) ui4;
		}
	      else
		mjpeg_error_exit1
		  ("Unconsistent CHROMINANCE keyword: %s\n", optarg);
	    }
	  if (strcmp (optarg, TOP_FIRST) == 0)
	    {
	      input = 1;
	      input_interlaced = Y4M_ILACE_TOP_FIRST;
	    }

	  if (strcmp (optarg, BOT_FIRST) == 0)
	    {
	      input = 1;
	      input_interlaced = Y4M_ILACE_BOTTOM_FIRST;
	    }
	  if ((strcmp (optarg, NOT_INTER) == 0)
	      || (strcmp (optarg, PROGRESSIVE) == 0))
	    {
	      input = 1;
	      input_interlaced = Y4M_ILACE_NONE;
	    }
	  if (strcmp (optarg, BOTT_FORWARD) == 0)
	    {
	      field_move += 1;
	      input = 1;
	    }
	  if (strcmp (optarg, TOP_FORWARD) == 0)
	    {
	      field_move -= 1;
	      input = 1;
	    }
	  if (strcmp (optarg, LINESWITCH) == 0)
	    {
	      input = 1;
	      line_switching = 1;
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
		      ("Unconsistent ACTIVE keyword: %s, offsets/sizes not multiple of 4 or offset+size>input size",
		       optarg);
		  if (input_interlaced != Y4M_ILACE_NONE)
		    {
		      if ((input_active_height % 4 != 0)
			  || (input_black_line_above % 4 != 0))
			mjpeg_error_exit1
			  ("Unconsistent ACTIVE keyword: %s, height offset or size not multiple of 4 but input is interlaced!!",
			   optarg);
		    }

		}
	      else
		mjpeg_error_exit1
		  ("Uncorrect input flag argument: %s", optarg);
	    }
	  if (input == 0)
	    mjpeg_error_exit1 ("Uncorrect input keyword: %s", optarg);
	  break;

	default:
	  break;
	}
    }

// Interlacing, line-switching and field move
// 
// Line_switching is not equivalent to a change in interlacing type from TOP_FIRST to BOTTOM_FIRST or vice_versa since
// such conversion needs to interleave field 1 from frame 0 with field 0 from frame 1. Whereas line switching only works 
// with frame 0.
// 
// In fact, conversion from TOP_FIRST to BOTTOM_FIRST only needs to move bottom field one frame forward : keyword BOTT_FORWARD
// and conversion from BOTTOM_FIRST to TOP_FIRST needs to move top field one frame forward : keyword TOP_FORWARD.
// 
// By default, there is no line switching. Line switching will occur only if specified
// 
  if (vcd == 1)
    {
      if ((output_interlaced == Y4M_ILACE_TOP_FIRST)
	  || (output_interlaced == Y4M_ILACE_BOTTOM_FIRST))
	mjpeg_warn
	  ("VCD requires non-interlaced output frames. Ignoring interlaced specification");
      output_interlaced = Y4M_ILACE_NONE;
      if ((input_interlaced == Y4M_ILACE_TOP_FIRST)
	  || (input_interlaced == Y4M_ILACE_BOTTOM_FIRST))
	mjpeg_warn
	  ("Interlaced input frames will be downscaled to non-interlaced VCD frames\nIf quality is an issue, please consider deinterlacing input frames with yuvdenoise -I");
    }
  else
    {
      // Output may be of any kind of interlacing : svcd, dvd, size or simply same size as input
      // First case: output interlacing type has not been specified. By default, same as input
      if (output_interlaced == -1)
	output_interlaced = input_interlaced;
      else
	{
	  // Second case: output interlacing has been specified and it is different from input 
	  //  => do necessary preprocessing
	  if (output_interlaced != input_interlaced)
	    {
	      switch (input_interlaced)
		{
		case Y4M_ILACE_NONE:
		  mjpeg_warn
		    ("input not_interlaced/progressive but interlaced output required: hope you know what you're doing!");
		  break;
		case Y4M_ILACE_TOP_FIRST:
		  if (output_interlaced == Y4M_ILACE_BOTTOM_FIRST)
		    field_move += 1;
		  else
		    mjpeg_warn
		      ("not_interlaced/progressive output required but top-interlaced input: hope you know what you're doing!");
		  break;
		case Y4M_ILACE_BOTTOM_FIRST:
		  if (output_interlaced == Y4M_ILACE_TOP_FIRST)
		    field_move -= 1;
		  else
		    mjpeg_warn
		      ("not_interlaced/progressive output required but bottom-interlaced input: hope you know what you're doing!");
		  break;

		}
	    }
	}
    }
  if (field_move > 0)
    field_move = 1;
  if (field_move < 0)
    field_move = -1;

  // END OF Interlacing and line-switching


  // Size Keyword final coherence check
  if ((output_interlaced != Y4M_ILACE_NONE) && (size_keyword == 1))
    {
      if (display_height % 4 != 0)
	mjpeg_error_exit1
	  ("Unconsistent SIZE keyword, Height is not multiple of 4 but output interlaced!!");
    }

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
	  ("Specified input ratios (%u and %u) does not divide input useful size (%u and %u)!",
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
	input_black = 0;	// black zone is not enterely inside useful zone
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
      ("Output sizes are not multiple of 4! %ux%u, %ux%u being displayed",
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
// MAIN
// *************************************************************************************
int
main (int argc, char *argv[])
{
  int n, err = Y4M_OK, nb;
  unsigned long int i, j;
//  char sar[]="nnn:ddd";

  long int frame_num = 0;
  unsigned int *height_coeff = NULL, *width_coeff = NULL;
  uint8_t *input = NULL, *output = NULL, *line = NULL, *field1 =
    NULL, *field2 = NULL, *padded_input = NULL, *padded_bottom =
    NULL, *padded_top = NULL;
  uint8_t *input_y, *input_u, *input_v;
  uint8_t *output_y, *output_u, *output_v;
  uint8_t *frame_y, *frame_u, *frame_v;
  uint8_t **frame_y_p = NULL, **frame_u_p = NULL, **frame_v_p = NULL;	// size is not yet known => pointer of pointer
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  unsigned int divider;

  // SPECIFIC TO BICUBIC
  unsigned int *in_line = NULL, *in_col = NULL, out_line, out_col;
  unsigned long int somme;
  int m;
  float *a = NULL, *b = NULL;
  int16_t *cubic_spline_m = NULL, *cubic_spline_n = NULL;
  long int value, lint;

  // SPECIFIC TO YUV4MPEG 
  unsigned long int nb_pixels;
  y4m_frame_info_t frameinfo;
  y4m_stream_info_t in_streaminfo;
  y4m_stream_info_t out_streaminfo;
  y4m_ratio_t frame_rate = y4m_fps_UNKNOWN;

  // SPECIFIC TO LUMINANCE AND CHROMINANCE CORRECTION
  uint8_t *luminance = NULL;
  uint8_t *Uchroma = NULL, *Vchroma = NULL, *input_uv = NULL;
  unsigned long int nb_y = 0, nb_uv = 0;

  // Information output
  mjpeg_info ("yuvscaler (version " YUVSCALER_VERSION
	      ") is a general scaling utility for yuv frames");
  mjpeg_info ("(C) 2001-2002 Xavier Biquard <xbiquard@free.fr>");
  mjpeg_info ("yuvscaler -h for help, or man yuvscaler");

  // Initialisation of global variables that are independent of the input stream, input_file in particular
  handle_args_global (argc, argv);

  // mjpeg tools global initialisations
  mjpeg_default_handler_verbosity (verbose);
  y4m_init_stream_info (&in_streaminfo);
  y4m_init_stream_info (&out_streaminfo);
  y4m_init_frame_info (&frameinfo);


  // ***************************************************************
  // Get video stream informations (size, framerate, interlacing, sample aspect ratio).
  // The in_streaminfo structure is filled in accordingly 
  // ***************************************************************
  if (y4m_read_stream_header (0, &in_streaminfo) != Y4M_OK)
    mjpeg_error_exit1 ("Could'nt read YUV4MPEG header!");
  input_width = y4m_si_get_width (&in_streaminfo);
  input_height = y4m_si_get_height (&in_streaminfo);
  frame_rate = y4m_si_get_framerate (&in_streaminfo);
  input_interlaced = y4m_si_get_interlace (&in_streaminfo);
  // ***************************************************************


  // INITIALISATIONS
  // Norm determination from header (this has precedence over user's specification through the -n flag)
  if (Y4M_RATIO_EQL (frame_rate, y4m_fps_PAL))
    norm = 0;
  if (Y4M_RATIO_EQL (frame_rate, y4m_fps_NTSC))
    norm = 1;
  if (norm < 0)
    {
      mjpeg_warn
	("Could not infer norm (PAL/SECAM or NTSC) from input data (frame size=%dx%d, frame rate=%d:%d fps)!!",
	 input_width, input_height, frame_rate.n, frame_rate.d);
    }



  // Deal with args that depend on input stream
  handle_args_dependent (argc, argv);

  // Scaling algorithm determination
  if ((algorithm == 0) || (algorithm == -1))
    {
      // Coherences check: resample can only downscale not upscale
      if ((input_useful_width < output_active_width)
	  || (input_useful_height < output_active_height))
	{
	  if (algorithm == 0)
	    mjpeg_info
	      ("Resampling algorithm can only downscale, not upscale => switching to bicubic algorithm");
	  algorithm = 1;
	}
      else
	algorithm = 0;
    }

  // USER'S INFORMATION OUTPUT
  yuvscaler_print_information (in_streaminfo, frame_rate);

  divider = pgcd (input_useful_width, output_active_width);
  input_width_slice = input_useful_width / divider;
  output_width_slice = output_active_width / divider;
  mjpeg_debug ("divider,i_w_s,o_w_s = %d,%d,%d",
	       divider, input_width_slice, output_width_slice);

  divider = pgcd (input_useful_height, output_active_height);
  input_height_slice = input_useful_height / divider;
  output_height_slice = output_active_height / divider;
  mjpeg_debug ("divider,i_w_s,o_w_s = %d,%d,%d",
	       divider, input_height_slice, output_height_slice);

  diviseur = input_height_slice * input_width_slice;
  mjpeg_debug ("Diviseur=%ld", diviseur);

  mjpeg_info ("Scaling ratio for width is %u to %u",
	      input_width_slice, output_width_slice);
  mjpeg_info ("and is %u to %u for height", input_height_slice,
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
      mjpeg_info (" --- Newly speed optimized parameters ---");
      yuvscaler_print_information (in_streaminfo, frame_rate);
    }


  // RESAMPLE RESAMPLE RESAMPLE   
  if (algorithm == 0)
    {
      // SPECIFIC
      // Is a specific downscaling speed enhanced treatment available?
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
	mjpeg_info ("Specific downscaling routing number %u", specific);

      // To determine scaled value of pixels in the case of the resample algorithm, we have to divide a long int by 
      // the long int "diviseur". So, to speed up downscaling, we tabulate all possible results of this division 
      // using the divide vector and the function yuvscaler_nearest_integer_division. 
      if (!
	  (divide =
	   (uint8_t *) malloc ((1 + 255 * diviseur) * sizeof (uint8_t) +
			       ALIGNEMENT)))
	mjpeg_error_exit1
	  ("Could not allocate memory for divide table. STOP!");
      mjpeg_debug ("before alignement: divide=%p", divide);
      // alignement instructions
      if (((unsigned long) divide % ALIGNEMENT) != 0)
	divide =
	  (uint8_t *) ((((unsigned long) divide / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
      mjpeg_debug ("after alignement: divide=%p", divide);

      u_c_p = divide;
      for (i = 0; i <= 255 * diviseur; i++)
	*(u_c_p++) = yuvscaler_nearest_integer_division (i, diviseur);

      // Calculate averaging coefficient
      // For the height
      height_coeff =
	malloc ((input_height_slice + 1) * output_height_slice *
		sizeof (unsigned int));
      average_coeff (input_height_slice, output_height_slice, height_coeff);

      // For the width
      width_coeff =
	malloc ((input_width_slice + 1) * output_width_slice *
		sizeof (unsigned int));
      average_coeff (input_width_slice, output_width_slice, width_coeff);

    }
  // END OF RESAMPLE RESAMPLE RESAMPLE      


  // BICUBIC BICUBIC BICUBIC  
  if (algorithm == 1)
    {

      // SPECIFIC
      // Is a specific speed enhanced treatment available?
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
	  // en largeur, les coefficients bicubic cubic_spline_n valent pour les numéros de colonnes impaires (b=0.5) :
	  // -5/144, 77/144, 77/144 et -5/144. Même chose qu'en hauteur pour les numéros de colonnes paires 
	}
      if (specific)
	{
	  mjpeg_info ("Specific downscaling routing number %u", specific);
	  if (!
	      (divide =
	       (uint8_t *) malloc ((bicubic_offset + bicubic_max) *
				   sizeof (uint8_t) + ALIGNEMENT)))
	    mjpeg_error_exit1
	      ("Could not allocate enough memory for divide table. STOP!");
	  mjpeg_debug ("before alignement divide=%p\n", divide);
	  // alignement instructions
	  if (((unsigned long) divide % ALIGNEMENT) != 0)
	    divide =
	      (uint8_t *) ((((unsigned long) divide / ALIGNEMENT) + 1) *
			   ALIGNEMENT);
	  mjpeg_debug ("after alignement divide=%p\n", divide);
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
      // END OF SPECIFIC

#ifdef HAVE_ASM_MMX

      if (!
	  (mmx_padded =
	   (int16_t *) malloc (4 * sizeof (int16_t) + ALIGNEMENT))
	  || !(mmx_cubic =
	       (int16_t *) malloc (4 * sizeof (int16_t) + ALIGNEMENT))
	  || !(mmx_res =
	       (int32_t *) malloc (2 * sizeof (int32_t) + ALIGNEMENT)))
	mjpeg_error_exit1
	  ("Could not allocate memory for mmx registers. STOP!");

      mjpeg_debug ("Before alignement");
      mjpeg_debug ("%p %p %p", mmx_padded, mmx_cubic, mmx_res);
      mjpeg_debug ("%u %u %u", (unsigned int) mmx_padded,
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

      mjpeg_debug ("After Alignement");
      mjpeg_debug ("%p %p %p", mmx_padded, mmx_cubic, mmx_res);
      mjpeg_debug ("%u %u %u", (unsigned int) mmx_padded,
		   (unsigned int) mmx_cubic, (unsigned int) mmx_res);

#endif

      // Let us tabulate several values
      // To the output pixel of coordinates (out_col,out_line) corresponds the input pixel (in_col,in_line), 
      // in_col and in_line being the nearest **smaller** values.
      // Tabulation of the width: in_col and b
      if (!(in_col =
	    (unsigned int *) malloc (output_active_width *
				     sizeof (unsigned int)))
	  || !(b = (float *) malloc (output_active_width * sizeof (float))))
	mjpeg_error_exit1
	  ("Could not allocate memory for in_col or b table. STOP!");
      for (out_col = 0; out_col < output_active_width; out_col++)
	{
	  in_col[out_col] =
	    (out_col * input_width_slice) / output_width_slice;
	  b[out_col] =
	    (float) ((out_col * input_width_slice) % output_width_slice) /
	    (float) output_width_slice;
	  mjpeg_debug ("out_col=%u,in_col=%u,b=%f", out_col, in_col[out_col],
		       b[out_col]);
	}
      // Tabulation of the height: in_line and a
      if (!(in_line =
	    (unsigned int *) malloc (output_active_height *
				     sizeof (unsigned int))) ||
	  !(a = (float *) malloc (output_active_height * sizeof (float))))
	mjpeg_error_exit1
	  ("Could not allocate memory for in_line or a table. STOP!");
      for (out_line = 0; out_line < output_active_height; out_line++)
	{
	  in_line[out_line] =
	    (out_line * input_height_slice) / output_height_slice;
	  a[out_line] =
	    (float) ((out_line * input_height_slice) % output_height_slice) /
	    (float) output_height_slice;
	  mjpeg_debug ("out_line=%u,in_line=%u,a=%f", out_line,
		       in_line[out_line], a[out_line]);
	}
      // Tabulation of the cubic values 
      if (!(cubic_spline_n =
	    (int16_t *) malloc (4 * output_active_width * sizeof (int16_t)))
	  || !(cubic_spline_m =
	       (int16_t *) malloc (4 * output_active_height *
				   sizeof (int16_t))))
	mjpeg_error_exit1
	  ("Could not allocate memory for cubic_spline_n or cubic_spline_mtable. STOP!");

      for (n = -1; n <= 2; n++)
	{

	  for (out_col = 0; out_col < output_active_width; out_col++)
	    {
	      cubic_spline_n[out_col + (n + 1) * output_active_width] =
		cubic_spline (b[out_col] - (float) n, bicubic_div_width);
	      mjpeg_debug ("n=%d,out_col=%u,cubic=%d", n, out_col,
			   cubic_spline (b[out_col] - (float) n,
					 bicubic_div_width));
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
	      mjpeg_debug ("out_col somme=%ld, to be normalized", somme);
	      cubic_spline_n[out_col + 3 * output_active_width] -=
		somme - bicubic_div_width;
	    }
	}


      for (m = -1; m <= 2; m++)
	for (out_line = 0; out_line < output_active_height; out_line++)
	  {
	    cubic_spline_m[out_line + (m + 1) * output_active_height] =
	      cubic_spline ((float) m - a[out_line], bicubic_div_height);
	    mjpeg_debug ("m=%d,out_line=%u,cubic=%d", m, out_line,
			 cubic_spline ((float) m - a[out_line],
				       bicubic_div_height));
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
	      mjpeg_debug ("out_line somme=%ld, to be normalized", somme);
	      cubic_spline_m[out_line + 3 * output_active_height] -=
		somme - bicubic_div_height;
	    }
	}

      if ((output_interlaced == Y4M_ILACE_NONE)
	  || (input_interlaced == Y4M_ILACE_NONE))
	{
	  if (!(padded_input =
		(uint8_t *) malloc ((input_useful_width + 3) *
				    (input_useful_height + 3))))
	    mjpeg_error_exit1
	      ("Could not allocate memory for padded_input table. STOP!");
	}
      else
	{
	  if (!(padded_top =
		(uint8_t *) malloc ((input_useful_width + 3) *
				    (input_useful_height / 2 + 3))) ||
	      !(padded_bottom =
		(uint8_t *) malloc ((input_useful_width + 3) *
				    (input_useful_height / 2 + 3))))
	    mjpeg_error_exit1
	      ("Could not allocate memory for padded_top|bottom tables. STOP!");
	}
    }
  // END OF BICUBIC BICUBIC BICUBIC     


  // Pointers allocations
  if (!(line = malloc (input_width)) ||
      !(field1 = malloc (3 * (input_width / 2) * (input_height / 2))) ||
      !(field2 = malloc (3 * (input_width / 2) * (input_height / 2))) ||
      !(input = malloc (((input_width * input_height * 3) / 2) + ALIGNEMENT))
      || !(output =
	   malloc (((output_width * output_height * 3) / 2) + ALIGNEMENT)))
    mjpeg_error_exit1
      ("Could not allocate memory for line, field1, field2, input or output tables. STOP!");

  // input and output pointers alignement
  mjpeg_debug ("before alignement: input=%p output=%p", input, output);
  if (((unsigned long) input % ALIGNEMENT) != 0)
    input =
      (uint8_t *) ((((unsigned long) input / ALIGNEMENT) + 1) * ALIGNEMENT);
  if (((unsigned long) output % ALIGNEMENT) != 0)
    output =
      (uint8_t *) ((((unsigned long) output / ALIGNEMENT) + 1) * ALIGNEMENT);
  mjpeg_debug ("after alignement: input=%p output=%p", input, output);


  // if skip_col==1
  if (!(frame_y_p = (uint8_t **) malloc (display_height * sizeof (uint8_t *)))
      || !(frame_u_p =
	   (uint8_t **) malloc (display_height / 2 * sizeof (uint8_t *)))
      || !(frame_v_p =
	   (uint8_t **) malloc (display_height / 2 * sizeof (uint8_t *))))
    mjpeg_error_exit1
      ("Could not allocate memory for frame_y_p, frame_u_p or frame_v_p tables. STOP!");

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


  // Various initialisatiosn for variables concerning input and output   
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

  // LUMINANCE AND CHROMINANCE CORRECTION
  if (luma == 1)
    {
      // Memory allocation for the luminance vector
      if (!
	  (luminance =
	   (uint8_t *) malloc (256 * sizeof (uint8_t) + ALIGNEMENT)))
	mjpeg_error_exit1
	  ("Could not allocate memory for luminance table. STOP!");
      // memory alignement of the luminance vector
      if (((unsigned long) luminance % ALIGNEMENT) != 0)
	luminance =
	  (uint8_t *) ((((unsigned long) luminance / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
      // Filling in the luminance vectors
      yuvscaler_luminance_init (luminance,
				input_y_min, input_y_max,
				output_y_min, output_y_max);
      nb_y = input_width * input_height;
    }
  if (chroma == 1)
    {
      // Memory allocation for the 2 chroma vectors
      if ((!(Uchroma =
	     (uint8_t *) malloc (256 * sizeof (uint8_t) + ALIGNEMENT))) ||
	  (!(Vchroma =
	     (uint8_t *) malloc (256 * sizeof (uint8_t) + ALIGNEMENT))))
	mjpeg_error_exit1
	  ("Could not allocate memory for Uchroma or Vchroma vectors. STOP!");
      // memory alignement of the 2 chroma vectors
      if (((unsigned long) Uchroma % ALIGNEMENT) != 0)
	Uchroma =
	  (uint8_t *) ((((unsigned long) Uchroma / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
      if (((unsigned long) Vchroma % ALIGNEMENT) != 0)
	Vchroma =
	  (uint8_t *) ((((unsigned long) Vchroma / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
      // Filling in the Uchroma and Vchroma vectors
      yuvscaler_chrominance_init (Uchroma, Vchroma,
				  Ufactor, Vfactor, UVmin, UVmax);
      nb_uv = input_width * input_height / 2;
      input_uv = input + input_width * input_height;
    }
  mjpeg_debug ("End of Initialisation");
  // END OF INITIALISATION
  // END OF INITIALISATION
  // END OF INITIALISATION


  // SCALE AND OUTPUT FRAMES 
  // Output file header
  y4m_copy_stream_info (&out_streaminfo, &in_streaminfo);
  y4m_si_set_width (&out_streaminfo, display_width);
  y4m_si_set_height (&out_streaminfo, display_height);
  y4m_si_set_interlace (&out_streaminfo, output_interlaced);
  y4m_si_set_sampleaspect (&out_streaminfo,
			   yuvscaler_calculate_output_sar (output_width_slice,
							   output_height_slice,
							   input_width_slice,
							   input_height_slice,
							   y4m_si_get_sampleaspect
							   (&in_streaminfo)));
  if (no_header == 0)
    y4m_write_stream_header (output_fd, &out_streaminfo);
  y4m_log_stream_info (LOG_INFO, "output: ", &out_streaminfo);

  // Master loop : continue until there is no next frame in stdin
  while ((err = yuvscaler_y4m_read_frame
	  (0, &frameinfo, nb_pixels, input, line_switching)) == Y4M_OK)
    {
      mjpeg_info ("Frame number %ld", frame_num);

      // PREPROCESSING
      // Time reordering
      if (field_move != 0)
	{
	  if (field_move == 1)
	    {
	      // Bottom field one frame forward
	      if (frame_num == 0)
		{
		  bottom_field_storage (input, frame_num, field1, field2);
		  if (yuvscaler_y4m_read_frame
		      (0, &frameinfo, nb_pixels, input,
		       line_switching) != Y4M_OK)
		     mjpeg_error_exit1("Can't read frame %ld",frame_num);
		  frame_num++;
		  mjpeg_info ("Frame number %ld", frame_num);
		}
	      bottom_field_storage (input, frame_num, field1, field2);
	      bottom_field_replace (input, frame_num, field1, field2);
	    }
	  else
	    {
	      // Top field one frame forward
	      if (frame_num == 0)
		{
		  top_field_storage (input, frame_num, field1, field2);
		  if (yuvscaler_y4m_read_frame
		      (0, &frameinfo, nb_pixels, input,
		       line_switching) != Y4M_OK)
		     mjpeg_error_exit1("Can't read frame %ld",frame_num);
		  frame_num++;
		  mjpeg_info ("Frame number %ld", frame_num);
		}
	      top_field_storage (input, frame_num, field1, field2);
	      top_field_replace (input, frame_num, field1, field2);
	    }
	}
      // luminance correction
      if (luma == 1)
	yuvscaler_luminance_treatment (input, nb_y, luminance);
      // chrominance correction
      if (chroma == 1)
	yuvscaler_chrominance_treatment (input_uv, nb_uv, Uchroma, Vchroma);
      // YUV statistics
//      if (verbose == 2)
//	yuvstat (input);
      // Blackout if necessary
      if (input_black == 1)
	blackout (input_y, input_u, input_v);

      frame_num++;
      // END OF PREPROCESSING

      // Output Frame Header
      if (y4m_write_frame_header (output_fd, &frameinfo) != Y4M_OK)
	goto out_error;


      // SCALE THE FRAME
      // ***************
      // RESAMPLE ALGORITHM       
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
		  average (input_y, output_y, height_coeff, width_coeff, 0);
		  average (input_u, output_u, height_coeff, width_coeff, 1);
		  average (input_v, output_v, height_coeff, width_coeff, 1);
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
		  padding_interlaced (padded_top, padded_bottom, input_y, 0);
		  cubic_scale_interlaced (padded_top, padded_bottom,
					  output_y, in_col, b, in_line, a,
					  cubic_spline_n, cubic_spline_m, 0);
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
		  padding_interlaced (padded_top, padded_bottom, input_y, 0);
		  cubic_scale_interlaced (padded_top, padded_bottom,
					  output_y, in_col, b, in_line, a,
					  cubic_spline_n, cubic_spline_m, 0);
		  padding_interlaced (padded_top, padded_bottom, input_u, 1);
		  cubic_scale_interlaced (padded_top, padded_bottom,
					  output_u, in_col, b, in_line, a,
					  cubic_spline_n, cubic_spline_m, 1);
		  padding_interlaced (padded_top, padded_bottom, input_v, 1);
		  cubic_scale_interlaced (padded_top, padded_bottom,
					  output_v, in_col, b, in_line, a,
					  cubic_spline_n, cubic_spline_m, 1);
		}
	    }
	}
      // BICIBIC ALGO
      // END OF SCALE THE FRAME
      // **********************

      // OUTPUT FRAME CONTENTS
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
		      (output_fd, frame_u_p[i], display_width / 2) != Y4M_OK)
		    goto out_error;
		}
	      for (i = 0; i < display_height / 2; i++)
		{
		  if (y4m_write
		      (output_fd, frame_v_p[i], display_width / 2) != Y4M_OK)
		    goto out_error;

		}
	    }
	}
    }
  // End of master loop => no more frame in stdin

  if (err != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("Couldn't read frame number %ld!", frame_num);
  else
    mjpeg_info ("Normal exit: end of stream with frame number %ld!",
		frame_num);
  y4m_fini_stream_info (&in_streaminfo);
  y4m_fini_stream_info (&out_streaminfo);
  y4m_fini_frame_info (&frameinfo);
  return 0;

out_error:
  mjpeg_error_exit1 ("Unable to write to output - aborting!");
  return 1;
}


// *************************************************************************************
unsigned int
pgcd (unsigned int num1, unsigned int num2)
{
  // Calculates the biggest common divider between num1 and num2, after Euclid's
  // pgcd(a,b)=pgcd(b,a%b)
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


// NO USE FOR NOW 
// *************************************************************************************
uint8_t
clip_0_255 (int16_t number)
{
  if (number <= 0)
    return (0);
  else
    {
      if (number >= 255)
	return (255);
      else
	return ((uint8_t) number);
    }
  mjpeg_error_exit1 ("function clip_0_255 failed!!!");
}

// *************************************************************************************


// NO USE FOR NOW 
// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
int
yuvscaler_RGB_init (int16_t * delta_red, int16_t * delta_green,
		    int16_t * delta_blue, uint8_t InRmin, uint8_t InRmax,
		    uint8_t OutRmin, uint8_t OutRmax, uint8_t InGmin,
		    uint8_t InGmax, uint8_t OutGmin, uint8_t OutGmax,
		    uint8_t InBmin, uint8_t InBmax, uint8_t OutBmin,
		    uint8_t OutBmax)
{
  // This function initialises the delta_(red,green,blue) vectors
  int16_t *pointer_red, *pointer_green, *pointer_blue;
  uint16_t i;


  // Filling in the vectors
  pointer_red = delta_red;
  pointer_green = delta_green;
  pointer_blue = delta_blue;
  for (i = 0; i < 256; i++)
    {
      // RED
      if (i <= InRmin)
	*(pointer_red++) = OutRmin - i;
      else
	{
	  if (i >= InRmax)
	    *(pointer_red++) = OutRmax - i;
	  else
	    *(pointer_red++) = OutRmin +
	      yuvscaler_nearest_integer_division ((unsigned long int) (OutRmax
								       -
								       OutRmin)
						  * (unsigned long int) (i -
									 InRmin),
						  (InRmax - InRmin)) - i;
	}
      mjpeg_debug ("delta_red[%u]=%d", i, delta_red[i]);
      // GREEN
      if (i <= InGmin)
	*(pointer_green++) = OutGmin - i;
      else
	{
	  if (i >= InGmax)
	    *(pointer_green++) = OutRmax - i;
	  else
	    *(pointer_green++) = OutGmin +
	      yuvscaler_nearest_integer_division ((unsigned long int) (OutGmax
								       -
								       OutGmin)
						  * (unsigned long int) (i -
									 InGmin),
						  (InGmax - InGmin)) - i;
	}
      mjpeg_debug ("delta_green[%u]=%d", i, delta_green[i]);
      // BLUE
      if (i <= InBmin)
	*(pointer_blue++) = OutBmin - i;
      else
	{
	  if (i >= InBmax)
	    *(pointer_blue++) = OutBmax - i;
	  else
	    *(pointer_blue++) = OutBmin +
	      yuvscaler_nearest_integer_division ((unsigned long int) (OutBmax
								       -
								       OutBmin)
						  * (unsigned long int) (i -
									 InBmin),
						  (InBmax - InBmin)) - i;
	}
      mjpeg_debug ("delta_green[%u]=%d", i, delta_green[i]);
    }

  return (0);
}

// *************************************************************************************


// NO USE FOR NOW 
// *************************************************************************************
// PREPROCESSING
// *************************************************************************************
// Rapidité : tabuler en fonction des 256*256*256 possibilités ? trop !!
int
yuvscaler_RGB_treatment (uint8_t * input, unsigned long size,
			 unsigned long input_height,
			 unsigned long input_width, uint8_t * delta_red,
			 uint8_t * delta_green, uint8_t * delta_blue)
{
  // This function corrects the current frame based on RGB corrections
  uint8_t *y_p, *u_p, *v_p, *line_u_p, *line_v_p;
  uint32_t i, j;
  int16_t pixel_red, pixel_green, pixel_blue;
   
  y_p = input;
  u_p = y_p + size / 4;
  v_p = u_p + size / 4;
  // Blue

  for (i = 0; i < input_height; i++)
    {
      line_u_p = u_p;
      line_v_p = v_p;
      for (j = 0; j < input_width; j++)
	{
	  pixel_red =
	    delta_red[clip_0_255
		      ((int)
		       floor (0.5 + (float) *y_p +
			      1.375 * ((float) *v_p - 128.0)))];
	  pixel_green =
	    delta_green[clip_0_255
			((int)
			 floor (0.5 + (float) *y_p -
				0.698 * ((float) *v_p - 128.0) -
				0.336 * ((float) *u_p - 128.0)))];
	  pixel_blue =
	    delta_blue[clip_0_255
		       ((int)
			floor (0.5 + (float) *y_p +
			       1.732 * ((float) *u_p - 128.0)))];
	  if (j % 2 == 1)
	    {
	      line_u_p++;
	      line_v_p++;
	    }
	  *y_p =
	    clip_0_255 ((int)
			floor (0.5 + (float) (*y_p) + 0.3000 * pixel_red +
			       0.5859 * pixel_green + 0.1120 * pixel_blue));
	  *u_p =
	    clip_0_255 ((int)
			floor (0.5 + (float) (*u_p) - 0.1719 * pixel_red -
			       0.3398 * pixel_green + 0.5117 * pixel_blue));
	  *v_p =
	    clip_0_255 ((int)
			floor (0.5 + (float) (*v_p) + 0.5117 * pixel_red -
			       0.4297 * pixel_green - 0.0820 * pixel_blue));
	}
      if (i % 2 == 1)
	{
	  u_p = line_u_p;
	  v_p = line_v_p;
	}

    }

  return (0);
}

// *************************************************************************************



/* 
 * Local variables:
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
