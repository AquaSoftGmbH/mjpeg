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

extern void	quantize_ni_mmx( short *, short *, unsigned char *,  int, int );

static void iquant1_intra _ANSI_ARGS_((short *src, short *dst,
  int dc_prec, unsigned char *quant_mat, int mquant));
static void iquant1_non_intra _ANSI_ARGS_((short *src, short *dst,
  unsigned char *quant_mat, int mquant));

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
int quant_intra(src,dst,dc_prec,quant_mat,mquant)
short *src, *dst;
int dc_prec;
unsigned char *quant_mat;
int mquant;
{
  int i;
  int x, y, d;
  int clipping;
  short tmp[64];	/* We buffer result in case we need to adjust quantization
					   to avoid clipping */
	short res[64];
	float *sres = (float *)res;

	/*	
	memcpy( tmp, src, 64*sizeof(short) );
	printf( "TESTING SSE ASSEMBLER ... " );
	for( i = 0; i < 4; ++i )
		printf( "%04d / %03d ", tmp[i], quant_mat[i] );
		printf( "\n" );
	quantize_ni_sse( res, tmp, quant_mat, mquant, 255 ); 
		
	exit(0);
	*/

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

#if 0
		  /* this quantizer is virtually identical to the above */
		  if (x<0)
			x = -x;
		  d = mquant*quant_mat[i];
		  y = (16*x + ((3*d)>>3)) / d;
		  dst[i] = (src[i]<0) ? -y : y;
#endif
		}
	} while( clipping );
  
  memcpy( dst, tmp, 64*sizeof(short) );
  return mquant;
}

/*
 * Quantisation matrix weighted Coefficient sum
 * To be used for rate control as a measure of dct block
 * complexity...
 *
 */

double quant_weight_coeff_sum( short *blk, int * i_quant_mat )
{
  int i;
 /* double dsum = 0.0; */
  int sum = 0;
 /* int sum = 64000; */   /* TODO: This base weight ought to be exposed.... */
   for( i = 0; i < 64; i+=2 )
	{
		sum += abs((int)blk[i]) * (i_quant_mat[i]) + abs((int)blk[i+1]) * (i_quant_mat[i+1]);
  /*
  		dsum += fabs((double)blk[i]) / ((double) quant_mat[i]) +
	 		   fabs((double)blk[i+1]) / ((double) quant_mat[i+1]); 

	  register int abs1, abs2;
	  abs1 = (fastabs(blk[i]) <<(4+8)) / quant_mat[i];
	  abs2 = (fastabs(blk[i+1]) <<(4+8)) ;
	  sum += abs1+abs2/ quant_mat[i+1];
	 */
	}

    return ((double) sum) / ((double) ( 1 << (16)));
  /* In case you're wondering typical average coeff_sum's for a rather noisy video
  	are around 20.0.
	*/
}

#ifdef FAST_FLOATING_POINT_VERSION_FOR_ALPHA_OWNERS
int quant_non_intra_inv(src,dst,iquant_mat,imquant_div16, pmquant)
short *src, *dst;
float *iquant_mat;
float imquant_div16;
int *pmquant;
{
  int i;
  int x, y, d;
  int x1, y1, d1, x2, y2, d2;
  int nzflag;
  int clipping;
  short tmp[64];	/* We buffer result in case we need to adjust quantization
					   to avoid clipping */

#ifdef FE_TOWARDZERO
		fesetround( FE_TOWARDZERO );
#else
#error THIS CODE ASSUMES THAT YOU CAN ROUND TO ZERO... 
#endif
  
  /* Inspired by suggestion by Juan.  Quantize a little harder if we clip...
   */
 do
	{

	  clipping = 0;
	  nzflag = 0;
	  for (i=0; i<64; i++)
		{
			register int y1;
			y1 = (int) ( ((float)src[i]) * iquant_mat[i] * imquant_div16 );
		  clipping |= ((y1 > 255) && (mpeg1 || (y1 > 2047)));
			nzflag |= (y1 != 0);
			dst[i] = y1;
		}
		if( clipping )
		{
			imquant_div16 /= 2.0;
			*pmquant *= 2;
		}
	}
	while( clipping );
	memcpy (dst, tmp, sizeof(short)*64 );
  return nzflag;
}

#endif

int quant_non_intra_inv(src,dst,quant_mat,iquant_mat,imquant, pmquant)
short *src, *dst;
unsigned char *quant_mat;
int *iquant_mat;  /* 2^16 / quant_mat */
int imquant;      /* 2^16 / 2*mquant */
int *pmquant;
{
  int i;
  int x, y, d;
  int x1, y1, d1, x2, y2, d2;
  int nzflag;
  int clipping;
  short tmp[64];	/* We buffer result in case we need to adjust quantization
					   to avoid clipping */

  /* Inspired by suggestion by Juan.  Quantize a little harder if we clip...
   */
 do
	{

	  clipping = 0;
	  nzflag = 0;
	  for (i=0; i<64; i++)
		{
			x1 = (int)src[i];
			y1 = (((  (((abs(x1)<<5)+ (quant_mat[i]>>1))  * i_inter_q[i] )>>14) * imquant)>>18);
		  clipping |= ((y1 > 255) && (mpeg1 || (y1 > 2047)));
			nzflag |= (y1 != 0);
			tmp[i] = samesign(x1,y1);
		}

		if( clipping )
		{
			imquant /= 2;
			*pmquant *= 2;
		}
	}
	while( clipping );
  memcpy (dst, tmp, sizeof(short)*64 );
  return nzflag;
}

