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


BitStreamBuffering::BitStreamBuffering() :
    bfr(0),
    bfr_size(0),
    buffered(0)
    
{
}


/*****
 *
 * Pseudo-destructor. Frees the internal buffer and marks buffer as
 * empty.
 *
 *****/
void BitStreamBuffering::Release()
{
    if( bfr != 0)
        delete bfr;
    bfr = 0;
    bfr_size = 0;
    buffered = 0;
}


/*****
 *
 * Empty the buffer.
 *
 *****/

void BitStreamBuffering::Empty()
{
    buffered = 0;
}

/** sets the internal buffer size. 
    @param new_buf_size requests new internal buffer size 
*/
void BitStreamBuffering::SetBufSize( unsigned int new_buf_size)
{
    //
    // If size has changed and we won't lose buffered data
    // we adjust the buffer size, otherwise we ignore the request
    //
	if( new_buf_size > BUFFER_CEILING )
	{
		mjpeg_error_exit1("INTERNAL ERROR: additional data required but "
                          " input buffer size would exceed ceiling");
	}
    
    if( new_buf_size > buffered && bfr_size != new_buf_size )
	{
		uint8_t *new_buf = new uint8_t[new_buf_size];
		memcpy( new_buf, bfr, static_cast<size_t>(buffered) );
        if( bfr != 0 )
            delete bfr;
		bfr_size = new_buf_size;
		bfr = new_buf;
	}

}

/****************************
 *
 * Return the pointer to where (after, if necessary extending the
 * buffer) up to to_append bytes may be appended to the buffered bytes
 *
 ****************************/

uint8_t *BitStreamBuffering::StartAppendPoint( unsigned int to_append )
{
    unsigned int resize_size = bfr_size;
    assert( resize_size != 0 );
    while( resize_size - buffered < to_append )
    {
        resize_size *= 2;
    }
    if( resize_size != bfr_size )
        SetBufSize( resize_size );
    return bfr+buffered;
}

/**
   Refills an IBitStream's input buffer based on the internal
   variables buffered and bfr_size.
   Strategy: we read 1/4 of a buffer, always.
 */
bool IBitStream::ReadIntoBuffer(unsigned int to_read)
{
	size_t i;
    unsigned int read_pow2 = BUFFER_SIZE/4;
    while( read_pow2 < to_read ) 
        read_pow2 <<= 1;
            
	i = fread(StartAppendPoint(read_pow2), sizeof(uint8_t), 
			  static_cast<size_t>(read_pow2), fileh);
    Appended(static_cast<unsigned int>(i));

	if ( i == 0 )
	{
		eobs = true;
		return false;
	}
	return true;
}



/** open the device to read the bit stream from it 
@param bs_filename filename to open
@param buf_size size of the internal buffer 
*/
void IBitStream::Open( char *bs_filename, unsigned int buf_size)
{
    if( fileh != 0)
        mjpeg_error_exit1( "Attempt to open already open stream!");
	if ((fileh = fopen(bs_filename, "rb")) == NULL)
	{
		mjpeg_error_exit1( "Unable to open file %s for reading.", bs_filename);
	}
	filename = strcpy( new char[strlen(bs_filename)+1], bs_filename );

    SetBufSize(buf_size);
	eobs = false;
	if (!ReadIntoBuffer())
	{
		if (buffered==0)
		{
			mjpeg_error_exit1( "Unable to read from file %s.", bs_filename);
		}
	}
}


#define masks(idx) (1<<(idx))

/*read 1 bit from the bit stream 
@returns the read bit, 0 on EOF */
uint32_t IBitStream::Get1Bit()
{
	unsigned int bit;

	if (eobs)
		return 0;

	bit = (bfr[byteidx] & masks(bitidx - 1)) >> (bitidx - 1);
	bitreadpos++;
	bitidx--;
	if (!bitidx)
	{
		bitidx = 8;
		byteidx++;
		if (byteidx == buffered)
		{
			ReadIntoBuffer();
		}
	}

	return bit;
}

