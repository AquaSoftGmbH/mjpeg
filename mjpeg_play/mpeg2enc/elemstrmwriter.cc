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
#include <cassert>
#include <string.h>

ElemStrmWriter::ElemStrmWriter(EncoderParams &_encparams ) :
	encparams( _encparams )
{
	serial_id = 1;
	last_flushed_serial_id = 0;
	outcnt = 8;
	bytecnt = BITCOUNT_OFFSET/8LL;
	buffer_size = 1024*128;

	buffer = NULL;
	ExpandBuffer();
}


ElemStrmWriter::~ElemStrmWriter()
{
	free( buffer );
}

void ElemStrmWriter::ExpandBuffer()
{
	buffer_size *= 2;
	buffer = static_cast<uint8_t *>(realloc( buffer, sizeof(uint8_t[buffer_size])));
	if( !buffer )
		mjpeg_error_exit1( "output buffer memory allocation: out of memory" );
}

ElemStrmBufferState	ElemStrmWriter::CurrentState()
{
	assert( outcnt == 8 );
	++serial_id;
	return static_cast<ElemStrmBufferState>(*this);
}

void ElemStrmWriter::FlushBuffer( )
{
	assert( outcnt == 8 );
	WriteOutBufferUpto( unflushed );
	unflushed = 0;
	++serial_id;
	last_flushed_serial_id = serial_id;
}

void ElemStrmWriter::RestoreState( const ElemStrmBufferState &restore )
{
	assert( restore.serial_id > last_flushed_serial_id );
	*static_cast<ElemStrmBufferState *>(this) = restore;
}

void ElemStrmWriter::AlignBits()
{
	if (outcnt!=8)
		PutBits(0,outcnt);
}

/**************
 *
 * Write least significant n (0<=n<=32) bits of val to output buffer
 *
 *************/

void ElemStrmWriter::PutBits(uint32_t val, int n)
{
	val = (n == 32) ? val : (val & (~(0xffffffffU << n)));
	while( n >= outcnt )
	{
		pendingbits = (pendingbits << outcnt ) | (val >> (n-outcnt));
		if( unflushed == buffer_size )
			ExpandBuffer();
		buffer[unflushed] = pendingbits;
		++unflushed;
		n -= outcnt;
		outcnt = 8;
		++bytecnt;
	}
	if( n != 0 )
	{
		pendingbits = (pendingbits<<n) | val;
		outcnt -= n;
	}
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
