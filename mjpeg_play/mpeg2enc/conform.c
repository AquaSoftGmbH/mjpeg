/* conform.c, conformance checks                                            */

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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"

/* check for (level independent) parameter limits */
void range_checks()
{
  int i;

  /* range and value checks */

  if (opt_horizontal_size<1 || opt_horizontal_size>16383)
    mjpeg_error_exit1("horizontal_size must be between 1 and 16383");
  if (opt_mpeg1 && opt_horizontal_size>4095)
    mjpeg_error_exit1("horizontal_size must be less than 4096 (MPEG-1)");
  if ((opt_horizontal_size&4095)==0)
    mjpeg_error_exit1("horizontal_size must not be a multiple of 4096");
  if (opt_chroma_format!=CHROMA444 && opt_horizontal_size%2 != 0)
    mjpeg_error_exit1("horizontal_size must be a even (4:2:0 / 4:2:2)");

  if (opt_vertical_size<1 || opt_vertical_size>16383)
    mjpeg_error_exit1("vertical_size must be between 1 and 16383");
  if (opt_mpeg1 && opt_vertical_size>4095)
    mjpeg_error_exit1("vertical size must be less than 4096 (MPEG-1)");
  if ((opt_vertical_size&4095)==0)
    mjpeg_error_exit1("vertical_size must not be a multiple of 4096");
  if (opt_chroma_format==CHROMA420 && opt_vertical_size%2 != 0)
    mjpeg_error_exit1("vertical_size must be a even (4:2:0)");
  if(opt_fieldpic)
  {
    if (opt_vertical_size%2 != 0)
      mjpeg_error_exit1("vertical_size must be a even (field pictures)");
    if (opt_chroma_format==CHROMA420 && opt_vertical_size%4 != 0)
      mjpeg_error_exit1("vertical_size must be a multiple of 4 (4:2:0 field pictures)");
  }

  if (opt_mpeg1)
  {
    if (opt_aspectratio<1 || opt_aspectratio>14)
      mjpeg_error_exit1("pel_aspect_ratio must be between 1 and 14 (MPEG-1)");
  }
  else
  {
    if (opt_aspectratio<1 || opt_aspectratio>4)
      mjpeg_error_exit1("aspect_ratio_information must be 1, 2, 3 or 4");
  }

  if (opt_frame_rate_code<1 || opt_frame_rate_code>8)
    mjpeg_error_exit1("frame_rate code must be between 1 and 8");

  if (opt_bit_rate<=0.0)
    mjpeg_error_exit1("opt_bit_rate must be positive");
  if (opt_bit_rate > ((1<<30)-1)*400.0)
    mjpeg_error_exit1("opt_bit_rate must be less than 429 Gbit/s");
  if (opt_mpeg1 && opt_bit_rate > ((1<<18)-1)*400.0)
    mjpeg_error_exit1("opt_bit_rate must be less than 104 Mbit/s (MPEG-1)");

  if (opt_vbv_buffer_code<1 || opt_vbv_buffer_code>0x3ffff)
    mjpeg_error_exit1("opt_vbv_buffer_size must be in range 1..(2^18-1)");
  if (opt_mpeg1 && opt_vbv_buffer_code>=1024)
    mjpeg_error_exit1("vbv_buffer_size must be less than 1024 (MPEG-1)");

  if (opt_chroma_format<CHROMA420 || opt_chroma_format>CHROMA444)
    mjpeg_error_exit1("chroma_format must be in range 1...3");

  if (opt_video_format<0 || opt_video_format>5)
    mjpeg_error_exit1("video_format must be in range 0...5");

  if (opt_color_primaries<1 || opt_color_primaries>7 || opt_color_primaries==3)
    mjpeg_error_exit1("color_primaries must be in range 1...2 or 4...7");

  if (opt_transfer_characteristics<1 || opt_transfer_characteristics>7
      || opt_transfer_characteristics==3)
    mjpeg_error_exit1("transfer_characteristics must be in range 1...2 or 4...7");

  if (opt_matrix_coefficients<1 || opt_matrix_coefficients>7 || opt_matrix_coefficients==3)
    mjpeg_error_exit1("matrix_coefficients must be in range 1...2 or 4...7");

  if (opt_display_horizontal_size<0 || opt_display_horizontal_size>16383)
    mjpeg_error_exit1("display_horizontal_size must be in range 0...16383");
  if (opt_display_vertical_size<0 || opt_display_vertical_size>16383)
    mjpeg_error_exit1("display_vertical_size must be in range 0...16383");

  if (opt_dc_prec<0 || opt_dc_prec>3)
    mjpeg_error_exit1("intra_dc_precision must be in range 0...3");

  for (i=0; i<ctl_M; i++)
  {
    if (opt_motion_data[i].forw_hor_f_code<1 || opt_motion_data[i].forw_hor_f_code>9)
      mjpeg_error_exit1("f_code x must be between 1 and 9");
    if (opt_motion_data[i].forw_vert_f_code<1 || opt_motion_data[i].forw_vert_f_code>9)
      mjpeg_error_exit1("f_code y must be between 1 and 9");
    if (opt_mpeg1 && opt_motion_data[i].forw_hor_f_code>7)
      mjpeg_error_exit1("f_code x must be less than 8");
    if (opt_mpeg1 && opt_motion_data[i].forw_vert_f_code>7)
      mjpeg_error_exit1("f_code y must be less than 8");
    if (opt_motion_data[i].sxf<=0)
      mjpeg_error_exit1("search window must be positive"); /* doesn't belong here */
    if (opt_motion_data[i].syf<=0)
      mjpeg_error_exit1("search window must be positive");
    if (i!=0)
    {
      if (opt_motion_data[i].back_hor_f_code<1 || opt_motion_data[i].back_hor_f_code>9)
        mjpeg_error_exit1("f_code must be between 1 and 9");
      if (opt_motion_data[i].back_vert_f_code<1 || opt_motion_data[i].back_vert_f_code>9)
        mjpeg_error_exit1("f_code must be between 1 and 9");
      if (opt_mpeg1 && opt_motion_data[i].back_hor_f_code>7)
        mjpeg_error_exit1("f_code must be le less than 8");
      if (opt_mpeg1 && opt_motion_data[i].back_vert_f_code>7)
        mjpeg_error_exit1("f_code must be le less than 8");
      if (opt_motion_data[i].sxb<=0)
        mjpeg_error_exit1("search window must be positive");
      if (opt_motion_data[i].syb<=0)
        mjpeg_error_exit1("search window must be positive");
    }
  }
}

