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
#include "config.h"
#include "global.h"

static void iquant1_intra _ANSI_ARGS_((short *src, short *dst,
  int dc_prec, unsigned char *quant_mat, int mquant));
static void iquant1_non_intra _ANSI_ARGS_((short *src, short *dst,
  unsigned char *quant_mat, int mquant));

  /* Unpredictable branches suck on modern CPU's... */
#define fabsshift ((8*sizeof(unsigned int))-1)
#define fastabs(x) (((x)-(((unsigned int)(x))>>fabsshift)) ^ ((x)>>fabsshift))
#define signmask(x) (((int)x)>>fabsshift)
#define samesign(x,y) ((signmask(x) & -y) | ((signmask(x)^-1) & y))


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

double quant_weight_coeff_sum( short *blk, unsigned char * quant_mat )
{
  int i;
  double sum = 2000.0;
 /* int sum = 2000; */   /* TODO: This base weight ought to be exposed.... */
   for( i = 0; i < 64; i+=2 )
	{
		sum += ((double)(fastabs(blk[i]) * 16)) / ((double) quant_mat[i]) +
	  ((double)(fastabs(blk[i+1]) * 16)) / ((double) quant_mat[i+1]);
/*
	  register int abs1, abs2;
	  abs1 = (fastabs(blk[i]) <<(4+8)) / quant_mat[i];
	  abs2 = (fastabs(blk[i+1]) <<(4+8)) ;
	  sum += abs1+abs2/ quant_mat[i+1];
	 */
	}
  /*
	sum += ((double)(fastabs(blk[i]) * 16)) / ((double) quant_mat[i]);
 */
	  
  return (double) (sum / 256);
}

int quant_non_intra(src,dst,quant_mat,mquant, mquant_ret)
short *src, *dst;
unsigned char *quant_mat;
int mquant;
int *mquant_ret;
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
	  for (i=0; i<64; i+=2)
		{
#ifdef ORIG_CODE
		  x = src[i];
		  d = quant_mat[i]; 

		  y = (32*(x>=0 ? x : -x) + (d>>1))/d; /* round(32*x/quant_mat) */
		  y /= (2*mquant);
		  /* clip to syntax limits */
		  if (y > 255)
			{
			  if (mpeg1)
				y = 255;
			  else if (y > 2047)
				y = 2047;
			}
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
		  y1 = (fastabs(x1)<<5) + (d1>>1);

		  x2 = src[i+1];
		  y1 /= (d1<<1)*mquant;

		  d2 = quant_mat[i+1];
		  y2 = (fastabs(x2)<<5) + (d2>>1);  
		  clipping |= ((y1 > 255) & (mpeg1 | (y1 > 2047)));

		  y2 /= (d2<<1)*mquant;
 		  nzflag |= ((tmp[i] = samesign(x1,y1))!=0);
	 	  clipping |= ((y2 > 255) & (mpeg1 | (y2 > 2047)));
		  nzflag |= ((tmp[i+1] = samesign(x2,y2))!=0);

		}
	  mquant += !!clipping;
#endif
	}
  while( clipping );

  memcpy( dst, tmp, 64*sizeof(short) );
  return nzflag;
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
