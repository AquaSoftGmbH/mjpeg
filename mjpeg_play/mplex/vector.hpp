#ifndef __AUSTREAM_H__
#define __AUSTREAM_H__

#include <vector>
#include "mjpeg_logging.h"
#include "aunit.hpp"

class AUStream
{
public:
	// TODO The buffer should be delete-ed on destruction...
	AUStream();
	void init( Aunit *rec );

	void append( Aunit &rec )
	{
		if( size == BUF_SIZE )
			mjpeg_error_exit1( "INTERNAL ERROR: AU buffer overflow" );
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

	inline void droplast()
		{
			if( size == 0 )
				mjpeg_error_exit1( "INTERNAL ERROR: droplast empty AU buffer" );
			--size;
			if( cur_wr == 0 )
				cur_wr = BUF_SIZE ;
			--cur_wr;
		}

	inline Aunit *lookahead( )
	{
		return size == 0 ? 0 : buf[cur_rd];
    }

	inline Aunit *last()
		{
			int i = cur_wr < 1 ? BUF_SIZE-1 : cur_wr-1;
			return buf[i];
		}

	static const unsigned int BUF_SIZE;

	inline unsigned int current() { return totalctr; }
private:
	unsigned int cur_rd;
	unsigned int cur_wr;
	unsigned int totalctr;
	unsigned int size;
	std::vector<AunitPtr> buf;
};




#endif // __AUSTREAM_H__
