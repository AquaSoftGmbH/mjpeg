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
extern uint8_t *divide;
extern long int bicubic_offset;


// Defines
#define FLOAT2INTEGERPOWER 11
extern long int FLOAT2INTOFFSET;
#define bicubic_VERSION "17-01-2003"

// *************************************************************************************
int
cubic_scale (uint8_t * padded_input, uint8_t * output, 
	     unsigned int *in_col, unsigned int *in_line,
	     int16_t * cubic_spline_n_0, int16_t * cubic_spline_n_1, int16_t * cubic_spline_n_2, int16_t * cubic_spline_n_3,
	     int16_t * cubic_spline_m_0, int16_t * cubic_spline_m_1, int16_t * cubic_spline_m_2, int16_t * cubic_spline_m_3,
	     unsigned int half)
{
  // Warning: because cubic-spline values may be <0 or >1.0, a range test on value is mandatory
  unsigned int local_output_active_width = output_active_width >> half;
  unsigned int local_output_active_height = output_active_height >> half;
  unsigned int local_output_width = output_width >> half;
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int out_line, out_col;
  unsigned long int in_line_offset;

  int16_t csn0, csn1, csn2, csn3;
  int16_t csm0, csm1, csm2, csm3;

  uint8_t *output_p,*adresse0,*adresse1,*adresse2,*adresse3;
  long int value;
  long int value1;

   mjpeg_debug ("Start of cubic_scale ");

   output_p = output;
   
   /* *INDENT-OFF* */
   if (specific == 0) 
     {
	for (out_line = 0; out_line < local_output_active_height; out_line++)
	  {
	     csm0 = cubic_spline_m_0[out_line];
	     csm1 = cubic_spline_m_1[out_line];
	     csm2 = cubic_spline_m_2[out_line];
	     csm3 = cubic_spline_m_3[out_line];
	     in_line_offset = in_line[out_line] * local_padded_width;
	     for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  csn0 =  cubic_spline_n_0[out_col];
		  csn1 =  cubic_spline_n_1[out_col];
		  csn2 =  cubic_spline_n_2[out_col];
		  csn3 =  cubic_spline_n_3[out_col];
		  adresse0 = padded_input + in_col[out_col] + in_line_offset;
		  adresse1 = adresse0 + local_padded_width;
		  adresse2 = adresse1 + local_padded_width;
		  adresse3 = adresse2 + local_padded_width;
		  value1  = (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn0 ;
		  value1 += (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn1 ;
		  value1 += (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn2 ;
		  value1 += (*(adresse0  ) * csm0 + *(adresse1  ) * csm1 + *(adresse2  ) * csm2 + *(adresse3  ) * csm3) * csn3 ;
		  value =
		    (value1 +
		     (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		  if (value < 0)
		    value = 0;
		  
		  if (value > 255) 
		    value = 255;
		  
		  *(output_p++) = (uint8_t) value;
	       }
	     // a line on output is now finished. We jump to the beginning of the next line
	     output_p+=local_output_width-local_output_active_width;
	  }
     }
   else 
     {
	if (specific==6) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  in_line_offset = in_line[out_line] * local_padded_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       csn0 =  cubic_spline_n_0[out_col];
		       csn1 =  cubic_spline_n_1[out_col];
		       csn2 =  cubic_spline_n_2[out_col];
		       csn3 =  cubic_spline_n_3[out_col];
		       adresse0 = padded_input + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       value1 =
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn0;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn1;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn2;
		       value1 += 
			 (int16_t) (*(adresse0  ) + (((int16_t)*(adresse1  ))<<4) + *(adresse2  )) * csn3;
		       *(output_p++) = (uint8_t) divide[value1+bicubic_offset];
		    }
		  output_p+=local_output_width-local_output_active_width;
	       }
	  }

	if (specific==1) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  in_line_offset = in_line[out_line] * local_padded_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       csn0 =  cubic_spline_n_0[out_col];
		       csn1 =  cubic_spline_n_1[out_col];
		       csn2 =  cubic_spline_n_2[out_col];
		       csn3 =  cubic_spline_n_3[out_col];
		       adresse0 = padded_input + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       value1 =
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn0;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn1;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn2;
		       value1 += 
			 (int16_t) (*(adresse0  ) + (((int16_t)*(adresse1  ))<<4) + *(adresse2  )) * csn3;
		       value1 = (9 + value1)/18;
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  // a line on output is now finished. We jump to the beginning of the next line
		  output_p+=local_output_width-local_output_active_width;
	       }
	  }
	if (specific == 5) 
	  {
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  csm0 = cubic_spline_m_0[out_line];
		  csm1 = cubic_spline_m_1[out_line];
		  csm2 = cubic_spline_m_2[out_line];
		  csm3 = cubic_spline_m_3[out_line];
		  in_line_offset = in_line[out_line] * local_padded_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       adresse0 = padded_input + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       adresse3 = adresse2 + local_padded_width;
		       value1  =   *(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3 ;
		       value1 += ((*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3 ) << 4) ;
		       value1 +=   *(adresse0  ) * csm0 + *(adresse1  ) * csm1 + *(adresse2  ) * csm2 + *(adresse3  ) * csm3 ;
		       value1 = (9 + value1)/18;
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  output_p+=local_output_width-local_output_active_width;
	       }
	  }
     }

   /* *INDENT-ON* */
   mjpeg_debug ("End of cubic_scale");
   return (0);
}

