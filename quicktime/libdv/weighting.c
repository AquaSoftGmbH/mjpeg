/* 
 *  weighting.c
 *
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  decoder.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */

#include <math.h>
#include "weighting.h"

dv_coeff_t preSC[64] __attribute__ ((aligned (32))) = {
16384,22725,21407,19266,16384,12873,8867,4520,
22725,31521,29692,26722,22725,17855,12299,6270,
21407,29692,27969,25172,21407,16819,11585,5906,
19266,26722,25172,22654,19266,15137,10426,5315,
16384,22725,21407,19266,16384,12873,8867,4520,
12873,17855,16819,15137,25746,20228,13933,7103,
17734,24598,23170,20853,17734,13933,9597,4892,
18081,25080,23624,21261,18081,14206,9785,4988
};

static double W[8];
static dv_coeff_t dv_weight_inverse_88_matrix[64];

double dv_weight_inverse_248_matrix[64];

static inline double CS(int m) {
  return cos(((double)m) * M_PI / 16.0);
}

void weight_88_inverse_float(double *block);

void weight_init(void) {
  double temp[64];
  int i, z, x;
  double dv_weight_bias_factor = (double)(1UL << DV_WEIGHT_BIAS);

  W[0] = 1.0;
  W[1] = CS(4) / (4.0 * CS(7) * CS(2));
  W[2] = CS(4) / (2.0 * CS(6));
  W[3] = 1.0 / (2 * CS(5));
  W[4] = 7.0 / 8.0;
  W[5] = CS(4) / CS(3); 
  W[6] = CS(4) / CS(2); 
  W[7] = CS(4) / CS(1);

  for (i=0;i<64;i++)
    temp[i] = 1.0;
  weight_88_inverse_float(temp);

  for (i=0;i<64;i++) {
    dv_weight_inverse_88_matrix[i] = (dv_coeff_t)rint(temp[i] * 16.0);
#ifdef USE_MMX
    /* If we're using MMX assembler, fold weights into the iDCT
       prescale */
    preSC[i] *= temp[i] * (16.0 / dv_weight_bias_factor);
#endif
  }

  for (z=0;z<4;z++) {
    for (x=0;x<8;x++) {
      dv_weight_inverse_248_matrix[z*8+x] = 1.0 / (W[x] * W[2*z] / 2);
      dv_weight_inverse_248_matrix[(z+4)*8+x] = 1.0 / (W[x] * W[2*z] / 2);
      
    }
  }
  dv_weight_inverse_248_matrix[0] = 4.0;
}

void weight_88(dv_coeff_t *block) {
  int x,y;
  dv_coeff_t dc;

  dc = block[0];
  for (y=0;y<8;y++) {
    for (x=0;x<8;x++) {
      block[y*8+x] *= W[x] * W[y] / 2;
    }
  }
  block[0] = dc / 4;
}

void weight_248(dv_coeff_t *block) {
  int x,z;
  dv_coeff_t dc;

  dc = block[0];
  for (z=0;z<8;z++) {
    for (x=0;x<8;x++) {
      block[z*8+x] *= W[x] * W[2*z] / 2;
      block[(z+4)*8+x] *= W[x] * W[2*z] / 2;
    }
  }
  block[0] = dc / 4;
}

void weight_88_inverse_float(double *block) {
  int x,y;
  double dc;

  dc = block[0];
  for (y=0;y<8;y++) {
    for (x=0;x<8;x++) {
      block[y*8+x] /= (W[x] * W[y] / 2.0);
    }
  }
  block[0] = dc * 4.0;
}

void weight_88_inverse(dv_coeff_t *block) {
  /* When we're using MMX assembler, weights are applied in the 8x8
     iDCT prescale */
#ifndef USE_MMX
  int i;
  for (i=0;i<64;i++) {
    block[i] *= dv_weight_inverse_88_matrix[i];
  }
#endif
}

void weight_248_inverse(dv_coeff_t *block) {
  /* These weights are now folded into the idct prescalers - so this
     function doesn't do anything. */
#if 0
  int x,z;
  dv_coeff_t dc;

  dc = block[0];
  for (z=0;z<4;z++) {
    for (x=0;x<8;x++) {
      block[z*8+x] /= (W[x] * W[2*z] / 2);
      block[(z+4)*8+x] /= (W[x] * W[2*z] / 2);
    }
  }
  block[0] = dc * 4;
#endif
}
