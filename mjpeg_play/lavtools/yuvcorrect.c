/*
  *  yuvcorrect.c
  *  Copyright (C) 2002 Xavier Biquard <xbiquard@free.fr>
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
// September/October 2002: First version 
// TODO:
// Slow down possibility at 1:2 => all preprocessing in a new utility called yuvcorrect

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "yuv4mpeg.h"
#include "yuvcorrect.h"

#define yuvcorrect_VERSION 18-10-2002
// For pointer adress alignement
#define ALIGNEMENT 16		// 16 bytes alignement for mmx registers in SIMD instructions for Pentium

// Arguments
const char *legal_opt_flags = "M:T:Y:R:v:h";
const char PIPE[]         = "PIPE";
const char STAT[]         = "STAT";
const char FULL[]         = "FULL";
const char HALF[]         = "HALF";
const char TOP_FIRST[]    = "INTERLACED_TOP_FIRST";
const char BOT_FIRST[]    = "INTERLACED_BOTTOM_FIRST";
const char NOT_INTER[]    = "NOT_INTERLACED";
const char PROGRESSIVE[]  = "PROGRESSIVE";
const char LINESWITCH[]   = "LINE_SWITCH";
const char NO_HEADER[]    = "NO_HEADER";
const char TOP_FORWARD[]  = "TOP_FORWARD";
const char BOTT_FORWARD[] = "BOTT_FORWARD";
const char LUMINANCE[]    = "LUMINANCE_";
const char CHROMINANCE[]  = "CHROMINANCE_";
const char Y[]            = "Y_";
const char UV[]           = "UV_";
const char CONFORM[]      = "CONFORM";


// *************************************************************************************
void
yuvcorrect_print_usage (void)
{
  fprintf (stderr,
	   "usage: yuvcorrect -M [mode_keyword] -T [general_keyword] -Y [yuv_keyword] -R [RGB_keyword]  [-v 0-2] [-h]\n"
	   "yuvcorrect applies different corrections related to interlacing and color\n"
	   "to yuv frames coming from stdin (in yuv4MPEG 4:2:2 format) to stdout.\n"
	   "In contrast to yuvscaler, frame size is kept constant.\n"
	   "\n"
	   "yuvcorrect is keyword driven :\n"
	   "\t -M for keyword concerning the correction MODE of yuvcorrect\n"
	   "\t -T for keyword concerning spatial, temporal or header corrections to be applied\n"
	   "\t -Y for keyword concerning color corrections in the yuv space\n"
	   "\t -R for keyword concerning color corrections in the RGB space (not implemented yet)\n"
	   "By default, yuvcorrect will not modify frames and simply act as a pass-through\n"
	   "\n"
	   "Possible mode keyword are:\n"
//	   "\t PIPE (default) to have yuvcorrect treated every frames it receives\n"
	   "\t STAT to have yuvcorrect print statistical information on your frames _before_ corrections\n" 
	   "\n"
	   "Possible general keyword are:\n"
	   "\t If you suspect that your video capture was given a wrong interlacing type,\n"
	   "\t and/or was spatially or temporarly missed up, please use and combine:\n"
	   "\t INTERLACED_TOP_FIRST    to correct file header by specifying top_field_first as interlacing type\n"
	   "\t INTERLACED_BOTTOM_FIRST to correct file header by specifying bottom_field_first as interlacing\n"
	   "\t NOT_INTERLACED          to correct file header by specifying not-interlaced/progressive as interlacing type\n"
	   "\t PROGRESSIVE             to correct file header by specifying not-interlaced/progressive as interlacing type\n"
	   "\t NO_HEADER    to suppress stream header generation (apply different corrections to different part of an input file)\n"
	   "\t LINE_SWITCH  to switch lines two by two\n"
	   "\t BOTT_FORWARD to move the bottom field one frame forward\n"
	   "\t TOP_FORWARD  to move the top    field one frame forward\n"
	   "\n"
	   "Possible yuv keywords are:\n"
	   "\t LUMINANCE_Gamma_InputYmin_InputYmax_OutputYmin_OutputYmax or\n"
	   "\t Y_Gamma_InputYmin_InputYmax_OutputYmin_OutputYmax\n"
	   "\t    to correct the input frame luminance by clipping it inside range [InputYmin;InputYmax],\n"
	   "\t    scale with power (1/Gamma), and expand/shrink/shift it to [OutputYmin;OutputYmax]\n"
	   "\t CHROMINANCE_UVrotation_Ufactor_Urotcenter_Vfactor_Vrotcenter_UVmin_UVmax or\n"
	   "\t UV_UVrotation_Ufactor_Urotcenter_Vfactor_Vrotcenter_UVmin_UVmax\n"
	   "\t    to rotate rescaled UV chroma components Ufactor*(U-Urotcenter) and Vfactor*(V-Vrotcenter)\n"
	   "\t    by (float) UVrotation degrees, recenter to the normalize (128,128) center,\n"
	   "\t    and _clip_ the result to range [UVmin;UVmax]\n"
	   "\t CONFORM to have yuvcorrect generate frames conform to the Rec.601 specification for Y'CbCr frames\n"
	   "\t    that is LUMINANCE_1.0_16_235_16_235 and CHROMINANCE_0.0_1.0_128_1.0_128_16_240\n"
	   "\n"
	   "\t No possible RGB keywords yet, sorry. Under development\n"
	   "\n"
	   "-v  Specifies the degree of verbosity: 0=quiet, 1=normal, 2=verbose/debug\n"
	   "-h : print this lot!\n");
  exit (1);
}
// *************************************************************************************

// ***********************************************************************
void
handle_args_overall (int argc, char *argv[], overall_t *overall)
{
  int c,verb;

   while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
     {
	switch (c)
	  {
	   case 'v':
	     verb = atoi (optarg);
	     if (verb < 0 || verb > 2)
	       {
		  mjpeg_info ("Verbose level must be 0, 1 or 2 ! => resuming to default 1");
	       } 
	     else 
	       {
		  overall->verbose = verb;
	       }
	     break;
	     
	   case 'h':
	     yuvcorrect_print_usage ();
	     break;
	     
	   default:
	     break;
	  }
     }
   if (optind != argc)
     yuvcorrect_print_usage ();
}
// *************************************************************************************

// *************************************************************************************
void
handle_args (int argc, char *argv[], overall_t *overall, general_correction_t *gen_correct, yuv_correction_t *yuv_correct)
{
  // This function handles argument passing on the command line
  int c;
  unsigned int ui1, ui2, ui3, ui4;
  int k_mode, k_general, k_yuv;
  float f1, f2, f3;

   // Ne pas oublier de mettre la putain de ligne qui suit, sinon, plus d'argument à la lign de commande, ils auront été bouffés par l'appel précédnt à getopt!!
   optind=1;
   while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
    {
      switch (c)
	{
	  // **************            
	  // MODE KEYOWRD
	  // *************
	case 'M':
	  k_mode = 0;
/*	  if (strcmp (optarg, PIPE) == 0)
	    {
	      k_mode = 1;
	      overall->mode = 0;
	    }
*/	  if (strcmp (optarg, STAT) == 0)
	    {
	      k_mode = 1;
//	      mjpeg_debug("ICi");
	      overall->stat = 1;
	    }
