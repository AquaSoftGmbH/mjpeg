/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
// Version of the 25/05/2003
// Copyright X. Biquard xbiquard@free.fr
// Great speed Warning : function mjpeg_debug implies an implicit test => may slow down a lot the execution
// of the program.
// SIMD accelerated multiplications with csplineh not possible since value to be multiply do not stand
// in int16_t, but in int32_t
// Maybe possible in SSE since xmm registers of 128 bits available

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
#include "mjpeg_types.h"
#include "yuvscaler.h"
#include "attributes.h"
#include "../utils/mmx.h"

extern unsigned int output_active_width;
extern unsigned int output_active_height;
extern unsigned int output_width;
extern unsigned int input_width;
extern unsigned int input_useful_width;
extern unsigned int input_useful_height;
extern unsigned int specific;
// extern uint8_t *divide;
// extern long int bicubic_offset;


// Defines
#define FLOAT2INTEGERPOWER 11
extern long int FLOAT2INTOFFSET;

// MMX test
#ifdef HAVE_ASM_MMX
extern int16_t *mmx_padded, *mmx_cubic;
extern int32_t *mmx_res;
extern long int max_lint_neg;	// maximal negative number available for long int
extern int mmx;			// =1 for mmx activated
#endif
// MMX test


// *************************************************************************************
int
cubic_scale (uint8_t * padded_input, uint8_t * output, 
	     unsigned int *in_col, unsigned int *in_line,
	     int16_t ** cspline_w_neighbors,uint16_t  width_neighbors,
	     int16_t ** cspline_h_neighbors,uint16_t height_neighbors,
	     unsigned int half)
{
  // Warning: because cubic-spline values may be <0 or >1.0, a range test on value is mandatory
  unsigned int local_output_active_width = output_active_width >> half;
  unsigned int local_output_active_height = output_active_height >> half;
  unsigned int local_output_width = output_width >> half;
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_padded_width = local_input_useful_width + width_neighbors -1;
  unsigned int out_line, out_col,w,h;
  uint32_t in_line_offset;
  int16_t output_offset;

  uint8_t *output_p,*line,*line_start[height_neighbors];
  int16_t *csplinew[width_neighbors],*csplineh[height_neighbors];
  int32_t value=0,value1=0,value2=0;

//   mjpeg_debug ("Start of cubic_scale ");

   output_p = output;
   output_offset = local_output_width-local_output_active_width;
   
   /* *INDENT-OFF* */
   
   switch(specific) {
    case 0: 
	{
#ifdef HAVE_ASM_MMX
	   if (mmx==1) 
	     {
		emms();
		// fill mm7 with zeros
		pxor_r2r(mm7,mm7);
		// MMX ROUTINE
		for (h=0;h<height_neighbors;h++)
		  csplineh[h]=cspline_h_neighbors[h];
		for (out_line = 0; out_line < local_output_active_height; out_line++)
		  {
		     in_line_offset = in_line[out_line] * local_padded_width;
		     // csplinew values in mmx registers
		     // Impossible de copier un segment mémoire de csplinew dans mmx_cubic
		     // En effet, si les pointeurs csplinew[w] sont contigus en mémoire,
		     // ce n'est pas le cas pour les valeurs pointées par ces pointeurs.
		     // Et comme c'est les valeurs qu'ils faut copier ...
		     switch(width_neighbors)
		       {
			case 4:
			  // csplinew dans mm0
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  break;
			case 6:
			  // csplinew dans mm0 et mm1
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  movq_m2r(*mmx_cubic,mm1);
			  break;
			case 8:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  // csplinew dans mm0 et mm1
			  break;
			case 10:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  movq_m2r(*mmx_cubic,mm2);
			  // csplinew dans mm0, mm1 & mm2
			  break;
			case 12:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  // csplinew dans mm0, mm1 & mm2
			  break;
			case 14:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			  movq_m2r(*mmx_cubic,mm3);
			  // csplinew dans mm0, mm1, mm2 & mm3
			  break;
			case 16:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			  mmx_cubic[2]=*(csplinew[14]=cspline_w_neighbors[14]);mmx_cubic[3]=*(csplinew[15]=cspline_w_neighbors[15]);
			  movq_m2r(*mmx_cubic,mm3);
			  // csplinew dans mm0, mm1, mm2 & mm3
			  break;
			default:
			  mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			  break;
		       }
		     for (out_col = 0; out_col < local_output_active_width; out_col++)
		       {
			  line_start[0]=padded_input + in_col[out_col] + in_line_offset;
			  for (h=1;h<height_neighbors;h++) 
			    line_start[h]=line_start[h-1]+local_padded_width;
			  value1=0;
			  for (h=0;h<height_neighbors;h++) 
			    {
			       // pixels values are put in mm6. Comme c'est des uint8_t,
			       // on en copie 8 puis on les interleave avec des zéros de mm7
			       // Ensuite, on multiplie par les csplinew
			       // SUPER WARNING: DON'T FORGET TO PUT A (mmx_t *) when using a "register2memory"
			       // mmx instruction. Otherwise, you will get nasty side effect on calculations
			       // involving mmx_res[0] or mmx_res[1] with gcc -O2, that is value1 and value2 here ...
			       switch(width_neighbors)
				 {
				  case 4:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value1+=(mmx_res[0]+mmx_res[1])*(*csplineh[h]);
				    break;
				  case 6:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 8:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 10:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 12:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 14:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+12),mm6); punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm3,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 16:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+12),mm6); punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm3,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  default:
				    mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
				    break;
				 }
			    }
			  // Mise à jour des valeurs de csplinew
			  switch(width_neighbors)
			    {
			     case 4:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       break;
			     case 6:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       movq_m2r(*mmx_cubic,mm1);
			       break;
			     case 8:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       break;
			     case 10:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       movq_m2r(*mmx_cubic,mm2);
			       break;
			     case 12:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       break;
			     case 14:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       mmx_cubic[0]=*(++csplinew[12]);  mmx_cubic[1]=*(++csplinew[13]);
			       movq_m2r(*mmx_cubic,mm3);
			       break;
			     case 16:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       mmx_cubic[0]=*(++csplinew[12]);  mmx_cubic[1]=*(++csplinew[13]);
			       mmx_cubic[2]=*(++csplinew[14]);  mmx_cubic[3]=*(++csplinew[15]);
			       movq_m2r(*mmx_cubic,mm3);
			       break;
			     default:
			       mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			       break;
			    }
			  if (value1 < 0) 
			    *(output_p++) = 0;
			  else 
			    {
			       value =
				 (value1 +
				  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
			       
			       if (value > 255) 
				 *(output_p++) = 255;
			       else
				 *(output_p++) = (uint8_t) value;
			    }
		       }
		     // a line on output is now finished. We jump to the beginning of the next line
		     output_p+=output_offset;
		     for (h=0;h<height_neighbors;h++)
		       csplineh[h]++;
		  }
	     }
	   // END OF MMX ROUTINE
	   else
