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

/* Test Model 5 quantization
 *
 * this quantizer has a bias of 1/8 stepsize towards zero
 * (except for the DC coefficient)
 */
int quant_intra(src,dst,dc_prec,quant_mat,i_quant_mat, mquant)
short *src, *dst;
int dc_prec;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
{
  int i;
  int x, y, d;
  int clipping;
  short tmp[64];	/* We buffer result in case we need to adjust quantization
					   to avoid clipping */


  /* Inspired by suggestion by Juan.  Quantize a little harder if we clip...
   */

  do
	{
	  clipping = 0;
	  x = src[0];
	  d = 8>>dc_prec; /* intra_dc_mult */
	  tmp[0] = (x>=0) ? (x+(d>>1))/d : -((-x+(d>>1))/d); /* round(x/d) */


	  for (i=1; i<64; i++)
		{
		  x = src[i];
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
		  if ( (y > 255) && (mpeg1 || (y > 2047)) )
			{
			  clipping = 1;
			  mquant += 2;
			  break;
			}
#endif
		  
		  tmp[i] = samesign(x,y);

		}
	} while( clipping );
  
  memcpy( dst, tmp, 64*sizeof(short) );
  return mquant;
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

int quant_non_intra_inv(src,dst,quant_mat,i_quant_mat,imquant, mquant, pmquant)
short *src, *dst;
unsigned short *quant_mat;
short *i_quant_mat;  /* 2^16 / quant_mat */
int imquant;      /* 2^16 / 2*mquant */
int mquant;
int *pmquant;
{
  int i;
  int x, y;
  int nzflag;
  int clipping;
  short tmp[64];	/* We buffer result in case we need to adjust quantization
					   to avoid clipping */

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
								     
static short quant_buf[64];
																							     											     
int quant_non_intra(src,dst,quant_mat, i_quant_mat, mquant, pmquant)
short *src, *dst;
unsigned short *quant_mat;
unsigned short *i_quant_mat;
int mquant;
int *pmquant;
{
  int i;
  int x, y, d;
  int nzflag;
  int clipping = 0;
  int clipvalue  = mpeg1 ? 255 : 2047;
  int imquant = (IQUANT_SCALE/mquant);

 /*
  int tstclip;
  short tst[64];

 

  tstclip = quant_non_intra_inv( src,tst,quant_mat, i_quant_mat,imquant, mquant, pmquant);
  tstclip = quantize_ni_mmx( tst, src, quant_mat, i_quant_mat, 
  						(IQUANT_SCALE/mquant), mquant, clipvalue ) & 0xffff;
  */

  /* MMX Quantizer currently disabled: occasionally causes artefacts due to
  	 a somewhat different rounding behaviour from the reference code...
  */
#if (defined(MMX) || defined(SSE))
	  int  ret = quantize_ni_mmx( quant_buf, src, quant_mat, i_quant_mat, 
	  			                 imquant, mquant, clipvalue );
	  nzflag = ret & 0xffff0000;
	  
	  /* We had a saturation problem with the MMX code revert to the tougher integer C
	  	code
		*/
	  if( ret & 0xffff )
	  {
	  	printf( "MMX saturation - reversion\n" );
#endif

 	 	do
		{

		  clipping = 0;
		  nzflag = 0;
		  for (i=0; i<64; i++)
		  {
			/* RJ: save one divide operation */
			/* AS: Lets make this a little more accurate... */
			x = abs(src[i]);
			d = quant_mat[i]; 
			/* N.b. accurate would be: y = (int)rint(32.0*((double)x)/((double)(d*2*mquant))); */
			/* Code below does *not* compute this always! */
			y = (32*abs(x) + (d>>1))/(d*2*mquant);
			if (y > clipvalue)
				y = clipvalue;
			nzflag |= (quant_buf[i] = samesign(src[i],y));
			
		  }
			  /*
				  TODO: BUG: THis *has* to be bogus in the event of non-linear mquant
				  values being chosen...
			if( clipping )
	  		mquant += 2;
					  */
		
	  }
  	while( clipping );
#if (defined(MMX) || defined(SSE))
  }
#endif
	/* TODO:  THis is bogus until it properly rescales in the non-linear case....
	for now we just do simple saturation...
  *pmquant = mquant;
  	*/
  
  memcpy( dst, quant_buf, 64*sizeof(short) );

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
