/* elemstrmwriter.cc -  bit-level output of MPEG-1/2 elementary video stream */

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


#include "elemstrmwriter.hh"
#include "mpeg2encoder.hh"
#include <stdio.h>


ElemStrmWriter::ElemStrmWriter(EncoderParams &_encparams ) :
	encparams( _encparams )
{
  outcnt = 8;
  bytecnt = BITCOUNT_OFFSET/8LL;
}



/* zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void ElemStrmWriter::AlignBits()
{
	if (outcnt!=8)
		PutBits(0,outcnt);
}



FILE_StrmWriter::FILE_StrmWriter( EncoderParams &encparams, const char *outfilename ) :
	ElemStrmWriter( encparams )
{
	/* open output file */
	if (!(outfile=fopen(outfilename,"wb")))
	{
		mjpeg_error_exit1("Couldn't create output file %s",outfilename);
	}
}

FILE_StrmWriter::~FILE_StrmWriter()
{
	fclose( outfile );
}

/**************
 *
 * Write rightmost n (0<=n<=32) bits of val to outfile 
 *
 *************/
void FILE_StrmWriter::PutBits(uint32_t val, int n)
{
	val = (n == 32) ? val : (val & (~(0xffffffffU << n)));
	while( n >= outcnt )
	{
		outbfr = (outbfr << outcnt ) | (val >> (n-outcnt));
		putc( outbfr, outfile );
		n -= outcnt;
		outcnt = 8;
		++bytecnt;
	}
	if( n != 0 )
	{
		outbfr = (outbfr<<n) | val;
		outcnt -= n;
	}
}


void FILE_StrmWriter::FrameBegin()
{
}

void FILE_StrmWriter::FrameFlush()
{
}

void FILE_StrmWriter::FrameDiscard()
{
}
    