/*	  if (strcmp (optarg, FULL) == 0)
	    {
	      k_mode = 1;
	      overall->mode = 1;
	    }
	  if (strcmp (optarg, HALF) == 0)
	    {
	      k_mode = 1;
	      overall->mode = 2;
	    }
*/	  if (k_mode == 0)
	    mjpeg_error_exit1 ("Unrecognized MODE keyword: %s", optarg);
	  break;
	  // *************


	  // **************            
	  // GENERAL KEYOWRD
	  // *************
	case 'T':
	  k_general = 0;
	  if (strcmp (optarg, TOP_FIRST) == 0)
	    {
	      k_general = 1;
	      y4m_si_set_interlace (&gen_correct->streaminfo, Y4M_ILACE_TOP_FIRST);
	    }
	  if (strcmp (optarg, BOT_FIRST) == 0)
	    {
	      k_general = 1;
	      y4m_si_set_interlace (&gen_correct->streaminfo, Y4M_ILACE_BOTTOM_FIRST);
	    }
	  if ((strcmp (optarg, NOT_INTER) == 0)
	      || (strcmp (optarg, PROGRESSIVE) == 0))
	    {
	      k_general = 1;
	      y4m_si_set_interlace (&gen_correct->streaminfo, Y4M_ILACE_NONE);
	    }
	  if (strcmp (optarg, NO_HEADER) == 0)
	    {
	      k_general = 1;
	      gen_correct->no_header = 1;
	    }
	  if (strcmp (optarg, LINESWITCH) == 0)
	    {
	      k_general = 1;
	      gen_correct->line_switch = 1;
	    }
	  if (strcmp (optarg, BOTT_FORWARD) == 0)
	    {
	      k_general = 1;
	      gen_correct->field_move = 1;
	    }
	  if (strcmp (optarg, TOP_FORWARD) == 0)
	    {
	      k_general = 1;
	      gen_correct->field_move = -1;
	    }
	  if (k_general == 0)
	    mjpeg_error_exit1 ("Unrecognized GENERAL keyword: %s", optarg);
	  break;
	  // *************
	   
	   
	  // **************            
	  // yuv KEYOWRD
	  // **************
	 case 'Y':
	   k_yuv = 0;
	   if (strncmp (optarg, LUMINANCE, 10) == 0)
	    {
	      k_yuv = 1;
	      if (sscanf
		  (optarg, "LUMINANCE_%f_%u_%u_%u_%u", &f1, &ui1, &ui2, &ui3,
		   &ui4) == 5)
		{
		  // Coherence check:
		  if ((f1 <= 0.0) ||
		      (ui1 < 0) || (ui1 > 255) ||
		      (ui2 < 0) || (ui2 > 255) ||
		      (ui3 < 0) || (ui3 > 255) ||
		      (ui4 < 0) || (ui4 > 255) || (ui1 > ui2) || (ui3 > ui4))
		    mjpeg_error_exit1
		      ("Uncoherent luminance correction (0<>255, small, large): Gamma=%f, InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u\n",
		       f1, ui1, ui2, ui3, ui4);
		  yuv_correct->luma         = 1;
		  yuv_correct->Gamma        = f1;
		  yuv_correct->InputYmin  = (uint8_t) ui1;
		  yuv_correct->InputYmax  = (uint8_t) ui2;
		  yuv_correct->OutputYmin = (uint8_t) ui3;
		  yuv_correct->OutputYmax = (uint8_t) ui4;
		}
	      else
		mjpeg_error_exit1
		  ("Wrong number of argument to LUMINANCE keyword: %s\n",
		   optarg);
	    }
	   if (strncmp (optarg, Y, 2) == 0)
	    {
	      k_yuv = 1;
	      if (sscanf
		  (optarg, "Y_%f_%u_%u_%u_%u", &f1, &ui1, &ui2, &ui3,
		   &ui4) == 5)
		{
		  // Coherence check:
		  if ((f1 <= 0.0) ||
		      (ui1 < 0) || (ui1 > 255) ||
		      (ui2 < 0) || (ui2 > 255) ||
		      (ui3 < 0) || (ui3 > 255) ||
		      (ui4 < 0) || (ui4 > 255) || (ui1 > ui2) || (ui3 > ui4))
		    mjpeg_error_exit1
		      ("Uncoherent luminance correction (0<>255, small, large): Gamma=%f, InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u\n",
		       f1, ui1, ui2, ui3, ui4);
		  yuv_correct->luma         = 1;
		  yuv_correct->Gamma        = f1;
		  yuv_correct->InputYmin  = (uint8_t) ui1;
		  yuv_correct->InputYmax  = (uint8_t) ui2;
		  yuv_correct->OutputYmin = (uint8_t) ui3;
		  yuv_correct->OutputYmax = (uint8_t) ui4;
		}
	      else
		mjpeg_error_exit1
		  ("Wrong number of argument to Y keyword: %s\n",
		   optarg);
	    }
	  if (strncmp (optarg, CHROMINANCE, 12) == 0)
	    {
	      k_yuv = 1;
	      if (sscanf
		  (optarg, "CHROMINANCE_%f_%f_%u_%f_%u_%u_%u", &f1, &f2, &ui1,
		   &f3, &ui2, &ui3, &ui4) == 7)
		{
		  // Coherence check:
		  if ((f2 <= 0.0) || (f3 <= 0.0) ||
		      (ui1 < 0) || (ui1 > 255) ||
		      (ui2 < 0) || (ui2 > 255) ||
		      (ui3 < 0) || (ui3 > 255) ||
		      (ui4 < 0) || (ui4 > 255) ||
		      (ui3 > ui4) || (ui1 > ui4) || (ui2 > ui4))
		    mjpeg_error_exit1
		      ("Uncoherent chrominance correction (0<>255, small, large): UVrotation=%f, Ufactor=%f, Ucenter=%u, Vfactor=%f, Vcenter=%u, UVmin=%u, UVmax=%u, \n",
		       f1, f2, ui1, f3, ui2, ui3, ui4);
		  yuv_correct->chroma     = 1;
		  yuv_correct->UVrotation = f1;
		  yuv_correct->Urotcenter = (uint8_t) ui1;
		  yuv_correct->Vrotcenter = (uint8_t) ui2;
		  yuv_correct->Ufactor    = f2;
		  yuv_correct->Vfactor    = f3;
		  yuv_correct->UVmin      = (uint8_t) ui3;
		  yuv_correct->UVmax      = (uint8_t) ui4;
		}
	      else
		mjpeg_error_exit1
		  ("Wrong number of argument to CHROMINANCE keyword: %s\n",
		   optarg);
	    }
	  if (strncmp (optarg, UV, 3) == 0)
	    {
	      k_yuv = 1;
	      if (sscanf
		  (optarg, "UV_%f_%f_%u_%f_%u_%u_%u", &f1, &f2, &ui1,
		   &f3, &ui2, &ui3, &ui4) == 7)
		{
		  // Coherence check:
		  if ((f2 <= 0.0) || (f3 <= 0.0) ||
		      (ui1 < 0) || (ui1 > 255) ||
		      (ui2 < 0) || (ui2 > 255) ||
		      (ui3 < 0) || (ui3 > 255) ||
		      (ui4 < 0) || (ui4 > 255) ||
		      (ui3 > ui4) || (ui1 > ui4) || (ui2 > ui4))
		    mjpeg_error_exit1
		      ("Uncoherent chrominance correction (0<>255, small, large): UVrotation=%f, Ufactor=%f, Ucenter=%u, Vfactor=%f, Vcenter=%u, UVmin=%u, UVmax=%u, \n",
		       f1, f2, ui1, f3, ui2, ui3, ui4);
		  yuv_correct->chroma     = 1;
		  yuv_correct->UVrotation = f1;
		  yuv_correct->Urotcenter = (uint8_t) ui1;
		  yuv_correct->Vrotcenter = (uint8_t) ui2;
		  yuv_correct->Ufactor    = f2;
		  yuv_correct->Vfactor    = f3;
		  yuv_correct->UVmin      = (uint8_t) ui3;
		  yuv_correct->UVmax      = (uint8_t) ui4;
		}
	      else
		mjpeg_error_exit1
		  ("Wrong number of argument to UV keyword: %s\n",
		   optarg);
	    }
	  if (strncmp (optarg, CONFORM, 7) == 0)
	    {
	       k_yuv = 1;
	       yuv_correct->luma       = 1;
	       yuv_correct->Gamma      = 1.0;
	       yuv_correct->InputYmin  = 16;
	       yuv_correct->InputYmax  = 235;
	       yuv_correct->OutputYmin = 16;
	       yuv_correct->OutputYmax = 235;
	       yuv_correct->chroma     = 1;
	       yuv_correct->UVrotation = 0.0;
	       yuv_correct->Urotcenter = 128;
	       yuv_correct->Vrotcenter = 128;
	       yuv_correct->Ufactor    = 1.0;
	       yuv_correct->Vfactor    = 1.0;
	       yuv_correct->UVmin      = 16;
	       yuv_correct->UVmax      = 240;
	    }
	  if (k_yuv == 0)
	    mjpeg_error_exit1 ("Unrecognized yuv keyword: %s", optarg);
	  break;
	  // *************

	default:
	  break;
	}
    }


}
// *************************************************************************************


