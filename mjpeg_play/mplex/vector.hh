#ifndef __AUSTREAM_H__
#define __AUSTREAM_H__

#include <config.h>
#include <deque>
#include "aunit.hh"


#ifdef REDUNDANT
typedef struct VectorStrct *Vector;

Vector NewVector( size_t recsize );
void *VectorAppend( Vector v, void *rec );
void VectorRewind( Vector v );
void *VectorNext( Vector v );
void *VectorLookAhead( Vector v, int lookahead );
#endif

template <class T>
class AUStream
{
public:
	AUStream() : cur( 0 ), totalctr(0) {}

	inline void append( T &rec )
	{
		buf.push_back( rec );
	}

	inline T *next( ) 
	{ 
		if( cur==buf.size() )
		{
			return 0;
		}
	    else
		{
			++totalctr;
			return &buf[cur++];
		}
	}

	inline T *lookahead( int lookahead )
	{
		if( cur+lookahead >= buf.size() )
		{
			return 0;
		}
		else
			return &buf[cur+lookahead];
    }

	void flush( unsigned int flush )
	{
		assert( flush < cur );
		// Do nothing if flush would purge current entry in deque
		cur -= flush;
		for( unsigned int i =0; i < flush ; ++i )
			buf.pop_front();
    }

	inline unsigned int curpos() { return totalctr; }
private:
	unsigned int cur;
	unsigned int totalctr;
	deque<T> buf;
};




#endif // __AUSTREAM_H__
