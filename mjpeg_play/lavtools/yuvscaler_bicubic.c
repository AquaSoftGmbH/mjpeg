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
  unsigned long int in_line_offset;
  int16_t output_offset;

  uint8_t *output_p,*line,*line_start[height_neighbors];
  int16_t *csplinew[width_neighbors],*csplineh[height_neighbors];
  long int value,value1,value2;

   mjpeg_debug ("Start of cubic_scale ");

   output_p = output;
   output_offset = local_output_width-local_output_active_width;

 switch(specific) {
   /* *INDENT-OFF* */
  case 0: 
      {
	 for (h=0;h<height_neighbors;h++)
	   csplineh[h]=cspline_h_neighbors[h];
	 for (out_line = 0; out_line < local_output_active_height; out_line++)
	   {
	      for (w=0;w<width_neighbors;w++)
		csplinew[w]=cspline_w_neighbors[w];
	      in_line_offset = in_line[out_line] * local_padded_width;
	      for (out_col = 0; out_col < local_output_active_width; out_col++)
	       {
		  line_start[0]=padded_input + in_col[out_col] + in_line_offset;
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
	      // a line on output is now finished. We jump to the beginning of the next line
	      output_p+=output_offset;
	      for (h=0;h<height_neighbors;h++)
		csplineh[h]++;
	   }
      }
    break;
    
  case 1:
    // We only downscale on width, not height
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
    break;

   

  case 5:
    // We only downscale on height, not width
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
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
	       }
	      // a line on output is now finished. We jump to the beginning of the next line
	      output_p+=output_offset;
	      for (h=0;h<height_neighbors;h++)
		csplineh[h]++;
	   }
      }
    break;
 }
   
    
// 6 : SVCD downscaling, height not scaled, 3 to 2 on width => width_neighbors=6
   /*
  else 
     {

	if (specific==6) 
	  {  
	     output_offset=local_output_width-local_output_active_width;
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  in_line_offset = in_line[out_line] * local_padded_width;
		  csplinew0 = cspline_w_neighbors[0];
		  csplinew1 = cspline_w_neighbors[1];
		  csplinew2 = cspline_w_neighbors[2];
		  csplinew3 = cspline_w_neighbors[3];
		  csplinew4 = cspline_w_neighbors[4];
		  csplinew5 = cspline_w_neighbors[5];
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       adresse1 = padded_input + in_col[out_col] + in_line_offset + local_padded_width;
		       value1  = *(adresse1++) * (*(csplinew0++));
		       value1 += *(adresse1++) * (*(csplinew1++));
		       value1 += *(adresse1++) * (*(csplinew2++));
		       value1 += *(adresse1++) * (*(csplinew3++));
		       value1 += *(adresse1++) * (*(csplinew4++));
		       value1 += *(adresse1  ) * (*(csplinew5++));
		       if (value1 < 0)
			 *(output_p++) = 0;
		       else 
			 {
			    value =
			      (value1 +
			       (1 << (FLOAT2INTEGERPOWER - 1))) >> FLOAT2INTEGERPOWER;
			    
			    if (value > 255) 
			      *(output_p++) = 255;
			    else
			      *(output_p++) = (uint8_t) value;
			 }
		    }
		  output_p+=output_offset;
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
		       value1 = (*adresse0) * csm0 + (*adresse1) * csm1 + (*adresse2) * csm2 + (*adresse3) * csm3 ;
		       if (value1 < 0)
			 *(output_p++) = 0;
		       else 
			 {
			    value =
			      (value1 +
			       (1 << (FLOAT2INTEGERPOWER - 1))) >> FLOAT2INTEGERPOWER;
			    
			    if (value > 255) 
			      *(output_p++) = 255;
			    else
			      *(output_p++) = (uint8_t) value;
			 }
		    }
		  output_p+=local_output_width-local_output_active_width;
	       }
	  }
     }
*/
   /* *INDENT-ON* */
   mjpeg_debug ("End of cubic_scale");
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
  long int value,value1,value2;

   mjpeg_debug ("Start of cubic_scale_interlaced");
   
   output_p = output ;
   output_offset = local_output_width-local_output_active_width;

   /* *INDENT-OFF* */
 switch(specific) {
  case 0: 
      {
	 for (h=0;h<height_neighbors;h++)
	   csplineh[h]=cspline_h_neighbors[h];
	 for (out_line = 0; out_line < (local_output_active_height>>1); out_line++)
	   {
	      in_line_offset = in_line[out_line] * local_padded_width;
	      for (w=0;w<width_neighbors;w++)
		csplinew[w]=cspline_w_neighbors[w];
	      // Top line
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
    break;
    
  case 1:
    // We only downscale on width, not height
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
    break;

   

  case 5:
    // We only downscale on height, not width
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
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
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
		  for (w=0;w<width_neighbors;w++)
		    csplinew[w]++;
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

  mjpeg_debug ("Start of padding, left_offset=%d,top_offset=%d,right_offset=%d,bottom_offset=%d,width_pad=%d",
	       left_offset,top_offset,right_offset,bottom_offset,width_pad);
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
  mjpeg_debug ("End of padding");

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

  mjpeg_debug ("Start of padding_interlaced, left_offset=%d,top_offset=%d,right_offset=%d,bottom_offset=%d,width_pad=%d",
	       left_offset,top_offset,right_offset,bottom_offset,width_pad);
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

  mjpeg_debug ("End of padding_interlaced");
  return (0);

}

// *************************************************************************************

