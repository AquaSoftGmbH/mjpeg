#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "yuv4mpeg.h"
#include "inttypes.h"
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


#ifdef HAVE_ASM_MMX
// MMX
extern int16_t *mmx_padded, *mmx_cubic;
extern int32_t *mmx_res;
extern long int max_lint_neg;	// maximal negative number available for long int
extern int mmx;			// =1 for mmx activated
#endif
// Defines
#define FLOAT2INTEGERPOWER 11
extern long int FLOAT2INTOFFSET;



// *************************************************************************************
int
cubic_scale (uint8_t * padded_input, uint8_t * output, unsigned int *in_col,
	     float *b, unsigned int *in_line, float *a,
	     int16_t * cubic_spline_n,
	     int16_t * cubic_spline_m, unsigned int half)
{
  // Warning: because cubic-spline values may be <0 or >1.0, a range test on value is mandatory
  unsigned int local_output_active_width = output_active_width >> half;
  unsigned int local_output_active_height = output_active_height >> half;
  unsigned int local_output_width = output_width >> half;
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int out_line, out_col;

  int16_t cubic_spline_n_0, cubic_spline_n_1, cubic_spline_n_2,
    cubic_spline_n_3;
  int16_t cubic_spline_m_0, cubic_spline_m_1, cubic_spline_m_2,
    cubic_spline_m_3;


//   uint8_t zero=0;
  uint8_t *output_p;
  long int value;
  long int value1;

#ifdef HAVE_ASM_MMX
  unsigned long int ulint, base_0, base_1, base_2, base_3;
  int32_t value_mmx_1, value_mmx_2, value_mmx_3, value_mmx_4;
  long int value2;
#endif
   
   
   mjpeg_debug ("Start of cubic_scale\n");


   
   /* *INDENT-OFF* */

#ifdef HAVE_ASM_MMX
   if (mmx==1) 
     {
	mjpeg_debug ("-- MMX --");
	if ((specific==1) || (specific==6)) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  output_p = output + out_line * local_output_width;
		  // MMX Calculations:
		  // We use the fact the on height, we have a 1 to 1 ratio => cubic_spline_3 are 1, 16, 1 and 0 => 
		  // we factorise cubic_spline_n 	     
		  // This implies that value_mmx_? never exceed 9330 => 
		  // may be stored on a signed 16 bits integer => mmx maybe used 5 times (+last calculation), not only 4 

		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       base_0 = in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width;
		       base_1 = in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width;
		       base_2 = in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width;
		       
		       mmx_cubic[0]=1;
		       mmx_cubic[1]=16;
		       mmx_cubic[2]=1;
		       mmx_cubic[3]=0;
		       
		       mmx_padded[0]=(int16_t)padded_input[base_0++];
		       mmx_padded[1]=(int16_t)padded_input[base_1++];
		       mmx_padded[2]=(int16_t)padded_input[base_2++];
		       mmx_padded[3]=0;
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_1 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_1 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_input[base_0++];
		       mmx_padded[1]=(int16_t)padded_input[base_1++];
		       mmx_padded[2]=(int16_t)padded_input[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_2 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_2 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_input[base_0++];
		       mmx_padded[1]=(int16_t)padded_input[base_1++];
		       mmx_padded[2]=(int16_t)padded_input[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_3 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_3 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_input[base_0++];
		       mmx_padded[1]=(int16_t)padded_input[base_1++];
		       mmx_padded[2]=(int16_t)padded_input[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_4 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_4 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       

		       mmx_cubic[0]=cubic_spline_n[out_col + 0 * output_active_width];
		       mmx_cubic[1]=cubic_spline_n[out_col + 1 * output_active_width];
		       mmx_cubic[2]=cubic_spline_n[out_col + 2 * output_active_width];
		       mmx_cubic[3]=cubic_spline_n[out_col + 3 * output_active_width];
		       mmx_padded[0]=(int16_t)value_mmx_1;
		       mmx_padded[1]=(int16_t)value_mmx_2;
		       mmx_padded[2]=(int16_t)value_mmx_3;
		       mmx_padded[3]=(int16_t)value_mmx_4;
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value2 = mmx_res[0]+mmx_res[1];
		       if (value2 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       *(output_p++) = (uint8_t) divide[value2+bicubic_offset];
		    }
	       }
	  }
	
	else
	  {
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  output_p = output + out_line * local_output_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       base_0 = in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width;
		       base_1 = in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width;
		       base_2 = in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width;
		       base_3 = in_col[out_col] + 0 + (in_line[out_line] + 3) * local_padded_width;

		       mmx_padded[0]=(int16_t)padded_input[base_0++];
		       mmx_padded[1]=(int16_t)padded_input[base_0++];
		       mmx_padded[2]=(int16_t)padded_input[base_0++];
		       mmx_padded[3]=(int16_t)padded_input[base_0];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_1 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_1 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       
		       mmx_padded[0]=(int16_t)padded_input[base_1++];
		       mmx_padded[1]=(int16_t)padded_input[base_1++];
		       mmx_padded[2]=(int16_t)padded_input[base_1++];
		       mmx_padded[3]=(int16_t)padded_input[base_1];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_2 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_2 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       
		       mmx_padded[0]=(int16_t)padded_input[base_2++];
		       mmx_padded[1]=(int16_t)padded_input[base_2++];
		       mmx_padded[2]=(int16_t)padded_input[base_2++];
		       mmx_padded[3]=(int16_t)padded_input[base_2];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_3 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_3 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");

		       mmx_padded[0]=(int16_t)padded_input[base_3++];
		       mmx_padded[1]=(int16_t)padded_input[base_3++];
		       mmx_padded[2]=(int16_t)padded_input[base_3++];
		       mmx_padded[3]=(int16_t)padded_input[base_3];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_4 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_4 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       value2 =
			 cubic_spline_m[out_line + 0 * output_active_height] * value_mmx_1 +
			 cubic_spline_m[out_line + 1 * output_active_height] * value_mmx_2 +
			 cubic_spline_m[out_line + 2 * output_active_height] * value_mmx_3 +
			 cubic_spline_m[out_line + 3 * output_active_height] * value_mmx_4 +
			 FLOAT2INTOFFSET;
		       
		       if (value2 < 0)
			 *(output_p++) = (uint8_t) 0;
		       else {
			  ulint = ((unsigned long int) value2) >> (2 * FLOAT2INTEGERPOWER);
			  if (ulint > 255)
			    *(output_p++) = (uint8_t) 255;
			  else
			    *(output_p++) = (uint8_t) ulint;
		       }
		       
		    }
		  
	       }
	     
	  }
     }
   else
