/* bits.cc, bit-level output                                              */

/* Copyright (C) 2001, Andrew Stevens <andrew.stevens@philips.com> */

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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "mjpeg_logging.h"
#include "bits.hh"

/* initialize buffer, call once before first putbits or alignbits */
void OBitStream::open(char *bs_filename)
{
  if ((fileh = fopen(bs_filename, "wb")) == NULL)
  {
	  mjpeg_error_exit1( "Unable to open file %s for writing; %s\n", 
						 bs_filename, sys_errlist[sys_nerr]);
  }
  // Save multiple buffering...
  setvbuf(fileh, 0, _IONBF, 0 );
  bitidx = 8;
  byteidx = 0;
  totbits = 0LL;
  outbyte = 0;
}

void OBitStream::close()
{
  if (fileh)
  {
    if (byteidx != 0)
      fwrite(bfr, sizeof(uint8_t), byteidx, fileh);
    fclose(fileh);
    fileh = NULL;
  }
}

void OBitStream::putbyte()
{
    bfr[byteidx++] = outbyte;
    if (byteidx == BUFFER_SIZE)
    {
		if (fwrite(bfr, sizeof(unsigned char), BUFFER_SIZE, fileh) != BUFFER_SIZE)
			mjpeg_error_exit1( "Write failed: %s\n", sys_errlist[sys_nerr]);
		byteidx = 0;
    }
	bitidx = 8;
}

/* write rightmost n (0<=n<=32) bits of val to outfile */
void OBitStream::putbits(int val, int n)
{
  int i;
  unsigned int mask;

  mask = 1 << (n - 1); /* selects first (leftmost) bit */
  for (i = 0; i < n; i++)
  {
    totbits += 1;
    outbyte <<= 1;
    if (val & mask)
      outbyte |= 1;
    mask >>= 1; /* select next bit */
    bitidx--;
    if (bitidx == 0) /* 8 bit buffer full */
      putbyte();
  }
}

/* write rightmost bit of val to outfile */
void OBitStream::put1bit(int val)
{
  totbits += 1;
  outbyte <<= 1;
  if (val & 0x1)
    outbyte |= 1;
  bitidx--;
  if (bitidx == 0) /* 8 bit buffer full */
    putbyte();
}

/* Prepare a upcoming undo at the beginning of a GOP */
void IBitStream::prepareundo( BitStream &undo)
{
  fgetpos(fileh, &actpos);
  undo = *(static_cast<BitStream*>(this));
}

/* Reset old status, undo all changes made up to a point */
void IBitStream::undochanges( BitStream &undo)
{
	*(static_cast<BitStream *>(this)) = undo;
	fsetpos(fileh, &actpos);
}

/* zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void OBitStream::alignbits()
{
  if (bitidx != 8)
    putbits( 0, bitidx);
}


bool IBitStream::refill_buffer()
{
	unsigned int i;

  i = fread(bfr, sizeof(uint8_t), BUFFER_SIZE, fileh);
  if (!i)
  {
    eobs = true;
    return false;
  }
  bufcount = i;
  return true;
}

/* open the device to read the bit stream from it */
void IBitStream::open( char *bs_filename)
{

  if ((fileh = fopen(bs_filename, "rb")) == NULL)
  {
	  mjpeg_error_exit1( "Unable to open file %s for reading.\n", bs_filename);
  }
  byteidx = 0;
  bitidx = 8;
  totbits = 0LL;
  bufcount = 0;
  eobs = false;
  if (!refill_buffer())
  {
    if (eobs)
    {
		mjpeg_error_exit1( "Unable to read from file %s.\n", bs_filename);
    }
  }
}


/* open the device to read the bit stream from it */
void IBitStream::rewind()
{

  byteidx = 0;
  bitidx = 8;
  totbits = 0LL;
  bufcount = 0;
  eobs = false;
  ::rewind(fileh);
  (void) refill_buffer();
}

/*close the device containing the bit stream after a read process*/
void IBitStream::close()
{
  if (fileh)
    fclose(fileh);
  fileh = NULL;
}


// TODO replace with shift ops!

uint8_t IBitStream::masks[8]={0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};

/*read 1 bit from the bit stream */
uint32_t IBitStream::get1bit()
{
  unsigned int bit;

  if (eobs)
    return false;

  bit = (bfr[byteidx] & masks[bitidx - 1]) >> (bitidx - 1);
  totbits++;
  bitidx--;
  if (!bitidx)
  {
    bitidx = 8;
    byteidx++;
    if (byteidx == bufcount)
    {
      if (bufcount == BUFFER_SIZE)
        refill_buffer();
      else
        eobs = true;
      byteidx = 0;
    }
  }

  return bit;
}

/*read N bits from the bit stream */
uint32_t IBitStream::getbits(int N)
{
  uint32_t val = 0;
  int i = N;
  unsigned int j;

  // Optimize: we are on byte boundary and want to read multiple of bytes!
  if ((bitidx == 8) && ((N & 7) == 0))
  {
    i = N >> 3;
    while (i > 0)
    {
      if (eobs)
        return 0;
      val = (val << 8) | bfr[byteidx];
      byteidx++;
      totbits += 8;
      if (byteidx == bufcount)
      {
        if (bufcount == BUFFER_SIZE)
          refill_buffer();
        else
          eobs = true;
        byteidx = 0;
      }
      i--;
    }
  }
  else
  {
    while (i > 0)
    {
      if (eobs)
        return false;

      j = (bfr[byteidx] & masks[bitidx - 1]) >> (bitidx - 1);
      totbits++;
      bitidx--;
      if (!bitidx)
      {
        bitidx = 8;
        byteidx++;
        if (byteidx == bufcount)
        {
          if (bufcount == BUFFER_SIZE)
            refill_buffer();
          else
            eobs = true;
          byteidx = 0;
        }
      }
      val = (val << 1) | j;
      i--;
    }
  }
  return val;
}


/* This function seeks for a byte aligned sync word (max 32 bits) in the bit stream and
  places the bit stream pointer right after the sync.
  This function returns 1 if the sync was found otherwise it returns 0  */

bool IBitStream::seek_sync(uint32_t sync, int N, int lim)
{
  uint32_t val, val1;
  uint32_t maxi = ((1U<<N)-1); /* pow(2.0, (double)N) - 1 */;

  while (bitidx != 8)
  {
    get1bit();
  }

  val = getbits(N);
  if( eobs )
  	return false;

  while ((val & maxi) != sync && --lim)
  {
    val <<= 8;
    val1 = getbits( 8 );
    val |= val1;
    if( eobs )
    	return false;
  }
  
  return (!!lim);
}

