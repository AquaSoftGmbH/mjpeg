/*
  *  yuvcorrect_tune.c
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
// add bott_forward and top_forward while frame is read_frame (general corrections)
// Slow down possibility at 1:2 => all preprocessing in a new utility called yuvcorrect

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include "yuv4mpeg.h"
#include "yuvcorrect.h"

#define yuvcorrect_VERSION LAVPLAY_VERSION
// For pointer adress alignement
#define ALIGNEMENT 16		// 16 bytes alignement for mmx registers in SIMD instructions for Pentium

// Arguments
const char *legal_opt_flags = "M:T:Y:R:I:v:h";
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

void yuvcorrect_tune_print_usage (void);


// *************************************************************************************
void
yuvcorrect_tune_print_usage (void)
{
  fprintf (stderr,
	   "usage: yuvcorrect_tune -I <filename> -M [mode_keyword] -T [general_keyword] -Y [yuv_keyword] -R [RGB_keyword]  [-v 0-2] [-h]\n"
	   "yuvcorrect_tune is an interactive tools enabling you to interactively tune different\n"
	   "corrections related to interlacing, luminance and color. <filename> defines the uncorrected\n"
	   "reference yuv frames (in yuv4MPEG 4:2:2 format)\n"
	   "Typical use is 'lav2yuv -f 1 <videofile> > frame ; cat fifo | yuvplay ; yuvcorrect_tune -I frame <keyowrds> > fifo' \n"
	   "\n"
	   "yuvcorrect_tune is keyword driven :\n"
	   "\t -I <filename>\n"
	   "\t -M for keyword concerning the correction MODE of yuvcorrect_tune\n"
	   "\t -T for keyword concerning spatial, temporal or header corrections to be applied\n"
	   "\t -Y for keyword concerning color corrections in the yuv space\n"
	   "\t -R for keyword concerning color corrections in the RGB space (not implemented yet)\n"
	   "\n"
	   "Possible mode keyword are:\n"
	   "\t FULL (default) Yuvcorrect_tune will then use _only_ the first frame it reads from <filename> as a raw reference.\n"
	   "\t      All corrections defined by keywords on the command line will then be applied to this raw reference\n" 
	   "\t      to generate the final reference\n"
	   "\t      Succesive keystrokes on the keyboard will activate or deactivate general keyword, and increase or decrease\n"
	   "\t      color corrections values. At each keystroke, yuvcorrect_tune will generate on stdout one frame that correcponds\n"
	   "\t      to the final reference frame with keyboard defined corrections.\n"
//	   "\t HALF same as FULL except that the generated frame left half is simular to the reference,\n"
//	   "\t      and simetrically the right half shows the corrected left half.\n"
	   "\n"
	   "Possible general keyword are:\n"
	   "\t LINE_SWITCH  to switch lines two by two\n"
	   "\n"
	   "Possible yuv keywords are:\n"
	   "\t LUMINANCE_Gamma_InputYmin_InputYmax_OutputYmin_OutputYmax\n"
	   "\t    to correct the input frame luminance by clipping it inside range [InputYmin;InputYmax],\n"
	   "\t    scale with power (1/Gamma), and expand/shrink/shift it to [OutputYmin;OutputYmax]\n"
	   "\t    (press E/e to increase/decrease Gamma by 0.05, starting from 1.0)\n"
	   "\t    (press R/r to increase/decrease InputYmin by 1, starting from 0)\n"
	   "\t    (press T/t to increase/decrease InputYmax by 1, starting from 255)\n"
	   "\t    (press Y/y to increase/decrease OutputYmin by 1, starting from 0)\n"
	   "\t    (press U/u to increase/decrease OutputYmax by 1, starting from 255)\n"
	   "\t    (all letters in the common part of the first raw for most us/european keyboards)\n"
	   "\t CHROMINANCE_UVrotation_Ufactor_Urotcenter_Vfactor_Vrotcenter_UVmin_UVmax\n"
	   "\t    to rotate rescaled UV chroma components Ufactor*(U-Urotcenter) and Vfactor*(V-Vrotcenter)\n"
	   "\t    by (float) UVrotation degrees, recenter to the normalize (128,128) center,\n"
	   "\t    and _clip_ the result to range [UVmin;UVmax]\n"
	   "\t    (press D/d to increase/decrease UVrotation by 1, starting from 0.0)\n"
	   "\t    (press F/f to increase/decrease Ufactor by 0.05, starting from 1.0)\n"
	   "\t    (press G/g to increase/decrease Urotcenter by 1, starting from 128)\n"
	   "\t    (press H/h to increase/decrease Vfactor by 0.05, starting from 1.0)\n"
	   "\t    (press J/j to increase/decrease Vrotcenter by 1, starting from 128)\n"
	   "\t    (press K/k to increase/decrease UVmin by 1, starting from 0)\n"
	   "\t    (press L/l to increase/decrease UVmax by 1, starting from 255)\n"
	   "\t    (all letters in the common part of the second raw for most us/european keyboards)\n"
//	   "\t CONFORM to have yuvcorrect_tune generate frames conform to the Rec.601 specification for Y'CbCr frames\n"
//	   "\t    for the moment equivalent to LUMINANCE_1.0_0_255_16_235 and CHROMINANCE_0.0_1.0_128_1.0_128_16_240\n"
//	   "\n"
//	   "No possible RGB keywords yet, under development\n"
	   "\n"
	   "-v  Specifies the degree of verbosity: 0=quiet, 1=normal, 2=verbose/debug\n"
	   "-h : print this lot!\n");
  exit (1);
}
// *************************************************************************************



// *************************************************************************************
void
handle_args_overall (int argc, char *argv[], overall_t *overall)
{
  int c,verb;

   while ((c = getopt (argc, argv, legal_opt_flags)) != -1)
     {
	switch (c)
	  {
	   case 'I':
	     if ((overall->RefFrame=open(optarg,O_RDONLY))==-1)
	       mjpeg_error_exit1("Unable to open %s!!",optarg);
	     break;
	   
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
	     yuvcorrect_tune_print_usage ();
	     break;
	     
	   default:
	     break;
	  }
     }
   if (optind != argc)
     yuvcorrect_tune_print_usage ();
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
	  if (strcmp (optarg, FULL) == 0)
	    {
	      k_mode = 1;
	      overall->mode = 1;
	    }
/*	  if (strcmp (optarg, HALF) == 0)
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
	  if (strcmp (optarg, LINESWITCH) == 0)
	    {
	      k_general = 1;
	      gen_correct->line_switch = 1;
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
   
  int err = Y4M_OK,c;

  unsigned long int frame_num = 0;
  uint8_t *u_c_p,*final_ref;		//u_c_p = uint8_t pointer

  // Information output
  mjpeg_info ("yuvcorrect_tune (version yuvcorrect_VERSION) is a general scaling utility for yuv frames");
  mjpeg_info ("(C) 2002 Xavier Biquard <xbiquard@free.fr>");
  mjpeg_info ("yuvcorrect_tune -h for help, or man yuvcorrect_tune");

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
   if (y4m_read_stream_header (overall->RefFrame, &gen_correct->streaminfo) != Y4M_OK)
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
   yuv_correct->luma=yuv_correct->chroma=1; // Automatically activate corrections
   yuv_correct->luminance=yuv_correct->chrominance=NULL; 
   yuv_correct->InputYmin=yuv_correct->OutputYmin=yuv_correct->UVmin=0;
   yuv_correct->InputYmax=yuv_correct->OutputYmax=yuv_correct->UVmax=255;
   yuv_correct->Gamma=yuv_correct->Ufactor=yuv_correct->Vfactor=1.0;
   yuv_correct->UVrotation=0.0;
   yuv_correct->Urotcenter=yuv_correct->Vrotcenter=128;

   // Deal with args 
   handle_args(argc, argv, overall, gen_correct, yuv_correct);
   
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
   
   mjpeg_debug ("End of Initialisation");
   // END OF INITIALISATION
   // END OF INITIALISATION
   // END OF INITIALISATION

  y4m_write_stream_header (1, &gen_correct->streaminfo);

  mjpeg_debug("overall: verbose=%u, mode=%d, stat=%u",overall->verbose,overall->mode,overall->stat);
  mjpeg_debug("frame: Y:%ux%u=>%lu UV:%ux%u=>%lu Size=%lu",frame->y_width,frame->y_height,frame->nb_y,frame->uv_width,frame->uv_height,frame->nb_uv,frame->length);
  mjpeg_debug("yuv: Gamma=%f, InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u",yuv_correct->Gamma,yuv_correct->InputYmin,yuv_correct->InputYmax,yuv_correct->OutputYmin,yuv_correct->OutputYmax);
   
  mjpeg_debug("yuv: Gamma=%f, InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u",yuv_correct->Gamma,yuv_correct->InputYmin,yuv_correct->InputYmax,yuv_correct->OutputYmin,yuv_correct->OutputYmax);
   if (!(u_c_p = malloc (frame->length + ALIGNEMENT)))
     mjpeg_error_exit1  ("Could not allocate memory for frame table. STOP!");
   mjpeg_debug ("before alignement: %p", u_c_p);
   if (((unsigned long) u_c_p % ALIGNEMENT) != 0)
     u_c_p =(uint8_t *) ((((unsigned long) u_c_p / ALIGNEMENT) + 1) * ALIGNEMENT);
   mjpeg_debug ("after alignement: %p", u_c_p);
   final_ref=u_c_p;
   // Read raw reference frame and apply corrections eventually defined on command line
   if((err = yuvcorrect_y4m_read_frame (overall->RefFrame, frame, gen_correct)) != Y4M_OK)
     mjpeg_error_exit1("Unable to read Raw Reference frame. Aborting now !!");
   // luminance correction
   yuvcorrect_luminance_treatment (frame, yuv_correct);
   // chrominance correction
   yuvcorrect_chrominance_treatment (frame, yuv_correct);
   // Now, frame->y points on the final reference frame
   memcpy(final_ref,frame->y,frame->length);
   // Output Frame Header
   if (y4m_write_frame_header (1, &frame->info) != Y4M_OK)
     goto out_error;
   // Output Frame content
   if (y4m_write (1, frame->y, frame->length) != Y4M_OK)
     goto out_error;
   
   // Master loop : continue until there is no more keystrokes
   while ((c=getc(stdin))!=EOF) 
     {
	// put back final reference inside frame pointer
	memcpy(frame->y,final_ref,frame->length);
	switch(c) 
	  {
	   case 'E':  // Increase Gamma value
	     yuv_correct->Gamma+=0.05;
	     mjpeg_info("Gamma = %f",yuv_correct->Gamma);
	     break;
	     
	   case 'e':  // Decrease Gamma value
	     if (yuv_correct->Gamma<=0.05)
	       mjpeg_info("Gamma value would become negative!! Ignoring");
	     else
	       yuv_correct->Gamma-=0.05;
	     mjpeg_info("Gamma = %f",yuv_correct->Gamma);
	     break;
	     
	   case 'R':  // Increase InputYmin by 1
	     if (yuv_correct->InputYmin==255)
	       mjpeg_info("InputYmin would be greater than 255!! Ignoring");
	     else
	       yuv_correct->InputYmin+=1;
	     mjpeg_info("InputYmin = %u",yuv_correct->InputYmin);
	     break;

	   case 'r':  // Decrease InputYmin by 1
	     if (yuv_correct->InputYmin==0)
	       mjpeg_info("InputYmin would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->InputYmin-=1;
	     mjpeg_info("InputYmin = %u",yuv_correct->InputYmin);
	     break;

	   case 'T':  // Increase InputYmax by 1
	     if (yuv_correct->InputYmax==255)
	       mjpeg_info("InputYmax would be greater than 255!! Ignoring");
	     else
	       yuv_correct->InputYmax+=1;
	     mjpeg_info("InputYmax = %u",yuv_correct->InputYmax);
	     break;

	   case 't':  // Decrease InputYmax by 1
	     if (yuv_correct->InputYmax==0)
	       mjpeg_info("InputYmax would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->InputYmax-=1;
	     mjpeg_info("InputYmax = %u",yuv_correct->InputYmax);
	     break;

	   case 'Y':  // Increase OutputYmin by 1
	     if (yuv_correct->OutputYmin==255)
	       mjpeg_info("OutputYmin would be greater than 255!! Ignoring");
	     else
	       yuv_correct->OutputYmin+=1;
	     mjpeg_info("OutputYmin = %u",yuv_correct->OutputYmin);
	     break;

	   case 'y':  // Decrease OutputYmin by 1
	     if (yuv_correct->OutputYmin==0)
	       mjpeg_info("OutputYmin would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->OutputYmin-=1;
	     mjpeg_info("OutputYmin = %u",yuv_correct->OutputYmin);
	     break;

	   case 'U':  // Increase OutputYmax by 1
	     if (yuv_correct->OutputYmax==255)
	       mjpeg_info("OutputYmax would be greater than 255!! Ignoring");
	     else
	       yuv_correct->OutputYmax+=1;
	     mjpeg_info("OutputYmax = %u",yuv_correct->OutputYmax);
	     break;

	   case 'u':  // Decrease OutputYmax by 1
	     if (yuv_correct->OutputYmax==0)
	       mjpeg_info("OutputYmax would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->OutputYmax-=1;
	     mjpeg_info("OutputYmax = %u",yuv_correct->OutputYmax);
	     break;
	     
	   case 'D':  // Increase UBrotation by 1
	     yuv_correct->UVrotation+=1.0;
	     mjpeg_info("UVrotation = %f",yuv_correct->UVrotation);
	     break;

	   case 'd':  // Decrease UBrotation by 1
	     yuv_correct->UVrotation-=1.0;
	     mjpeg_info("UVrotation = %f",yuv_correct->UVrotation);
	     break;

	   case 'F':  // Increase Ufactor
	     yuv_correct->Ufactor+=0.05;
	     mjpeg_info("UFactor = %f",yuv_correct->Ufactor);
	     break;
	     
	   case 'f':  // Decrease Ufactor value
	     if (yuv_correct->Ufactor<=0.05)
	       mjpeg_info("Ufactor value would become negative!! Ignoring");
	     else
	       yuv_correct->Ufactor-=0.05;
	     mjpeg_info("Ufactor = %f",yuv_correct->Ufactor);
	     break;

	   case 'G':  // Increase Urotcenter by 1
	     if (yuv_correct->Urotcenter==255)
	       mjpeg_info("Urotcenter would be greater than 255!! Ignoring");
	     else
	       yuv_correct->Urotcenter+=1;
	     mjpeg_info("Urotcenter = %u",yuv_correct->Urotcenter);
	     break;

	   case 'g':  // Decrease Urotcenter by 1
	     if (yuv_correct->Urotcenter==0)
	       mjpeg_info("Urotcenter would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->Urotcenter-=1;
	     mjpeg_info("Urotcenter = %u",yuv_correct->Urotcenter);
	     break;

	     
	   case 'H':  // Increase Vfactor
	     yuv_correct->Vfactor+=0.05;
	     mjpeg_info("VFactor = %f",yuv_correct->Vfactor);
	     break;
	     
	   case 'h':  // Decrease Vfactor value
	     if (yuv_correct->Vfactor<=0.05)
	       mjpeg_info("Vfactor value would become negative!! Ignoring");
	     else
	       yuv_correct->Vfactor-=0.05;
	     mjpeg_info("Vfactor = %f",yuv_correct->Vfactor);
	     break;

	   case 'J':  // Increase Vrotcenter by 1
	     if (yuv_correct->Vrotcenter==255)
	       mjpeg_info("Vrotcenter would be greater than 255!! Ignoring");
	     else
	       yuv_correct->Vrotcenter+=1;
	     mjpeg_info("Vrotcenter = %u",yuv_correct->Vrotcenter);
	     break;

	   case 'j':  // Decrease Vrotcenter by 1
	     if (yuv_correct->Vrotcenter==0)
	       mjpeg_info("Vrotcenter would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->Vrotcenter-=1;
	     mjpeg_info("Vrotcenter = %u",yuv_correct->Vrotcenter);
	     break;
	     
	   case 'K':  // Increase UVmin by 1
	     if (yuv_correct->UVmin==255)
	       mjpeg_info("UVmin would be greater than 255!! Ignoring");
	     else
	       yuv_correct->UVmin+=1;
	     mjpeg_info("UVmin = %u",yuv_correct->UVmin);
	     break;

	   case 'k':  // Decrease UVmin by 1
	     if (yuv_correct->UVmin==0)
	       mjpeg_info("UVmin would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->UVmin-=1;
	     mjpeg_info("UVmin = %u",yuv_correct->UVmin);
	     break;
	     
	   case 'L':  // Increase UVmax by 1
	     if (yuv_correct->UVmax==255)
	       mjpeg_info("UVmax would be greater than 255!! Ignoring");
	     else
	       yuv_correct->UVmax+=1;
	     mjpeg_info("UVmax = %u",yuv_correct->UVmax);
	     break;

	   case 'l':  // Decrease UVmax by 1
	     if (yuv_correct->UVmax==0)
	       mjpeg_info("UVmax would be smaller than 0!! Ignoring");
	     else
	       yuv_correct->UVmax-=1;
	     mjpeg_info("UVmax = %u",yuv_correct->UVmax);
	     break;
	     
	     
	   default:
	     break;
	  }
	mjpeg_debug("yuv: Gamma=%f, InputYmin=%u, InputYmax=%u, OutputYmin=%u, OutputYmax=%u",yuv_correct->Gamma,yuv_correct->InputYmin,yuv_correct->InputYmax,yuv_correct->OutputYmin,yuv_correct->OutputYmax);
	// Apply correction
	yuvcorrect_luminance_init (yuv_correct);
	yuvcorrect_chrominance_init (yuv_correct);
	yuvcorrect_luminance_treatment (frame, yuv_correct);
	yuvcorrect_chrominance_treatment (frame, yuv_correct);
	     
	// Output Frame Header
	if (y4m_write_frame_header (1, &frame->info) != Y4M_OK)
	  goto out_error;
	// Output Frame content
	if (y4m_write (1, frame->y, frame->length) != Y4M_OK)
	  goto out_error;
     }

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
