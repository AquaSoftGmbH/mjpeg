#ifndef __AUSTREAM_H__
#define __AUSTREAM_H__

#include <config.h>
#include <deque>
#include "mjpeg_logging.h"
#include "aunit.hh"

class AUStream
{
public:
	AUStream();

	void init( Aunit *rec );

	void append( Aunit &rec )
	{
		if( size == BUF_SIZE )
			mjpeg_error_exit1( "INTERNAL ERROR: AU buffer overflow\n" );
		*buf[cur_wr] = rec;
		++size;
		++cur_wr;
		cur_wr = cur_wr >= BUF_SIZE ? 0 : cur_wr;
	}

	inline Aunit *next( ) 
	{ 
		if( size==0 )
		{
			return 0;
		}
	    else
		{
			Aunit *ret;
			ret = buf[cur_rd];
			++cur_rd;
			++totalctr;
			--size;
			cur_rd = cur_rd >= BUF_SIZE ? 0 : cur_rd;
			return ret;
		}
	}

	inline Aunit *lookahead( int lookahead )
	{
		if( lookahead >= size )
		{
			return 0;
		}
		else
			return buf[(cur_rd+lookahead)%BUF_SIZE];
    }

	static const int BUF_SIZE = 128;

	inline unsigned int current() { return totalctr; }
//private:
	unsigned int cur_rd;
	unsigned int cur_wr;
	unsigned int totalctr;
	unsigned int size;
	Aunit **buf;
};




#endif // __AUSTREAM_H__
