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


  /* Unpredictable branches suck on modern CPU's... */
#define fabsshift ((8*sizeof(unsigned int))-1)
#define fastabs(x) (((x)-(((unsigned int)(x))>>fabsshift)) ^ ((x)>>fabsshift))
#define signmask(x) (((int)x)>>fabsshift)
#define samesign(x,y) (y+(signmask(x) & -(y<<1)))

/*
 *
 * Computes the next quantisation up.  Used to avoid saturation
 * in macroblock coefficients - common in MPEG-1 - which causes
 * nasty artefacts.
 *
 * NOTE: Does no range checking...
 *
 */
 

static int next_larger_quant( int quant )
{
	if( q_scale_type )
		{
			return non_linear_mquant_table[map_non_linear_mquant[quant]+1];
		}
	else 
		{
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
		          of block_count *adjact* short[64] arrays... 
 *
 */

void quant_intra(src,dst,dc_prec,quant_mat,i_quant_mat, mquant, nonsat_mquant)
short *src, *dst;
int dc_prec;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
int *nonsat_mquant;
{
  short *psrc,*pbuf;
  int i,comp;
  int x, y, d;
  int clipping;
  int clipvalue  = mpeg1 ? 255 : 2047;


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
		d = 8>>dc_prec; /* intra_dc_mult */
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
				mquant = next_larger_quant( mquant );
				if( ! quiet )
					fprintf( stderr, "I" );
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

#if !defined(SSE) && !defined(MMX)

/*
 * Quantisation matrix weighted Coefficient sum fixed-point
 * integer with low 16 bits fractional...
 * To be used for rate control as a measure of dct block
 * complexity...
 *
 */

int quant_weight_coeff_sum( short *blk, unsigned short * i_quant_mat )
{
  int i;
  int sum = 0;
   for( i = 0; i < 64; i+=2 )
	{
		sum += abs((int)blk[i]) * (i_quant_mat[i]) + abs((int)blk[i+1]) * (i_quant_mat[i+1]);
	}
    return sum;
  /* In case you're wondering typical average coeff_sum's for a rather noisy video
  	are around 20.0.
	*/
}
#endif

#ifdef DEBUG

int quant_non_intra_inv(src,dst,quant_mat,i_quant_mat,imquant, mquant)
short *src, *dst;
unsigned short *quant_mat;
short *i_quant_mat;  /* 2^16 / quant_mat */
int imquant;      /* 2^16 / 2*mquant */
int mquant;
{
  int i;
  int x, y;
  int nzflag;
  int clipping;

	  clipping = 0;

	  for (i=0; i<64; i++)
		{
			x = (int)src[i];
			y = ( (((abs(x)<<5)+ (quant_mat[i]>>1))) * i_quant_mat[i])>>(IQUANT_SCALE_POW2);
			/* y1 is  2*divisor i.e. 1!! guard bits */
			y = ((y<<4)+(mquant>>1))*imquant;
			y = y >> (IQUANT_SCALE_POW2+5);
		  clipping |= ((y > 255) && (mpeg1 || (y > 2047)));
			nzflag |= (y != 0);
			dst[i] = samesign(x,y);
		}

  return clipping;
}
#endif
							     
/* 
 * Quantisation for non-intra blocks using Test Model 5 quantization
 *
 * this quantizer has a bias of 1/8 stepsize towards zero
 * (except for the DC coefficient)
 *
	PRECONDITION: src dst point to *disinct* memory buffers...
	              of block_count *adjact* short[64] arrays...
 *
 */

																							     											     
int quant_non_intra(src,dst,quant_mat, i_quant_mat, mquant, nonsat_mquant)
short *src, *dst;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
int *nonsat_mquant;
{
  unsigned short *psrc, *pdst;
  int i,comp;
  int x, y, d;
  int nzflag;
  int coeff_count;
  int clipvalue  = mpeg1 ? 255 : 2047;
  int imquant;
  int flags;

    	/* If available use the fast MMX quantiser.  It returns
		flags to signal if coefficients are outside its limited range or
		saturation would occur with the specified quantisation
		factor
		Top 16 bits - non zero quantised coefficient present
		Bits 8-15   - Saturation occurred
		Bits 0-7    - Coefficient out of range.
		*/

#if (defined(MMX) || defined(SSE))

  pdst = dst;
  psrc = src;
  comp = 0; 
  do
  {
	imquant = (IQUANT_SCALE/mquant);
	flags = quantize_ni_mmx( pdst, psrc, quant_mat, i_quant_mat, 
							 imquant, mquant, clipvalue );
	nzflag = flags & 0xffff0000;
	
	/* If we're saturating simply bump up quantization and start from scratch...
	*/

	if( (flags & 0xff00) != 0 )
	{
	  mquant = next_larger_quant( mquant );
	  comp = 0; 
	  pdst = dst;
	  psrc = src;
	}
	else
	{
	  ++comp;
	  pdst += 64;
	  psrc +=64;
	}
	/* Fall back to 32-bit(or better - if some hero(ine) made this work on
	  non 32-bit int machines ;-)) if out of dynamic range for MMX...
	  */
  }
  while( comp < block_count && (flags& 0xff00) != 0 && (flags & 0xff) == 0 );
  
  if( (flags & 0xff) != 0)
#endif  
  {
	coeff_count = 64*block_count;
	fprintf( stderr, "O" );


	nzflag = 0;
	pdst = dst;
	psrc = src;		
	for (i=0; i<coeff_count; ++i)
	{
	  /* RJ: save one divide operation */

	  x = abs(*psrc);
	  d = quant_mat[(i&63)]; 
	  y = (32*abs(x) + (d>>1))/(d*2*mquant);
	  nzflag |= (*pdst=samesign(src[i],y));
	  if (y > clipvalue)
	  {
		  fprintf( stderr, "C");
		  mquant = next_larger_quant( mquant );
		  i=0;
		  pdst = dst;
		  psrc = src;
		  continue;		
	  }
	  else
	  {
	  	++pdst; 
		++psrc;
	  }
   }

#if (defined(MMX) || defined(SSE))
  }
#endif

  *nonsat_mquant = mquant;
  return !!nzflag;
}

/* MPEG-1 inverse quantization */
static void iquant1_intra(src,dst,dc_prec,quant_mat,i_quant_mat, mquant)
short *src, *dst;
int dc_prec;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
{
  int i, val;

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
void iquant_intra(src,dst,dc_prec,quant_mat,i_quant_mat,mquant)
short *src, *dst;
int dc_prec;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
{
  int i, val, sum;

  if (mpeg1)
    iquant1_intra(src,dst,dc_prec,quant_mat,i_quant_mat, mquant);
  else
  {
    sum = dst[0] = src[0] << (3-dc_prec);
    for (i=1; i<64; i++)
    {
      val = (int)(src[i]*quant_mat[i]*mquant)/16;
      sum+= dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
    }

    /* mismatch control */
    if ((sum&1)==0)
      dst[63]^= 1;
  }
}




static void iquant1_non_intra(src,dst,quant_mat,i_quant_mat,mquant)
short *src, *dst;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
{
  int i, val;

  for (i=0; i<64; i++)
  {
    val = src[i];
    if (val!=0)
    {
      val = (int)((2*val+(val>0 ? 1 : -1))*quant_mat[i]*mquant)/32;

      /* mismatch control */
      if ((val&1)==0 && val!=0)
        val+= (val>0) ? -1 : 1;
    }

    /* saturation */
    dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
  }
}


void iquant_non_intra(src,dst,quant_mat,i_quant_mat, mquant)
short *src, *dst;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
{
  int i, val, sum;

  if (mpeg1)
    iquant1_non_intra(src,dst,quant_mat,i_quant_mat, mquant);
  else
  {
    sum = 0;
    for (i=0; i<64; i++)
    {
      val = src[i];
      if (val!=0)
        val = (int)((2*val+(val>0 ? 1 : -1))*quant_mat[i]*mquant)/32;
      sum+= dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
    }

    /* mismatch control */
    if ((sum&1)==0)
      dst[63]^= 1;
  }
}
