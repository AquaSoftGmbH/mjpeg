/* fdctref.c, forward discrete cosine transform, double precision           */

/* RJ: Optimized version using integer arithmetic */

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

#include <math.h>

#include "config.h"

#ifndef PI
# ifdef M_PI
#  define PI M_PI
# else
#  define PI 3.14159265358979323846
# endif
#endif

/* global declarations */
void init_fdct _ANSI_ARGS_((void));
void fdct _ANSI_ARGS_((short *block));

/* private data */
static int c[8][8]; /* transform coefficients */

void init_fdct()
{
  int i, j;
  double s;

  for (i=0; i<8; i++)
  {
    s = (i==0) ? sqrt(0.125) : 0.5;

    for (j=0; j<8; j++)
      c[i][j] = s * cos((PI/8.0)*i*(j+0.5))*512 + 0.5;
  }
}

void fdct(block)
short *block;
{
  int i, j;
  int s;
  int tmp[64];

  for (i=0; i<8; i++)
    for (j=0; j<8; j++)
    {
        s = c[j][0] * block[8*i+0]
          + c[j][1] * block[8*i+1]
          + c[j][2] * block[8*i+2]
          + c[j][3] * block[8*i+3]
          + c[j][4] * block[8*i+4]
          + c[j][5] * block[8*i+5]
          + c[j][6] * block[8*i+6]
          + c[j][7] * block[8*i+7];

      tmp[8*i+j] = s;
    }

  for (j=0; j<8; j++)
    for (i=0; i<8; i++)
    {
        s = c[i][0] * tmp[8*0+j]
          + c[i][1] * tmp[8*1+j]
          + c[i][2] * tmp[8*2+j]
          + c[i][3] * tmp[8*3+j]
          + c[i][4] * tmp[8*4+j]
          + c[i][5] * tmp[8*5+j]
          + c[i][6] * tmp[8*6+j]
          + c[i][7] * tmp[8*7+j];

      block[8*i+j] = s>>18;
    }
}