#endif
	     {
//	      mjpeg_info("NOT-MMX ROUTINE");
	      // NOT-MMX ROUTINE
	      for (h=0;h<height_neighbors;h++)
		csplineh[h]=cspline_h_neighbors[h];
	      for (out_line = 0; out_line < local_output_active_height; out_line++)
		{
		   in_line_offset = in_line[out_line] * local_padded_width;
		   for (w=0;w<width_neighbors;w++)
		     csplinew[w]=cspline_w_neighbors[w];
		   for (out_col = 0; out_col < local_output_active_width; out_col++)
		     {
			line_start[0]=padded_input + in_col[out_col] + in_line_offset;
			for (h=1;h<height_neighbors;h++) 
			  {
			     line_start[h]=line_start[h-1]+local_padded_width;
			  }
			value1=0;
			for (h=0;h<height_neighbors;h++) 
			  {
			     // Please note that width_neighbors and height_neighbors are even and superior to 4
			     value2=0;
			     for (w=0;w<width_neighbors;w++)
			       value2+=(*(line_start[h]++))*(*csplinew[w]);
			     value1+=value2*(*csplineh[h]);
			  }
			if (value1 < 0)
			  *(output_p++) = 0;
			else 
			  {
			     value =
			       (value1 +
				(1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
			     
			     if (value > 255) 
			       *(output_p++) = 255;
			     else
			       *(output_p++) = (uint8_t) value;
			  }
			for (w=0;w<width_neighbors;w++)
			  csplinew[w]++;
		     }
		   // a line on output is now finished. We jump to the beginning of the next line
		   output_p+=output_offset;
		   for (h=0;h<height_neighbors;h++)
		     csplineh[h]++;
		}
	      // END OF NOT-MMX ROUTINE
	   }
      }
    break;
    
  case 1:
    // We only downscale on width, not height
	{
#ifdef HAVE_ASM_MMX
	   if (mmx==1) 
	   {
	      emms();
	      // fill mm7 with zeros
	      pxor_r2r(mm7,mm7);
	      for (out_line = 0; out_line < local_output_active_height; out_line++)
		{
		   in_line_offset = in_line[out_line] * local_padded_width;
		   switch(width_neighbors)
		     {
		      case 4:
			// csplinew dans mm0
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			break;
		      case 6:
			// csplinew dans mm0 et mm1
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			movq_m2r(*mmx_cubic,mm1);
			break;
		      case 8:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			// csplinew dans mm0 et mm1
			break;
		      case 10:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			movq_m2r(*mmx_cubic,mm2);
			// csplinew dans mm0, mm1 & mm2
			break;
		      case 12:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			// csplinew dans mm0, mm1 & mm2
			break;
		      case 14:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			movq_m2r(*mmx_cubic,mm3);
			// csplinew dans mm0, mm1, mm2 & mm3
			break;
		      case 16:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			mmx_cubic[2]=*(csplinew[14]=cspline_w_neighbors[14]);mmx_cubic[3]=*(csplinew[15]=cspline_w_neighbors[15]);
			movq_m2r(*mmx_cubic,mm3);
			// csplinew dans mm0, mm1, mm2 & mm3
			break;
		      default:
			mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			break;
		     }
		   // csplinew dans mm0, mm1, mm2 & mm3
		   for (out_col = 0; out_col < local_output_active_width; out_col++)
		     {
			line = padded_input + in_col[out_col] + in_line_offset;
			switch(width_neighbors)
			  {
			   case 4:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);      movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     break;
			   case 6:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);      movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);    punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);       movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 8:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   case 10:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 12:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   case 14:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+12),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm3,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 16:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+12),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm3,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   default:
			     mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			     break;
			  }
			// Mise à jour des valeurs de csplinew
			switch(width_neighbors)
			  {
			   case 4:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     break;
			   case 6:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     movq_m2r(*mmx_cubic,mm1);
			     break;
			   case 8:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     break;
			   case 10:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     movq_m2r(*mmx_cubic,mm2);
			     break;
			   case 12:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     break;
			   case 14:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     mmx_cubic[0]=*(++csplinew[12]);     mmx_cubic[1]=*(++csplinew[13]);
			     movq_m2r(*mmx_cubic,mm3);
			     break;
			   case 16:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     mmx_cubic[0]=*(++csplinew[12]);     mmx_cubic[1]=*(++csplinew[13]);
			     mmx_cubic[2]=*(++csplinew[14]);     mmx_cubic[3]=*(++csplinew[15]);
			     movq_m2r(*mmx_cubic,mm3);
			     break;
			   default:
			     mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			     break;
			  }
			if (value1 < 0)
			  *(output_p++) = 0;
			else 
			  {
			     value =
			       (value1 +
				(1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
			     
			     if (value > 255) 
			       *(output_p++) = 255;
			     else
			       *(output_p++) = (uint8_t) value;
			  }
		     }
		   // a line on output is now finished. We jump to the beginning of the next line
		   output_p+=output_offset;
		}
	   }
	 else
