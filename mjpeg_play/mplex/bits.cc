/** @file bits.cc, bit-level output                                              */

/* Copyright (C) 2001, Andrew Stevens <andrew.stevens@philips.com> *

 *
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
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <assert.h>
#include "mjpeg_logging.h"
#include "bits.hh"


/// Initializes the bitstream, sets internal variables.
// TODO: The buffer size should be set dynamically to sensible sizes.
//
BitStream::BitStream() : 
	fileh(0)
{
	totbits = 0LL;
	buffer_start = 0LL;
	eobs = true;
	readpos =0LL;
	bfr = 0;
	bfr_size = 0;
}

/// Deconstructor. Deletes the internal buffer. 
BitStream::~BitStream()
{
	delete bfr;
}

/**
   Initialize buffer, call once before first putbits or alignbits.
  @param bs_filename output filename
  @param buf_size size of the temporary output buffer to use
*/
void OBitStream::open(char *bs_filename, unsigned int buf_size)
{
	if ((fileh = fopen(bs_filename, "wb")) == NULL)
	{
		mjpeg_error_exit1( "Unable to open file %s for writing; %s", 
						   bs_filename, strerror(errno));
	}
	filename = strcpy( new char[strlen(bs_filename)+1], bs_filename );
	bfr_size = buf_size;
	if( bfr == 0 )
		bfr = new uint8_t[buf_size];
	else
	{
		delete bfr;
		bfr = new uint8_t[buf_size];
	}
	// Save multiple buffering...
	setvbuf(fileh, 0, _IONBF, 0 );
	bitidx = 8;
	byteidx = 0;
	totbits = 0LL;
	outbyte = 0;
}


/** 
    Closes the OBitStream. Flushes the output buffer and closes the output file if one was open. 
 */
void OBitStream::close()
{
	if (fileh)
	{
		if (byteidx != 0)
			fwrite(bfr, sizeof(uint8_t), byteidx, fileh);
		fclose(fileh);
		delete filename;
		fileh = NULL;
	}
}

/**
  Puts a byte into the OBitStream. 
  Puts the byte into the internal buffer; if the buffer is full, it flushes the buffer to disk. 
 */
void OBitStream::putbyte()
{
    bfr[byteidx++] = outbyte;
    if (byteidx == bfr_size)
    {
		if (fwrite(bfr, sizeof(unsigned char), bfr_size, fileh) != bfr_size)
			mjpeg_error_exit1( "Write failed: %s", strerror(errno));
		byteidx = 0;
    }
	bitidx = 8;
}

/** write rightmost n (0<=n<=32) bits of val to outfile 
@param val value to write bitwise
@param n number of bits to write
*/
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

/** write rightmost bit of val to outfile
    @param val value to write one bit of
*/
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


/** zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void OBitStream::alignbits()
{
	if (bitidx != 8)
		putbits( 0, bitidx);
}

/**
   Refills an IBitStream's input buffer based on the internal variables bufcount and bfr_size. 
 */
bool IBitStream::refill_buffer()
{
	size_t i;
	if( bufcount >= bfr_size )
	{
		mjpeg_error_exit1("INTERNAL ERROR: additional data required but "
                                  "no free space in input buffer");
	}

	i = fread(bfr+bufcount, sizeof(uint8_t), 
			  static_cast<size_t>(bfr_size-bufcount), fileh);
	bufcount += i;

	if ( i == 0 )
	{
		eobs = true;
		return false;
	}
	return true;
}

/**
  Flushes all read input up-to *but not including* byte  unbuffer_upto.
@param flush_to the number of bits to flush
*/

void IBitStream::flush(bitcount_t flush_upto )
{
	if( flush_upto > buffer_start+bufcount )
		mjpeg_error_exit1("INTERNAL ERROR: attempt to flush input beyond buffered amount" );

	if( flush_upto < buffer_start )
		mjpeg_error_exit1("INTERNAL ERROR: attempt to flush input stream before  first buffered byte %d last is %d", flush_upto, buffer_start );

	unsigned int bytes_to_flush = 
		static_cast<unsigned int>(flush_upto - buffer_start);
	//
	// Don't bother actually flushing until a good fraction of a buffer
	// will be cleared.
	//

	if( bytes_to_flush < bfr_size*3/4 )
		return;

	bufcount -= bytes_to_flush;
	buffer_start = flush_upto;
	byteidx -= bytes_to_flush;
	memmove( bfr, bfr+bytes_to_flush, static_cast<size_t>(bufcount));
}


/**
  Undo scanning / reading
  N.b buffer *must not* be flushed between prepareundo and undochanges.
  @param undo handle to store the undo information
*/
void IBitStream::prepareundo( BitStreamUndo &undo)
{
	undo = *(static_cast<BitStreamUndo*>(this));
}

/**
Undoes changes committed to an IBitStream. 
@param undo handle to retrieve the undo information   
 */
void IBitStream::undochanges( BitStreamUndo &undo)
{
	*(static_cast<BitStreamUndo*>(this)) = undo;
}

/**
   Read a number bytes over an IBitStream, using the buffer. 
   @param dst buffer to read to 
   @param length the number of bytes to read
 */