int quant_non_intra(src,dst,quant_mat,mquant, pmquant)
short *src, *dst;
unsigned char *quant_mat;
int mquant;
int *pmquant;
{
  int i;
  int x, y, d;
  int x1, y1, d1, x2, y2, d2;
  int nzflag;
  int clipping;
#ifdef DEBUG_TESTING	
  short tmp[64];	/* We buffer result in case we need to adjust quantization
					   to avoid clipping */
	short tst[64];
	int mismatch;
	int clipvalue  = mpeg1 ? 255 : 2047;
	int scaled_mq   = mquant*4;
  do
	{
	  clipping = 0;
	  nzflag = 0;
	  for (i=0; i<64; i++)
		{
#ifdef ORIG_CODE
		  x = src[i];
		  d = quant_mat[i]; 

		  y = (32*(x>=0 ? x : -x) + (d>>1))/d; /* round(32*x/d*2*quant_mat) */
		  y /= (2*mquant);
		  /* clip to syntax limits */
		  if (y > 255)
			{
			  if (mpeg1)
				y = 255;
			  else if (y > 2047)
				y = 2047;
			}
			dst[i] = x < 0 ? -y : y;
#else
		  /* RJ: save one divide operation 
		  x = src[i];
		  d = quant_mat[i]; 
		  y = (32*fastabs(x) + (d>>1))/(d*2*mquant);
		  clipping |= ((y > 255) & (mpeg1 | (y > 2047)));
		  nzflag |= ((tmp[i] = samesign(x,y)) != 0);
		  */
		  x1 = src[i];
		  d1 = quant_mat[i];
		  y1 = (abs(x1)<<6) + d1;

		  x2 = src[i+1];
		  y1 /= d1*scaled_mq;
		  clipping |= (y1 > clipvalue);
 		  nzflag |= ((tmp[i] = samesign(x1,y1))!=0);
		  
		/*
		  d2 = quant_mat[i+1];
		  y2 = (abs(x2)<<6) + d2;  
		  y2 /= d2*scaled_mq;
	 	  clipping |= (y2 > clipvalue);
		  nzflag |= ((tmp[i+1] = samesign(x2,y2))!=0);
		*/

		}
	  mquant += !!clipping;
#endif
	}
  while( clipping );
#endif
  return quant_non_intra_inv( src, dst, quant_mat, i_inter_q, ((1<<15) / mquant), pmquant );
#ifdef DEBUG_TESTING
	mismatch = 0;
	for( i = 0; i < 64; ++i )
		{
				mismatch += abs(tst[i] - tmp[i]);
		}
	if( mismatch > 4 )
	{
		printf( "mquant = %d \n", mquant );
		for( i = 0; i < 64; ++i )
		{
			/* rint( ((double)src[i]) * iquant_mat[i] * imquant_div16 ); */
			 x1 = (int)src[i];
		 	y1 = ((  (((abs(x1)<<5)+ (quant_mat[i]>>1))  * i_inter_q[i] )>>16) * ((1<<15) / mquant ))>>16;

			if( tst[i] != tmp[i] )
				printf( "I=%d C=%d D=%d T=%d X=%d %d %g %d %d %d\n", i, src[i], quant_mat[i], tmp[i],tst[i],
					i_inter_q[i], ((double)(1<<16)*16) / (double)i_inter_q[i], (1<<15) / mquant,
					y1,
					samesign(x1,y1)
			   );
		}
		exit(0);
	}

	*pmquant = mquant;
  memcpy( dst, tmp, 64*sizeof(short) );

  return nzflag;
#endif
}

/* MPEG-2 inverse quantization */
void iquant_intra(src,dst,dc_prec,quant_mat,mquant)
short *src, *dst;
int dc_prec;
unsigned char *quant_mat;
int mquant;
{
  int i, val, sum;

  if (mpeg1)
    iquant1_intra(src,dst,dc_prec,quant_mat,mquant);
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

void iquant_non_intra(src,dst,quant_mat,mquant)
short *src, *dst;
unsigned char *quant_mat;
int mquant;
{
  int i, val, sum;

  if (mpeg1)
    iquant1_non_intra(src,dst,quant_mat,mquant);
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

/* MPEG-1 inverse quantization */
static void iquant1_intra(src,dst,dc_prec,quant_mat,mquant)
short *src, *dst;
int dc_prec;
unsigned char *quant_mat;
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

static void iquant1_non_intra(src,dst,quant_mat,mquant)
short *src, *dst;
unsigned char *quant_mat;
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
