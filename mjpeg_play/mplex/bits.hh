#ifndef __BITS_H__
#define __BITS_H__

#include <config.h>
#include <stdio.h>
#include <inttypes.h>


typedef uint64_t bitcount_t;

class BitStream 
{
public:
	FILE *fileh;
//protected:	
	uint8_t outbyte;
	unsigned int byteidx;
	int bitidx;
	unsigned int bufcount;
	fpos_t actpos;
	bitcount_t totbits;
	bool eobs;
	static const unsigned int BUFFER_SIZE=8192;
	uint8_t bfr[BUFFER_SIZE];
public:
	BitStream() : fileh(0), totbits(0LL), eobs(true) {}
	inline bitcount_t bitcount() { return totbits; }
	inline bool eos() { return eobs; }
};


class IBitStream : public BitStream {
public:
	void open( char *bs_filename);
	void close();
	void rewind();
	uint32_t get1bit();
	uint32_t getbits(int N);
	void prepareundo(BitStream &undobuf);
	void undochanges(BitStream &undobuf);
	bool seek_sync( unsigned int sync, int N, int lim);
private:
	bool refill_buffer();
	static uint8_t masks[8];
};

class OBitStream : public BitStream {
public:
	void open( char *bs_filename);
	void close();
	void putbits( int val, int n);
	void put1bit( int val);
	void alignbits();
private:
	void putbyte();
};

#endif  // __BITS_H__

