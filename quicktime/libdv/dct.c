/* 
 *  dct.c
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
#include "dct.h"
#ifdef USE_MMX
#include "mmx.h"
#endif


static double KC248[8][4][4][8];
static double KC88[8][8][8][8];
static double C[8];

void dct_init(void) {
  int x, y, z, h, v, u, i;
    
  for (x = 0; x < 8; x++) {
    for (z = 0; z < 4; z++) {
      for (u = 0; u < 4; u++) {
        for (h = 0; h < 8; h++) {
		KC248[x][z][u][h] = 
			cos((M_PI * u * ((2.0 * z) + 1.0)) / 8.0) *
			cos((M_PI * h * ((2.0 * x) + 1.0)) / 16.0);
        }                       /* for h */
      }                         /* for u */
    }                           /* for z */
  }                             /* for x */

  for (x = 0; x < 8; x++) {
    for (y = 0; y < 8; y++) {
      for (v = 0; v < 8; v++) {
        for (h = 0; h < 8; h++) {
          KC88[x][y][h][v] =
            cos((M_PI * v * ((2.0 * y) + 1.0)) / 16.0) *
            cos((M_PI * h * ((2.0 * x) + 1.0)) / 16.0);
        }
      }
    }
  }

  for (i = 0; i < 8; i++) {
    C[i] = (i == 0 ? 0.5 / sqrt(2.0) : 0.5);
  } /* for i */
}

void dct_88(double *block) {
  int v,h,y,x,i;
  double temp[64];

  memset(temp,0,sizeof(temp));
  for (v=0;v<8;v++) {
    for (h=0;h<8;h++) {
      for (y=0;y<8;y++) {
        for (x=0;x<8;x++) {
          temp[v*8+h] += block[y*8+x] * KC88[x][y][h][v];
        }
      }
      temp[v*8+h] *= (C[h] * C[v]);
    }
  }

  for (i=0;i<64;i++)
    block[i] = temp[i];
}

void dct_248(double *block) {
  int u,h,z,x,i;
  double temp[64];

  memset(temp,0,sizeof(temp));
  for (u=0;u<4;u++) {
    for (h=0;h<8;h++) {
      for (z=0;z<4;z++) {
        for (x=0;x<8;x++) {
          temp[u*8+h] += (block[(2*z)*8+x] + block[(2*z+1)*8+x]) *
                            KC248[x][z][h][u];
          temp[(u+4)*8+h] += (block[(2*z)*8+x] - block[(2*z+1)*8+x]) *
                              KC248[x][z][h][u];
        }
      }
      temp[u*8+h] *= (C[h] * C[u]);
      temp[(u+4)*8+h] *= (C[h] * C[u]);
    }
  }

  for (i=0;i<64;i++)
    block[i] = temp[i];
}

void idct_block_mmx(gint16 *block);

void idct_88(dv_coeff_t *block) {
#ifndef USE_MMX
  int v,h,y,x,i;
  double temp[64];

  memset(temp,0,sizeof(temp));
  for (v=0;v<8;v++) {
    for (h=0;h<8;h++) {
      for (y=0;y<8;y++){ 
        for (x=0;x<8;x++) {
          temp[y*8+x] += C[v] * C[h] * block[v*8+h] * KC88[x][y][h][v];
        }
      }
    }
  }

  for (i=0;i<64;i++)
    block[i] = temp[i];

#else
  idct_block_mmx(block);
  emms();
#endif
}

#if BRUTE_FORCE_248

void idct_248(double *block) {
  int u,h,z,x,i;
  double temp[64];
  double temp2[64];
  double b,c;
  double (*in)[8][8], (*out)[8][8]; /* don't really need storage -- fixup later */

  memset(temp,0,sizeof(temp));

  out = &temp;
  in = &temp2;

  
  for(z=0;z<8;z++) {
    for(x=0;x<8;x++)
      (*in)[z][x] = block[z*8+x];
  }

    for (x = 0; x < 8; x++) {
      for (z = 0; z < 4; z++) {
	for (u = 0; u < 4; u++) {
	  for (h = 0; h < 8; h++) {
	    b = (double)(*in)[u][h];  
	    c = (double)(*in)[u+4][h];
	    (*out)[2*z][x] += C[u] * C[h] * (b + c) * KC248[x][z][u][h];
	    (*out)[2*z+1][x] += C[u] * C[h] * (b - c) * KC248[x][z][u][h];
	  }                       /* for h */
	}                         /* for u */
      }                           /* for z */
    }                             /* for x */

  for(z=0;z<8;z++) {
    for(x=0;x<8;x++)
      block[z*8+x] = (*out)[z][x];
  }
}


#endif // BRUTE_FORCE_248
