/* quantize.c, quantization / inverse quantization                          */

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

#include <stdio.h>
#include <math.h>
#include <fenv.h>
#include "config.h"
#include "global.h"
#include "cpu_accel.h"
#include "simd.h"

  /* Unpredictable branches suck on modern CPU's... */

#define signmask(x) (((int)x)>>fabsshift)
static inline int samesign(int x, int y)
{
	return (y+(signmask(x) & -(y<<1)));
}

/* Global function pointers for SIMD-dependent functions */
int (*pquant_non_intra)(pict_data_s *picture, int16_t *src, int16_t *dst,
						int mquant, int *nonsat_mquant);
int (*pquant_weight_coeff_sum)(int16_t *blk, uint16_t*i_quant_mat );

/* Local functions pointers for SIMD-dependent functions */

static void (*piquant_non_intra_m1)(int16_t *src, int16_t *dst,  uint16_t *quant_mat);


static int quant_weight_coeff_sum( int16_t *blk, uint16_t * i_quant_mat );
static void iquant_non_intra_m1(int16_t *src, int16_t *dst, uint16_t *quant_mat);
static int quant_non_intra(pict_data_s *picture, int16_t *src, int16_t *dst,int mquant,
							int *nonsat_mquant);

/*
  Initialise quantization routines.
  Currently just setting up MMX routines if available...
 */

void init_quantizer()
{
  int flags;
  flags = cpu_accel();
#ifdef X86_CPU
  if( (flags & ACCEL_X86_MMX) != 0 ) /* MMX CPU */
	{
		if( flags & ACCEL_X86_3DNOW )
		{
			fprintf( stderr, "SETTING 3DNOW for QUANTIZER!\n");
			pquant_non_intra = quant_non_intra_3dnow;
			pquant_weight_coeff_sum = quant_weight_coeff_sum_mmx;
			piquant_non_intra_m1 = iquant_non_intra_m1_sse;		
		}
		else if ( (flags & ACCEL_X86_MMXEXT) != 0 )
		{
			fprintf( stderr, "SETTING EXTENDED MMX for QUANTIZER!\n");
			pquant_non_intra = quant_non_intra_mmx;
			pquant_weight_coeff_sum = quant_weight_coeff_sum_mmx;
			piquant_non_intra_m1 = iquant_non_intra_m1_sse;
		}
		else
		{
			fprintf( stderr, "SETTING MMX for QUANTIZER!\n");
			pquant_non_intra = quant_non_intra_mmx;
			pquant_weight_coeff_sum = quant_weight_coeff_sum_mmx;
			piquant_non_intra_m1 = iquant_non_intra_m1_mmx;
		}
	}
  else
#endif
	{
	  pquant_non_intra = quant_non_intra;	  
	  pquant_weight_coeff_sum = quant_weight_coeff_sum;
	  piquant_non_intra_m1 = iquant_non_intra_m1;
	}
}

/*
 *
 * Computes the next quantisation up.  Used to avoid saturation
 * in macroblock coefficients - common in MPEG-1 - which causes
 * nasty artefacts.
 *
 * NOTE: Does no range checking...
 *
 */
 

int next_larger_quant( pict_data_s *picture, int quant )
{
	if( picture->q_scale_type )
		{
			if( map_non_linear_mquant[quant]+1 > 31 )
				return quant;
			else
				return non_linear_mquant_table[map_non_linear_mquant[quant]+1];
		}
	else 
		{
			if( quant+2 > 31 )
				return quant;
			else 
				return quant+2;
		}
	
}

/* 
 * Quantisation for intra blocks using Test Model 5 quantization
 *
 * this quantizer has a bias of 1/8 stepsize towards zero
 * (except for the DC coefficient)
 *
	PRECONDITION: src dst point to *disinct* memory buffers...
		          of block_count *adjact* int16_t[64] arrays... 
 *
 * RETURN: 1 If non-zero coefficients left after quantisaiont 0 otherwise
 */