#endif
	   {
	      for (out_line = 0; out_line < local_output_active_height; out_line++)
		{
		   for (w=0;w<width_neighbors;w++)
		     csplinew[w]=cspline_w_neighbors[w];
		   in_line_offset = in_line[out_line] * local_padded_width;
		   for (out_col = 0; out_col < local_output_active_width; out_col++)
		     {
			line = padded_input + in_col[out_col] + in_line_offset;
			value1=0;
			for (w=0;w<width_neighbors;w++) 
			  {
			     value1+=*(line++)*(*csplinew[w]);
			  }
			if (value1 < 0)
			  *(output_p++) = 0;
			else 
			  {
			     value =
			       (value1 +
				(1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
			     
			     if (value > 255) 
			       *(output_p++) = 255;
			     else
			       *(output_p++) = (uint8_t) value;
			  }
			for (w=0;w<width_neighbors;w++)
			  csplinew[w]++;
		     }
		   // a line on output is now finished. We jump to the beginning of the next line
		   output_p+=output_offset;
		}
	   }
      }
      break;

   

  case 5:
      // We only downscale on height, not width
      // MMX version doesn't go faster than NOMMX version
	{
/*	   
#ifdef HAVE_ASM_MMX
	   if (mmx==1) 
	     {
		emms();
		switch(height_neighbors)
		  {
		   case 4:
		     // csplineh dans mm0
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     break;
		   case 6:
		     // csplineh dans mm0 et mm1
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     mmx_cubic[0]=*(csplineh[4]=cspline_h_neighbors[4]);
		     mmx_cubic[1]=*(csplineh[5]=cspline_h_neighbors[5]);
		     movq_m2r(*mmx_cubic,mm1);
		     break;
		   case 8:
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     mmx_cubic[0]=*(csplineh[4]=cspline_h_neighbors[4]);
		     mmx_cubic[1]=*(csplineh[5]=cspline_h_neighbors[5]);
		     mmx_cubic[2]=*(csplineh[6]=cspline_h_neighbors[6]);
		     mmx_cubic[3]=*(csplineh[7]=cspline_h_neighbors[7]);
		     movq_m2r(*mmx_cubic,mm1);
		     // csplineh dans mm0 et mm1
		     break;
		   case 10:
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     mmx_cubic[0]=*(csplineh[4]=cspline_h_neighbors[4]);
		     mmx_cubic[1]=*(csplineh[5]=cspline_h_neighbors[5]);
		     mmx_cubic[2]=*(csplineh[6]=cspline_h_neighbors[6]);
		     mmx_cubic[3]=*(csplineh[7]=cspline_h_neighbors[7]);
		     movq_m2r(*mmx_cubic,mm1);
		     mmx_cubic[0]=*(csplineh[8]=cspline_h_neighbors[8]);
		     mmx_cubic[1]=*(csplineh[9]=cspline_h_neighbors[9]);
		     movq_m2r(*mmx_cubic,mm2);
		     // csplineh dans mm0, mm1 & mm2
		     break;
		   case 12:
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     mmx_cubic[0]=*(csplineh[4]=cspline_h_neighbors[4]);
		     mmx_cubic[1]=*(csplineh[5]=cspline_h_neighbors[5]);
		     mmx_cubic[2]=*(csplineh[6]=cspline_h_neighbors[6]);
		     mmx_cubic[3]=*(csplineh[7]=cspline_h_neighbors[7]);
		     movq_m2r(*mmx_cubic,mm1);
		     mmx_cubic[0]=*(csplineh[8]=cspline_h_neighbors[8]);
		     mmx_cubic[1]=*(csplineh[9]=cspline_h_neighbors[9]);
		     mmx_cubic[2]=*(csplineh[10]=cspline_h_neighbors[10]);
		     mmx_cubic[3]=*(csplineh[11]=cspline_h_neighbors[11]);
		     movq_m2r(*mmx_cubic,mm2);
		     // csplineh dans mm0, mm1 & mm2
		     break;
		   case 14:
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     mmx_cubic[0]=*(csplineh[4]=cspline_h_neighbors[4]);
		     mmx_cubic[1]=*(csplineh[5]=cspline_h_neighbors[5]);
		     mmx_cubic[2]=*(csplineh[6]=cspline_h_neighbors[6]);
		     mmx_cubic[3]=*(csplineh[7]=cspline_h_neighbors[7]);
		     movq_m2r(*mmx_cubic,mm1);
		     mmx_cubic[0]=*(csplineh[8]=cspline_h_neighbors[8]);
		     mmx_cubic[1]=*(csplineh[9]=cspline_h_neighbors[9]);
		     mmx_cubic[2]=*(csplineh[10]=cspline_h_neighbors[10]);
		     mmx_cubic[3]=*(csplineh[11]=cspline_h_neighbors[11]);
		     movq_m2r(*mmx_cubic,mm2);
		     mmx_cubic[0]=*(csplineh[12]=cspline_h_neighbors[12]);
		     mmx_cubic[1]=*(csplineh[13]=cspline_h_neighbors[13]);
		     movq_m2r(*mmx_cubic,mm3);
		     // csplineh dans mm0, mm1, mm2 & mm3
		     break;
		   case 16:
		     mmx_cubic[0]=*(csplineh[0]=cspline_h_neighbors[0]);
		     mmx_cubic[1]=*(csplineh[1]=cspline_h_neighbors[1]);
		     mmx_cubic[2]=*(csplineh[2]=cspline_h_neighbors[2]);
		     mmx_cubic[3]=*(csplineh[3]=cspline_h_neighbors[3]);
		     movq_m2r(*mmx_cubic,mm0);
		     mmx_cubic[0]=*(csplineh[4]=cspline_h_neighbors[4]);
		     mmx_cubic[1]=*(csplineh[5]=cspline_h_neighbors[5]);
		     mmx_cubic[2]=*(csplineh[6]=cspline_h_neighbors[6]);
		     mmx_cubic[3]=*(csplineh[7]=cspline_h_neighbors[7]);
		     movq_m2r(*mmx_cubic,mm1);
		     mmx_cubic[0]=*(csplineh[8]=cspline_h_neighbors[8]);
		     mmx_cubic[1]=*(csplineh[9]=cspline_h_neighbors[9]);
		     mmx_cubic[2]=*(csplineh[10]=cspline_h_neighbors[10]);
		     mmx_cubic[3]=*(csplineh[11]=cspline_h_neighbors[11]);
		     movq_m2r(*mmx_cubic,mm2);
		     mmx_cubic[0]=*(csplineh[12]=cspline_h_neighbors[12]);
		     mmx_cubic[1]=*(csplineh[13]=cspline_h_neighbors[13]);
		     mmx_cubic[2]=*(csplineh[14]=cspline_h_neighbors[14]);
		     mmx_cubic[3]=*(csplineh[15]=cspline_h_neighbors[15]);
		     movq_m2r(*mmx_cubic,mm3);
		     // csplineh dans mm0, mm1, mm2 & mm3
		     break;
		   default:
		     mjpeg_warn("height neighbors = %d, is not supported inside cubic-scale function",height_neighbors);
		     break;
		  }
		//	 for (h=0;h<height_neighbors;h++)
		//	   csplineh[h]=cspline_h_neighbors[h];
		for (out_line = 0; out_line < local_output_active_height; out_line++)
		  {
		     in_line_offset = in_line[out_line] * local_padded_width;
		     for (out_col = 0; out_col < local_output_active_width; out_col++)
		       {
 
			  line_start[0]=padded_input + in_col[out_col] + in_line_offset;
			  for (h=1;h<height_neighbors;h++)
			    line_start[h]=line_start[h-1]+local_padded_width;
			  switch(height_neighbors)
			    {
			     case 4:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
			       break;
			     case 6:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[4]);
                               mmx_padded[1]=*(line_start[5]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm1,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0];
			       break;
			     case 8:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[4]);
                               mmx_padded[1]=*(line_start[5]);
                               mmx_padded[2]=*(line_start[6]);
                               mmx_padded[3]=*(line_start[7]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm1,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
			       break;
			     case 10:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[4]);
                               mmx_padded[1]=*(line_start[5]);
                               mmx_padded[2]=*(line_start[6]);
                               mmx_padded[3]=*(line_start[7]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm1,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[8]);
                               mmx_padded[1]=*(line_start[9]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm2,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0];
			       break;
			     case 12:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[4]);
                               mmx_padded[1]=*(line_start[5]);
                               mmx_padded[2]=*(line_start[6]);
                               mmx_padded[3]=*(line_start[7]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm1,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[8]);
                               mmx_padded[1]=*(line_start[9]);
                               mmx_padded[2]=*(line_start[10]);
                               mmx_padded[3]=*(line_start[11]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm2,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
			       break;
			     case 14:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[4]);
                               mmx_padded[1]=*(line_start[5]);
                               mmx_padded[2]=*(line_start[6]);
                               mmx_padded[3]=*(line_start[7]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm1,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[8]);
                               mmx_padded[1]=*(line_start[9]);
                               mmx_padded[2]=*(line_start[10]);
                               mmx_padded[3]=*(line_start[11]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm2,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[12]);
                               mmx_padded[1]=*(line_start[13]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm3,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0];
			       break;
			     case 16:
                               mmx_padded[0]=*(line_start[0]);
                               mmx_padded[1]=*(line_start[1]);
                               mmx_padded[2]=*(line_start[2]);
                               mmx_padded[3]=*(line_start[3]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm0,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[4]);
                               mmx_padded[1]=*(line_start[5]);
                               mmx_padded[2]=*(line_start[6]);
                               mmx_padded[3]=*(line_start[7]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm1,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[8]);
                               mmx_padded[1]=*(line_start[9]);
                               mmx_padded[2]=*(line_start[10]);
                               mmx_padded[3]=*(line_start[11]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm2,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
                               mmx_padded[0]=*(line_start[12]);
                               mmx_padded[1]=*(line_start[13]);
                               mmx_padded[2]=*(line_start[14]);
                               mmx_padded[3]=*(line_start[15]);
			       movq_m2r(*mmx_padded,mm7);
			       pmaddwd_r2r(mm3,mm7);
			       movq_r2m(mm7,*(mmx_t *)mmx_res);
			       value1+=mmx_res[0]+mmx_res[1];
			       break;
			     default:
			       mjpeg_warn("height neighbors = %d, is not supported inside cubic-scale function",height_neighbors);
			       break;
			    }
			  if (value1 < 0)
			    *(output_p++) = 0;
			  else 
			    {
			       value =
				 (value1 +
				  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
			       
			       if (value > 255) 
				 *(output_p++) = 255;
			       else
				 *(output_p++) = (uint8_t) value;
			    }
		       }
		     // a line on output is now finished. We jump to the beginning of the next line
		     output_p+=output_offset;
		     // Mise à jour des valeurs de csplineh
		     switch(height_neighbors)
		       {
			case 4:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  break;
			case 6:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(++csplineh[4]);
			  mmx_cubic[1]=*(++csplineh[5]);
			  movq_m2r(*mmx_cubic,mm1);
			  break;
			case 8:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(++csplineh[4]);
			  mmx_cubic[1]=*(++csplineh[5]);
			  mmx_cubic[2]=*(++csplineh[6]);
			  mmx_cubic[3]=*(++csplineh[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  break;
			case 10:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(++csplineh[4]);
			  mmx_cubic[1]=*(++csplineh[5]);
			  mmx_cubic[2]=*(++csplineh[6]);
			  mmx_cubic[3]=*(++csplineh[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(++csplineh[8]);
			  mmx_cubic[1]=*(++csplineh[9]);
			  movq_m2r(*mmx_cubic,mm2);
			  break;
			case 12:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(++csplineh[4]);
			  mmx_cubic[1]=*(++csplineh[5]);
			  mmx_cubic[2]=*(++csplineh[6]);
			  mmx_cubic[3]=*(++csplineh[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(++csplineh[8]);
			  mmx_cubic[1]=*(++csplineh[9]);
			  mmx_cubic[2]=*(++csplineh[10]);
			  mmx_cubic[3]=*(++csplineh[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  break;
			case 14:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(++csplineh[4]);
			  mmx_cubic[1]=*(++csplineh[5]);
			  mmx_cubic[2]=*(++csplineh[6]);
			  mmx_cubic[3]=*(++csplineh[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(++csplineh[8]);
			  mmx_cubic[1]=*(++csplineh[9]);
			  mmx_cubic[2]=*(++csplineh[10]);
			  mmx_cubic[3]=*(++csplineh[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(++csplineh[12]);
			  mmx_cubic[1]=*(++csplineh[13]);
			  movq_m2r(*mmx_cubic,mm3);
			  break;
			case 16:
			  mmx_cubic[0]=*(++csplineh[0]);
			  mmx_cubic[1]=*(++csplineh[1]);
			  mmx_cubic[2]=*(++csplineh[2]);
			  mmx_cubic[3]=*(++csplineh[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(++csplineh[4]);
			  mmx_cubic[1]=*(++csplineh[5]);
			  mmx_cubic[2]=*(++csplineh[6]);
			  mmx_cubic[3]=*(++csplineh[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(++csplineh[8]);
			  mmx_cubic[1]=*(++csplineh[9]);
			  mmx_cubic[2]=*(++csplineh[10]);
			  mmx_cubic[3]=*(++csplineh[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(++csplineh[12]);
			  mmx_cubic[1]=*(++csplineh[13]);
			  mmx_cubic[2]=*(++csplineh[14]);
			  mmx_cubic[3]=*(++csplineh[15]);
			  movq_m2r(*mmx_cubic,mm3);
			  break;
			default:
			  mjpeg_warn("height neighbors = %d, is not supported inside cubic-scale function",height_neighbors);
			  break;
		       }
		  }
	     }
	   else
#endif	 
*/
	     {
		for (h=0;h<height_neighbors;h++)
		  csplineh[h]=cspline_h_neighbors[h];
		for (out_line = 0; out_line < local_output_active_height; out_line++)
		  {
		     in_line_offset = in_line[out_line] * local_padded_width;
		     for (out_col = 0; out_col < local_output_active_width; out_col++)
		       {
			  line_start[0]=padded_input + in_col[out_col] + in_line_offset;
			  for (h=1;h<height_neighbors;h++)
			    line_start[h]=line_start[h-1]+local_padded_width;
			  
			  value1 = *(line_start[0])*(*csplineh[0]);
			  for (h=1;h<height_neighbors;h++)
			    value1+=(*(line_start[h]))*(*csplineh[h]);
			  
			  if (value1 < 0)
			    *(output_p++) = 0;
			  else 
			    {
			       value =
				 (value1 +
				  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
			       
			       if (value > 255) 
				 *(output_p++) = 255;
			       else
				 *(output_p++) = (uint8_t) value;
			    }
			  //		  for (w=0;w<width_neighbors;w++)
			  //		    csplinew[w]++;
		       }
		     // a line on output is now finished. We jump to the beginning of the next line
		     output_p+=output_offset;
		     for (h=0;h<height_neighbors;h++)
		       csplineh[h]++;
		  }
	     }
	}
      break;
   }

   /* *INDENT-ON* */
//   mjpeg_debug ("End of cubic_scale");
   return (0);
}