/*read N bits from the bit stream 
@returns the read bits, 0 on EOF */
uint32_t IBitStream::GetBits(int N)
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
			bitreadpos += 8;
			if (byteidx == buffered)
			{
				ReadIntoBuffer();
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
			bitreadpos++;
			bitidx--;
			if (!bitidx)
			{
				bitidx = 8;
				byteidx++;
				if (byteidx == buffered)
				{
					ReadIntoBuffer();
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

bool IBitStream::SeekSync(uint32_t sync, int N, int lim)
{
	uint32_t val, val1;
	uint32_t maxi = ((1U<<N)-1); /* pow(2.0, (double)N) - 1 */;
	if( maxi == 0 )
	{
		maxi = 0xffffffff;
	}
	while (bitidx != 8)
	{
		Get1Bit();
	}

	val = GetBits(N);
	if( eobs )
		return false;
	while ((val & maxi) != sync && --lim)
	{
		val <<= 8;
		val1 = GetBits( 8 );
		val |= val1;
		if( eobs )
			return false;
	}
  
	return (!!lim);
}

/****************
 *
 * Move the bit read position forward a specified number of bytes
 * buffering bytes skipped over that are not already buffered
 *
 ***************/

bool IBitStream::SeekFwdBits( unsigned int bytes_to_seek_fwd)
{ 
    assert(bitidx == 8);
    unsigned int req_byteidx = byteidx + bytes_to_seek_fwd;
    unsigned int org_buffered = buffered;
    while( req_byteidx >= buffered && !eobs)
    {
        ReadIntoBuffer( req_byteidx - (buffered-1) );
    }
    
    eobs = ( req_byteidx >= buffered );
    if( eobs )
        bitreadpos += (buffered-org_buffered)*8;
    else
        bitreadpos += bytes_to_seek_fwd*8;
    byteidx = req_byteidx;
}


/**
  Flushes all read input up-to *but not including* byte flush_upto.
  @param flush_to the number of bits to flush
*/

void IBitStream::Flush(bitcount_t flush_upto )
{
	if( flush_upto > bfr_start+buffered )
		mjpeg_error_exit1("INTERNAL ERROR: attempt to flush input beyond buffered amount" );

	if( flush_upto < bfr_start )
		mjpeg_error_exit1("INTERNAL ERROR: attempt to flush input stream before  first buffered byte %d last is %d", flush_upto, bfr_start );

	unsigned int bytes_to_flush = 
		static_cast<unsigned int>(flush_upto - bfr_start);
	//
	// Don't bother actually flushing until a good fraction of a buffer
	// will be cleared.
	//

	if( bytes_to_flush < bfr_size/2 )
		return;
	buffered -= bytes_to_flush;
	bfr_start = flush_upto;
	byteidx -= bytes_to_flush;
	memmove( bfr, bfr+bytes_to_flush, static_cast<size_t>(buffered));
}


/**
  Undo scanning / reading
  N.b buffer *must not* be flushed between prepareundo and undochanges.
  @param undo handle to store the undo information
*/
void IBitStream::PrepareUndo( IBitStreamUndo &undo)
{
	undo = *(static_cast<IBitStreamUndo*>(this));
}

/**
Undoes changes committed to an IBitStream. 
@param undo handle to retrieve the undo information   
 */
void IBitStream::UndoChanges( IBitStreamUndo &undo)
{
	*(static_cast<IBitStreamUndo*>(this)) = undo;
}

/**
   Read a number bytes over an IBitStream, using the buffer. 
   @param dst buffer to read to 
   @param length the number of bytes to read
 */
unsigned int IBitStream::GetBytes(uint8_t *dst, unsigned int length)
{
	unsigned int to_read = length;
	if( bytereadpos < bfr_start)
		mjpeg_error_exit1("INTERNAL ERROR: access to input stream buffer @ %d: before first buffered byte (%d)", bytereadpos, bfr_start );

	if(  bytereadpos+length > bfr_start+buffered )
	{
		if( !feof(fileh) )
        {
			mjpeg_error("INTERNAL ERROR: access to input stream buffer beyond last buffered byte @POS=%lld END=%d REQ=%lld + %d bytes", 
                        bytereadpos,
                        buffered, 
                        bytereadpos-bfr_start,
                        length  );
            abort();
        }
		to_read = static_cast<unsigned int>( (bfr_start+buffered)- bytereadpos );
	}
	memcpy( dst, 
			bfr+(static_cast<unsigned int>( bytereadpos-bfr_start)), 
			to_read);
	// We only ever flush up to the start of a read as we
	// have only scanned up to a header *beginning* a block that is then
	// read
	//flush( bytereadpos );
	bytereadpos += to_read;
	return to_read;
}

/** Skips forward the bit read pos.  Skipped bytes are not necessarily
    reads and the buffer is flush to the position skipped to and
    the byte read position set to match the bit read position.
*/


void IBitStream::BitsBytesSkipFlush( unsigned int bytes_to_skip )
{
    assert( bitidx == 8 );
    if( byteidx + bytes_to_skip >= buffered )
    {
        long to_seek = (bfr_start+byteidx+bytes_to_skip) -
                       (bfr_start+buffered);
        bytereadpos = bfr_start+byteidx+bytes_to_skip ;
        bfr_start = bytereadpos;
        byteidx = 0;
        bitreadpos += bytes_to_skip*8;
        Empty();
        if( ! fseek( fileh, to_seek, SEEK_CUR ) )
        {
            eobs = true;
        }
        else
            ReadIntoBuffer();
    }
    else
    {
        byteidx += bytes_to_skip;
        //
        // Move readpos so it corresponds to the byte buffered
        // at position byteidx in the read buffer.
        // N.b. readpos-bfr_start is always in buffer and <=
        // byteidx
        bytereadpos += byteidx - (bytereadpos-bfr_start);
        Flush( bytereadpos );
        ReadIntoBuffer();
    }
}


/**
   close the device containing the bit stream after a read process
*/
void IBitStream::Close()
{
	if (fileh)
	{
		fclose(fileh);
		delete filename;
	}
	fileh = 0;
    Release();
}


#ifdef REDUNDANT_CODE
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
    SetBufSize(buf_size);
	// Save multiple buffering...
	setvbuf(fileh, 0, _IONBF, 0 );
	bitidx = 8;
	byteidx = 0;
	bitwritepos = 0LL;
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
  Puts a byte into the OBitStream.  Puts the byte into the internal
  buffer; if the buffer is full, it flushes the buffer to disk.
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
void OBitStream::putbits(uint32_t val, int n)
{
	int i;
	unsigned int mask;

    bitwritepos += n;
    while( n >= bitidx )
    {
        outbyte = 
            (outbyte<<bitidx) | ((val >> (n-bitidx)) & ((1u<<bitidx)-1));
        n -= bitidx;
        putbyte();
    }
    if( n > 0 )
    {
        bitidx -= n;
        outbyte = val & ((1u<<bitidx)-1);
    }
#ifdef ORIGINAL
	mask = 1 << (n - 1); /* selects first (leftmost, msb) bit */
	for (i = 0; i < n; i++)
	{
		bitwritepos += 1;
		outbyte <<= 1;
		if (val & mask)
			outbyte |= 1;
		mask >>= 1; /* select next bit */
		bitidx--;
		if (bitidx == 0) /* 8 bit buffer full */
			putbyte();
	}
#endif
}

/** write rightmost bit of val to outfile
    @param val value to write one bit of
*/
void OBitStream::put1bit(int val)
{
	++bitwritepos;
    outbyte = (outbyte<<1) | (val & 0x1);
	--bitidx;
	if (bitidx == 0) /* 8 bit buffer full */
		putbyte();
}


/** zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void OBitStream::alignbits()
{
	if (bitidx != 8)
		putbits( 0, bitidx);
    flush_bytes();
}

#endif

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