// *************************************************************************************
void
yuvcorrect_print_information (general_correction_t *gen_correct, yuv_correction_t *yuv_correct)
{
  // This function print USER'S INFORMATION

  y4m_log_stream_info (LOG_INFO, "input: ", &gen_correct->streaminfo);

  switch (gen_correct->line_switch)
    {
    case 0:
      mjpeg_info ("no line switching");
      break;
    case 1:
      mjpeg_info ("with line switching");
      break;
    default:
      mjpeg_error_exit1 ("Unknown line switching status: %d", gen_correct->line_switch);
    }

  switch (gen_correct->field_move)
    {
    case 0:
      mjpeg_info ("no time forwarding");
      break;
    case 1:
      mjpeg_info ("with bottom field one frame forward");
      break;
    case -1:
      mjpeg_info ("with top field one frame forward");
      break;
    default:
      mjpeg_error_exit1 ("Unknown time reordering status: %d", gen_correct->field_move);
    }

  switch (yuv_correct->luma)
    {
    case 0:
      mjpeg_info ("Without luminance correction");
      break;
    case 1:
      mjpeg_info ("With luminance correction");
      break;
    default:
      mjpeg_error_exit1 ("Unknown luminance correction status %u", yuv_correct->luma);
    }

  switch (yuv_correct->chroma)
    {
    case 0:
      mjpeg_info ("Without chrominance correction");
      break;
    case 1:
      mjpeg_info ("With chrominance correction");
      break;
    default:
      mjpeg_error_exit1 ("Unknown chrominance correction status %u", yuv_correct->chroma);
    }
}
// *************************************************************************************