void quant_intra(
	pict_data_s *picture,
	int16_t *src, 
	int16_t *dst,
	int mquant,
	int *nonsat_mquant
	)
{
  int16_t *psrc,*pbuf;
  int i,comp;
  int x, y, d;
  int clipping;
  int clipvalue  = dctsatlim;
  uint16_t *quant_mat = intra_q;


  /* Inspired by suggestion by Juan.  Quantize a little harder if we clip...
   */

  do
	{
	  clipping = 0;
	  pbuf = dst;
	  psrc = src;
	  for( comp = 0; comp<block_count && !clipping; ++comp )
	  {
		x = psrc[0];
		d = 8>>picture->dc_prec; /* intra_dc_mult */
		pbuf[0] = (x>=0) ? (x+(d>>1))/d : -((-x+(d>>1))/d); /* round(x/d) */


		for (i=1; i<64 ; i++)
		  {
			x = psrc[i];
			d = quant_mat[i];
#ifdef ORIGINAL_CODE
			y = (32*(x >= 0 ? x : -x) + (d>>1))/d; /* round(32*x/quant_mat) */
			d = (3*mquant+2)>>2;
			y = (y+d)/(2*mquant); /* (y+0.75*mquant) / (2*mquant) */

			/* clip to syntax limits */
			if (y > 255)
			  {
				if (mpeg1)
				  y = 255;
				else if (y > 2047)
				  y = 2047;
			  }
#else
			/* RJ: save one divide operation */
			y = (32*fastabs(x) + (d>>1) + d*((3*mquant+2)>>2))/(quant_mat[i]*2*mquant);
			if ( y > clipvalue )
			  {
				clipping = 1;
				mquant = next_larger_quant( picture, mquant );
				break;
			  }
#endif
		  
		  	pbuf[i] = samesign(x,y);
		  }
		pbuf += 64;
		psrc += 64;
	  }
			
	} while( clipping );
  *nonsat_mquant = mquant;
}


/*
 * Quantisation matrix weighted Coefficient sum fixed-point
 * integer with low 16 bits fractional...
 * To be used for rate control as a measure of dct block
 * complexity...
 *
 */

int quant_weight_coeff_sum( int16_t *blk, uint16_t * i_quant_mat )
{
  int i;
  int sum = 0;
   for( i = 0; i < 64; i+=2 )
	{
		sum += abs((int)blk[i]) * (i_quant_mat[i]) + abs((int)blk[i+1]) * (i_quant_mat[i+1]);
	}
    return sum;
  /* In case you're wondering typical average coeff_sum's for a rather
  	noisy video are around 20.0.  */
}


							     
/* 
 * Quantisation for non-intra blocks using Test Model 5 quantization
 *
 * this quantizer has a bias of 1/8 stepsize towards zero
 * (except for the DC coefficient)
 *
 * A.Stevens 2000: The above comment is nonsense.  Only the intra quantiser does
 * this.  This one just truncates with a modest bias of 1/(4*quant_matrix_scale)
 * to 1.
 *
 *	PRECONDITION: src dst point to *disinct* memory buffers...
 *	              of block_count *adjacent* int16_t[64] arrays...
 *
 * RETURN: A bit-mask of block_count bits indicating non-zero blocks (a 1).
 *
 * TODO: A candidate for use of efficient abs and "samesign". If only gcc understood
 * PPro conditional moves...
 */
																							     											     
