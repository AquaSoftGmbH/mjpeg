#ifndef __BITS_H__
#define __BITS_H__

#include <config.h>
#include <stdio.h>
#include <assert.h>

typedef uint64_t bitcount_t;


class BitStreamBuffering
{
public:
	BitStreamBuffering();
	void Release();
	void Empty();
	void SetBufSize( unsigned int buf_size);
	uint8_t *StartAppendPoint(unsigned int additional);
	inline void Appended(unsigned int additional)
		{
			buffered += additional;
			assert( buffered <= bfr_size );
		}
private:
	inline uint8_t *BufferEnd() { return bfr+buffered; }
protected:
	//
	// TODO: really we should set these based on system parameters etc.
	//
	static const unsigned int BUFFER_SIZE = 64 * 1024;
	static const unsigned int BUFFER_CEILING = 32 * 1024 * 1024;
	uint8_t *bfr;				// The read/write buffer tiself
	unsigned int bfr_size;		// The physical size of the buffer =
								// maximum buffered data-bytes possible
	unsigned int buffered;		// Number of data-bytes in buffer
};



/*******
 *
 * the input bit stream class provides a mechanism to restore the
 * scanning state to a marked point, provided no flush has taken place.
 *
 * This base class contains the state information needed to mark/restore
 * between flushes.
 *
 * N.b. not that we override the BitStreamBuffering destructor so
 * we don't cheerfully deallocate the buffer when an undo
 * goes out of scope!
 *
 ******/
 
class IBitStreamUndo : public BitStreamBuffering
{
public:
	inline bool eos() { return eobs; }
	inline bitcount_t bitcount() { return bitreadpos; }

protected:
	bitcount_t bfr_start;	    // The position in the underlying
								// data-stream of the first byte of
								// the buffer.
	unsigned int byteidx;		// Byte in buffer holding bit at current
								// to current bit read position
	int bitidx;					// Position in byteidx buffer byte of
								// bit at current bit read position
								// N.b. coded as bits-left-to-read count-down
	
	bitcount_t bitreadpos;			// Total bits read at current bit read position
	bitcount_t bytereadpos;			// Bit position of the current *byte*
								// read position
	bool eobs;					// End-of-bitstream  flag: true iff
								// Bit read position has reached the end
								// of the underlying bitstream...
};

//
// Input bit stream class.   Supports the "scanning" of a stream
// into a large buffer which is flushed once it has been read.
// N.b. if you scan ahead a long way and don't read its your
// responsibility to flush manually...
//

class IBitStream : public IBitStreamUndo {
public:
	IBitStream() :
		fileh(0)
		{
			bfr_start = 0LL;
			bitreadpos = 0LL;
			bytereadpos = 0LL;
			bitidx = 8;
			eobs = true;
		}
	~IBitStream() { Release(); }
	void Open( char *bs_filename, unsigned int buf_size = BUFFER_SIZE);
	void Close();
	uint32_t Get1Bit();
	uint32_t GetBits(int N);
	void PrepareUndo(IBitStreamUndo &undobuf);
	void UndoChanges(IBitStreamUndo &undobuf);
	bool SeekSync( unsigned int sync, int N, int lim);
	bool SeekFwdBits( unsigned int bytes_to_seek_fwd );
	void BitsBytesSkipFlush( unsigned int bytes_to_skip );
	inline bitcount_t GetBytePos() { return bytereadpos; }
	void Flush( bitcount_t byte_position );
	inline unsigned int BufferedBytes()
		{
			return (bfr_start+buffered-bytereadpos);
		}
	unsigned int GetBytes( uint8_t *dst,
						   unsigned int length_bytes);
	inline const char *StreamName() { return filename; }
private:
	FILE *fileh;
	const char *filename;
	bool ReadIntoBuffer( unsigned int to_read = BUFFER_SIZE );
};

#ifdef REDUNDANT_CODE
class OBitStreamUndo : public BitStreamBuffering
{
protected:
	uint8_t outbyte;
	unsigned int byteidx;
	unsigned int bitidx;
	unsigned int buffered;
	bitcount_t bitwritepos;
	uint8_t *bfr;
	unsigned int bfr_size;

};


class BitStream : public OBitStreamUndo
{
};



class OBitStream : public OBitStreamUndo {
public:
	inline bitcount_t bitcount() { return bitwritepos; }
	void open( char *bs_filename, unsigned int buf_size = BUFFER_SIZE);
	void close();
	void putbits( int val, int n);
	void put1bit( int val);
	void alignbits();
private:
	FILE *fileh;
	const char *filename;
	void putbyte();
};

#endif

#endif  // __BITS_H__