unsigned int IBitStream::read_buffered_bytes(uint8_t *dst, unsigned int length)
{
	unsigned int to_read = length;
	if( readpos < buffer_start)
		mjpeg_error_exit1("INTERNAL ERROR: access to input stream buffer @ %d: before first buffered byte (%d)", readpos, buffer_start );

	if( readpos+length > buffer_start+bufcount )
	{
		if( !feof(fileh) )
        {
			mjpeg_error("INTERNAL ERROR: access to input stream buffer beyond last buffered byte @POS=%lld END=%d REQ=%lld + %d bytes", 
                        readpos,
                        bufcount, readpos-(bitcount_t)buffer_start,length  );
            abort();
        }
		to_read = static_cast<unsigned int>( (buffer_start+bufcount)-readpos );
	}
	memcpy( dst, 
			bfr+(static_cast<unsigned int>(readpos-buffer_start)), 
			to_read);
	// We only ever flush up to the start of a read as we
	// have only scanned up to a header *beginning* a block that is then
	// read
	flush( readpos );
	readpos += to_read;
	return to_read;
}

/** open the device to read the bit stream from it 
@param bs_filename filename to open
@param buf_size size of the internal buffer 
*/
void IBitStream::open( char *bs_filename, unsigned int buf_size)
{

	if ((fileh = fopen(bs_filename, "rb")) == NULL)
	{
		mjpeg_error_exit1( "Unable to open file %s for reading.", bs_filename);
	}
	filename = strcpy( new char[strlen(bs_filename)+1], bs_filename );

	bfr_size = buf_size;
	if( bfr == NULL )
		bfr = new uint8_t[buf_size];
	else
	{
		delete bfr;
		bfr = new uint8_t[buf_size];
	}

	byteidx = 0;
	bitidx = 8;
	totbits = 0LL;
	bufcount = 0;
	eobs = false;
	if (!refill_buffer())
	{
		if (bufcount==0)
		{
			mjpeg_error_exit1( "Unable to read from file %s.", bs_filename);
		}
	}
}

/** Skips forward longer sections fairly efficiently, flushing up to
    the seek point as it goes.
    Constraint: stream read point must be byte aligned...
    
*/
void IBitStream::skip_flush_bytes( unsigned int bytes_to_skip )
{
    assert( bitidx == 8 );
    if( byteidx + bytes_to_skip >= bufcount )
    {
        long to_seek = (buffer_start+byteidx+bytes_to_skip) -
                       (buffer_start+bufcount);
        readpos = buffer_start+byteidx+bytes_to_skip ;
        buffer_start = readpos;
        byteidx = 0;
        bufcount = 0;
        totbits += bytes_to_skip*8;
        if( ! fseek( fileh, to_seek, SEEK_CUR ) )
        {
            eobs = true;
        }
        else
            refill_buffer();
    }
    else
    {
        byteidx += bytes_to_skip;
        //
        // Move readpos so it corresponds to the byte buffered
        // at position byteidx in the read buffer.
        // N.b. readpos-buffer_start is always in buffer and <=
        // byteidx
        readpos += byteidx - (readpos-buffer_start);
        flush( readpos );
    }
}

/** sets the internal buffer size. 
    @param new_buf_size the new internal buffer size 
*/
void IBitStream::SetBufSize( unsigned int new_buf_size)
{
	assert( bfr != NULL ); // Must be open first!
	assert( new_buf_size >= bfr_size );	// Can only be increased in size...
    if( bfr_size != new_buf_size )
	{
		uint8_t *new_buf = new uint8_t[new_buf_size];
		memcpy( new_buf, bfr, static_cast<size_t>(bfr_size) );
		delete bfr;
		bfr_size = new_buf_size;
		bfr = new_buf;
	}

}

/**
   close the device containing the bit stream after a read process
*/
void IBitStream::close()
{
	if (fileh)
	{
		fclose(fileh);
		delete filename;
	}
	fileh = NULL;
}


#define masks(idx) (1<<(idx))

/*read 1 bit from the bit stream 
@returns the read bit, 0 on EOF */
uint32_t IBitStream::get1bit()
{
	unsigned int bit;

	if (eobs)
		return 0;

	bit = (bfr[byteidx] & masks(bitidx - 1)) >> (bitidx - 1);
	totbits++;
	bitidx--;
	if (!bitidx)
	{
		bitidx = 8;
		byteidx++;
		if (byteidx == bufcount)
		{
			unsigned int org_bufcount=bufcount;
			refill_buffer();
		}
	}

	return bit;
}

/*read N bits from the bit stream 
@returns the read bits, 0 on EOF */
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
				unsigned int org_bufcount=bufcount;
				refill_buffer();
			}
			i--;
		}
	}
	else
	{
		while (i > 0)
		{
			if (eobs)
				return 0;

			j = (bfr[byteidx] & masks(bitidx - 1)) >> (bitidx - 1);
			totbits++;
			bitidx--;
			if (!bitidx)
			{
				bitidx = 8;
				byteidx++;
				if (byteidx == bufcount)
				{
					unsigned int org_bufcount=bufcount;
					refill_buffer();
				}
			}
			val = (val << 1) | j;
			i--;
		}
	}
	return val;
}


/** This function seeks for a byte aligned sync word (max 32 bits) in the bit stream and
    places the bit stream pointer right after the sync.
    This function returns 1 if the sync was found otherwise it returns 0
@param sync the sync word to search for
@param N the number of bits to retrieve
@param lim number of bytes to search through
@returns false on error */

bool IBitStream::seek_sync(uint32_t sync, int N, int lim)
{
	uint32_t val, val1;
	uint32_t maxi = ((1U<<N)-1); /* pow(2.0, (double)N) - 1 */;
	if( maxi == 0 )
	{
		maxi = 0xffffffff;
	}
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



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