#endif
     {
	// NON-MMX algorithms
	if ((specific==1) || (specific==6)) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  output_p = output + out_line * local_output_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       cubic_spline_n_0 =  cubic_spline_n[out_col + 0 * output_active_width];
		       cubic_spline_n_1 =  cubic_spline_n[out_col + 1 * output_active_width];
		       cubic_spline_n_2 =  cubic_spline_n[out_col + 2 * output_active_width];
		       cubic_spline_n_3 =  cubic_spline_n[out_col + 3 * output_active_width];
		       value1 =
			 (
			  padded_input[in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width] 
			  * cubic_spline_n_0
			  + padded_input[in_col[out_col] + 1 + (in_line[out_line] + 0) * local_padded_width] 
			  * cubic_spline_n_1
			  + padded_input[in_col[out_col] + 2 + (in_line[out_line] + 0) * local_padded_width]
			  * cubic_spline_n_2
			  + padded_input[in_col[out_col] + 3 + (in_line[out_line] + 0) * local_padded_width]
			  * cubic_spline_n_3
			  ) + ((
				padded_input[in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_0
				+ padded_input[in_col[out_col] + 1 + (in_line[out_line] + 1) * local_padded_width] 
				* cubic_spline_n_1
				+ padded_input[in_col[out_col] + 2 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_2
				+ padded_input[in_col[out_col] + 3 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_3
				)<<4) + ( 
					  padded_input[in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_0
					  + padded_input[in_col[out_col] + 1 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_1
					  + padded_input[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_2
					  + padded_input[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_3
					  );
		       *(output_p++) = (uint8_t) divide[value1+bicubic_offset];
		    }
   
	       }
	  }
	else
	  {
	     for (out_line = 0; out_line < local_output_active_height; out_line++)
	       {
		  cubic_spline_m_0 = cubic_spline_m[out_line + 0 * output_active_height];
		  cubic_spline_m_1 = cubic_spline_m[out_line + 1 * output_active_height];
		  cubic_spline_m_2 = cubic_spline_m[out_line + 2 * output_active_height];
		  cubic_spline_m_3 = cubic_spline_m[out_line + 3 * output_active_height];
		  output_p = output + out_line * local_output_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       cubic_spline_n_0 =  cubic_spline_n[out_col + 0 * output_active_width];
		       cubic_spline_n_1 =  cubic_spline_n[out_col + 1 * output_active_width];
		       cubic_spline_n_2 =  cubic_spline_n[out_col + 2 * output_active_width];
		       cubic_spline_n_3 =  cubic_spline_n[out_col + 3 * output_active_width];
		       value1 = 
			 cubic_spline_m_0 *
			 (
			   padded_input[in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width] 
			   * cubic_spline_n_0
			   + padded_input[in_col[out_col] + 1 + (in_line[out_line] + 0) * local_padded_width] 
			   * cubic_spline_n_1
			   + padded_input[in_col[out_col] + 2 + (in_line[out_line] + 0) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_input[in_col[out_col] + 3 + (in_line[out_line] + 0) * local_padded_width]
			   * cubic_spline_n_3
			   ) + 
			 cubic_spline_m_1 *
			 ( 
			   padded_input[in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_input[in_col[out_col] + 1 + (in_line[out_line] + 1) * local_padded_width] 
			   * cubic_spline_n_1
			   + padded_input[in_col[out_col] + 2 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_input[in_col[out_col] + 3 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_3
			   ) + 
			 cubic_spline_m_2 *
			 ( 
			   padded_input[in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_input[in_col[out_col] + 1 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_1
			   + padded_input[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_input[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_3
			   ) +  
			 cubic_spline_m_3 *
			 (
 			   padded_input[in_col[out_col] + 0 + (in_line[out_line] + 3) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_input[in_col[out_col] + 1 + (in_line[out_line] + 3) * local_padded_width]                                          
			   * cubic_spline_n_1
			   + padded_input[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]                                          
			   * cubic_spline_n_2
			   + padded_input[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_3
			   );
		       value =
			 (value1 +
			  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
	       }
	  }
     }



   /* *INDENT-ON* */
  mjpeg_debug ("End of cubic_scale\n");
  return (0);
}

// *************************************************************************************


// *************************************************************************************
int
cubic_scale_interlaced (uint8_t * padded_top, uint8_t * padded_bottom,
			uint8_t * output, unsigned int *in_col, float *b,
			unsigned int *in_line, float *a,
			int16_t * cubic_spline_n,
			int16_t * cubic_spline_m, unsigned int half)
{
  unsigned int local_output_active_width = output_active_width >> half;
  unsigned int local_output_active_height = output_active_height >> half;
  unsigned int local_output_width = output_width >> half;
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int out_line, out_col;

  long int value1, value;
  int16_t cubic_spline_n_0, cubic_spline_n_1, cubic_spline_n_2,
    cubic_spline_n_3;
  int16_t cubic_spline_m_0, cubic_spline_m_1, cubic_spline_m_2,
    cubic_spline_m_3;
  uint8_t *output_p;

#ifdef HAVE_ASM_MMX
  unsigned long int ulint, base_0, base_1, base_2, base_3;
  long int value_mmx_1, value_mmx_2, value_mmx_3, value_mmx_4;
  long int value2;
#endif

   mjpeg_debug ("Start of cubic_scale_interlaced\n");

   /* *INDENT-OFF* */
#ifdef HAVE_ASM_MMX
   if (mmx==1) 
     {
	mjpeg_debug ("-- MMX --");
	if ((specific==1) || (specific==6)) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height / 2; out_line++)
	       {
		  output_p = output + 2 * out_line * local_output_width;
		  // MMX Calculations:
		  // We use the fact the on height, we have a 1 to 1 ratio => cubic_spline_3 are 1, 16, 1 and 0 => 
		  // we factorise cubic_spline_n 	     
		  // This implies that value_mmx_? never exceed 9330 => 
		  // may be stored on a signed 16 bits integer => mmx maybe used 5 times (+last calculation), not only 4 

		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       base_0 = in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width;
		       base_1 = in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width;
		       base_2 = in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width;
		       
		       mmx_cubic[0]=1;
		       mmx_cubic[1]=16;
		       mmx_cubic[2]=1;
		       mmx_cubic[3]=0;
		       
		       mmx_padded[0]=(int16_t)padded_top[base_0++];
		       mmx_padded[1]=(int16_t)padded_top[base_1++];
		       mmx_padded[2]=(int16_t)padded_top[base_2++];
		       mmx_padded[3]=0;
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_1 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_1 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_top[base_0++];
		       mmx_padded[1]=(int16_t)padded_top[base_1++];
		       mmx_padded[2]=(int16_t)padded_top[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_2 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_2 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_top[base_0++];
		       mmx_padded[1]=(int16_t)padded_top[base_1++];
		       mmx_padded[2]=(int16_t)padded_top[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_3 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_3 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_top[base_0++];
		       mmx_padded[1]=(int16_t)padded_top[base_1++];
		       mmx_padded[2]=(int16_t)padded_top[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_4 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_4 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       

		       mmx_cubic[0]=cubic_spline_n[out_col + 0 * output_active_width];
		       mmx_cubic[1]=cubic_spline_n[out_col + 1 * output_active_width];
		       mmx_cubic[2]=cubic_spline_n[out_col + 2 * output_active_width];
		       mmx_cubic[3]=cubic_spline_n[out_col + 3 * output_active_width];
		       mmx_padded[0]=(int16_t)value_mmx_1;
		       mmx_padded[1]=(int16_t)value_mmx_2;
		       mmx_padded[2]=(int16_t)value_mmx_3;
		       mmx_padded[3]=(int16_t)value_mmx_4;
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value2 = mmx_res[0]+mmx_res[1];
		       if (value2 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       *(output_p++) = (uint8_t) divide[value2+bicubic_offset];
		    }
		  // First line output is now finished. We jump to the beginning of the next line
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       base_0 = in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width;
		       base_1 = in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width;
		       base_2 = in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width;
		       
		       mmx_cubic[0]=1;
		       mmx_cubic[1]=16;
		       mmx_cubic[2]=1;
		       mmx_cubic[3]=0;
		       
		       mmx_padded[0]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_2++];
		       mmx_padded[3]=0;
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_1 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_1 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_2 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_2 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_3 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_3 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       mmx_padded[0]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_2++];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_4 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_4 ==  max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       
		       mmx_cubic[0]=cubic_spline_n[out_col + 0 * output_active_width];
		       mmx_cubic[1]=cubic_spline_n[out_col + 1 * output_active_width];
		       mmx_cubic[2]=cubic_spline_n[out_col + 2 * output_active_width];
		       mmx_cubic[3]=cubic_spline_n[out_col + 3 * output_active_width];
		       mmx_padded[0]=(int16_t)value_mmx_1;
		       mmx_padded[1]=(int16_t)value_mmx_2;
		       mmx_padded[2]=(int16_t)value_mmx_3;
		       mmx_padded[3]=(int16_t)value_mmx_4;
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value2 = mmx_res[0]+mmx_res[1];
		       if (value2 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       *(output_p++) = (uint8_t) divide[value2+bicubic_offset];
		    }
	       }
	  }
	
	else
	  {
	     for (out_line = 0; out_line < local_output_active_height / 2; out_line++)
	       {
		  output_p = output + 2 * out_line * local_output_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       base_0 = in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width;
		       base_1 = in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width;
		       base_2 = in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width;
		       base_3 = in_col[out_col] + 0 + (in_line[out_line] + 3) * local_padded_width;

		       mmx_padded[0]=(int16_t)padded_top[base_0++];
		       mmx_padded[1]=(int16_t)padded_top[base_0++];
		       mmx_padded[2]=(int16_t)padded_top[base_0++];
		       mmx_padded[3]=(int16_t)padded_top[base_0];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_1 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_1 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       
		       mmx_padded[0]=(int16_t)padded_top[base_1++];
		       mmx_padded[1]=(int16_t)padded_top[base_1++];
		       mmx_padded[2]=(int16_t)padded_top[base_1++];
		       mmx_padded[3]=(int16_t)padded_top[base_1];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_2 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_2 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       
		       mmx_padded[0]=(int16_t)padded_top[base_2++];
		       mmx_padded[1]=(int16_t)padded_top[base_2++];
		       mmx_padded[2]=(int16_t)padded_top[base_2++];
		       mmx_padded[3]=(int16_t)padded_top[base_2];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_3 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_3 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");

		       mmx_padded[0]=(int16_t)padded_top[base_3++];
		       mmx_padded[1]=(int16_t)padded_top[base_3++];
		       mmx_padded[2]=(int16_t)padded_top[base_3++];
		       mmx_padded[3]=(int16_t)padded_top[base_3];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_4 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_4 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       value2 =
			 cubic_spline_m[out_line + 0 * output_active_height] * value_mmx_1 +
			 cubic_spline_m[out_line + 1 * output_active_height] * value_mmx_2 +
			 cubic_spline_m[out_line + 2 * output_active_height] * value_mmx_3 +
			 cubic_spline_m[out_line + 3 * output_active_height] * value_mmx_4 +
			 FLOAT2INTOFFSET;
		       
		       if (value2 < 0)
			 *(output_p++) = (uint8_t) 0;
		       else {
			  ulint = ((unsigned long int) value2) >> (2 * FLOAT2INTEGERPOWER);
			  if (ulint > 255)
			    *(output_p++) = (uint8_t) 255;
			  else
			    *(output_p++) = (uint8_t) ulint;
		       }
		       
		    }
		  
		  // First line output is now finished. We jump to the beginning of the next line
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       base_0 = in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width;
		       base_1 = in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width;
		       base_2 = in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width;
		       base_3 = in_col[out_col] + 0 + (in_line[out_line] + 3) * local_padded_width;
		       
		       mmx_padded[0]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_0++];
		       mmx_padded[3]=(int16_t)padded_bottom[base_0];
		       mmx_cubic[0]=cubic_spline_n[out_col + 0 * output_active_width];
		       mmx_cubic[1]=cubic_spline_n[out_col + 1 * output_active_width];
		       mmx_cubic[2]=cubic_spline_n[out_col + 2 * output_active_width];
		       mmx_cubic[3]=cubic_spline_n[out_col + 3 * output_active_width];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_1 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_1 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");
		       
		       
		       mmx_padded[0]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_1++];
		       mmx_padded[3]=(int16_t)padded_bottom[base_1];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_2 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_2 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");

		    
		       mmx_padded[0]=(int16_t)padded_bottom[base_2++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_2++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_2++];
		       mmx_padded[3]=(int16_t)padded_bottom[base_2];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_3 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_3 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");

		       mmx_padded[0]=(int16_t)padded_bottom[base_3++];
		       mmx_padded[1]=(int16_t)padded_bottom[base_3++];
		       mmx_padded[2]=(int16_t)padded_bottom[base_3++];
		       mmx_padded[3]=(int16_t)padded_bottom[base_3];
		       movq_m2r(*mmx_padded,mm1);
		       movq_m2r(*mmx_cubic ,mm2);
		       pmaddwd_r2r(mm1,mm2);
		       movq_r2m(mm2,*mmx_res);
		       value_mmx_4 = mmx_res[0]+mmx_res[1];
		       if (value_mmx_4 == max_lint_neg)
			 fprintf(stderr,"MMX MAGIC NECESSITY\n");

		       value2 =
			 cubic_spline_m[out_line + 0 * output_active_height] * value_mmx_1 +
			 cubic_spline_m[out_line + 1 * output_active_height] * value_mmx_2 +
			 cubic_spline_m[out_line + 2 * output_active_height] * value_mmx_3 +
			 cubic_spline_m[out_line + 3 * output_active_height] * value_mmx_4 +
			 FLOAT2INTOFFSET;
		       
		       if (value2 < 0)
			 *(output_p++) = (uint8_t) 0;
		       else {
			  ulint = ((unsigned long int) value2) >> (2 * FLOAT2INTEGERPOWER);
			  if (ulint > 255)
			    *(output_p++) = (uint8_t) 255;
			  else
			    *(output_p++) = (uint8_t) ulint;
		       }
		       
		    }
	       }
	     
	  }
     }
   else
#endif
     {
	// NON-MMX algorithms
	if ((specific==1) || (specific==6)) 
	  {  
	     for (out_line = 0; out_line < local_output_active_height / 2; out_line++)
	       {
		  output_p = output + 2 * out_line * local_output_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       cubic_spline_n_0 =  cubic_spline_n[out_col + 0 * output_active_width];
		       cubic_spline_n_1 =  cubic_spline_n[out_col + 1 * output_active_width];
		       cubic_spline_n_2 =  cubic_spline_n[out_col + 2 * output_active_width];
		       cubic_spline_n_3 =  cubic_spline_n[out_col + 3 * output_active_width];
		       value1 =
			 (
			  padded_top[in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width] 
			  * cubic_spline_n_0
			  + padded_top[in_col[out_col] + 1 + (in_line[out_line] + 0) * local_padded_width] 
			  * cubic_spline_n_1
			  + padded_top[in_col[out_col] + 2 + (in_line[out_line] + 0) * local_padded_width]
			  * cubic_spline_n_2
			  + padded_top[in_col[out_col] + 3 + (in_line[out_line] + 0) * local_padded_width]
			  * cubic_spline_n_3
			  ) + ((
				padded_top[in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_0
				+ padded_top[in_col[out_col] + 1 + (in_line[out_line] + 1) * local_padded_width] 
				* cubic_spline_n_1
				+ padded_top[in_col[out_col] + 2 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_2
				+ padded_top[in_col[out_col] + 3 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_3
				)<<4) + ( 
					  padded_top[in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_0
					  + padded_top[in_col[out_col] + 1 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_1
					  + padded_top[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_2
					  + padded_top[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_3
					  );
		       *(output_p++) = (uint8_t) divide[value1+bicubic_offset];
		    }
		  // First line output is now finished. We jump to the beginning of the next line
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       cubic_spline_n_0 =  cubic_spline_n[out_col + 0 * output_active_width];
		       cubic_spline_n_1 =  cubic_spline_n[out_col + 1 * output_active_width];
		       cubic_spline_n_2 =  cubic_spline_n[out_col + 2 * output_active_width];
		       cubic_spline_n_3 =  cubic_spline_n[out_col + 3 * output_active_width];
		       value1 =
			 (
			  padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width] 
			  * cubic_spline_n_0
			  + padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 0) * local_padded_width] 
			  * cubic_spline_n_1
			  + padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 0) * local_padded_width]
			  * cubic_spline_n_2
			  + padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 0) * local_padded_width]
			  * cubic_spline_n_3
			  ) + ((
				padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_0
				+ padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 1) * local_padded_width] 
				* cubic_spline_n_1
				+ padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_2
				+ padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 1) * local_padded_width]
				* cubic_spline_n_3
				)<<4) + ( 
					  padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_0
					  + padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_1
					  + padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_2
					  + padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
					  * cubic_spline_n_3
					  );
		       *(output_p++) = (uint8_t) divide[value1+bicubic_offset];
		    }
   
	       }
	  }
	else
	  {
	     for (out_line = 0; out_line < local_output_active_height / 2; out_line++)
	       {
		  cubic_spline_m_0 = cubic_spline_m[out_line + 0 * output_active_height];
		  cubic_spline_m_1 = cubic_spline_m[out_line + 1 * output_active_height];
		  cubic_spline_m_2 = cubic_spline_m[out_line + 2 * output_active_height];
		  cubic_spline_m_3 = cubic_spline_m[out_line + 3 * output_active_height];
		  output_p = output + 2 * out_line * local_output_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       cubic_spline_n_0 =  cubic_spline_n[out_col + 0 * output_active_width];
		       cubic_spline_n_1 =  cubic_spline_n[out_col + 1 * output_active_width];
		       cubic_spline_n_2 =  cubic_spline_n[out_col + 2 * output_active_width];
		       cubic_spline_n_3 =  cubic_spline_n[out_col + 3 * output_active_width];
		       value1 = 
			 cubic_spline_m_0 *
			 (
			   padded_top[in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width] 
			   * cubic_spline_n_0
			   + padded_top[in_col[out_col] + 1 + (in_line[out_line] + 0) * local_padded_width] 
			   * cubic_spline_n_1
			   + padded_top[in_col[out_col] + 2 + (in_line[out_line] + 0) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_top[in_col[out_col] + 3 + (in_line[out_line] + 0) * local_padded_width]
			   * cubic_spline_n_3
			   ) + 
			 cubic_spline_m_1 *
			 ( 
			   padded_top[in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_top[in_col[out_col] + 1 + (in_line[out_line] + 1) * local_padded_width] 
			   * cubic_spline_n_1
			   + padded_top[in_col[out_col] + 2 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_top[in_col[out_col] + 3 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_3
			   ) + 
			 cubic_spline_m_2 *
			 ( 
			   padded_top[in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_top[in_col[out_col] + 1 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_1
			   + padded_top[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_top[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_3
			   ) +  
			 cubic_spline_m_3 *
			 (
 			   padded_top[in_col[out_col] + 0 + (in_line[out_line] + 3) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_top[in_col[out_col] + 1 + (in_line[out_line] + 3) * local_padded_width]                                          
			   * cubic_spline_n_1
			   + padded_top[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]                                          
			   * cubic_spline_n_2
			   + padded_top[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_3
			   );
		       value =
			 (value1 +
			  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  output_p+=local_output_width-local_output_active_width;
		  for (out_col = 0; out_col < local_output_active_width; out_col++)
		    {
		       cubic_spline_n_0 =  cubic_spline_n[out_col + 0 * output_active_width];
		       cubic_spline_n_1 =  cubic_spline_n[out_col + 1 * output_active_width];
		       cubic_spline_n_2 =  cubic_spline_n[out_col + 2 * output_active_width];
		       cubic_spline_n_3 =  cubic_spline_n[out_col + 3 * output_active_width];
		       value1 = 
			 cubic_spline_m_0 *
			 (
			   padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 0) * local_padded_width] 
			   * cubic_spline_n_0
			   + padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 0) * local_padded_width] 
			   * cubic_spline_n_1
			   + padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 0) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 0) * local_padded_width]
			   * cubic_spline_n_3
			   ) + 
			 cubic_spline_m_1 *
			 ( 
			   padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 1) * local_padded_width] 
			   * cubic_spline_n_1
			   + padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 1) * local_padded_width]
			   * cubic_spline_n_3
			   ) + 
			 cubic_spline_m_2 *
			 ( 
			   padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_1
			   + padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_2
			   + padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_3
			   ) +  
			 cubic_spline_m_3 *
			 (
			   padded_bottom[in_col[out_col] + 0 + (in_line[out_line] + 3) * local_padded_width]
			   * cubic_spline_n_0
			   + padded_bottom[in_col[out_col] + 1 + (in_line[out_line] + 3) * local_padded_width]                                          
			   * cubic_spline_n_1
			   + padded_bottom[in_col[out_col] + 2 + (in_line[out_line] + 2) * local_padded_width]                                          
			   * cubic_spline_n_2
			   + padded_bottom[in_col[out_col] + 3 + (in_line[out_line] + 2) * local_padded_width]
			   * cubic_spline_n_3
			   );
		       value =
			 (value1 +
			  (1 << (2 * FLOAT2INTEGERPOWER - 1))) >> (2 * FLOAT2INTEGERPOWER);
		       if (value < 0)
			 value = 0;
		       
		       if (value > 255) 
			 value = 255;
		       
		       *(output_p++) = (uint8_t) value;
		    }
		  
	       }
	  }
     }


/* *INDENT-ON* */
  mjpeg_debug ("End of cubic_scale_interlaced\n");
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
    return ((long int)
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
  if (fabs (x) < 2)
    return ((long int)
	    floor (0.5 +
		   (((-B - 6.0 * C) * fabs (x) * fabs (x) * fabs (x) +
		     (6.0 * B + 30.0 * C) * fabs (x) * fabs (x) +
		     (-12.0 * B - 48.0 * C) * fabs (x) + (8.0 * B +
							  24.0 * C)) /
		    6.0) * multiplicative));
//   fprintf(stderr,"Bizarre!\n");
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
  return (0);
}

// *************************************************************************************

// *************************************************************************************
int
padding_interlaced (uint8_t * padded_top, uint8_t * padded_bottom,
		    uint8_t * input, unsigned int half)
{
  // This padding function requires output_interlaced!=0
  unsigned int local_input_useful_width = input_useful_width >> half;
  unsigned int local_input_useful_height = input_useful_height >> half;
  unsigned int local_padded_width = local_input_useful_width + 3;
  unsigned int local_padded_height = local_input_useful_height / 2 + 3;
  unsigned int local_input_width = input_width >> half;
  unsigned int line;

  // PADDING
  // Content Copy with 1 pixel offset on the left and top
  for (line = 0; line < local_input_useful_height / 2; line++)
    {
      memcpy (padded_top + 1 + (line + 1) * local_padded_width,
	      input + 2 * line * local_input_width, local_input_useful_width);
      memcpy (padded_bottom + 1 + (line + 1) * local_padded_width,
	      input + (2 * line + 1) * local_input_width,
	      local_input_useful_width);
    }

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
  return (0);

}

// *************************************************************************************