// *************************************************************************************


// *************************************************************************************
int
cubic_scale_interlaced (uint8_t * padded_top, uint8_t * padded_bottom, uint8_t * output, 
			unsigned int *in_col, unsigned int *in_line,
			int16_t * cubic_spline_n_0, int16_t * cubic_spline_n_1, int16_t * cubic_spline_n_2, int16_t * cubic_spline_n_3,
			int16_t * cubic_spline_m_0, int16_t * cubic_spline_m_1, int16_t * cubic_spline_m_2, int16_t * cubic_spline_m_3,
			unsigned int half)
{
  unsigned int local_output_active_width = output_active_width >> half;
  unsigned int local_output_active_height = output_active_height >> half;
  unsigned int local_output_width = output_width >> half;
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int out_line, out_col;
  unsigned long int in_line_offset;

  long int value1, value;
  int16_t csn0, csn1, csn2, csn3;
  int16_t csm0, csm1, csm2, csm3;
  uint8_t *output_p,*adresse0,*adresse1,*adresse2,*adresse3;


   mjpeg_debug ("Start of cubic_scale_interlaced");
   
   output_p = output ;

   /* *INDENT-OFF* */
   if (specific == 0) 
     {
	for (out_line = 0; out_line < local_output_active_height/2; out_line++)
	  {
	     csm0 = cubic_spline_m_0[out_line];
	     csm1 = cubic_spline_m_1[out_line];
	     csm2 = cubic_spline_m_2[out_line];
	     csm3 = cubic_spline_m_3[out_line];
	     in_line_offset = in_line[out_line] * local_padded_width;
	     for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  csn0 =  cubic_spline_n_0[out_col];
		  csn1 =  cubic_spline_n_1[out_col];
		  csn2 =  cubic_spline_n_2[out_col];
		  csn3 =  cubic_spline_n_3[out_col];
		  adresse0 = padded_top + in_col[out_col] + in_line_offset;
		  adresse1 = adresse0 + local_padded_width;
		  adresse2 = adresse1 + local_padded_width;
		  adresse3 = adresse2 + local_padded_width;
		  value1  = (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn0 ;
		  value1 += (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn1 ;
		  value1 += (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn2 ;
		  value1 += (*(adresse0  ) * csm0 + *(adresse1  ) * csm1 + *(adresse2  ) * csm2 + *(adresse3  ) * csm3) * csn3 ;
		  value =
		    (value1 +
		     (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		  if (value < 0)
		    value = 0;
		  
		  if (value > 255) 
		    value = 255;
		  
		  *(output_p++) = (uint8_t) value;
	       }
	     // a top line on output is now finished. We jump to the beginning of the next bottom line
	     output_p+=local_output_width-local_output_active_width;
	     for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  csn0 =  cubic_spline_n_0[out_col];
		  csn1 =  cubic_spline_n_1[out_col];
		  csn2 =  cubic_spline_n_2[out_col];
		  csn3 =  cubic_spline_n_3[out_col];
		  adresse0 = padded_bottom + in_col[out_col] + in_line_offset;
		  adresse1 = adresse0 + local_padded_width;
		  adresse2 = adresse1 + local_padded_width;
		  adresse3 = adresse2 + local_padded_width;
		  value1  = (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn0 ;
		  value1 += (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn1 ;
		  value1 += (*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3) * csn2 ;
		  value1 += (*(adresse0  ) * csm0 + *(adresse1  ) * csm1 + *(adresse2  ) * csm2 + *(adresse3  ) * csm3) * csn3 ;
		  value =
		    (value1 +
		     (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		  if (value < 0)
		    value = 0;
		  
		  if (value > 255) 
		    value = 255;
		  
		  *(output_p++) = (uint8_t) value;
	       }
	     // a bottom line on output is now finished. We jump to the beginning of the next top line
	     output_p+=local_output_width-local_output_active_width;
	  }
     }
   else
     {

 if (specific==6) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height /2 ; out_line++)
	       {
		  in_line_offset = in_line[out_line] * local_padded_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       csn0 =  cubic_spline_n_0[out_col];
		       csn1 =  cubic_spline_n_1[out_col];
		       csn2 =  cubic_spline_n_2[out_col];
		       csn3 =  cubic_spline_n_3[out_col];
		       adresse0 = padded_top + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       value1 =
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn0;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn1;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn2;
		       value1 += 
			 (int16_t) (*(adresse0  ) + (((int16_t)*(adresse1  ))<<4) + *(adresse2  )) * csn3;
		       *(output_p++) = (uint8_t) divide[value1+bicubic_offset];
		    }
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       csn0 =  cubic_spline_n_0[out_col];
		       csn1 =  cubic_spline_n_1[out_col];
		       csn2 =  cubic_spline_n_2[out_col];
		       csn3 =  cubic_spline_n_3[out_col];
		       adresse0 = padded_bottom + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       value1 =
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn0;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn1;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn2;
		       value1 += 
			 (int16_t) (*(adresse0  ) + (((int16_t)*(adresse1  ))<<4) + *(adresse2  )) * csn3;
		       *(output_p++) = (uint8_t) divide[value1+bicubic_offset];
		    }
		  // a bottom line on output is now finished. We jump to the beginning of the next top line
		  output_p+=local_output_width-local_output_active_width;
	       }
        }

	if (specific==1) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height /2 ; out_line++)
	       {
		  in_line_offset = in_line[out_line] * local_padded_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       csn0 =  cubic_spline_n_0[out_col];
		       csn1 =  cubic_spline_n_1[out_col];
		       csn2 =  cubic_spline_n_2[out_col];
		       csn3 =  cubic_spline_n_3[out_col];
		       adresse0 = padded_top + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       value1 =
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn0;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn1;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn2;
		       value1 += 
			 (int16_t) (*(adresse0  ) + (((int16_t)*(adresse1  ))<<4) + *(adresse2  )) * csn3;
		       value1 = (9 + value1)/18;
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  // a top line on output is now finished. We jump to the beginning of the bottom line
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       csn0 =  cubic_spline_n_0[out_col];
		       csn1 =  cubic_spline_n_1[out_col];
		       csn2 =  cubic_spline_n_2[out_col];
		       csn3 =  cubic_spline_n_3[out_col];
		       adresse0 = padded_bottom + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       value1 =
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn0;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn1;
		       value1 += 
			 (int16_t) (*(adresse0++) + (((int16_t)*(adresse1++))<<4) + *(adresse2++)) * csn2;
		       value1 += 
			 (int16_t) (*(adresse0  ) + (((int16_t)*(adresse1  ))<<4) + *(adresse2  )) * csn3;
		       value1 = (9 + value1)/18;
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  // a bottom line on output is now finished. We jump to the beginning of the next top line
		  output_p+=local_output_width-local_output_active_width;
	       }
	  }
	if (specific == 5) 
	  {
	     for (out_line = 0; out_line < local_output_active_height/2; out_line++)
	       {
		  csm0 = cubic_spline_m_0[out_line];
		  csm1 = cubic_spline_m_1[out_line];
		  csm2 = cubic_spline_m_2[out_line];
		  csm3 = cubic_spline_m_3[out_line];
		  in_line_offset = in_line[out_line] * local_padded_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       adresse0 = padded_top + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       adresse3 = adresse2 + local_padded_width;
		       value1  =   *(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3 ;
		       value1 += ((*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3 ) << 4) ;
		       value1 +=   *(adresse0  ) * csm0 + *(adresse1  ) * csm1 + *(adresse2  ) * csm2 + *(adresse3  ) * csm3 ;
		       value1 = (9 + value1)/18;
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       adresse0 = padded_bottom + in_col[out_col] + in_line_offset;
		       adresse1 = adresse0 + local_padded_width;
		       adresse2 = adresse1 + local_padded_width;
		       adresse3 = adresse2 + local_padded_width;
		       value1  =   *(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3 ;
		       value1 += ((*(adresse0++) * csm0 + *(adresse1++) * csm1 + *(adresse2++) * csm2 + *(adresse3++) * csm3 ) << 4) ;
		       value1 +=   *(adresse0  ) * csm0 + *(adresse1  ) * csm1 + *(adresse2  ) * csm2 + *(adresse3  ) * csm3 ;
		       value1 = (9 + value1)/18;
		       value =
			 (value1 +
			  (1 << (FLOAT2INTEGERPOWER - 1))) >> (FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  output_p+=local_output_width-local_output_active_width;
	       }
	  }
     }

   /* *INDENT-ON* */
   mjpeg_debug ("End of cubic_scale_interlaced");
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
		   (((12.0 - 9.0 * B -
		      6.0 * C) * fabs (x) * fabs (x) * fabs (x) + (-18.0 +
								   12.0 *
								   B +
								   6.0 *
								   C) *
		     fabs (x) * fabs (x) + (6.0 -
					    2.0 * B)) / 6.0) *
		   multiplicative));
  if (fabs (x) <= 2)
    return ((int16_t)
	    floor (0.5 +
		   (((-B - 6.0 * C) * fabs (x) * fabs (x) * fabs (x) +
		     (6.0 * B + 30.0 * C) * fabs (x) * fabs (x) +
		     (-12.0 * B - 48.0 * C) * fabs (x) + (8.0 * B +
							  24.0 * C)) /
		    6.0) * multiplicative));
  mjpeg_info("In function cubic_spline : x=%f >2",x);
  return (0);
}

// *************************************************************************************


// *************************************************************************************
int
padding (uint8_t * padded_input, uint8_t * input, unsigned int half)
{
  // In cubic interpolation, output pixel are evaluated from the 4*4 neirest neigbors. For border pixels, this
  // requires that input datas along the edge to be duplicated.
  // This padding functions requires output_interlaced==0
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_input_useful_height = input_useful_height >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int local_padded_height = local_input_useful_height + 3;
  unsigned int local_input_width = input_width >> half;
  unsigned int line;

  mjpeg_debug ("Start of padding");
  // PADDING
  // Content Copy with 1 pixel offset on the left and top
  for (line = 0; line < local_input_useful_height; line++)
    memcpy (padded_input + 1 + (line + 1) * local_padded_width,
	    input + line * local_input_width, local_input_useful_width);

  // borders padding: 1 pixel on the left and 2 on the right
  for (line = 1; line < local_padded_height - 2; line++)
    {
      *(padded_input + 0 + line * local_padded_width)
	= *(padded_input + 1 + line * local_padded_width);
      *(padded_input + (local_padded_width - 1) + line * local_padded_width)
	= *(padded_input + (local_padded_width - 2) +
	    line * local_padded_width)
	= *(padded_input + (local_padded_width - 3) +
	    line * local_padded_width);
    }

  // borders padding: 1 pixel on top and 2 on the bottom
  memcpy (padded_input, padded_input + 1 * local_padded_width,
	  local_padded_width);
  memcpy (padded_input + (local_padded_height - 1) * local_padded_width,
	  padded_input + (local_padded_height - 3) * local_padded_width,
	  local_padded_width);
  memcpy (padded_input + (local_padded_height - 2) * local_padded_width,
	  padded_input + (local_padded_height - 3) * local_padded_width,
	  local_padded_width);
   mjpeg_debug ("End of padding");

  return (0);
}

// *************************************************************************************

// *************************************************************************************
int
padding_interlaced (uint8_t * padded_top, uint8_t * padded_bottom,
		    uint8_t * input, unsigned int half)
{
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_input_useful_height = input_useful_height >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int local_padded_height = local_input_useful_height / 2 + 3;
  unsigned int local_input_width = input_width >> half;
  unsigned int line;
  uint8_t * uint8_ptop, * uint8_pbot, * uint8_input;

  mjpeg_debug ("Start of padding_interlaced");
  // PADDING
  // Content Copy with 1 pixel offset on the left and top
  uint8_ptop = padded_top + 1 + local_padded_width;
  uint8_pbot = padded_bottom + 1 + local_padded_width;
  uint8_input  = input;
  for (line = 0; line < local_input_useful_height / 2; line++)
    {
       memcpy (uint8_ptop,uint8_input,local_input_useful_width);
       uint8_ptop += local_padded_width;
       uint8_input += local_input_width;
       memcpy (uint8_pbot,uint8_input,local_input_useful_width);
       uint8_pbot += local_padded_width;
       uint8_input += local_input_width;
    }
/*       
      memcpy (padded_top + 1 + (line + 1) * local_padded_width,
	      input + 2 * line * local_input_width, local_input_useful_width);
      memcpy (padded_bottom + 1 + (line + 1) * local_padded_width,
	      input + (2 * line + 1) * local_input_width,
	      local_input_useful_width);
*/

  // borders padding: 1 pixel on the left and 2 on the right
  for (line = 1; line < local_padded_height - 2; line++)
    {
      *(padded_top + 0 + line * local_padded_width)
	= *(padded_top + 1 + line * local_padded_width);
      *(padded_top + (local_padded_width - 1) + line * local_padded_width)
	= *(padded_top + (local_padded_width - 2) +
	    line * local_padded_width)
	= *(padded_top + (local_padded_width - 3) +
	    line * local_padded_width);
    }
  for (line = 1; line < local_padded_height - 2; line++)
    {
      *(padded_bottom + 0 + line * local_padded_width)
	= *(padded_bottom + 1 + line * local_padded_width);
      *(padded_bottom + (local_padded_width - 1) +
	line * local_padded_width) =
	*(padded_bottom + (local_padded_width - 2) +
	  line * local_padded_width) =
	*(padded_bottom + (local_padded_width - 3) +
	  line * local_padded_width);
    }
  // borders padding: 1 pixel on top and 2 on the bottom
  memcpy (padded_top, padded_top + 1 * local_padded_width,
	  local_padded_width);
  memcpy (padded_top + (local_padded_height - 1) * local_padded_width,
	  padded_top + (local_padded_height - 3) * local_padded_width,
	  local_padded_width);
  memcpy (padded_top + (local_padded_height - 2) * local_padded_width,
	  padded_top + (local_padded_height - 3) * local_padded_width,
	  local_padded_width);
  memcpy (padded_bottom, padded_bottom + 1 * local_padded_width,
	  local_padded_width);
  memcpy (padded_bottom + (local_padded_height - 1) * local_padded_width,
	  padded_bottom + (local_padded_height - 3) * local_padded_width,
	  local_padded_width);
  memcpy (padded_bottom + (local_padded_height - 2) * local_padded_width,
	  padded_bottom + (local_padded_height - 3) * local_padded_width,
	  local_padded_width);
  mjpeg_debug ("End of padding_interlaced");
  return (0);

}

// *************************************************************************************

