#ifndef __AUNITBUFFER_H__
#define __AUNITBUFFER_H__
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <deque>
#include "mjpeg_logging.h"
#include "aunit.hpp"

class AUStream
{
public:
	AUStream() : sum_AU_sizes(0) {}
	~AUStream() 
	{
		for( std::deque<AUnit *>::iterator i = buf.begin(); i < buf.end(); ++i )
			delete *i;
	}
	
	void Append( AUnit &rec )
	{
		if( buf.size() >= BUF_SIZE_SANITY )
			mjpeg_error_exit1( "INTERNAL ERROR: AU buffer overflow" );
		buf.push_back( new AUnit(rec) );
		sum_AU_sizes += rec.PayloadSize();
	}

	inline AUnit *Next( ) 
	{ 
		if( buf.size()==0 )
		{
			return 0;
		}
	    else
		{
			AUnit *res = buf.front();
			sum_AU_sizes -= res->PayloadSize();
			buf.pop_front();
			return res;
		}
	}

	inline void DropLast()
		{
			if( buf.empty() )
				mjpeg_error_exit1( "INTERNAL ERROR: droplast empty AU buffer" );
			sum_AU_sizes -= buf.back()->PayloadSize();
			buf.pop_back();
			
		}

	inline AUnit *Lookahead( unsigned int n = 1)
	{
		return buf.size() <= n ? 0 : buf[n];
    }

	inline unsigned int MaxAULookahead() const { return buf.size()-1; }
	inline unsigned int MaxPayloadLookahead() const { return sum_AU_sizes; }

private:
	static const unsigned int BUF_SIZE_SANITY = 1000;


	unsigned int sum_AU_sizes;
	
	std::deque<AUnit *> buf;
};




#endif // __AUSTREAM_H__