// *************************************************************************************


// *************************************************************************************
int
cubic_scale_interlaced (uint8_t * padded_top, uint8_t * padded_bottom, uint8_t * output, 
			unsigned int *in_col, unsigned int *in_line,
			int16_t ** cspline_w_neighbors,uint16_t  width_neighbors,
			int16_t ** cspline_h_neighbors,uint16_t height_neighbors,
			unsigned int half)
{
  // Warning: because cubic-spline values may be <0 or >1.0, a range test on value is mandatory
  unsigned int local_output_active_width = output_active_width >> half;
  unsigned int local_output_active_height = output_active_height >> half;
  unsigned int local_output_width = output_width >> half;
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_padded_width = local_input_useful_width + width_neighbors -1;
  unsigned int out_line, out_col,w,h;
  unsigned long int in_line_offset;
  int16_t output_offset;

  uint8_t *output_p,*line,*line_start[height_neighbors];
  int16_t *csplinew[width_neighbors],*csplineh[height_neighbors];
  long int value=0,value1=0,value2=0;

   output_p = output ;
   output_offset = local_output_width-local_output_active_width;

   /* *INDENT-OFF* */

  switch(specific) {
  case 0: 
      {
#ifdef HAVE_ASM_MMX
	 if (mmx==1)
	   {
		emms();
		// fill mm7 with zeros
		pxor_r2r(mm7,mm7);
		// MMX ROUTINE
		for (h=0;h<height_neighbors;h++)
		  csplineh[h]=cspline_h_neighbors[h];
		for (out_line = 0; out_line < (local_output_active_height>>1); out_line++)
		  {
		     in_line_offset = in_line[out_line] * local_padded_width;
		     // Top line
		     switch(width_neighbors)
		       {
			case 4:
			  // csplinew dans mm0
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  break;
			case 6:
			  // csplinew dans mm0 et mm1
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  movq_m2r(*mmx_cubic,mm1);
			  break;
			case 8:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  // csplinew dans mm0 et mm1
			  break;
			case 10:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  movq_m2r(*mmx_cubic,mm2);
			  // csplinew dans mm0, mm1 & mm2
			  break;
			case 12:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  // csplinew dans mm0, mm1 & mm2
			  break;
			case 14:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			  movq_m2r(*mmx_cubic,mm3);
			  // csplinew dans mm0, mm1, mm2 & mm3
			  break;
			case 16:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			  mmx_cubic[2]=*(csplinew[14]=cspline_w_neighbors[14]);mmx_cubic[3]=*(csplinew[15]=cspline_w_neighbors[15]);
			  movq_m2r(*mmx_cubic,mm3);
			  // csplinew dans mm0, mm1, mm2 & mm3
			  break;
			default:
			  mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			  break;
		       }
		     for (out_col = 0; out_col < local_output_active_width; out_col++)
		       {
			  line_start[0]=padded_top + in_col[out_col] + in_line_offset;
			  for (h=1;h<height_neighbors;h++) 
			    line_start[h]=line_start[h-1]+local_padded_width;
			  value1=0;
			  for (h=0;h<height_neighbors;h++) 
			    {
			       switch(width_neighbors)
				 {
				  case 4:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value1+=(mmx_res[0]+mmx_res[1])*(*csplineh[h]);
				    break;
				  case 6:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 8:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 10:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 12:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 14:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+12),mm6); punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm3,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 16:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+12),mm6); punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm3,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  default:
				    mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
				    break;
				 }
			    }
			  // Mise à jour des valeurs de csplinew
			  switch(width_neighbors)
			    {
			     case 4:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       break;
			     case 6:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       movq_m2r(*mmx_cubic,mm1);
			       break;
			     case 8:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       break;
			     case 10:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       movq_m2r(*mmx_cubic,mm2);
			       break;
			     case 12:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       break;
			     case 14:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       mmx_cubic[0]=*(++csplinew[12]);  mmx_cubic[1]=*(++csplinew[13]);
			       movq_m2r(*mmx_cubic,mm3);
			       break;
			     case 16:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       mmx_cubic[0]=*(++csplinew[12]);  mmx_cubic[1]=*(++csplinew[13]);
			       mmx_cubic[2]=*(++csplinew[14]);  mmx_cubic[3]=*(++csplinew[15]);
			       movq_m2r(*mmx_cubic,mm3);
			       break;
			     default:
			       mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			       break;
			    }
			  if (value1 < 0) 
			    *(output_p++) = 0;
			  else 
			    {
			       value =
				 (value1 +
				  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
			       
			       if (value > 255) 
				 *(output_p++) = 255;
			       else
				 *(output_p++) = (uint8_t) value;
			    }
		       }
		     // a top line on output is now finished. We jump to the beginning of the next bottom line
		     output_p+=output_offset;
		     // Bottom line
		     switch(width_neighbors)
		       {
			case 4:
			  // csplinew dans mm0
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  break;
			case 6:
			  // csplinew dans mm0 et mm1
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  movq_m2r(*mmx_cubic,mm1);
			  break;
			case 8:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  // csplinew dans mm0 et mm1
			  break;
			case 10:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  movq_m2r(*mmx_cubic,mm2);
			  // csplinew dans mm0, mm1 & mm2
			  break;
			case 12:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  // csplinew dans mm0, mm1 & mm2
			  break;
			case 14:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			  movq_m2r(*mmx_cubic,mm3);
			  // csplinew dans mm0, mm1, mm2 & mm3
			  break;
			case 16:
			  mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			  mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			  movq_m2r(*mmx_cubic,mm0);
			  mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			  mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			  movq_m2r(*mmx_cubic,mm1);
			  mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			  mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			  movq_m2r(*mmx_cubic,mm2);
			  mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			  mmx_cubic[2]=*(csplinew[14]=cspline_w_neighbors[14]);mmx_cubic[3]=*(csplinew[15]=cspline_w_neighbors[15]);
			  movq_m2r(*mmx_cubic,mm3);
			  // csplinew dans mm0, mm1, mm2 & mm3
			  break;
			default:
			  mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			  break;
		       }
		     for (out_col = 0; out_col < local_output_active_width; out_col++)
		       {
			  line_start[0]=padded_bottom + in_col[out_col] + in_line_offset;
			  for (h=1;h<height_neighbors;h++) 
			    line_start[h]=line_start[h-1]+local_padded_width;
			  value1=0;
			  for (h=0;h<height_neighbors;h++) 
			    {
			       switch(width_neighbors)
				 {
				  case 4:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value1+=(mmx_res[0]+mmx_res[1])*(*csplineh[h]);
				    break;
				  case 6:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 8:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 10:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 12:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 14:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+12),mm6); punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm3,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0];
				    value1+=value2*(*csplineh[h]);
				    break;
				  case 16:
				    movq_m2r(*(line_start[h]),mm6);    punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm0,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+4),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm1,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+8),mm6);  punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm2,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    movq_m2r(*(line_start[h]+12),mm6); punpcklbw_r2r(mm7,mm6);
				    pmaddwd_r2r(mm3,mm6);              movq_r2m(mm6,*(mmx_t *)mmx_res);
				    value2+=mmx_res[0]+mmx_res[1];
				    value1+=value2*(*csplineh[h]);
				    break;
				  default:
				    mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
				    break;
				 }
			    }
			  // Mise à jour des valeurs de csplinew
			  switch(width_neighbors)
			    {
			     case 4:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       break;
			     case 6:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       movq_m2r(*mmx_cubic,mm1);
			       break;
			     case 8:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       break;
			     case 10:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       movq_m2r(*mmx_cubic,mm2);
			       break;
			     case 12:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       break;
			     case 14:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       mmx_cubic[0]=*(++csplinew[12]);  mmx_cubic[1]=*(++csplinew[13]);
			       movq_m2r(*mmx_cubic,mm3);
			       break;
			     case 16:
			       mmx_cubic[0]=*(++csplinew[0]);   mmx_cubic[1]=*(++csplinew[1]);
			       mmx_cubic[2]=*(++csplinew[2]);   mmx_cubic[3]=*(++csplinew[3]);
			       movq_m2r(*mmx_cubic,mm0);
			       mmx_cubic[0]=*(++csplinew[4]);   mmx_cubic[1]=*(++csplinew[5]);
			       mmx_cubic[2]=*(++csplinew[6]);   mmx_cubic[3]=*(++csplinew[7]);
			       movq_m2r(*mmx_cubic,mm1);
			       mmx_cubic[0]=*(++csplinew[8]);   mmx_cubic[1]=*(++csplinew[9]);
			       mmx_cubic[2]=*(++csplinew[10]);  mmx_cubic[3]=*(++csplinew[11]);
			       movq_m2r(*mmx_cubic,mm2);
			       mmx_cubic[0]=*(++csplinew[12]);  mmx_cubic[1]=*(++csplinew[13]);
			       mmx_cubic[2]=*(++csplinew[14]);  mmx_cubic[3]=*(++csplinew[15]);
			       movq_m2r(*mmx_cubic,mm3);
			       break;
			     default:
			       mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			       break;
			    }
			  if (value1 < 0) 
			    *(output_p++) = 0;
			  else 
			    {
			       value =
				 (value1 +
				  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
			       
			       if (value > 255) 
				 *(output_p++) = 255;
			       else
				 *(output_p++) = (uint8_t) value;
			    }
		       }
		     // a bottom line on output is now finished. We jump to the beginning of the next top line
		     output_p+=output_offset;
		     for (h=0;h<height_neighbors;h++)
		       csplineh[h]++;
		  }
	   }
	 else
