#ifndef __AUSTREAM_H__
#define __AUSTREAM_H__

#include <config.h>
#include <deque>
#include "aunit.hh"



typedef struct VectorStrct *Vector;

Vector NewVector( size_t recsize );
void *VectorAppend( Vector v, void *rec );
void VectorRewind( Vector v );
void *VectorNext( Vector v );
void *VectorLookAhead( Vector v, int lookahead );

template <class T>
class AUStream
{
public:
	AUStream() : cur( 0 ) {}
	void append( T *rec );
	void rewind();
	inline T *next( ) 
	{ 
		if( cur==buf.size() )
			return 0;
	    else
			return &buf[cur++]; 
	}
	inline T *lookahead( int lookahead )
	{
		if( cur+lookahead >= buf.size )
			return 0;
		else
			return &buf[cur+lookahead];
    }		
private:
	int cur;
	deque<T> buf;
};




#endif // __AUSTREAM_H__