// *************************************************************************************
// MAIN
// *************************************************************************************
int
main (int argc, char *argv[])
{
   
   // Defining yuvcorrect dedicated structures (see yuvcorrect.h) 
   overall_t *overall;
   frame_t *frame;
   general_correction_t *gen_correct; 
   yuv_correction_t *yuv_correct;
   
  int err = Y4M_OK;
  uint8_t oddeven;

  unsigned long int frame_num = 0;
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  uint8_t *field1 = NULL, *field2 = NULL;

  // Information output
  mjpeg_info ("yuvcorrect LAVPLAY_VERSION (yuvcorrect_VERSION) is a general image correction utility for yuv frames");
  mjpeg_info ("(C) 2002 Xavier Biquard <xbiquard@free.fr>, yuvcorrect -h for usage, or man yuvcorrect");

   // START OF INITIALISATION 
   // START OF INITIALISATION 
   // START OF INITIALISATION 
   // yuvcorrect overall structure initialisation
   if (!(overall = malloc(sizeof(overall_t))))
     mjpeg_error_exit1("Could not allocate memory for overall structure pointer");
   overall->verbose=1;
   overall->mode=overall->stat=0;
   handle_args_overall(argc, argv, overall);
   mjpeg_default_handler_verbosity (overall->verbose);
   
   mjpeg_debug("Start of initialisation");
      
  // yuvcorrect general_correction_t structure initialisations
   if (!(gen_correct = malloc(sizeof(general_correction_t))))
     mjpeg_error_exit1("Could not allocate memory for gen_correct structure pointer");
   gen_correct->no_header=gen_correct->line_switch=gen_correct->field_move=0;
   y4m_init_stream_info (&gen_correct->streaminfo);
   if (y4m_read_stream_header (0, &gen_correct->streaminfo) != Y4M_OK)
     mjpeg_error_exit1 ("Could'nt read yuv4mpeg header!");
   
  // yuvcorrect frame_t structure initialisations
   if (!(frame = malloc(sizeof(frame_t))))
     mjpeg_error_exit1("Could not allocate memory for frame structure pointer");
  frame->y_width   = y4m_si_get_width (&gen_correct->streaminfo);
  frame->y_height  = y4m_si_get_height (&gen_correct->streaminfo);
  frame->nb_y      = frame->y_width*frame->y_height;
  frame->uv_width  = frame->y_width >> 1;  // /2
  frame->uv_height = frame->y_height >> 1; // /2
  frame->nb_uv     = frame->nb_y >> 2;     // /4
  frame->length    = (frame->nb_y * 3 ) >> 1; // * 3/2
  if (!(u_c_p = malloc (frame->length + ALIGNEMENT)))
     mjpeg_error_exit1  ("Could not allocate memory for frame table. STOP!");
   mjpeg_debug ("before alignement: %p", u_c_p);
   if (((unsigned long) u_c_p % ALIGNEMENT) != 0)
     u_c_p =(uint8_t *) ((((unsigned long) u_c_p / ALIGNEMENT) + 1) * ALIGNEMENT);
   mjpeg_debug ("after alignement: %p", u_c_p);
  frame->y          = u_c_p;
  frame->u          = frame->y + frame->nb_y;
  frame->v          = frame->u + frame->nb_uv;
   y4m_init_frame_info (&frame->info);
   
   
 // yuvcorrect yuv_correction_t structure initialisation  
   if (!(yuv_correct = malloc(sizeof(yuv_correction_t))))
     mjpeg_error_exit1("Could not allocate memory for yuv_correct structure pointer");
   yuv_correct->luma=yuv_correct->chroma=0;
   yuv_correct->luminance=yuv_correct->chrominance=NULL; 
   yuv_correct->InputYmin=yuv_correct->OutputYmin=yuv_correct->UVmin=0;
   yuv_correct->InputYmax=yuv_correct->OutputYmax=yuv_correct->UVmax=255;
   yuv_correct->Gamma=yuv_correct->Ufactor=yuv_correct->Vfactor=1.0;
   yuv_correct->UVrotation=0.0;
   yuv_correct->Urotcenter=yuv_correct->Vrotcenter=128;

  // Deal with args 
  handle_args(argc, argv, overall, gen_correct, yuv_correct);
   
  // Further initialisations depending on the stream itself 
  // General correction initialisations
  if (gen_correct->field_move != 0) 
     {
      if (!(field1 = malloc (frame->length >> 1)) ||
	  !(field2 = malloc (frame->length >> 1)))
	  mjpeg_error_exit1
	  ("Could not allocate memory for field1 or field2 tables. STOP!");
     }
  // Luminance correction initialisation
  if (yuv_correct->luma == 1)
    {
      // Memory allocation for the luminance vector
      if (!
	  (u_c_p =
	   (uint8_t *) malloc (256 * sizeof (uint8_t) + ALIGNEMENT)))
	mjpeg_error_exit1
	  ("Could not allocate memory for luminance table. STOP!");
      if (((unsigned long) u_c_p % ALIGNEMENT) != 0)
	u_c_p =
	  (uint8_t *) ((((unsigned long) u_c_p / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
       yuv_correct->luminance=u_c_p;
      // Filling in the luminance vectors
      yuvcorrect_luminance_init (yuv_correct);
    }
  // Chrominance correction initialisation
  if (yuv_correct->chroma == 1)
    {
      // Memory allocation for the UVchroma vector
      if (!(u_c_p =
	    (uint8_t *) malloc (2 * 256 * 256 * sizeof (uint8_t) +
				ALIGNEMENT)))
	mjpeg_error_exit1
	  ("Could not allocate memory for UVchroma vector. STOP!");
      // memory alignement of the 2 chroma vectors
      if (((unsigned long) u_c_p % ALIGNEMENT) != 0)
	u_c_p =
	  (uint8_t *) ((((unsigned long) u_c_p / ALIGNEMENT) + 1) *
		       ALIGNEMENT);
       yuv_correct->chrominance=u_c_p;
      // Filling in the UVchroma vector
      yuvcorrect_chrominance_init (yuv_correct);
    }
   
  // USER'S INFORMATION OUTPUT
  yuvcorrect_print_information (gen_correct,yuv_correct);

   mjpeg_debug ("End of Initialisation");
  // END OF INITIALISATION
  // END OF INITIALISATION
  // END OF INITIALISATION


  // Eventually output file header
  if (gen_correct->no_header == 0)
    y4m_write_stream_header (1, &gen_correct->streaminfo);

  mjpeg_debug("overall: verbose=%u, mode=%d, stat=%u",overall->verbose,overall->mode,overall->stat);
  mjpeg_debug("frame: Y:%ux%u=>%lu UV:%ux%u=>%lu Size=%lu",frame->y_width,frame->y_height,frame->nb_y,frame->uv_width,frame->uv_height,frame->nb_uv,frame->length);
  mjpeg_debug("yuv: Gamma=%f, InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u",yuv_correct->Gamma,yuv_correct->InputYmin,yuv_correct->InputYmax,yuv_correct->OutputYmin,yuv_correct->OutputYmax);
   // Master loop : continue until there is no next frame in stdin
   while ((err = yuvcorrect_y4m_read_frame (0, frame, gen_correct)) == Y4M_OK)
     {
	if (overall->stat==1)
	  yuvstat (frame);
	
	mjpeg_info ("Frame number %ld", frame_num);
	
	// Time reordering
	if (gen_correct->field_move != 0)
	  {
	     oddeven = frame_num & (unsigned long int)1; // oddeven = frame_num % 2, fast implementation
	     if (gen_correct->field_move == 1)
	       {
		  // Bottom field one frame forward
		  if (frame_num == 0)
		    {
		       bottom_field_storage (frame, oddeven, field1, field2);
		       if (yuvcorrect_y4m_read_frame(0, frame, gen_correct) != Y4M_OK)
			 mjpeg_error_exit1 ("Can't read frame %ld", frame_num);
		       frame_num++;
		       oddeven = frame_num & (unsigned long int)1; 
		       mjpeg_info ("Frame number %ld", frame_num);
		    }
		  bottom_field_storage (frame, oddeven, field1, field2);
		  bottom_field_replace (frame, oddeven, field1, field2);
	       }
	     else
	       {
		  // Top field one frame forward
		  if (frame_num == 0)
		    {
		       top_field_storage (frame, oddeven, field1, field2);
		       if (yuvcorrect_y4m_read_frame(0, frame, gen_correct) != Y4M_OK)
			 mjpeg_error_exit1 ("Can't read frame %ld", frame_num);
		       frame_num++;
		       oddeven = frame_num & (unsigned long int)1; 
		       mjpeg_info ("Frame number %ld", frame_num);
		    }
		  top_field_storage (frame, oddeven, field1, field2);
		  top_field_replace (frame, oddeven, field1, field2);
	       }
	  }
	
	
	// luminance correction
	if (yuv_correct->luma == 1)
	  yuvcorrect_luminance_treatment (frame, yuv_correct);
	// chrominance correction
	if (yuv_correct->chroma == 1)
	  yuvcorrect_chrominance_treatment (frame, yuv_correct);
	

	// Output Frame Header
	if (y4m_write_frame_header (1, &frame->info) != Y4M_OK)
	  goto out_error;
	
	// Output Frame content
	if (y4m_write (1, frame->y, frame->length) != Y4M_OK)
	  goto out_error;
	
	frame_num++;
	
     }
   // End of master loop => no more frame in stdin

   if (err != Y4M_ERR_EOF)
     mjpeg_error_exit1 ("Couldn't read frame number %ld!", frame_num);
   else
     mjpeg_info ("Normal exit: end of stream with frame number %ld!",frame_num);
   y4m_fini_stream_info (&gen_correct->streaminfo);
   y4m_fini_frame_info (&frame->info);
   return 0;
   
   
out_error:
  mjpeg_error_exit1 ("Unable to write to output - aborting!");
  return 1;
}
// *************************************************************************************
// *************************************************************************************
// *************************************************************************************
// *************************************************************************************


/* 
 * Local variables:
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