#endif	   
	   {
	      // NON MMX treatment
	 for (h=0;h<height_neighbors;h++)
	   csplineh[h]=cspline_h_neighbors[h];
	 for (out_line = 0; out_line < (local_output_active_height>>1); out_line++)
	   {
	      in_line_offset = in_line[out_line] * local_padded_width;
	      // Top line
	      for (w=0;w<width_neighbors;w++)
		csplinew[w]=cspline_w_neighbors[w];
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line_start[0]=padded_top + in_col[out_col] + in_line_offset;
		  for (h=1;h<height_neighbors;h++)
		    line_start[h]=line_start[h-1]+local_padded_width;
		  value1=0;
		  for (w=0;w<width_neighbors;w++) 
		    {
		       value2=0;
		       for (h=0;h<height_neighbors;h++)
			 value2+=(*(line_start[h]++))*(*csplineh[h]);
		       value1+=value2*(*csplinew[w]);
		    }
		  if (value1 < 0)
		    *(output_p++) = 0;
		  else 
		    {
		       value =
			 (value1 +
			  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		       
		       if (value > 255) 
			 *(output_p++) = 255;
		       else
			 *(output_p++) = (uint8_t) value;
		    }
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
	       }
	      // a top line on output is now finished. We jump to the beginning of the next bottom line
	      output_p+=output_offset;
	      // Bottom line
	      for (w=0;w<width_neighbors;w++)
		csplinew[w]=cspline_w_neighbors[w];
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line_start[0]=padded_bottom + in_col[out_col] + in_line_offset;
		  for (h=1;h<height_neighbors;h++)
		    line_start[h]=line_start[h-1]+local_padded_width;
		  value1=0;
		  for (w=0;w<width_neighbors;w++) 
		    {
		       value2=0;
		       for (h=0;h<height_neighbors;h++)
			 value2+=(*(line_start[h]++))*(*csplineh[h]);
		       value1+=value2*(*csplinew[w]);
		    }
		  if (value1 < 0)
		    *(output_p++) = 0;
		  else 
		    {
		       value =
			 (value1 +
			  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		       
		       if (value > 255) 
			 *(output_p++) = 255;
		       else
			 *(output_p++) = (uint8_t) value;
		    }
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
	       }
             // a bottom line on output is now finished. We jump to the beginning of the next top line
	      output_p+=output_offset;
	      for (h=0;h<height_neighbors;h++)
		csplineh[h]++;
	   }
      }
      }
    break;
    
  case 1:
    // We only downscale on width, not height
      {
#ifdef HAVE_ASM_MMX
	   if (mmx==1) 
	   {
	      emms();
	      // fill mm7 with zeros
	      pxor_r2r(mm7,mm7);
	      for (out_line = 0; out_line < (local_output_active_height>>1); out_line++)
		{
		   in_line_offset = in_line[out_line] * local_padded_width;
		   switch(width_neighbors)
		     {
		      case 4:
			// csplinew dans mm0
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			break;
		      case 6:
			// csplinew dans mm0 et mm1
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			movq_m2r(*mmx_cubic,mm1);
			break;
		      case 8:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			// csplinew dans mm0 et mm1
			break;
		      case 10:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			movq_m2r(*mmx_cubic,mm2);
			// csplinew dans mm0, mm1 & mm2
			break;
		      case 12:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			// csplinew dans mm0, mm1 & mm2
			break;
		      case 14:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			movq_m2r(*mmx_cubic,mm3);
			// csplinew dans mm0, mm1, mm2 & mm3
			break;
		      case 16:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			mmx_cubic[2]=*(csplinew[14]=cspline_w_neighbors[14]);mmx_cubic[3]=*(csplinew[15]=cspline_w_neighbors[15]);
			movq_m2r(*mmx_cubic,mm3);
			// csplinew dans mm0, mm1, mm2 & mm3
			break;
		      default:
			mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			break;
		     }
		   // csplinew dans mm0, mm1, mm2 & mm3
		   for (out_col = 0; out_col < local_output_active_width; out_col++)
		     {
			line = padded_top + in_col[out_col] + in_line_offset;
			// Top line
			switch(width_neighbors)
			  {
			   case 4:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);      movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     break;
			   case 6:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);      movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);    punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);       movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 8:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   case 10:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 12:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   case 14:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+12),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm3,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 16:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+12),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm3,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   default:
			     mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			     break;
			  }
			// Mise à jour des valeurs de csplinew
			switch(width_neighbors)
			  {
			   case 4:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     break;
			   case 6:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     movq_m2r(*mmx_cubic,mm1);
			     break;
			   case 8:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     break;
			   case 10:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     movq_m2r(*mmx_cubic,mm2);
			     break;
			   case 12:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     break;
			   case 14:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     mmx_cubic[0]=*(++csplinew[12]);     mmx_cubic[1]=*(++csplinew[13]);
			     movq_m2r(*mmx_cubic,mm3);
			     break;
			   case 16:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     mmx_cubic[0]=*(++csplinew[12]);     mmx_cubic[1]=*(++csplinew[13]);
			     mmx_cubic[2]=*(++csplinew[14]);     mmx_cubic[3]=*(++csplinew[15]);
			     movq_m2r(*mmx_cubic,mm3);
			     break;
			   default:
			     mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			     break;
			  }
			if (value1 < 0)
			  *(output_p++) = 0;
			else 
			  {
			     value =
			       (value1 +
				(1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
			     
			     if (value > 255) 
			       *(output_p++) = 255;
			     else
			       *(output_p++) = (uint8_t) value;
			  }
		     }
		   // a top line on output is now finished. We jump to the beginning of the next bottom line
		   output_p+=output_offset;
		   // Bottom line
		   switch(width_neighbors)
		     {
		      case 4:
			// csplinew dans mm0
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			break;
		      case 6:
			// csplinew dans mm0 et mm1
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			movq_m2r(*mmx_cubic,mm1);
			break;
		      case 8:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			// csplinew dans mm0 et mm1
			break;
		      case 10:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			movq_m2r(*mmx_cubic,mm2);
			// csplinew dans mm0, mm1 & mm2
			break;
		      case 12:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			// csplinew dans mm0, mm1 & mm2
			break;
		      case 14:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			movq_m2r(*mmx_cubic,mm3);
			// csplinew dans mm0, mm1, mm2 & mm3
			break;
		      case 16:
			mmx_cubic[0]=*(csplinew[0]=cspline_w_neighbors[0]);  mmx_cubic[1]=*(csplinew[1]=cspline_w_neighbors[1]);
			mmx_cubic[2]=*(csplinew[2]=cspline_w_neighbors[2]);  mmx_cubic[3]=*(csplinew[3]=cspline_w_neighbors[3]);
			movq_m2r(*mmx_cubic,mm0);
			mmx_cubic[0]=*(csplinew[4]=cspline_w_neighbors[4]);  mmx_cubic[1]=*(csplinew[5]=cspline_w_neighbors[5]);
			mmx_cubic[2]=*(csplinew[6]=cspline_w_neighbors[6]);  mmx_cubic[3]=*(csplinew[7]=cspline_w_neighbors[7]);
			movq_m2r(*mmx_cubic,mm1);
			mmx_cubic[0]=*(csplinew[8]=cspline_w_neighbors[8]);  mmx_cubic[1]=*(csplinew[9]=cspline_w_neighbors[9]);
			mmx_cubic[2]=*(csplinew[10]=cspline_w_neighbors[10]);mmx_cubic[3]=*(csplinew[11]=cspline_w_neighbors[11]);
			movq_m2r(*mmx_cubic,mm2);
			mmx_cubic[0]=*(csplinew[12]=cspline_w_neighbors[12]);mmx_cubic[1]=*(csplinew[13]=cspline_w_neighbors[13]);
			mmx_cubic[2]=*(csplinew[14]=cspline_w_neighbors[14]);mmx_cubic[3]=*(csplinew[15]=cspline_w_neighbors[15]);
			movq_m2r(*mmx_cubic,mm3);
			// csplinew dans mm0, mm1, mm2 & mm3
			break;
		      default:
			mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			break;
		     }
		   // csplinew dans mm0, mm1, mm2 & mm3
		   for (out_col = 0; out_col < local_output_active_width; out_col++)
		     {
			line = padded_bottom + in_col[out_col] + in_line_offset;
			switch(width_neighbors)
			  {
			   case 4:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);      movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     break;
			   case 6:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);      movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);    punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);       movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 8:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   case 10:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 12:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   case 14:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+12),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm3,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0];
			     break;
			   case 16:
			     movq_m2r(*(line),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm0,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+4),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm1,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+8),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm2,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     movq_m2r(*(line+12),mm6);     punpcklbw_r2r(mm7,mm6);
			     pmaddwd_r2r(mm3,mm6);     movq_r2m(mm6,*(mmx_t *)mmx_res);
			     value1+=mmx_res[0]+mmx_res[1];
			     break;
			   default:
			     mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			     break;
			  }
			// Mise à jour des valeurs de csplinew
			switch(width_neighbors)
			  {
			   case 4:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     break;
			   case 6:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     movq_m2r(*mmx_cubic,mm1);
			     break;
			   case 8:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     break;
			   case 10:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     movq_m2r(*mmx_cubic,mm2);
			     break;
			   case 12:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     break;
			   case 14:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     mmx_cubic[0]=*(++csplinew[12]);     mmx_cubic[1]=*(++csplinew[13]);
			     movq_m2r(*mmx_cubic,mm3);
			     break;
			   case 16:
			     mmx_cubic[0]=*(++csplinew[0]);     mmx_cubic[1]=*(++csplinew[1]);
			     mmx_cubic[2]=*(++csplinew[2]);     mmx_cubic[3]=*(++csplinew[3]);
			     movq_m2r(*mmx_cubic,mm0);
			     mmx_cubic[0]=*(++csplinew[4]);     mmx_cubic[1]=*(++csplinew[5]);
			     mmx_cubic[2]=*(++csplinew[6]);     mmx_cubic[3]=*(++csplinew[7]);
			     movq_m2r(*mmx_cubic,mm1);
			     mmx_cubic[0]=*(++csplinew[8]);     mmx_cubic[1]=*(++csplinew[9]);
			     mmx_cubic[2]=*(++csplinew[10]);     mmx_cubic[3]=*(++csplinew[11]);
			     movq_m2r(*mmx_cubic,mm2);
			     mmx_cubic[0]=*(++csplinew[12]);     mmx_cubic[1]=*(++csplinew[13]);
			     mmx_cubic[2]=*(++csplinew[14]);     mmx_cubic[3]=*(++csplinew[15]);
			     movq_m2r(*mmx_cubic,mm3);
			     break;
			   default:
			     mjpeg_warn("width neighbors = %d, is not supported inside cubic-scale function",width_neighbors);
			     break;
			  }
			if (value1 < 0)
			  *(output_p++) = 0;
			else 
			  {
			     value =
			       (value1 +
				(1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
			     
			     if (value > 255) 
			       *(output_p++) = 255;
			     else
			       *(output_p++) = (uint8_t) value;
			  }
		     }
		   // a bottom line on output is now finished. We jump to the beginning of the next top line
		   output_p+=output_offset;
		}
	   }
	 else
