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
 * the input bit stream class (see below) provides a mechanism to
 * restore the scanning state to a marked point, provided no flush has
 * taken place.
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
	IBitStreamUndo() :
		bfr_start(0LL),
		bitreadpos(0LL),
		bitidx(8),
		bytereadpos(0LL),
		eobs(true)
		{}
	inline bool eos() { return eobs; }
	inline bitcount_t bitcount() { return bitreadpos; }

protected:
	bitcount_t bfr_start;	    // The position in the underlying
								// data-stream of the first byte of
								// the buffer.
	unsigned int byteidx;		// Byte in buffer holding bit at current
								// to current bit read position
	bitcount_t bitreadpos;			// Total bits read at current bit read position
	int bitidx;					// Position in byteidx buffer byte of
								// bit at current bit read position
								// N.b. coded as bits-left-to-read count-down
	
	bitcount_t bytereadpos;			// Bit position of the current *byte*
								// read position
	bool eobs;					// End-of-bitstream  flag: true iff
								// Bit read position has reached the end
								// of the underlying bitstream...
};

/***************************************
 *
 * IBitStream -  Input bit stream base class.  Supports the
 * "scanning" of a stream held in a large buffer which is flushed
 * once it has been "read".
 *
 * I.e. there are in effect two file-pointers: 
 *
 * A bit-level parsing file-pointers intended for bit-level parsing
 * through the 'Get*' and 'Seek*'.  Scanning/seeking using these entry
 * points keeps appending the got/sought data from the underlying
 * stream to a (dynamically sized) internal buffer.
 *
 * A byte-level I/O file pointer used for reading chunks of data
 * identified through parsing.
 *
 * A final set of entry-points allow parsed/read data that no longer
 * needs to buffered to be flushed from the buffer (and buffer space
 * reclaimed!).
 *
 * INVARIANT: only data items up to the bit-level file-pointer can be 'read'
 *
 * The actual source of the bit stream to be parsed/read is *abstract*
 * in this base class.  Access in derived classes is through the
 * virtual member function 'ReadStreamBytes' which should behave in
 * the same way as 'fread'.  I.e. it should only return a short count
 * at EOF or ERROR and further calls after EOF or ERROR should return
 * a zero count.
 *
 * Hence the actual source of the bit stream need not support seeking.
 *
 ******************************************/



class IBitStream : public IBitStreamUndo 
{
public:
 	IBitStream() :
		IBitStreamUndo(),
		streamname( "unnamed" )
		{
		}
	virtual ~IBitStream() { Release(); }


	// Bit-level Parsing file-pointer entry-points
	uint32_t Get1Bit();
	uint32_t GetBits(int N);
	bool SeekSync( unsigned int sync, int N, int lim);
	void SeekFwdBits( unsigned int bytes_to_seek_fwd );

	// Bit-level parsing state undo mechanism
	void PrepareUndo(IBitStreamUndo &undobuf);
	void UndoChanges(IBitStreamUndo &undobuf);

	// Byte-level file-I/O entry-points
	inline bitcount_t GetBytePos() { return bytereadpos; }
	inline unsigned int BufferedBytes()
		{
			return (bfr_start+buffered-bytereadpos);
		}
	unsigned int GetBytes( uint8_t *dst,
						   unsigned int length_bytes);

	//
	// Byte data buffer management
	void Flush( bitcount_t byte_position );

	inline const char *StreamName() { return streamname; }

protected:
	bool ReadIntoBuffer( unsigned int to_read = BUFFER_SIZE );
	virtual size_t ReadStreamBytes( uint8_t *buf, size_t number ) = 0;
	virtual bool EndOfStream() = 0;
	const char *streamname;

};

/********************************
 *
 * IFileBitStream - Input bit stream class for bit streams sourced
 * from standard file I/O (this of course *includes* network sockets,
 * fifo's, et al).
 *
 * OLAF: To hook into your PES reader/reconstructor you need to define
 * a class like this one, where 'ReadStreamBytes' calls you code to
 * generate the required number of bytes of ES data and transfer it 
 * to the specified buffer.  The logical way to do this would be to
 * inherit IBitStream as a base class of the top-level classes for the ES
 * reconstructors.
 *
 ********************************/

class IFileBitStream : public IBitStream
{
public:
 	IFileBitStream( const char *bs_filename, 
					unsigned int buf_size = BUFFER_SIZE);
	~IFileBitStream();

private:
	FILE *fileh;
	const char *filename;
	virtual size_t ReadStreamBytes( uint8_t *buf, size_t number ) 
		{
			return fread(buf,sizeof(uint8_t), number, fileh ); 
		}
	virtual bool EndOfStream() { return feof(fileh); }
	
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

