/* putbits.c, bit-level output                                              */

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
/* Modifications and enhancements (C) 2000/2001 Andrew Stevens */

/* These modifications are free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */


#include <config.h>
#include <stdio.h>
#include "global.h"

extern FILE *outfile; /* the only global var we need here */
/* private data */
#ifdef ORIGINAL_CODE
static unsigned char outbfr;
#else
static unsigned int outbfr;
char *outbfrch=(char *)&outbfr;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#define BYTE4 3
#else
#define BYTE4  0
#endif
#define ORIGINAL_CODE
static int outcnt;
static int64_t bytecnt;

/* initialize buffer, call once before first putbits or alignbits */
void initbits()
{
#ifdef ORIGINAL_CODE
  outcnt = 8;
#else
  outcnt = 0;
  outbfr = 0;
#endif
  bytecnt = BITCOUNT_OFFSET/8LL;
}


/* write rightmost n (0<=n<=32) bits of val to outfile */
void putbits(val,n)
int val;
int n;
{
#ifdef ORIGINAL_CODE
  int i;
  unsigned int mask;

  mask = 1 << (n-1); /* selects first (leftmost) bit */

  for (i=0; i<n; i++)
  {
    outbfr <<= 1;

    if (val & mask)
      outbfr|= 1;

    mask >>= 1; /* select next bit */
    outcnt--;

    if (outcnt==0) /* 8 bit buffer full */
    {
      putc(outbfr,outfile);
      outcnt = 8;
      bytecnt++;
    }
  }
#else
  /* We do it in 16-bit (max) chunks to guarantee there's always space in the
   buffer */
  int shft;
  unsigned int byte;
  int floor = (((n-1) >> 3)<<3);	/* Largest 8*k s.t. 8*k < n */

  while( floor >= 0 )
  {
      byte = ((unsigned int)val)>>floor;
	  n -= floor;
	  val &= ~(0xff<<floor);
	  shft = (32-outcnt-n);
	  outbfr |= (byte<<shft);
	  outcnt += n;
	  if( outcnt >= 8 )
	  {
		  putc(outbfrch[BYTE4],outfile);
		  outbfr <<= 8;
		  ++bytecnt;
		  outcnt -= 8;
	  }
	  n = floor;
	  floor -= 8;
  } 
  
#endif
}

/* zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void alignbits()
{
#ifdef ORIGINAL_CODE
  if (outcnt!=8)
    putbits(0,outcnt);
#else
  if (outcnt!=0)
	  putbits(0,8-outcnt);
#endif
}

/* return total number of generated bits */
int64_t bitcount()
{
#ifdef ORIGINAL_CODE
  return 8LL*bytecnt + (8-outcnt);
#else
  return 8LL*bytecnt + outcnt;
#endif
}