#endif
	   {
	 for (out_line = 0; out_line < (local_output_active_height>>1); out_line++)
	   {
	      in_line_offset = in_line[out_line] * local_padded_width;
	      // Top line
	      for (w=0;w<width_neighbors;w++)
		csplinew[w]=cspline_w_neighbors[w];
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line = padded_top + in_col[out_col] + in_line_offset;
		  value1=0;
		  for (w=0;w<width_neighbors;w++) 
		    {
		       value1+=*(line++)*(*csplinew[w]);
		    }
		  if (value1 < 0)
		    *(output_p++) = 0;
		  else 
		    {
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       
		       if (value > 255) 
			 *(output_p++) = 255;
		       else
			 *(output_p++) = (uint8_t) value;
		    }
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
	       }
	      // a line on output is now finished. We jump to the beginning of the next bottom line
	      output_p+=output_offset;
	      // Bottom line
	      for (w=0;w<width_neighbors;w++)
		csplinew[w]=cspline_w_neighbors[w];
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line = padded_bottom + in_col[out_col] + in_line_offset;
		  value1=0;
		  for (w=0;w<width_neighbors;w++) 
		    {
		       value1+=*(line++)*(*csplinew[w]);
		    }
		  if (value1 < 0)
		    *(output_p++) = 0;
		  else 
		    {
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       
		       if (value > 255) 
			 *(output_p++) = 255;
		       else
			 *(output_p++) = (uint8_t) value;
		    }
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
	       }
	      // a line on output is now finished. We jump to the beginning of the next top line
	      output_p+=output_offset;
	   }
	   }
      }
    break;

   

  case 5:
    // We only downscale on height, not width
    // MMX treatment doesn't go much faster than non-mmx one => keep it simple !
      {
	 for (h=0;h<height_neighbors;h++)
	   csplineh[h]=cspline_h_neighbors[h];
	 for (out_line = 0; out_line < (local_output_active_height>>1); out_line++)
	   {
	      in_line_offset = in_line[out_line] * local_padded_width;
	      // Top line
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line_start[0]=padded_top + in_col[out_col] + in_line_offset;
		  for (h=1;h<height_neighbors;h++)
		    line_start[h]=line_start[h-1]+local_padded_width;
		  value1 = *(line_start[0])*(*csplineh[0]);
		  for (h=1;h<height_neighbors;h++)
		    value1+=(*(line_start[h]))*(*csplineh[h]);

		  if (value1 < 0)
		    *(output_p++) = 0;
		  else 
		    {
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       
		       if (value > 255) 
			 *(output_p++) = 255;
		       else
			 *(output_p++) = (uint8_t) value;
		    }
	       }
	      // a line on output is now finished. We jump to the beginning of the next bottom line
	      output_p+=output_offset;
	      // Bottom line
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line_start[0]=padded_bottom + in_col[out_col] + in_line_offset;
		  for (h=1;h<height_neighbors;h++)
		    line_start[h]=line_start[h-1]+local_padded_width;
		  value1 = *(line_start[0])*(*csplineh[0]);
		  for (h=1;h<height_neighbors;h++)
		    value1+=(*(line_start[h]))*(*csplineh[h]);

		  if (value1 < 0)
		    *(output_p++) = 0;
		  else 
		    {
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       
		       if (value > 255) 
			 *(output_p++) = 255;
		       else
			 *(output_p++) = (uint8_t) value;
		    }
	       }
             // a bottom line on output is now finished. We jump to the beginning of the next top line
	      output_p+=output_offset;
	      for (h=0;h<height_neighbors;h++)
		csplineh[h]++;
	   }
      }
    break;
 }
   /* *INDENT-ON* */
   // mjpeg_debug ("End of cubic_scale_interlaced");
   return (0);
}