int quant_non_intra(
	pict_data_s *picture,
	int16_t *src, int16_t *dst,
	int mquant,
	int *nonsat_mquant)
{
	int i;
	int x, y, d;
	int nzflag;
	int coeff_count;
	int clipvalue  = dctsatlim;
	int flags = 0;
	int saturated = 0;
	uint16_t *quant_mat = inter_q;
	
	coeff_count = 64*block_count;
	flags = 0;
	nzflag = 0;
	for (i=0; i<coeff_count; ++i)
	{
		if( (i%64) == 0 )
		{
			nzflag = (nzflag<<1) | !!flags;
			flags = 0;
			  
		}
		/* RJ: save one divide operation */

		x = (src[i] >= 0 ? src[i] : -src[i]);
		d = (int)quant_mat[(i&63)]; 
		y = (32*x + (d>>1))/(d*2*mquant);
		if ( y > clipvalue )
		{
			if( saturated )
			{
				y = clipvalue;
			}
			else
			{
				int new_mquant = next_larger_quant( picture, mquant );
				if( new_mquant != mquant )
					mquant = new_mquant;
				else
				{
					saturated = 1;
				}
				i=0;
				nzflag =0;
				continue;
			}
		}
		dst[i] = (src[i] >= 0 ? y : -y);
		flags |= dst[i];
	}
	nzflag = (nzflag<<1) | !!flags;

    *nonsat_mquant = mquant;
    return nzflag;
}

/* MPEG-1 inverse quantization */
static void iquant1_intra(int16_t *src, int16_t *dst, int dc_prec, int mquant)
{
  int i, val;
  uint16_t *quant_mat = intra_q;

  dst[0] = src[0] << (3-dc_prec);
  for (i=1; i<64; i++)
  {
    val = (int)(src[i]*quant_mat[i]*mquant)/16;

    /* mismatch control */
    if ((val&1)==0 && val!=0)
      val+= (val>0) ? -1 : 1;

    /* saturation */
    dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
  }
}


/* MPEG-2 inverse quantization */
void iquant_intra(int16_t *src, int16_t *dst, int dc_prec, int mquant)
{
  int i, val, sum;

  if (mpeg1)
    iquant1_intra(src,dst,dc_prec, mquant);
  else
  {
    sum = dst[0] = src[0] << (3-dc_prec);
    for (i=1; i<64; i++)
    {
      val = (int)(src[i]*intra_q[i]*mquant)/16;
      sum+= dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
    }

    /* mismatch control */
    if ((sum&1)==0)
      dst[63]^= 1;
  }
}


static void iquant_non_intra_m1(int16_t *src, int16_t *dst,  uint16_t *quant_mat)
{
  int i, val;

#ifndef ORIGINAL_CODE

  for (i=0; i<64; i++)
  {
    val = src[i];
    if (val!=0)
    {
      val = (int)((2*val+(val>0 ? 1 : -1))*quant_mat[i])/32;

      /* mismatch control */
      if ((val&1)==0 && val!=0)
        val+= (val>0) ? -1 : 1;
    }

    /* saturation */
     dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
 }
#else
  
  for (i=0; i<64; i++)
  {
    val = abs(src[i]);
    if (val!=0)
    {
		val = ((val+val+1)*quant_mat[i]) >> 5;
		/* mismatch control */
		val -= (~(val&1))&(val!=0);
		val = fastmin(val, 2047); /* Saturation */
    }
	dst[i] = samesign(src[i],val);
	
  }
  
#endif
}




void iquant_non_intra(int16_t *src, int16_t *dst, int mquant )
{
  int i, val, sum;
  uint16_t *quant_mat;
  if (mpeg1)
    (*piquant_non_intra_m1)(src,dst,inter_q_tbl[mquant]);
  else
  {
	  sum = 0;
#ifdef ORIGINAL_CODE

	  for (i=0; i<64; i++)
	  {
		  val = src[i];
		  if (val!=0)
			  
			  val = (int)((2*val+(val>0 ? 1 : -1))*inter_q[i]*mquant)/32;
		  sum+= dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
	  }
#else
	  quant_mat = inter_q_tbl[mquant];
	  for (i=0; i<64; i++)
	  {
		  val = src[i];
		  if( val != 0 )
		  {
			  val = fastabs(val);
			  val = (int)((val+val+1)*quant_mat[i])>>5;
			  val = fastmin( val, 2047);
			  sum += val;
		  }
		  dst[i] = val;
	  }
#endif

    /* mismatch control */
    if ((sum&1)==0)
      dst[63]^= 1;
  }
}