/* identifies valid profile / level combinations */
static char profile_level_defined[5][4] =
{
/* HL   H-14 ML   LL  */
  {1,   1,   1,   0},  /* HP   */
  {0,   1,   0,   0},  /* Spat */
  {0,   0,   1,   1},  /* SNR  */
  {1,   1,   1,   1},  /* MP   */
  {0,   0,   1,   0}   /* SP   */
};

static struct level_limits {
  int hor_f_code;
  int vert_f_code;
  int hor_size;
  int vert_size;
  int sample_rate;
  int opt_bit_rate; /* Mbit/s */
  int vbv_buffer_size; /* 16384 bit steps */
} maxval_tab[4] =
{
  {9, 5, 1920, 1152, 62668800, 80, 597}, /* HL */
  {9, 5, 1440, 1152, 47001600, 60, 448}, /* H-14 */
  {8, 5,  720,  576, 10368000, 15, 112}, /* ML */
  {7, 4,  352,  288,  3041280,  4,  29}  /* LL */
};

#define SP   5
#define MP   4
#define SNR  3
#define SPAT 2
#define HP   1

#define LL  10
#define ML   8
#define H14  6
#define HL   4

void profile_and_level_checks(void);


void profile_and_level_checks(void)
{
  int i;
  struct level_limits *maxval;

  if (opt_profile<0 || opt_profile>15)
    mjpeg_error_exit1("profile must be between 0 and 15");

  if (opt_level<0 || opt_level>15)
    mjpeg_error_exit1("level must be between 0 and 15");

  if (opt_profile>=8)
  {
	  mjpeg_warn("profile uses a reserved value, conformance checks skipped\n");
    return;
  }

  if (opt_profile<HP || opt_profile>SP)
    mjpeg_error_exit1("undefined Profile");

  if (opt_profile==SNR || opt_profile==SPAT)
    mjpeg_error_exit1("This encoder currently generates no scalable bitstreams");

  if (opt_level<HL || opt_level>LL || opt_level&1)
    mjpeg_error_exit1("undefined Level");

  maxval = &maxval_tab[(opt_level-4) >> 1];

  /* check profile@level combination */
  if(!profile_level_defined[opt_profile-1][(opt_level-4) >> 1])
    mjpeg_error_exit1("undefined profile@level combination");
  

  /* profile (syntax) constraints */

  if (opt_profile==SP && ctl_M!=1)
    mjpeg_error_exit1("Simple Profile does not allow B pictures");

  if (opt_profile!=HP && opt_chroma_format!=CHROMA420)
    mjpeg_error_exit1("chroma format must be 4:2:0 in specified Profile");

  if (opt_profile==HP && opt_chroma_format==CHROMA444)
    mjpeg_error_exit1("chroma format must be 4:2:0 or 4:2:2 in High Profile");

  if (opt_profile>=MP) /* SP, MP: constrained repeat_first_field */
  {
    if (opt_frame_rate_code<=2 && opt_repeatfirst)
      mjpeg_error_exit1("repeat_first_first must be zero");
    if (opt_frame_rate_code<=6 && opt_prog_seq && opt_repeatfirst)
      mjpeg_error_exit1("repeat_first_first must be zero");
  }

  if (opt_profile!=HP && opt_dc_prec==3)
    mjpeg_error_exit1("11 bit DC precision only allowed in High Profile");


  /* level (parameter value) constraints */

  /* Table 8-8 */
  if (opt_frame_rate_code>5 && opt_level>=ML)
    mjpeg_error_exit1("Picture rate greater than permitted in specified Level");

  for (i=0; i<ctl_M; i++)
  {
    if (opt_motion_data[i].forw_hor_f_code > maxval->hor_f_code)
      mjpeg_error_exit1("forward horizontal f_code greater than permitted in specified Level");

    if (opt_motion_data[i].forw_vert_f_code > maxval->vert_f_code)
      mjpeg_error_exit1("forward vertical f_code greater than permitted in specified Level");

    if (i!=0)
    {
      if (opt_motion_data[i].back_hor_f_code > maxval->hor_f_code)
        mjpeg_error_exit1("backward horizontal f_code greater than permitted in specified Level");
  
      if (opt_motion_data[i].back_vert_f_code > maxval->vert_f_code)
        mjpeg_error_exit1("backward vertical f_code greater than permitted in specified Level");
    }
  }

  /* Table 8-10 */
  if (opt_horizontal_size > maxval->hor_size)
    mjpeg_error_exit1("Horizontal size is greater than permitted in specified Level");

  if (opt_vertical_size > maxval->vert_size)
    mjpeg_error_exit1("Horizontal size is greater than permitted in specified Level");

  /* Table 8-11 */
  if (opt_horizontal_size*opt_vertical_size*opt_frame_rate > 
	  maxval->sample_rate)
    mjpeg_error_exit1("Sample rate is greater than permitted in specified Level");

  /* Table 8-12 */
  if (opt_bit_rate> 1.0e6 * maxval->opt_bit_rate)
    mjpeg_error_exit1("Bit rate is greater than permitted in specified Level");

  /* Table 8-13 */
  if (opt_vbv_buffer_code > maxval->vbv_buffer_size)
    mjpeg_error_exit1("vbv_buffer_size exceeds High Level limit");
}