// *************************************************************************************
// 

// *************************************************************************************
int16_t
cubic_spline (float x, unsigned int multiplicative)
{
  // Implementation of the Mitchell-Netravalli cubic spline, with recommended parameters B and C
  // [after Reconstruction filters in Computer Graphics by P. Mitchel and N. Netravali : Computer Graphics, Volume 22, Number 4, pp 221-228]
  // Normally, coefficiants are float, but they are transformed into integer with 1/FLOAT2INTEGER = 1/2"11 precision for speed reasons.
  // Please note that these coefficient may over and under shoot in the sense that they may be <0.0 and >1.0
  // Given out values of B and C, maximum value is (x=0) 8/9 and undeshoot is bigger than -0.04 (x#1.5)
  const float B = 1.0 / 3.0;
  const float C = 1.0 / 3.0;


  if (fabs (x) < 1)
    return ((int16_t)
	    floor (0.5 +
		   (((12.0 - 9.0 * B - 6.0 * C)  * fabs (x) * fabs (x) * fabs (x) 
		  + (-18.0 + 12.0 * B + 6.0 * C) * fabs (x) * fabs (x) 
		  + (6.0 - 2.0 * B)) / 6.0
		    ) * multiplicative));
  if (fabs (x) <= 2)
    return ((int16_t)
	    floor (0.5 +
		   (((-B - 6.0 * C) * fabs (x) * fabs (x) * fabs (x) +
		     (6.0 * B + 30.0 * C) * fabs (x) * fabs (x) +
		     (-12.0 * B - 48.0 * C) * fabs (x) + (8.0 * B +
							  24.0 * C)) /
		    6.0) * multiplicative));
  if (fabs (x) <= 3)
     return (0);
  mjpeg_info("In function cubic_spline: x=%f >3",x);
  return (0);
}

// *************************************************************************************


// *************************************************************************************
int
padding (uint8_t * padded_input, uint8_t * input, unsigned int half, 
	 uint16_t left_offset, uint16_t top_offset, uint16_t right_offset, uint16_t bottom_offset,
	 uint16_t width_pad)
{
  // In cubic interpolation, output pixel are evaluated from the 4*4 to 12*12 nearest neigbors. 
  // For border pixels, this requires that input datas along the edge to be padded. 
  // We choose to pad border pixel with black pixel, since border pixel along width are much of the time non-visible
  // (TV set for example) and along the height they are either non-visible or black borders are displayed 
  // This padding functions requires output_interlaced==0
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_input_useful_height = input_useful_height >> half;
  unsigned int local_padded_width = local_input_useful_width + width_pad;
  unsigned int local_input_width = input_width >> half;
  unsigned int line;
  uint8_t black,*uint8_pad,*uint8_inp;
  unsigned long int nb_top=top_offset*local_padded_width;

  // mjpeg_debug ("Start of padding, left_offset=%d,top_offset=%d,right_offset=%d,bottom_offset=%d,width_pad=%d",
//	       left_offset,top_offset,right_offset,bottom_offset,width_pad);
  if (half)
     black=128;
  else
     black=16;
  // PADDING
  // vertical offset of top_offset lines
  // Black pixel on the left_offset left pixels
  // Content Copy with left_offset pixels offset on the left and right_offset pixels of the right
  // Black pixel on the right_offset right pixels
  // vertical offset of the last bottom_offset lines
  
  memset(padded_input,black,nb_top);
  uint8_inp=input;
  uint8_pad=padded_input+nb_top;
  for (line = 0; line < local_input_useful_height; line++) 
     {
	memset(uint8_pad,black,left_offset);
	uint8_pad+=left_offset;
	memcpy (uint8_pad, uint8_inp, local_input_useful_width);
	uint8_pad+=local_input_useful_width;
	uint8_inp+=local_input_width; // it is local_input_width, not local_input_useful_width, see yuvscaler_implementation.txt
	memset(uint8_pad,black,right_offset);
	uint8_pad+=right_offset;
     }
  memset(uint8_pad,black,bottom_offset*local_padded_width);
  // mjpeg_debug ("End of padding");

  return (0);
}

// *************************************************************************************

// *************************************************************************************
int
padding_interlaced (uint8_t * padded_top, uint8_t * padded_bottom, uint8_t * input, unsigned int half,
		    uint16_t left_offset, uint16_t top_offset, uint16_t right_offset, uint16_t bottom_offset,
		    uint16_t width_pad)
{
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_input_useful_height = input_useful_height >> half;
  unsigned int local_padded_width = local_input_useful_width + width_pad;
  unsigned int local_input_width = input_width >> half;
  unsigned int line;
  uint8_t black, * uint8_ptop, * uint8_pbot, * uint8_inp;
  unsigned long int nb_top=top_offset*local_padded_width;
  unsigned long int nb_bot=bottom_offset*local_padded_width;

  // mjpeg_debug ("Start of padding_interlaced, left_offset=%d,top_offset=%d,right_offset=%d,bottom_offset=%d,width_pad=%d",
//	       left_offset,top_offset,right_offset,bottom_offset,width_pad);
  if (half)
     black=128;
  else
     black=16;
  // PADDING
  // vertical offset of top_offset lines
  // Black pixel on the left_offset left pixels
  // Content Copy with left_offset pixels offset on the left and right_offset pixels of the right
  // Black pixel on the right_offset right pixels
  // vertical offset of the last bottom_offset lines
  
  memset(padded_top,black,nb_top);
  memset(padded_bottom,black,nb_top);
  uint8_inp=input;
  uint8_ptop=padded_top+nb_top;
  uint8_pbot=padded_bottom+nb_top;
  for (line = 0; line < (local_input_useful_height >> 1); line++) 
     {
	memset(uint8_ptop,black,left_offset);
	uint8_ptop+=left_offset;
	memset(uint8_pbot,black,left_offset);
	uint8_pbot+=left_offset;
	memcpy (uint8_ptop, uint8_inp, local_input_useful_width);
	uint8_ptop+=local_input_useful_width;
	uint8_inp +=local_input_width; // it is local_input_width, not local_input_useful_width, see yuvscaler_implementation.txt
	memcpy (uint8_pbot, uint8_inp, local_input_useful_width);
	uint8_pbot+=local_input_useful_width;
	uint8_inp +=local_input_width; // it is local_input_width, not local_input_useful_width, see yuvscaler_implementation.txt
	memset(uint8_ptop,black,right_offset);
	uint8_ptop+=right_offset;
	memset(uint8_pbot,black,right_offset);
	uint8_pbot+=right_offset;
     }
  memset(uint8_ptop,black,nb_bot);
  memset(uint8_pbot,black,nb_bot);

  // mjpeg_debug ("End of padding_interlaced");
  return (0);

}

// *************************************************************************************

				  // THE FOLLOWING LINE "if (!mmx) mmx=0;" DOES NO USEFUL CALCULATION BUT
				  // it is necessary when gcc compilation with -O2 flag 
				  // to calculate correct values for value1+=(mmx_res[0]+mmx_res[1])*(*csplineh[h]);
				  // I know this sounds incredible, but believe me, it is true !
				  // On the other hand, using only gcc -O1, this line is no more necessary.
				  // And in both case, mmx_res[0],mmx_res[1] and *csplineh[h] have the same values
				  // Indeed, the corresponding machine code is totally different with or without
				  // this line with gcc -O2.
				  // The line value1+=(mmx_res[0]+mmx_res[1])*(*csplineh[h]) is compiled into
				  // (From DDD):
				  // Right calculation, that is including the "if (!mmx) mmx=0;" line
				  // --> 0x804f07f <cubic_scale+1615>:pmaddwd %mm0,%mm6
				  // --> 0x804f082 <cubic_scale+1618>:movq   %mm6,(%ecx)
				  //     0x804f085 <cubic_scale+1621>:mov    (%ecx),%edx
				  //     0x804f087 <cubic_scale+1623>:mov    0x4(%ecx),%eax
				  //     0x804f08a <cubic_scale+1626>:mov    0xffffffbc(%ebp),%ebx
				  //     0x804f08d <cubic_scale+1629>:add    %edx,%eax
				  //     0x804f08f <cubic_scale+1631>:mov    (%ebx,%edi,4),%edx
				  // --> 0x804f092 <cubic_scale+1634>:inc    %edi
				  // Wrong calculation, that is not including the "if (!mmx) mmx=0;" line
				  // --> 0x804f062 <cubic_scale+1586>:pmaddwd %mm0,%mm6
				  // --> 0x804f065 <cubic_scale+1589>:movq   %mm6,(%ecx)
				  //     0x804f068 <cubic_scale+1592>:mov    0xffffffbc(%ebp),%ebx
				  //     0x804f06b <cubic_scale+1595>:mov    (%ecx),%eax
				  //     0x804f06d <cubic_scale+1597>:mov    (%ebx,%edi,4),%edx
				  //     0x804f070 <cubic_scale+1600>:add    %esi,%eax
				  // --> 0x804f072 <cubic_scale+1602>:inc    %edi
				  
//				  if (!mmx)
//				    mmx=0;
